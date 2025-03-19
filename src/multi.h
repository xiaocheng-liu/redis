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
    multiCmd *commands;         /* Array of MULTI commands */
    int count;                  /* Total number of MULTI commands */
    int cmd_flags;              /* The accumulated command flags OR-ed together.
                               So if at least a command has a given flag, it
                               will be set in this field. */
    int cmd_inv_flags;          /* Same as cmd_flags, OR-ing the ~flags. so that it
                               is possible to know if all the commands have a
                               certain flag. */
    int minreplicas;            /* MINREPLICAS for synchronous replication */
    time_t minreplicas_timeout; /* MINREPLICAS timeout as unixtime. */
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

#endif
