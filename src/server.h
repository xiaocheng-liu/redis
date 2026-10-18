/* Redis 服务端核心类型、全局状态及跨模块接口声明。 */
#ifndef SERVER_H
#define SERVER_H

#include <lua.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "atomicvar.h"
#include "config.h"
#include "macro.h"
#include "rio.h"
#include "solarisfixes.h"

#include "anet.h" /* Networking the easy way */ // 网络编程

#include "adlist.h" /* Linked lists  */ // 双向链表
#include "dict.h" /* Hash tables */ // 哈希表
#include "rax.h" /* Radix tree */ // 基数树
#include "sds.h" /* Dynamic safe strings */ // 二进制安全的动态字符串
#include "t_list.h" /* List data type header file. */ // 链表数据类型头文件
#include "t_stream.h" /* Stream data type header file. */ // 流数据类型头文件。
#include "zmalloc.h" /* total memory usage aware version of malloc/free */ // 带内存用量统计的分配与释放接口

#include "connection.h" /* Connection abstraction */ // TCP/TLS 连接抽象
#include "redismodule.h" /* Redis modules API defines. */ // Redis 模块开发接口

#include "acl.h"
#include "multi.h"
#include "rdb.h"
#include "tls.h"


/* Slave replication state. Used in server.repl_state for slaves to remember
 * what to do next. */
// 从节点连接主节点的状态机,保存在 server.repl_state 中。
typedef enum {
    REPL_STATE_NONE = 0,
    /* No active replication */ // 当前未作为从节点进行复制。
    REPL_STATE_CONNECT,
    /* Must connect to master */ // 等待发起到主节点的连接。
    REPL_STATE_CONNECTING,
    /* Connecting to master */ // 正在异步连接主节点,尚未进入握手阶段。
    /* --- Handshake states, must be ordered --- */
    // 握手状态必须保持以下顺序,状态范围判断依赖其连续性。
    REPL_STATE_RECEIVE_PING_REPLY,
    /* Wait for PING reply */ // 等待PING的回复
    REPL_STATE_SEND_HANDSHAKE,
    /* Send handshake sequance to master */ // 向主节点发送认证和 REPLCONF 握手命令。
    REPL_STATE_RECEIVE_AUTH_REPLY,
    /* Wait for AUTH reply */ // 等待身份验证回复
    REPL_STATE_RECEIVE_PORT_REPLY,
    /* Wait for REPLCONF reply */ // 等待 REPLCONF listening-port 的回复。
    REPL_STATE_RECEIVE_IP_REPLY,
    /* Wait for REPLCONF reply */ // 等待 REPLCONF ip-address 的回复。
    REPL_STATE_RECEIVE_CAPA_REPLY,
    /* Wait for REPLCONF reply */ // 等待 REPLCONF capa 的回复。
    REPL_STATE_SEND_PSYNC,
    /* Send PSYNC */ // 发送 PSYNC,尝试部分重同步。
    REPL_STATE_RECEIVE_PSYNC_REPLY,
    /* Wait for PSYNC reply */ // 等待 PSYNC 回复
    /* --- End of handshake states --- */
    REPL_STATE_TRANSFER,
    /* Receiving .rdb from master */ // 正在接收RDB文件
    REPL_STATE_CONNECTED,
    /* Connected to master */ // 同步已完成,持续接收主节点的复制命令流;也可由部分重同步进入。

} repl_state;

/* The state of an in progress coordinated failover */
// 正在进行的协调故障转移的状态
typedef enum {
    NO_FAILOVER = 0,
    /* No failover in progress */ // 没有正在进行的故障转移
    FAILOVER_WAIT_FOR_SYNC,
    /* Waiting for target replica to catch up */ // 等待目标从节点追平复制偏移量。
    FAILOVER_IN_PROGRESS /* Waiting for target replica to accept
                          * PSYNC FAILOVER request. */
    // 等待目标从节点接受 PSYNC FAILOVER 请求。
} failover_state;

/* Client pause types, larger types are more restrictive
 * pause types than smaller pause types. */
// 客户端命令暂停级别,枚举值越大,限制范围越广。
typedef enum {
    CLIENT_PAUSE_OFF = 0,
    /* Pause no commands */ // 不暂停任何命令
    CLIENT_PAUSE_WRITE,
    /* Pause write commands */ // 暂停写入命令
    CLIENT_PAUSE_ALL /* Pause all commands */ // 暂停所有命令
} pause_type;

struct RedisModule;
struct RedisModuleIO;
struct RedisModuleDigest;
struct RedisModuleCtx;
struct redisObject;
struct RedisModuleDefragCtx;
typedef struct streamID streamID;

/* Each module type implementation should export a set of methods in order
 * to serialize and deserialize the value in the RDB file, rewrite the AOF
 * log, create the digest for "DEBUG DIGEST", and free the value when a key
 * is deleted. */
// 模块自定义类型的回调:持久化、摘要计算、内存统计、复制及资源释放。
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
// 经模块认证的客户端切换用户或断开连接时,由 moduleNotifyUserChanged() 调用。
// 显式声明函数指针类型,避免依赖函数指针与 void * 之间的转换。
typedef void (*RedisModuleUserChangedFunc)(uint64_t client_id, void *privdata);

/* The module type, which is referenced in each value of a given type, defines
 * the methods and links to the module exporting the type. */
// 模块自定义类型的元数据,关联所属模块及操作该类型值的回调。
typedef struct RedisModuleType {
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
// 模块值包装,关联类型回调和模块持有的实际数据。
typedef struct moduleValue {
    moduleType *type;
    void *value;
} moduleValue;

/* This is a wrapper for the 'rio' streams used inside rdb.c in Redis, so that
 * the user does not have to take the total count of the written bytes nor
 * to care about error conditions. */
// 模块持久化的 I/O 上下文,封装 rio 流并记录读写字节数和错误状态。
typedef struct RedisModuleIO {
    size_t bytes; /* Bytes read / written so far. */
    rio *rio; /* Rio stream. */
    moduleType *type; /* Module type doing the operation. */
    int error; /* True if error condition happened. */
    int ver; /* Module serialization version: 1 (old),
              * 2 (current version with opcodes annotation). */
    struct RedisModuleCtx *ctx; /* Optional context, see RM_GetContextFromIO()*/
    struct redisObject *key; /* Optional name of key processed */
} RedisModuleIO;

/* Macro to initialize an IO context. Note that the 'ver' field is populated
 * inside rdb.c according to the version of the value to load. */
// 初始化模块 I/O 上下文;加载数据时由 rdb.c 设置序列化版本 ver。
#define moduleInitIOContext(iovar, mtype, rioptr, keyptr)                                                              \
    do {                                                                                                               \
        iovar.rio = rioptr;                                                                                            \
        iovar.type = mtype;                                                                                            \
        iovar.bytes = 0;                                                                                               \
        iovar.error = 0;                                                                                               \
        iovar.ver = 0;                                                                                                 \
        iovar.key = keyptr;                                                                                            \
        iovar.ctx = NULL;                                                                                              \
    } while (0)

/* This is a structure used to export DEBUG DIGEST capabilities to Redis
 * modules. We want to capture both the ordered and unordered elements of
 * a data structure, so that a digest can be created in a way that correctly
 * reflects the values. See the DEBUG DIGEST command implementation for more
 * background. */
// 模块 DEBUG DIGEST 上下文,分别累积有序元素摘要和无序元素的异或摘要。
typedef struct RedisModuleDigest {
    unsigned char o[20]; /* Ordered elements. */
    unsigned char x[20]; /* Xored elements. */
} RedisModuleDigest;

/* Just start with a digest composed of all zero bytes. */
// 将两组摘要缓冲区清零。
#define moduleInitDigestContext(mdvar)                                                                                 \
    do {                                                                                                               \
        memset(mdvar.o, 0, sizeof(mdvar.o));                                                                           \
        memset(mdvar.x, 0, sizeof(mdvar.x));                                                                           \
    } while (0)

// Redis 值对象,记录逻辑类型、底层编码、淘汰信息和引用计数。
typedef struct redisObject {
    unsigned type : 4; // 逻辑类型,取值见 OBJ_STRING、OBJ_LIST、OBJ_STREAM 等。
    unsigned encoding : 4; // 底层存储编码,例如整数、SDS、ziplist 或 skiplist。
    unsigned lru : LRU_BITS; /* 24 位复用字段:LRU 模式存储秒级循环时钟;
                              * LFU 模式高 16 位存储分钟级时间,低 8 位存储对数访问计数。 */
    int refcount; // 普通对象引用计数归零后释放;共享对象和栈对象使用特殊计数值。
    void *ptr; // 由 encoding 决定含义:指向底层数据,或在整数编码时直接保存整数。
} robj;

/* The a string name for an object's type as listed above
 * Native types are checked against the OBJ_STRING, OBJ_LIST, OBJ_* defines,
 * and Module types have their registered name returned. */
// 返回对象类型名称;模块自定义类型返回注册名称。
char *getObjectTypeName(robj *);

/* This structure is used in order to represent the output buffer of a client,
 * which is actually a linked list of blocks like that, that is: client->reply. */
// 客户端回复链表中的数据块;size 为容量,used 为已使用字节数。
typedef struct clientReplyBlock {
    size_t size, used;
    char buf[];
} clientReplyBlock;

/* Redis database representation. There are multiple databases identified
 * by integers from 0 (the default database) up to the max configured
 * database. The database number is the 'id' field in the structure. */
// 逻辑数据库,编号范围为 0 到 server.dbnum - 1,默认选择 0 号数据库。
typedef struct redisDb {
    dict *dict; /* The keyspace for this DB */ // 键空间:SDS 键名映射到 robj 值对象。
    struct dict *expires; /* Timeout of keys with a timeout set */ // 设置了过期时间的键及其 Unix 毫秒时间戳。
    struct dict *blocking_keys; /* Keys with clients waiting for data (BLPOP)*/ // 键映射到等待其数据的阻塞客户端列表。
    struct dict *ready_keys; /* Blocked keys that received a PUSH */ // 已进入就绪队列的键,用于去重。
    struct dict *watched_keys; /* WATCHED keys for MULTI/EXEC */ // 键映射到通过 WATCH 监视它的客户端列表。
    int id; /* Database ID */ // 逻辑数据库编号。
    long long avg_ttl; /* Average TTL, just for stats */ // 采样得到的平均剩余有效期,单位为毫秒,仅用于统计。
    unsigned long expires_cursor; /* Cursor of the active expire cycle. */ // 主动过期扫描使用的游标。
    list *defrag_later;
            /* List of key names to attempt to defrag one by one, gradually. */ // 待分批进行碎片整理的键名列表。
} redisDb;

/* This structure holds the blocking operation state for a client.
 * The fields used depend on client->btype. */
// 此结构保存客户端的阻塞操作状态。 使用的字段取决于 client->btype
typedef struct blockingState {
    /* Generic fields. */
    mstime_t timeout; /* Blocking operation timeout. If UNIX current time
                       * is > timeout then the operation timed out. */
    // 阻塞截止时间,Unix 毫秒时间戳;0 表示无限等待。
    /* BLOCKED_LIST, BLOCKED_ZSET and BLOCKED_STREAM */
    dict *keys; /* The keys we are waiting to terminate a blocking
                 * operation such as BLPOP or XREAD. Or NULL. */
    // 正在等待数据的键集合。
    robj *target; /* The key that should receive the element,
                   * for BLMOVE. */
    // BLMOVE 接收弹出元素的目标键。
    struct listPos {
        int wherefrom; /* Where to pop from */ // 源列表弹出端:头部或尾部。
        int whereto; /* Where to push to */ // 目标列表插入端:头部或尾部。
    } listpos; /* The positions in the src/dst lists
                * where we want to pop/push an element
                * for BLPOP, BRPOP and BLMOVE. */

    /* BLOCK_STREAM */
    size_t xread_count; /* XREAD COUNT option. */
    robj *xread_group; /* XREADGROUP group name. */
    robj *xread_consumer; /* XREADGROUP consumer name. */
    mstime_t xread_retry_time, xread_retry_ttl;
    int xread_group_noack;

    /* BLOCKED_WAIT */
    int numreplicas; /* Number of replicas we are waiting for ACK. */
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
// 就绪键记录:命令或脚本执行后,用于唤醒等待该键数据的阻塞客户端。
// server.ready_keys 保存记录,各数据库的 ready_keys 字典负责去重。
typedef struct readyList {
    redisDb *db;
    robj *key;
} readyList;

// 用于存储 Redis 用户的权限和相关信息。
typedef struct user {
    sds name; /* The username as an SDS string. */ // 作为 SDS 字符串的用户名。
    uint64_t flags; /* See USER_FLAG_* */ // 用户状态和权限选项,取值见 USER_FLAG_*。

    /* The bit in allowed_commands is set if this user has the right to
     * execute this command. In commands having subcommands, if this bit is
     * set, then all the subcommands are also available.
     *
     * If the bit for a given command is NOT set and the command has
     * subcommands, Redis will also check allowed_subcommands in order to
     * understand if the command can be executed. */
    // 命令权限位图:对应位为 1 时允许该命令及其全部子命令。
    uint64_t allowed_commands[USER_COMMAND_BITS_COUNT / 64];

    /* This array points, for each command ID (corresponding to the command
     * bit set in allowed_commands), to an array of SDS strings, terminated by
     * a NULL pointer, with all the sub commands that can be executed for
     * this command. When no subcommands matching is used, the field is just
     * set to NULL to avoid allocating USER_COMMAND_BITS_COUNT pointers. */
    // 按命令 ID 索引的子命令白名单;命令权限位未设置时用于细粒度授权。
    sds **allowed_subcommands;
    list *passwords; /* A list of SDS valid passwords for this user. */ // 可用密码的 SHA256 摘要列表。

    list *patterns; /* A list of allowed key patterns. If this field is NULL
                       the user cannot mention any key in a command, unless
                       the flag ALLKEYS is set in the user. */
    // 允许的键模式列表

    list *channels; /* A list of allowed Pub/Sub channel patterns. If this
                       field is NULL the user cannot mention any channel in a
                       `PUBLISH` or [P][UNSUSBSCRIBE] command, unless the flag
                       ALLCHANNELS is set in the user. */
    // 允许的 Pub/Sub 频道模式列表。
} user;

// 客户端连接上下文,保存协议解析、命令执行、回复、复制及阻塞状态。
typedef struct client {
    uint64_t id; /* Client incremental unique ID. */ // 客户端唯一ID
    connection *conn; // 连接
    int resp; /* RESP protocol version. Can be 2 or 3. */ // RESP协议版本
    redisDb *db; /* Pointer to currently SELECTed DB. */ // 当前选择的DB
    robj *name; /* As set by CLIENT SETNAME. */ // 客户端名称,可以使用命令CLIENT SETNAME设置。
    sds querybuf;
            /* Buffer we use to accumulate client queries. */ // 输入缓冲区,recv函数接收的客户端命令请求会暂时缓存在此缓冲区。
    size_t qb_pos; /* The position we have read in querybuf. */ // 输入缓冲区中下一个待解析字节的位置。
    sds pending_querybuf; /* If this client is flagged as master, this buffer
                              represents the yet not applied portion of the
                              replication stream that we are receiving from
                              the master. */
    // 对主节点连接,保存已接收但尚未应用的复制流。
    size_t querybuf_peak; /* Recent (100ms or more) peak of querybuf size. */ // 最近采样周期的输入缓冲区大小峰值。
    int argc; /* Num of arguments of current command. */ // 当前命令参数数量,包含命令名。
    robj **argv; /* Arguments of current command. */ // 当前命令的参数
    int original_argc; /* Num of arguments of original command if arguments were rewritten. */
    robj **original_argv; /* Arguments of original command if arguments were rewritten. */
    size_t argv_len_sum; /* Sum of lengths of objects in argv list. */
    struct redisCommand *cmd, *lastcmd; /* Last command executed. */
    // cmd 为当前命令,lastcmd 为最近一次查找到的命令。

    user *user; /* User associated with this connection. If the
                    user is set to NULL the connection can do
                    anything (admin). */
    // 关联的 ACL 用户;内部连接可用 NULL 表示不受用户权限限制。
    int reqtype; /* Request protocol type: PROTO_REQ_* */ // 请求协议类型
    int multibulklen; /* Number of multi bulk arguments left to read. */ // RESP 数组请求中尚未读完的参数数量。
    long bulklen; /* Length of bulk argument in multi bulk request. */ // 当前 bulk string 参数的字节长度,-1 表示尚未解析长度。
    list *reply; /* List of reply objects to send to the client. */ // 待发送回复数据块链表。
    unsigned long long reply_bytes; /* Tot bytes of objects in reply list. */ // 回复链表中数据块的容量总和,不含固定缓冲区。
    size_t sentlen; /* 当前正在发送的回复缓冲区或数据块中,已发送的字节数。 */
    time_t ctime; /* Client creation time. */ // 客户端创建时间
    long duration; /* Current command duration. Used for measuring latency of blocking/non-blocking cmds */
    time_t lastinteraction;
            /* Time of the last interaction, used for timeout */ // 客户端上次与服务器交互的时间,以此实现客户端的超时处理。
    time_t obuf_soft_limit_reached_time; // 输出缓冲区开始持续超过软限制的 Unix 秒时间戳。
    uint64_t flags; /* Client flags: CLIENT_* macros. */ // 客户端标志 CLIENT_*  宏
    int authenticated; /* Needed when the default user requires auth. */ // 客户端是否已通过认证。
    int replstate; /* Replication state if this is a slave. */ // 复制状态,如果这是一个从节点
    int repl_put_online_on_ack; /* Install slave write handler on first ACK. */ // 等待从节点首次 ACK 后再安装写事件处理器。
    int repldbfd; /* Replication DB file descriptor. */ // 向从节点发送的 RDB 文件描述符。
    off_t repldboff; /* Replication DB file offset. */ // 已发送 RDB 数据的文件偏移量。
    off_t repldbsize; /* Replication DB file size. */ // 待发送 RDB 文件的字节大小。
    sds replpreamble; /* Replication DB preamble. */ // 发送 RDB 前的协议前导数据,例如长度头。
    long long read_reploff; /* Read replication offset if this is a master. */ // 对主节点连接,记录已读取的复制流偏移量。
    long long reploff; /* Applied replication offset if this is a master. */ // 对主节点连接,记录已应用的复制流偏移量。
    long long repl_ack_off; /* Replication ack offset, if this is a slave. */ // 从节点最近 ACK 确认的复制偏移量。
    long long repl_ack_time; /* Replication ack time, if this is a slave. */ // 最近收到从节点 ACK 的 Unix 秒时间戳。
    long long psync_initial_offset; /* FULLRESYNC reply offset other slaves
                             copying this slave output buffer
                             should use. */
    char replid[CONFIG_RUN_ID_SIZE + 1];
            /* Master replication ID (if master). */ // 对主节点连接,保存其复制 ID。
    int slave_listening_port; /* As configured with: REPLCONF listening-port */ // 从节点通过 REPLCONF listening-port 上报的监听端口。
    char slave_ip[NET_IP_STR_LEN]; /* Optionally given by REPLCONF ip-address */ // 从节点通过 REPLCONF ip-address 上报的可选地址。
    int slave_capa; /* Slave capabilities: SLAVE_CAPA_* bitwise OR. */ // 从节点能力位图,由 SLAVE_CAPA_* 标志按位或组合。
    multiState mstate; /* MULTI/EXEC state */ // 事务状态
    int btype; /* Type of blocking op if CLIENT_BLOCKED. */ // 客户端阻塞类型
    blockingState bpop; /* blocking state */ // 阻塞状态
    long long woff; /* Last write global replication offset. */ // 当前客户端最近一次写命令对应的全局复制偏移量,供 WAIT 使用。
    list *watched_keys; /* Keys WATCHED for MULTI/EXEC CAS */ // 当前客户端通过 WATCH 监视的键。
    dict *pubsub_channels; /* channels a client is interested in (SUBSCRIBE) */ // 当前客户端通过 SUBSCRIBE 订阅的频道。
    list *pubsub_patterns; /* patterns a client is interested in (SUBSCRIBE) */ // 当前客户端通过 PSUBSCRIBE 订阅的模式。
    sds peerid; /* Cached peer ID. */ // 缓存的远端地址标识。
    sds sockname; /* Cached connection target address. */ // 缓存的连接目标地址。
    listNode *client_list_node; /* list node in client list */ // 客户端列表的节点
    listNode *paused_list_node; /* list node within the pause list */
    RedisModuleUserChangedFunc auth_callback; /* Module callback to execute
                                               * when the authenticated user
                                               * changes. */
    // 模块认证用户变更或连接关闭时执行的回调。
    void *auth_callback_privdata; /* Private data that is passed when the auth
                                   * changed callback is executed. Opaque for
                                   * Redis Core. */
    // 传给认证变更回调的私有数据,核心代码不解释其内容。
    void *auth_module; /* The module that owns the callback, which is used
                        * to disconnect the client if the module is
                        * unloaded for cleanup. Opaque for Redis Core.*/
    // 回调所属模块,卸载模块时用于清理相关客户端。

    /* If this client is in tracking mode and this field is non zero,
     * invalidation messages for keys fetched by this client will be send to
     * the specified client ID. */
    // 跟踪模式下,非零值表示将缓存失效通知转发给该 ID 对应的客户端。
    uint64_t client_tracking_redirection;
    rax *client_tracking_prefixes; /* A dictionary of prefixes we are already
                                      subscribed to in BCAST mode, in the
                                      context of client side caching. */
    // 客户端缓存 BCAST 模式订阅的键前缀,使用基数树保存。
    /* In clientsCronTrackClientsMemUsage() we track the memory usage of
     * each client and add it to the sum of all the clients of a given type,
     * however we need to remember what was the old contribution of each
     * client, and in which categoty the client was, in order to remove it
     * before adding it the new value. */
    // 缓存上次统计的内存占用和客户端类别,更新分类汇总时先扣除旧值。
    uint64_t client_cron_last_memory_usage;
    int client_cron_last_memory_type;
    /* Response buffer */
    // 固定大小的回复缓冲区。
    int bufpos; // 固定回复缓冲区中已使用的字节数。
    char buf[PROTO_REPLY_CHUNK_BYTES]; // 输出缓冲区,存储待返回给客户端的命令回复数据,
} client;

// 一条自动 RDB 保存条件:时间间隔和最少变更次数。
typedef struct saveparam {
    time_t seconds; // 自动触发 RDB 保存的时间间隔,单位为秒。
    int changes; // 该时间间隔内触发保存所需的最少数据变更次数。
} saveparam;

// 用于存储模块加载队列的条目信息。
struct moduleLoadQueueEntry {
    sds path;
    int argc;
    robj **argv;
};

struct sentinelLoadQueueEntry {
    int argc;
    sds *argv;
    int linenum;
    sds line;
};

struct sentinelConfig {
    list *pre_monitor_cfg;
    list *monitor_cfg;
    list *post_monitor_cfg;
};

// 复用常见协议回复、错误信息、命令参数和整数对象,减少重复分配。
struct sharedObjectsStruct {
    robj *crlf, *ok, *err, *emptybulk, *czero, *cone, *pong, *space, *colon, *queued, *null[4], *nullarray[4],
            *emptymap[4], *emptyset[4], *emptyarray, *wrongtypeerr, *nokeyerr, *syntaxerr, *sameobjecterr,
            *outofrangeerr, *noscripterr, *loadingerr, *slowscripterr, *bgsaveerr, *masterdownerr, *roslaveerr,
            *execaborterr, *noautherr, *noreplicaserr, *busykeyerr, *oomerr, *plus, *messagebulk, *pmessagebulk,
            *subscribebulk, *unsubscribebulk, *psubscribebulk, *punsubscribebulk, *del, *unlink, *rpop, *lpop, *lpush,
            *rpoplpush, *lmove, *blmove, *zpopmin, *zpopmax, *emptyscan, *multi, *exec, *left, *right, *hset, *srem,
            *xgroup, *xclaim, *script, *replconf, *eval, *persist, *set, *pexpireat, *pexpire, *time,
            *pxat, // 绝对过期时间标志
            *px, // 相对过期时间标志
            *retrycount, *force, *justid, *lastid, *ping, *setid, *keepttl, *load, *createconsumer, *getack,
            *special_asterick, *special_equals, *default_username, *select[PROTO_SHARED_SELECT_CMDS],
            *integers[OBJ_SHARED_INTEGERS], // 共享整数对象
            *mbulkhdr[OBJ_SHARED_BULKHDR_LEN], /* "*<value>\r\n" */
            *bulkhdr[OBJ_SHARED_BULKHDR_LEN]; /* "$<value>\r\n" */
    sds minstring, maxstring;
};

// 客户端输出缓冲区限制:硬限制立即生效,软限制需持续超过指定秒数。
typedef struct clientBufferLimitsConfig {
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
// 待追加传播的命令,记录参数、数据库和 AOF/复制传播目标(PROPAGATE_*)。
typedef struct redisOp {
    robj **argv; // 指向参数数组的指针
    int argc, dbid, target; // 参数个数、数据库 ID 和目标标识
    struct redisCommand *cmd; // 指向 Redis 命令的指针
} redisOp;

/* Defines an array of Redis operations. There is an API to add to this
 * structure in an easy way.
 *
 * redisOpArrayInit();
 * redisOpArrayAppend();
 * redisOpArrayFree();
 */
// 待传播命令数组,由 redisOpArray 系列函数管理。
typedef struct redisOpArray {
    redisOp *ops; // 指向操作数组的指针
    int numops; // 数量
} redisOpArray;

/* This structure is returned by the getMemoryOverheadData() function in
 * order to return memory overhead information. */
// 内存使用统计:记录客户端、持久化缓冲区及各数据库哈希表等内存开销。
// 容量字段以字节为单位,键数量、数据库数量及比例字段除外。
struct redisMemOverhead {
    size_t peak_allocated; // 内存分配峰值
    size_t total_allocated; // 总内存分配
    size_t startup_allocated; // 启动时分配的内存
    size_t repl_backlog; // 复制积压缓冲区大小
    size_t clients_slaves; // 从节点客户端占用的内存字节数。
    size_t clients_normal; // 非从节点客户端的内存字节数,含普通、发布订阅和主节点连接。
    size_t aof_buffer; // AOF 缓冲区大小
    size_t lua_caches; // Lua 缓存大小
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
    struct {
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
// RDB 中保存和恢复的复制元数据,供全量同步后的复制流衔接使用。
typedef struct rdbSaveInfo {
    /* Used saving and loading. */
    int repl_stream_db;
            /* DB to select in server.master client. */ // 指定当前复制流中服务器客户端应选择的数据库(DB)。

    /* Used only loading. */
    int repl_id_is_set; /* True if repl_id field is set. */
    char repl_id[CONFIG_RUN_ID_SIZE + 1];
            /* Replication ID. */ // 复制 ID,末尾预留字符串结束符。
    long long repl_offset; // RDB 保存的复制偏移量,加载时用于恢复复制状态。
} rdbSaveInfo;

// 周期采样的内存分配器及进程内存统计,单位为字节。
struct malloc_stats {
    size_t zmalloc_used;        // zmalloc 使用的内存量
    size_t process_rss;         // 进程占用的物理内存大小
    size_t allocator_allocated; // 分配器已分配的内存量
    size_t allocator_active;    // 分配器活跃的内存量
    size_t allocator_resident;  // 分配器驻留的内存量
};

/*-----------------------------------------------------------------------------
 * Global server state
 *----------------------------------------------------------------------------*/
struct clusterState;

// redisServer结构体存储服务端配置项、运行时数据
struct redisServer {
    /* General */
    // 常规
    pid_t pid; /* Main process pid. */ // 主进程ID
    pthread_t main_thread_id; /* Main thread id */ // 主线程ID
    char *configfile; /* Absolute config file path, or NULL */ // 配置文件路径
    char *executable; /* Absolute executable file path. */ // redis的可执行文件路径
    char **exec_argv; /* Executable argv vector (copy). */ // 记录redis执行的参数
    int dynamic_hz; /* Change hz value depending on # of clients. */ // 是否根据客户端数量动态调整 serverCron 运行频率。
    int config_hz; /* Configured HZ value. May be different than
                        the actual 'hz' field value if dynamic-hz
                        is enabled. */
    // 配置的 HZ 值。如果启用了动态 hz,则可能与实际的"hz"字段值不同。
    mode_t umask; /* The umask value of the process on startup */ // 进程在启动时的掩码值
    int hz; /* serverCron() calls frequency in hertz */ // redis 定时任务触发的频率
    int in_fork_child; /* indication that this is a fork child */ // 表明这是一个子进程
    redisDb *db; // redisDb 数组,默认 16 个 redisDb
    dict *commands; /* Command table */ // redis 支持的命令的字典
    dict *orig_commands; /* Command table before command renaming. */ // 命令重命名前的原始命令表。
    aeEventLoop *el; // redis事件循环实例
    rax *errors; /* Errors table */ // 错误表
    redisAtomic unsigned int lruclock; /* Clock for LRU eviction */ // LRU驱逐时钟
    volatile sig_atomic_t shutdown_asap; /* SHUTDOWN needed ASAP */ // 需要尽快关闭
    int activerehashing; /* Incremental rehash in serverCron() */ // serverCron() 中的增量重新哈希
    int active_defrag_running;
            /* Active defragmentation running (holds current scan aggressiveness) */ // 当前主动碎片整理的扫描强度,0 表示未运行。
    char *pidfile; /* PID file path */ // pidfile路径
    int arch_bits; /* 32 or 64 depending on sizeof(long) */ // 32或者64取决于long的大小
    int cronloops; /* Number of times the cron function run */ // cron 函数运行的次数
    char runid[CONFIG_RUN_ID_SIZE + 1]; /* ID always different at every exec. */ // 当前redis实例的 runid
    int sentinel_mode; /* True if this instance is a Sentinel. */ // 如果此实例是哨兵,则为 true。
    size_t initial_memory_usage; /* Bytes used after initialization. */ // 初始化后使用的字节数。
    int always_show_logo; /* Show logo even for non-stdout logging. */ // 始终显示logo
    int in_eval; /* Are we inside EVAL? */ // 当前是否正在执行 Lua 脚本。
    int in_exec; /* Are we inside EXEC? */ // 当前是否正在执行事务队列。
    int propagate_in_transaction;
            /* Make sure we don't propagate nested MULTI/EXEC */ // 记录事务传播状态,防止生成嵌套的 MULTI/EXEC。
    char *ignore_warnings; /* Config: warnings that should be ignored. */ // 配置:应忽略的警告。
    int client_pause_in_transaction;
            /* Was a client pause executed during this Exec? */ // 当前 EXEC 执行期间是否触发了客户端暂停。

    /* Modules */
    // 模块
    dict *moduleapi; /* Exported core APIs dictionary for modules. */ // 导出模块的核心 API 字典。
    dict *sharedapi; /* Like moduleapi but containing the APIs that
                        modules share with each other. */
    // 与模块API类似,但包含模块相互共享的API。
    list *loadmodule_queue; /* List of modules to load at startup. */ // 启动时要加载的模块列表。
    int module_blocked_pipe[2]; /* Pipe used to awake the event loop if a
                                   client blocked on a module command needs
                                   to be processed. */
    // 模块阻塞客户端需要处理时,用于唤醒主事件循环的管道。
    pid_t child_pid; /* PID of current child */ // 当前后台子进程的 PID。
    int child_type; /* Type of current child */ // 当前后台子进程类型。

    /* Networking */
    // 网络
    int port; /* TCP listening port */ // TCP 侦听端口
    int tls_port; /* TLS listening port */ // TLS 侦听端口
    int tcp_backlog; /* TCP listen() backlog */ // TCP listen() 的待接收连接队列长度参数。
    char *bindaddr[CONFIG_BINDADDR_MAX]; /* Addresses we should bind to */ // 绑定的地址
    int bindaddr_count; /* Number of addresses in server.bindaddr[] */ // 绑定的地址数
    char *unixsocket; /* UNIX socket path */ // UNIX 套接字路径
    mode_t unixsocketperm; /* UNIX socket permission */ // UNIX 套接字权限
    int ipfd[CONFIG_BINDADDR_MAX]; /* TCP socket file descriptors */ // TCP 套接字文件描述符
    int ipfd_count; /* Used slots in ipfd[] */ // ipfd 中有效的文件描述符数量
    int tlsfd[CONFIG_BINDADDR_MAX]; /* TLS socket file descriptors */ // TLS 套接字文件描述符
    int tlsfd_count; /* Used slots in tlsfd[] */ // tlsfd 中有效的文件描述符数量
    int sofd; /* Unix socket file descriptor */ // Unix 套接字文件描述符
    int cfd[CONFIG_BINDADDR_MAX]; /* Cluster bus listening socket */ // 集群总线侦听套接字
    int cfd_count; /* Used slots in cfd[] */ // cfd 中有效的文件描述符数量
    list *clients; /* List of active clients */ // 所有活跃的client
    list *clients_to_close; /* Clients to close asynchronously */ // 要异步关闭的客户端
    list *clients_pending_write; /* There is to write or install handler. */ // 待写回数据的客户端
    list *clients_pending_read; /* Client has pending read socket buffers. */ // 待读取数据的客户端
    list *slaves, *monitors; /* List of slaves and MONITORs */ // 从节点连接列表和 MONITOR 客户端列表。
    client *current_client; /* Current client executing the command. */ // 当前正在执行命令的客户端。
    rax *clients_timeout_table; /* Radix tree for blocked clients timeouts. */
    long fixed_time_expire; /* If > 0, expire keys against server.mstime. */
    rax *clients_index; /* Active clients dictionary by client ID. */ // 按客户端 ID 索引活跃连接的基数树。
    pause_type client_pause_type; /* True if clients are currently paused */
    list *paused_clients; /* List of pause clients */
    mstime_t client_pause_end_time; /* Time when we undo clients_paused */
    char neterr[ANET_ERR_LEN]; /* Error buffer for anet.c */
    dict *migrate_cached_sockets;
    /* MIGRATE cached sockets */
    // 下一个客户端 ID,以原子方式递增。
    redisAtomic uint64_t next_client_id; /* Next client unique ID. Incremental. */
    int protected_mode; /* Don't accept external connections. */
    int gopher_enabled; /* If true the server will reply to gopher
                              queries. Will still serve RESP2 queries. */
    int io_threads_num; /* Number of IO threads to use. */ // 要使用的 IO 线程数。
    int io_threads_do_reads; /* Read and parse from IO threads? */ // 是否允许 I/O 线程读取并解析请求。
    int io_threads_active; /* Is IO threads currently active? */ // I/O 工作线程当前是否已启用。
    long long events_processed_while_blocked; /* processEventsWhileBlocked() */

    /* RDB / AOF loading information */
    // RDB AOF 加载信息
    volatile sig_atomic_t loading; /* We are loading data from disk if true */ // 如果为 true,我们正在从磁盘加载数据
    off_t loading_total_bytes; // 加载的总字节数
    off_t loading_rdb_used_mem; /* Memory used by the RDB file */ // RDB 元数据记录的保存端内存用量,单位为字节。
    off_t loading_loaded_bytes; /* Bytes loaded so far */ // 到目前为止加载的字节数
    time_t loading_start_time; // 加载开始时间
    off_t loading_process_events_interval_bytes; /* Process events every N bytes */ // 每N个字节处理事件

    /* Fast pointers to often looked up command */
    // 缓存常用命令的指针,避免反复按名称查找命令表。
    struct redisCommand *delCommand, *multiCommand, *lpushCommand, *lpopCommand, *rpopCommand, *zpopminCommand,
            *zpopmaxCommand, *sremCommand, *execCommand, *expireCommand, *pexpireCommand, *xclaimCommand,
            *xgroupCommand, *rpoplpushCommand, *lmoveCommand;

    /* Fields used only for stats */
    // 仅用于统计信息的字段
    time_t stat_starttime; /* Server start time */ // 服务器启动时间
    long long stat_numcommands; /* Number of processed commands */ // 已处理的命令数
    long long stat_numconnections; /* Number of connections received */ // 收到的连接数
    long long stat_expiredkeys; /* Number of expired keys */ // 过期键的数量
    double stat_expired_stale_perc; /* Percentage of keys probably expired */ // 键可能已过期的百分比
    long long stat_expired_time_cap_reached_count; /* Early expire cylce stops.*/
    long long stat_expire_cycle_time_used; /* Cumulative microseconds used. */
    long long stat_evictedkeys; /* Number of evicted keys (maxmemory) */
    long long stat_keyspace_hits; /* Number of successful lookups of keys */ // 成功查找键的次数
    long long stat_keyspace_misses; /* Number of failed lookups of keys */
    long long stat_active_defrag_hits; /* number of allocations moved */
    long long stat_active_defrag_misses; /* number of allocations scanned but not moved */
    long long stat_active_defrag_key_hits; /* number of keys with moved allocations */
    long long stat_active_defrag_key_misses; /* number of keys scanned and not moved */
    long long stat_active_defrag_scanned; /* number of dictEntries scanned */
    size_t stat_peak_memory; /* Max used memory record */
    long long stat_fork_time; /* Time needed to perform latest fork() */
    double stat_fork_rate; /* Fork rate in GB/sec. */
    long long stat_total_forks; /* Total count of fork. */
    long long stat_rejected_conn; /* Clients rejected because of maxclients */
    long long stat_sync_full; /* Number of full resyncs with slaves. */
    long long stat_sync_partial_ok; /* Number of accepted PSYNC requests. */
    long long stat_sync_partial_err; /* Number of unaccepted PSYNC requests. */
    list *slowlog; /* SLOWLOG list of commands */
    long long slowlog_entry_id; /* SLOWLOG current entry ID */
    long long slowlog_log_slower_than; /* SLOWLOG time limit (to get logged) */
    unsigned long slowlog_max_len; /* SLOWLOG max number of items logged */
    struct malloc_stats cron_malloc_stats; /* sampled in serverCron(). */
    redisAtomic long long stat_net_input_bytes; /* Bytes read from network. */ // 记录从网络输入的数据量(以字节为单位)
    redisAtomic long long stat_net_output_bytes;
            /* Bytes written to network. */ // 记录写入到网络的数据量(以字节为单位)
    size_t stat_current_cow_bytes; /* Copy on write bytes while child is active. */
    size_t stat_rdb_cow_bytes; /* Copy on write bytes during RDB saving. */
    size_t stat_aof_cow_bytes; /* Copy on write bytes during AOF rewrite. */
    size_t stat_module_cow_bytes; /* Copy on write bytes during module fork. */
    uint64_t stat_clients_type_memory[CLIENT_TYPE_COUNT]; /* Mem usage by type */
    long long stat_unexpected_error_replies; /* Number of unexpected (aof-loading, replica to master, etc.) error
                                                replies */
    long long stat_total_error_replies; /* Total number of issued error replies ( command + rejected errors ) */
    long long stat_dump_payload_sanitizations; /* Number deep dump payloads integrity validations. */
    long long stat_io_reads_processed; /* Number of read events processed by IO / Main threads */
    long long stat_io_writes_processed; /* Number of write events processed by IO / Main threads */
    redisAtomic long long stat_total_reads_processed;
            /* Total number of read events processed */ // 已处理的读取事件总数
    redisAtomic long long stat_total_writes_processed;
            /* Total number of write events processed */ // 已处理的写入事件总数

    /* The following two are used to track instantaneous metrics, like
     * number of operations per second, network traffic. */
    // 瞬时指标的采样窗口,用于估算每秒命令数和网络吞吐量。
    struct {
        long long last_sample_time; /* Timestamp of last sample in ms */
        long long last_sample_count; /* Count in last sample */
        long long samples[STATS_METRIC_SAMPLES];
        int idx;
    } inst_metric[STATS_METRIC_COUNT];

    /* Configuration */
    // 配置
    int verbosity; /* Loglevel in redis.conf */ // redis.conf配置的日志级别
    int maxidletime; /* Client timeout in seconds */ // 普通客户端空闲超时,单位为秒。
    int tcpkeepalive; /* Set SO_KEEPALIVE if non-zero. */ // SO_KEEPALIVE选项
    int active_expire_enabled; /* Can be disabled for testing purposes. */ // 是否启用主动过期扫描;不控制访问时的惰性过期检查。
    int active_expire_effort; /* From 1 (default) to 10, active effort. */ // 主动过期扫描强度,范围为 1 到 10。
    int active_defrag_enabled;
    int sanitize_dump_payload; /* Enables deep sanitization for ziplist and listpack in RDB and RESTORE. */
    int skip_checksum_validation; /* Disables checksum validateion for RDB and RESTORE payload. */
    int jemalloc_bg_thread; /* Enable jemalloc background thread */
    size_t active_defrag_ignore_bytes; /* minimum amount of fragmentation waste to start active defrag */
    int active_defrag_threshold_lower; /* minimum percentage of fragmentation to start active defrag */
    int active_defrag_threshold_upper; /* maximum percentage of fragmentation at which we use maximum effort */
    int active_defrag_cycle_min; /* minimal effort for defrag in CPU percentage */
    int active_defrag_cycle_max; /* maximal effort for defrag in CPU percentage */
    unsigned long active_defrag_max_scan_fields; /* maximum number of fields of set/hash/zset/list to process from
                                                    within the main dict scan */
    size_t client_max_querybuf_len; /* Limit for client query buffer length */ // 客户端查询缓冲区的最大长度限制
    int dbnum; /* Total number of configured DBs */ // 数据库数量
    // 是否由 systemd 或 upstart 等服务管理器监管。
    int supervised; /* 1 if supervised, 0 otherwise. */
    int supervised_mode; /* See SUPERVISED_* */
    int daemonize; /* True if running as a daemon */ // 如果作为守护程序运行,则为 True
    int set_proc_title; /* True if change proc title */ // 如果更改进程标题,则为 True
    char *proc_title_template; /* Process title template format */ // 进程标题模板。
    clientBufferLimitsConfig client_obuf_limits[CLIENT_TYPE_OBUF_COUNT];

    /* AOF persistence */
    // AOF 持久化
    int aof_enabled; /* AOF configuration */
    int aof_state; /* AOF_(ON|OFF|WAIT_REWRITE) */
    int aof_fsync; /* Kind of fsync() policy */
    char *aof_filename; /* Name of the AOF file */
    int aof_no_fsync_on_rewrite; /* Don't fsync if a rewrite is in prog. */
    int aof_rewrite_perc; /* Rewrite AOF if % growth is > M and... */
    off_t aof_rewrite_min_size; /* the AOF file is at least N bytes. */
    off_t aof_rewrite_base_size; /* AOF size on latest startup or rewrite. */
    off_t aof_current_size; /* AOF 当前大小. */
    off_t aof_fsync_offset; /* AOF offset which is already synced to disk. */
    int aof_flush_sleep; /* Micros to sleep before flush. (used by tests) */
    int aof_rewrite_scheduled; /* Rewrite once BGSAVE terminates. */
    list *aof_rewrite_buf_blocks; /* Hold changes during an AOF rewrite. */
    sds aof_buf; /* AOF 缓冲区, written before entering the event loop */
    int aof_fd; /* File descriptor of currently selected AOF file */
    int aof_selected_db; /* Currently selected DB in AOF */
    time_t aof_flush_postponed_start; /* UNIX time of postponed AOF flush */
    time_t aof_last_fsync; /* UNIX time of last fsync() */
    time_t aof_rewrite_time_last; /* Time used by last AOF rewrite run. */
    time_t aof_rewrite_time_start; /* Current AOF rewrite start time. */
    int aof_lastbgrewrite_status; /* C_OK or C_ERR */
    unsigned long aof_delayed_fsync; /* delayed AOF fsync() counter */
    int aof_rewrite_incremental_fsync; /* fsync incrementally while aof rewriting? */
    int rdb_save_incremental_fsync; /* fsync incrementally while rdb saving? */
    int aof_last_write_status; /* C_OK or C_ERR */
    int aof_last_write_errno; /* Valid if aof_last_write_status is ERR */
    int aof_load_truncated; /* Don't stop on unexpected AOF EOF. */
    int aof_use_rdb_preamble; /* Use RDB preamble on AOF rewrites. */
    /* AOF pipes used to communicate between parent and child during rewrite. */
    int aof_pipe_write_data_to_child;
    int aof_pipe_read_data_from_parent;
    int aof_pipe_write_ack_to_parent;
    int aof_pipe_read_ack_from_child;
    int aof_pipe_write_ack_to_child;
    int aof_pipe_read_ack_from_parent;
    int aof_stop_sending_diff; /* If true stop sending accumulated diffs
                                      to child process. */
    sds aof_child_diff; /* AOF diff accumulator child side. */

    /* RDB persistence */ // RDB 持久化
    long long dirty; // 尚未由成功 RDB 保存覆盖的数据变更计数,不等同于写命令条数。
    long long dirty_before_bgsave; /* Used to restore dirty on failed BGSAVE */ // 启动 BGSAVE 时的变更计数,用于完成后的计数调整。
    saveparam *saveparams; /* Save points array for RDB */ // RDB的保存点数组
    int saveparamslen; /* Number of saving points */ // RDB保存点的数量
    char *rdb_filename; /* Name of RDB file */ // RDB文件名
    int rdb_compression; /* Use compression in RDB? */ // 是否压缩RDB文件
    int rdb_checksum; /* Use RDB checksum? */ // 是否使用RDB校验
    int rdb_del_sync_files;
            /* Remove RDB files used only for SYNC if the instance does not use persistence. */ // 删除仅用于同步的RDB文件,如果实例不使用持久化

    time_t lastsave; /* Unix time of last successful save */ // 上次成功保存的 Unix 时间
    time_t lastbgsave_try; /* Unix time of last attempted bgsave */ // 记录最后一次尝试RDB保存的 Unix 时间
    time_t rdb_save_time_last; /* Time used by last RDB save run. */ // 最近一次 RDB 保存耗时,单位为秒。
    time_t rdb_save_time_start; /* Current RDB save start time. */ // 当前 RDB 保存开始时的 Unix 秒时间戳。
    int rdb_bgsave_scheduled; /* BGSAVE when possible if true. */ // 如果为真,则尝试在可能的情况下进行RDB保存
    int rdb_child_type; /* Type of save by active child. */ // 活跃子进程保存类型
    int lastbgsave_status; /* C_OK or C_ERR */ // 记录最后一次RDB保存的状态
    int stop_writes_on_bgsave_err; /* Don't allow writes if can't BGSAVE */ // 如果bgsave失败,则停止写入
    int rdb_pipe_read; /* RDB pipe used to transfer the rdb data to the parent process in diskless repl.*/ // 无盘复制时,父进程从该管道读取子进程生成的 RDB 数据。

    int rdb_child_exit_pipe;
            /* Used by the diskless parent allow child exit. */ // 用于无盘环境下的父进程控制子进程的退出。
    connection **rdb_pipe_conns; /* Connections which are currently the */
    int rdb_pipe_numconns; /* target of diskless rdb fork child. */
    int rdb_pipe_numconns_writing; /* Number of rdb conns with pending writes. */
    char *rdb_pipe_buff; /* In diskless replication, this buffer holds data */
    int rdb_pipe_bufflen; /* that was read from the the rdb pipe. */
    int rdb_key_save_delay; /* Delay in microseconds between keys while
                             * writing the RDB. (for testings). negative
                             * value means fractions of microsecons (on average). */
    int key_load_delay; /* Delay in microseconds between keys while
                         * loading aof or rdb. (for testings). negative
                         * value means fractions of microsecons (on average). */
    /* Pipe and data structures for child -> parent info sharing. */
    int child_info_pipe[2]; /* Pipe used to write the child_info_data. */
    int child_info_nread; /* Num of bytes of the last read from pipe */
    /* Propagation of commands in AOF / replication */
    redisOpArray also_propagate; /* Additional command to propagate. */

    /* Logging */
    // 日志
    char *logfile; /* Path of log file */ // 日志文件路径
    int syslog_enabled; /* Is syslog enabled? */ // 是否启用syslog
    char *syslog_ident; /* Syslog ident */ // syslog标识
    int syslog_facility; /* Syslog facility */ // 用于表示系统日志设施的类型。
    int crashlog_enabled; /* Enable signal handler for crashlog.
                           * disable for clean core dumps. */
    // 崩溃时使用信号处理程序进行crashlog
    int memcheck_enabled; /* Enable memory check on crash. */ // 内存检查
    int use_exit_on_panic; /* Use exit() on panic and assert rather than
                            * abort(). useful for Valgrind. */
    // 崩溃时使用exit()而不是abort()

    /* Replication (master) */
    // 复制(主)
    char replid[CONFIG_RUN_ID_SIZE + 1]; /* My current replication ID. */ // 当前复制历史的 ID,与进程 runid 不同。
    char replid2[CONFIG_RUN_ID_SIZE + 1]; /* replid inherited from master*/ // 保留的上一段复制历史 ID,用于部分重同步。
    long long master_repl_offset; /* My current replication offset */ // 当前复制流的全局字节偏移量。
    long long second_replid_offset;
            /* Accept offsets up to this for replid2. */ // 使用 replid2 请求部分重同步时,允许的最大请求偏移量。
    int slaveseldb; /* Last SELECTed DB in replication output */
    /* 向从节点发送复制心跳 PING 的间隔,单位为秒。 */
    int repl_ping_slave_period; /* Master pings the slave every N seconds */ // 主节点向从节点发送 PING 的间隔,单位为秒。
    /* 保存最近的复制流历史,供断线后的部分重同步使用;这是环形缓冲区。 */
    char *repl_backlog; /* Replication backlog for partial syncs */ // 用于部分重同步的复制积压缓冲区。
    long long repl_backlog_size; /* Backlog circular buffer size */ // 复制积压环形缓冲区容量,单位为字节。
    long long repl_backlog_histlen; /* Backlog actual data length */ // 积压缓冲区内有效复制历史的字节数。
    /* 环形缓冲区下一次写入的位置,不是最后一个已写入字节的位置。 */
    long long repl_backlog_idx; /* Backlog circular buffer current offset,
                             that is the next byte will'll write to.*/
    /* 复制缓冲区中第一个字节的复制偏移量 */
    long long repl_backlog_off; /* Replication "master offset" of first
                             byte in the replication backlog buffer.*/
    time_t repl_backlog_time_limit; /* Time without slaves after the backlog
                                 gets released. */
    // 没有从节点时保留积压缓冲区的时长(秒),0 表示不自动释放。
    /* 从节点数量降为零时的 Unix 秒时间戳。 */
    time_t repl_no_slaves_since; /* We have no slaves since that time.
                              Only valid if server.slaves len is 0. */
    // 仅在 server.slaves 为空时有效。
    /* 当有效从服务器的数目小于该值时,主服务器会拒绝执行写命令 */
    int repl_min_slaves_to_write; /* Min number of slaves to write. */ // 要写入的最小从属数量。
    /* 判定可计入写入保障的从节点所允许的最大 ACK 间隔,单位为秒。 */
    int repl_min_slaves_max_lag; /* Max lag of <count> slaves to write. */
    /* 在线且最近 ACK 距今不超过 repl_min_slaves_max_lag 的从节点数量。 */
    int repl_good_slaves_count; /* Number of slaves with lag <= max_lag. */
    int repl_diskless_sync; /* Master send RDB to slaves sockets directly. */ // 是否通过无盘复制向从节点传输 RDB,避免先写临时 RDB 文件。
    int repl_diskless_load; /* Slave parse RDB directly from the socket.
                             * see REPL_DISKLESS_LOAD_* enum */
    // 从节点无盘加载 RDB 的策略,取值见 REPL_DISKLESS_LOAD_*。
    int repl_diskless_sync_delay; /* Delay to start a diskless repl BGSAVE. */

    /* Replication (slave) */
    /* 连接主节点时用于 ACL 认证的可选用户名。 */
    char *masteruser; /* AUTH with this user and masterauth with master */ // 与 masterauth 一起用于向主节点发送 AUTH。
    /* 连接主节点时使用的认证密码,来自 masterauth 配置。 */
    sds masterauth; /* AUTH with this password with master */ // 向主节点认证时使用的密码。
    /* 主节点主机名或 IP 地址。 */
    char *masterhost; /* Hostname of master */ // 主服务器的主机名
    /* 主服务器的端口 */
    int masterport; /* Port of master */ // 主服务器的端口
    int repl_timeout; /* Timeout after N seconds of master idle */ // 主服务器空闲 N 秒后超时
    /* 本实例用 client 对象表示来自主节点的复制连接。 */
    client *master; /* Client that is master for this slave */ // 表示上游主节点的客户端对象。
    client *cached_master; /* Cached master to be reused for PSYNC. */ // 要重用于 PSYNC 的缓存主服务器。
    int repl_syncio_timeout; /* Timeout for synchronous I/O calls */ // 同步 IO 调用超时
    /* 主从复制流程的进展(从服务器状态)*/
    int repl_state; /* Replication status if the instance is a slave */ // 复制状态(如果实例是从属实例)
    off_t repl_transfer_size; /* Size of RDB to read from master during sync. */ // 同步期间从主服务器读取的 RDB 大小。
    off_t repl_transfer_read; /* Amount of RDB read from master during sync. */ // 同步期间从主服务器读取的 RDB 量。
    off_t repl_transfer_last_fsync_off; /* Offset when we fsync-ed last time. */ // 接收 RDB 时最近一次执行 fsync 的文件偏移量。
    connection *repl_transfer_s; /* Slave -> Master SYNC connection */ // 从节点到主机的连接
    int repl_transfer_fd; /* Slave -> Master SYNC temp file descriptor */ // 从节点到主机的临时文件描述符
    char *repl_transfer_tmpfile; /* Slave-> master SYNC temp file name */ // 从节点到主机的临时文件名
    time_t repl_transfer_lastio; /* Unix time of the latest read, for timeout */ // 最新读取的 Unix 时间,用于超时
    /* 与主节点断连或尚未完成首次同步时,是否仍允许提供旧数据。 */
    int repl_serve_stale_data; /* Serve stale data when link is down? */
    /* 从节点是否拒绝普通客户端写入;主节点复制的写命令仍可执行。 */
    int repl_slave_ro; /* Slave is read only? */
    /* 从节点是否忽略 maxmemory 限制,不执行内存淘汰;不等于关闭键过期。 */
    int repl_slave_ignore_maxmemory; /* If true slaves do not evict. */
    /* 从节点与主机断开的时间 */
    time_t repl_down_since; /* Unix time at which link with master went down */ // 与主节点断连时的 Unix 秒时间戳。
    int repl_disable_tcp_nodelay; /* Disable TCP_NODELAY after SYNC? */ // 同步后禁用TCP_NODELAY？
    int slave_priority; /* Reported in INFO and used by Sentinel. */ // 在 INFO 中报告并由 Sentinel 使用。
    int slave_announce_port; /* Give the master this listening port. */ // 为主机提供此侦听端口。
    char *slave_announce_ip; /* Give the master this ip address. */ // 为主服务器提供此 IP 地址。
    /* The following two fields is where we store master PSYNC replid/offset
     * while the PSYNC is in progress. At the end we'll copy the fields into
     * the server->master client structure. */
    char master_replid[CONFIG_RUN_ID_SIZE + 1]; /* Master PSYNC runid. */
    long long master_initial_offset; /* Master PSYNC offset. */
    int repl_slave_lazy_flush; /* Lazy FLUSHALL before loading DB? */ // 复制加载新数据前,是否异步释放旧数据库。

    /* Replication script cache. */
    // 复制脚本缓存。
    dict *repl_scriptcache_dict; /* SHA1 all slaves are aware of. */
    list *repl_scriptcache_fifo; /* First-in, first-out eviction. */ // 已传播脚本摘要的先进先出淘汰队列。
    unsigned int repl_scriptcache_size; /* Max number of elements. */

    /* Synchronous replication. */
    // 同步复制。
    list *clients_waiting_acks; /* Clients waiting in WAIT command. */ // 正在执行 WAIT、等待从节点复制确认的客户端。
    int get_ack_from_slaves; /* If true we send REPLCONF GETACK. */ // 是否需要向从节点发送 REPLCONF GETACK 请求确认。

    /* Limits */
    // 限制
    unsigned int maxclients; /* Max number of simultaneous clients */ // 最大允许的并发客户端数
    unsigned long long maxmemory; /* Max number of memory bytes to use */ // 要使用的最大内存字节数
    int maxmemory_policy; /* Policy for key eviction */ // 达到内存限制时的键淘汰策略。
    int maxmemory_samples;
            /* Precision of random sampling */ // 由 redis.conf 中的配置项 maxmemory-samples 决定的,该配置项的默认值是 5
    int maxmemory_eviction_tenacity; /* Aggressiveness of eviction processing */ // 单轮内存淘汰的执行强度。
    int lfu_log_factor; /* LFU logarithmic counter factor. */ // LFU 对数计数器因子
    int lfu_decay_time; /* LFU counter decay factor. */ // LFU 访问计数衰减周期,单位为分钟;0 表示不衰减。
    long long proto_max_bulk_len; /* Protocol bulk length maximum size. */ // RESP bulk string 参数允许的最大字节长度。
    int oom_score_adj_base;
            /* Base oom_score_adj value, as observed on startup */ // 启动时观察到的 oom_score_adj 的基本值
    int oom_score_adj_values[CONFIG_OOM_COUNT]; /* Linux oom_score_adj configuration */ // Linux oom_score_adj 配置
    int oom_score_adj; /* If true, oom_score_adj is managed */ // 如果为真,则管理 oom_score_adj
    int disable_thp; /* If true, disable THP by syscall */ // 禁用 THP 的系统调用

    /* Blocked clients */
    unsigned int blocked_clients; /* # of clients executing a blocking cmd.*/ // 记录当前执行阻塞命令的客户端数量。
    unsigned int blocked_clients_by_type[BLOCKED_NUM]; // 按类型统计或记录被阻塞的客户端数量。
    list *unblocked_clients; /* list of clients to unblock before next loop */ // 已解除阻塞、等待重新处理输入的客户端队列。
    list *ready_keys; /* List of readyList structures for BLPOP & co */

    /* Client side caching. */
    // 客户端缓存。
    unsigned int tracking_clients; /* # of clients with tracking enabled.*/ // 记录启用跟踪功能的客户端数量。
    size_t tracking_table_max_keys;
            /* Max number of keys in tracking table. */ // 客户端缓存跟踪表允许记录的最大键数量。
    /* Sort parameters shared with sortCompare(). */
    // 为兼容不同平台的排序接口,通过全局状态向 sortCompare() 传递排序选项。
    int sort_desc;
    int sort_alpha;
    int sort_bypattern;
    int sort_store;

    /* Zip structure config, see redis.conf for more information  */
    // 紧凑编码和流节点的大小阈值,具体配置见 redis.conf。
    size_t hash_max_ziplist_entries; // 哈希保持 ziplist 编码时允许的最大字段数量。
    size_t hash_max_ziplist_value; // 哈希保持 ziplist 编码时,字段名或值的最大字节长度。
    size_t set_max_intset_entries; // 集合保持 intset 整数集合编码时允许的最大成员数量。
    size_t zset_max_ziplist_entries; // 有序集合保持 ziplist 编码时允许的最大成员数量。
    size_t zset_max_ziplist_value; // 有序集合保持 ziplist 编码时,成员的最大字节长度。
    size_t hll_sparse_max_bytes; // HyperLogLog 稀疏编码的最大字节数,超限后转为稠密编码。
    size_t stream_node_max_bytes; // Stream 单个 listpack 节点的目标字节上限,单条大消息可能超出。
    long long stream_node_max_entries; // Stream 单个 listpack 节点的消息数量上限。

    /* List parameters */
    int list_max_ziplist_size; // quicklist 节点大小限制:非负值限制元素数,负值选择预设字节上限。
    int list_compress_depth; // quicklist 两端各保留不压缩的节点数,0 表示禁用压缩。

    /* time cache */
    redisAtomic time_t unixtime; /* Unix time sampled every cron cycle. */ // 缓存 unixtime
    time_t timezone; /* Cached timezone. As set by tzset(). */ // 缓存时区
    int daylight_active; /* Currently in daylight saving time. */ // 当前是否处于夏令时
    mstime_t mstime; /* 'unixtime' in milliseconds. */ // 毫秒级时间戳
    ustime_t ustime; /* 'unixtime' in microseconds. */ // 微秒级时间戳
    size_t blocking_op_nesting;
            /* Nesting level of blocking operation, used to reset blocked_last_cron. */ // 阻塞操作嵌套层数。
    long long blocked_last_cron;
            /* Indicate the mstime of the last time we did cron jobs from a blocking operation */ // 阻塞操作的最后一次cron执行的时间戳

    /* Pubsub */
    // 发布订阅
    dict *pubsub_channels;
            /* Map channels to list of subscribed clients */ // 将频道映射到已订阅客户端的列表(就是保存客户端和订阅的频道信息)
    list *pubsub_patterns; /* A list of pubsub_patterns */ // pubsub_patterns列表
    dict *pubsub_patterns_dict; /* A dict of pubsub_patterns */ // pubsub_patterns字典
    int notify_keyspace_events; /* Events to propagate via Pub/Sub. This is an
                                   bitwise OR of NOTIFY_... flags. */
    // 键空间通知事件位图,由 NOTIFY_* 标志按位或组合。
    /* Cluster */
    // 集群
    int cluster_enabled; /* Is cluster enabled? */ // 是否启用集群模式。
    mstime_t cluster_node_timeout; /* Cluster node timeout. */ // 集群节点超时阈值,单位为毫秒。
    char *cluster_configfile; /* Cluster auto-generated config file name. */ // 自动维护的集群配置文件路径。
    struct clusterState *cluster; /* State of the cluster */ // 集群运行状态。
    int cluster_migration_barrier; /* Cluster replicas migration barrier. */ // 从节点自动迁移时,原主节点需保留的正常从节点数量门槛。
    int cluster_slave_validity_factor; /* Slave max data age for failover. */ // 从节点自动故障转移的数据新鲜度判定系数,0 表示不按该条件限制。
    int cluster_require_full_coverage; /* If true, put the cluster down if
                                          there is at least an uncovered slot.*/
    // 是否在存在未覆盖槽位时将集群状态标记为不可用,不是关闭进程。
    int cluster_slave_no_failover; /* Prevent slave from starting a failover
                                   if the master is in failure state. */
    // 是否禁止从节点在主节点故障时发起自动故障转移。
    char *cluster_announce_ip; /* IP address to announce on cluster bus. */ // 通过集群总线向其他节点公布的 IP 地址。
    int cluster_announce_port; /* base port to announce on cluster bus. */ // 向其他集群节点公布的客户端连接端口。
    int cluster_announce_bus_port; /* bus port to announce on cluster bus. */ // 向其他集群节点公布的集群总线端口。
    int cluster_module_flags; /* Set of flags that Redis modules are able
                             to set in order to suppress certain
                             native Redis Cluster features. Check the
                             REDISMODULE_CLUSTER_FLAG_*. */
    // 模块用于禁用部分内置集群行为的标志,取值见 REDISMODULE_CLUSTER_FLAG_*。
    int cluster_allow_reads_when_down; /* Are reads allowed when the cluster is down? */ // 集群处于不可用状态时,是否允许读取本节点负责的键。
    int cluster_config_file_lock_fd; /* cluster config fd, will be flock */ // 集群配置文件锁 fd,将使用 flock

    /* Scripting */
    // 脚本
    lua_State *lua; /* The Lua interpreter. We use just one for all clients */ // 所有客户端共享的 Lua 解释器。
    client *lua_client; /* The "fake client" to query Redis from Lua */ // Lua 脚本调用 Redis 命令时复用的内部客户端。
    client *lua_caller; /* The client running EVAL right now, or NULL */ // 正在运行 EVAL 的客户端,或者为 NULL
    char *lua_cur_script; /* SHA1 of the script currently running, or NULL */ // 当前运行的脚本的 SHA1,或者为 NULL
    dict *lua_scripts; /* A dictionary of SHA1 -> Lua scripts */ // 脚本字典
    unsigned long long lua_scripts_mem; /* Cached scripts' memory + oh */ // 脚本缓存内存及相关对象开销。
    mstime_t lua_time_limit; /* Script timeout in milliseconds */ // Lua 脚本执行时间限制,单位为毫秒;超时后进入响应受限状态。
    mstime_t lua_time_start; /* Start time of script, milliseconds time */ // 脚本开始时间,毫秒时间
    int lua_write_dirty; /* True if a write command was called during the execution of the current script. */
    // 如果在执行当前脚本期间调用了写命令,则为真
    int lua_random_dirty; /* True if a random command was called during the execution of the current script. */
    // 如果在执行当前脚本期间调用了随机命令,则为真
    int lua_replicate_commands; /* True if we are doing single commands repl. */ // 如果执行单个命令复制,则为真
    int lua_multi_emitted; /* True if we already propagated MULTI. */ // 如果我们已经传播了 MULTI,则为真
    int lua_repl; /* Script replication flags for redis.set_repl(). */ // 脚本复制标志
    int lua_timedout; /* True if we reached the time limit for script execution. */ // 如果达到脚本执行时间限制,则为真
    int lua_kill; /* Kill the script if true. */ // 如果为真,则杀死脚本
    int lua_always_replicate_commands; /* Default replication type. */ // 默认复制类型
    int lua_oom; /* OOM detected when script start? */ // 脚本开始执行时是否已超过内存限制。

    /* Lazy free */
    // 惰性释放
    int lazyfree_lazy_eviction; // 内存淘汰时是否采用惰性释放。
    int lazyfree_lazy_expire; // 删除过期键时是否采用惰性释放。
    int lazyfree_lazy_server_del; // 服务器内部隐式删除键时是否采用惰性释放。
    int lazyfree_lazy_user_del; // DEL 命令是否采用惰性释放。
    int lazyfree_lazy_user_flush; // FLUSHDB/FLUSHALL 未指定模式时是否默认异步释放。

    /* Latency monitor */
    // 延迟监视器
    long long latency_monitor_threshold; // 延迟监视阈值
    dict *latency_events; // 延迟事件字典

    /* ACLs */
    char *acl_filename; /* ACL Users file. NULL if not configured. */ // ACL 用户配置文件路径,未配置时为 NULL。
    unsigned long acllog_max_len; /* Maximum length of the ACL LOG list. */ // ACL 日志列表的最大长度。
    sds requirepass; /* Remember the cleartext password set with
                        the old "requirepass" directive for
                        backward compatibility with Redis <= 5. */
    // 保留旧版 requirepass 配置的明文值,用于兼容配置接口。
    int acl_pubusub_default; /* Default ACL pub/sub channels flag */ // 新建或重置 ACL 用户时采用的默认频道访问权限。

    /* Assert & bug reporting */
    // 断言和错误报告
    int watchdog_period; /* Software watchdog period in ms. 0 = off */

    /* System hardware info */
    // 系统硬件信息
    size_t system_memory_size; /* Total memory in system as reported by OS */ // 系统内存大小

    /* TLS Configuration */
    // TLS 配置
    int tls_cluster; // 集群总线通信是否使用 TLS。
    int tls_replication; // 连接主节点进行复制时是否使用 TLS。
    int tls_auth_clients; // TLS 客户端证书验证策略。
    redisTLSContextConfig tls_ctx_config; // TLS 上下文配置

    /* cpu affinity */
    // CPU 亲和性
    char *server_cpulist;
            /* cpu affinity list of redis server main/io thread. */ // Redis 服务器主线程/IO 线程的 CPU 亲和性列表。
    char *bio_cpulist; /* cpu affinity list of bio thread. */ // BIO 线程的 CPU 亲和性列表。
    char *aof_rewrite_cpulist; /* cpu affinity list of aof rewrite process. */ // AOF 重写进程的 CPU 亲和性列表。
    char *bgsave_cpulist; /* cpu affinity list of bgsave process. */ // BGSave 进程的 CPU 亲和性列表。

    /* Sentinel config */
    // 哨兵配置
    struct sentinelConfig *sentinel_config; /* sentinel config to load at startup time. */ // 要加载的哨兵配置

    /* Coordinate failover info */
    // 协调故障转移信息
    mstime_t failover_end_time; /* Deadline for failover command. */ // FAILOVER 操作截止时间,Unix 毫秒时间戳。
    int force_failover;
            /* If true then failover will be foreced at the deadline, otherwise failover is aborted. */ // 到达截止时间仍未追平时,是否强制切换;否则取消切换。
    char *target_replica_host;
            /* Failover target host. If null during a failover then any replica can be used. */ // 故障转移目标主机。如果为空,则任何副本都可以使用。
    int target_replica_port; /* Failover target port */ // 故障转移目标端口
    int failover_state; /* Failover state */ // 故障转移状态
};

#define MAX_KEYS_BUFFER 256
/* A result structure for the various getkeys function calls. It lists the
 * keys as indices to the provided argv.
 */
typedef struct {
    int keysbuf[MAX_KEYS_BUFFER]; /* Pre-allocated buffer, to save heap allocations */ // 内嵌键索引缓冲区,避免少量键场景下的堆分配。
    int *keys; /* Key indices array, points to keysbuf or heap */ // 键索引数组,指向keysbuf或堆
    int numkeys; /* Number of key indices return */ // 返回的键索引数量
    int size; /* Available array size */ // 可用数组大小
} getKeysResult;
#define GETKEYS_RESULT_INIT {{0}, NULL, 0, MAX_KEYS_BUFFER}

// 命令处理函数
typedef void redisCommandProc(client *c);
// 使用函数确定命令行中的键参数。用于 Redis 集群重定向。
typedef int redisGetKeysProc(struct redisCommand *cmd, robj **argv, int argc, getKeysResult *result);

/*
 * redis将所有的命令都封装为一个redisCommand结构体,
 * 并用函数指针redisCommandProc指向该命令的具体逻辑
 */
struct redisCommand {
    char *name; // 命令名字
    redisCommandProc *proc; // 指向该命令的具体逻辑,命令处理函数
    int arity; // 参数数量包含命令名:正数表示固定数量,负数表示至少需要 -arity 个参数。
               // 例如 GET 的 arity 为 2,SET 的 arity 为 -3。
    char *sflags; /* Space-separated command flags and ACL categories. */ // 空格分隔的命令标志,例如 read-only、write、fast 和 ACL 分类。
    uint64_t flags;
            /* The actual flags, obtained from the 'sflags' field. */ // 命令的二进制标志,服务器启动时解析sflags字段生成。
    /* Use a function to determine keys arguments in a command line.
     * Used for Redis Cluster redirect. */
    // 使用函数确定命令行中的键参数。用于 Redis 集群重定向。
    redisGetKeysProc *getkeys_proc;
    /* Fixed-position key arguments in the command vector. */
    // 固定位置键参数的提取规则;动态位置由 getkeys_proc 处理。
    int firstkey; /* The first argument that's a key (0 = no keys) */ // 第一个键在 argv 中的索引,0 表示无固定位置键。
    int lastkey; /* The last argument that's a key */ // 最后一个键的索引;负数表示相对 argc 的位置。
    int keystep; /* The step between first and last key */ // 相邻键参数的索引步长。
    long long microseconds, calls, rejected_calls,
            failed_calls; // 累计执行耗时(微秒)、执行次数、执行前拒绝次数和执行报错次数;可重置。
    int id; /* Command ID. This is a progressive ID starting from 0 that
                   is assigned at runtime, and is used in order to check
                   ACLs. A connection is able to execute a given command if
                   the user associated to the connection has this command
                   bit set in the bitmap of allowed commands. */
    // 运行时分配的递增命令 ID,用于索引用户的命令权限位图。
};
void addReplyCommandCategories(client *c, struct redisCommand *cmd);

// 按错误名称统计回复次数,对应 INFO errorstats 的计数。
struct redisError {
    long long count;
};

// 函数符号记录,保存函数名称和地址。
struct redisFunctionSym {
    char *name;
    unsigned long pointer;
};

// SORT 的待排序元素,按数值 score 或字符串比较对象 cmpobj 排序。
typedef struct _redisSortObject {
    robj *obj;
    union {
        double score;
        robj *cmpobj;
    } u;
} redisSortObject;

// SORT 的附加操作,保存操作类型及用于查找外部键的模式。
typedef struct _redisSortOperation {
    int type;
    robj *pattern;
} redisSortOperation;

/* Structure to hold hash iteration abstraction. Note that iteration over
 * hashes involves both fields and values. Because it is possible that
 * not both are required, store pointers in the iterator to avoid
 * unnecessary memory allocation for fields/values. */
// 统一遍历 ziplist 和哈希表编码的字段/值,保存当前位置以避免不必要的对象分配。
typedef struct {
    robj *subject;
    int encoding;

    unsigned char *fptr, *vptr;

    dictIterator *di;
    dictEntry *de;
} hashTypeIterator;

/*-----------------------------------------------------------------------------
 * Extern declarations  外部声明 在其他文件中定义的全局变量,当前文件仅使用其定义,而不负责分配内存。
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
extern dictType commandTableDictType;
extern dictType keylistDictType;
extern dictType migrateCacheDictType;

/* Modules */
// 模块
// 初始化模块 API、模块字典、阻塞客户端队列及相关同步设施。
void moduleInitModulesSystem(void);
void moduleInitModulesSystemLast(void);
// 加载指定路径的模块并传入初始化参数;成功返回 C_OK,失败返回 C_ERR。
int moduleLoad(const char *path, void **argv, int argc);
void moduleLoadFromQueue(void);
// 调用模块的键位置查询接口,将参数索引写入 result,并返回键数量。
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
// 获取当前 Unix 时间戳:ustime 返回微秒,mstime 返回毫秒。
long long ustime(void);
long long mstime(void);

uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l);
void exitFromChild(int retcode);

size_t redisPopcount(void *s, long count);
int redisSetProcTitle(char *title);
int validateProcTitleTemplate(const char *template);
int redisCommunicateSystemd(const char *sd_notify_msg);
void redisSetCpuAffinity(const char *cpulist);
int redisIsSupervised(int mode);
void init_setproctitle_replacement(int argc, char **argv);

/* networking.c -- Networking and Client related operations */
// networking.c -- 网络和客户端相关操作
client *createClient(connection *conn);
void closeTimedoutClients(void);
void freeClient(client *c); // 释放客户端
void freeClientAsync(client *c); // 将客户端加入延迟关闭队列,由主线程安全释放。
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

void readQueryFromClient(connection *conn); // 从连接读取请求数据并推进输入缓冲区处理。
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

void getClientsMaxBuffers(unsigned long *longest_output_list, unsigned long *biggest_input_buffer);

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
void readOOMScoreAdj(void);
void checkTcpBacklogSettings(void);
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
void addReplyErrorFormat(client *c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void addReplyStatusFormat(client *c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#else
void addReplyErrorFormat(client *c, const char *fmt, ...);
void addReplyStatusFormat(client *c, const char *fmt, ...);
#endif

/* Client side caching (tracking mode) */
// 客户端缓存(跟踪模式)
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
// 通用持久化及加载状态管理。
void startLoadingFile(FILE *fp, char *filename, int rdbflags);
void startLoading(size_t size, int rdbflags);
void loadingProgress(off_t pos);
void stopLoading(int success);
void startSaving(int rdbflags);
void stopSaving(int success);
int allPersistenceDisabled(void);

#define DISK_ERROR_TYPE_AOF 1 /* Don't accept writes: AOF errors. */
#define DISK_ERROR_TYPE_RDB 2 /* Don't accept writes: RDB errors. */
#define DISK_ERROR_TYPE_NONE 0 /* No problems, we can accept writes. */

int writeCommandsDeniedByDiskError(void);

/* RDB persistence */
// RDB 持久化
void killRDBChild(void);
int bg_unlink(const char *filename);

/* AOF persistence */
// AOF 持久化
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
// 子进程状态信息。
void openChildInfoPipe(void);
void closeChildInfoPipe(void);
void sendChildInfo(int process_type, int on_exit, size_t cow_size);
void receiveChildInfo(void);

/* Fork helpers */
// 子进程创建和状态管理。
const char *strChildType(int type);
int redisFork(int type);
int hasActiveChildProcess(void);
void resetChildState(void);
int isMutuallyExclusiveChildType(int type);
void sendChildCOWInfo(int ptype, int on_exit, char *pname);

/* Input flags. */
#define ZADD_NONE 0
#define ZADD_INCR (1 << 0) /* Increment the score instead of setting it. */
#define ZADD_NX (1 << 1) /* 仅添加不存在的成员,不更新已有成员。 */
#define ZADD_XX (1 << 2) /* Only touch elements already existing. */
#define ZADD_GT (1 << 7) /* Only update existing when new scores are higher. */
#define ZADD_LT (1 << 8) /* Only update existing when new scores are lower. */

/* Output flags. */
#define ZADD_NOP (1 << 3) /* Operation not performed because of conditionals.*/
#define ZADD_NAN (1 << 4) /* 分数计算结果为 NaN,操作失败。 */
#define ZADD_ADDED (1 << 5) /* The element was new and was added. */
#define ZADD_UPDATED (1 << 6) /* The element already existed, score updated. */

/* Flags only used by the ZADD command but not by zsetAdd() API: */
#define ZADD_CH (1 << 16) /* Return num of elements added or updated. */

/* Core functions */
// 核心函数
// 校验已解析的命令及执行条件,并执行命令或加入事务队列。
// 返回 C_OK 表示客户端仍可继续处理,C_ERR 表示调用方不可继续使用该客户端。
int processCommand(client *c);
int processPendingCommandsAndResetClient(client *c);
void setupSignalHandlers(void);
void removeSignalHandlers(void);
struct redisCommand *lookupCommand(sds name);
struct redisCommand *lookupCommandByCString(const char *s);
struct redisCommand *lookupCommandOrOriginal(sds name);
// 执行命令
void call(client *c, int flags);
void propagate(struct redisCommand *cmd, int dbid, robj **argv, int argc, int flags);
void alsoPropagate(struct redisCommand *cmd, int dbid, robj **argv, int argc, int target);
void redisOpArrayInit(redisOpArray *oa);
int redisOpArrayAppend(redisOpArray *oa, struct redisCommand *cmd, int dbid, robj **argv, int argc, int target);
void redisOpArrayFree(redisOpArray *oa);
void forceCommandPropagation(client *c, int flags);
void preventCommandPropagation(client *c);
void preventCommandAOF(client *c);
void preventCommandReplication(client *c);
int prepareForShutdown(int flags);

#ifdef __GNUC__
void serverLog(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
void serverLog(int level, const char *fmt, ...);
#endif
void serverLogRaw(int level, const char *msg);
void serverLogFromHandler(int level, const char *msg);
// 创建Pid文件
void createPidFile(void);
// 将服务器转为后台守护进程。
void daemonize(void);
// 打印启动日志
void printStartLog(int argc, char **argv);
// 打印版本
void version(void);
// 打印帮助
void usage(void);
// 打印Redis ASCII艺术Logo
void redisAsciiArt(void);
int checkForSentinelMode(int argc, char **argv);
void parseArgv(int argc, char **argv);
void redisOutOfMemoryHandler(size_t allocation_size);
void redis_test(int argc, char *const *argv);
void initServerOther(void);
void updateDictResizePolicy(void);
// 判断哈希表是否过于稀疏,需要收缩容量。
int htNeedsResize(dict *dict);
/* 收缩指定数据库的键空间和过期字典。 */
void tryResizeHashTables(int dbid);
/* 为指定数据库推进渐进式 rehash,返回是否执行了迁移。 */
int incrementallyRehash(int dbid);
// 初始化或填充命令表。
void populateCommandTable(void);

// 重置服务统计
void resetServerStats(void);
// 重置命令统计
void resetCommandTableStats(void);
// 重置错误统计
void resetErrorTableStats(void);
void adjustOpenFilesLimit(void);
#ifdef __linux__
int linuxOvercommitMemoryValue(void);
void linuxMemoryWarnings(void);
int linuxMadvFreeForkBugCheck(void);
#endif

// 统计错误
void incrementErrorCount(const char *fullerr, size_t namelen);
// 关闭监听的socket
void closeListeningSockets(int unlink_unix_socket);
void updateCachedTime(int update_daylight_info);

void activeDefragCycle(void);

unsigned int getLRUClock(void);

unsigned int LRU_CLOCK(void);

const char *evictPolicyToString(void);

// 汇总内存占用和碎片统计,结果由 freeMemoryOverheadData() 释放。
struct redisMemOverhead *getMemoryOverheadData(void);
// 释放内存统计结构及其数据库统计数组。
void freeMemoryOverheadData(struct redisMemOverhead *mh);
void checkChildrenDone(void);
/* 启动阶段的模式初始化、检查工具分派和 maxmemory 告警。 */
void checkAndInitSentenel(void);
void checkRdbOrAof(int argc, char **argv);
void checkMaxmemory(void);
int setOOMScoreAdj(int process_class);
void rejectCommandFormat(client *c, const char *fmt, ...);
void *activeDefragAlloc(void *ptr);
robj *activeDefragStringOb(robj *ob, long *defragged);

// 服务器重启选项,可按位组合;NONE 表示无附加选项,仍会执行重启。
// GRACEFULLY 表示先执行正常关闭流程,CONFIG_REWRITE 表示重启前重写配置。
#define RESTART_SERVER_NONE 0
#define RESTART_SERVER_GRACEFULLY (1 << 0) /* Do proper shutdown. */
#define RESTART_SERVER_CONFIG_REWRITE (1 << 1) /* CONFIG REWRITE before restart.*/
int restartServer(int flags, mstime_t delay);

/* Keyspace events notification */
// 键空间事件通知。
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
int keyIsExpired(redisDb *db, robj *key);
// 移除键的过期时间,不删除键值。
int removeExpire(redisDb *db, robj *key);
void propagateExpire(redisDb *db, robj *key, int lazy);
int expireIfNeeded(redisDb *db, robj *key);
// 获取键的 Unix 毫秒过期时间,未设置时返回 -1。
long long getExpire(redisDb *db, robj *key);
// 设置键的过期时刻,when 为 Unix 毫秒时间戳。
void setExpire(client *c, redisDb *db, robj *key, long long when);
// 判断给定时间是否已过期,并考虑加载和主从角色。
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
int objectSetLRUOrLFU(robj *val, long long lfu_freq, long long lru_idle, long long lru_clock, int lru_multiplier);
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

#define EMPTYDB_NO_FLAGS 0 /* No flags. */
#define EMPTYDB_ASYNC (1 << 0) /* Reclaim memory in another thread. */
long long emptyDb(int dbnum, int flags, void(callback)(void *));
long long emptyDbStructure(redisDb *dbarray, int dbnum, int async, void(callback)(void *));
void flushAllDataAndResetRDB(int flags);
long long dbTotalServerKeyCount(void);

/* Declare database backup that include redis main DBs and slots to keys map.
 * Definition is in db.c. We can't define it here since we define CLUSTER_SLOTS
 * in cluster.h. */
// 数据库备份包含数据库数组和槽到键的映射。
// 具体结构定义在 db.c,避免在此头文件引入 cluster.h 中的槽数量常量。
typedef struct dbBackup dbBackup;
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
// 阻塞客户端管理。
void processUnblockedClients(void);
void blockClient(client *c, int btype);
void unblockClient(client *c);
void queueClientForReprocessing(client *c);
void replyToBlockedClientTimedOut(client *c);
int getTimeoutFromObjectOrReply(client *c, robj *object, mstime_t *timeout, int unit);
void disconnectAllBlockedClients(void);
void handleClientsBlockedOnKeys(void);
void signalKeyAsReady(redisDb *db, robj *key, int type);
void blockForKeys(client *c, int btype, robj **keys, int numkeys, mstime_t timeout, robj *target,
                  struct listPos *listpos, streamID *ids);
void updateStatsOnUnblock(client *c, long blocked_us, long reply_us);
void unblockClientWaitingData(client *c);

/* timeout.c -- Blocked clients timeout and connections timeout. */
// timeout.c -- 阻塞等待超时和连接空闲超时。
void addClientToTimeoutTable(client *c); // 将有限时等待的客户端加入超时索引。
void removeClientFromTimeoutTable(client *c); // 从阻塞超时索引中移除客户端。
void handleBlockedClientsTimeout(void);
int clientsCronHandleTimeout(client *c, mstime_t now_ms);

/* expire.c -- Handling of expired keys */
// expire.c -- 键过期处理。
void activeExpireCycle(int type);
void expireSlaveKeys(void);
void rememberSlaveKeyWithExpire(redisDb *db, robj *key);
void flushSlaveKeysWithExpireList(void);
size_t getSlaveKeyWithExpireCount(void);

/* Keys hashing / comparison functions for dict.c hash tables. */
// dict.c 哈希表使用的键哈希、比较及释放函数。
uint64_t dictSdsHash(const void *key);
int dictSdsKeyCompare(void *privdata, const void *key1, const void *key2);
void dictSdsDestructor(void *privdata, void *val);

/* Git SHA1 */
char *redisGitSHA1(void);
char *redisGitDirty(void);
uint64_t redisBuildId(void);
char *redisBuildIdString(void);

void serverLogObjectDebugInfo(const robj *o);
void sigsegvHandler(int sig, siginfo_t *info, void *secret);

/* 读取周期采样的瞬时指标,供 INFO 统计使用。 */
long long getInstantaneousMetric(int metric);
void trackInstantaneousMetric(int metric, long long current_reading);
/* 获取近期客户端输入、输出缓冲区的峰值字节数。 */
void getExpansiveClientsInfo(size_t *in_usage, size_t *out_usage);
/* 分批执行客户端超时检查、缓冲区整理和内存统计。 */
void clientsCron(void);

sds genRedisInfoString(const char *section);
sds genModulesInfoString(sds info);

void enableWatchdog(int period);
void disableWatchdog(void);
void watchdogScheduleSignal(int period);
void serverLogHexDump(int level, char *descr, void *value, size_t len);
void mixDigest(unsigned char *digest, void *ptr, size_t len);
void xorDigest(unsigned char *digest, void *ptr, size_t len);
void debugDelay(int usec);

// 内存测试
void memtest(size_t megabytes, int passes);
int memtest_preserving_test(unsigned long *m, size_t bytes, int passes);
int populateCommandTableParseFlags(struct redisCommand *c, char *strflags);

void killIOThreads(void);
void killThreads(void);
void makeThreadKillable(void);
void InitServerLast(void);
int iAmMaster(void);

/* Debugging stuff */
void _serverAssertWithInfo(const client *c, const robj *o, const char *estr, const char *file, int line);
void _serverAssert(const char *estr, const char *file, int line);

// 创建常用回复、命令参数和整数的共享对象。
void createSharedObjects(void);
void initServer(void);
void initServerConfig(void);
int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData);
void databasesCron(void);
void cronUpdateMemoryStats(void);
void beforeSleep(struct aeEventLoop *eventLoop);
void afterSleep(struct aeEventLoop *eventLoop);
void addReplyCommandCategories(client *c, struct redisCommand *cmd);
#endif
