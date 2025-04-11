#ifndef ACL_H
#define ACL_H

#include "server.h"
#include "rax.h"

typedef struct user user;

/* =============================================================================
 * Global state for ACLs
 * ==========================================================================*/

rax *Users; /* Table mapping usernames to user structures. */

user *DefaultUser; /* Global reference to the default user.
                        Every new connection is associated to it, if no
                        AUTH or HELLO is used to authenticate with a
                        different user. */

// 这是在配置文件中找到的用户列表，我们需要在Redis初始化的最后阶段加载，在所有模块都已经加载之后。
// 每个列表元素都是一个以NULL结尾的SDS指针数组：第一个是用户名，其余所有指针都是ACL规则，格式与ACLSetUser（）相同。
list *UsersToLoad; /* This is a list of users found in the configuration file
                        that we'll need to load in the final stage of Redis
                        initialization, after all the modules are already
                        loaded. Every list element is a NULL terminated
                        array of SDS pointers: the first is the user name,
                        all the remaining pointers are ACL rules in the same
                        format as ACLSetUser(). */

// 我们的安全日志，用户可以使用ACL log命令检查它。
list *ACLLog; /* Our security log, the user is able to inspect that
                 using the ACL LOG command. */

static rax *commandId = NULL; /* Command name to id mapping */

static unsigned long nextid = 0; /* Next command id that has not been assigned */

// 表示 Redis 访问控制列表（ACL）系统中的权限类别项。
struct ACLCategoryItem
{
    const char *name; // 存储权限类别的名称
    uint64_t flag;    // 权限类别的标志位。标志位通常以位掩码的形式使用，每一位表示一个特定的权限。例如，某个类别可能使用特定位来表示是否允许执行某些命令。通过这种方式，可以高效地存储和检查权限。
};

// 表示 Redis 访问控制列表（ACL）系统中与用户权限相关的标志项。
struct ACLUserFlag
{
    const char *name; // 存储标志的名称
    uint64_t flag;    // 表示标志的值。标志通常以位掩码的形式使用，每一位表示一个特定的权限。例如，某些标志可能通过特定位来表示是否允许用户执行某些命令或访问某些资源。通过这种方式，可以高效地存储和检查用户的权限。
};

/* =============================================================================
 * ACL log
 * ==========================================================================*/
#define ACL_LOG_CTX_TOPLEVEL 0
#define ACL_LOG_CTX_LUA 1
#define ACL_LOG_CTX_MULTI 2
#define ACL_LOG_GROUPING_MAX_TIME_DELTA 60000

/* This structure defines an entry inside the ACL log. */
typedef struct ACLLogEntry
{
    uint64_t count; /* Number of times this happened recently. */
    int reason;     /* Reason for denying the command. ACL_DENIED_*. */
    int context;    /* Toplevel, Lua or MULTI/EXEC? ACL_LOG_CTX_*. */
    sds object;     /* The key name or command name. */
    sds username;   /* User the client is authenticated with. */
    mstime_t ctime; /* Milliseconds time of last update to this entry. */
    sds cinfo;      /* Client info (last client if updated). */
} ACLLogEntry;

void ACLResetSubcommandsForCommand(user *u, unsigned long id);
void ACLResetSubcommands(user *u);
void ACLAddAllowedSubcommand(user *u, unsigned long id, const char *sub);
void ACLFreeLogEntry(void *le);

/* The length of the string representation of a hashed password. */
#define HASH_PASSWORD_LEN SHA256_BLOCK_SIZE * 2

/* acl.c -- Authentication related prototypes. */
// acl.c -- 与身份验证相关的原型。
extern rax *Users;
extern user *DefaultUser;
void ACLInit(void);
/* Return values for ACLCheckCommandPerm() and ACLCheckPubsubPerm(). */
// 返回 ACLCheckCommandPerm() 和 ACLCheckPubsubPerm() 的值。
#define ACL_OK 0
#define ACL_DENIED_CMD 1
#define ACL_DENIED_KEY 2
#define ACL_DENIED_AUTH 3 /* Only used for ACL LOG entries. */    // 仅用于 ACL 日志条目。
#define ACL_DENIED_CHANNEL 4 /* Only used for pub/sub commands */ // 仅用于发布订阅命令,表示权限被拒绝。

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

#endif // ACL_H
