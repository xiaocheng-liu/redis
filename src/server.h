#ifndef SERVER_H
#define SERVER_H

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

#include "macro.h"
#include "util.h"                              /* Misc functions useful in many places 工具函数*/
#include "latency.h" /* Latency monitor API */ // 延迟监视器API,通过包含此头文件，程序可以调用其中定义的函数和宏来监测系统的延迟情况。
#include "sparkline.h" /* ASCII graphs API */  // ASCII 图表的 API。通过包含这个头文件，程序可以调用其中定义的函数来绘制简单的文本图表。
#include "version.h"
#include "fmacros.h"
#include "config.h"
#include "solarisfixes.h"
#include "rio.h"
#include "atomicvar.h"

#include "ae.h"                                 /* Event driven programming library 事件驱动库*/
#include "anet.h" /* Networking the easy way */ // 网络编程

#include "sds.h"    /* Dynamic safe strings 动态安全字符串*/
#include "dict.h"   /* Hash tables 哈希表*/
#include "adlist.h" /* Linked lists 链表 */
#include "t_list.h"
#include "t_zset.h"
#include "t_stream.h" /* Stream data type header file. */                                // 流数据类型头文件。
#include "zmalloc.h" /* total memory usage aware version of malloc/free */               // 该头文件提供了内存分配函数的替代版本（如 malloc 和 free）
#include "ziplist.h"                                                                     /* Compact list data structure 压缩列表数据结构*/
#include "intset.h"                                                                      /* Compact integer set structure 压缩整型结构*/
#include "quicklist.h" /* Lists are encoded as linked lists of N-elements flat arrays */ // 列表被编码为包含n个元素的平面数组的链表
#include "rax.h" /* Radix tree */                                                        // 基数树

#include "connection.h" /* Connection abstraction */      // 连接抽象的接口或实现。通过包含此头文件，程序可以使用其中定义的与连接相关的功能和数据结构。
#include "redismodule.h" /* Redis modules API defines. */ // 模块开发接口。这为后续编写 Redis 模块提供了必要的函数和数据结构支持。

/* Following includes allow test functions to be called from Redis main() */
// 以下内容包括允许从 Redis main（） 调用测试函数
#include "zipmap.h"
#include "sha1.h"
#include "endianconv.h"
#include "crc64.h"
#include "evict.h"
#include "geo.h"
#include "rdb.h"
#include "tls.h"
#include "pubsub.h"
#include "multi.h"
#include "server_cammand_define.h"
#include "acl.h"

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

// 这段代码定义了一个名为 robj 的结构体，用于表示 Redis 中的对象
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
// Redis数据库表示。已识别多个数据库从0（默认数据库）到配置的最大值的整数数据库。
// 数据库编号是结构中的"id"字段。
typedef struct redisDb
{
    dict *dict; /* The keyspace for this DB */                                              // 保存着数据库中的所有键值对数据, 这个属性也被称为键空间（key space）
    dict *expires; /* Timeout of keys with a timeout set */                                 // 保存key对应的过期时间
    dict *blocking_keys; /* Keys with clients waiting for data (BLPOP)*/                    // key对应的等待数据的client列表 (BLPOP)
    dict *ready_keys; /* Blocked keys that received a PUSH */                               // 收到推送的被阻止密钥
    dict *watched_keys; /* WATCHED keys for MULTI/EXEC */                                   // CAS 存储监听key的clients
    int id; /* Database ID */                                                               // 保存着数据库以整数表示的号码
    long long avg_ttl; /* Average TTL, just for stats */                                    // 存储的数据库对象的平均ttl(time to live)，用于统计
    unsigned long expires_cursor; /* Cursor of the active expire cycle. */                  // 过期删除过程中的下标
    list *defrag_later; /* List of key names to attempt to defrag one by one, gradually. */ // 要尝试逐个碎片整理的键名称列表，逐渐。
} redisDb;

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
// 定义了一个名为 readyList 的结构体，包含两个成员：redisDb *db 和 robj *key。
// 该结构体用于存储与 Redis 数据库和键对象相关的引用。
typedef struct readyList
{
    redisDb *db;
    robj *key;
} readyList;

// 用于存储 Redis 用户的权限和相关信息。
typedef struct user
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
    list *patterns; /* A list of allowed key patterns. If this field is NULL
                       the user cannot mention any key in a command, unless
                       the flag ALLKEYS is set in the user. */
    // 允许的 Pub/Sub 频道模式列表。
    list *channels; /* A list of allowed Pub/Sub channel patterns. If this
                       field is NULL the user cannot mention any channel in a
                       `PUBLISH` or [P][UNSUSBSCRIBE] command, unless the flag
                       ALLCHANNELS is set in the user. */
} user;

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

    // 定义了一个指向用户结构体的指针user，用于表示与当前连接关联的用户。
    // 如果 user 指针为 NULL，则表示该连接具有管理员权限，可以执行任何操作
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

typedef struct saveparam
{
    time_t seconds; // 用于存储时间（以秒为单位）
    int changes;    // 用于记录变化次数
} saveparam;

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

// 这段代码定义了一个名为 sharedObjectsStruct 的结构体，用于存储 Redis 中常用的对象和字符串。
// 这些对象包括各种错误信息、命令关键字、常用整数对象等，旨在减少内存分配次数并提高性能。
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
        *time,
        *pxat, // 绝对过期时间标志
        *px,   // 相对过期时间标志
        *retrycount, *force, *justid,
        *lastid, *ping, *setid, *keepttl, *load, *createconsumer,
        *getack, *special_asterick, *special_equals, *default_username,
        *select[PROTO_SHARED_SELECT_CMDS],
        *integers[OBJ_SHARED_INTEGERS],    // 共享整数对象
        *mbulkhdr[OBJ_SHARED_BULKHDR_LEN], /* "*<value>\r\n" */
        *bulkhdr[OBJ_SHARED_BULKHDR_LEN];  /* "$<value>\r\n" */
    sds minstring, maxstring;
};

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
// 这段代码定义了一个名为 redisOpArray 的结构体，用于存储操作数组。
// 其中 ops 是指向 redisOp 类型的指针，表示操作数组；numops 是整数类型，表示操作数组中元素的数量。
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
 * Global server state
 *----------------------------------------------------------------------------*/
struct clusterState;

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
    // 这段代码定义了一个指向字符的指针变量 pidfile，用于存储进程ID文件的路径
    char *pidfile; /* PID file path */                                                   // pidfile路径
    int arch_bits; /* 32 or 64 depending on sizeof(long) */                              // 32或者64取决于long的大小
    int cronloops; /* Number of times the cron function run */                           // cron 函数运行的次数
    char runid[CONFIG_RUN_ID_SIZE + 1]; /* ID always different at every exec. */         // 当前redis实例的 runid
    int sentinel_mode; /* True if this instance is a Sentinel. */                        // 如果此实例是哨兵，则为 true。
    size_t initial_memory_usage; /* Bytes used after initialization. */                  // 初始化后使用的字节数。
    int always_show_logo; /* Show logo even for non-stdout logging. */                   // 始终显示logo
    int in_eval; /* Are we inside EVAL? */                                               // 我们在EVAL里面吗？
    int in_exec; /* Are we inside EXEC? */                                               // 我们在EXEC里面吗？
    int propagate_in_transaction; /* Make sure we don't propagate nested MULTI/EXEC */   // 确保我们不会传播嵌套的 MULTIEXEC
    char *ignore_warnings; /* Config: warnings that should be ignored. */                // 配置：应忽略的警告。
    int client_pause_in_transaction; /* Was a client pause executed during this Exec? */ // 在此执行期间是否执行了客户端暂停？

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
    rax *clients_index; /* Active clients dictionary by client ID. */         // 活动客户端字典按客户端 ID
    pause_type client_pause_type;                                             /* True if clients are currently paused */
    list *paused_clients;                                                     /* List of pause clients */
    mstime_t client_pause_end_time;                                           /* Time when we undo clients_paused */
    char neterr[ANET_ERR_LEN];                                                /* Error buffer for anet.c */
    dict *migrate_cached_sockets;
    /* MIGRATE cached sockets */
    // 这段代码定义了一个名为 next_client_id 的全局变量，类型为 redisAtomic uint64_t。它用于存储下一个客户端的唯一ID，并且该ID是递增的。
    redisAtomic uint64_t next_client_id;                           /* Next client unique ID. Incremental. */
    int protected_mode;                                            /* Don't accept external connections. */
    int gopher_enabled;                                            /* If true the server will reply to gopher
                                                                         queries. Will still serve RESP2 queries. */
    int io_threads_num; /* Number of IO threads to use. */         // 要使用的 IO 线程数。
    int io_threads_do_reads; /* Read and parse from IO threads? */ // 从 IO 线程读取和解析？
    int io_threads_active; /* Is IO threads currently active? */   // IO 线程当前是否处于活动状态？
    long long events_processed_while_blocked;                      /* processEventsWhileBlocked() */

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
    int verbosity; /* Loglevel in redis.conf */                            // redis.conf配置的日志级别
    int maxidletime; /* Client timeout in seconds */                       // 客户端超时时间
    int tcpkeepalive; /* Set SO_KEEPALIVE if non-zero. */                  // SO_KEEPALIVE选项
    int active_expire_enabled; /* Can be disabled for testing purposes. */ // 是否启用过期功能
    int active_expire_effort; /* From 1 (default) to 10, active effort. */ // 过期的效率
    int active_defrag_enabled;
    int sanitize_dump_payload;                      /* Enables deep sanitization for ziplist and listpack in RDB and RESTORE. */
    int skip_checksum_validation;                   /* Disables checksum validateion for RDB and RESTORE payload. */
    int jemalloc_bg_thread;                         /* Enable jemalloc background thread */
    size_t active_defrag_ignore_bytes;              /* minimum amount of fragmentation waste to start active defrag */
    int active_defrag_threshold_lower;              /* minimum percentage of fragmentation to start active defrag */
    int active_defrag_threshold_upper;              /* maximum percentage of fragmentation at which we use maximum effort */
    int active_defrag_cycle_min;                    /* minimal effort for defrag in CPU percentage */
    int active_defrag_cycle_max;                    /* maximal effort for defrag in CPU percentage */
    unsigned long active_defrag_max_scan_fields;    /* maximum number of fields of set/hash/zset/list to process from within the main dict scan */
    size_t client_max_querybuf_len;                 /* Limit for client query buffer length */
    int dbnum; /* Total number of configured DBs */ // 数据库数量
    // 用于表示是否处于监督状态。值为1时表示监督状态，值为0时表示非监督状态。
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

    /* RDB persistence */                                                                                       // RDB 持久性
    long long dirty;                                                                                            // RDB持久化之后数据有变化，可以看到所有redis写命令都会执行server.dirty++
    long long dirty_before_bgsave; /* Used to restore dirty on failed BGSAVE */                                 // 记录上次RDB保存 dirty 的值
    saveparam *saveparams; /* Save points array for RDB */                                                      // RDB的保存点数组
    int saveparamslen; /* Number of saving points */                                                            // RDB保存点的数量
    char *rdb_filename; /* Name of RDB file */                                                                  // RDB文件名
    int rdb_compression; /* Use compression in RDB? */                                                          // 是否压缩RDB文件
    int rdb_checksum; /* Use RDB checksum? */                                                                   // 是否使用RDB校验
    int rdb_del_sync_files; /* Remove RDB files used only for SYNC if the instance does not use persistence. */ // 删除仅用于同步的RDB文件，如果实例不使用持久化

    time_t lastsave; /* Unix time of last successful save */                                               // 上次成功保存的 Unix 时间
    time_t lastbgsave_try; /* Unix time of last attempted bgsave */                                        // 记录最后一次尝试RDB保存的 Unix 时间
    time_t rdb_save_time_last; /* Time used by last RDB save run. */                                       // 记录最后一次RDB保存的时间
    time_t rdb_save_time_start; /* Current RDB save start time. */                                         // 记录当前的RDB保存的时间
    int rdb_bgsave_scheduled; /* BGSAVE when possible if true. */                                          // 如果为真，则尝试在可能的情况下进行RDB保存
    int rdb_child_type; /* Type of save by active child. */                                                // 活跃子进程保存类型
    int lastbgsave_status; /* C_OK or C_ERR */                                                             // 记录最后一次RDB保存的状态
    int stop_writes_on_bgsave_err; /* Don't allow writes if can't BGSAVE */                                // 如果bgsave失败，则停止写入
    int rdb_pipe_read; /* RDB pipe used to transfer the rdb data to the parent process in diskless repl.*/ // RDB管道，用于将RDB数据传输到父进程的磁盘less repl中

    int rdb_child_exit_pipe; /* Used by the diskless parent allow child exit. */ // 用于无盘环境下的父进程控制子进程的退出。
    connection **rdb_pipe_conns;                                                 /* Connections which are currently the */
    int rdb_pipe_numconns;                                                       /* target of diskless rdb fork child. */
    int rdb_pipe_numconns_writing;                                               /* Number of rdb conns with pending writes. */
    char *rdb_pipe_buff;                                                         /* In diskless replication, this buffer holds data */
    int rdb_pipe_bufflen;                                                        /* that was read from the the rdb pipe. */
    int rdb_key_save_delay;                                                      /* Delay in microseconds between keys while
                                                                                  * writing the RDB. (for testings). negative
                                                                                  * value means fractions of microsecons (on average). */
    int key_load_delay;                                                          /* Delay in microseconds between keys while
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
    int syslog_facility; /* Syslog facility */                // 用于表示系统日志设施的类型。
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
    unsigned int maxclients; /* Max number of simultaneous clients */                   // 最大允许的并发客户端数
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
    unsigned int blocked_clients; /* # of clients executing a blocking cmd.*/  // 记录当前执行阻塞命令的客户端数量。
    unsigned int blocked_clients_by_type[BLOCKED_NUM];                         // 按类型统计或记录被阻塞的客户端数量。
    list *unblocked_clients; /* list of clients to unblock before next loop */ // 存储未被阻塞的客户端列表。
    list *ready_keys;                                                          /* List of readyList structures for BLPOP & co */

    /* Client side caching. */
    // 客户端缓存。
    unsigned int tracking_clients; /* # of clients with tracking enabled.*/     // 记录启用跟踪功能的客户端数量。
    size_t tracking_table_max_keys; /* Max number of keys in tracking table. */ // 表示跟踪表中最大键值的数量。该变量通常用于限制或配置跟踪表的容量。
    /* Sort parameters - qsort_r() is only available under BSD so we
     * have to take this state global, in order to pass it to sortCompare() */
    // qsort_r() 函数仅在 BSD 系统中可用。为了在 sortCompare() 函数中使用排序参数，需要将状态设为全局变量。
    int sort_desc;
    int sort_alpha;
    int sort_bypattern;
    int sort_store;

    /* Zip structure config, see redis.conf for more information  */
    // zip结构配置，更多信息见redis.conf
    size_t hash_max_ziplist_entries;   // 用于存储哈希表中压缩列表的最大条目数。
    size_t hash_max_ziplist_value;     // 表示哈希表中使用压缩列表的最大值限制
    size_t set_max_intset_entries;     // 用于存储集合表中压缩列表的最大条目数。
    size_t zset_max_ziplist_entries;   // 用于存储有序集合表中压缩列表的最大条目数。
    size_t zset_max_ziplist_value;     // 表示有序集合表中使用压缩列表的最大值限制
    size_t hll_sparse_max_bytes;       // 用于存储 HyperLogLog 压缩列表的最大字节数。
    size_t stream_node_max_bytes;      // 用于存储流节点压缩列表的最大字节数。
    long long stream_node_max_entries; // 用于存储流节点压缩列表的最大条目数。

    /* List parameters */
    int list_max_ziplist_size; // 用于存储压缩列表的最大字节数。
    int list_compress_depth;   // 用于存储压缩列表的最大深度。

    /* time cache */
    redisAtomic time_t unixtime; /* Unix time sampled every cron cycle. */                                             // 缓存 unixtime
    time_t timezone; /* Cached timezone. As set by tzset(). */                                                         // 缓存时区
    int daylight_active; /* Currently in daylight saving time. */                                                      // 当前是否处于夏令时
    mstime_t mstime; /* 'unixtime' in milliseconds. */                                                                 // 毫秒级时间戳
    ustime_t ustime; /* 'unixtime' in microseconds. */                                                                 // 微秒级时间戳
    size_t blocking_op_nesting; /* Nesting level of blocking operation, used to reset blocked_last_cron. */            // 阻塞操作的嵌套级别，用于重置 blocked_last_cron
    long long blocked_last_cron; /* Indicate the mstime of the last time we did cron jobs from a blocking operation */ // 阻塞操作的最后一次cron执行的时间戳

    /* Pubsub */
    // 发布订阅
    dict *pubsub_channels; /* Map channels to list of subscribed clients */ // 将频道映射到已订阅客户端的列表(就是保存客户端和订阅的频道信息)
    list *pubsub_patterns; /* A list of pubsub_patterns */                  // pubsub_patterns列表
    dict *pubsub_patterns_dict; /* A dict of pubsub_patterns */             // pubsub_patterns字典
    int notify_keyspace_events;                                             /* Events to propagate via Pub/Sub. This is an
                                                                               xor of NOTIFY_... flags. */
                                                                            // 通过发布/订阅传播的事件，这是 xor NOTIFY_... 标志
    /* Cluster */
    // 集群
    int cluster_enabled; /* Is cluster enabled? */                                       // 是否启用了群集？
    mstime_t cluster_node_timeout; /* Cluster node timeout. */                           // 群集节点超时。
    char *cluster_configfile; /* Cluster auto-generated config file name. */             // 群集自动生成的配置文件名。
    struct clusterState *cluster; /* State of the cluster */                             // 群集的状态
    int cluster_migration_barrier; /* Cluster replicas migration barrier. */             // 群集副本迁移屏障。
    int cluster_slave_validity_factor; /* Slave max data age for failover. */            // 故障转移的从站最大数据期限。
    int cluster_require_full_coverage;                                                   /* If true, put the cluster down if
                                                                                            there is at least an uncovered slot.*/
                                                                                         // 如果为 true，则如果至少有一个未覆盖的插槽，请关闭群集。
    int cluster_slave_no_failover;                                                       /* Prevent slave from starting a failover
                                                                                         if the master is in failure state. */
                                                                                         // 防止从站启动故障转移，如果主站处于故障状态。
    char *cluster_announce_ip; /* IP address to announce on cluster bus. */              // 要在群集总线上宣布的 IP 地址。
    int cluster_announce_port; /* base port to announce on cluster bus. */               // 要在群集总线上宣布的基本端口。
    int cluster_announce_bus_port; /* bus port to announce on cluster bus. */            // 要在群集总线上公告的总线端口。
    int cluster_module_flags;                                                            /* Set of flags that Redis modules are able
                                                                                        to set in order to suppress certain
                                                                                        native Redis Cluster features. Check the
                                                                                        REDISMODULE_CLUSTER_FLAG_*. */
                                                                                         // Redis 模块能够设置的标志集，用于抑制某些本机 Redis 集群功能。检查REDISMODULE_CLUSTER_FLAG_。
    int cluster_allow_reads_when_down; /* Are reads allowed when the cluster is down? */ // 当集群关闭时是否允许读取？
    int cluster_config_file_lock_fd; /* cluster config fd, will be flock */              // 集群配置文件锁 fd，将使用 flock

    /* Scripting */
    // 脚本
    lua_State *lua; /* The Lua interpreter. We use just one for all clients */      // Lua解释器。我们为所有客户端使用一个
    client *lua_client; /* The "fake client" to query Redis from Lua */             // 从 Lua 查询 Redis 的"假客户"
    client *lua_caller; /* The client running EVAL right now, or NULL */            // 正在运行 EVAL 的客户端，或者为 NULL
    char *lua_cur_script; /* SHA1 of the script currently running, or NULL */       // 当前运行的脚本的 SHA1，或者为 NULL
    dict *lua_scripts; /* A dictionary of SHA1 -> Lua scripts */                    // 脚本字典
    unsigned long long lua_scripts_mem; /* Cached scripts' memory + oh */           // 缓存脚本的内存+oh
    mstime_t lua_time_limit; /* Script timeout in milliseconds */                   // 脚本超时
    mstime_t lua_time_start; /* Start time of script, milliseconds time */          // 脚本开始时间，毫秒时间
    int lua_write_dirty;                                                            /* True if a write command was called during the execution of the current script. */
                                                                                    // 如果在执行当前脚本期间调用了写命令，则为真
    int lua_random_dirty;                                                           /* True if a random command was called during the execution of the current script. */
                                                                                    // 如果在执行当前脚本期间调用了随机命令，则为真
    int lua_replicate_commands; /* True if we are doing single commands repl. */    // 如果执行单个命令复制，则为真
    int lua_multi_emitted; /* True if we already propagated MULTI. */               // 如果我们已经传播了 MULTI，则为真
    int lua_repl; /* Script replication flags for redis.set_repl(). */              // 脚本复制标志
    int lua_timedout; /* True if we reached the time limit for script execution. */ // 如果达到脚本执行时间限制，则为真
    int lua_kill; /* Kill the script if true. */                                    // 如果为真，则杀死脚本
    int lua_always_replicate_commands; /* Default replication type. */              // 默认复制类型
    int lua_oom; /* OOM detected when script start? */                              // 当脚本开始时，检测到 OOM？

    /* Lazy free */
    // 惰性释放
    int lazyfree_lazy_eviction;   // 懒惰释放的懒惰驱逐
    int lazyfree_lazy_expire;     // 懒惰释放的懒惰过期
    int lazyfree_lazy_server_del; // 懒惰释放的懒惰删除
    int lazyfree_lazy_user_del;   // 懒惰释放的懒惰用户删除
    int lazyfree_lazy_user_flush; // 懒惰释放的懒惰用户刷新

    /* Latency monitor */
    // 延迟监视器
    long long latency_monitor_threshold; // 延迟监视阈值
    dict *latency_events;                // 延迟事件字典

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
    size_t system_memory_size; /* Total memory in system as reported by OS */ // 系统内存大小

    /* TLS Configuration */
    // TLS 配置
    int tls_cluster;                      // 集群
    int tls_replication;                  // 复制
    int tls_auth_clients;                 // 客户端验证
    redisTLSContextConfig tls_ctx_config; // TLS 上下文配置

    /* cpu affinity */
    // CPU 关联性
    char *server_cpulist; /* cpu affinity list of redis server main/io thread. */ // Redis 服务器主线程/IO 线程的 CPU 关联性列表。
    char *bio_cpulist; /* cpu affinity list of bio thread. */                     // BIO 线程的 CPU 关联性列表。
    char *aof_rewrite_cpulist; /* cpu affinity list of aof rewrite process. */    // AOF 重写进程的 CPU 关联性列表。
    char *bgsave_cpulist; /* cpu affinity list of bgsave process. */              // BGSave 进程的 CPU 关联性列表。

    /* Sentinel config */
    // 哨兵配置
    struct sentinelConfig *sentinel_config; /* sentinel config to load at startup time. */ // 要加载的哨兵配置

    /* Coordinate failover info */
    // 协调故障转移信息
    mstime_t failover_end_time; /* Deadline for failover command. */                                                // 故障转移命令的截止日期。
    int force_failover; /* If true then failover will be foreced at the deadline, otherwise failover is aborted. */ // 如果为真，则在截止日期时强制故障转移，否则失败转移被取消。
    char *target_replica_host; /* Failover target host. If null during a failover then any replica can be used. */  // 故障转移目标主机。如果为空，则任何副本都可以使用。
    int target_replica_port; /* Failover target port */                                                             // 故障转移目标端口
    int failover_state; /* Failover state */                                                                        // 故障转移状态
};

#define MAX_KEYS_BUFFER 256

/* A result structure for the various getkeys function calls. It lists the
 * keys as indices to the provided argv.
 */
typedef struct
{
    int keysbuf[MAX_KEYS_BUFFER]; /* Pre-allocated buffer, to save heap allocations */ // 预先分配的缓冲区，以保存堆分配
    int *keys; /* Key indices array, points to keysbuf or heap */                      // 键索引数组，指向keysbuf或堆
    int numkeys; /* Number of key indices return */                                    // 返回的键索引数量
    int size; /* Available array size */                                               // 可用数组大小
} getKeysResult;
#define GETKEYS_RESULT_INIT \
    {                       \
        {0}, NULL, 0, MAX_KEYS_BUFFER}

// 命令处理函数
typedef void redisCommandProc(client *c);
// 使用函数确定命令行中的键参数。用于 Redis 集群重定向。
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
    long long microseconds, calls, rejected_calls, failed_calls;      // calls: 从服务器启动至今命令执行的次数，用于统计。 rejected_calls: 从服务器启动至今被拒绝的命令次数，用于统计。 failed_calls: 从服务器启动至今失败的命令次数，用于统计。
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

/*-----------------------------------------------------------------------------
 * Extern declarations  外部声明 在其他文件中定义的全局变量，当前文件仅使用其定义，而不负责分配内存。
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
// 执行命令
void call(client *c, int flags);
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
// 守护进程
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
// 该函数 htNeedsResize 用于判断哈希表是否需要调整大小。
int htNeedsResize(dict *dict);
// 初始化或填充命令表。
void populateCommandTable(void);

// 重置服务统计
void resetServerStats(void);
// 重置命令统计
void resetCommandTableStats(void);
// 重置错误统计
void resetErrorTableStats(void);
void adjustOpenFilesLimit(void);

// 统计错误
void incrementErrorCount(const char *fullerr, size_t namelen);
// 关闭监听的socket
void closeListeningSockets(int unlink_unix_socket);
void updateCachedTime(int update_daylight_info);

void activeDefragCycle(void);

unsigned int getLRUClock(void);

unsigned int LRU_CLOCK(void);

const char *evictPolicyToString(void);

// 此函数可能用于获取 Redis 内存开销的相关数据。
struct redisMemOverhead *getMemoryOverheadData(void);
// 该函数的功能是释放struct redisMemOverhead类型的内存开销数据结构
void freeMemoryOverheadData(struct redisMemOverhead *mh);
void checkChildrenDone(void);
int setOOMScoreAdj(int process_class);
void rejectCommandFormat(client *c, const char *fmt, ...);
void *activeDefragAlloc(void *ptr);
robj *activeDefragStringOb(robj *ob, long *defragged);

// 这段代码定义了三个宏，用于表示服务器重启的不同模式：
// RESTART_SERVER_NONE 表示不执行任何重启操作。
// RESTART_SERVER_GRACEFULLY 表示优雅关闭服务器后重启。
// RESTART_SERVER_CONFIG_REWRITE 表示在重启前重写配置文件。
#define RESTART_SERVER_NONE 0
#define RESTART_SERVER_GRACEFULLY (1 << 0)     /* Do proper shutdown. */
#define RESTART_SERVER_CONFIG_REWRITE (1 << 1) /* CONFIG REWRITE before restart.*/
int restartServer(int flags, mstime_t delay);

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

/* Declare database backup that include redis main DBs and slots to keys map.
 * Definition is in db.c. We can't define it here since we define CLUSTER_SLOTS
 * in cluster.h. */
// 声明数据库备份，包括redis主DB和槽到键映射。定义在db.c中。
// 我们无法在此处定义它，因为我们定义了CLUSTER_SLOTS在cluster.h中。
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

void serverLogObjectDebugInfo(const robj *o);
void sigsegvHandler(int sig, siginfo_t *info, void *secret);

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
int iAmMaster(void);

/* Debugging stuff */
void _serverAssertWithInfo(const client *c, const robj *o, const char *estr, const char *file, int line);
void _serverAssert(const char *estr, const char *file, int line);

// 该函数用于创建共享对象
void createSharedObjects(void);
#endif
