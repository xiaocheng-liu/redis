/* 内置命令注册、元数据查询及基础管理命令实现。 */
#include <sys/time.h>
#include "version.h"
#include "server.h"
#include "server_commands.h"
#include "slowlog.h"

/* Our command table.
 * Every entry is composed of the following fields:
 *
 * name:        A string representing the command name.
 * function:    Pointer to the C function implementing the command.
 * arity:       Number of arguments, it is possible to use -N to say >= N
 * sflags:      Command flags as string. See below for a table of flags.
 * flags:       Flags as bitmask. Computed by Redis using the 'sflags' field.
 * get_keys_proc: An optional function to get key arguments from a command.
 *                This is only used when the following three fields are not
 *                enough to specify what arguments are keys.
 * first_key_index: First argument that is a key
 * last_key_index: Last argument that is a key
 * key_step:    Step to get all the keys from first to last argument.
 *              For instance in MSET the step is two since arguments
 *              are key,val,key,val,...
 * microseconds: Microseconds of total execution time for this command.
 * calls:       Total number of calls of this command.
 * id:          Command bit identifier for ACLs or other goals.
 * The flags, microseconds and calls fields are computed by Redis and should
 * always be set to zero.
 * Command flags are expressed using space separated strings, that are turned
 * into actual flags by the populateCommandTable() function.
 * This is the meaning of the flags:
 * write:       Write command (may modify the key space).
 * read-only:   Commands just reading from keys without changing the content.
 *              Note that commands that don't read from the keyspace such as
 *              TIME, SELECT, INFO, administrative commands, and connection
 *              or transaction related commands (multi, exec, discard, ...)
 *              are not flagged as read-only commands, since they affect the
 *              server or the connection in other ways.
 * use-memory:  May increase memory usage once called. Don't allow if out
 *              of memory.
 * admin:       Administrative command, like SAVE or SHUTDOWN.
 * pub-sub:     Pub/Sub related command.
 * no-script:   Command not allowed in scripts.
 * random:      Random command. Command is not deterministic, that is, the same
 *              command with the same arguments, with the same key space, may
 *              have different results. For instance SPOP and RANDOMKEY are
 *              two random commands.
 * to-sort:     Sort command output array if called from script, so that the
 *              output is deterministic. When this flag is used (not always
 *              possible), then the "random" flag is not needed.
 * ok-loading:  Allow the command while loading the database.
 * ok-stale:    Allow the command while a slave has stale data but is not
 *              allowed to serve this data. Normally no command is accepted
 *              in this condition but just a few.
 * no-monitor:  Do not automatically propagate the command on MONITOR.
 * no-slowlog:  Do not automatically propagate the command to the slowlog.
 * cluster-asking: Perform an implicit ASKING for this command, so the
 *              command will be accepted in cluster mode if the slot is marked
 *              as 'importing'.
 * fast:        Fast command: O(1) or O(log(N)) command that should never
 *              delay its execution as long as the kernel scheduler is giving
 *              us time. Note that commands that may trigger a DEL as a side
 *              effect (like SET) are not fast commands.
 * may-replicate: Command may produce replication traffic, but should be
 *                allowed under circumstances where write commands are disallowed.
 *                Examples include PUBLISH, which replicates pubsub messages,and
 *                EVAL, which may execute write commands, which are replicated,
 *                or may just execute read commands. A command can not be marked
 *                both "write" and "may-replicate"
 * The following additional flags are only used in order to put commands
 * in a specific ACL category. Commands can have multiple ACL categories.
 *
 * @keyspace, @read, @write, @set, @sortedset, @list, @hash, @string, @bitmap,
 * @hyperloglog, @stream, @admin, @fast, @slow, @pubsub, @blocking, @dangerous,
 * @connection, @transaction, @scripting, @geo.
 *
 * Note that:
 *
 * 1) The read-only flag implies the @read ACL category.
 * 2) The write flag implies the @write ACL category.
 * 3) The fast flag implies the @fast ACL category.
 * 4) The admin flag implies the @admin and @dangerous ACL category.
 * 5) The pub-sub flag implies the @pubsub ACL category.
 * 6) The lack of fast flag implies the @slow ACL category.
 * 7) The non obvious "keyspace" category includes the commands
 *    that interact with keys without having anything to do with
 *    specific data structures, such as: DEL, RENAME, MOVE, SELECT,
 *    TYPE, EXPIRE*, PEXPIRE*, TTL, PTTL, ...
 */
// 内置命令定义及运行时统计,由命令字典引用同一份表项。
static struct redisCommand redisCommandTable[] = {
        {"module", moduleCommand, -2, "admin no-script", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"get", getCommand, 2, "read-only fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"getex", getexCommand, -2, "write fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"getdel", getdelCommand, 2, "write fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        /* Note that we can't flag set as fast, since it may perform an
         * implicit DEL of a large key. */
        {"set", setCommand, -3, "write use-memory @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"setnx", setnxCommand, 3, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"setex", setexCommand, 4, "write use-memory @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"psetex", psetexCommand, 4, "write use-memory @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"append", appendCommand, 3, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"strlen", strlenCommand, 2, "read-only fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"del", delCommand, -2, "write @keyspace", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"unlink", unlinkCommand, -2, "write fast @keyspace", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"exists", existsCommand, -2, "read-only fast @keyspace", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"setbit", setbitCommand, 4, "write use-memory @bitmap", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"getbit", getbitCommand, 3, "read-only fast @bitmap", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"bitfield", bitfieldCommand, -2, "write use-memory @bitmap", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"bitfield_ro", bitfieldroCommand, -2, "read-only fast @bitmap", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"setrange", setrangeCommand, 4, "write use-memory @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"getrange", getrangeCommand, 4, "read-only @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"substr", getrangeCommand, 4, "read-only @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"incr", incrCommand, 2, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"decr", decrCommand, 2, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"mget", mgetCommand, -2, "read-only fast @string", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"rpush", rpushCommand, -3, "write use-memory fast @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lpush", lpushCommand, -3, "write use-memory fast @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"rpushx", rpushxCommand, -3, "write use-memory fast @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lpushx", lpushxCommand, -3, "write use-memory fast @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"linsert", linsertCommand, 5, "write use-memory @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"rpop", rpopCommand, -2, "write fast @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lpop", lpopCommand, -2, "write fast @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"brpop", brpopCommand, -3, "write no-script @list @blocking", 0, NULL, 1, -2, 1, 0, 0, 0},
        {"brpoplpush", brpoplpushCommand, 4, "write use-memory no-script @list @blocking", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"blmove", blmoveCommand, 6, "write use-memory no-script @list @blocking", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"blpop", blpopCommand, -3, "write no-script @list @blocking", 0, NULL, 1, -2, 1, 0, 0, 0},
        {"llen", llenCommand, 2, "read-only fast @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lindex", lindexCommand, 3, "read-only @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lset", lsetCommand, 4, "write use-memory @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lrange", lrangeCommand, 4, "read-only @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"ltrim", ltrimCommand, 4, "write @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lpos", lposCommand, -3, "read-only @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"lrem", lremCommand, 4, "write @list", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"rpoplpush", rpoplpushCommand, 3, "write use-memory @list", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"lmove", lmoveCommand, 5, "write use-memory @list", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"sadd", saddCommand, -3, "write use-memory fast @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"srem", sremCommand, -3, "write fast @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"smove", smoveCommand, 4, "write fast @set", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"sismember", sismemberCommand, 3, "read-only fast @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"smismember", smismemberCommand, -3, "read-only fast @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"scard", scardCommand, 2, "read-only fast @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"spop", spopCommand, -2, "write random fast @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"srandmember", srandmemberCommand, -2, "read-only random @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"sinter", sinterCommand, -2, "read-only to-sort @set", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"sinterstore", sinterstoreCommand, -3, "write use-memory @set", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"sunion", sunionCommand, -2, "read-only to-sort @set", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"sunionstore", sunionstoreCommand, -3, "write use-memory @set", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"sdiff", sdiffCommand, -2, "read-only to-sort @set", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"sdiffstore", sdiffstoreCommand, -3, "write use-memory @set", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"smembers", sinterCommand, 2, "read-only to-sort @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"sscan", sscanCommand, -3, "read-only random @set", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zadd", zaddCommand, -4, "write use-memory fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zincrby", zincrbyCommand, 4, "write use-memory fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrem", zremCommand, -3, "write fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zremrangebyscore", zremrangebyscoreCommand, 4, "write @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zremrangebyrank", zremrangebyrankCommand, 4, "write @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zremrangebylex", zremrangebylexCommand, 4, "write @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zunionstore", zunionstoreCommand, -4, "write use-memory @sortedset", 0, zunionInterDiffStoreGetKeys, 1, 1, 1,
         0, 0, 0},
        {"zinterstore", zinterstoreCommand, -4, "write use-memory @sortedset", 0, zunionInterDiffStoreGetKeys, 1, 1, 1,
         0, 0, 0},
        {"zdiffstore", zdiffstoreCommand, -4, "write use-memory @sortedset", 0, zunionInterDiffStoreGetKeys, 1, 1, 1, 0,
         0, 0},
        {"zunion", zunionCommand, -3, "read-only @sortedset", 0, zunionInterDiffGetKeys, 0, 0, 0, 0, 0, 0},
        {"zinter", zinterCommand, -3, "read-only @sortedset", 0, zunionInterDiffGetKeys, 0, 0, 0, 0, 0, 0},
        {"zdiff", zdiffCommand, -3, "read-only @sortedset", 0, zunionInterDiffGetKeys, 0, 0, 0, 0, 0, 0},
        {"zrange", zrangeCommand, -4, "read-only @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrangestore", zrangestoreCommand, -5, "write use-memory @sortedset", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"zrangebyscore", zrangebyscoreCommand, -4, "read-only @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrevrangebyscore", zrevrangebyscoreCommand, -4, "read-only @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrangebylex", zrangebylexCommand, -4, "read-only @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrevrangebylex", zrevrangebylexCommand, -4, "read-only @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zcount", zcountCommand, 4, "read-only fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zlexcount", zlexcountCommand, 4, "read-only fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrevrange", zrevrangeCommand, -4, "read-only @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zcard", zcardCommand, 2, "read-only fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zscore", zscoreCommand, 3, "read-only fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zmscore", zmscoreCommand, -3, "read-only fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrank", zrankCommand, 3, "read-only fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zrevrank", zrevrankCommand, 3, "read-only fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zscan", zscanCommand, -3, "read-only random @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zpopmin", zpopminCommand, -2, "write fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"zpopmax", zpopmaxCommand, -2, "write fast @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"bzpopmin", bzpopminCommand, -3, "write no-script fast @sortedset @blocking", 0, NULL, 1, -2, 1, 0, 0, 0},
        {"bzpopmax", bzpopmaxCommand, -3, "write no-script fast @sortedset @blocking", 0, NULL, 1, -2, 1, 0, 0, 0},
        {"zrandmember", zrandmemberCommand, -2, "read-only random @sortedset", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hset", hsetCommand, -4, "write use-memory fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hsetnx", hsetnxCommand, 4, "write use-memory fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hget", hgetCommand, 3, "read-only fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hmset", hsetCommand, -4, "write use-memory fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hmget", hmgetCommand, -3, "read-only fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hincrby", hincrbyCommand, 4, "write use-memory fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hincrbyfloat", hincrbyfloatCommand, 4, "write use-memory fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hdel", hdelCommand, -3, "write fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hlen", hlenCommand, 2, "read-only fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hstrlen", hstrlenCommand, 3, "read-only fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hkeys", hkeysCommand, 2, "read-only to-sort @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hvals", hvalsCommand, 2, "read-only to-sort @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hgetall", hgetallCommand, 2, "read-only random @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hexists", hexistsCommand, 3, "read-only fast @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hrandfield", hrandfieldCommand, -2, "read-only random @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"hscan", hscanCommand, -3, "read-only random @hash", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"incrby", incrbyCommand, 3, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"decrby", decrbyCommand, 3, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"incrbyfloat", incrbyfloatCommand, 3, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"getset", getsetCommand, 3, "write use-memory fast @string", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"mset", msetCommand, -3, "write use-memory @string", 0, NULL, 1, -1, 2, 0, 0, 0},
        {"msetnx", msetnxCommand, -3, "write use-memory @string", 0, NULL, 1, -1, 2, 0, 0, 0},
        {"randomkey", randomkeyCommand, 1, "read-only random @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"select", selectCommand, 2, "ok-loading fast ok-stale @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"swapdb", swapdbCommand, 3, "write fast @keyspace @dangerous", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"move", moveCommand, 3, "write fast @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"copy", copyCommand, -3, "write use-memory @keyspace", 0, NULL, 1, 2, 1, 0, 0, 0},

        /* Like for SET, we can't mark rename as a fast command because
         * overwriting the target key may result in an implicit slow DEL. */
        {"rename", renameCommand, 3, "write @keyspace", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"renamenx", renamenxCommand, 3, "write fast @keyspace", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"expire", expireCommand, 3, "write fast @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"expireat", expireatCommand, 3, "write fast @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"pexpire", pexpireCommand, 3, "write fast @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"pexpireat", pexpireatCommand, 3, "write fast @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"keys", keysCommand, 2, "read-only to-sort @keyspace @dangerous", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"scan", scanCommand, -2, "read-only random @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"dbsize", dbsizeCommand, 1, "read-only fast @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"auth", authCommand, -2, "no-auth no-script ok-loading ok-stale fast no-monitor no-slowlog @connection", 0,
         NULL, 0, 0, 0, 0, 0, 0},

        /* We don't allow PING during loading since in Redis PING is used as
         * failure detection, and a loading server is considered to be
         * not available. */
        {"ping", pingCommand, -1, "ok-stale fast @connection", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"echo", echoCommand, 2, "fast @connection", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"save", saveCommand, 1, "admin no-script", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"bgsave", bgsaveCommand, -1, "admin no-script", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"bgrewriteaof", bgrewriteaofCommand, 1, "admin no-script", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"shutdown", shutdownCommand, -1, "admin no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"lastsave", lastsaveCommand, 1, "random fast ok-loading ok-stale @admin @dangerous", 0, NULL, 0, 0, 0, 0, 0,
         0},

        // 查看数据类型
        {"type", typeCommand, 2, "read-only fast @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"multi", multiCommand, 1, "no-script fast ok-loading ok-stale @transaction", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"exec", execCommand, 1, "no-script no-monitor no-slowlog ok-loading ok-stale @transaction", 0, NULL, 0, 0, 0,
         0, 0, 0},
        {"discard", discardCommand, 1, "no-script fast ok-loading ok-stale @transaction", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"sync", syncCommand, 1, "admin no-script", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"psync", syncCommand, -3, "admin no-script", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"replconf", replconfCommand, -1, "admin no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"flushdb", flushdbCommand, -1, "write @keyspace @dangerous", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"flushall", flushallCommand, -1, "write @keyspace @dangerous", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"sort", sortCommand, -2, "write use-memory @list @set @sortedset @dangerous", 0, sortGetKeys, 1, 1, 1, 0, 0,
         0},

        {"info", infoCommand, -1, "ok-loading ok-stale random @dangerous", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"version", versionCommand, -1, "ok-loading ok-stale random @dangerous", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"monitor", monitorCommand, 1, "admin no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"ttl", ttlCommand, 2, "read-only fast random @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"touch", touchCommand, -2, "read-only fast @keyspace", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"pttl", pttlCommand, 2, "read-only fast random @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"persist", persistCommand, 2, "write fast @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},

        /* slaveof和replicaof这两个命令调用同一个函数 */
        {"slaveof", replicaofCommand, 3, "admin no-script ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"replicaof", replicaofCommand, 3, "admin no-script ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"role", roleCommand, 1, "ok-loading ok-stale no-script fast @dangerous", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"debug", debugCommand, -2, "admin no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"config", configCommand, -2, "admin ok-loading ok-stale no-script", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"subscribe", subscribeCommand, -2, "pub-sub no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"unsubscribe", unsubscribeCommand, -1, "pub-sub no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"psubscribe", psubscribeCommand, -2, "pub-sub no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"punsubscribe", punsubscribeCommand, -1, "pub-sub no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"publish", publishCommand, 3, "pub-sub ok-loading ok-stale fast may-replicate", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"pubsub", pubsubCommand, -2, "pub-sub ok-loading ok-stale random", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"watch", watchCommand, -2, "no-script fast ok-loading ok-stale @transaction", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"unwatch", unwatchCommand, 1, "no-script fast ok-loading ok-stale @transaction", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"cluster", clusterCommand, -2, "admin ok-stale random", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"restore", restoreCommand, -4, "write use-memory @keyspace @dangerous", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"restore-asking", restoreCommand, -4, "write use-memory cluster-asking @keyspace @dangerous", 0, NULL, 1, 1, 1,
         0, 0, 0},
        {"migrate", migrateCommand, -6, "write random @keyspace @dangerous", 0, migrateGetKeys, 0, 0, 0, 0, 0, 0},
        {"asking", askingCommand, 1, "fast @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"readonly", readonlyCommand, 1, "fast @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"readwrite", readwriteCommand, 1, "fast @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"dump", dumpCommand, 2, "read-only random @keyspace", 0, NULL, 1, 1, 1, 0, 0, 0},

        // object命令
        {"object", objectCommand, -2, "read-only random @keyspace", 0, NULL, 2, 2, 1, 0, 0, 0},
        {"memory", memoryCommand, -2, "random read-only", 0, memoryGetKeys, 0, 0, 0, 0, 0, 0},
        {"client", clientCommand, -2, "admin no-script random ok-loading ok-stale @connection", 0, NULL, 0, 0, 0, 0, 0,
         0},
        {"hello", helloCommand, -1, "no-auth no-script fast no-monitor ok-loading ok-stale no-slowlog @connection", 0,
         NULL, 0, 0, 0, 0, 0, 0},

        /* EVAL can modify the dataset, however it is not flagged as a write
         * command since we do the check while running commands from Lua. */
        {"eval", evalCommand, -3, "no-script may-replicate @scripting", 0, evalGetKeys, 0, 0, 0, 0, 0, 0},
        {"evalsha", evalShaCommand, -3, "no-script may-replicate @scripting", 0, evalGetKeys, 0, 0, 0, 0, 0, 0},
        {"slowlog", slowlogCommand, -2, "admin random ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"script", scriptCommand, -2, "no-script may-replicate @scripting", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"time", timeCommand, 1, "random fast ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"bitop", bitopCommand, -4, "write use-memory @bitmap", 0, NULL, 2, -1, 1, 0, 0, 0},
        {"bitcount", bitcountCommand, -2, "read-only @bitmap", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"bitpos", bitposCommand, -3, "read-only @bitmap", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"wait", waitCommand, 3, "no-script @keyspace", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"command", commandCommand, -1, "ok-loading ok-stale random @connection", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"geoadd", geoaddCommand, -5, "write use-memory @geo", 0, NULL, 1, 1, 1, 0, 0, 0},

        /* GEORADIUS has store options that may write. */
        {"georadius", georadiusCommand, -6, "write use-memory @geo", 0, georadiusGetKeys, 1, 1, 1, 0, 0, 0},
        {"georadius_ro", georadiusroCommand, -6, "read-only @geo", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"georadiusbymember", georadiusbymemberCommand, -5, "write use-memory @geo", 0, georadiusGetKeys, 1, 1, 1, 0, 0,
         0},
        {"georadiusbymember_ro", georadiusbymemberroCommand, -5, "read-only @geo", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"geohash", geohashCommand, -2, "read-only @geo", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"geopos", geoposCommand, -2, "read-only @geo", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"geodist", geodistCommand, -4, "read-only @geo", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"geosearch", geosearchCommand, -7, "read-only @geo", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"geosearchstore", geosearchstoreCommand, -8, "write use-memory @geo", 0, NULL, 1, 2, 1, 0, 0, 0},
        {"pfselftest", pfselftestCommand, 1, "admin @hyperloglog", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"pfadd", pfaddCommand, -2, "write use-memory fast @hyperloglog", 0, NULL, 1, 1, 1, 0, 0, 0},

        /* Technically speaking PFCOUNT may change the key since it changes the
         * final bytes in the HyperLogLog representation. However in this case
         * we claim that the representation, even if accessible, is an internal
         * affair, and the command is semantically read only. */
        {"pfcount", pfcountCommand, -2, "read-only may-replicate @hyperloglog", 0, NULL, 1, -1, 1, 0, 0, 0},
        {"pfmerge", pfmergeCommand, -2, "write use-memory @hyperloglog", 0, NULL, 1, -1, 1, 0, 0, 0},

        /* Unlike PFCOUNT that is considered as a read-only command (although
         * it changes a bit), PFDEBUG may change the entire key when converting
         * from sparse to dense representation */
        {"pfdebug", pfdebugCommand, -3, "admin write use-memory @hyperloglog", 0, NULL, 2, 2, 1, 0, 0, 0},
        {"xadd", xaddCommand, -5, "write use-memory fast random @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xrange", xrangeCommand, -4, "read-only @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xrevrange", xrevrangeCommand, -4, "read-only @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xlen", xlenCommand, 2, "read-only fast @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xread", xreadCommand, -4, "read-only @stream @blocking", 0, xreadGetKeys, 0, 0, 0, 0, 0, 0},
        {"xreadgroup", xreadCommand, -7, "write @stream @blocking", 0, xreadGetKeys, 0, 0, 0, 0, 0, 0},
        {"xgroup", xgroupCommand, -2, "write use-memory @stream", 0, NULL, 2, 2, 1, 0, 0, 0},
        {"xsetid", xsetidCommand, 3, "write use-memory fast @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xack", xackCommand, -4, "write fast random @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xpending", xpendingCommand, -3, "read-only random @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xclaim", xclaimCommand, -6, "write random fast @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xautoclaim", xautoclaimCommand, -6, "write random fast @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xinfo", xinfoCommand, -2, "read-only random @stream", 0, NULL, 2, 2, 1, 0, 0, 0},
        {"xdel", xdelCommand, -3, "write fast @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"xtrim", xtrimCommand, -2, "write random @stream", 0, NULL, 1, 1, 1, 0, 0, 0},
        {"post", securityWarningCommand, -1, "ok-loading ok-stale read-only", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"host:", securityWarningCommand, -1, "ok-loading ok-stale read-only", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"latency", latencyCommand, -2, "admin no-script ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"lolwut", lolwutCommand, -1, "read-only fast", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"acl", aclCommand, -2, "admin no-script no-slowlog ok-loading ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"stralgo", stralgoCommand, -2, "read-only @string", 0, lcsGetKeys, 0, 0, 0, 0, 0, 0},
        {"reset", resetCommand, 1, "no-script ok-stale ok-loading fast @connection", 0, NULL, 0, 0, 0, 0, 0, 0},
        {"failover", failoverCommand, -1, "admin no-script ok-stale", 0, NULL, 0, 0, 0, 0, 0, 0}};

/* Parse the flags string description 'strflags' and set them to the
 * command 'c'. If the flags are all valid C_OK is returned, otherwise
 * C_ERR is returned (yet the recognized flags are set in the command). */
// 解析命令标志及 ACL 分类;成功返回 C_OK,遇到未知标志返回 C_ERR。
int populateCommandTableParseFlags(struct redisCommand *c, char *strflags) {
    int argc;
    sds *argv;

    /* Split the line into arguments for processing. */
    argv = sdssplitargs(strflags, &argc);
    if (argv == NULL)
        return C_ERR;

    for (int j = 0; j < argc; j++) {
        char *flag = argv[j];
        if (!strcasecmp(flag, "write")) {
            c->flags |= CMD_WRITE | CMD_CATEGORY_WRITE;
        } else if (!strcasecmp(flag, "read-only")) {
            c->flags |= CMD_READONLY | CMD_CATEGORY_READ;
        } else if (!strcasecmp(flag, "use-memory")) {
            c->flags |= CMD_DENYOOM;
        } else if (!strcasecmp(flag, "admin")) {
            c->flags |= CMD_ADMIN | CMD_CATEGORY_ADMIN | CMD_CATEGORY_DANGEROUS;
        } else if (!strcasecmp(flag, "pub-sub")) {
            c->flags |= CMD_PUBSUB | CMD_CATEGORY_PUBSUB;
        } else if (!strcasecmp(flag, "no-script")) {
            c->flags |= CMD_NOSCRIPT;
        } else if (!strcasecmp(flag, "random")) {
            c->flags |= CMD_RANDOM;
        } else if (!strcasecmp(flag, "to-sort")) {
            c->flags |= CMD_SORT_FOR_SCRIPT;
        } else if (!strcasecmp(flag, "ok-loading")) {
            c->flags |= CMD_LOADING;
        } else if (!strcasecmp(flag, "ok-stale")) {
            c->flags |= CMD_STALE;
        } else if (!strcasecmp(flag, "no-monitor")) {
            c->flags |= CMD_SKIP_MONITOR;
        } else if (!strcasecmp(flag, "no-slowlog")) {
            c->flags |= CMD_SKIP_SLOWLOG;
        } else if (!strcasecmp(flag, "cluster-asking")) {
            c->flags |= CMD_ASKING;
        } else if (!strcasecmp(flag, "fast")) {
            c->flags |= CMD_FAST | CMD_CATEGORY_FAST;
        } else if (!strcasecmp(flag, "no-auth")) {
            c->flags |= CMD_NO_AUTH;
        } else if (!strcasecmp(flag, "may-replicate")) {
            c->flags |= CMD_MAY_REPLICATE;
        } else {
            /* Parse ACL categories here if the flag name starts with @. */
            uint64_t catflag;
            if (flag[0] == '@' && (catflag = ACLGetCommandCategoryFlagByName(flag + 1)) != 0) {
                c->flags |= catflag;
            } else {
                sdsfreesplitres(argv, argc);
                return C_ERR;
            }
        }
    }
    /* If it's not @fast is @slow in this binary world. */
    if (!(c->flags & CMD_CATEGORY_FAST))
        c->flags |= CMD_CATEGORY_SLOW;

    sdsfreesplitres(argv, argc);
    return C_OK;
}

/* 将内置命令注册到运行时命令字典及不受命令重命名影响的原始字典。 */
void populateCommandTable(void) {
    int j;
    int numcommands = sizeof(redisCommandTable) / sizeof(struct redisCommand);

    for (j = 0; j < numcommands; j++) {
        struct redisCommand *c = redisCommandTable + j;
        int retval1, retval2;

        /* Translate the command string flags description into an actual
         * set of flags. */
        if (populateCommandTableParseFlags(c, c->sflags) == C_ERR)
            serverPanic("Unsupported command flag");

        c->id = ACLGetCommandID(c->name); /* Assign the ID used for ACL. */
        retval1 = dictAdd(server.commands, sdsnew(c->name), c);
        /* Populate an additional dictionary that will be unaffected
         * by rename-command statements in redis.conf. */
        retval2 = dictAdd(server.orig_commands, sdsnew(c->name), c);
        serverAssert(retval1 == DICT_OK && retval2 == DICT_OK);
    }
}

/* 基础命令:连接检查、消息回显和服务器时间查询。 */
/* The PING command. It works in a different way if the client is in
 * in Pub/Sub mode. */
// ping命令,如果客户端处于 PubSub 模式,则它以不同的方式工作。
void pingCommand(client *c) {
    /* The command takes zero or one arguments. */
    // 如果参数大于2,直接返回报错信息
    if (c->argc > 2) {
        addReplyErrorFormat(c, "wrong number of arguments for '%s' command", c->cmd->name);
        return;
    }

    // 如果客户端是发布订阅模式,并且客户端协议版本是2
    if (c->flags & CLIENT_PUBSUB && c->resp == 2) {
        addReply(c, shared.mbulkhdr[2]);
        addReplyBulkCBuffer(c, "pong", 4);
        if (c->argc == 1)
            addReplyBulkCBuffer(c, "", 0);
        else
            addReplyBulk(c, c->argv[1]);
    } else {
        if (c->argc == 1)
            addReply(c, shared.pong);
        else
            addReplyBulk(c, c->argv[1]);
    }
}

// echo命令
void echoCommand(client *c) { addReplyBulk(c, c->argv[1]); }

// time命令
void timeCommand(client *c) {
    struct timeval tv;

    /* gettimeofday() can only fail if &tv is a bad address so we
     * don't check for errors. */
    // 这段代码是对gettimeofday()函数的注释说明,表示该函数仅在传入的地址&tv无效时才会失败,因此无需检查错误。
    // 功能是获取当前时间
    gettimeofday(&tv, NULL);
    addReplyArrayLen(c, 2);
    addReplyBulkLongLong(c, tv.tv_sec);
    addReplyBulkLongLong(c, tv.tv_usec);
}


/* Returns 1 for commands that may have key names in their arguments, but have
 * no pre-determined key positions. */
int cmdHasMovableKeys(struct redisCommand *cmd) {
    return (cmd->getkeys_proc && !(cmd->flags & CMD_MODULE)) || cmd->flags & CMD_MODULE_GETKEYS;
}

/* Helper function for addReplyCommand() to output flags. */
static int addReplyCommandFlag(client *c, struct redisCommand *cmd, int f, char *reply) {
    if (cmd->flags & f) {
        addReplyStatus(c, reply);
        return 1;
    }
    return 0;
}

/* Output the representation of a Redis command. Used by the COMMAND command. */
static void addReplyCommand(client *c, struct redisCommand *cmd) {
    if (!cmd) {
        addReplyNull(c);
    } else {
        /* We are adding: command name, arg count, flags, first, last, offset, categories */
        addReplyArrayLen(c, 7);
        addReplyBulkCString(c, cmd->name);
        addReplyLongLong(c, cmd->arity);

        int flagcount = 0;
        void *flaglen = addReplyDeferredLen(c);
        flagcount += addReplyCommandFlag(c, cmd, CMD_WRITE, "write");
        flagcount += addReplyCommandFlag(c, cmd, CMD_READONLY, "readonly");
        flagcount += addReplyCommandFlag(c, cmd, CMD_DENYOOM, "denyoom");
        flagcount += addReplyCommandFlag(c, cmd, CMD_ADMIN, "admin");
        flagcount += addReplyCommandFlag(c, cmd, CMD_PUBSUB, "pubsub");
        flagcount += addReplyCommandFlag(c, cmd, CMD_NOSCRIPT, "noscript");
        flagcount += addReplyCommandFlag(c, cmd, CMD_RANDOM, "random");
        flagcount += addReplyCommandFlag(c, cmd, CMD_SORT_FOR_SCRIPT, "sort_for_script");
        flagcount += addReplyCommandFlag(c, cmd, CMD_LOADING, "loading");
        flagcount += addReplyCommandFlag(c, cmd, CMD_STALE, "stale");
        flagcount += addReplyCommandFlag(c, cmd, CMD_SKIP_MONITOR, "skip_monitor");
        flagcount += addReplyCommandFlag(c, cmd, CMD_SKIP_SLOWLOG, "skip_slowlog");
        flagcount += addReplyCommandFlag(c, cmd, CMD_ASKING, "asking");
        flagcount += addReplyCommandFlag(c, cmd, CMD_FAST, "fast");
        flagcount += addReplyCommandFlag(c, cmd, CMD_NO_AUTH, "no_auth");
        flagcount += addReplyCommandFlag(c, cmd, CMD_MAY_REPLICATE, "may_replicate");
        if (cmdHasMovableKeys(cmd)) {
            addReplyStatus(c, "movablekeys");
            flagcount += 1;
        }
        setDeferredSetLen(c, flaglen, flagcount);

        addReplyLongLong(c, cmd->firstkey);
        addReplyLongLong(c, cmd->lastkey);
        addReplyLongLong(c, cmd->keystep);

        addReplyCommandCategories(c, cmd);
    }
}

/* COMMAND <subcommand> <args> */
void commandCommand(client *c) {
    dictIterator *di;
    dictEntry *de;

    if (c->argc == 2 && !strcasecmp(c->argv[1]->ptr, "help")) {
        const char *help[] = {"(no subcommand)",
                              "    Return details about all Redis commands.",
                              "COUNT",
                              "    Return the total number of commands in this Redis server.",
                              "GETKEYS <full-command>",
                              "    Return the keys from a full Redis command.",
                              "INFO [<command-name> ...]",
                              "    Return details about multiple Redis commands.",
                              NULL};
        addReplyHelp(c, help);
    } else if (c->argc == 1) {
        addReplyArrayLen(c, dictSize(server.commands));
        di = dictGetIterator(server.commands);
        while ((de = dictNext(di)) != NULL) {
            addReplyCommand(c, dictGetVal(de));
        }
        dictReleaseIterator(di);
    } else if (!strcasecmp(c->argv[1]->ptr, "info")) {
        int i;
        addReplyArrayLen(c, c->argc - 2);
        for (i = 2; i < c->argc; i++) {
            addReplyCommand(c, dictFetchValue(server.commands, c->argv[i]->ptr));
        }
    } else if (!strcasecmp(c->argv[1]->ptr, "count") && c->argc == 2) {
        // 命令表命令的个数
        addReplyLongLong(c, dictSize(server.commands));
    } else if (!strcasecmp(c->argv[1]->ptr, "getkeys") && c->argc >= 3) {
        struct redisCommand *cmd = lookupCommand(c->argv[2]->ptr);
        getKeysResult result = GETKEYS_RESULT_INIT;
        int j;

        if (!cmd) {
            addReplyError(c, "Invalid command specified");
            return;
        } else if (cmd->getkeys_proc == NULL && cmd->firstkey == 0) {
            addReplyError(c, "The command has no key arguments");
            return;
        } else if ((cmd->arity > 0 && cmd->arity != c->argc - 2) || ((c->argc - 2) < -cmd->arity)) {
            addReplyError(c, "Invalid number of arguments specified for command");
            return;
        }

        if (!getKeysFromCommand(cmd, c->argv + 2, c->argc - 2, &result)) {
            addReplyError(c, "Invalid arguments specified for command");
        } else {
            addReplyArrayLen(c, result.numkeys);
            for (j = 0; j < result.numkeys; j++)
                addReplyBulk(c, c->argv[result.keys[j] + 2]);
        }
        getKeysFreeResult(&result);
    } else {
        addReplySubcommandSyntaxError(c);
    }
}

#define REDIS_VERSION_CONCATENATE(version) "redis version: " version

void versionCommand(client *c) {
    if (c->argc > 1) {
        addReplyErrorObject(c, shared.syntaxerr);
        return;
    }
    addReplyBulkCString(c, REDIS_VERSION_CONCATENATE(REDIS_VERSION));
}

// 处理 Redis 服务器中的 `MONITOR` 命令
void monitorCommand(client *c) {
    // 检查客户端是否设置了 `CLIENT_DENY_BLOCKING` 标志
    if (c->flags & CLIENT_DENY_BLOCKING) {
        /**
         * A client that has CLIENT_DENY_BLOCKING flag on
         * expects a reply per command and so can't execute MONITOR. */
        addReplyError(c, "MONITOR isn't allowed for DENY BLOCKING client");
        return;
    }

    /* ignore MONITOR if already slave or in monitor mode */
    // 检查客户端是否已经是从节点或已经处于监控模式
    if (c->flags & CLIENT_SLAVE)
        return;

    c->flags |= (CLIENT_SLAVE | CLIENT_MONITOR);
    // - 将客户端添加到 `server.monitors` 列表的尾部,表示该客户端现在处于监控模式。
    listAddNodeTail(server.monitors, c);
    addReply(c, shared.ok);
}



/* Redis 命令传播操作数组。 */
void redisOpArrayInit(redisOpArray *oa) {
    oa->ops = NULL;
    oa->numops = 0;
}

int redisOpArrayAppend(redisOpArray *oa, struct redisCommand *cmd, int dbid, robj **argv, int argc, int target) {
    redisOp *op;

    oa->ops = zrealloc(oa->ops, sizeof(redisOp) * (oa->numops + 1));
    op = oa->ops + oa->numops;
    op->cmd = cmd;
    op->dbid = dbid;
    op->argv = argv;
    op->argc = argc;
    op->target = target;
    oa->numops++;
    return oa->numops;
}

void redisOpArrayFree(redisOpArray *oa) {
    while (oa->numops) {
        int j;
        redisOp *op;

        oa->numops--;
        op = oa->ops + oa->numops;
        for (j = 0; j < op->argc; j++)
            decrRefCount(op->argv[j]);
        zfree(op->argv);
    }
    zfree(oa->ops);
}

