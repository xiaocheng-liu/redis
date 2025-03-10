/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __REDIS_H
#define __REDIS_H

#include "fmacros.h"
#include "config.h"
#include "solarisfixes.h"
#include "rio.h"
#include "atomicvar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <syslog.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <lua.h>
#include <signal.h>

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

typedef long long mstime_t; /* millisecond time type. */ // 毫秒时间类型
typedef long long ustime_t; /* microsecond time type. */ // 微秒时间类型

#include "version.h" /* Version macro 版本宏*/

#include "ae.h"        /* Event driven programming library 事件驱动库*/
#include "sds.h"       /* Dynamic safe strings 动态安全字符串*/
#include "dict.h"      /* Hash tables 哈希表*/
#include "adlist.h"    /* Linked lists 链表 */
#include "zmalloc.h"   /* total memory usage aware version of malloc/free */ // 该头文件提供了内存分配函数的替代版本（如 malloc 和 free）
#include "anet.h"      /* Networking the easy way */ // 网络编程
#include "ziplist.h"   /* Compact list data structure 压缩列表数据结构*/
#include "intset.h"    /* Compact integer set structure 压缩整型结构*/
#include "quicklist.h" /* Lists are encoded as linked lists of N-elements flat arrays */ // 列表被编码为包含n个元素的平面数组的链表
#include "rax.h"       /* Radix tree */ // 基数树

#include "util.h"       /* Misc functions useful in many places 工具函数*/
#include "latency.h"    /* Latency monitor API */    // 延迟监视器API,通过包含此头文件，程序可以调用其中定义的函数和宏来监测系统的延迟情况。
#include "sparkline.h"  /* ASCII graphs API */ // ASCII 图表的 API。通过包含这个头文件，程序可以调用其中定义的函数来绘制简单的文本图表。
#include "connection.h" /* Connection abstraction */ // 连接抽象的接口或实现。通过包含此头文件，程序可以使用其中定义的与连接相关的功能和数据结构。

#define REDISMODULE_CORE 1 // 这个宏通常用于标识代码是否为核心模块的一部分，以便在编译时进行条件编译。
#include "redismodule.h" /* Redis modules API defines. */ // 模块开发接口。这为后续编写 Redis 模块提供了必要的函数和数据结构支持。

/* Following includes allow test functions to be called from Redis main() */
// 以下内容包括允许从 Redis main（） 调用测试函数
#include "zipmap.h"
#include "sha1.h"
#include "endianconv.h"
#include "crc64.h"
#include "evict.h"

/* Error codes */
// 错误代码
#define C_OK 0
#define C_ERR -1

/* Static server configuration */
// 静态服务器配置
// 这段代码定义了三个宏，用于配置系统的时间中断频率。CONFIG_DEFAULT_HZ 设置默认时间为每秒 10 次中断，CONFIG_MIN_HZ 和 CONFIG_MAX_HZ 分别设置最小和最大时间中断频率为每秒 1 次和 500 次。
#define CONFIG_DEFAULT_HZ 10 /* Time interrupt calls/sec. */
#define CONFIG_MIN_HZ 1
#define CONFIG_MAX_HZ 500
#define MAX_CLIENTS_PER_CLOCK_TICK 200 /* HZ is adapted based on that. */
#define CONFIG_MAX_LINE 1024
#define CRON_DBS_PER_CALL 16
#define NET_MAX_WRITES_PER_EVENT (1024 * 64)
#define PROTO_SHARED_SELECT_CMDS 10
#define OBJ_SHARED_INTEGERS 10000
#define OBJ_SHARED_BULKHDR_LEN 32
// 这段代码定义了一个宏 LOG_MAX_LEN，其值为1024，表示系统日志消息的最大默认长度。
#define LOG_MAX_LEN 1024 /* Default maximum length of syslog messages.*/
#define AOF_REWRITE_ITEMS_PER_CMD 64
#define AOF_READ_DIFF_INTERVAL_BYTES (1024 * 10)
#define CONFIG_AUTHPASS_MAX_LEN 512
#define CONFIG_RUN_ID_SIZE 40
#define RDB_EOF_MARK_SIZE 40
#define CONFIG_REPL_BACKLOG_MIN_SIZE (1024 * 16)                               /* 16k */
#define CONFIG_BGSAVE_RETRY_DELAY 5 /* Wait a few secs before trying again. */ // 请等待几秒钟，然后重试。
#define CONFIG_DEFAULT_PID_FILE "/var/run/redis.pid"                           // 默认的 PID 文件路径
#define CONFIG_DEFAULT_CLUSTER_CONFIG_FILE "nodes.conf"                        // 默认的集群配置文件路径
#define CONFIG_DEFAULT_UNIX_SOCKET_PERM 0                                      // 默认的 UNIX 域套接字权限
#define CONFIG_DEFAULT_LOGFILE ""                                              // 默认的日志文件路径
#define NET_IP_STR_LEN 46                                                      /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define NET_ADDR_STR_LEN (NET_IP_STR_LEN + 32)                                 /* Must be enough for ip:port */
#define CONFIG_BINDADDR_MAX 16
#define CONFIG_MIN_RESERVED_FDS 32
#define CONFIG_DEFAULT_PROC_TITLE_TEMPLATE "{title} {listen-addr} {server-mode}"

#define ACTIVE_EXPIRE_CYCLE_SLOW 0
#define ACTIVE_EXPIRE_CYCLE_FAST 1

/* Children process will exit with this status code to signal that the
 * process terminated without an error: this is useful in order to kill
 * a saving child (RDB or AOF one), without triggering in the parent the
 * write protection that is normally turned on on write errors.
 * Usually children that are terminated with SIGUSR1 will exit with this
 * special code. */
// 子进程将使用此状态代码退出，以指示进程已终止且没有错误：这对于杀死保存的子进程（RDB 或 AOF 一个）很有用，
// 而不会在父进程中触发通常在写入错误时打开的写保护。通常，以SIGUSR1终止的儿童将使用此特殊代码退出。
#define SERVER_CHILD_NOERROR_RETVAL 255

/* Instantaneous metrics tracking. */
// 即时指标跟踪。
#define STATS_METRIC_SAMPLES 16 /* Number of samples per metric. */ // 每个指标的样本数。
#define STATS_METRIC_COMMAND 0 /* Number of commands executed. */   // 执行的命令数。
#define STATS_METRIC_NET_INPUT 1 /* Bytes read to network. */       // 读取到网络的字节数
#define STATS_METRIC_NET_OUTPUT 2 /* Bytes written to network. */   // 写入网络的字节数。
#define STATS_METRIC_COUNT 3

/* Protocol and I/O related defines */
// 协议和 IO 相关定义
#define PROTO_MAX_QUERYBUF_LEN (1024 * 1024 * 1024) /* 1GB max query buffer. */ // 最大查询缓冲区为 1GB。
#define PROTO_IOBUF_LEN (1024 * 16) /* Generic I/O buffer size */               // 通用 IO 缓冲区大小
#define PROTO_REPLY_CHUNK_BYTES (16 * 1024) /* 16k output buffer */             // 16k 输出缓冲器
#define PROTO_INLINE_MAX_SIZE (1024 * 64) /* Max size of inline reads */        // 内联读取的最大大小
#define PROTO_MBULK_BIG_ARG (1024 * 32)
#define LONG_STR_SIZE 21 /* Bytes needed for long -> str + '\0' */ // long -> str + '\0' 所需的字节数
#define REDIS_AUTOSYNC_BYTES (1024 * 1024 * 32)                    /* fdatasync every 32MB */

#define LIMIT_PENDING_QUERYBUF (4 * 1024 * 1024) /* 4mb */

/* When configuring the server eventloop, we setup it so that the total number
 * of file descriptors we can handle are server.maxclients + RESERVED_FDS +
 * a few more to stay safe. Since RESERVED_FDS defaults to 32, we add 96
 * in order to make sure of not over provisioning more than 128 fds. */
// 在配置服务器 eventloop 时，我们对其进行设置，以便我们可以处理的文件描述符总数为
// server.maxclients + RESERVED_FDS + 更多以保持安全。由于 RESERVED_FDS 默认为 32，
// 因此我们添加了 96，以确保不会过度配置超过 128 个 fd。
#define CONFIG_FDSET_INCR (CONFIG_MIN_RESERVED_FDS + 96)

/* OOM Score Adjustment classes. */
// OOM 分数调整类。
#define CONFIG_OOM_MASTER 0
#define CONFIG_OOM_REPLICA 1
#define CONFIG_OOM_BGCHILD 2
#define CONFIG_OOM_COUNT 3

extern int configOOMScoreAdjValuesDefaults[CONFIG_OOM_COUNT];

/* Hash table parameters */
// 哈希表参数
#define HASHTABLE_MIN_FILL 10 /* Minimal hash table fill 10% */               // 最小哈希表填充 10%
#define HASHTABLE_MAX_LOAD_FACTOR 1.618 /* Maximum hash table load factor. */ // 最大哈希表负载因子。

/* Command flags. Please check the command table defined in the server.c file
 * for more information about the meaning of every flag. */
// 命令标志。请检查 server.c 文件中定义的命令表，了解有关每个标志含义的更多信息。
#define CMD_WRITE (1ULL << 0) /* "write" flag */                  // "写入"标志
#define CMD_READONLY (1ULL << 1) /* "read-only" flag */           // "只读"标志
#define CMD_DENYOOM (1ULL << 2) /* "use-memory" flag */           // "使用内存"标志
#define CMD_MODULE (1ULL << 3) /* Command exported by module. */  // 命令由模块导出。
#define CMD_ADMIN (1ULL << 4) /* "admin" flag */                  // "管理员"标志
#define CMD_PUBSUB (1ULL << 5) /* "pub-sub" flag */               // "发布-订阅"标志
#define CMD_NOSCRIPT (1ULL << 6) /* "no-script" flag */           // "无脚本"标志
#define CMD_RANDOM (1ULL << 7) /* "random" flag */                // "随机"标志
#define CMD_SORT_FOR_SCRIPT (1ULL << 8) /* "to-sort" flag */      // "待排序"标志
#define CMD_LOADING (1ULL << 9) /* "ok-loading" flag */           // "正常加载"标志
#define CMD_STALE (1ULL << 10) /* "ok-stale" flag */              // "确定修复"标志
#define CMD_SKIP_MONITOR (1ULL << 11) /* "no-monitor" flag */     // "无监视器"标志
#define CMD_SKIP_SLOWLOG (1ULL << 12) /* "no-slowlog" flag */     // "无慢日志"标志
#define CMD_ASKING (1ULL << 13) /* "cluster-asking" flag */       // "群集询问"标志
#define CMD_FAST (1ULL << 14) /* "fast" flag */                   // "快速"标志
#define CMD_NO_AUTH (1ULL << 15) /* "no-auth" flag */             // "无身份验证"标志
#define CMD_MAY_REPLICATE (1ULL << 16) /* "may-replicate" flag */ // "可以复制"标志

/* Command flags used by the module system. */
// 模块系统使用的命令标志。
#define CMD_MODULE_GETKEYS (1ULL << 17) /* Use the modules getkeys interface. */ // 使用模块获取密钥接口。
#define CMD_MODULE_NO_CLUSTER (1ULL << 18) /* Deny on Redis Cluster. */          // 在 Redis 集群上拒绝。

/* Command flags that describe ACLs categories. */
// 描述 ACL 类别的命令标志。
#define CMD_CATEGORY_KEYSPACE (1ULL << 19)
#define CMD_CATEGORY_READ (1ULL << 20)
#define CMD_CATEGORY_WRITE (1ULL << 21)
#define CMD_CATEGORY_SET (1ULL << 22)
#define CMD_CATEGORY_SORTEDSET (1ULL << 23)
#define CMD_CATEGORY_LIST (1ULL << 24)
#define CMD_CATEGORY_HASH (1ULL << 25)
#define CMD_CATEGORY_STRING (1ULL << 26)
#define CMD_CATEGORY_BITMAP (1ULL << 27)
#define CMD_CATEGORY_HYPERLOGLOG (1ULL << 28)
#define CMD_CATEGORY_GEO (1ULL << 29)
#define CMD_CATEGORY_STREAM (1ULL << 30)
#define CMD_CATEGORY_PUBSUB (1ULL << 31)
#define CMD_CATEGORY_ADMIN (1ULL << 32)
#define CMD_CATEGORY_FAST (1ULL << 33)
#define CMD_CATEGORY_SLOW (1ULL << 34)
#define CMD_CATEGORY_BLOCKING (1ULL << 35)
#define CMD_CATEGORY_DANGEROUS (1ULL << 36)
#define CMD_CATEGORY_CONNECTION (1ULL << 37)
#define CMD_CATEGORY_TRANSACTION (1ULL << 38)
#define CMD_CATEGORY_SCRIPTING (1ULL << 39)

/* AOF states */
// AOF状态
#define AOF_OFF 0 /* AOF is off */                                    // AOF已关闭
#define AOF_ON 1 /* AOF is on */                                      // AOF已开启
#define AOF_WAIT_REWRITE 2 /* AOF waits rewrite to start appending */ // AOF 等待重写开始追加

/* Client flags */
// 客户端标志
#define CLIENT_SLAVE (1 << 0) /* This client is a replica */                        // 此客户端是副本
#define CLIENT_MASTER (1 << 1) /* This client is a master */                        // 此客户端是主客户端
#define CLIENT_MONITOR (1 << 2) /* This client is a slave monitor, see MONITOR */   // 此客户端是从属监视器，请参阅监视器
#define CLIENT_MULTI (1 << 3) /* This client is in a MULTI context */               // 此客户端位于多上下文中
#define CLIENT_BLOCKED (1 << 4) /* The client is waiting in a blocking operation */ // 客户端正在阻止操作中等待
#define CLIENT_DIRTY_CAS (1 << 5) /* Watched keys modified. EXEC will fail. */      // 监视的键已修改。执行将失败。
#define CLIENT_CLOSE_AFTER_REPLY (1 << 6) /* Close after writing entire reply. */   // 写完整个回复后关闭。
#define CLIENT_UNBLOCKED (1 << 7)                                                   /* This client was unblocked and is stored in \
                                                                                      server.unblocked_clients */                 \
                                                                                    // 此客户端已解除阻止，存储在 server.unblocked_clients
#define CLIENT_LUA (1 << 8) /* This is a non connected client used by Lua */        // 这是 Lua 使用的未连接客户端
#define CLIENT_ASKING (1 << 9) /* Client issued the ASKING command */               // 客户端发出了 ASK 命令
#define CLIENT_CLOSE_ASAP (1 << 10) /* Close this client ASAP */                    // 尽快关闭此客户端
#define CLIENT_UNIX_SOCKET (1 << 11)                                                /* Client connected via Unix domain socket */
#define CLIENT_DIRTY_EXEC (1 << 12)                                                 /* EXEC will fail for errors while queueing */
#define CLIENT_MASTER_FORCE_REPLY (1 << 13)                                         /* Queue replies even if is master */
#define CLIENT_FORCE_AOF (1 << 14)                                                  /* Force AOF propagation of current cmd. */
#define CLIENT_FORCE_REPL (1 << 15)                                                 /* Force replication of current cmd. */
#define CLIENT_PRE_PSYNC (1 << 16)                                                  /* Instance don't understand PSYNC. */
#define CLIENT_READONLY (1 << 17)                                                   /* Cluster client is in read-only state. */
#define CLIENT_PUBSUB (1 << 18) /* Client is in Pub/Sub mode. */                    // 客户端是发布/订阅模式
#define CLIENT_PREVENT_AOF_PROP (1 << 19)                                           /* Don't propagate to AOF. */
#define CLIENT_PREVENT_REPL_PROP (1 << 20)                                          /* Don't propagate to slaves. */
#define CLIENT_PREVENT_PROP (CLIENT_PREVENT_AOF_PROP | CLIENT_PREVENT_REPL_PROP)
#define CLIENT_PENDING_WRITE (1 << 21)            /* Client has output to send but a write \
                                                     handler is yet not installed. */
#define CLIENT_REPLY_OFF (1 << 22)                /* Don't send replies to client. */
#define CLIENT_REPLY_SKIP_NEXT (1 << 23)          /* Set CLIENT_REPLY_SKIP for next cmd */
#define CLIENT_REPLY_SKIP (1 << 24)               /* Don't send just this reply. */
#define CLIENT_LUA_DEBUG (1 << 25)                /* Run EVAL in debug mode. */
#define CLIENT_LUA_DEBUG_SYNC (1 << 26)           /* EVAL debugging without fork() */
#define CLIENT_MODULE (1 << 27)                   /* Non connected client used by some module. */
#define CLIENT_PROTECTED (1 << 28)                /* Client should not be freed for now. */
#define CLIENT_PENDING_READ (1 << 29)             /* The client has pending reads and was put \
                                                     in the list of clients we can read       \
                                                     from. */
#define CLIENT_PENDING_COMMAND (1 << 30)          /* Indicates the client has a fully \
                                                   * parsed command ready for execution. */
#define CLIENT_TRACKING (1ULL << 31)              /* Client enabled keys tracking in order to \
                                                  perform client side caching. */
#define CLIENT_TRACKING_BROKEN_REDIR (1ULL << 32) /* Target client is invalid. */
#define CLIENT_TRACKING_BCAST (1ULL << 33)        /* Tracking in BCAST mode. */
#define CLIENT_TRACKING_OPTIN (1ULL << 34)        /* Tracking in opt-in mode. */
#define CLIENT_TRACKING_OPTOUT (1ULL << 35)       /* Tracking in opt-out mode. */
#define CLIENT_TRACKING_CACHING (1ULL << 36)      /* CACHING yes/no was given, \
                                                     depending on optin/optout mode. */
#define CLIENT_TRACKING_NOLOOP (1ULL << 37)       /* Don't send invalidation messages \
                                                     about writes performed by myself.*/
#define CLIENT_IN_TO_TABLE (1ULL << 38)           /* This client is in the timeout table. */
#define CLIENT_PROTOCOL_ERROR (1ULL << 39)        /* Protocol error chatting with it. */
#define CLIENT_CLOSE_AFTER_COMMAND (1ULL << 40)   /* Close after executing commands \
                                                   * and writing entire reply. */
#define CLIENT_DENY_BLOCKING (1ULL << 41)         /* Indicate that the client should not be blocked.  \
                                                     currently, turned on inside MULTI, Lua, RM_Call, \
                                                     and AOF client */
#define CLIENT_REPL_RDBONLY (1ULL << 42)          /* This client is a replica that only wants \
                                                     RDB without replication buffer. */

/* Client block type (btype field in client structure)
 * if CLIENT_BLOCKED flag is set. */
// 客户端块类型（客户端结构中的 btype 字段）CLIENT_BLOCKED如果设置了标志。
#define BLOCKED_NONE 0   /* Not blocked, no CLIENT_BLOCKED flag set. */
#define BLOCKED_LIST 1   /* BLPOP & co. */
#define BLOCKED_WAIT 2   /* WAIT for synchronous replication. */
#define BLOCKED_MODULE 3 /* Blocked by a loadable module. */
#define BLOCKED_STREAM 4 /* XREAD. */
#define BLOCKED_ZSET 5   /* BZPOP et al. */
#define BLOCKED_PAUSE 6  /* Blocked by CLIENT PAUSE */
#define BLOCKED_NUM 7    /* Number of blocked states. */

/* Client request types */
// 客户端请求类型
#define PROTO_REQ_INLINE 1    // 内联型
#define PROTO_REQ_MULTIBULK 2 // 协议型

/* Client classes for client limits, currently used only for
 * the max-client-output-buffer limit implementation. */
// 客户端限制的客户端类，当前仅用于最大客户端输出缓冲区限制实现。
#define CLIENT_TYPE_NORMAL 0     /* Normal req-reply clients + MONITORs */
#define CLIENT_TYPE_SLAVE 1      /* Slaves. */
#define CLIENT_TYPE_PUBSUB 2     /* Clients subscribed to PubSub channels. */
#define CLIENT_TYPE_MASTER 3     /* Master. */
#define CLIENT_TYPE_COUNT 4      /* Total number of client types. */
#define CLIENT_TYPE_OBUF_COUNT 3 /* Number of clients to expose to output \
                                    buffer configuration. Just the first  \
                                    three: normal, slave, pubsub. */

/* Slave replication state. Used in server.repl_state for slaves to remember
 * what to do next. */
// 从属复制状态。用于server.repl_state奴隶记住下一步该做什么。
typedef enum
{
    REPL_STATE_NONE = 0,
    /* No active replication */ // 未开启主从复制功能，当前服务器是普通的Redis实例；
    REPL_STATE_CONNECT,
    /* Must connect to master */ // 待发起Socket连接主服务器；
    REPL_STATE_CONNECTING,
    /* Connecting to master */ // Socket连接成功；
    /* --- Handshake states, must be ordered --- */
    // 握手状态，必须排序
    REPL_STATE_RECEIVE_PING_REPLY,
    /* Wait for PING reply */ // 等待PING的回复
    REPL_STATE_SEND_HANDSHAKE,
    /* Send handshake sequance to master */ // 将握手顺序发送到主服务器
    REPL_STATE_RECEIVE_AUTH_REPLY,
    /* Wait for AUTH reply */ // 等待身份验证回复
    REPL_STATE_RECEIVE_PORT_REPLY,
    /* Wait for REPLCONF reply */ // 等待回复
    REPL_STATE_RECEIVE_IP_REPLY,
    /* Wait for REPLCONF reply */ // 等待回复
    REPL_STATE_RECEIVE_CAPA_REPLY,
    /* Wait for REPLCONF reply */ // 等待回复
    REPL_STATE_SEND_PSYNC,
    /* Send PSYNC */ // 发送同步
    REPL_STATE_RECEIVE_PSYNC_REPLY,
    /* Wait for PSYNC reply */ // 等待 PSYNC 回复
    /* --- End of handshake states --- */
    REPL_STATE_TRANSFER,
    /* Receiving .rdb from master */ // 正在接收RDB文件；
    REPL_STATE_CONNECTED,
    /* Connected to master */ // RDB文件接收并载入完毕，主从复制连接建立成功，此时从服务器只需要等待接收主服务器同步数据即可。

} repl_state;

/* The state of an in progress coordinated failover */
// 正在进行的协调故障转移的状态
typedef enum
{
    NO_FAILOVER = 0,
    /* No failover in progress */ // 没有正在进行的故障转移
    FAILOVER_WAIT_FOR_SYNC,
    /* Waiting for target replica to catch up */ // 等待目标副本赶上
    FAILOVER_IN_PROGRESS                         /* Waiting for target replica to accept
                                                  * PSYNC FAILOVER request. */
                                                 // 等待目标副本接受同步故障转移请求。
} failover_state;

/* State of slaves from the POV of the master. Used in client->replstate.
 * In SEND_BULK and ONLINE state the slave receives new updates
 * in its output queue. In the WAIT_BGSAVE states instead the server is waiting
 * to start the next background saving in order to send updates to it. */
// 来自主节点 POV 的从属状态。用于client->replstate。在SEND_BULK和ONLINE状态下，从站在其输出队列中接收新的更新。
// 相反，在WAIT_BGSAVE状态下，服务器正在等待开始下一次后台保存，以便向其发送更新。
#define SLAVE_STATE_WAIT_BGSAVE_START 6 /* We need to produce a new RDB file. */ // 我们需要生成一个新的 RDB 文件。
#define SLAVE_STATE_WAIT_BGSAVE_END 7 /* Waiting RDB file creation to finish. */ // 等待 RDB 文件创建完成。
#define SLAVE_STATE_SEND_BULK 8 /* Sending RDB file to slave. */                 // 将 RDB 文件发送到从属设备。
#define SLAVE_STATE_ONLINE 9 /* RDB file transmitted, sending just updates. */   // RDB文件已传输，仅发送更新。

/* Slave capabilities. */
// 从属功能。
#define SLAVE_CAPA_NONE 0
#define SLAVE_CAPA_EOF (1 << 0) /* Can parse the RDB EOF streaming format. */ // 可以解析 RDB EOF 流格式。
#define SLAVE_CAPA_PSYNC2 (1 << 1) /* Supports PSYNC2 protocol. */            // 支持 PSYNC2 协议。

/* Synchronous read timeout - slave side */
// 同步读取超时 - 从端
#define CONFIG_REPL_SYNCIO_TIMEOUT 5

/* List related stuff */
// List相关
#define LIST_HEAD 0
#define LIST_TAIL 1
#define ZSET_MIN 0
#define ZSET_MAX 1

/* Sort operations */
// 排序操作
#define SORT_OP_GET 0

/* Log levels */
// 日志级别
#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3
#define LL_RAW (1 << 10) /* Modifier to log without timestamp */ // 不带时间戳的记录修饰符

/* Supervision options */
// 监督选项
#define SUPERVISED_NONE 0
#define SUPERVISED_AUTODETECT 1
#define SUPERVISED_SYSTEMD 2
#define SUPERVISED_UPSTART 3

/* Anti-warning macro... */
// 防预警宏
#define UNUSED(V) ((void)V)

#define ZSKIPLIST_MAXLEVEL 32 /* Should be enough for 2^64 elements */ // 足够存储 2^64 个元素了，因为按概率，每4个才会加一层
#define ZSKIPLIST_P 0.25 /* Skiplist P = 1/4 */                        // 跳表层数增加的概率 P = 1/4

/* Append only defines */
// 仅追加定义
#define AOF_FSYNC_NO 0
#define AOF_FSYNC_ALWAYS 1
#define AOF_FSYNC_EVERYSEC 2

/* Replication diskless load defines */
// 复制无盘负载定义
#define REPL_DISKLESS_LOAD_DISABLED 0
#define REPL_DISKLESS_LOAD_WHEN_DB_EMPTY 1
#define REPL_DISKLESS_LOAD_SWAPDB 2

/* TLS Client Authentication */
// TLS 客户端身份验证
#define TLS_CLIENT_AUTH_NO 0
#define TLS_CLIENT_AUTH_YES 1
#define TLS_CLIENT_AUTH_OPTIONAL 2

/* Sanitize dump payload */
// 清理转储有效负载
#define SANITIZE_DUMP_NO 0
#define SANITIZE_DUMP_YES 1
#define SANITIZE_DUMP_CLIENTS 2

/* Sets operations codes */
// 这段代码定义了三个宏，用于表示集合操作的类型：SET_OP_UNION 表示并集操作，SET_OP_DIFF 表示差集操作，SET_OP_INTER 表示交集操作。
#define SET_OP_UNION 0
#define SET_OP_DIFF 1
#define SET_OP_INTER 2

/* oom-score-adj defines */
// 这段代码定义了三个宏，用于表示不同的OOM（Out of Memory）分数调整方式：OOM_SCORE_ADJ_NO 表示不调整，OOM_SCORE_RELATIVE 表示相对调整，OOM_SCORE_ADJ_ABSOLUTE 表示绝对调整。
#define OOM_SCORE_ADJ_NO 0
#define OOM_SCORE_RELATIVE 1
#define OOM_SCORE_ADJ_ABSOLUTE 2

/* Redis maxmemory strategies. Instead of using just incremental number
 * for this defines, we use a set of flags so that testing for certain
 * properties common to multiple policies is faster. */
// Redis 最大内存策略。我们不是只使用增量数字来定义，而是使用一组标志，以便更快地测试多个策略共有的某些属性。
#define MAXMEMORY_FLAG_LRU (1 << 0)
#define MAXMEMORY_FLAG_LFU (1 << 1)
#define MAXMEMORY_FLAG_ALLKEYS (1 << 2)
#define MAXMEMORY_FLAG_NO_SHARED_INTEGERS \
    (MAXMEMORY_FLAG_LRU | MAXMEMORY_FLAG_LFU)

// redis的8中淘汰策略
#define MAXMEMORY_VOLATILE_LRU ((0 << 8) | MAXMEMORY_FLAG_LRU)                         // 仅在有过期时间的数据上执行LRU
#define MAXMEMORY_VOLATILE_LFU ((1 << 8) | MAXMEMORY_FLAG_LFU)                         // 仅在有过期时间的数据上执行LFU
#define MAXMEMORY_VOLATILE_TTL (2 << 8)                                                // 在有过期时间的数据上按TTL长度淘汰
#define MAXMEMORY_VOLATILE_RANDOM (3 << 8)                                             // 仅在有过期时间的数据上随机淘汰
#define MAXMEMORY_ALLKEYS_LRU ((4 << 8) | MAXMEMORY_FLAG_LRU | MAXMEMORY_FLAG_ALLKEYS) // 在全局数据上执行LRU
#define MAXMEMORY_ALLKEYS_LFU ((5 << 8) | MAXMEMORY_FLAG_LFU | MAXMEMORY_FLAG_ALLKEYS) // 在全局数据上执行LFU
#define MAXMEMORY_ALLKEYS_RANDOM ((6 << 8) | MAXMEMORY_FLAG_ALLKEYS)                   // 在全局数据上随机淘汰
#define MAXMEMORY_NO_EVICTION (7 << 8)                                                 // 不淘汰数，当内存空间满时插入数据会报错

/* Units */
// 这段代码定义了两个宏，用于表示时间单位。UNIT_SECONDS 表示秒，值为 0；UNIT_MILLISECONDS 表示毫秒，值为 1。
#define UNIT_SECONDS 0
#define UNIT_MILLISECONDS 1

/* SHUTDOWN flags */
// 关闭标志
#define SHUTDOWN_NOFLAGS 0 /* No flags. */              // 没有标志。
#define SHUTDOWN_SAVE 1                                 /* Force SAVE on SHUTDOWN even if no save \
                                                           points are configured. */              \
                                                        // 即使未配置保存点，也会在关机时强制保存。
#define SHUTDOWN_NOSAVE 2 /* Don't SAVE on SHUTDOWN. */ // 不要在关机时保存。

/* Command call flags, see call() function */
// 命令调用标志，请参阅 call（） 函数
#define CMD_CALL_NONE 0
#define CMD_CALL_SLOWLOG (1 << 0)
#define CMD_CALL_STATS (1 << 1)
#define CMD_CALL_PROPAGATE_AOF (1 << 2)
#define CMD_CALL_PROPAGATE_REPL (1 << 3)
#define CMD_CALL_PROPAGATE (CMD_CALL_PROPAGATE_AOF | CMD_CALL_PROPAGATE_REPL)
#define CMD_CALL_FULL (CMD_CALL_SLOWLOG | CMD_CALL_STATS | CMD_CALL_PROPAGATE)
#define CMD_CALL_NOWRAP (1 << 4) /* Don't wrap also propagate array into \
                                    MULTI/EXEC: the caller will handle it.  */

/* Command propagation flags, see propagate() function */
// 命令传播标志，请参阅 propagate（） 函数
#define PROPAGATE_NONE 0
#define PROPAGATE_AOF 1
#define PROPAGATE_REPL 2

/* Client pause types, larger types are more restrictive
 * pause types than smaller pause types. */
// 客户端暂停类型，较大的类型比较小的暂停类型更严格。
typedef enum
{
    CLIENT_PAUSE_OFF = 0,
    /* Pause no commands */ // 不暂停任何命令
    CLIENT_PAUSE_WRITE,
    /* Pause write commands */                // 暂停写入命令
    CLIENT_PAUSE_ALL /* Pause all commands */ // 暂停所有命令
} pause_type;

/* RDB active child save type. */
// RDB 活动存储类型
#define RDB_CHILD_TYPE_NONE 0
#define RDB_CHILD_TYPE_DISK 1 /* RDB is written to disk. */           // 保存到磁盘
#define RDB_CHILD_TYPE_SOCKET 2 /* RDB is written to slave socket. */ // RDB被写入从套接字

/* Keyspace changes notification classes. Every class is associated with a
 * character for configuration purposes. */
// 键空间更改通知类。出于配置目的，每个类都与一个字符相关联。
#define NOTIFY_KEYSPACE (1 << 0)                                                                                                                             /* K */
#define NOTIFY_KEYEVENT (1 << 1)                                                                                                                             /* E */
#define NOTIFY_GENERIC (1 << 2)                                                                                                                              /* g */
#define NOTIFY_STRING (1 << 3)                                                                                                                               /* $ */
#define NOTIFY_LIST (1 << 4)                                                                                                                                 /* l */
#define NOTIFY_SET (1 << 5)                                                                                                                                  /* s */
#define NOTIFY_HASH (1 << 6)                                                                                                                                 /* h */
#define NOTIFY_ZSET (1 << 7)                                                                                                                                 /* z */
#define NOTIFY_EXPIRED (1 << 8)                                                                                                                              /* x */
#define NOTIFY_EVICTED (1 << 9)                                                                                                                              /* e */
#define NOTIFY_STREAM (1 << 10)                                                                                                                              /* t */
#define NOTIFY_KEY_MISS (1 << 11)                                                                                                                            /* m (Note: This one is excluded from NOTIFY_ALL on purpose) */
#define NOTIFY_LOADED (1 << 12)                                                                                                                              /* module only key space notification, indicate a key loaded from rdb */
#define NOTIFY_ALL (NOTIFY_GENERIC | NOTIFY_STRING | NOTIFY_LIST | NOTIFY_SET | NOTIFY_HASH | NOTIFY_ZSET | NOTIFY_EXPIRED | NOTIFY_EVICTED | NOTIFY_STREAM) /* A flag */

/* Get the first bind addr or NULL */
// 获取第一个绑定地址或 NULL
#define NET_FIRST_BIND_ADDR (server.bindaddr_count ? server.bindaddr[0] : NULL)

/* Using the following macro you can run code inside serverCron() with the
 * specified period, specified in milliseconds.
 * The actual resolution depends on server.hz. */
// 使用以下宏，您可以在 serverCron（） 中以指定的时间段（以毫秒为单位）运行代码。实际分辨率取决于 server.hz。
#define run_with_period(_ms_) if ((_ms_ <= 1000 / server.hz) || !(server.cronloops % ((_ms_) / (1000 / server.hz))))

/* We can print the stacktrace, so our assert is defined this way: */
// 我们可以打印堆栈跟踪，因此我们的断言是这样定义的：
#define serverAssertWithInfo(_c, _o, _e) ((_e) ? (void)0 : (_serverAssertWithInfo(_c, _o, #_e, __FILE__, __LINE__), redis_unreachable()))
#define serverAssert(_e) ((_e) ? (void)0 : (_serverAssert(#_e, __FILE__, __LINE__), redis_unreachable()))
#define serverPanic(...) _serverPanic(__FILE__, __LINE__, __VA_ARGS__), redis_unreachable()

/*-----------------------------------------------------------------------------
 * Data types
 *----------------------------------------------------------------------------*/

/* A redis object, that is a type able to hold a string / list / set */
// 一个 redis 对象，即能够保存字符串列表集的类型

/* The actual Redis Object */
// type是表示当前robj里所存储的数据类型
// 对象类型 5种基础数据类型
#define OBJ_STRING 0 /* String object. */   // 字符串(string)
#define OBJ_LIST 1 /* List object. */       // 列表(list)
#define OBJ_SET 2 /* Set object. */         // 集合(set)
#define OBJ_ZSET 3 /* Sorted set object. */ // 有序集合(zset)
#define OBJ_HASH 4 /* Hash object. */       // 哈希表(hash)

/* The "module" object type is a special one that signals that the object
 * is one directly managed by a Redis module. In this case the value points
 * to a moduleValue struct, which contains the object value (which is only
 * handled by the module itself) and the RedisModuleType struct which lists
 * function pointers in order to serialize, deserialize, AOF-rewrite and
 * free the object.
 *
 * Inside the RDB file, module types are encoded as OBJ_MODULE followed
 * by a 64 bit module type ID, which has a 54 bits module-specific signature
 * in order to dispatch the loading to the right module, plus a 10 bits
 * encoding version. */
#define OBJ_MODULE 5 /* Module object. */ // 模块(module)
#define OBJ_STREAM 6 /* Stream object. */ // 流(stream)

/* Extract encver / signature from a module type ID. */
#define REDISMODULE_TYPE_ENCVER_BITS 10
#define REDISMODULE_TYPE_ENCVER_MASK ((1 << REDISMODULE_TYPE_ENCVER_BITS) - 1)
#define REDISMODULE_TYPE_ENCVER(id) (id & REDISMODULE_TYPE_ENCVER_MASK)
#define REDISMODULE_TYPE_SIGN(id) ((id & ~((uint64_t)REDISMODULE_TYPE_ENCVER_MASK)) >> REDISMODULE_TYPE_ENCVER_BITS)

/* Bit flags for moduleTypeAuxSaveFunc */
// #define REDISMODULE_AUX_BEFORE_RDB (1 << 0)
// #define REDISMODULE_AUX_AFTER_RDB (1 << 1)

struct RedisModule;
struct RedisModuleIO;
struct RedisModuleDigest;
struct RedisModuleCtx;
struct redisObject;
struct RedisModuleDefragCtx;

/* Each module type implementation should export a set of methods in order
 * to serialize and deserialize the value in the RDB file, rewrite the AOF
 * log, create the digest for "DEBUG DIGEST", and free the value when a key
 * is deleted. */
// 每个模块类型实现都应导出一组方法，以便序列化和反序列化 RDB 文件中的值，重写 AOF 日志，为"调试摘要"创建摘要，并在删除键时释放值。
typedef void *(*moduleTypeLoadFunc)(struct RedisModuleIO *io, int encver);
typedef void (*moduleTypeSaveFunc)(struct RedisModuleIO *io, void *value);
typedef int (*moduleTypeAuxLoadFunc)(struct RedisModuleIO *rdb, int encver, int when);
typedef void (*moduleTypeAuxSaveFunc)(struct RedisModuleIO *rdb, int when);
typedef void (*moduleTypeRewriteFunc)(struct RedisModuleIO *io, struct redisObject *key, void *value);
typedef void (*moduleTypeDigestFunc)(struct RedisModuleDigest *digest, void *value);
typedef size_t (*moduleTypeMemUsageFunc)(const void *value);
typedef void (*moduleTypeFreeFunc)(void *value);
typedef size_t (*moduleTypeFreeEffortFunc)(struct redisObject *key, const void *value);
typedef void (*moduleTypeUnlinkFunc)(struct redisObject *key, void *value);
typedef void *(*moduleTypeCopyFunc)(struct redisObject *fromkey, struct redisObject *tokey, const void *value);
typedef int (*moduleTypeDefragFunc)(struct RedisModuleDefragCtx *ctx, struct redisObject *key, void **value);

/* This callback type is called by moduleNotifyUserChanged() every time
 * a user authenticated via the module API is associated with a different
 * user or gets disconnected. This needs to be exposed since you can't cast
 * a function pointer to (void *). */
// 每次通过模块 API 进行身份验证的用户与其他用户关联或断开连接时，模块通知用户更改 （） 都会调用此回调类型。
// 这需要公开，因为您无法将函数指针转换为 （void ）。
typedef void (*RedisModuleUserChangedFunc)(uint64_t client_id, void *privdata);

/* The module type, which is referenced in each value of a given type, defines
 * the methods and links to the module exporting the type. */
// 在给定类型的每个值中引用的模块类型定义方法和指向导出类型的模块的链接。
typedef struct RedisModuleType
{
    uint64_t id; /* Higher 54 bits of type ID + 10 lower bits of encoding ver. */
    struct RedisModule *module;
    moduleTypeLoadFunc rdb_load;
    moduleTypeSaveFunc rdb_save;
    moduleTypeRewriteFunc aof_rewrite;
    moduleTypeMemUsageFunc mem_usage;
    moduleTypeDigestFunc digest;
    moduleTypeFreeFunc free;
    moduleTypeFreeEffortFunc free_effort;
    moduleTypeUnlinkFunc unlink;
    moduleTypeCopyFunc copy;
    moduleTypeDefragFunc defrag;
    moduleTypeAuxLoadFunc aux_load;
    moduleTypeAuxSaveFunc aux_save;
    int aux_save_triggers;
    char name[10]; /* 9 bytes name + null term. Charset: A-Z a-z 0-9 _- */
} moduleType;

/* In Redis objects 'robj' structures of type OBJ_MODULE, the value pointer
 * is set to the following structure, referencing the moduleType structure
 * in order to work with the value, and at the same time providing a raw
 * pointer to the value, as created by the module commands operating with
 * the module type.
 *
 * So for example in order to free such a value, it is possible to use
 * the following code:
 *
 *  if (robj->type == OBJ_MODULE) {
 *      moduleValue *mt = robj->ptr;
 *      mt->type->free(mt->value);
 *      zfree(mt); // We need to release this in-the-middle struct as well.
 *  }
 */
typedef struct moduleValue
{
    moduleType *type;
    void *value;
} moduleValue;

/* This is a wrapper for the 'rio' streams used inside rdb.c in Redis, so that
 * the user does not have to take the total count of the written bytes nor
 * to care about error conditions. */
// 这是 Redis 中 rdb.c 内部使用的"rio"流的包装器，因此用户不必获取写入字节的总数，也不必关心错误条件。
typedef struct RedisModuleIO
{
    size_t bytes;               /* Bytes read / written so far. */
    rio *rio;                   /* Rio stream. */
    moduleType *type;           /* Module type doing the operation. */
    int error;                  /* True if error condition happened. */
    int ver;                    /* Module serialization version: 1 (old),
                                 * 2 (current version with opcodes annotation). */
    struct RedisModuleCtx *ctx; /* Optional context, see RM_GetContextFromIO()*/
    struct redisObject *key;    /* Optional name of key processed */
} RedisModuleIO;

/* Macro to initialize an IO context. Note that the 'ver' field is populated
 * inside rdb.c according to the version of the value to load. */
// 用于初始化 IO 上下文的宏。请注意，"ver"字段根据要加载的值的版本填充在 rdb.c 中。
#define moduleInitIOContext(iovar, mtype, rioptr, keyptr) \
    do                                                    \
    {                                                     \
        iovar.rio = rioptr;                               \
        iovar.type = mtype;                               \
        iovar.bytes = 0;                                  \
        iovar.error = 0;                                  \
        iovar.ver = 0;                                    \
        iovar.key = keyptr;                               \
        iovar.ctx = NULL;                                 \
    } while (0)

/* This is a structure used to export DEBUG DIGEST capabilities to Redis
 * modules. We want to capture both the ordered and unordered elements of
 * a data structure, so that a digest can be created in a way that correctly
 * reflects the values. See the DEBUG DIGEST command implementation for more
 * background. */
// 这是用于将调试摘要功能导出到 Redis 模块的结构。我们希望捕获数据结构的有序和无序元素，以便能够以正确反映值的方式创建摘要。
// 有关更多背景信息，请参阅调试摘要命令实现。
typedef struct RedisModuleDigest
{
    unsigned char o[20]; /* Ordered elements. */
    unsigned char x[20]; /* Xored elements. */
} RedisModuleDigest;

/* Just start with a digest composed of all zero bytes. */
// 只需从由所有零字节组成的摘要开始。
#define moduleInitDigestContext(mdvar)       \
    do                                       \
    {                                        \
        memset(mdvar.o, 0, sizeof(mdvar.o)); \
        memset(mdvar.x, 0, sizeof(mdvar.x)); \
    } while (0)

/* Objects encoding. Some kind of objects like Strings and Hashes can be
 * internally represented in multiple ways. The 'encoding' field of the object
 * is set to one of this fields for this object. */
// 对象编码。某些类型的对象（如字符串和哈希）可以在内部以多种方式表示。对象的"编码"字段设置为此对象的此字段之一。
/*
 * 编码方式，如果说每个类型只有一种方式，那么其实type和encoding两个字段只需要保留一个即可，
 * 但redis为了在各种情况下尽可能节约内存，对每种类型的数据在不同情况下有不同的编码格式，
 * 所以这里需要用额外的字段标识出来。
 */
// 对象编码  对象编码(数据结构类型)。某些类型的对象（如字符串和哈希）可以通过多种方式在内部表示。ENCODING表明表示方式。
#define OBJ_ENCODING_RAW 0 /* Raw representation */                        // 最原始的标识方式，只有string才会用到
#define OBJ_ENCODING_INT 1 /* Encoded as integer */                        // 整数
#define OBJ_ENCODING_HT 2 /* Encoded as hash table */                      // 哈希表
#define OBJ_ENCODING_ZIPMAP 3 /* Encoded as zipmap */                      // ZIPMAP
#define OBJ_ENCODING_LINKEDLIST 4 /* No longer used: old list encoding. */ // LINKEDLIST
#define OBJ_ENCODING_ZIPLIST 5 /* Encoded as ziplist */                    // ziplist
#define OBJ_ENCODING_INTSET 6 /* Encoded as intset */                      // intset
#define OBJ_ENCODING_SKIPLIST 7 /* Encoded as skiplist */                  // skiplist跳表
#define OBJ_ENCODING_EMBSTR 8 /* Embedded sds string encoding */           // 嵌入式的sds
#define OBJ_ENCODING_QUICKLIST 9 /* Encoded as linked list of ziplists */  // 快表 quicklist
#define OBJ_ENCODING_STREAM 10 /* Encoded as a radix tree of listpacks */  // 流 stream

#define LRU_BITS 24
// LRU时钟的最大值
#define LRU_CLOCK_MAX ((1 << LRU_BITS) - 1) /* Max value of obj->lru */
// 以毫秒为单位的LRU时钟精度
#define LRU_CLOCK_RESOLUTION 1000 /* LRU clock resolution in ms */

// 全局对象的引用计数，值为 INT_MAX，意味着该对象永远不会被销毁
#define OBJ_SHARED_REFCOUNT INT_MAX       /* Global object never destroyed. */
// OBJ_STATIC_REFCOUNT 表示栈上分配的对象的引用计数，值为 INT_MAX - 1
#define OBJ_STATIC_REFCOUNT (INT_MAX - 1) /* Object allocated in the stack. */
// OBJ_FIRST_SPECIAL_REFCOUNT 是 OBJ_STATIC_REFCOUNT 的别名。
#define OBJ_FIRST_SPECIAL_REFCOUNT OBJ_STATIC_REFCOUNT

typedef struct redisObject
{
    unsigned type : 4;       // 数据类型  string  list  set  sortset  hash
    unsigned encoding : 4;   // 这个属性指明了对象底层的存储结构，比如 ZSet 类型对象可能的存储结构有 ZIPLIST 和 SKIPLIST
    unsigned lru : LRU_BITS; /* LRU time (relative to global lru_clock) or
                              * LFU data (least significant 8 bits frequency
                              * and most significant 16 bits access time).
                              * redis用24个位来保存LRU和LFU的信息，当使用LRU时保存上次
                              * 读写的时间戳(秒),使用LFU时保存上次时间戳(16位 min级) 保存近似统计数8位 */
                             // LRU_BITS = 24，共24位，高16位存储一个分钟数级别的时间戳，低8位存储访问计数(lfu : 最近访问次数)
                             // lru 记录的是对象最后一次被命令程序访问的时间
    int refcount;            // 引用计数
                             // refcount 记录的是该对象被引用的次数，类型为整型。refcount 的作用，主要在于对象的引用计数和内存回收。
                             // 当refcount减少到0时，就可以释放robj
    void *ptr;               // 指针指向具体存储的值，类型用type区分
                             // ptr 指针指向具体的数据，比如:set hello world，ptr 指向包含字符串 world 的 SDS。
} robj;

/* The a string name for an object's type as listed above
 * Native types are checked against the OBJ_STRING, OBJ_LIST, OBJ_* defines,
 * and Module types have their registered name returned. */
// 该函数 getObjectTypeName 根据传入的对象指针 robj * 返回对象的类型名称。
// 对于原生类型，它会检查对象是否属于预定义的类型（如字符串、列表等），并返回相应的名称；
// 对于模块类型，则返回其注册的名称。
char *getObjectTypeName(robj *);

/* Macro used to initialize a Redis object allocated on the stack.
 * Note that this macro is taken near the structure definition to make sure
 * we'll update it when the structure is changed, to avoid bugs like
 * bug #85 introduced exactly in this way. */
// 用于初始化堆栈上分配的 Redis 对象的宏。请注意，此宏位于结构定义附近，以确保我们在结构更改时对其进行更新，以避免以这种方式引入的 bug 85 等错误。
// 设置对象的引用计数、类型、编码方式和指针。
#define initStaticStringObject(_var, _ptr)   \
    do                                       \
    {                                        \
        _var.refcount = OBJ_STATIC_REFCOUNT; \
        _var.type = OBJ_STRING;              \
        _var.encoding = OBJ_ENCODING_RAW;    \
        _var.ptr = _ptr;                     \
    } while (0)

// struct evictionPoolEntry; /* Defined in evict.c */ // 在 evict.c 中定义

/* This structure is used in order to represent the output buffer of a client,
 * which is actually a linked list of blocks like that, that is: client->reply. */
// 该结构用于表示客户端的输出缓冲器，这实际上是一个类似的块链接列表，即：client->reply。
typedef struct clientReplyBlock
{
    size_t size, used;
    char buf[];
} clientReplyBlock;

/* Redis database representation. There are multiple databases identified
 * by integers from 0 (the default database) up to the max configured
 * database. The database number is the 'id' field in the structure. */
// Redis数据库表示。已识别多个数据库从0（默认数据库）到配置的最大值的整数数据库。数据库编号是结构中的"id"字段。
typedef struct redisDb
{
    dict *dict;                                                                             /* The keyspace for this DB                                           // 保存着数据库中的所有键值对数据, 这个属性也被称为键空间（key space）*/
    dict *expires; /* Timeout of keys with a timeout set */                                 // 保存key对应的过期时间
    dict *blocking_keys; /* Keys with clients waiting for data (BLPOP)*/                    // key对应的等待数据的client列表 (BLPOP)
    dict *ready_keys; /* Blocked keys that received a PUSH */                               // 收到推送的被阻止密钥
    dict *watched_keys; /* WATCHED keys for MULTI/EXEC */                                   // CAS 存储监听key的clients
    int id; /* Database ID */                                                               // 保存着数据库以整数表示的号码
    long long avg_ttl; /* Average TTL, just for stats */                                    // 存储的数据库对象的平均ttl(time to live)，用于统计
    unsigned long expires_cursor; /* Cursor of the active expire cycle. */                  // 过期删除过程中的下标
    list *defrag_later; /* List of key names to attempt to defrag one by one, gradually. */ // 要尝试逐个碎片整理的键名称列表，逐渐。
} redisDb;

/* Declare database backup that include redis main DBs and slots to keys map.
 * Definition is in db.c. We can't define it here since we define CLUSTER_SLOTS
 * in cluster.h. */
// 声明数据库备份，包括redis主DB和槽到键映射。定义在db.c中。
// 我们无法在此处定义它，因为我们定义了CLUSTER_SLOTS在cluster.h中。
typedef struct dbBackup dbBackup;

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

/* This structure holds the blocking operation state for a client.
 * The fields used depend on client->btype. */
// 此结构保存客户端的阻塞操作状态。 使用的字段取决于 client->btype
typedef struct blockingState
{
    /* Generic fields. */
    mstime_t timeout; /* Blocking operation timeout. If UNIX current time
                       * is > timeout then the operation timed out. */
                      // 阻塞超过时间
    /* BLOCKED_LIST, BLOCKED_ZSET and BLOCKED_STREAM */
    dict *keys;   /* The keys we are waiting to terminate a blocking
                   * operation such as BLPOP or XREAD. Or NULL. */
                  // 造成阻塞的键字典
    robj *target; /* The key that should receive the element,
                   * for BLMOVE. */
                  // 存储解除阻塞的键，用于保存PUSH入元素的键，也就是dstkey
    struct listPos
    {
        int wherefrom; /* Where to pop from */ // 从哪里弹出
        int whereto; /* Where to push to */    // 推送到哪里
    } listpos;                                 /* The positions in the src/dst lists
                                                * where we want to pop/push an element
                                                * for BLPOP, BRPOP and BLMOVE. */

    /* BLOCK_STREAM */
    size_t xread_count;   /* XREAD COUNT option. */
    robj *xread_group;    /* XREADGROUP group name. */
    robj *xread_consumer; /* XREADGROUP consumer name. */
    mstime_t xread_retry_time, xread_retry_ttl;
    int xread_group_noack;

    /* BLOCKED_WAIT */
    int numreplicas;      /* Number of replicas we are waiting for ACK. */
    long long reploffset; /* Replication offset to reach. */

    /* BLOCKED_MODULE */
    void *module_blocked_handle; /* RedisModuleBlockedClient structure.
                                    which is opaque for the Redis core, only
                                    handled in module.c. */
} blockingState;

/* The following structure represents a node in the server.ready_keys list,
 * where we accumulate all the keys that had clients blocked with a blocking
 * operation such as B[LR]POP, but received new data in the context of the
 * last executed command.
 *
 * After the execution of every command or script, we run this list to check
 * if as a result we should serve data to clients blocked, unblocking them.
 * Note that server.ready_keys will not have duplicates as there dictionary
 * also called ready_keys in every structure representing a Redis database,
 * where we make sure to remember if a given key was already added in the
 * server.ready_keys list. */
// 定义了一个名为 readyList 的结构体，包含两个成员：redisDb *db 和 robj *key。该结构体用于存储与 Redis 数据库和键对象相关的引用。
typedef struct readyList
{
    redisDb *db;
    robj *key;
} readyList;

/* This structure represents a Redis user. This is useful for ACLs, the
 * user is associated to the connection after the connection is authenticated.
 * If there is no associated user, the connection uses the default user. */
#define USER_COMMAND_BITS_COUNT 1024             /* The total number of command bits      \
                                                    in the user structure. The last valid \
                                                    command ID we can set in the user     \
                                                    is USER_COMMAND_BITS_COUNT-1. */
#define USER_FLAG_ENABLED (1 << 0)               /* The user is active. */
#define USER_FLAG_DISABLED (1 << 1)              /* The user is disabled. */
#define USER_FLAG_ALLKEYS (1 << 2)               /* The user can mention any key. */
#define USER_FLAG_ALLCOMMANDS (1 << 3)           /* The user can run all commands. */
#define USER_FLAG_NOPASS (1 << 4)                /* The user requires no password, any   \
                                                    provided password will work. For the \
                                                    default user, this also means that   \
                                                    no AUTH is needed, and every         \
                                                    connection is immediately            \
                                                    authenticated. */
#define USER_FLAG_ALLCHANNELS (1 << 5)           /* The user can mention any Pub/Sub \
                                                    channel. */
#define USER_FLAG_SANITIZE_PAYLOAD (1 << 6)      /* The user require a deep RESTORE \
                                                  * payload sanitization. */
#define USER_FLAG_SANITIZE_PAYLOAD_SKIP (1 << 7) /* The user should skip the     \
                                                  * deep sanitization of RESTORE \
                                                  * payload. */

// 用于存储 Redis 用户的权限和相关信息。
typedef struct
{
    sds name; /* The username as an SDS string. */ // 作为 SDS 字符串的用户名。
    uint64_t flags; /* See USER_FLAG_* */          // 请参阅USER_FLAG_* 权限标志，如是否可以访问所有键或频道。

    /* The bit in allowed_commands is set if this user has the right to
     * execute this command. In commands having subcommands, if this bit is
     * set, then all the subcommands are also available.
     *
     * If the bit for a given command is NOT set and the command has
     * subcommands, Redis will also check allowed_subcommands in order to
     * understand if the command can be executed. */
    // 允许执行的命令列表。
    uint64_t allowed_commands[USER_COMMAND_BITS_COUNT / 64];

    /* This array points, for each command ID (corresponding to the command
     * bit set in allowed_commands), to an array of SDS strings, terminated by
     * a NULL pointer, with all the sub commands that can be executed for
     * this command. When no subcommands matching is used, the field is just
     * set to NULL to avoid allocating USER_COMMAND_BITS_COUNT pointers. */
    // 允许执行的子命令列表。
    sds **allowed_subcommands;
    // 密码列表
    list *passwords; /* A list of SDS valid passwords for this user. */
    // 允许的键模式列表
    list *patterns;  /* A list of allowed key patterns. If this field is NULL
                        the user cannot mention any key in a command, unless
                        the flag ALLKEYS is set in the user. */
    // 允许的 Pub/Sub 频道模式列表。
    list *channels;  /* A list of allowed Pub/Sub channel patterns. If this
                        field is NULL the user cannot mention any channel in a
                        `PUBLISH` or [P][UNSUSBSCRIBE] command, unless the flag
                        ALLCHANNELS is set in the user. */
} user;

/* With multiplexing we need to take per-client state.
 * Clients are taken in a linked list. */
// 使用多路复用，我们需要采用每个客户端的状态。客户端在链表中获取。

#define CLIENT_ID_AOF (UINT64_MAX) /* Reserved ID for the AOF client. If you   \
                                      need more reserved IDs use UINT64_MAX-1, \
                                      -2, ... and so forth. */

typedef struct client
{
    uint64_t id; /* Client incremental unique ID. */                          // 客户端唯一ID
    connection *conn;                                                         // 连接
    int resp; /* RESP protocol version. Can be 2 or 3. */                     // RESP协议版本
    redisDb *db; /* Pointer to currently SELECTed DB. */                      // 当前选择的DB
    robj *name; /* As set by CLIENT SETNAME. */                               // 客户端名称，可以使用命令CLIENT SETNAME设置。
    sds querybuf; /* Buffer we use to accumulate client queries. */           // 输入缓冲区，recv函数接收的客户端命令请求会暂时缓存在此缓冲区。
    size_t qb_pos; /* The position we have read in querybuf. */               // 记录从querybuf去读的位置
    sds pending_querybuf;                                                     /* If this client is flagged as master, this buffer
                                                                                  represents the yet not applied portion of the
                                                                                  replication stream that we are receiving from
                                                                                  the master. */
                                                                              // 如果客户机被标记为主机，那么这个缓存代表从主机复制过来尚未实施的部分流数据
    size_t querybuf_peak; /* Recent (100ms or more) peak of querybuf size. */ // 最近查询缓存大小的峰值(100毫秒或更多)
    int argc; /* Num of arguments of current command. */                      // 当前命令参数的个数
    robj **argv; /* Arguments of current command. */                          // 当前命令的参数
    int original_argc;                                                        /* Num of arguments of original command if arguments were rewritten. */
    robj **original_argv;                                                     /* Arguments of original command if arguments were rewritten. */
    size_t argv_len_sum;                                                      /* Sum of lengths of objects in argv list. */
    struct redisCommand *cmd, *lastcmd;                                       /* Last command executed. */
                                                                              // cmd: 待执行的客户端命令；解析命令请求后，会根据命令名称查找该命令对应的命令对象，存储在客户端cmd字段，
                                                                              // 可以看到其类型为struct redisCommand。

    // 义了一个指向用户结构体的指针 user，用于表示与当前连接关联的用户。如果 user 指针为 NULL，则表示该连接具有管理员权限，可以执行任何操作
    user *user;                                                                   /* User associated with this connection. If the
                                                                                      user is set to NULL the connection can do
                                                                                      anything (admin). */
                                                                                  // 连接关联的用户，如果用户被设置为空，那么连接可以干任何时期(因为是管理员)
    int reqtype; /* Request protocol type: PROTO_REQ_* */                         // 请求协议类型
    int multibulklen; /* Number of multi bulk arguments left to read. */          // 剩余要读取的多批量参数数
    long bulklen; /* Length of bulk argument in multi bulk request. */            // 多批量请求中批量参数的长度
    list *reply; /* List of reply objects to send to the client. */               // 要发送到客户端的答复对象列表
    unsigned long long reply_bytes; /* Tot bytes of objects in reply list. */     // 表示输出链表中所有节点的存储空间总和；
    size_t sentlen;                                                               /* Amount of bytes already sent in the current              //表示已返回给客户端的字节数；
                                                                                      buffer or object being sent. */
                                                                                  // 当前缓冲区已经发出的或者正在发送对象的字节大小
    time_t ctime; /* Client creation time. */                                     // 客户端创建时间
    long duration;                                                                /* Current command duration. Used for measuring latency of blocking/non-blocking cmds */
    time_t lastinteraction; /* Time of the last interaction, used for timeout */  // 客户端上次与服务器交互的时间，以此实现客户端的超时处理。
    time_t obuf_soft_limit_reached_time;                                          // 输出缓存软性限制大小到达时间
    uint64_t flags; /* Client flags: CLIENT_* macros. */                          // 客户端标志 CLIENT_*  宏
    int authenticated; /* Needed when the default user requires auth. */          // 当默认用户需要认证时就需要
    int replstate; /* Replication state if this is a slave. */                    // 复制状态,如果这是一个从机
    int repl_put_online_on_ack; /* Install slave write handler on first ACK. */   // 在第一个确认之后 安装从机写入句柄
    int repldbfd; /* Replication DB file descriptor. */                           // 复制数据库文件描述符
    off_t repldboff; /* Replication DB file offset. */                            // 复制数据库文件偏移量
    off_t repldbsize; /* Replication DB file size. */                             // 复制数据库文件大小
    sds replpreamble; /* Replication DB preamble. */                              // 制数据库前奏（标识）
    long long read_reploff; /* Read replication offset if this is a master. */    // 如果这是主机，则读取复制偏移量。
    long long reploff; /* Applied replication offset if this is a master. */      // 如果这是主机，则应用复制偏移量
    long long repl_ack_off; /* Replication ack offset, if this is a slave. */     // 复制确认偏移量，如果这是从机。
    long long repl_ack_time; /* Replication ack time, if this is a slave. */      // 复制确认时间，如果这是从机。
    long long psync_initial_offset;                                               /* FULLRESYNC reply offset other slaves
                                                                           copying this slave output buffer
                                                                           should use. */
    char replid[CONFIG_RUN_ID_SIZE + 1]; /* Master replication ID (if master). */ // 主机复制ID（如果是主机） #define CONFIG_RUN_ID_SIZE 40
    int slave_listening_port; /* As configured with: REPLCONF listening-port */   // 配置为：SLAVECONF 侦听端口
    char slave_ip[NET_IP_STR_LEN]; /* Optionally given by REPLCONF ip-address */  // 由REPLCONF ip地址给出的选项
    int slave_capa; /* Slave capabilities: SLAVE_CAPA_* bitwise OR. */            // 从机功能：从机按位或
    multiState mstate; /* MULTI/EXEC state */                                     // 事务状态
    int btype; /* Type of blocking op if CLIENT_BLOCKED. */                       // 客户端阻塞类型
    blockingState bpop; /* blocking state */                                      // 阻塞状态
    long long woff; /* Last write global replication offset. */                   // 最近一次全局复制的偏移量
    list *watched_keys; /* Keys WATCHED for MULTI/EXEC CAS */                     // 通过事务总线监视的键
    dict *pubsub_channels; /* channels a client is interested in (SUBSCRIBE) */   // 客户感兴趣的频道（订阅）
    list *pubsub_patterns; /* patterns a client is interested in (SUBSCRIBE) */   // 客户感兴趣的模式（订阅）
    sds peerid; /* Cached peer ID. */                                             // 缓存的对方ID
    sds sockname; /* Cached connection target address. */                         // 缓存的连接目标地址。
    listNode *client_list_node; /* list node in client list */                    // 客户端列表的节点
    listNode *paused_list_node;                                                   /* list node within the pause list */
    RedisModuleUserChangedFunc auth_callback;                                     /* Module callback to execute
                                                                                   * when the authenticated user
                                                                                   * changes. */
                                                                                  // 当认证用户改变时，需要回调执行的模块
    void *auth_callback_privdata;                                                 /* Private data that is passed when the auth
                                                                                   * changed callback is executed. Opaque for
                                                                                   * Redis Core. */
                                                                                  // 执行认证改变回调时传递的私有数据。对Redis核心隐藏
    void *auth_module;                                                            /* The module that owns the callback, which is used
                                                                                   * to disconnect the client if the module is
                                                                                   * unloaded for cleanup. Opaque for Redis Core.*/
                                                                                  // 拥有回调的模块，用于在卸载该模块进行清理时断开客户端的连接。对于Redis Core来说是不透明的。

    /* If this client is in tracking mode and this field is non zero,
     * invalidation messages for keys fetched by this client will be send to
     * the specified client ID. */
    // 如果这个客户端处于跟踪模式，那么这个字段就不为0，通过客户端获取的键的无效消息将被送往指定ID的客户端
    uint64_t client_tracking_redirection;
    // 这段代码定义了一个名为 client_tracking_prefixes 的指针变量，指向一个字典。该字典用于存储客户端已经订阅的广播模式（BCAST mode）前缀，在客户端缓存上下文中使用。
    rax *client_tracking_prefixes; /* A dictionary of prefixes we are already
                                      subscribed to in BCAST mode, in the
                                      context of client side caching. */
                                   // 一个有已经订阅的广播模式的前缀字典，在客户单的上下文缓存中
    /* In clientsCronTrackClientsMemUsage() we track the memory usage of
     * each client and add it to the sum of all the clients of a given type,
     * however we need to remember what was the old contribution of each
     * client, and in which categoty the client was, in order to remove it
     * before adding it the new value. */
    // 在clientsCronTrackClientsMemUsage（）中，我们跟踪每个客户机的内存使用情况，
    // 并将其添加到给定类型的所有客户机的总和中，但是我们需要记住每个客户机的旧贡献是什么，
    // 以及客户机在哪个类别中，以便在添加新值之前将其删除
    uint64_t client_cron_last_memory_usage;
    int client_cron_last_memory_type;
    /* Response buffer */
    // 回复的缓存
    int bufpos;                        // 表示输出缓冲区中数据的最大字节位置
    char buf[PROTO_REPLY_CHUNK_BYTES]; // 输出缓冲区，存储待返回给客户端的命令回复数据，
} client;

struct saveparam
{
    time_t seconds;
    int changes;
};

struct moduleLoadQueueEntry
{
    sds path;
    int argc;
    robj **argv;
};

struct sentinelLoadQueueEntry
{
    int argc;
    sds *argv;
    int linenum;
    sds line;
};

struct sentinelConfig
{
    list *pre_monitor_cfg;
    list *monitor_cfg;
    list *post_monitor_cfg;
};

// 这段代码定义了一个名为 sharedObjectsStruct 的结构体，用于存储 Redis 中常用的对象和字符串。这些对象包括各种错误信息、命令关键字、常用整数对象等，旨在减少内存分配次数并提高性能。
struct sharedObjectsStruct
{
    robj *crlf, *ok, *err, *emptybulk, *czero, *cone, *pong, *space,
        *colon, *queued, *null[4], *nullarray[4], *emptymap[4], *emptyset[4],
        *emptyarray, *wrongtypeerr, *nokeyerr, *syntaxerr, *sameobjecterr,
        *outofrangeerr, *noscripterr, *loadingerr, *slowscripterr, *bgsaveerr,
        *masterdownerr, *roslaveerr, *execaborterr, *noautherr, *noreplicaserr,
        *busykeyerr, *oomerr, *plus, *messagebulk, *pmessagebulk, *subscribebulk,
        *unsubscribebulk, *psubscribebulk, *punsubscribebulk, *del, *unlink,
        *rpop, *lpop, *lpush, *rpoplpush, *lmove, *blmove, *zpopmin, *zpopmax,
        *emptyscan, *multi, *exec, *left, *right, *hset, *srem, *xgroup, *xclaim,
        *script, *replconf, *eval, *persist, *set, *pexpireat, *pexpire,
        *time, *pxat, *px, *retrycount, *force, *justid,
        *lastid, *ping, *setid, *keepttl, *load, *createconsumer,
        *getack, *special_asterick, *special_equals, *default_username,
        *select[PROTO_SHARED_SELECT_CMDS],
        *integers[OBJ_SHARED_INTEGERS],
        *mbulkhdr[OBJ_SHARED_BULKHDR_LEN], /* "*<value>\r\n" */
        *bulkhdr[OBJ_SHARED_BULKHDR_LEN];  /* "$<value>\r\n" */
    sds minstring, maxstring;
};

/* ZSETs use a specialized version of Skiplists
 * skiplist节点定义 */
typedef struct zskiplistNode
{
    sds ele;                        // 成员对象
    double score;                   // 分值
    struct zskiplistNode *backward; // 前向指针
    // 层
    struct zskiplistLevel
    {
        struct zskiplistNode *forward; // 每一层的后向指针
        unsigned long span;            // 下一个节点的跨度
    } level[];
} zskiplistNode;

// skiplist定义
// 跳跃表的定义
typedef struct zskiplist
{
    struct zskiplistNode *header, *tail; // 跳表的头节点和尾节点
    unsigned long length;                // 节点数量
    int level;                           // 层数
} zskiplist;

// 有序集
typedef struct zset
{
    dict *dict;     // 字典
    zskiplist *zsl; // 跳跃表
} zset;

typedef struct clientBufferLimitsConfig
{
    unsigned long long hard_limit_bytes;
    unsigned long long soft_limit_bytes;
    time_t soft_limit_seconds;
} clientBufferLimitsConfig;

extern clientBufferLimitsConfig clientBufferLimitsDefaults[CLIENT_TYPE_OBUF_COUNT];

/* The redisOp structure defines a Redis Operation, that is an instance of
 * a command with an argument vector, database ID, propagation target
 * (PROPAGATE_*), and command pointer.
 *
 * Currently only used to additionally propagate more commands to AOF/Replication
 * after the propagation of the executed command. */
// 这段代码定义了一个名为 redisOp 的结构体，用于表示 Redis 操作。结构体包含以下成员：
// argv：指向参数数组的指针。
// argc：参数个数。
// dbid：数据库 ID。
// target：目标标识。
// cmd：指向 Redis 命令的指针。
typedef struct redisOp
{
    robj **argv;
    int argc, dbid, target;
    struct redisCommand *cmd;
} redisOp;

/* Defines an array of Redis operations. There is an API to add to this
 * structure in an easy way.
 *
 * redisOpArrayInit();
 * redisOpArrayAppend();
 * redisOpArrayFree();
 */
// 这段代码定义了一个名为 redisOpArray 的结构体，用于存储操作数组。其中 ops 是指向 redisOp 类型的指针，表示操作数组；numops 是整数类型，表示操作数组中元素的数量。
typedef struct redisOpArray
{
    redisOp *ops;
    int numops;
} redisOpArray;

/* This structure is returned by the getMemoryOverheadData() function in
 * order to return memory overhead information. */
// 定义了一个名为 redisMemOverhead 的结构体，用于存储 Redis 内存开销的统计信息。
// 结构体成员包括内存分配峰值、客户端连接数、AOF 缓冲区大小等。
// 此外，还包含一个嵌套结构体 db，用于存储每个数据库的哈希表开销。
struct redisMemOverhead
{
    size_t peak_allocated;
    size_t total_allocated;
    size_t startup_allocated;
    size_t repl_backlog;
    size_t clients_slaves;
    size_t clients_normal;
    size_t aof_buffer;
    size_t lua_caches;
    size_t overhead_total;
    size_t dataset;
    size_t total_keys;
    size_t bytes_per_key;
    float dataset_perc;
    float peak_perc;
    float total_frag;
    ssize_t total_frag_bytes;
    float allocator_frag;
    ssize_t allocator_frag_bytes;
    float allocator_rss;
    ssize_t allocator_rss_bytes;
    float rss_extra;
    size_t rss_extra_bytes;
    size_t num_dbs;
    struct
    {
        size_t dbid;
        size_t overhead_ht_main;
        size_t overhead_ht_expires;
    } *db;
};

/* This structure can be optionally passed to RDB save/load functions in
 * order to implement additional functionalities, by storing and loading
 * metadata to the RDB file.
 *
 * Currently the only use is to select a DB at load time, useful in
 * replication in order to make sure that chained slaves (slaves of slaves)
 * select the correct DB and are able to accept the stream coming from the
 * top-level master. */
typedef struct rdbSaveInfo
{
    /* Used saving and loading. */
    int repl_stream_db; /* DB to select in server.master client. */

    /* Used only loading. */
    int repl_id_is_set;                   /* True if repl_id field is set. */
    char repl_id[CONFIG_RUN_ID_SIZE + 1]; /* Replication ID. */
    long long repl_offset;                /* 副本偏移量. */
} rdbSaveInfo;

#define RDB_SAVE_INFO_INIT \
    {                      \
        -1, 0, "0000000000000000000000000000000000000000", -1}

// 这段代码定义了一个名为 malloc_stats 的结构体，用于存储与内存分配相关的统计信息。具体字段包括：
// zmalloc_used：zmalloc 使用的内存量
// process_rss：进程占用的物理内存大小
// allocator_allocated：分配器已分配的内存量
// allocator_active：分配器活跃的内存量
// allocator_resident：分配器驻留的内存量
struct malloc_stats
{
    size_t zmalloc_used;
    size_t process_rss;
    size_t allocator_allocated;
    size_t allocator_active;
    size_t allocator_resident;
};

/*-----------------------------------------------------------------------------
 * TLS Context Configuration
 *----------------------------------------------------------------------------*/

typedef struct redisTLSContextConfig
{
    char *cert_file;        /* Server side and optionally client side cert file name */
    char *key_file;         /* Private key filename for cert_file */
    char *client_cert_file; /* Certificate to use as a client; if none, use cert_file */
    char *client_key_file;  /* Private key filename for client_cert_file */
    char *dh_params_file;
    char *ca_cert_file;
    char *ca_cert_dir;
    char *protocols;
    char *ciphers;
    char *ciphersuites;
    int prefer_server_ciphers;
    int session_caching;
    int session_cache_size;
    int session_cache_timeout;
} redisTLSContextConfig;

/*-----------------------------------------------------------------------------
 * Global server state
 *----------------------------------------------------------------------------*/

struct clusterState;

/* AIX defines hz to __hz, we don't use this define and in order to allow
 * Redis build on AIX we need to undef it. */
#ifdef _AIX
#undef hz
#endif

#define CHILD_TYPE_NONE 0
#define CHILD_TYPE_RDB 1
#define CHILD_TYPE_AOF 2
#define CHILD_TYPE_LDB 3
#define CHILD_TYPE_MODULE 4

// redisServer结构体存储服务端配置项、运行时数据
struct redisServer
{
    /* General */
    // 常规
    pid_t pid; /* Main process pid. */                                                                  // 主进程ID
    pthread_t main_thread_id; /* Main thread id */                                                      // 主线程ID
    char *configfile; /* Absolute config file path, or NULL */                                          // 配置文件路径
    char *executable; /* Absolute executable file path. */                                              // redis的可执行文件路径
    char **exec_argv; /* Executable argv vector (copy). */                                              // 记录redis执行的参数
    int dynamic_hz; /* Change hz value depending on # of clients. */                                    // 根据客户端更改 hz 值。
    int config_hz;                                                                                      /* Configured HZ value. May be different than
                                                                                                             the actual 'hz' field value if dynamic-hz
                                                                                                             is enabled. */
                                                                                                        // 配置的 HZ 值。如果启用了动态 hz，则可能与实际的"hz"字段值不同。
    mode_t umask; /* The umask value of the process on startup */                                       // 进程在启动时的掩码值
    int hz; /* serverCron() calls frequency in hertz */                                                 // redis 定时任务触发的频率
    int in_fork_child; /* indication that this is a fork child */                                       // 表明这是一个子进程
    redisDb *db;                                                                                        // redisDb 数组，默认 16 个 redisDb
    dict *commands; /* Command table */                                                                 // redis 支持的命令的字典
    dict *orig_commands; /* Command table before command renaming. */                                   // 没有转化的命令
    aeEventLoop *el;                                                                                    // redis事件循环实例
    rax *errors; /* Errors table */                                                                     // 错误表
    redisAtomic unsigned int lruclock; /* Clock for LRU eviction */                                     // LRU驱逐时钟
    volatile sig_atomic_t shutdown_asap; /* SHUTDOWN needed ASAP */                                     // 需要尽快关闭
    int activerehashing; /* Incremental rehash in serverCron() */                                       // serverCron（） 中的增量重新哈希
    int active_defrag_running; /* Active defragmentation running (holds current scan aggressiveness) */ // 活动碎片整理正在运行（保持当前扫描主动性）
    char *pidfile; /* PID file path */                                                                  // pidfile路径
    int arch_bits; /* 32 or 64 depending on sizeof(long) */                                             // 32或者64取决于long的大小
    int cronloops; /* Number of times the cron function run */                                          // cron 函数运行的次数
    char runid[CONFIG_RUN_ID_SIZE + 1]; /* ID always different at every exec. */                        // 当前redis实例的 runid
    int sentinel_mode; /* True if this instance is a Sentinel. */                                       // 如果此实例是哨兵，则为 true。
    size_t initial_memory_usage; /* Bytes used after initialization. */                                 // 初始化后使用的字节数。
    int always_show_logo; /* Show logo even for non-stdout logging. */                                  // 始终显示logo
    int in_eval; /* Are we inside EVAL? */                                                              // 我们在EVAL里面吗？
    int in_exec; /* Are we inside EXEC? */                                                              // 我们在EXEC里面吗？
    int propagate_in_transaction; /* Make sure we don't propagate nested MULTI/EXEC */                  // 确保我们不会传播嵌套的 MULTIEXEC
    char *ignore_warnings; /* Config: warnings that should be ignored. */                               // 配置：应忽略的警告。
    int client_pause_in_transaction; /* Was a client pause executed during this Exec? */                // 在此执行期间是否执行了客户端暂停？

    /* Modules */
    // 模块
    dict *moduleapi; /* Exported core APIs dictionary for modules. */ // 导出模块的核心 API 字典。
    dict *sharedapi;                                                  /* Like moduleapi but containing the APIs that
                                                                         modules share with each other. */
                                                                      // 与模块API类似，但包含模块相互共享的API。
    list *loadmodule_queue; /* List of modules to load at startup. */ // 启动时要加载的模块列表。
    int module_blocked_pipe[2];                                       /* Pipe used to awake the event loop if a
                                                                         client blocked on a module command needs
                                                                         to be processed. */
                                                                      // 用于在需要处理模块命令上被阻止的客户端时唤醒事件循环的管道。
    pid_t child_pid; /* PID of current child */                       // 当前子项的 PID
    int child_type; /* Type of current child */                       // 当前子项的类型

    /* Networking */
    // 网络
    int port; /* TCP listening port */                                        // TCP 侦听端口
    int tls_port; /* TLS listening port */                                    // TLS 侦听端口
    int tcp_backlog; /* TCP listen() backlog */                               // TCP 侦听（） 积压工作
    char *bindaddr[CONFIG_BINDADDR_MAX]; /* Addresses we should bind to */    // 我们应该绑定到的地址
    int bindaddr_count; /* Number of addresses in server.bindaddr[] */        // 绑定的地址数
    char *unixsocket; /* UNIX socket path */                                  // UNIX 套接字路径
    mode_t unixsocketperm; /* UNIX socket permission */                       // UNIX 套接字权限
    int ipfd[CONFIG_BINDADDR_MAX]; /* TCP socket file descriptors */          // TCP 套接字文件描述符
    int ipfd_count; /* Used slots in ipfd[] */                                // ipfd 中使用的插槽数量
    int tlsfd[CONFIG_BINDADDR_MAX]; /* TLS socket file descriptors */         // TLS 套接字文件描述符
    int tlsfd_count; /* Used slots in tlsfd[] */                              // tlsfd 中使用的插槽数量
    int sofd; /* Unix socket file descriptor */                               // Unix 套接字文件描述符
    int cfd[CONFIG_BINDADDR_MAX]; /* Cluster bus listening socket */          // 集群总线侦听套接字
    int cfd_count; /* Used slots in cfd[] */                                  // cfd 中使用的插槽数量
    list *clients; /* List of active clients */                               // 所有活跃的client
    list *clients_to_close; /* Clients to close asynchronously */             // 要异步关闭的客户端
    list *clients_pending_write; /* There is to write or install handler. */  // 待写回数据的客户端
    list *clients_pending_read; /* Client has pending read socket buffers. */ // 待读取数据的客户端
    list *slaves, *monitors; /* List of slaves and MONITORs */                // 从站和监视器列表
    client *current_client; /* Current client executing the command. */       // 当前正在执行命令的客户端。
    rax *clients_timeout_table;                                               /* Radix tree for blocked clients timeouts. */
    long fixed_time_expire;                                                   /* If > 0, expire keys against server.mstime. */
    rax *clients_index;                                                       /* Active clients dictionary by client ID. */
    pause_type client_pause_type;                                             /* True if clients are currently paused */
    list *paused_clients;                                                     /* List of pause clients */
    mstime_t client_pause_end_time;                                           /* Time when we undo clients_paused */
    char neterr[ANET_ERR_LEN];                                                /* Error buffer for anet.c */
    dict *migrate_cached_sockets;
    /* MIGRATE cached sockets */
    // 这段代码定义了一个名为 next_client_id 的全局变量，类型为 redisAtomic uint64_t。它用于存储下一个客户端的唯一ID，并且该ID是递增的。
    redisAtomic uint64_t next_client_id;                                      /* Next client unique ID. Incremental. */
    int protected_mode;                                                       /* Don't accept external connections. */
    int gopher_enabled;                                                       /* If true the server will reply to gopher
                                                                                    queries. Will still serve RESP2 queries. */
    int io_threads_num; /* Number of IO threads to use. */                    // 要使用的 IO 线程数。
    int io_threads_do_reads; /* Read and parse from IO threads? */            // 从 IO 线程读取和解析？
    int io_threads_active; /* Is IO threads currently active? */              // IO 线程当前是否处于活动状态？
    long long events_processed_while_blocked;                                 /* processEventsWhileBlocked() */

    /* RDB / AOF loading information */
    // RDB AOF 加载信息
    volatile sig_atomic_t loading; /* We are loading data from disk if true */ // 如果为 true，我们正在从磁盘加载数据
    off_t loading_total_bytes;
    off_t loading_rdb_used_mem;
    off_t loading_loaded_bytes;
    time_t loading_start_time;
    off_t loading_process_events_interval_bytes;

    /* Fast pointers to often looked up command */
    // 指向经常查找命令的快速指针
    struct redisCommand *delCommand, *multiCommand, *lpushCommand,
        *lpopCommand, *rpopCommand, *zpopminCommand,
        *zpopmaxCommand, *sremCommand, *execCommand,
        *expireCommand, *pexpireCommand, *xclaimCommand,
        *xgroupCommand, *rpoplpushCommand, *lmoveCommand;

    /* Fields used only for stats */
    // 仅用于统计信息的字段
    time_t stat_starttime; /* Server start time */                                                  // 服务器启动时间
    long long stat_numcommands; /* Number of processed commands */                                  // 已处理的命令数
    long long stat_numconnections; /* Number of connections received */                             // 收到的连接数
    long long stat_expiredkeys; /* Number of expired keys */                                        // 过期键的数量
    double stat_expired_stale_perc; /* Percentage of keys probably expired */                       // 键可能已过期的百分比
    long long stat_expired_time_cap_reached_count;                                                  /* Early expire cylce stops.*/
    long long stat_expire_cycle_time_used;                                                          /* Cumulative microseconds used. */
    long long stat_evictedkeys;                                                                     /* Number of evicted keys (maxmemory) */
    long long stat_keyspace_hits; /* Number of successful lookups of keys */                        // 成功查找键的次数
    long long stat_keyspace_misses;                                                                 /* Number of failed lookups of keys */
    long long stat_active_defrag_hits;                                                              /* number of allocations moved */
    long long stat_active_defrag_misses;                                                            /* number of allocations scanned but not moved */
    long long stat_active_defrag_key_hits;                                                          /* number of keys with moved allocations */
    long long stat_active_defrag_key_misses;                                                        /* number of keys scanned and not moved */
    long long stat_active_defrag_scanned;                                                           /* number of dictEntries scanned */
    size_t stat_peak_memory;                                                                        /* Max used memory record */
    long long stat_fork_time;                                                                       /* Time needed to perform latest fork() */
    double stat_fork_rate;                                                                          /* Fork rate in GB/sec. */
    long long stat_total_forks;                                                                     /* Total count of fork. */
    long long stat_rejected_conn;                                                                   /* Clients rejected because of maxclients */
    long long stat_sync_full;                                                                       /* Number of full resyncs with slaves. */
    long long stat_sync_partial_ok;                                                                 /* Number of accepted PSYNC requests. */
    long long stat_sync_partial_err;                                                                /* Number of unaccepted PSYNC requests. */
    list *slowlog;                                                                                  /* SLOWLOG list of commands */
    long long slowlog_entry_id;                                                                     /* SLOWLOG current entry ID */
    long long slowlog_log_slower_than;                                                              /* SLOWLOG time limit (to get logged) */
    unsigned long slowlog_max_len;                                                                  /* SLOWLOG max number of items logged */
    struct malloc_stats cron_malloc_stats;                                                          /* sampled in serverCron(). */
    redisAtomic long long stat_net_input_bytes;                                                     /* Bytes read from network. */
    redisAtomic long long stat_net_output_bytes;                                                    /* Bytes written to network. */
    size_t stat_current_cow_bytes;                                                                  /* Copy on write bytes while child is active. */
    size_t stat_rdb_cow_bytes;                                                                      /* Copy on write bytes during RDB saving. */
    size_t stat_aof_cow_bytes;                                                                      /* Copy on write bytes during AOF rewrite. */
    size_t stat_module_cow_bytes;                                                                   /* Copy on write bytes during module fork. */
    uint64_t stat_clients_type_memory[CLIENT_TYPE_COUNT];                                           /* Mem usage by type */
    long long stat_unexpected_error_replies;                                                        /* Number of unexpected (aof-loading, replica to master, etc.) error replies */
    long long stat_total_error_replies;                                                             /* Total number of issued error replies ( command + rejected errors ) */
    long long stat_dump_payload_sanitizations;                                                      /* Number deep dump payloads integrity validations. */
    long long stat_io_reads_processed;                                                              /* Number of read events processed by IO / Main threads */
    long long stat_io_writes_processed;                                                             /* Number of write events processed by IO / Main threads */
    redisAtomic long long stat_total_reads_processed; /* Total number of read events processed */   // 已处理的读取事件总数
    redisAtomic long long stat_total_writes_processed; /* Total number of write events processed */ // 已处理的写入事件总数

    /* The following two are used to track instantaneous metrics, like
     * number of operations per second, network traffic. */
    // 以下两个用于跟踪即时指标，例如每秒的操作数、网络流量。
    struct
    {
        long long last_sample_time;  /* Timestamp of last sample in ms */
        long long last_sample_count; /* Count in last sample */
        long long samples[STATS_METRIC_SAMPLES];
        int idx;
    } inst_metric[STATS_METRIC_COUNT];

    /* Configuration */
    // 配置
    int verbosity; /* Loglevel in redis.conf */      // redis.conf配置的日志级别
    int maxidletime; /* Client timeout in seconds */ //
    int tcpkeepalive;                                /* Set SO_KEEPALIVE if non-zero. */
    int active_expire_enabled;                       /* Can be disabled for testing purposes. */
    int active_expire_effort;                        /* From 1 (default) to 10, active effort. */
    int active_defrag_enabled;
    int sanitize_dump_payload;                                     /* Enables deep sanitization for ziplist and listpack in RDB and RESTORE. */
    int skip_checksum_validation;                                  /* Disables checksum validateion for RDB and RESTORE payload. */
    int jemalloc_bg_thread;                                        /* Enable jemalloc background thread */
    size_t active_defrag_ignore_bytes;                             /* minimum amount of fragmentation waste to start active defrag */
    int active_defrag_threshold_lower;                             /* minimum percentage of fragmentation to start active defrag */
    int active_defrag_threshold_upper;                             /* maximum percentage of fragmentation at which we use maximum effort */
    int active_defrag_cycle_min;                                   /* minimal effort for defrag in CPU percentage */
    int active_defrag_cycle_max;                                   /* maximal effort for defrag in CPU percentage */
    unsigned long active_defrag_max_scan_fields;                   /* maximum number of fields of set/hash/zset/list to process from within the main dict scan */
    size_t client_max_querybuf_len;                                /* Limit for client query buffer length */
    int dbnum; /* Total number of configured DBs */                // 数据库数量
    int supervised;                                                /* 1 if supervised, 0 otherwise. */
    int supervised_mode;                                           /* See SUPERVISED_* */
    int daemonize; /* True if running as a daemon */               // 如果作为守护程序运行，则为 True
    int set_proc_title; /* True if change proc title */            // 如果更改进程标题，则为 True
    char *proc_title_template; /* Process title template format */ // 流程标题模板格式
    clientBufferLimitsConfig client_obuf_limits[CLIENT_TYPE_OBUF_COUNT];

    /* AOF persistence */
    // AOF持久性
    int aof_enabled;                   /* AOF configuration */
    int aof_state;                     /* AOF_(ON|OFF|WAIT_REWRITE) */
    int aof_fsync;                     /* Kind of fsync() policy */
    char *aof_filename;                /* Name of the AOF file */
    int aof_no_fsync_on_rewrite;       /* Don't fsync if a rewrite is in prog. */
    int aof_rewrite_perc;              /* Rewrite AOF if % growth is > M and... */
    off_t aof_rewrite_min_size;        /* the AOF file is at least N bytes. */
    off_t aof_rewrite_base_size;       /* AOF size on latest startup or rewrite. */
    off_t aof_current_size;            /* AOF 当前大小. */
    off_t aof_fsync_offset;            /* AOF offset which is already synced to disk. */
    int aof_flush_sleep;               /* Micros to sleep before flush. (used by tests) */
    int aof_rewrite_scheduled;         /* Rewrite once BGSAVE terminates. */
    list *aof_rewrite_buf_blocks;      /* Hold changes during an AOF rewrite. */
    sds aof_buf;                       /* AOF 缓冲区, written before entering the event loop */
    int aof_fd;                        /* File descriptor of currently selected AOF file */
    int aof_selected_db;               /* Currently selected DB in AOF */
    time_t aof_flush_postponed_start;  /* UNIX time of postponed AOF flush */
    time_t aof_last_fsync;             /* UNIX time of last fsync() */
    time_t aof_rewrite_time_last;      /* Time used by last AOF rewrite run. */
    time_t aof_rewrite_time_start;     /* Current AOF rewrite start time. */
    int aof_lastbgrewrite_status;      /* C_OK or C_ERR */
    unsigned long aof_delayed_fsync;   /* delayed AOF fsync() counter */
    int aof_rewrite_incremental_fsync; /* fsync incrementally while aof rewriting? */
    int rdb_save_incremental_fsync;    /* fsync incrementally while rdb saving? */
    int aof_last_write_status;         /* C_OK or C_ERR */
    int aof_last_write_errno;          /* Valid if aof_last_write_status is ERR */
    int aof_load_truncated;            /* Don't stop on unexpected AOF EOF. */
    int aof_use_rdb_preamble;          /* Use RDB preamble on AOF rewrites. */
    /* AOF pipes used to communicate between parent and child during rewrite. */
    int aof_pipe_write_data_to_child;
    int aof_pipe_read_data_from_parent;
    int aof_pipe_write_ack_to_parent;
    int aof_pipe_read_ack_from_child;
    int aof_pipe_write_ack_to_child;
    int aof_pipe_read_ack_from_parent;
    int aof_stop_sending_diff; /* If true stop sending accumulated diffs
                                      to child process. */
    sds aof_child_diff;        /* AOF diff accumulator child side. */

    /* RDB persistence */
    // RDB 持久性
    long long dirty;                                                            /* RDB持久化之后数据有变化，可以看到所有redis写命令都会执行server.dirty++ */
    long long dirty_before_bgsave; /* Used to restore dirty on failed BGSAVE */ // 记录上次RDB保存 dirty 的值
    struct saveparam *saveparams; /* Save points array for RDB */               // 保存点数组
    int saveparamslen; /* Number of saving points */                            // 保存点的数量
    char *rdb_filename; /* Name of RDB file */                                  // RDB文件名
    int rdb_compression; /* Use compression in RDB? */                          // 是否压缩RDB文件
    int rdb_checksum; /* Use RDB checksum? */                                   // 是否使用RDB校验
    int rdb_del_sync_files;                                                     /* Remove RDB files used only for SYNC if the instance does not use persistence. */
                                                                                // 删除仅用于同步的RDB文件，如果实例不使用持久化

    time_t lastsave; /* Unix time of last successful save */                                               // 上次成功保存的 Unix 时间
    time_t lastbgsave_try; /* Unix time of last attempted bgsave */                                        // 记录最后一次尝试RDB保存的 Unix 时间
    time_t rdb_save_time_last; /* Time used by last RDB save run. */                                       // 记录最后一次RDB保存的时间
    time_t rdb_save_time_start; /* Current RDB save start time. */                                         // 记录当前的RDB保存的时间
    int rdb_bgsave_scheduled; /* BGSAVE when possible if true. */                                          // 如果为真，则尝试在可能的情况下进行RDB保存
    int rdb_child_type; /* Type of save by active child. */                                                // 保存类型
    int lastbgsave_status; /* C_OK or C_ERR */                                                             // 记录最后一次RDB保存的状态
    int stop_writes_on_bgsave_err; /* Don't allow writes if can't BGSAVE */                                // 如果为真，则不允许写入
    int rdb_pipe_read; /* RDB pipe used to transfer the rdb data to the parent process in diskless repl.*/ // RDB管道，用于将RDB数据传输到父进程的磁盘less repl中

    int rdb_child_exit_pipe;       /* Used by the diskless parent allow child exit. */
    connection **rdb_pipe_conns;   /* Connections which are currently the */
    int rdb_pipe_numconns;         /* target of diskless rdb fork child. */
    int rdb_pipe_numconns_writing; /* Number of rdb conns with pending writes. */
    char *rdb_pipe_buff;           /* In diskless replication, this buffer holds data */
    int rdb_pipe_bufflen;          /* that was read from the the rdb pipe. */
    int rdb_key_save_delay;        /* Delay in microseconds between keys while
                                    * writing the RDB. (for testings). negative
                                    * value means fractions of microsecons (on average). */
    int key_load_delay;            /* Delay in microseconds between keys while
                                    * loading aof or rdb. (for testings). negative
                                    * value means fractions of microsecons (on average). */
    /* Pipe and data structures for child -> parent info sharing. */
    int child_info_pipe[2]; /* Pipe used to write the child_info_data. */
    int child_info_nread;   /* Num of bytes of the last read from pipe */
    /* Propagation of commands in AOF / replication */
    redisOpArray also_propagate; /* Additional command to propagate. */

    /* Logging */
    // 日志
    char *logfile; /* Path of log file */                     // 日志文件路径
    int syslog_enabled; /* Is syslog enabled? */              // 是否启用syslog
    char *syslog_ident; /* Syslog ident */                    // syslog标识
    int syslog_facility; /* Syslog facility */                //  syslog facility
    int crashlog_enabled;                                     /* Enable signal handler for crashlog.
                                                               * disable for clean core dumps. */
                                                              // 崩溃时使用信号处理程序进行crashlog
    int memcheck_enabled; /* Enable memory check on crash. */ // 内存检查
    int use_exit_on_panic;                                    /* Use exit() on panic and assert rather than
                                                               * abort(). useful for Valgrind. */
                                                              // 崩溃时使用exit()而不是abort()

    /* Replication (master) */
    // 复制（主）
    char replid[CONFIG_RUN_ID_SIZE + 1]; /* My current replication ID. */        // 当前任期的master的运行Id
    char replid2[CONFIG_RUN_ID_SIZE + 1]; /* replid inherited from master*/      // 上个任期的master的运行Id
    long long master_repl_offset; /* My current replication offset */            // 当前任期的缓冲区最后一个字节的复制偏移量
    long long second_replid_offset; /* Accept offsets up to this for replid2. */ // 上一个任期的缓冲区最后一个字节的复制偏移量
    int slaveseldb;                                                              /* Last SELECTed DB in replication output */
    /*
      表示发送心跳包的周期，主服务器以此周期向所有从服务器发送心跳包.
      主服务器和从服务器之间是通过TCP长连接交互数据的，就必然需要周期性地发送心跳包来检测连接有
      效性，该字段表示发送心跳包的周期，主服务器以此周期向所有从服务器发送心跳包. 可通过配置参数
      repl-ping-replica-period或者repl-ping-slave-period设置，默认为10.
    */
    int repl_ping_slave_period; /* Master pings the slave every N seconds */ // 主站每 N 秒对从站执行 ping 操作一次
    /*
      复制缓冲区，用于缓存主服务器已执行且待发送给从服务器的命令请求；
      缓冲区大小由字段repl_backlog_size指定，其可通过配置参数repl-backlog-size设置，默认为1MB
    */
    char *repl_backlog; /* Replication backlog for partial syncs */  // 部分同步的复制积压工作
    long long repl_backlog_size; /* Backlog circular buffer size */  // 复制缓冲区的大小
    long long repl_backlog_histlen; /* Backlog actual data length */ // 复制缓冲区中存储的命令请求数据长度
    /* 复制缓冲区中存储的命令请求最后一个字节索引位置,即向复制缓冲区写入数据时会从该索引位置开始 */
    long long repl_backlog_idx; /* Backlog circular buffer current offset,
                             that is the next byte will'll write to.*/
    /* 复制缓冲区中第一个字节的复制偏移量 */
    long long repl_backlog_off;     /* Replication "master offset" of first
                                 byte in the replication backlog buffer.*/
    time_t repl_backlog_time_limit; /* Time without slaves after the backlog
                                 gets released. */
                                    // 积压工作释放后没有从属的时间。
    /* 表示有多久没有从机了*/
    time_t repl_no_slaves_since; /* We have no slaves since that time.
                              Only valid if server.slaves len is 0. */
                                 // 仅当服务器从属 len 为 0 时才有效
    /* 当有效从服务器的数目小于该值时，主服务器会拒绝执行写命令 */
    int repl_min_slaves_to_write; /* Min number of slaves to write. */ // 要写入的最小从属数量。
    /* 决定从服务器是否处于失效状态的超时门限 */
    int repl_min_slaves_max_lag; /* Max lag of <count> slaves to write. */
    /*
        当前有效从服务器的数目.
        什么样的从服务器是有效的呢？我们说过主服务器和从服务器之间是通过TCP长连接交互数据的，
        并且会发送心跳包来检测连接有效性；主服务器会记录每个从服务器上次心跳检测成功的时间repl_ack_time，
        并且定时检测当前时间距离repl_ack_time是否超过一定超时门限，如果超过则认为从服务器处于失效状态。
        字段repl_min_slaves_max_lag存储的就是该超时门限，可通过配置参数min-slaves-max-lag
        或者min-replicas-max-lag设置，默认为10，单位秒。
     */
    int repl_good_slaves_count;                                               /* Number of slaves with lag <= max_lag. */
    int repl_diskless_sync; /* Master send RDB to slaves sockets directly. */ // 主站将RDB直接发送到从机套接字。
    int repl_diskless_load;                                                   /* Slave parse RDB directly from the socket.
                                                                               * see REPL_DISKLESS_LOAD_* enum */
                                                                              // 从站直接从套接字解析RDB。参见REPL_DISKLESS_LOAD_枚举
    int repl_diskless_sync_delay;                                             /* Delay to start a diskless repl BGSAVE. */

    /* Replication (slave) */
    /* 必须要这个用户才能登录 */
    char *masteruser; /* AUTH with this user and masterauth with master */ // 此用户的身份验证和主用户的主身份验证
    /* masteruser用户对应的验证密码，当主服务器配置了"requirepass password"时，即表示从服
       务器必须通过密码认证才能同步主服务器数据。同样的需要在从服务器配置"masterauth<master-
       password>"，用于设置请求同步主服务器时的认证密码.
     */
    sds masterauth; /* AUTH with this password with master */ // 使用此密码与主密码进行身份验证
    /* 主服务器的IP */
    char *masterhost; /* Hostname of master */ // 主服务器的主机名
    /* 主服务器的端口 */
    int masterport; /* Port of master */                           // 主服务器的端口
    int repl_timeout; /* Timeout after N seconds of master idle */ // 主服务器空闲 N 秒后超时
    /* 当主从服务器成功建立连接之后，从服务器将成为主服务器的客户端，同样的主服务器也会成为从服务
       器的客户端，master即为主服务器，类型为client
    */
    client *master; /* Client that is master for this slave */         // 作为此从站主站的客户端
    client *cached_master; /* Cached master to be reused for PSYNC. */ // 要重用于 PSYNC 的缓存主服务器。
    int repl_syncio_timeout; /* Timeout for synchronous I/O calls */   // 同步 IO 调用超时
    /* 主从复制流程的进展（从服务器状态）*/
    int repl_state; /* Replication status if the instance is a slave */          // 复制状态（如果实例是从属实例）
    off_t repl_transfer_size; /* Size of RDB to read from master during sync. */ // 同步期间从主服务器读取的 RDB 大小。
    off_t repl_transfer_read; /* Amount of RDB read from master during sync. */  // 同步期间从主服务器读取的 RDB 量。
    off_t repl_transfer_last_fsync_off; /* Offset when we fsync-ed last time. */ // 上次同步时的偏移量。
    connection *repl_transfer_s; /* Slave -> Master SYNC connection */           // 从机到主机的连接
    int repl_transfer_fd; /* Slave -> Master SYNC temp file descriptor */        // 从机到主机的临时文件描述符
    char *repl_transfer_tmpfile; /* Slave-> master SYNC temp file name */        // 从机到主机的临时文件名
    time_t repl_transfer_lastio; /* Unix time of the latest read, for timeout */ // 最新读取的 Unix 时间，用于超时
    /* 当主从服务器断开连接时，该变量表示从服务器是否继续处理命令请求，可通过配置参数
       slave-serve-stale-data或者replica-serve-stale-data设置，默认为1，即可以继续处理
       命令请求。
    */
    int repl_serve_stale_data; /* Serve stale data when link is down? */
    /* 配置从机是否只是可读的(不处理除了主服务器发来以外的写命令).
       可通过配置参数slave-read-only或者replica-read-only设置,默认为1,即从服务器不处理写命
       令请求，除非该命令是主服务器发送过来的.
    */
    int repl_slave_ro; /* Slave is read only? */
    /* 从机是否没有键的过期处理策略*/
    int repl_slave_ignore_maxmemory; /* If true slaves do not evict. */
    /* 从机与主机断开的时间 */
    time_t repl_down_since; /* Unix time at which link with master went down */ // 与主站的链接关闭的 Unix 时间
    int repl_disable_tcp_nodelay; /* Disable TCP_NODELAY after SYNC? */         // 同步后禁用TCP_NODELAY？
    int slave_priority; /* Reported in INFO and used by Sentinel. */            // 在 INFO 中报告并由 Sentinel 使用。
    int slave_announce_port; /* Give the master this listening port. */         // 为主机提供此侦听端口。
    char *slave_announce_ip; /* Give the master this ip address. */             // 为主服务器提供此 IP 地址。
    /* The following two fields is where we store master PSYNC replid/offset
     * while the PSYNC is in progress. At the end we'll copy the fields into
     * the server->master client structure. */
    char master_replid[CONFIG_RUN_ID_SIZE + 1];                       /* Master PSYNC runid. */
    long long master_initial_offset;                                  /* Master PSYNC offset. */
    int repl_slave_lazy_flush; /* Lazy FLUSHALL before loading DB? */ // 加载数据库之前懒惰FLUSHALL？

    /* Replication script cache. */
    // 复制脚本缓存。
    dict *repl_scriptcache_dict;                                         /* SHA1 all slaves are aware of. */
    list *repl_scriptcache_fifo; /* First in, first out LRU eviction. */ // 先进先出 LRU 驱逐。
    unsigned int repl_scriptcache_size;                                  /* Max number of elements. */

    /* Synchronous replication. */
    // 同步复制。
    list *clients_waiting_acks; /* Clients waiting in WAIT command. */ // 在等待命令中等待的客户端。
    int get_ack_from_slaves; /* If true we send REPLCONF GETACK. */    // 如果属实，我们发送REPLCONF GETACK。

    /* Limits */
    // 限制
    unsigned int maxclients; /* Max number of simultaneous clients */                   // 最大并发客户端数
    unsigned long long maxmemory; /* Max number of memory bytes to use */               // 要使用的最大内存字节数
    int maxmemory_policy; /* Policy for key eviction */                                 // 密钥逐出策略
    int maxmemory_samples; /* Precision of random sampling */                           // 由 redis.conf 中的配置项 maxmemory-samples 决定的，该配置项的默认值是 5
    int maxmemory_eviction_tenacity; /* Aggressiveness of eviction processing */        // 驱逐处理的积极性
    int lfu_log_factor; /* LFU logarithmic counter factor. */                           // LFU 对数计数器因子
    int lfu_decay_time; /* LFU counter decay factor. */                                 // LFU 计数器衰减因子。
    long long proto_max_bulk_len; /* Protocol bulk length maximum size. */              // 协议最大批量长度最大大小
    int oom_score_adj_base; /* Base oom_score_adj value, as observed on startup */      // 启动时观察到的 oom_score_adj 的基本值
    int oom_score_adj_values[CONFIG_OOM_COUNT]; /* Linux oom_score_adj configuration */ // Linux oom_score_adj 配置
    int oom_score_adj; /* If true, oom_score_adj is managed */                          // 如果为真，则管理 oom_score_adj
    int disable_thp; /* If true, disable THP by syscall */                              // 禁用 THP 的系统调用

    /* Blocked clients */
    // 被阻止的客户端
    unsigned int blocked_clients; /* # of clients executing a blocking cmd.*/
    unsigned int blocked_clients_by_type[BLOCKED_NUM];
    list *unblocked_clients; /* list of clients to unblock before next loop */
    list *ready_keys;        /* List of readyList structures for BLPOP & co */

    /* Client side caching. */
    // 客户端缓存。
    unsigned int tracking_clients;  /* # of clients with tracking enabled.*/
    size_t tracking_table_max_keys; /* Max number of keys in tracking table. */
    /* Sort parameters - qsort_r() is only available under BSD so we
     * have to take this state global, in order to pass it to sortCompare() */
    int sort_desc;
    int sort_alpha;
    int sort_bypattern;
    int sort_store;

    /* Zip structure config, see redis.conf for more information  */
    // zip结构配置，更多信息见redis.conf
    size_t hash_max_ziplist_entries;
    size_t hash_max_ziplist_value;
    size_t set_max_intset_entries;
    size_t zset_max_ziplist_entries;
    size_t zset_max_ziplist_value;
    size_t hll_sparse_max_bytes;
    size_t stream_node_max_bytes;
    long long stream_node_max_entries;

    /* List parameters */
    int list_max_ziplist_size;
    int list_compress_depth;

    /* time cache */
    redisAtomic time_t unixtime; /* Unix time sampled every cron cycle. */
    time_t timezone;             /* Cached timezone. As set by tzset(). */
    int daylight_active;         /* Currently in daylight saving time. */
    mstime_t mstime;             /* 'unixtime' in milliseconds. */
    ustime_t ustime;             /* 'unixtime' in microseconds. */
    size_t blocking_op_nesting;  /* Nesting level of blocking operation, used to reset blocked_last_cron. */
    long long blocked_last_cron; /* Indicate the mstime of the last time we did cron jobs from a blocking operation */

    /* Pubsub */
    // 发布订阅
    dict *pubsub_channels; /* Map channels to list of subscribed clients */ // 将频道映射到已订阅客户端的列表(就是保存客户端和订阅的频道信息)
    list *pubsub_patterns; /* A list of pubsub_patterns */                  // pubsub_patterns列表
    dict *pubsub_patterns_dict;                                             /* A dict of pubsub_patterns */
    int notify_keyspace_events;                                             /* Events to propagate via Pub/Sub. This is an
                                                                               xor of NOTIFY_... flags. */
    /* Cluster */
    // 集群
    int cluster_enabled; /* Is cluster enabled? */                            // 是否启用了群集？
    mstime_t cluster_node_timeout; /* Cluster node timeout. */                // 群集节点超时。
    char *cluster_configfile; /* Cluster auto-generated config file name. */  // 群集自动生成的配置文件名。
    struct clusterState *cluster; /* State of the cluster */                  // 群集的状态
    int cluster_migration_barrier; /* Cluster replicas migration barrier. */  // 群集副本迁移屏障。
    int cluster_slave_validity_factor; /* Slave max data age for failover. */ // 故障转移的从站最大数据期限。
    int cluster_require_full_coverage;                                        /* If true, put the cluster down if
                                                                                 there is at least an uncovered slot.*/
                                                                              // 如果为 true，则如果至少有一个未覆盖的插槽，请关闭群集。
    int cluster_slave_no_failover;                                            /* Prevent slave from starting a failover
                                                                              if the master is in failure state. */
                                                                              // 防止从站启动故障转移，如果主站处于故障状态。
    char *cluster_announce_ip; /* IP address to announce on cluster bus. */   // 要在群集总线上宣布的 IP 地址。
    int cluster_announce_port; /* base port to announce on cluster bus. */    // 要在群集总线上宣布的基本端口。
    int cluster_announce_bus_port; /* bus port to announce on cluster bus. */ // 要在群集总线上公告的总线端口。
    int cluster_module_flags;                                                 /* Set of flags that Redis modules are able
                                                                             to set in order to suppress certain
                                                                             native Redis Cluster features. Check the
                                                                             REDISMODULE_CLUSTER_FLAG_*. */
                                                                              // Redis 模块能够设置的标志集，用于抑制某些本机 Redis 集群功能。检查REDISMODULE_CLUSTER_FLAG_。
    int cluster_allow_reads_when_down;                                        /* Are reads allowed when the cluster
                                                                               is down? */
                                                                              // 集群关闭时是否允许读取？
    int cluster_config_file_lock_fd; /* cluster config fd, will be flock */   // 集群配置 FD，将蜂拥而至

    /* Scripting */
    // 脚本
    lua_State *lua;                     /* The Lua interpreter. We use just one for all clients */
    client *lua_client;                 /* The "fake client" to query Redis from Lua */
    client *lua_caller;                 /* The client running EVAL right now, or NULL */
    char *lua_cur_script;               /* SHA1 of the script currently running, or NULL */
    dict *lua_scripts;                  /* A dictionary of SHA1 -> Lua scripts */
    unsigned long long lua_scripts_mem; /* Cached scripts' memory + oh */
    mstime_t lua_time_limit;            /* Script timeout in milliseconds */
    mstime_t lua_time_start;            /* Start time of script, milliseconds time */
    int lua_write_dirty;                /* True if a write command was called during the
                             execution of the current script. */
    int lua_random_dirty;               /* True if a random command was called during the
                             execution of the current script. */
    int lua_replicate_commands;         /* True if we are doing single commands repl. */
    int lua_multi_emitted;              /* True if we already propagated MULTI. */
    int lua_repl;                       /* Script replication flags for redis.set_repl(). */
    int lua_timedout;                   /* True if we reached the time limit for script
                             execution. */
    int lua_kill;                       /* Kill the script if true. */
    int lua_always_replicate_commands;  /* Default replication type. */
    int lua_oom;                        /* OOM detected when script start? */

    /* Lazy free */
    // 惰性释放
    int lazyfree_lazy_eviction;
    int lazyfree_lazy_expire;
    int lazyfree_lazy_server_del;
    int lazyfree_lazy_user_del;
    int lazyfree_lazy_user_flush;

    /* Latency monitor */
    // 延迟监视器
    long long latency_monitor_threshold;
    dict *latency_events;

    /* ACLs */
    char *acl_filename; /* ACL Users file. NULL if not configured. */       // ACL 用户文件。空（如果未配置）。
    unsigned long acllog_max_len; /* Maximum length of the ACL LOG list. */ // ACL 日志列表的最大长度。
    sds requirepass;                                                        /* Remember the cleartext password set with
                                                                               the old "requirepass" directive for
                                                                               backward compatibility with Redis <= 5. */
                                                                            // 请记住使用旧的"requirepass"指令设置的明文密码，以便向后兼容Redis <= 5。
    int acl_pubusub_default; /* Default ACL pub/sub channels flag */        // 默认 ACL 发布子频道标志

    /* Assert & bug reporting */
    // 断言和错误报告
    int watchdog_period; /* Software watchdog period in ms. 0 = off */

    /* System hardware info */
    // 系统硬件信息
    size_t system_memory_size; /* Total memory in system as reported by OS */

    /* TLS Configuration */
    // TLS 配置
    int tls_cluster;
    int tls_replication;
    int tls_auth_clients;
    redisTLSContextConfig tls_ctx_config;

    /* cpu affinity */
    // CPU 关联性
    char *server_cpulist;      /* cpu affinity list of redis server main/io thread. */
    char *bio_cpulist;         /* cpu affinity list of bio thread. */
    char *aof_rewrite_cpulist; /* cpu affinity list of aof rewrite process. */
    char *bgsave_cpulist;      /* cpu affinity list of bgsave process. */

    /* Sentinel config */
    // 哨兵配置
    struct sentinelConfig *sentinel_config; /* sentinel config to load at startup time. */

    /* Coordinate failover info */
    // 协调故障转移信息
    mstime_t failover_end_time; /* Deadline for failover command. */
    int force_failover;         /* If true then failover will be foreced at the
                                 * deadline, otherwise failover is aborted. */
    char *target_replica_host;  /* Failover target host. If null during a
                                 * failover then any replica can be used. */
    int target_replica_port;    /* Failover target port */
    int failover_state;         /* Failover state */
};

// 这段代码定义了一个名为 pubsubPattern 的结构体，用于表示发布/订阅模式中的模式匹配。
// 结构体包含两个成员：一个指向 client 类型的指针，表示客户端；一个指向 robj 类型的指针，表示模式对象。
typedef struct pubsubPattern
{
    client *client;
    robj *pattern;
} pubsubPattern;

#define MAX_KEYS_BUFFER 256

/* A result structure for the various getkeys function calls. It lists the
 * keys as indices to the provided argv.
 */
typedef struct
{
    int keysbuf[MAX_KEYS_BUFFER]; /* Pre-allocated buffer, to save heap allocations */
    int *keys;                    /* Key indices array, points to keysbuf or heap */
    int numkeys;                  /* Number of key indices return */
    int size;                     /* Available array size */
} getKeysResult;
#define GETKEYS_RESULT_INIT \
    {                       \
        {0}, NULL, 0, MAX_KEYS_BUFFER}

typedef void redisCommandProc(client *c);

typedef int redisGetKeysProc(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);

/*
 * redis将所有的命令都封装为一个redisCommand结构体，
 * 并用函数指针redisCommandProc指向该命令的具体逻辑
 */
struct redisCommand
{
    char *name;                                                               // 命令名字
    redisCommandProc *proc;                                                   // 指向该命令的具体逻辑,命令处理函数
    int arity;                                                                // 命令参数数目，用于校验命令请求格式是否正确；当arity小于0时，表示命令参数数目大于等于arity；
                                                                              // 当arity大于0时，表示命令参数数目必须为arity；注意命令请求中，命令的名称本身也是一个参数，
                                                                              // 如get命令的参数数目为2，命令请求格式为get key。
    char *sflags; /* Flags as string representation, one char per flag. */    // 命令标志，例如标识命令时读命令还是写命令
    uint64_t flags; /* The actual flags, obtained from the 'sflags' field. */ // 命令的二进制标志，服务器启动时解析sflags字段生成。
    /* Use a function to determine keys arguments in a command line.
     * Used for Redis Cluster redirect. */
    // 使用函数确定命令行中的键参数。用于 Redis 集群重定向。
    redisGetKeysProc *getkeys_proc;
    /* What keys should be loaded in background when calling this command? */
    // 调用此命令时应在后台加载哪些键？
    int firstkey; /* The first argument that's a key (0 = no keys) */ // 第一个参数是键（0 = 无键）
    int lastkey; /* The last argument that's a key */                 // 最后一个参数是一个键
    int keystep; /* The step between first and last key */            // 第一个键和最后一个键之间的步骤
    long long microseconds, calls, rejected_calls, failed_calls;      // calls: 从服务器启动至今命令执行的次数，用于统计。
    int id;                                                           /* Command ID. This is a progressive ID starting from 0 that
                                                                             is assigned at runtime, and is used in order to check
                                                                             ACLs. A connection is able to execute a given command if
                                                                             the user associated to the connection has this command
                                                                             bit set in the bitmap of allowed commands. */
                                                                      // 命令 ID。这是一个从 0 开始的渐进式 ID，在运行时分配，用于检查 ACL。如果与连接关联的用户在允许命令的位图中设置了此命令位，则连接能够执行给定命令。
};

// 这段代码定义了一个名为 redisError 的结构体，其中包含一个名为 count 的成员变量，类型为 long long。
// 该结构体用于表示错误信息，count 可能用于记录错误的数量或其他与错误相关的计数值。
struct redisError
{
    long long count;
};

// 这段代码定义了一个名为 redisFunctionSym 的结构体，用于存储函数符号的信息。
// 它包含两个成员：name 是一个指向函数名称的字符串指针，pointer 是一个无符号长整型，用于存储函数的地址。
struct redisFunctionSym
{
    char *name;
    unsigned long pointer;
};

// 这段代码定义了一个名为 redisSortObject 的结构体，用于存储 Redis 排序操作中的对象。结构体包含一个指向 Redis 对象的指针 obj 和一个联合体 u，联合体中包含一个双精度浮点数 score 或指向 Redis 对象的指针 cmpobj，用于不同的排序依据。
typedef struct _redisSortObject
{
    robj *obj;
    union
    {
        double score;
        robj *cmpobj;
    } u;
} redisSortObject;

// 这段代码定义了一个名为 _redisSortOperation 的结构体，并使用 typedef 将其别名为 redisSortOperation。该结构体包含两个成员：一个整型变量 type，用于表示操作类型；一个指向 robj 类型对象的指针 pattern，用于存储模式。
typedef struct _redisSortOperation
{
    int type;
    robj *pattern;
} redisSortOperation;

/* Structure to hold list iteration abstraction. */
// 用于保存列表迭代抽象的结构。
typedef struct
{
    robj *subject;
    unsigned char encoding;
    unsigned char direction; /* Iteration direction */ // 迭代方向
    quicklistIter *iter;
} listTypeIterator;

/* Structure for an entry while iterating over a list. */
// 循环访问列表时条目的结构。
typedef struct
{
    listTypeIterator *li;
    quicklistEntry entry; /* Entry in quicklist */ // 快速列表中的条目
} listTypeEntry;

/* Structure to hold set iteration abstraction. */
// 用于保存集合迭代抽象的结构。
typedef struct
{
    robj *subject;
    int encoding;
    int ii; /* intset iterator */ // 集成迭代器
    dictIterator *di;
} setTypeIterator;

/* Structure to hold hash iteration abstraction. Note that iteration over
 * hashes involves both fields and values. Because it is possible that
 * not both are required, store pointers in the iterator to avoid
 * unnecessary memory allocation for fields/values. */
// 用于保存哈希迭代抽象的结构。请注意，哈希迭代涉及字段和值。由于可能不需要两者，因此请在迭代器中存储指针以避免为字段值分配不必要的内存。
typedef struct
{
    robj *subject;
    int encoding;

    unsigned char *fptr, *vptr;

    dictIterator *di;
    dictEntry *de;
} hashTypeIterator;

#include "stream.h" /* Stream data type header file. */ // 流数据类型头文件。

#define OBJ_HASH_KEY 1
#define OBJ_HASH_VALUE 2

/*-----------------------------------------------------------------------------
 * Extern declarations  外部声明
 *----------------------------------------------------------------------------*/

extern struct redisServer server;
extern struct sharedObjectsStruct shared;
extern dictType objectKeyPointerValueDictType;
extern dictType objectKeyHeapPointerValueDictType;
extern dictType setDictType;
extern dictType zsetDictType;
extern dictType clusterNodesDictType;
extern dictType clusterNodesBlackListDictType;
extern dictType dbDictType;
extern dictType shaScriptObjectDictType;
extern double R_Zero, R_PosInf, R_NegInf, R_Nan;
extern dictType hashDictType;
extern dictType replScriptCacheDictType;
extern dictType dbExpiresDictType;
extern dictType modulesDictType;
extern dictType sdsReplyDictType;

/*-----------------------------------------------------------------------------
 * Functions prototypes 函数原型
 *----------------------------------------------------------------------------*/

/* Modules */
// 模块
// 该函数 moduleInitModulesSystem 用于初始化模块系统。具体功能包括初始化各个子模块，设置初始参数和状态，确保系统在启动时处于正确的工作状态。
void moduleInitModulesSystem(void);
void moduleInitModulesSystemLast(void);
// moduleLoad 函数用于加载模块，参数包括模块路径、参数列表和参数个数。该函数返回一个整数表示加载结果，成功返回0，失败返回非0值。argv 是指向指针数组的指针，argc 是参数个数。
int moduleLoad(const char *path, void **argv, int argc);
void moduleLoadFromQueue(void);
// 该函数用于通过API获取Redis命令中的键。它接收Redis命令结构体、参数列表、参数个数和结果结构体作为输入，返回一个整数值表示操作结果。
int moduleGetCommandKeysViaAPI(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
moduleType *moduleTypeLookupModuleByID(uint64_t id);
void moduleTypeNameByID(char *name, uint64_t moduleid);
void moduleFreeContext(struct RedisModuleCtx *ctx);
void unblockClientFromModule(client *c);
void moduleHandleBlockedClients(void);
void moduleBlockedClientTimedOut(client *c);
void moduleBlockedClientPipeReadable(aeEventLoop *el, int fd, void *privdata, int mask);
size_t moduleCount(void);
void moduleAcquireGIL(void);
int moduleTryAcquireGIL(void);
void moduleReleaseGIL(void);
void moduleNotifyKeyspaceEvent(int type, const char *event, robj *key, int dbid);
void moduleCallCommandFilters(client *c);
void moduleForkDoneHandler(int exitcode, int bysignal);
int terminateModuleForkChild(int child_pid, int wait);
ssize_t rdbSaveModulesAux(rio *rdb, int when);
int moduleAllDatatypesHandleErrors(void);
sds modulesCollectInfo(sds info, const char *section, int for_crash_report, int sections);
void moduleFireServerEvent(uint64_t eid, int subid, void *data);
void processModuleLoadingProgressEvent(int is_aof);
int moduleTryServeClientBlockedOnKey(client *c, robj *key);
void moduleUnblockClient(client *c);
int moduleClientIsBlockedOnKeys(client *c);
void moduleNotifyUserChanged(client *c);
void moduleNotifyKeyUnlink(robj *key, robj *val);
robj *moduleTypeDupOrReply(client *c, robj *fromkey, robj *tokey, robj *value);
int moduleDefragValue(robj *key, robj *obj, long *defragged);
int moduleLateDefrag(robj *key, robj *value, unsigned long *cursor, long long endtime, long long *defragged);
long moduleDefragGlobals(void);

/* Utils */
// 这两个函数 ustime 和 mstime 分别用于获取当前时间，前者返回微秒级的时间戳，后者返回毫秒级的时间戳。它们都返回一个 long long 类型的值
long long ustime(void);
long long mstime(void);

void getRandomHexChars(char *p, size_t len);
void getRandomBytes(unsigned char *p, size_t len);
uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l);
void exitFromChild(int retcode);

size_t redisPopcount(void *s, long count);

int redisSetProcTitle(char *title);

int validateProcTitleTemplate(const char *template);

int redisCommunicateSystemd(const char *sd_notify_msg);

void redisSetCpuAffinity(const char *cpulist);

/* networking.c -- Networking and Client related operations */
// networking.c -- 网络和客户端相关操作
client *createClient(connection *conn);
void closeTimedoutClients(void);
void freeClient(client *c);
void freeClientAsync(client *c);
void resetClient(client *c);
void freeClientOriginalArgv(client *c);
void sendReplyToClient(connection *conn);
void *addReplyDeferredLen(client *c);
void setDeferredArrayLen(client *c, void *node, long length);
void setDeferredMapLen(client *c, void *node, long length);
void setDeferredSetLen(client *c, void *node, long length);
void setDeferredAttributeLen(client *c, void *node, long length);
void setDeferredPushLen(client *c, void *node, long length);
void processInputBuffer(client *c);
void processGopherRequest(client *c);

void acceptHandler(aeEventLoop *el, int fd, void *privdata, int mask);
void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask);
void acceptTLSHandler(aeEventLoop *el, int fd, void *privdata, int mask);
void acceptUnixHandler(aeEventLoop *el, int fd, void *privdata, int mask);

void readQueryFromClient(connection *conn); // 读取来自客户端的查询
void addReplyNull(client *c);
void addReplyNullArray(client *c);
void addReplyBool(client *c, int b);
void addReplyVerbatim(client *c, const char *s, size_t len, const char *ext);
void addReplyProto(client *c, const char *s, size_t len);
void addReplyFromClient(client *c, client *src);
void addReplyBulk(client *c, robj *obj);
void addReplyBulkCString(client *c, const char *s);
void addReplyBulkCBuffer(client *c, const void *p, size_t len);
void addReplyBulkLongLong(client *c, long long ll);
void addReply(client *c, robj *obj);
void addReplySds(client *c, sds s);
void addReplyBulkSds(client *c, sds s);
void setDeferredReplyBulkSds(client *c, void *node, sds s);
void addReplyErrorObject(client *c, robj *err);
void addReplyErrorSds(client *c, sds err);
void addReplyError(client *c, const char *err);
void addReplyStatus(client *c, const char *status);
void addReplyDouble(client *c, double d);
void addReplyHumanLongDouble(client *c, long double d);
void addReplyLongLong(client *c, long long ll);
void addReplyArrayLen(client *c, long length);
void addReplyMapLen(client *c, long length);
void addReplySetLen(client *c, long length);
void addReplyAttributeLen(client *c, long length);
void addReplyPushLen(client *c, long length);
void addReplyHelp(client *c, const char **help);
void addReplySubcommandSyntaxError(client *c);
void addReplyLoadedModules(client *c);

void copyClientOutputBuffer(client *dst, client *src);

size_t sdsZmallocSize(sds s);

size_t getStringObjectSdsUsedMemory(robj *o);

void freeClientReplyValue(void *o);

void *dupClientReplyValue(void *o);

void getClientsMaxBuffers(unsigned long *longest_output_list,
                          unsigned long *biggest_input_buffer);

char *getClientPeerId(client *client);

char *getClientSockName(client *client);

sds catClientInfoString(sds s, client *client);

sds getAllClientsInfoString(int type);

void rewriteClientCommandVector(client *c, int argc, ...);

void rewriteClientCommandArgument(client *c, int i, robj *newval);

void replaceClientCommandVector(client *c, int argc, robj **argv);

unsigned long getClientOutputBufferMemoryUsage(client *c);

int freeClientsInAsyncFreeQueue(void);

void asyncCloseClientOnOutputBufferLimitReached(client *c);

int getClientType(client *c);

int getClientTypeByName(char *name);

char *getClientTypeName(int class);

void flushSlavesOutputBuffers(void);

void disconnectSlaves(void);

int listenToPort(int port, int *fds, int *count);

void pauseClients(mstime_t duration, pause_type type);

void unpauseClients(void);

int areClientsPaused(void);

int checkClientPauseTimeoutAndReturnIfPaused(void);

void processEventsWhileBlocked(void);

void loadingCron(void);

void whileBlockedCron(void);

void blockingOperationStarts(void);

void blockingOperationEnds(void);

int handleClientsWithPendingWrites(void);

int handleClientsWithPendingWritesUsingThreads(void);

int handleClientsWithPendingReadsUsingThreads(void);

int stopThreadedIOIfNeeded(void);

int clientHasPendingReplies(client *c);

void unlinkClient(client *c);

int writeToClient(client *c, int handler_installed);

void linkClient(client *c);

void protectClient(client *c);

void unprotectClient(client *c);

void initThreadedIO(void);

client *lookupClientByID(uint64_t id);

#ifdef __GNUC__

void addReplyErrorFormat(client *c, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

void addReplyStatusFormat(client *c, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#else
void addReplyErrorFormat(client *c, const char *fmt, ...);
void addReplyStatusFormat(client *c, const char *fmt, ...);
#endif

/* Client side caching (tracking mode) */
// 客户端缓存（跟踪模式）
void enableTracking(client *c, uint64_t redirect_to, uint64_t options, robj **prefix, size_t numprefix);
void disableTracking(client *c);
void trackingRememberKeys(client *c);
void trackingInvalidateKey(client *c, robj *keyobj);
void trackingInvalidateKeysOnFlush(int async);
void freeTrackingRadixTreeAsync(rax *rt);
void trackingLimitUsedSlots(void);
uint64_t trackingGetTotalItems(void);
uint64_t trackingGetTotalKeys(void);
uint64_t trackingGetTotalPrefixes(void);
void trackingBroadcastInvalidationMessages(void);
int checkPrefixCollisionsOrReply(client *c, robj **prefix, size_t numprefix);

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

/* Redis object implementation */
// Redis 对象实现
void decrRefCount(robj *o);
void decrRefCountVoid(void *o);
void incrRefCount(robj *o);
robj *makeObjectShared(robj *o);
robj *resetRefCount(robj *obj);
void freeStringObject(robj *o);
void freeListObject(robj *o);
void freeSetObject(robj *o);
void freeZsetObject(robj *o);
void freeHashObject(robj *o);
robj *createObject(int type, void *ptr);
robj *createStringObject(const char *ptr, size_t len);
robj *createRawStringObject(const char *ptr, size_t len);
robj *createEmbeddedStringObject(const char *ptr, size_t len);
robj *dupStringObject(const robj *o);
int isSdsRepresentableAsLongLong(sds s, long long *llval);
int isObjectRepresentableAsLongLong(robj *o, long long *llongval);
robj *tryObjectEncoding(robj *o);
robj *getDecodedObject(robj *o);
size_t stringObjectLen(robj *o);
robj *createStringObjectFromLongLong(long long value);
robj *createStringObjectFromLongLongForValue(long long value);
robj *createStringObjectFromLongDouble(long double value, int humanfriendly);
robj *createQuicklistObject(void);
robj *createZiplistObject(void);
robj *createSetObject(void);
robj *createIntsetObject(void);
robj *createHashObject(void);
robj *createZsetObject(void);
robj *createZsetZiplistObject(void);
robj *createStreamObject(void);
robj *createModuleObject(moduleType *mt, void *value);
int getLongFromObjectOrReply(client *c, robj *o, long *target, const char *msg);
int getPositiveLongFromObjectOrReply(client *c, robj *o, long *target, const char *msg);
int getRangeLongFromObjectOrReply(client *c, robj *o, long min, long max, long *target, const char *msg);
int checkType(client *c, robj *o, int type);
int getLongLongFromObjectOrReply(client *c, robj *o, long long *target, const char *msg);
int getDoubleFromObjectOrReply(client *c, robj *o, double *target, const char *msg);
int getDoubleFromObject(const robj *o, double *target);
int getLongLongFromObject(robj *o, long long *target);
int getLongDoubleFromObject(robj *o, long double *target);
int getLongDoubleFromObjectOrReply(client *c, robj *o, long double *target, const char *msg);
int getIntFromObjectOrReply(client *c, robj *o, int *target, const char *msg);
char *strEncoding(int encoding);
int compareStringObjects(robj *a, robj *b);
int collateStringObjects(robj *a, robj *b);
int equalStringObjects(robj *a, robj *b);
unsigned long long estimateObjectIdleTime(robj *o);
void trimStringObjectIfNeeded(robj *o);

#define sdsEncodedObject(objptr) (objptr->encoding == OBJ_ENCODING_RAW || objptr->encoding == OBJ_ENCODING_EMBSTR)

/* Synchronous I/O with timeout */
// 带超时的同步 IO
ssize_t syncWrite(int fd, char *ptr, ssize_t size, long long timeout);
ssize_t syncRead(int fd, char *ptr, ssize_t size, long long timeout);
ssize_t syncReadLine(int fd, char *ptr, ssize_t size, long long timeout);

/* Replication */
// 复制
void replicationFeedSlaves(list *slaves, int dictid, robj **argv, int argc);
void replicationFeedSlavesFromMasterStream(list *slaves, char *buf, size_t buflen);
void replicationFeedMonitors(client *c, list *monitors, int dictid, robj **argv, int argc);
void updateSlavesWaitingBgsave(int bgsaveerr, int type);
void replicationCron(void);
void replicationStartPendingFork(void);
void replicationHandleMasterDisconnection(void);
void replicationCacheMaster(client *c);
void resizeReplicationBacklog(long long newsize);
void replicationSetMaster(char *ip, int port);
void replicationUnsetMaster(void);
void refreshGoodSlavesCount(void);
void replicationScriptCacheInit(void);
void replicationScriptCacheFlush(void);
void replicationScriptCacheAdd(sds sha1);
int replicationScriptCacheExists(sds sha1);
void processClientsWaitingReplicas(void);
void unblockClientWaitingReplicas(client *c);
int replicationCountAcksByOffset(long long offset);
void replicationSendNewlineToMaster(void);
long long replicationGetSlaveOffset(void);
char *replicationGetSlaveName(client *c);
long long getPsyncInitialOffset(void);
int replicationSetupSlaveForFullResync(client *slave, long long offset);
void changeReplicationId(void);
void clearReplicationId2(void);
void chopReplicationBacklog(void);
void replicationCacheMasterUsingMyself(void);
void feedReplicationBacklog(void *ptr, size_t len);
void showLatestBacklog(void);
void rdbPipeReadHandler(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
void rdbPipeWriteHandlerConnRemoved(struct connection *conn);
void clearFailoverState(void);
void updateFailoverStatus(void);
void abortFailover(const char *err);
const char *getFailoverStateString(void);

/* Generic persistence functions */
// 通用持久性函数
void startLoadingFile(FILE *fp, char *filename, int rdbflags);
void startLoading(size_t size, int rdbflags);
void loadingProgress(off_t pos);
void stopLoading(int success);
void startSaving(int rdbflags);
void stopSaving(int success);
int allPersistenceDisabled(void);

#define DISK_ERROR_TYPE_AOF 1  /* Don't accept writes: AOF errors. */
#define DISK_ERROR_TYPE_RDB 2  /* Don't accept writes: RDB errors. */
#define DISK_ERROR_TYPE_NONE 0 /* No problems, we can accept writes. */

int writeCommandsDeniedByDiskError(void);

/* RDB persistence */
// RDB 持久性
#include "rdb.h"
void killRDBChild(void);
int bg_unlink(const char *filename);

/* AOF persistence */
// AOF持久性
void flushAppendOnlyFile(int force);
void feedAppendOnlyFile(struct redisCommand *cmd, int dictid, robj **argv, int argc);
void aofRemoveTempFile(pid_t childpid);
int rewriteAppendOnlyFileBackground(void);
int loadAppendOnlyFile(char *filename);
void stopAppendOnly(void);
int startAppendOnly(void);
void backgroundRewriteDoneHandler(int exitcode, int bysignal);
void aofRewriteBufferReset(void);
unsigned long aofRewriteBufferSize(void);
ssize_t aofReadDiffFromParent(void);
void killAppendOnlyChild(void);
void restartAOFAfterSYNC(void);

/* Child info */
// child信息
void openChildInfoPipe(void);
void closeChildInfoPipe(void);
void sendChildInfo(int process_type, int on_exit, size_t cow_size);
void receiveChildInfo(void);

/* Fork helpers */
// fork助手
int redisFork(int type);
int hasActiveChildProcess(void);
void resetChildState(void);
int isMutuallyExclusiveChildType(int type);
void sendChildCOWInfo(int ptype, int on_exit, char *pname);

/* acl.c -- Authentication related prototypes. */
// acl.c -- 与身份验证相关的原型。
extern rax *Users;
extern user *DefaultUser;
void ACLInit(void);
/* Return values for ACLCheckCommandPerm() and ACLCheckPubsubPerm(). */
// 返回 ACLCheckCommandPerm（） 和 ACLCheckPubsubPerm（） 的值。
#define ACL_OK 0
#define ACL_DENIED_CMD 1
#define ACL_DENIED_KEY 2
#define ACL_DENIED_AUTH 3 /* Only used for ACL LOG entries. */    // 仅用于 ACL 日志条目。
#define ACL_DENIED_CHANNEL 4 /* Only used for pub/sub commands */ // 仅用于发布订阅命令

int ACLCheckUserCredentials(robj *username, robj *password);
int ACLAuthenticateUser(client *c, robj *username, robj *password);
unsigned long ACLGetCommandID(const char *cmdname);
void ACLClearCommandID(void);
user *ACLGetUserByName(const char *name, size_t namelen);
int ACLCheckCommandPerm(client *c, int *keyidxptr);
int ACLCheckPubsubPerm(client *c, int idx, int count, int literal, int *idxptr);
int ACLSetUser(user *u, const char *op, ssize_t oplen);
sds ACLDefaultUserFirstPassword(void);
uint64_t ACLGetCommandCategoryFlagByName(const char *name);
int ACLAppendUserForLoading(sds *argv, int argc, int *argc_err);
const char *ACLSetUserStringError(void);
int ACLLoadConfiguredUsers(void);
sds ACLDescribeUser(user *u);
void ACLLoadUsersAtStartup(void);
void addReplyCommandCategories(client *c, struct redisCommand *cmd);
user *ACLCreateUnlinkedUser(void);
void ACLFreeUserAndKillClients(user *u);
void addACLLogEntry(client *c, int reason, int keypos, sds username);

/* Sorted sets data type */
// 排序集数据类型

/* Input flags. */
#define ZADD_NONE 0
#define ZADD_INCR (1 << 0) /* Increment the score instead of setting it. */
#define ZADD_NX (1 << 1)   /* Don't touch elements not already existing. */
#define ZADD_XX (1 << 2)   /* Only touch elements already existing. */
#define ZADD_GT (1 << 7)   /* Only update existing when new scores are higher. */
#define ZADD_LT (1 << 8)   /* Only update existing when new scores are lower. */

/* Output flags. */
#define ZADD_NOP (1 << 3)     /* Operation not performed because of conditionals.*/
#define ZADD_NAN (1 << 4)     /* Only touch elements already existing. */
#define ZADD_ADDED (1 << 5)   /* The element was new and was added. */
#define ZADD_UPDATED (1 << 6) /* The element already existed, score updated. */

/* Flags only used by the ZADD command but not by zsetAdd() API: */
#define ZADD_CH (1 << 16) /* Return num of elements added or updated. */

/* Struct to hold an inclusive/exclusive range spec by score comparison. */
typedef struct
{
    double min, max;
    int minex, maxex; /* are min or max exclusive? */
} zrangespec;

/* Struct to hold an inclusive/exclusive range spec by lexicographic comparison. */
typedef struct
{
    sds min, max;     /* May be set to shared.(minstring|maxstring) */
    int minex, maxex; /* are min or max exclusive? */
} zlexrangespec;

zskiplist *zslCreate(void);

void zslFree(zskiplist *zsl);

zskiplistNode *zslInsert(zskiplist *zsl, double score, sds ele);

unsigned char *zzlInsert(unsigned char *zl, sds ele, double score);

int zslDelete(zskiplist *zsl, double score, sds ele, zskiplistNode **node);

zskiplistNode *zslFirstInRange(zskiplist *zsl, zrangespec *range);

zskiplistNode *zslLastInRange(zskiplist *zsl, zrangespec *range);

double zzlGetScore(unsigned char *sptr);

void zzlNext(unsigned char *zl, unsigned char **eptr, unsigned char **sptr);

void zzlPrev(unsigned char *zl, unsigned char **eptr, unsigned char **sptr);

unsigned char *zzlFirstInRange(unsigned char *zl, zrangespec *range);

unsigned char *zzlLastInRange(unsigned char *zl, zrangespec *range);

unsigned long zsetLength(const robj *zobj);

void zsetConvert(robj *zobj, int encoding);

void zsetConvertToZiplistIfNeeded(robj *zobj, size_t maxelelen);

int zsetScore(robj *zobj, sds member, double *score);

unsigned long zslGetRank(zskiplist *zsl, double score, sds o);

int zsetAdd(robj *zobj, double score, sds ele, int *flags, double *newscore);

long zsetRank(robj *zobj, sds ele, int reverse);

int zsetDel(robj *zobj, sds ele);

robj *zsetDup(robj *o);

int zsetZiplistValidateIntegrity(unsigned char *zl, size_t size, int deep);

void genericZpopCommand(client *c, robj **keyv, int keyc, int where, int emitkey, robj *countarg);

sds ziplistGetObject(unsigned char *sptr);

int zslValueGteMin(double value, zrangespec *spec);

int zslValueLteMax(double value, zrangespec *spec);

void zslFreeLexRange(zlexrangespec *spec);

int zslParseLexRange(robj *min, robj *max, zlexrangespec *spec);

unsigned char *zzlFirstInLexRange(unsigned char *zl, zlexrangespec *range);

unsigned char *zzlLastInLexRange(unsigned char *zl, zlexrangespec *range);

zskiplistNode *zslFirstInLexRange(zskiplist *zsl, zlexrangespec *range);

zskiplistNode *zslLastInLexRange(zskiplist *zsl, zlexrangespec *range);

int zzlLexValueGteMin(unsigned char *p, zlexrangespec *spec);

int zzlLexValueLteMax(unsigned char *p, zlexrangespec *spec);

int zslLexValueGteMin(sds value, zlexrangespec *spec);

int zslLexValueLteMax(sds value, zlexrangespec *spec);

/* Core functions */
// 核心函数
// 该函数 processCommand 接收一个 client 类型的指针参数 c，用于处理客户端命令。具体功能包括解析命令、执行相应操作并返回处理结果。由于代码逻辑较为复杂，具体实现细节需要查看函数内部。
int processCommand(client *c);
int processPendingCommandsAndResetClient(client *c);
void setupSignalHandlers(void);
void removeSignalHandlers(void);
struct redisCommand *lookupCommand(sds name);
struct redisCommand *lookupCommandByCString(const char *s);
struct redisCommand *lookupCommandOrOriginal(sds name);
void call(client *c, int flags); // 执行命令
void propagate(struct redisCommand *cmd, int dbid, robj **argv, int argc, int flags);
void alsoPropagate(struct redisCommand *cmd, int dbid, robj **argv, int argc, int target);
void redisOpArrayInit(redisOpArray *oa);
void redisOpArrayFree(redisOpArray *oa);
void forceCommandPropagation(client *c, int flags);
void preventCommandPropagation(client *c);
void preventCommandAOF(client *c);
void preventCommandReplication(client *c);
int prepareForShutdown(int flags);

#ifdef __GNUC__
void serverLog(int level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
void serverLog(int level, const char *fmt, ...);
#endif
void serverLogRaw(int level, const char *msg);
void serverLogFromHandler(int level, const char *msg);
// 创建Pid文件
void createPidFile(void);
void daemonize(void);
// 打印启动日志
void printStartLog(int argc, char **argv);
// 打印版本
void version(void);
// 打印帮助
void usage(void);
// 打印Redis ASCII艺术Logo
void redisAsciiArt(void);
void updateDictResizePolicy(void);
int htNeedsResize(dict *dict);
void populateCommandTable(void);

// 重置服务统计
void resetServerStats(void);
// 重置命令统计
void resetCommandTableStats(void);
// 重置错误统计
void resetErrorTableStats(void);
void adjustOpenFilesLimit(void);

void incrementErrorCount(const char *fullerr, size_t namelen);
void closeListeningSockets(int unlink_unix_socket);
void updateCachedTime(int update_daylight_info);

void activeDefragCycle(void);

unsigned int getLRUClock(void);

unsigned int LRU_CLOCK(void);

const char *evictPolicyToString(void);

struct redisMemOverhead *getMemoryOverheadData(void);

void freeMemoryOverheadData(struct redisMemOverhead *mh);

void checkChildrenDone(void);

int setOOMScoreAdj(int process_class);

void rejectCommandFormat(client *c, const char *fmt, ...);

void *activeDefragAlloc(void *ptr);

robj *activeDefragStringOb(robj *ob, long *defragged);

#define RESTART_SERVER_NONE 0
#define RESTART_SERVER_GRACEFULLY (1 << 0)     /* Do proper shutdown. */
#define RESTART_SERVER_CONFIG_REWRITE (1 << 1) /* CONFIG REWRITE before restart.*/

int restartServer(int flags, mstime_t delay);

/* Set data type */
// Set数据类型
robj *setTypeCreate(sds value);
int setTypeAdd(robj *subject, sds value);
int setTypeRemove(robj *subject, sds value);
int setTypeIsMember(robj *subject, sds value);
setTypeIterator *setTypeInitIterator(robj *subject);
void setTypeReleaseIterator(setTypeIterator *si);
int setTypeNext(setTypeIterator *si, sds *sdsele, int64_t *llele);
sds setTypeNextObject(setTypeIterator *si);
int setTypeRandomElement(robj *setobj, sds *sdsele, int64_t *llele);
unsigned long setTypeRandomElements(robj *set, unsigned long count, robj *aux_set);
unsigned long setTypeSize(const robj *subject);
void setTypeConvert(robj *subject, int enc);
robj *setTypeDup(robj *o);

/* Hash data type */
// Hash数据类型
#define HASH_SET_TAKE_FIELD (1 << 0)
#define HASH_SET_TAKE_VALUE (1 << 1)
#define HASH_SET_COPY 0

void hashTypeConvert(robj *o, int enc);
void hashTypeTryConversion(robj *subject, robj **argv, int start, int end);
int hashTypeExists(robj *o, sds key);
int hashTypeDelete(robj *o, sds key);
unsigned long hashTypeLength(const robj *o);
hashTypeIterator *hashTypeInitIterator(robj *subject);
void hashTypeReleaseIterator(hashTypeIterator *hi);
int hashTypeNext(hashTypeIterator *hi);
void hashTypeCurrentFromZiplist(hashTypeIterator *hi, int what,
                                unsigned char **vstr,
                                unsigned int *vlen,
                                long long *vll);
sds hashTypeCurrentFromHashTable(hashTypeIterator *hi, int what);
void hashTypeCurrentObject(hashTypeIterator *hi, int what, unsigned char **vstr, unsigned int *vlen, long long *vll);
sds hashTypeCurrentObjectNewSds(hashTypeIterator *hi, int what);
robj *hashTypeLookupWriteOrCreate(client *c, robj *key);
robj *hashTypeGetValueObject(robj *o, sds field);
int hashTypeSet(robj *o, sds field, sds value, int flags);
robj *hashTypeDup(robj *o);
int hashZiplistValidateIntegrity(unsigned char *zl, size_t size, int deep);

/* Pub / Sub */
// 发布/订阅
int pubsubUnsubscribeAllChannels(client *c, int notify);
int pubsubUnsubscribeAllPatterns(client *c, int notify);
void freePubsubPattern(void *p);
int listMatchPubsubPattern(void *a, void *b);
int pubsubPublishMessage(robj *channel, robj *message);
void addReplyPubsubMessage(client *c, robj *channel, robj *msg);

/* Keyspace events notification */
// Keyspace事件通知
void notifyKeyspaceEvent(int type, char *event, robj *key, int dbid);
int keyspaceEventsStringToFlags(char *classes);
sds keyspaceEventsFlagsToString(int flags);

/* Configuration */
// 配置
// 加载服务配置
void loadServerConfig(char *filename, char config_from_stdin, char *options);

// 追加服务器保存参数
void appendServerSaveParams(time_t seconds, int changes);

// 重置服务器保存参数
void resetServerSaveParams(void);

struct rewriteConfigState; /* Forward declaration to export API. */
void rewriteConfigRewriteLine(struct rewriteConfigState *state, const char *option, sds line, int force);

void rewriteConfigMarkAsProcessed(struct rewriteConfigState *state, const char *option);

int rewriteConfig(char *path, int force_all);

void initConfigValues(void);

/* db.c -- Keyspace access API 键访问API */
// 删除过期
int removeExpire(redisDb *db, robj *key);
void propagateExpire(redisDb *db, robj *key, int lazy);
int expireIfNeeded(redisDb *db, robj *key);
// 获取过期时间
long long getExpire(redisDb *db, robj *key);
// 设置过期时间
void setExpire(client *c, redisDb *db, robj *key, long long when);
// 检查是否已经过期
int checkAlreadyExpired(long long when);
robj *lookupKey(redisDb *db, robj *key, int flags);
robj *lookupKeyRead(redisDb *db, robj *key);
robj *lookupKeyWrite(redisDb *db, robj *key);
robj *lookupKeyReadOrReply(client *c, robj *key, robj *reply);
robj *lookupKeyWriteOrReply(client *c, robj *key, robj *reply);
robj *lookupKeyReadWithFlags(redisDb *db, robj *key, int flags);
robj *lookupKeyWriteWithFlags(redisDb *db, robj *key, int flags);
robj *objectCommandLookup(client *c, robj *key);
robj *objectCommandLookupOrReply(client *c, robj *key, robj *reply);
int objectSetLRUOrLFU(robj *val, long long lfu_freq, long long lru_idle,
                      long long lru_clock, int lru_multiplier);
#define LOOKUP_NONE 0
#define LOOKUP_NOTOUCH (1 << 0)
#define LOOKUP_NONOTIFY (1 << 1)
void dbAdd(redisDb *db, robj *key, robj *val);
int dbAddRDBLoad(redisDb *db, sds key, robj *val);
void dbOverwrite(redisDb *db, robj *key, robj *val);
void genericSetKey(client *c, redisDb *db, robj *key, robj *val, int keepttl, int signal);
void setKey(client *c, redisDb *db, robj *key, robj *val);
robj *dbRandomKey(redisDb *db);
int dbSyncDelete(redisDb *db, robj *key);
int dbDelete(redisDb *db, robj *key);
robj *dbUnshareStringValue(redisDb *db, robj *key, robj *o);

#define EMPTYDB_NO_FLAGS 0     /* No flags. */
#define EMPTYDB_ASYNC (1 << 0) /* Reclaim memory in another thread. */

long long emptyDb(int dbnum, int flags, void(callback)(void *));
long long emptyDbStructure(redisDb *dbarray, int dbnum, int async, void(callback)(void *));
void flushAllDataAndResetRDB(int flags);
long long dbTotalServerKeyCount(void);
dbBackup *backupDb(void);
void restoreDbBackup(dbBackup *buckup);
void discardDbBackup(dbBackup *buckup, int flags, void(callback)(void *));
int selectDb(client *c, int id);
void signalModifiedKey(client *c, redisDb *db, robj *key);
void signalFlushedDb(int dbid, int async);
unsigned int getKeysInSlot(unsigned int hashslot, robj **keys, unsigned int count);
unsigned int countKeysInSlot(unsigned int hashslot);
unsigned int delKeysInSlot(unsigned int hashslot);
int verifyClusterConfigWithData(void);
void scanGenericCommand(client *c, robj *o, unsigned long cursor);
int parseScanCursorOrReply(client *c, robj *o, unsigned long *cursor);
void slotToKeyAdd(sds key);
void slotToKeyDel(sds key);
int dbAsyncDelete(redisDb *db, robj *key);
void emptyDbAsync(redisDb *db);
void slotToKeyFlush(int async);
size_t lazyfreeGetPendingObjectsCount(void);
size_t lazyfreeGetFreedObjectsCount(void);
void freeObjAsync(robj *key, robj *obj);
void freeSlotsToKeysMapAsync(rax *rt);
void freeSlotsToKeysMap(rax *rt, int async);
/* API to get key arguments from commands */
int *getKeysPrepareResult(getKeysResult *result, int numkeys);
int getKeysFromCommand(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
void getKeysFreeResult(getKeysResult *result);
int zunionInterDiffGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int zunionInterDiffStoreGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int evalGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int sortGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int migrateGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int georadiusGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int xreadGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int memoryGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);
int lcsGetKeys(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);

/* Cluster */
// 集群
void clusterInit(void);
unsigned short crc16(const char *buf, int len);
unsigned int keyHashSlot(char *key, int keylen);
void clusterCron(void);
void clusterPropagatePublish(robj *channel, robj *message);
void migrateCloseTimedoutSockets(void);
void clusterBeforeSleep(void);
int clusterSendModuleMessageToTarget(const char *target, uint64_t module_id, uint8_t type, unsigned char *payload,
                                     uint32_t len);

/* Sentinel */
// 哨兵
void initSentinelConfig(void);
void initSentinel(void);
void sentinelTimer(void);
const char *sentinelHandleConfiguration(char **argv, int argc);
void queueSentinelConfig(sds *argv, int argc, int linenum, sds line);
void loadSentinelConfigFromQueue(void);
void sentinelIsRunning(void);

/* redis-check-rdb & aof */
int redis_check_rdb(char *rdbfilename, FILE *fp);
int redis_check_rdb_main(int argc, char **argv, FILE *fp);
int redis_check_aof_main(int argc, char **argv);

/* Scripting */
// 脚本
void scriptingInit(int setup);
int ldbRemoveChild(pid_t pid);
void ldbKillForkedSessions(void);
int ldbPendingChildren(void);
sds luaCreateFunction(client *c, lua_State *lua, robj *body);
void freeLuaScriptsAsync(dict *lua_scripts);

/* Blocked clients */
// 被阻止的客户端
void processUnblockedClients(void);
void blockClient(client *c, int btype);
void unblockClient(client *c);
void queueClientForReprocessing(client *c);
void replyToBlockedClientTimedOut(client *c);
int getTimeoutFromObjectOrReply(client *c, robj *object, mstime_t *timeout, int unit);
void disconnectAllBlockedClients(void);
void handleClientsBlockedOnKeys(void);
void signalKeyAsReady(redisDb *db, robj *key, int type);
void blockForKeys(client *c, int btype, robj **keys, int numkeys, mstime_t timeout, robj *target, struct listPos *listpos,
                  streamID *ids);
void updateStatsOnUnblock(client *c, long blocked_us, long reply_us);

/* timeout.c -- Blocked clients timeout and connections timeout. */
// timeout.c -- 阻止的客户端超时和连接超时。
void addClientToTimeoutTable(client *c);      // 阻止的客户端超时和连接超时
void removeClientFromTimeoutTable(client *c); // 当客户端因超时以外的原因而取消阻止时，将其从表中删除。
void handleBlockedClientsTimeout(void);
int clientsCronHandleTimeout(client *c, mstime_t now_ms);

/* expire.c -- Handling of expired keys */
// expire.c -- 过期密钥的处理
void activeExpireCycle(int type);
void expireSlaveKeys(void);
void rememberSlaveKeyWithExpire(redisDb *db, robj *key);
void flushSlaveKeysWithExpireList(void);
size_t getSlaveKeyWithExpireCount(void);

/* evict.c -- maxmemory handling and LRU eviction. */
// 最大内存处理和 LRU 逐出。
void evictionPoolAlloc(void);

#define LFU_INIT_VAL 5

unsigned long LFUGetTimeInMinutes(void);

uint8_t LFULogIncr(uint8_t value);

unsigned long LFUDecrAndReturn(robj *o);

#define EVICT_OK 0
#define EVICT_RUNNING 1
#define EVICT_FAIL 2

int performEvictions(void);

/* Keys hashing / comparison functions for dict.c hash tables. */
// dict.c 哈希表的键哈希比较函数。。
uint64_t dictSdsHash(const void *key);
int dictSdsKeyCompare(void *privdata, const void *key1, const void *key2);
void dictSdsDestructor(void *privdata, void *val);

/* Git SHA1 */
char *redisGitSHA1(void);
char *redisGitDirty(void);
uint64_t redisBuildId(void);
char *redisBuildIdString(void);

/* Commands prototypes */
// 命令原型
void authCommand(client *c);    // auth命令
void pingCommand(client *c);    // ping命令
void echoCommand(client *c);    // echo命令
void commandCommand(client *c); // command命令
void setCommand(client *c);     // set命令
void setnxCommand(client *c);   // setnx命令
void setexCommand(client *c);
void psetexCommand(client *c);
void getCommand(client *c);
void getexCommand(client *c);
void getdelCommand(client *c);
void delCommand(client *c);
void unlinkCommand(client *c);
void existsCommand(client *c);
void setbitCommand(client *c);
void getbitCommand(client *c);
void bitfieldCommand(client *c);
void bitfieldroCommand(client *c);
void setrangeCommand(client *c);
void getrangeCommand(client *c);
void incrCommand(client *c);
void decrCommand(client *c);
void incrbyCommand(client *c);
void decrbyCommand(client *c);
void incrbyfloatCommand(client *c);
void selectCommand(client *c);
void swapdbCommand(client *c);
void randomkeyCommand(client *c);
void keysCommand(client *c);
void scanCommand(client *c);
void dbsizeCommand(client *c);
void lastsaveCommand(client *c);
/** save命令 */
void saveCommand(client *c);
/** bgsave命令 */
void bgsaveCommand(client *c);
void bgrewriteaofCommand(client *c);
void shutdownCommand(client *c);
void moveCommand(client *c);
void copyCommand(client *c);
void renameCommand(client *c);
void renamenxCommand(client *c);
void lpushCommand(client *c);
void rpushCommand(client *c);
void lpushxCommand(client *c);
void rpushxCommand(client *c);
void linsertCommand(client *c);
void lpopCommand(client *c);
void rpopCommand(client *c);
void llenCommand(client *c);
void lindexCommand(client *c);
void lrangeCommand(client *c);
void ltrimCommand(client *c);
void typeCommand(client *c);
void lsetCommand(client *c);
void saddCommand(client *c);
void sremCommand(client *c);
void smoveCommand(client *c);
void sismemberCommand(client *c);
void smismemberCommand(client *c);
void scardCommand(client *c);
void spopCommand(client *c);
void srandmemberCommand(client *c);
void sinterCommand(client *c);
void sinterstoreCommand(client *c);
void sunionCommand(client *c);
void sunionstoreCommand(client *c);
void sdiffCommand(client *c);
void sdiffstoreCommand(client *c);
void sscanCommand(client *c);
void syncCommand(client *c);
void flushdbCommand(client *c);
void flushallCommand(client *c);
void sortCommand(client *c);
void lremCommand(client *c);
void lposCommand(client *c);
void rpoplpushCommand(client *c);
void lmoveCommand(client *c);
void infoCommand(client *c);
void mgetCommand(client *c);
void monitorCommand(client *c);
void expireCommand(client *c);
void expireatCommand(client *c);
void pexpireCommand(client *c);
void pexpireatCommand(client *c);
void getsetCommand(client *c);
void ttlCommand(client *c);
void touchCommand(client *c);
void pttlCommand(client *c);
void persistCommand(client *c);
void replicaofCommand(client *c);
void roleCommand(client *c);
void debugCommand(client *c);
void msetCommand(client *c);
void msetnxCommand(client *c);
void zaddCommand(client *c);
void zincrbyCommand(client *c);
void zrangeCommand(client *c);
void zrangebyscoreCommand(client *c);
void zrevrangebyscoreCommand(client *c);
void zrangebylexCommand(client *c);
void zrevrangebylexCommand(client *c);
void zcountCommand(client *c);
void zlexcountCommand(client *c);
void zrevrangeCommand(client *c);
void zcardCommand(client *c);
void zremCommand(client *c);
void zscoreCommand(client *c);
void zmscoreCommand(client *c);
void zremrangebyscoreCommand(client *c);
void zremrangebylexCommand(client *c);
void zpopminCommand(client *c);
void zpopmaxCommand(client *c);
void bzpopminCommand(client *c);
void bzpopmaxCommand(client *c);
void zrandmemberCommand(client *c);
void multiCommand(client *c);
void execCommand(client *c);
void discardCommand(client *c);
void blpopCommand(client *c);
void brpopCommand(client *c);
void brpoplpushCommand(client *c);
void blmoveCommand(client *c);
void appendCommand(client *c);
void strlenCommand(client *c);
void zrankCommand(client *c);
void zrevrankCommand(client *c);
void hsetCommand(client *c);
void hsetnxCommand(client *c);
void hgetCommand(client *c);
void hmsetCommand(client *c);
void hmgetCommand(client *c);
void hdelCommand(client *c);
void hlenCommand(client *c);
void hstrlenCommand(client *c);
void zremrangebyrankCommand(client *c);
void zunionstoreCommand(client *c);
void zinterstoreCommand(client *c);
void zdiffstoreCommand(client *c);
void zunionCommand(client *c);
void zinterCommand(client *c);
void zrangestoreCommand(client *c);
void zdiffCommand(client *c);
void zscanCommand(client *c);
void hkeysCommand(client *c);
void hvalsCommand(client *c);
void hgetallCommand(client *c);
void hexistsCommand(client *c);
void hscanCommand(client *c);
void hrandfieldCommand(client *c);
void configCommand(client *c);
void hincrbyCommand(client *c);
void hincrbyfloatCommand(client *c);

// 发布订阅相关
void subscribeCommand(client *c);    // subscribe命令
void unsubscribeCommand(client *c);  // unsubscribe命令
void psubscribeCommand(client *c);   // psubscribe命令
void punsubscribeCommand(client *c); // punsubscribe命令
void publishCommand(client *c);      // publish命令
void pubsubCommand(client *c);       // pubsub命令

void watchCommand(client *c);
void unwatchCommand(client *c);

void clusterCommand(client *c);

void restoreCommand(client *c);

void migrateCommand(client *c);

void askingCommand(client *c);

void readonlyCommand(client *c);

void readwriteCommand(client *c);

void dumpCommand(client *c);

void objectCommand(client *c);

void memoryCommand(client *c);

void clientCommand(client *c);

void helloCommand(client *c);

void evalCommand(client *c);

void evalShaCommand(client *c);

void scriptCommand(client *c);

void timeCommand(client *c);

void bitopCommand(client *c);

void bitcountCommand(client *c);

void bitposCommand(client *c);

void replconfCommand(client *c);

void waitCommand(client *c);

void geoencodeCommand(client *c);

void geodecodeCommand(client *c);

void georadiusbymemberCommand(client *c);

void georadiusbymemberroCommand(client *c);

void georadiusCommand(client *c);

void georadiusroCommand(client *c);

void geoaddCommand(client *c);

void geohashCommand(client *c);

void geoposCommand(client *c);

void geodistCommand(client *c);

void geosearchCommand(client *c);

void geosearchstoreCommand(client *c);

void pfselftestCommand(client *c);

void pfaddCommand(client *c);

void pfcountCommand(client *c);

void pfmergeCommand(client *c);

void pfdebugCommand(client *c);

void latencyCommand(client *c);

void moduleCommand(client *c);

void securityWarningCommand(client *c);

void xaddCommand(client *c);

void xrangeCommand(client *c);

void xrevrangeCommand(client *c);

void xlenCommand(client *c);

void xreadCommand(client *c);

void xgroupCommand(client *c);

void xsetidCommand(client *c);

void xackCommand(client *c);

void xpendingCommand(client *c);

void xclaimCommand(client *c);

void xautoclaimCommand(client *c);

void xinfoCommand(client *c);

void xdelCommand(client *c);

void xtrimCommand(client *c);

void lolwutCommand(client *c);

void aclCommand(client *c);

void stralgoCommand(client *c);

void resetCommand(client *c);

void failoverCommand(client *c);

#if defined(__GNUC__)
void *calloc(size_t count, size_t size) __attribute__((deprecated));
void free(void *ptr) __attribute__((deprecated));
void *malloc(size_t size) __attribute__((deprecated));
void *realloc(void *ptr, size_t size) __attribute__((deprecated));
#endif

/* Debugging stuff */
void _serverAssertWithInfo(const client *c, const robj *o, const char *estr, const char *file, int line);
void _serverAssert(const char *estr, const char *file, int line);

#ifdef __GNUC__
void _serverPanic(const char *file, int line, const char *msg, ...)
    __attribute__((format(printf, 3, 4)));
#else
void _serverPanic(const char *file, int line, const char *msg, ...);
#endif

void serverLogObjectDebugInfo(const robj *o);

void sigsegvHandler(int sig, siginfo_t *info, void *secret);

sds genRedisInfoString(const char *section);

sds genModulesInfoString(sds info);

void enableWatchdog(int period);

void disableWatchdog(void);

void watchdogScheduleSignal(int period);

void serverLogHexDump(int level, char *descr, void *value, size_t len);

int memtest_preserving_test(unsigned long *m, size_t bytes, int passes);

void mixDigest(unsigned char *digest, void *ptr, size_t len);

void xorDigest(unsigned char *digest, void *ptr, size_t len);

int populateCommandTableParseFlags(struct redisCommand *c, char *strflags);

void debugDelay(int usec);

void killIOThreads(void);

void killThreads(void);

void makeThreadKillable(void);

/* TLS stuff */
void tlsInit(void);

int tlsConfigure(redisTLSContextConfig *ctx_config);

#define redisDebug(fmt, ...) \
    printf("DEBUG %s:%d > " fmt "\n", __FILE__, __LINE__, __VA_ARGS__)
#define redisDebugMark() \
    printf("-- MARK %s:%d --\n", __FILE__, __LINE__)

int iAmMaster(void);

#endif
