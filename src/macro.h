#ifndef MACRO_H
#define MACRO_H

/* Anti-warning macro... */
// 防预警宏
#define UNUSED(V) ((void)V)

/* Error codes */
// 错误代码
#define C_OK 0
#define C_ERR -1

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

/* OOM Score Adjustment classes. */
// OOM 分数调整类。
#define CONFIG_OOM_MASTER 0
#define CONFIG_OOM_REPLICA 1
#define CONFIG_OOM_BGCHILD 2
#define CONFIG_OOM_COUNT 3

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

/* Hash table parameters */
// 哈希表参数
#define HASHTABLE_MIN_FILL 10 /* Minimal hash table fill 10% */               // 最小哈希表填充 10%
#define HASHTABLE_MAX_LOAD_FACTOR 1.618 /* Maximum hash table load factor. */ // 最大哈希表负载因子。

/* Static server configuration */
// 静态服务器配置
// 这段代码定义了三个宏，用于配置系统的时间中断频率。
// CONFIG_DEFAULT_HZ 设置默认时间为每秒 10 次中断，CONFIG_MIN_HZ 和 CONFIG_MAX_HZ 分别设置最小和最大时间中断频率为每秒 1 次和 500 次。
#define CONFIG_DEFAULT_HZ 10 /* Time interrupt calls/sec. */
#define CONFIG_MIN_HZ 1
#define CONFIG_MAX_HZ 500
#define MAX_CLIENTS_PER_CLOCK_TICK 200 /* HZ is adapted based on that. */
#define CONFIG_MAX_LINE 1024
#define CRON_DBS_PER_CALL 16
#define NET_MAX_WRITES_PER_EVENT (1024 * 64)
#define PROTO_SHARED_SELECT_CMDS 10
// 共享整数对象的数量上限
#define OBJ_SHARED_INTEGERS 10000
// 共享对象的批量头部长度
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

/* Version macro 版本宏*/
typedef long long mstime_t; /* millisecond time type. */ // 毫秒时间类型
typedef long long ustime_t; /* microsecond time type. */ // 微秒时间类型

#define REDISMODULE_CORE 1 // 这个宏通常用于标识代码是否为核心模块的一部分，以便在编译时进行条件编译。

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

/* Command propagation flags, see propagate() function */
// 命令传播标志，请参阅 propagate（） 函数
#define PROPAGATE_NONE 0
#define PROPAGATE_AOF 1
#define PROPAGATE_REPL 2

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

/* Objects encoding. Some kind of objects like Strings and Hashes can be
 * internally represented in multiple ways. The 'encoding' field of the object
 * is set to one of this fields for this object. */
// 对象编码。某些类型的对象（如字符串和哈希）可以在内部以多种方式表示。对象的"编码"字段设置为此对象的此字段之一。
/*
 * 编码方式，如果说每个类型只有一种方式，那么其实type和encoding两个字段只需要保留一个即可，
 * 但redis为了在各种情况下尽可能节约内存，对每种类型的数据在不同情况下有不同的编码格式，
 * 所以这里需要用额外的字段标识出来。
 */
#define OBJ_ENCODING_RAW 0 /* Raw representation */                        // 最原始的标识方式，只有string才会用到
#define OBJ_ENCODING_INT 1 /* Encoded as integer */                        // 整数
#define OBJ_ENCODING_HT 2 /* Encoded as hash table */                      // 哈希表
#define OBJ_ENCODING_ZIPMAP 3 /* Encoded as zipmap */                      // ZIPMAP
#define OBJ_ENCODING_LINKEDLIST 4 /* No longer used: old list encoding. */ // LINKEDLIST 不再使用
#define OBJ_ENCODING_ZIPLIST 5 /* Encoded as ziplist */                    // 压缩列表
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
#define OBJ_SHARED_REFCOUNT INT_MAX /* Global object never destroyed. */
// OBJ_STATIC_REFCOUNT 表示栈上分配的对象的引用计数，值为 INT_MAX - 1
#define OBJ_STATIC_REFCOUNT (INT_MAX - 1) /* Object allocated in the stack. */
// OBJ_FIRST_SPECIAL_REFCOUNT 是 OBJ_STATIC_REFCOUNT 的别名。
#define OBJ_FIRST_SPECIAL_REFCOUNT OBJ_STATIC_REFCOUNT

// 该代码定义了一个宏 sdsEncodedObject，用于判断对象指针 objptr 的编码是否为 OBJ_ENCODING_RAW 或 OBJ_ENCODING_EMBSTR。
#define sdsEncodedObject(objptr) (objptr->encoding == OBJ_ENCODING_RAW || objptr->encoding == OBJ_ENCODING_EMBSTR)

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

/* With multiplexing we need to take per-client state.
 * Clients are taken in a linked list. */
// 使用多路复用，我们需要采用每个客户端的状态。客户端在链表中获取。
#define CLIENT_ID_AOF (UINT64_MAX) /* Reserved ID for the AOF client. If you \
need more reserved IDs use UINT64_MAX-1,                                     \
-2, ... and so forth. */

#define redisDebug(fmt, ...) \
    printf("DEBUG %s:%d > " fmt "\n", __FILE__, __LINE__, __VA_ARGS__)
#define redisDebugMark() \
    printf("-- MARK %s:%d --\n", __FILE__, __LINE__)

#define RDB_SAVE_INFO_INIT \
    {                      \
        -1, 0, "0000000000000000000000000000000000000000", -1}

#define CHILD_TYPE_NONE 0
#define CHILD_TYPE_RDB 1
#define CHILD_TYPE_AOF 2
#define CHILD_TYPE_LDB 3
#define CHILD_TYPE_MODULE 4

#endif
