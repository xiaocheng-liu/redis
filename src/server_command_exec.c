#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>

#include "ae.h"
#include "atomicvar.h"
#include "bio.h"
#include "cluster.h"
#include "config.h"
#include "crc64.h"
#include "debugmacro.h"
#include "evict.h"
#include "latency.h"
#include "monotonic.h"
#include "mt19937-64.h"
#include "pubsub.h"
#include "sentinel.h"
#include "server.h"
#include "server_commands.h"
#include "slowlog.h"
#include "util.h"
#include "version.h"

int allPersistenceDisabled(void) { return server.saveparamslen == 0 && server.aof_state == AOF_OFF; }

/* This is our timer interrupt, called server.hz times per second.
 * Here is where we do a number of things that need to be done asynchronously.
 * For instance:
 *
 * - Active expired keys collection (it is also performed in a lazy way on
 *   lookup).
 * - 激活过期key的回收
 * - Software watchdog.
 * - 监控信息
 * - Update some statistic.
 * - 更新统计信息
 * - Incremental rehashing of the DBs hash tables.
 * - db重hash
 * - Triggering BGSAVE / AOF rewrite, and handling of terminated children.
 * - 触发rdb和aof备份,处理终止的子进程
 * - Clients timeout of different kinds.
 * - Replication reconnection.
 * - 重置链接
 * - Many more...
 *
 * Everything directly called here will be called server.hz times per second,
 * so in order to throttle execution of things we want to do less frequently
 * a macro is used: run_with_period(milliseconds) { .... }
 */

/* This function gets called every time Redis is entering the
 * main loop of the event driven library, that is, before to sleep
 * for ready file descriptors.
 * 这个方法会在每次redis事件循环进入的时候调用
 * 注意:这个方法目前会在以下两个方法中调用
 * 1. aeMain - The main server loop
 * 2. processEventsWhileBlocked - Process clients during RDB/AOF load
 *
 * If it was called from processEventsWhileBlocked we don't want
 * to perform all actions (For example, we don't want to expire
 * keys), but we do need to perform some actions.
 *
 * The most important is freeClientsInAsyncFreeQueue but we also
 * call some other low-risk functions. */
/* =========================== Server initialization ======================== */
/* =========================== 服务器初始化 ======================== */

// 创建共享对象
// 初始化服务器配置
extern char **environ;

/**
 * 读取进程的OOM(Out of Memory)score adjustment值
 *
 * 此函数旨在读取当前进程的OOM score adjustment值,该值影响内存不足时进程被Linux内核OOM杀手选择的可能性
 * OOM score adjustment值越高,进程在内存不足时被选择的可能性越大
 * 此函数仅在定义了HAVE_PROC_OOM_SCORE_ADJ宏时编译,确保系统支持此特性
 */
/* ========================== Redis OP Array API ============================ */

/* ====================== Commands lookup and execution ===================== */

struct redisCommand *lookupCommand(sds name) { return dictFetchValue(server.commands, name); }

// 通过C字符串查找命令
struct redisCommand *lookupCommandByCString(const char *s) {
    struct redisCommand *cmd;
    sds name = sdsnew(s);

    cmd = dictFetchValue(server.commands, name);
    sdsfree(name);
    return cmd;
}

/* Lookup the command in the current table, if not found also check in
 * the original table containing the original command names unaffected by
 * redis.conf rename-command statement.
 *
 * This is used by functions rewriting the argument vector such as
 * rewriteClientCommandVector() in order to set client->cmd pointer
 * correctly even if the command was renamed. */
// 通过sds字符串查找命令
struct redisCommand *lookupCommandOrOriginal(sds name) {
    struct redisCommand *cmd = dictFetchValue(server.commands, name);

    if (!cmd)
        cmd = dictFetchValue(server.orig_commands, name);
    return cmd;
}

/* Propagate the specified command (in the context of the specified database id)
 * to AOF and Slaves.
 *
 * flags are an xor between:
 * + PROPAGATE_NONE (no propagation of command at all)
 * + PROPAGATE_AOF (propagate into the AOF file if is enabled)
 * + PROPAGATE_REPL (propagate into the replication link)
 *
 * This should not be used inside commands implementation since it will not
 * wrap the resulting commands in MULTI/EXEC. Use instead alsoPropagate(),
 * preventCommandPropagation(), forceCommandPropagation().
 *
 * However for functions that need to (also) propagate out of the context of a
 * command execution, for example when serving a blocked client, you
 * want to use propagate().
 */
// propagate函数的作用就是将带来数据改动的命令传播给slave和AOF
void propagate(struct redisCommand *cmd, int dbid, robj **argv, int argc, int flags) {
    /* Propagate a MULTI request once we encounter the first command which
     * is a write command.
     * This way we'll deliver the MULTI/..../EXEC block as a whole and
     * both the AOF and the replication link will have the same consistency
     * and atomicity guarantees. */
    // 这段代码的功能是:当遇到第一个写命令时,传播一个MULTI请求。
    // 这样可以确保MULTI/.../EXEC块作为一个整体被传递,从而使AOF和复制链路具有相同的一致性和原子性保证。
    if (server.in_exec && !server.propagate_in_transaction)
        execCommandPropagateMulti(dbid);

    /* This needs to be unreachable since the dataset should be fixed during
     * client pause, otherwise data may be lossed during a failover. */
    // 这段代码是一个注释,说明在客户端暂停期间,数据集应该是固定的,否则在故障转移过程中可能会丢失数据。
    // 因此,这部分代码逻辑应该是不可达的。
    serverAssert(!(areClientsPaused() && !server.client_pause_in_transaction));

    if (server.aof_state != AOF_OFF && flags & PROPAGATE_AOF)
        // feedAppendOnlyFile() 函数会同步命令到AOF文件
        feedAppendOnlyFile(cmd, dbid, argv, argc);
    if (flags & PROPAGATE_REPL)
        // replicationFeedSlaves() 同步命令到 Slave 节点
        replicationFeedSlaves(server.slaves, dbid, argv, argc);
}

/* Used inside commands to schedule the propagation of additional commands
 * after the current command is propagated to AOF / Replication.
 *
 * 'cmd' must be a pointer to the Redis command to replicate, dbid is the
 * database ID the command should be propagated into.
 * Arguments of the command to propagate are passed as an array of redis
 * objects pointers of len 'argc', using the 'argv' vector.
 *
 * The function does not take a reference to the passed 'argv' vector,
 * so it is up to the caller to release the passed argv (but it is usually
 * stack allocated).  The function automatically increments ref count of
 * passed objects, so the caller does not need to. */
void alsoPropagate(struct redisCommand *cmd, int dbid, robj **argv, int argc, int target) {
    robj **argvcopy;
    int j;

    if (server.loading)
        return; /* No propagation during loading. */

    argvcopy = zmalloc(sizeof(robj *) * argc);
    for (j = 0; j < argc; j++) {
        argvcopy[j] = argv[j];
        incrRefCount(argv[j]);
    }
    redisOpArrayAppend(&server.also_propagate, cmd, dbid, argvcopy, argc, target);
}

/* It is possible to call the function forceCommandPropagation() inside a
 * Redis command implementation in order to to force the propagation of a
 * specific command execution into AOF / Replication. */
void forceCommandPropagation(client *c, int flags) {
    serverAssert(c->cmd->flags & (CMD_WRITE | CMD_MAY_REPLICATE));
    if (flags & PROPAGATE_REPL)
        c->flags |= CLIENT_FORCE_REPL;
    if (flags & PROPAGATE_AOF)
        c->flags |= CLIENT_FORCE_AOF;
}

/* Avoid that the executed command is propagated at all. This way we
 * are free to just propagate what we want using the alsoPropagate()
 * API. */
void preventCommandPropagation(client *c) { c->flags |= CLIENT_PREVENT_PROP; }

/* AOF specific version of preventCommandPropagation(). */
void preventCommandAOF(client *c) { c->flags |= CLIENT_PREVENT_AOF_PROP; }

/* Replication specific version of preventCommandPropagation(). */
void preventCommandReplication(client *c) { c->flags |= CLIENT_PREVENT_REPL_PROP; }

/* Call()是redis命令执行的核心方法.
 *
 * The following flags can be passed:
 * CMD_CALL_NONE        No flags.
 * CMD_CALL_SLOWLOG     Check command speed and log in the slow log if needed.
 * CMD_CALL_STATS       Populate command stats.
 * CMD_CALL_PROPAGATE_AOF   Append command to AOF if it modified the dataset
 *                          or if the client flags are forcing propagation.
 * CMD_CALL_PROPAGATE_REPL  Send command to slaves if it modified the dataset
 *                          or if the client flags are forcing propagation.
 * CMD_CALL_PROPAGATE   Alias for PROPAGATE_AOF|PROPAGATE_REPL.
 * CMD_CALL_FULL        Alias for SLOWLOG|STATS|PROPAGATE.
 *
 * The exact propagation behavior depends on the client flags.
 * Specifically:
 *
 * 1. If the client flags CLIENT_FORCE_AOF or CLIENT_FORCE_REPL are set
 *    and assuming the corresponding CMD_CALL_PROPAGATE_AOF/REPL is set
 *    in the call flags, then the command is propagated even if the
 *    dataset was not affected by the command.
 * 2. If the client flags CLIENT_PREVENT_REPL_PROP or CLIENT_PREVENT_AOF_PROP
 *    are set, the propagation into AOF or to slaves is not performed even
 *    if the command modified the dataset.
 *
 * Note that regardless of the client flags, if CMD_CALL_PROPAGATE_AOF
 * or CMD_CALL_PROPAGATE_REPL are not set, then respectively AOF or
 * slaves propagation will never occur.
 *
 * Client flags are modified by the implementation of a given command
 * using the following API:
 *
 * forceCommandPropagation(client *c, int flags);
 * preventCommandPropagation(client *c);
 * preventCommandAOF(client *c);
 * preventCommandReplication(client *c);
 *
 */
void call(client *c, int flags) {
    long long dirty;
    // 这行代码定义了一个名为 call_timer 的变量,类型为 monotime。monotime
    // 通常用于记录单调递增的时间值,常用于测量时间间隔或定时任务。
    monotime call_timer;
    int client_old_flags = c->flags;
    struct redisCommand *real_cmd = c->cmd;
    static long long prev_err_count;

    server.fixed_time_expire++;

    /* Send the command to clients in MONITOR mode if applicable.
     * Administrative commands are considered too dangerous to be shown. */
    // 将命令发送到处于监视模式的客户端(如果适用)。管理命令被认为太危险而无法显示。
    if (listLength(server.monitors) && !server.loading && !(c->cmd->flags & (CMD_SKIP_MONITOR | CMD_ADMIN))) {
        replicationFeedMonitors(c, server.monitors, c->db->id, c->argv, c->argc);
    }

    /* Initialization: clear the flags that must be set by the command on
     * demand, and initialize the array for additional commands propagation. */
    // 初始化:清除命令必须按需设置的标志,并初始化数组以进行其他命令传播。
    c->flags &= ~(CLIENT_FORCE_AOF | CLIENT_FORCE_REPL | CLIENT_PREVENT_PROP);
    redisOpArray prev_also_propagate = server.also_propagate;
    redisOpArrayInit(&server.also_propagate);

    /* Call the command. */
    // 调用该命令。
    dirty = server.dirty;
    prev_err_count = server.stat_total_error_replies;
    updateCachedTime(0);

    // 这段代码的功能是启动一个计时器 call_timer,记录从调用该函数开始的时间。elapsedStart
    // 函数用于初始化或重置计时器,以便后续可以计算经过的时间。
    elapsedStart(&call_timer);

    // 会调用客户端命令对应的 redisCommand 的处理方法
    // serverLog(LL_DEBUG, "命令名称:%s", c->cmd->name);
    c->cmd->proc(c);
    // 这段代码的功能是计算并存储 call_timer 计时器所经过的时间,单位为微秒。elapsedUs
    // 函数用于获取计时器的流逝时间,并将结果赋值给常量 duration。
    const long duration = elapsedUs(call_timer);
    c->duration = duration;
    dirty = server.dirty - dirty;
    if (dirty < 0)
        dirty = 0;

    /* Update failed command calls if required.
     * We leverage a static variable (prev_err_count) to retain
     * the counter across nested function calls and avoid logging
     * the same error twice. */
    // 如果需要,更新失败的命令调用。我们利用静态变量 (prev_err_count)
    // 在嵌套函数调用中保留计数器,并避免两次记录相同的错误。
    if ((server.stat_total_error_replies - prev_err_count) > 0) {
        real_cmd->failed_calls++;
    }

    /* After executing command, we will close the client after writing entire
     * reply if it is set 'CLIENT_CLOSE_AFTER_COMMAND' flag. */
    // 执行命令后,如果设置了"CLIENT_CLOSE_AFTER_COMMAND"标志,我们将在写入整个回复后关闭客户端。
    if (c->flags & CLIENT_CLOSE_AFTER_COMMAND) {
        c->flags &= ~CLIENT_CLOSE_AFTER_COMMAND;
        c->flags |= CLIENT_CLOSE_AFTER_REPLY;
    }

    /* When EVAL is called loading the AOF we don't want commands called
     * from Lua to go into the slowlog or to populate statistics. */
    if (server.loading && c->flags & CLIENT_LUA)
        flags &= ~(CMD_CALL_SLOWLOG | CMD_CALL_STATS);

    /* If the caller is Lua, we want to force the EVAL caller to propagate
     * the script if the command flag or client flag are forcing the
     * propagation. */
    // 这段代码的功能是:如果调用者是Lua,则强制EVAL调用者在命令标志或客户端标志要求传播脚本时进行传播。
    if (c->flags & CLIENT_LUA && server.lua_caller) {
        if (c->flags & CLIENT_FORCE_REPL)
            server.lua_caller->flags |= CLIENT_FORCE_REPL;
        if (c->flags & CLIENT_FORCE_AOF)
            server.lua_caller->flags |= CLIENT_FORCE_AOF;
    }

    /* Log the command into the Slow log if needed, and populate the
     * per-command statistics that we show in INFO commandstats. */
    // 这段代码的功能是:如果需要,将命令记录到慢查询日志中,并更新每个命令的统计信息,这些统计信息会在INFO
    // commandstats中显示。
    if (flags & CMD_CALL_SLOWLOG && !(c->cmd->flags & CMD_SKIP_SLOWLOG)) {
        char *latency_event = (c->cmd->flags & CMD_FAST) ? "fast-command" : "command";
        latencyAddSampleIfNeeded(latency_event, duration / 1000);
        /* If command argument vector was rewritten, use the original
         * arguments. */
        // 这段代码的功能是检查命令参数向量是否被重写,如果是,则使用原始参数。这通常用于确保在某些情况下(如参数被临时修改后)能够恢复到最初的参数值。
        robj **argv = c->original_argv ? c->original_argv : c->argv;
        int argc = c->original_argv ? c->original_argc : c->argc;
        /* If the client is blocked we will handle slowlog when it is unblocked . */
        // 这段代码表示如果客户端被阻塞,将在客户端解除阻塞时处理慢查询日志。该逻辑简化了在阻塞期间的日志处理,确保日志记录的准确性和及时性。
        if (!(c->flags & CLIENT_BLOCKED)) {
            // 记录慢查询日志
            slowlogPushEntryIfNeeded(c, argv, argc, duration);
        }
    }
    // 释放客户端原始参数内存。该函数用于释放与客户端关联的原始参数数组所占用的内存,确保资源正确回收。
    freeClientOriginalArgv(c);

    // 是否需要统计命令调用信息。
    if (flags & CMD_CALL_STATS) {
        /* use the real command that was executed (cmd and lastamc) may be
         * different, in case of MULTI-EXEC or re-written commands such as
         * EXPIRE, GEOADD, etc. */
        real_cmd->microseconds += duration;
        real_cmd->calls++;
    }

    /* Propagate the command into the AOF and replication link */
    // 这段代码的功能是将命令传播到AOF(Append Only File)和复制链接中。
    // 具体来说,它确保命令不仅被记录在AOF文件中以便持久化,还会通过复制链接同步到从服务器。
    if (flags & CMD_CALL_PROPAGATE && (c->flags & CLIENT_PREVENT_PROP) != CLIENT_PREVENT_PROP) {
        int propagate_flags = PROPAGATE_NONE;

        /* Check if the command operated changes in the data set. If so
         * set for replication / AOF propagation. */
        // 这段代码的功能是检查命令是否对数据集进行了更改。如果确实进行了更改,则设置为进行复制或AOF传播。
        if (dirty)
            propagate_flags |= (PROPAGATE_AOF | PROPAGATE_REPL);

        /* If the client forced AOF / replication of the command, set
         * the flags regardless of the command effects on the data set. */
        // 这段代码的功能是:如果客户端强制要求AOF(Append Only
        // File)或复制命令,则无论该命令是否对数据集产生影响,都设置相应的标志位
        if (c->flags & CLIENT_FORCE_REPL)
            propagate_flags |= PROPAGATE_REPL;
        if (c->flags & CLIENT_FORCE_AOF)
            propagate_flags |= PROPAGATE_AOF;

        /* However prevent AOF / replication propagation if the command
         * implementation called preventCommandPropagation() or similar,
         * or if we don't have the call() flags to do so. */
        // 这段代码的功能是防止在特定条件下将命令传播到AOF或复制系统。
        // 具体来说,如果命令实现调用了preventCommandPropagation()函数,或者没有设置call()标志来允许传播,则不会进行AOF或复制传播。
        if (c->flags & CLIENT_PREVENT_REPL_PROP || !(flags & CMD_CALL_PROPAGATE_REPL))
            propagate_flags &= ~PROPAGATE_REPL;
        if (c->flags & CLIENT_PREVENT_AOF_PROP || !(flags & CMD_CALL_PROPAGATE_AOF))
            propagate_flags &= ~PROPAGATE_AOF;

        /* Call propagate() only if at least one of AOF / replication
         * propagation is needed. Note that modules commands handle replication
         * in an explicit way, so we never replicate them automatically. */
        // 这段代码的功能是判断是否需要调用 propagate() 函数。
        // 只有在 AOF 或者复制传播至少有一个需要的情况下才会调用该函数。
        // 模块命令的复制是显式处理的,因此不会自动复制。
        if (propagate_flags != PROPAGATE_NONE && !(c->cmd->flags & CMD_MODULE))

            // 调用 propagate()函数同步数据到 AOF 文件和 slave节点。
            propagate(c->cmd, c->db->id, c->argv, c->argc, propagate_flags);
    }

    /* Restore the old replication flags, since call() can be executed
     * recursively. */
    // 这段代码的功能是恢复旧的复制标志,因为 call()
    // 函数可能会递归执行。在递归调用之前保存复制标志,在递归调用之后恢复这些标志,以确保状态一致。
    c->flags &= ~(CLIENT_FORCE_AOF | CLIENT_FORCE_REPL | CLIENT_PREVENT_PROP);
    c->flags |= client_old_flags & (CLIENT_FORCE_AOF | CLIENT_FORCE_REPL | CLIENT_PREVENT_PROP);

    /* Handle the alsoPropagate() API to handle commands that want to propagate
     * multiple separated commands. Note that alsoPropagate() is not affected
     * by CLIENT_PREVENT_PROP flag. */
    // 这段代码的功能是处理 alsoPropagate() API,用于支持需要传播多个分离命令的场景。需要注意的是,alsoPropagate()
    // 的行为不受 CLIENT_PREVENT_PROP 标志的影响。具体逻辑可能涉及判断是否需要传播命令以及如何传播。
    if (server.also_propagate.numops) {
        int j;
        redisOp *rop;

        if (flags & CMD_CALL_PROPAGATE) {
            int multi_emitted = 0;
            /* Wrap the commands in server.also_propagate array,
             * but don't wrap it if we are already in MULTI context,
             * in case the nested MULTI/EXEC.
             *
             * And if the array contains only one command, no need to
             * wrap it, since the single command is atomic. */
            if (server.also_propagate.numops > 1 && !(c->cmd->flags & CMD_MODULE) && !(c->flags & CLIENT_MULTI) &&
                !(flags & CMD_CALL_NOWRAP)) {
                execCommandPropagateMulti(c->db->id);
                multi_emitted = 1;
            }

            for (j = 0; j < server.also_propagate.numops; j++) {
                rop = &server.also_propagate.ops[j];
                int target = rop->target;
                /* Whatever the command wish is, we honor the call() flags. */
                if (!(flags & CMD_CALL_PROPAGATE_AOF))
                    target &= ~PROPAGATE_AOF;
                if (!(flags & CMD_CALL_PROPAGATE_REPL))
                    target &= ~PROPAGATE_REPL;
                if (target)
                    propagate(rop->cmd, rop->dbid, rop->argv, rop->argc, target);
            }

            if (multi_emitted) {
                execCommandPropagateExec(c->db->id);
            }
        }
        redisOpArrayFree(&server.also_propagate);
    }
    server.also_propagate = prev_also_propagate;

    /* Client pause takes effect after a transaction has finished. This needs
     * to be located after everything is propagated. */
    if (!server.in_exec && server.client_pause_in_transaction) {
        server.client_pause_in_transaction = 0;
    }

    /* If the client has keys tracking enabled for client side caching,
     * make sure to remember the keys it fetched via this command. */
    if (c->cmd->flags & CMD_READONLY) {
        client *caller = (c->flags & CLIENT_LUA && server.lua_caller) ? server.lua_caller : c;
        if (caller->flags & CLIENT_TRACKING && !(caller->flags & CLIENT_TRACKING_BCAST)) {
            trackingRememberKeys(caller);
        }
    }

    server.fixed_time_expire--;
    server.stat_numcommands++;
    prev_err_count = server.stat_total_error_replies;

    /* Record peak memory after each command and before the eviction that runs
     * before the next command. */
    size_t zmalloc_used = zmalloc_used_memory();
    if (zmalloc_used > server.stat_peak_memory)
        server.stat_peak_memory = zmalloc_used;
}

/* Used when a command that is ready for execution needs to be rejected, due to
 * varios pre-execution checks. it returns the appropriate error to the client.
 * If there's a transaction is flags it as dirty, and if the command is EXEC,
 * it aborts the transaction.
 * Note: 'reply' is expected to end with \r\n */
void rejectCommand(client *c, robj *reply) {
    flagTransaction(c);
    if (c->cmd)
        c->cmd->rejected_calls++;
    if (c->cmd && c->cmd->proc == execCommand) {
        execCommandAbort(c, reply->ptr);
    } else {
        /* using addReplyError* rather than addReply so that the error can be logged. */
        addReplyErrorObject(c, reply);
    }
}

// 该函数 rejectCommandFormat 用于处理客户端命令格式错误的情况。主要功能包括:
// 增加命令的拒绝调用计数。
// 标记事务状态。
// 格式化错误信息并确保其不包含换行符。
// 根据命令类型选择不同的错误处理方式。
void rejectCommandFormat(client *c, const char *fmt, ...) {
    if (c->cmd)
        c->cmd->rejected_calls++;
    flagTransaction(c);
    va_list ap;
    va_start(ap, fmt);
    sds s = sdscatvprintf(sdsempty(), fmt, ap);
    va_end(ap);
    /* Make sure there are no newlines in the string, otherwise invalid protocol
     * is emitted (The args come from the user, they may contain any character). */
    sdsmapchars(s, "\r\n", "  ", 2);
    if (c->cmd && c->cmd->proc == execCommand) {
        execCommandAbort(c, s);
        sdsfree(s);
    } else {
        /* The following frees 's'. */
        addReplyErrorSds(c, s);
    }
}

/* If this function gets called we already read a whole
 * command, arguments are in the client argv/argc fields.
 * processCommand() execute the command or prepare the
 * server for a bulk read from the client.
 *
 * If C_OK is returned the client is still alive and valid and
 * other operations can be performed by the caller. Otherwise
 * if C_ERR is returned the client was destroyed (i.e. after QUIT).
 */
/* 如果调用此函数,我们已经读取了一个整体命令,参数位于客户端 argv/argc 字段中。
 * processCommand() 执行命令或准备用于从客户端批量读取的服务器。
 *
 * 如果返回C_OK,则客户端仍处于活动状态且有效,并且其他操作可由调用方执行。
 * 否则如果返回C_ERR,则客户端被销毁(即在 QUIT 之后)
 */
int processCommand(client *c) {
    moduleCallCommandFilters(c);

    /* The QUIT command is handled separately. Normal command procs will
     * go through checking for replication and QUIT will cause trouble
     * when FORCE_REPLICATION is enabled and would be implemented in
     * a regular command proc.
     * quit命令也会被redis服务端接收,从源码看起来是什么都没做,服务端处理可以避免
     * FORCE_REPLICATION开启后导致的问题 ？？
     * */
    if (!strcasecmp(c->argv[0]->ptr, "quit")) {
        addReply(c, shared.ok);
        c->flags |= CLIENT_CLOSE_AFTER_REPLY;
        return C_ERR;
    }

    /* Now lookup the command and check ASAP about trivial error conditions
     * such as wrong arity, bad command name and so forth.
     * 这里是对client的请求解析出对应的redis命令,并校验参数的合法性 */
    // 根据argv[0]在字典中查找当前命令,并进行命令合法性检查,以及命令参数个数检查 字典名为commands
    // c->cmd为当前要执行的命令 同时更新lastcmd loopupcommand其实就是在字典中根据键查找值而已
    c->cmd = c->lastcmd = lookupCommand(c->argv[0]->ptr);
    if (!c->cmd) {
        // 没找到指定的命令
        sds args = sdsempty();
        int i;
        for (i = 1; i < c->argc && sdslen(args) < 128; i++)
            args = sdscatprintf(args, "`%.*s`, ", 128 - (int) sdslen(args), (char *) c->argv[i]->ptr);
        rejectCommandFormat(c, "unknown command `%s`, with args beginning with: %s", (char *) c->argv[0]->ptr, args);
        sdsfree(args);
        return C_OK;
    } else if (
            (c->cmd->arity > 0 && c->cmd->arity != c->argc) ||
            (c->argc <
             -c->cmd->arity)) // 如果命令的参数个数大于0且不等于实际传递的参数个数,或者实际传递的参数个数小于命令参数个数的负值,则进入错误处理流程。
    {
        rejectCommandFormat(c, "wrong number of arguments for '%s' command", c->cmd->name);
        return C_OK;
    }

    // 这段代码用于判断当前命令是否为写命令。
    int is_write_command =
            (c->cmd->flags & CMD_WRITE) || (c->cmd->proc == execCommand && (c->mstate.cmd_flags & CMD_WRITE));
    // 这段代码用于判断当前命令是否为禁止内存溢出(OOM)的命令。
    int is_denyoom_command =
            (c->cmd->flags & CMD_DENYOOM) || (c->cmd->proc == execCommand && (c->mstate.cmd_flags & CMD_DENYOOM));
    // 这段代码用于判断是否拒绝过期命令。
    int is_denystale_command =
            !(c->cmd->flags & CMD_STALE) || (c->cmd->proc == execCommand && (c->mstate.cmd_inv_flags & CMD_STALE));
    // 这段代码用于判断是否禁止加载命令。
    int is_denyloading_command =
            !(c->cmd->flags & CMD_LOADING) || (c->cmd->proc == execCommand && (c->mstate.cmd_inv_flags & CMD_LOADING));
    // 这段代码用于判断命令是否可能需要复制。
    int is_may_replicate_command =
            (c->cmd->flags & (CMD_WRITE | CMD_MAY_REPLICATE)) ||
            (c->cmd->proc == execCommand && (c->mstate.cmd_flags & (CMD_WRITE | CMD_MAY_REPLICATE)));

    /* Check if the user is authenticated. This check is skipped in case
     * the default user is flagged as "nopass" and is active.  */
    // 是否已经认证
    // 检查认证信息 默认未开启 在redis.conf中修改requirepass打开
    int auth_required = (!(DefaultUser->flags & USER_FLAG_NOPASS) || (DefaultUser->flags & USER_FLAG_DISABLED)) &&
                        !c->authenticated;
    if (auth_required) {
        /* AUTH and HELLO and no auth modules are valid even in
         * non-authenticated state. */
        if (!(c->cmd->flags & CMD_NO_AUTH)) {
            rejectCommand(c, shared.noautherr);
            return C_OK;
        }
    }

    /* Check if the user can run this command according to the current
     * ACLs.
     * 如果是开启了ACL,需要对client请求做鉴权 */
    int acl_keypos;
    int acl_retval = ACLCheckCommandPerm(c, &acl_keypos);
    if (acl_retval != ACL_OK) {
        addACLLogEntry(c, acl_retval, acl_keypos, NULL);
        if (acl_retval == ACL_DENIED_CMD)
            rejectCommandFormat(c,
                                "-NOPERM this user has no permissions to run "
                                "the '%s' command or its subcommand",
                                c->cmd->name);
        else
            rejectCommandFormat(c, "-NOPERM this user has no permissions to access "
                                   "one of the keys used as arguments");
        return C_OK;
    }

    /* If cluster is enabled perform the cluster redirection here.
     * However we don't perform the redirection if:
     * 1) The sender of this command is our master.
     * 2) The command has no key arguments. */
    /* 如果启用了群集,请在此处执行群集重定向。
     * 但是,在以下情况下,我们不会执行重定向:
     * 1)命令的发送者是本节点的主节点。
     * 2)该命令没有key参数。*/
    if (server.cluster_enabled && !(c->flags & CLIENT_MASTER) &&
        !(c->flags & CLIENT_LUA && server.lua_caller->flags & CLIENT_MASTER) &&
        !(!cmdHasMovableKeys(c->cmd) && c->cmd->firstkey == 0 && c->cmd->proc != execCommand)) {
        int hashslot;
        int error_code;
        // 集群模式下,根据key确认可服务的redis节点,如果不是本节点就向client发送重定向信息
        clusterNode *n = getNodeByQuery(c, c->cmd, c->argv, c->argc, &hashslot, &error_code);
        if (n == NULL || n != server.cluster->myself) {
            if (c->cmd->proc == execCommand) {
                discardTransaction(c);
            } else {
                flagTransaction(c);
            }
            clusterRedirectClient(c, n, hashslot, error_code);
            c->cmd->rejected_calls++;
            return C_OK;
        }
    }

    /* Handle the maxmemory directive.
     *
     * Note that we do not want to reclaim memory if we are here re-entering
     * the event loop since there is a busy Lua script running in timeout
     * condition, to avoid mixing the propagation of scripts with the
     * propagation of DELs due to eviction.
     */
    // 每次执行命令前先检查内存是否充足,如果内存不够就要尝试按配置的淘汰策略淘汰掉一部分内存
    if (server.maxmemory && !server.lua_timedout) {
        // 如果内存已超过限制,那么尝试通过删除过期键来释放内存
        int out_of_memory = (performEvictions() == EVICT_FAIL);
        /* performEvictions may flush slave output buffers. This may result
         * in a slave, that may be the active client, to be freed. */
        if (server.current_client == NULL)
            return C_ERR;

        int reject_cmd_on_oom = is_denyoom_command;
        /* If client is in MULTI/EXEC context, queuing may consume an unlimited
         * amount of memory, so we want to stop that.
         * However, we never want to reject DISCARD, or even EXEC (unless it
         * contains denied commands, in which case is_denyoom_command is already
         * set. */
        if (c->flags & CLIENT_MULTI && c->cmd->proc != execCommand && c->cmd->proc != discardCommand &&
            c->cmd->proc != resetCommand) {
            reject_cmd_on_oom = 1;
        }

        if (out_of_memory && reject_cmd_on_oom) {
            rejectCommand(c, shared.oomerr);
            return C_OK;
        }

        /* Save out_of_memory result at script start, otherwise if we check OOM
         * until first write within script, memory used by lua stack and
         * arguments might interfere. */
        // 这段代码的注释解释了在脚本开始时保存内存不足(OOM)的结果,以避免Lua栈和参数占用的内存对后续检查产生干扰。
        if (c->cmd->proc == evalCommand || c->cmd->proc == evalShaCommand) {
            server.lua_oom = out_of_memory;
        }
    }

    /* Make sure to use a reasonable amount of memory for client side
     * caching metadata. */
    if (server.tracking_clients)
        trackingLimitUsedSlots();

    /* Don't accept write commands if there are problems persisting on disk
     * and if this is a master instance. */
    // 这段代码的功能是:如果当前实例是主服务器,并且磁盘持久化存在问题,则不接受写命令。
    // 这确保了在磁盘出现问题时,主服务器不会继续处理写操作,从而避免数据丢失或损坏
    int deny_write_type = writeCommandsDeniedByDiskError();
    if (deny_write_type != DISK_ERROR_TYPE_NONE && server.masterhost == NULL &&
        (is_write_command || c->cmd->proc == pingCommand)) {
        if (deny_write_type == DISK_ERROR_TYPE_RDB)
            rejectCommand(c, shared.bgsaveerr);
        else
            rejectCommandFormat(c, "-MISCONF Errors writing to the AOF file: %s",
                                strerror(server.aof_last_write_errno));
        return C_OK;
    }

    /* Don't accept write commands if there are not enough good slaves and
     * user configured the min-slaves-to-write option. */
    // 这段代码的功能是:在用户配置了最小从节点写入数量(min-slaves-to-write)的情况下,如果可用的从节点数量不足,则不接受写命令
    if (server.masterhost == NULL && server.repl_min_slaves_to_write && server.repl_min_slaves_max_lag &&
        is_write_command && server.repl_good_slaves_count < server.repl_min_slaves_to_write) {
        rejectCommand(c, shared.noreplicaserr);
        return C_OK;
    }

    /* Don't accept write commands if this is a read only slave. But
     * accept write commands if this is our master. */
    // 这段代码的功能是:如果当前实例是只读从服务器,则拒绝写入命令;但如果当前实例是主服务器,则接受写入命令。
    // 逻辑主要用于区分主从角色,确保只读从服务器不会执行写操作。
    if (server.masterhost && server.repl_slave_ro && !(c->flags & CLIENT_MASTER) && is_write_command) {
        rejectCommand(c, shared.roslaveerr);
        return C_OK;
    }

    /* Only allow a subset of commands in the context of Pub/Sub if the
     * connection is in RESP2 mode. With RESP3 there are no limits. */
    // 这段代码的功能是限制在Pub/Sub上下文中仅允许一部分命令,前提是连接处于RESP2模式。如果连接使用RESP3模式,则没有命令限制。
    if ((c->flags & CLIENT_PUBSUB && c->resp == 2) && c->cmd->proc != pingCommand && c->cmd->proc != subscribeCommand &&
        c->cmd->proc != unsubscribeCommand && c->cmd->proc != psubscribeCommand &&
        c->cmd->proc != punsubscribeCommand && c->cmd->proc != resetCommand) {
        rejectCommandFormat(c,
                            "Can't execute '%s': only (P)SUBSCRIBE / "
                            "(P)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context",
                            c->cmd->name);
        return C_OK;
    }

    /* Only allow commands with flag "t", such as INFO, SLAVEOF and so on,
     * when slave-serve-stale-data is no and we are a slave with a broken
     * link with master. */
    // 这段代码的功能是:当从服务器与主服务器的连接中断且配置项 slave-serve-stale-data 设置为否时,仅允许带有 "t"
    // 标志的命令(如 INFO、SLAVEOF 等)执行。
    if (server.masterhost && server.repl_state != REPL_STATE_CONNECTED && server.repl_serve_stale_data == 0 &&
        is_denystale_command) {
        rejectCommand(c, shared.masterdownerr);
        return C_OK;
    }

    /* Loading DB? Return an error if the command has not the
     * CMD_LOADING flag. */
    // 这段代码的功能是检查当前是否正在加载数据库。如果正在加载数据库,并且调用的命令没有设置CMD_LOADING标志,则返回错误。
    if (server.loading && is_denyloading_command) {
        rejectCommand(c, shared.loadingerr);
        return C_OK;
    }

    /* Lua script too slow? Only allow a limited number of commands.
     * Note that we need to allow the transactions commands, otherwise clients
     * sending a transaction with pipelining without error checking, may have
     * the MULTI plus a few initial commands refused, then the timeout
     * condition resolves, and the bottom-half of the transaction gets
     * executed, see Github PR #7022. */
    // 这段代码的功能是限制Lua脚本中命令的数量,以防止脚本执行过慢。
    // 同时,为了确保事务命令(如MULTI)能够正常执行,避免因超时导致部分事务命令被拒绝,从而影响事务的完整性。
    if (server.lua_timedout && c->cmd->proc != authCommand && c->cmd->proc != helloCommand &&
        c->cmd->proc != replconfCommand && c->cmd->proc != multiCommand && c->cmd->proc != discardCommand &&
        c->cmd->proc != watchCommand && c->cmd->proc != unwatchCommand &&
        !(c->cmd->proc == shutdownCommand && c->argc == 2 && tolower(((char *) c->argv[1]->ptr)[0]) == 'n') &&
        !(c->cmd->proc == scriptCommand && c->argc == 2 && tolower(((char *) c->argv[1]->ptr)[0]) == 'k')) {
        rejectCommand(c, shared.slowscripterr);
        return C_OK;
    }

    /* If the server is paused, block the client until
     * the pause has ended. Replicas are never paused. */
    // 如果服务器处于暂停状态,则阻塞客户端直到暂停结束。副本永远不会被暂停。
    if (!(c->flags & CLIENT_SLAVE) && ((server.client_pause_type == CLIENT_PAUSE_ALL) ||
                                       (server.client_pause_type == CLIENT_PAUSE_WRITE && is_may_replicate_command))) {
        c->bpop.timeout = 0;
        blockClient(c, BLOCKED_PAUSE);
        return C_OK;
    }

    // 执行命令
    // 这段代码检查客户端是否处于多命令事务模式(CLIENT_MULTI),并且当前命令不是特定的几个命令(exec、discard、multi、watch、reset)。
    if (c->flags & CLIENT_MULTI && c->cmd->proc != execCommand && c->cmd->proc != discardCommand &&
        c->cmd->proc != multiCommand && c->cmd->proc != watchCommand && c->cmd->proc != resetCommand) {
        // 调用 queueMultiCommand 函数,将命令添加到多命令队列中。
        // 此函数通常用于批量处理多个命令,提高执行效率。
        queueMultiCommand(c);
        addReply(c, shared.queued);
    } else {
        // 调用call执行命令
        call(c, CMD_CALL_FULL);
        // 这段代码的功能是将服务器的主复制偏移量(server.master_repl_offset)赋值给对象c的woff成员变量。
        // 这通常用于记录或同步复制状态。
        c->woff = server.master_repl_offset;
        if (listLength(server.ready_keys))
            handleClientsBlockedOnKeys();
    }

    return C_OK;
}

/* ====================== Error lookup and execution ===================== */
// 增加错误统计
/*================================== Shutdown =============================== */

/* Close listening sockets. Also unlink the unix domain socket if
 * unlink_unix_socket is non-zero. */
// 关闭监听的socket
void closeListeningSockets(int unlink_unix_socket) {
    int j;

    for (j = 0; j < server.ipfd_count; j++)
        close(server.ipfd[j]);
    for (j = 0; j < server.tlsfd_count; j++)
        close(server.tlsfd[j]);
    if (server.sofd != -1)
        close(server.sofd);
    if (server.cluster_enabled)
        for (j = 0; j < server.cfd_count; j++)
            close(server.cfd[j]);
    if (unlink_unix_socket && server.unixsocket) {
        serverLog(LL_NOTICE, "Removing the unix socket file.");
        unlink(server.unixsocket); /* don't care if this fails */
    }
}

// 服务停止前做一些收尾工作,比如保存rdb
int prepareForShutdown(int flags) {
    /* When SHUTDOWN is called while the server is loading a dataset in
     * memory we need to make sure no attempt is performed to save
     * the dataset on shutdown (otherwise it could overwrite the current DB
     * with half-read data).
     *
     * Also when in Sentinel mode clear the SAVE flag and force NOSAVE. */
    if (server.loading || server.sentinel_mode)
        flags = (flags & ~SHUTDOWN_SAVE) | SHUTDOWN_NOSAVE;

    int save = flags & SHUTDOWN_SAVE;
    int nosave = flags & SHUTDOWN_NOSAVE;

    serverLog(LL_WARNING, "User requested shutdown...");
    if (server.supervised_mode == SUPERVISED_SYSTEMD)
        redisCommunicateSystemd("STOPPING=1\n");

    /* Kill all the Lua debugger forked sessions. */
    ldbKillForkedSessions();

    /* Kill the saving child if there is a background saving in progress.
       We want to avoid race conditions, for instance our saving child may
       overwrite the synchronous saving did by SHUTDOWN. */
    if (server.child_type == CHILD_TYPE_RDB) {
        serverLog(LL_WARNING, "There is a child saving an .rdb. Killing it!");
        killRDBChild();
        /* Note that, in killRDBChild normally has backgroundSaveDoneHandler
         * doing it's cleanup, but in this case this code will not be reached,
         * so we need to call rdbRemoveTempFile which will close fd(in order
         * to unlink file actully) in background thread.
         * The temp rdb file fd may won't be closed when redis exits quickly,
         * but OS will close this fd when process exits. */
        rdbRemoveTempFile(server.child_pid, 0);
    }

    /* Kill module child if there is one. */
    if (server.child_type == CHILD_TYPE_MODULE) {
        serverLog(LL_WARNING, "There is a module fork child. Killing it!");
        terminateModuleForkChild(server.child_pid, 0);
    }

    if (server.aof_state != AOF_OFF) {
        /* Kill the AOF saving child as the AOF we already have may be longer
         * but contains the full dataset anyway. */
        if (server.child_type == CHILD_TYPE_AOF) {
            /* If we have AOF enabled but haven't written the AOF yet, don't
             * shutdown or else the dataset will be lost. */
            if (server.aof_state == AOF_WAIT_REWRITE) {
                serverLog(LL_WARNING, "Writing initial AOF, can't exit.");
                return C_ERR;
            }
            serverLog(LL_WARNING, "There is a child rewriting the AOF. Killing it!");
            killAppendOnlyChild();
        }
        /* Append only file: flush buffers and fsync() the AOF at exit */
        serverLog(LL_NOTICE, "Calling fsync() on the AOF file.");
        flushAppendOnlyFile(1);
        redis_fsync(server.aof_fd);
    }

    /* 服务退出前存一份全量的rdb */
    if ((server.saveparamslen > 0 && !nosave) || save) {
        serverLog(LL_NOTICE, "Saving the final RDB snapshot before exiting.");
        if (server.supervised_mode == SUPERVISED_SYSTEMD)
            redisCommunicateSystemd("STATUS=Saving the final RDB snapshot\n");
        /* Snapshotting. Perform a SYNC SAVE and exit */
        rdbSaveInfo rsi, *rsiptr;
        rsiptr = rdbPopulateSaveInfo(&rsi);
        if (rdbSave(server.rdb_filename, rsiptr) != C_OK) {
            /* Ooops.. error saving! The best we can do is to continue
             * operating. Note that if there was a background saving process,
             * in the next cron() Redis will be notified that the background
             * saving aborted, handling special stuff like slaves pending for
             * synchronization... */
            serverLog(LL_WARNING, "Error trying to save the DB, can't exit.");
            if (server.supervised_mode == SUPERVISED_SYSTEMD)
                redisCommunicateSystemd("STATUS=Error trying to save the DB, can't exit.\n");
            return C_ERR;
        }
    }

    /* Fire the shutdown modules event. */
    moduleFireServerEvent(REDISMODULE_EVENT_SHUTDOWN, 0, NULL);

    /* Remove the pid file if possible and needed. */
    if (server.daemonize || server.pidfile) {
        serverLog(LL_NOTICE, "Removing the pid file.");
        unlink(server.pidfile);
    }

    /* Best effort flush of slave output buffers, so that we hopefully
     * send them pending writes. */
    flushSlavesOutputBuffers();

    /* Close the listening sockets. Apparently this allows faster restarts. */
    closeListeningSockets(1);
    serverLog(LL_WARNING, "%s is now ready to exit, bye bye...", server.sentinel_mode ? "Sentinel" : "Redis");
    return C_OK;
}

/*================================== Commands =============================== */

/* Sometimes Redis cannot accept write commands because there is a persistence
 * error with the RDB or AOF file, and Redis is configured in order to stop
 * accepting writes in such situation. This function returns if such a
 * condition is active, and the type of the condition.
 *
 * Function return values:
 *
 * DISK_ERROR_TYPE_NONE:    No problems, we can accept writes.
 * DISK_ERROR_TYPE_AOF:     Don't accept writes: AOF errors.
 * DISK_ERROR_TYPE_RDB:     Don't accept writes: RDB errors.
 */
int writeCommandsDeniedByDiskError(void) {
    if (server.stop_writes_on_bgsave_err && server.saveparamslen > 0 && server.lastbgsave_status == C_ERR) {
        return DISK_ERROR_TYPE_RDB;
    } else if (server.aof_state != AOF_OFF && server.aof_last_write_status == C_ERR) {
        return DISK_ERROR_TYPE_AOF;
    } else {
        return DISK_ERROR_TYPE_NONE;
    }
}

/* =================================== Main! ================================ */

int checkIgnoreWarning(const char *warning) {
    int argc, j;
    sds *argv = sdssplitargs(server.ignore_warnings, &argc);
    if (argv == NULL)
        return 0;

    for (j = 0; j < argc; j++) {
        char *flag = argv[j];
        if (!strcasecmp(flag, warning))
            break;
    }
    sdsfreesplitres(argv, argc);
    return j < argc;
}


