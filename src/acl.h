#ifndef ACL_H
#define ACL_H
#include "rax.h"
#include "server.h"

/* =============================================================================
 * Global state for ACLs
 * ==========================================================================*/

rax *Users; /* Table mapping usernames to user structures. */

user *DefaultUser;  /* Global reference to the default user.
                       Every new connection is associated to it, if no
                       AUTH or HELLO is used to authenticate with a
                       different user. */

// 这是在配置文件中找到的用户列表，我们需要在Redis初始化的最后阶段加载，在所有模块都已经加载之后。
// 每个列表元素都是一个以NULL结尾的SDS指针数组：第一个是用户名，其余所有指针都是ACL规则，格式与ACLSetUser（）相同。
list *UsersToLoad;  /* This is a list of users found in the configuration file
                       that we'll need to load in the final stage of Redis
                       initialization, after all the modules are already
                       loaded. Every list element is a NULL terminated
                       array of SDS pointers: the first is the user name,
                       all the remaining pointers are ACL rules in the same
                       format as ACLSetUser(). */
// 我们的安全日志，用户可以使用ACL log命令检查它。
list *ACLLog;       /* Our security log, the user is able to inspect that
                       using the ACL LOG command. */

static rax *commandId = NULL; /* Command name to id mapping */

static unsigned long nextid = 0; /* Next command id that has not been assigned */

struct ACLCategoryItem {
    const char *name;
    uint64_t flag;
} ACLCommandCategories[] = {
    {"keyspace", CMD_CATEGORY_KEYSPACE},
    {"read", CMD_CATEGORY_READ},
    {"write", CMD_CATEGORY_WRITE},
    {"set", CMD_CATEGORY_SET},
    {"sortedset", CMD_CATEGORY_SORTEDSET},
    {"list", CMD_CATEGORY_LIST},
    {"hash", CMD_CATEGORY_HASH},
    {"string", CMD_CATEGORY_STRING},
    {"bitmap", CMD_CATEGORY_BITMAP},
    {"hyperloglog", CMD_CATEGORY_HYPERLOGLOG},
    {"geo", CMD_CATEGORY_GEO},
    {"stream", CMD_CATEGORY_STREAM},
    {"pubsub", CMD_CATEGORY_PUBSUB},
    {"admin", CMD_CATEGORY_ADMIN},
    {"fast", CMD_CATEGORY_FAST},
    {"slow", CMD_CATEGORY_SLOW},
    {"blocking", CMD_CATEGORY_BLOCKING},
    {"dangerous", CMD_CATEGORY_DANGEROUS},
    {"connection", CMD_CATEGORY_CONNECTION},
    {"transaction", CMD_CATEGORY_TRANSACTION},
    {"scripting", CMD_CATEGORY_SCRIPTING},
    {NULL,0} /* Terminator. */
};

struct ACLUserFlag {
    const char *name;
    uint64_t flag;
} ACLUserFlags[] = {
    /* Note: the order here dictates the emitted order at ACLDescribeUser */
    {"on", USER_FLAG_ENABLED},
    {"off", USER_FLAG_DISABLED},
    {"allkeys", USER_FLAG_ALLKEYS},
    {"allchannels", USER_FLAG_ALLCHANNELS},
    {"allcommands", USER_FLAG_ALLCOMMANDS},
    {"nopass", USER_FLAG_NOPASS},
    {"skip-sanitize-payload", USER_FLAG_SANITIZE_PAYLOAD_SKIP},
    {"sanitize-payload", USER_FLAG_SANITIZE_PAYLOAD},
    {NULL,0} /* Terminator. */
};

void ACLResetSubcommandsForCommand(user *u, unsigned long id);
void ACLResetSubcommands(user *u);
void ACLAddAllowedSubcommand(user *u, unsigned long id, const char *sub);
void ACLFreeLogEntry(void *le);

/* The length of the string representation of a hashed password. */
#define HASH_PASSWORD_LEN SHA256_BLOCK_SIZE*2

#endif //ACL_H
