#include "server.h"
#include "server_cammand_define.h"

/* ================================ MULTI/EXEC ============================== */

/* Client state initialization for MULTI/EXEC */
// 初始化事务状态
void initClientMultiState(client *c)
{
    c->mstate.commands = NULL;
    c->mstate.count = 0;
    c->mstate.cmd_flags = 0;
    c->mstate.cmd_inv_flags = 0;
}

/* Release all the resources associated with MULTI/EXEC state */
// 用于释放 Redis 客户端事务状态（mstate）中分配的内存资源。
void freeClientMultiState(client *c)
{
    int j;

    for (j = 0; j < c->mstate.count; j++)
    { // 遍历所有命令
        int i;
        multiCmd *mc = c->mstate.commands + j;

        for (i = 0; i < mc->argc; i++) // 遍历所有参数
            decrRefCount(mc->argv[i]);
        zfree(mc->argv);
    }
    zfree(c->mstate.commands);
}

/* Add a new command into the MULTI commands queue */
// 将一个新命令插入事务命令队列
void queueMultiCommand(client *c)
{
    multiCmd *mc;
    int j;

    /* No sense to waste memory if the transaction is already aborted.
     * this is useful in case client sends these in a pipeline, or doesn't
     * bother to read previous responses and didn't notice the multi was already
     * aborted. */
    if (c->flags & CLIENT_DIRTY_EXEC)
        return;

    c->mstate.commands = zrealloc(c->mstate.commands,
                                  sizeof(multiCmd) * (c->mstate.count + 1));
    mc = c->mstate.commands + c->mstate.count;
    mc->cmd = c->cmd;
    mc->argc = c->argc;
    mc->argv = zmalloc(sizeof(robj *) * c->argc);
    memcpy(mc->argv, c->argv, sizeof(robj *) * c->argc);
    for (j = 0; j < c->argc; j++)
        incrRefCount(mc->argv[j]);
    c->mstate.count++;
    c->mstate.cmd_flags |= c->cmd->flags;
    c->mstate.cmd_inv_flags |= ~c->cmd->flags;
}

// 丢弃事务
void discardTransaction(client *c)
{
    freeClientMultiState(c);                                            // 释放事务状态
    initClientMultiState(c);                                            // 初始化事务状态
    c->flags &= ~(CLIENT_MULTI | CLIENT_DIRTY_CAS | CLIENT_DIRTY_EXEC); // 将flags置为与事务无关
    unwatchAllKeys(c);                                                  // 取消客户端所有watch的键
}

/* Flag the transaction as DIRTY_EXEC so that EXEC will fail.
 * Should be called every time there is an error while queueing a command. */
void flagTransaction(client *c)
{
    if (c->flags & CLIENT_MULTI)
        c->flags |= CLIENT_DIRTY_EXEC;
}

// MULTI 是 Redis 事务机制的起点命令，用于开启一个事务块，允许客户端将多个命令打包为一个原子操作。
void multiCommand(client *c)
{
    if (c->flags & CLIENT_MULTI)
    {
        addReplyError(c, "MULTI calls can not be nested"); // 如果已经使用事务了，不能嵌套使用
        return;
    }
    c->flags |= CLIENT_MULTI;

    addReply(c, shared.ok); // 返回成功状态
}

// 撤销命令
void discardCommand(client *c)
{
    if (!(c->flags & CLIENT_MULTI))
    {
        addReplyError(c, "DISCARD without MULTI"); // 没有处在事务状态
        return;
    }
    discardTransaction(c); // 撤销事务
    addReply(c, shared.ok);
}

void beforePropagateMultiOrExec(int multi)
{
    if (multi)
    {
        /* Propagating MULTI */
        serverAssert(!server.propagate_in_transaction);
        server.propagate_in_transaction = 1;
    }
    else
    {
        /* Propagating EXEC */
        serverAssert(server.propagate_in_transaction == 1);
        server.propagate_in_transaction = 0;
    }
}

/* Send a MULTI command to all the slaves and AOF file. Check the execCommand
 * implementation for more information. */
void execCommandPropagateMulti(int dbid)
{
    beforePropagateMultiOrExec(1);
    propagate(server.multiCommand, dbid, &shared.multi, 1,
              PROPAGATE_AOF | PROPAGATE_REPL); // 传播命令
}

void execCommandPropagateExec(int dbid)
{
    beforePropagateMultiOrExec(0);
    propagate(server.execCommand, dbid, &shared.exec, 1,
              PROPAGATE_AOF | PROPAGATE_REPL);
}

/* Aborts a transaction, with a specific error message.
 * The transaction is always aboarted with -EXECABORT so that the client knows
 * the server exited the multi state, but the actual reason for the abort is
 * included too.
 * Note: 'error' may or may not end with \r\n. see addReplyErrorFormat. */
void execCommandAbort(client *c, sds error)
{
    discardTransaction(c);

    if (error[0] == '-')
        error++;
    addReplyErrorFormat(c, "-EXECABORT Transaction discarded because of: %s", error);

    /* Send EXEC to clients waiting data from MONITOR. We did send a MULTI
     * already, and didn't send any of the queued commands, now we'll just send
     * EXEC so it is clear that the transaction is over. */
    if (listLength(server.monitors) && !server.loading)
        replicationFeedMonitors(c, server.monitors, c->db->id, c->argv, c->argc);
}

// EXEC 是 Redis 事务机制的核心命令之一，用于执行客户端通过 MULTI 命令开启的事务块中积累的所有命令。
void execCommand(client *c)
{
    int j;
    robj **orig_argv;
    int orig_argc;
    struct redisCommand *orig_cmd;
    int was_master = server.masterhost == NULL;

    if (!(c->flags & CLIENT_MULTI))
    {
        addReplyError(c, "EXEC without MULTI");
        return;
    }

    /* Check if we need to abort the EXEC because:
     * 检查是否需要取消支持执行EXEC，因为：
     * 1. 有些key被监听了
     * 2. 队列前面的指令执行发生错误
     * 1) Some WATCHed key was touched.
     * 2) There was a previous error while queueing commands.
     * A failed EXEC in the first case returns a multi bulk nil object
     * (technically it is not an error but a special behavior), while
     * in the second an EXECABORT error is returned. */
    if (c->flags & (CLIENT_DIRTY_CAS | CLIENT_DIRTY_EXEC))
    {
        addReply(c, c->flags & CLIENT_DIRTY_EXEC ? shared.execaborterr : shared.nullarray[c->resp]);
        discardTransaction(c);
        goto handle_monitor;
    }

    uint64_t old_flags = c->flags;

    /* we do not want to allow blocking commands inside multi */
    c->flags |= CLIENT_DENY_BLOCKING;

    /* Exec all the queued commands */
    unwatchAllKeys(c); /* Unwatch ASAP otherwise we'll waste CPU cycles */

    server.in_exec = 1;

    orig_argv = c->argv;
    orig_argc = c->argc;
    orig_cmd = c->cmd;
    addReplyArrayLen(c, c->mstate.count);
    for (j = 0; j < c->mstate.count; j++)
    {
        c->argc = c->mstate.commands[j].argc;
        c->argv = c->mstate.commands[j].argv;
        c->cmd = c->mstate.commands[j].cmd;

        /* ACL permissions are also checked at the time of execution in case
         * they were changed after the commands were ququed. */
        int acl_errpos;
        int acl_retval = ACLCheckCommandPerm(c, &acl_errpos);
        if (acl_retval == ACL_OK && c->cmd->proc == publishCommand)
            acl_retval = ACLCheckPubsubPerm(c, 1, 1, 0, &acl_errpos);
        if (acl_retval != ACL_OK)
        {
            char *reason;
            switch (acl_retval)
            {
            case ACL_DENIED_CMD:
                reason = "no permission to execute the command or subcommand";
                break;
            case ACL_DENIED_KEY:
                reason = "no permission to touch the specified keys";
                break;
            case ACL_DENIED_CHANNEL:
                reason = "no permission to publish to the specified channel";
                break;
            default:
                reason = "no permission";
                break;
            }
            addACLLogEntry(c, acl_retval, acl_errpos, NULL);
            addReplyErrorFormat(c,
                                "-NOPERM ACLs rules changed between the moment the "
                                "transaction was accumulated and the EXEC call. "
                                "This command is no longer allowed for the "
                                "following reason: %s",
                                reason);
        }
        else
        {
            call(c, server.loading ? CMD_CALL_NONE : CMD_CALL_FULL);
            serverAssert((c->flags & CLIENT_BLOCKED) == 0);
        }

        /* Commands may alter argc/argv, restore mstate. */
        c->mstate.commands[j].argc = c->argc;
        c->mstate.commands[j].argv = c->argv;
        c->mstate.commands[j].cmd = c->cmd;
    }

    // restore old DENY_BLOCKING value
    if (!(old_flags & CLIENT_DENY_BLOCKING))
        c->flags &= ~CLIENT_DENY_BLOCKING;

    c->argv = orig_argv;
    c->argc = orig_argc;
    c->cmd = orig_cmd;
    discardTransaction(c);

    /* Make sure the EXEC command will be propagated as well if MULTI
     * was already propagated. */
    if (server.propagate_in_transaction)
    {
        int is_master = server.masterhost == NULL;
        server.dirty++;
        beforePropagateMultiOrExec(0);
        /* If inside the MULTI/EXEC block this instance was suddenly
         * switched from master to slave (using the SLAVEOF command), the
         * initial MULTI was propagated into the replication backlog, but the
         * rest was not. We need to make sure to at least terminate the
         * backlog with the final EXEC. */
        if (server.repl_backlog && was_master && !is_master)
        {
            char *execcmd = "*1\r\n$4\r\nEXEC\r\n";
            feedReplicationBacklog(execcmd, strlen(execcmd));
        }
    }

    server.in_exec = 0;

handle_monitor:
    /* Send EXEC to clients waiting data from MONITOR. We do it here
     * since the natural order of commands execution is actually:
     * MUTLI, EXEC, ... commands inside transaction ...
     * Instead EXEC is flagged as CMD_SKIP_MONITOR in the command
     * table, and we do it here with correct ordering. */
    if (listLength(server.monitors) && !server.loading)
        replicationFeedMonitors(c, server.monitors, c->db->id, c->argv, c->argc);
}

/* Watch for the specified key */
void watchForKey(client *c, robj *key)
{
    list *clients = NULL;
    listIter li;
    listNode *ln;
    watchedKey *wk;

    /* Check if we are already watching for this key */
    listRewind(c->watched_keys, &li);
    while ((ln = listNext(&li)))
    {
        wk = listNodeValue(ln);
        if (wk->db == c->db && equalStringObjects(key, wk->key))
            return; /* key已经在client的监听列表中了，直接返回 */
    }
    /* key不在client的监听列表中就添加进去*/
    clients = dictFetchValue(c->db->watched_keys, key);
    if (!clients)
    {
        clients = listCreate();
        dictAdd(c->db->watched_keys, key, clients);
        incrRefCount(key);
    }
    listAddNodeTail(clients, c);
    /* Add the new key to the list of keys watched by this client */
    wk = zmalloc(sizeof(*wk));
    wk->key = key;
    wk->db = c->db;
    incrRefCount(key);
    listAddNodeTail(c->watched_keys, wk);
}

/* Unwatch all the keys watched by this client. To clean the EXEC dirty
 * flag is up to the caller. */
void unwatchAllKeys(client *c)
{
    listIter li;
    listNode *ln;

    if (listLength(c->watched_keys) == 0)
        return;
    listRewind(c->watched_keys, &li);
    while ((ln = listNext(&li)))
    {
        list *clients;
        watchedKey *wk;

        /* Lookup the watched key -> clients list and remove the client
         * from the list */
        wk = listNodeValue(ln);
        clients = dictFetchValue(wk->db->watched_keys, wk->key);
        serverAssertWithInfo(c, NULL, clients != NULL);
        listDelNode(clients, listSearchKey(clients, c));
        /* Kill the entry at all if this was the only client */
        if (listLength(clients) == 0)
            dictDelete(wk->db->watched_keys, wk->key);
        /* Remove this watched key from the client->watched list */
        listDelNode(c->watched_keys, ln);
        decrRefCount(wk->key);
        zfree(wk);
    }
}

/* "Touch" a key, so that if this key is being WATCHed by some client the
 * next EXEC will fail. */
void touchWatchedKey(redisDb *db, robj *key)
{
    list *clients;
    listIter li;
    listNode *ln;

    if (dictSize(db->watched_keys) == 0)
        return;
    clients = dictFetchValue(db->watched_keys, key);
    if (!clients)
        return;

    /* Mark all the clients watching this key as CLIENT_DIRTY_CAS */
    /* Check if we are already watching for this key */
    listRewind(clients, &li);
    while ((ln = listNext(&li)))
    {
        client *c = listNodeValue(ln);

        c->flags |= CLIENT_DIRTY_CAS;
    }
}

/* Set CLIENT_DIRTY_CAS to all clients of DB when DB is dirty.
 * It may happen in the following situations:
 * FLUSHDB, FLUSHALL, SWAPDB
 *
 * replaced_with: for SWAPDB, the WATCH should be invalidated if
 * the key exists in either of them, and skipped only if it
 * doesn't exist in both. */
void touchAllWatchedKeysInDb(redisDb *emptied, redisDb *replaced_with)
{
    listIter li;
    listNode *ln;
    dictEntry *de;

    if (dictSize(emptied->watched_keys) == 0)
        return;

    dictIterator *di = dictGetSafeIterator(emptied->watched_keys);
    while ((de = dictNext(di)) != NULL)
    {
        robj *key = dictGetKey(de);
        list *clients = dictGetVal(de);
        if (!clients)
            continue;
        listRewind(clients, &li);
        while ((ln = listNext(&li)))
        {
            client *c = listNodeValue(ln);
            if (dictFind(emptied->dict, key->ptr))
            {
                c->flags |= CLIENT_DIRTY_CAS;
            }
            else if (replaced_with && dictFind(replaced_with->dict, key->ptr))
            {
                c->flags |= CLIENT_DIRTY_CAS;
            }
        }
    }
    dictReleaseIterator(di);
}

// WATCH 是 Redis 事务机制中的一个重要命令，用于监视一个或多个键的变化，以便在事务执行时检测到这些键是否被其他客户端修改。
void watchCommand(client *c)
{
    int j;

    if (c->flags & CLIENT_MULTI)
    {
        addReplyError(c, "WATCH inside MULTI is not allowed");
        return;
    }
    for (j = 1; j < c->argc; j++)
        watchForKey(c, c->argv[j]);
    addReply(c, shared.ok);
}

// UNWATCH 是 Redis 事务机制中的一个命令，用于取消客户端对所有被 WATCH 命令监视的键的监视。
void unwatchCommand(client *c)
{
    unwatchAllKeys(c);
    c->flags &= (~CLIENT_DIRTY_CAS);
    addReply(c, shared.ok);
}
