#ifndef SLOWLOG_H
#define SLOWLOG_H

#include "server.h"

#define SLOWLOG_ENTRY_MAX_ARGC 32    /* 每条慢查询日志最多记录32个查询参数 */
#define SLOWLOG_ENTRY_MAX_STRING 128 /* 每个查询参数最长只记录128个字符 */

/* This structure defines an entry inside the slow log list */
typedef struct slowlogEntry
{
    robj **argv;                                                        /* 参数信息 */
    int argc;                                                           /* 参数个数 */
    long long id; /* Unique entry identifier. */                        /* 日志ID */
    long long duration; /* Time spent by the query, in microseconds. */ /* 命令执行时间 */
    time_t time; /* Unix time at which the query was executed. */       /* 日志创建时间 */
    sds cname; /* Client name. */                                       /* 客户端名称*/
    sds peerid; /* Client network address. */                           /* 客户端地址*/
} slowlogEntry;

/* Exported API */
void slowlogInit(void);
void slowlogPushEntryIfNeeded(client *c, robj **argv, int argc, long long duration);

/* Exported commands */
void slowlogCommand(client *c);

#endif /* SLOWLOG_H */
