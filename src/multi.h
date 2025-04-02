#ifndef MULTI_H
#define MULTI_H

#include "server.h"

typedef struct client client;
typedef struct redisObject robj;
typedef struct redisDb redisDb;

/* Client MULTI/EXEC state */
// 客户端MULTI/EXEC状态
typedef struct multiCmd
{
    robj **argv;
    int argc;
    struct redisCommand *cmd;
} multiCmd;

// 定义了一个名为 multiState 的结构体，用于存储 Redis 中 MULTI 命令执行状态。
// 它包含命令数组、命令总数、命令标志位的累积、反向标志位累积、最小副本数以及同步复制超时时间。
typedef struct multiState
{
    multiCmd *commands; /* Array of MULTI commands */                  // 表示事务中积累的命令数组。
    int count; /* Total number of MULTI commands */                    // 表示事务中积累的命令总数。
    int cmd_flags;                                                     /* The accumulated command flags OR-ed together.
                                                                      So if at least a command has a given flag, it
                                                                      will be set in this field. */
                                                                       // 存储事务中所有命令的标志位（flags）的按位或（OR）结果。
    int cmd_inv_flags;                                                 /* Same as cmd_flags, OR-ing the ~flags. so that it
                                                                      is possible to know if all the commands have a
                                                                      certain flag. */
                                                                       // 存储的是标志位的按位取反（~flags）结果的累积。
    int minreplicas; /* MINREPLICAS for synchronous replication */     // 表示事务执行时需要的最小副本数
    time_t minreplicas_timeout; /* MINREPLICAS timeout as unixtime. */ // 表示等待 minreplicas 个副本确认的超时时间，以 Unix 时间戳（秒）表示。
} multiState;

/* MULTI/EXEC/WATCH... */
void unwatchAllKeys(client *c);
void initClientMultiState(client *c);
void freeClientMultiState(client *c);
void queueMultiCommand(client *c);
void touchWatchedKey(redisDb *db, robj *key);
void touchAllWatchedKeysInDb(redisDb *emptied, redisDb *replaced_with);
void discardTransaction(client *c);
void flagTransaction(client *c);
void execCommandAbort(client *c, sds error);
void execCommandPropagateMulti(int dbid);
void execCommandPropagateExec(int dbid);
void beforePropagateMultiOrExec(int multi);

/* ===================== WATCH (CAS alike for MULTI/EXEC) ===================
 *
 * The implementation uses a per-DB hash table mapping keys to list of clients
 * WATCHing those keys, so that given a key that is going to be modified
 * we can mark all the associated clients as dirty.
 *
 * Also every client contains a list of WATCHed keys so that's possible to
 * un-watch such keys when the client is freed or when UNWATCH is called. */

/* In the client->watched_keys list we need to use watchedKey structures
 * as in order to identify a key in Redis we need both the key name and the
 * DB */
// 用于表示 Redis 事务机制中被监视的键及其所属的数据库。
typedef struct watchedKey
{
    robj *key;   // 表示被监视的键。
    redisDb *db; // 表示被监视键所在的数据库。
} watchedKey;

#endif
