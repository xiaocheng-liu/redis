#ifndef SERVER_CAMMAND_DEFINE_H
#define SERVER_CAMMAND_DEFINE_H

#include "server.h"

typedef struct client client;

/* Commands prototypes */
void pingCommand(client *c); // ping命令
void echoCommand(client *c); // echo命令
void timeCommand(client *c);
void infoCommand(client *c);
void monitorCommand(client *c);
void latencyCommand(client *c);
void moduleCommand(client *c);
void debugCommand(client *c);

// acl start
void aclCommand(client *c);
void authCommand(client *c); // auth命令
// acl end

// t_string start
void stralgoCommand(client *c);
void commandCommand(client *c); // command命令
void setCommand(client *c);     // set命令
void setnxCommand(client *c);   // setnx命令
void setexCommand(client *c);
void psetexCommand(client *c);
void getCommand(client *c);
void getexCommand(client *c);
void getdelCommand(client *c);
void setrangeCommand(client *c);
void getrangeCommand(client *c);
void incrCommand(client *c);
void decrCommand(client *c);
void incrbyCommand(client *c);
void decrbyCommand(client *c);
void incrbyfloatCommand(client *c);
void mgetCommand(client *c);
void getsetCommand(client *c);
void msetCommand(client *c);
void msetnxCommand(client *c);
void appendCommand(client *c);
void strlenCommand(client *c);
// t_string end

// db start
void delCommand(client *c);
void unlinkCommand(client *c);
void existsCommand(client *c);
void selectCommand(client *c);
void swapdbCommand(client *c);
void randomkeyCommand(client *c);
void keysCommand(client *c);
void scanCommand(client *c);
void dbsizeCommand(client *c);
void lastsaveCommand(client *c);
void shutdownCommand(client *c);
void moveCommand(client *c);
void copyCommand(client *c);
void renameCommand(client *c);
void renamenxCommand(client *c);
void flushdbCommand(client *c);
void flushallCommand(client *c);
// db end

// bitops start
void bitopCommand(client *c);
void bitcountCommand(client *c);
void bitposCommand(client *c);
void setbitCommand(client *c);
void getbitCommand(client *c);
void bitfieldCommand(client *c);
void bitfieldroCommand(client *c);
// bitops end

// rdb start
void saveCommand(client *c);
void bgsaveCommand(client *c);
// rdb end

// aof start
void bgrewriteaofCommand(client *c);
// aof end

// t_list start
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
void lremCommand(client *c);
void lposCommand(client *c);
void rpoplpushCommand(client *c);
void lmoveCommand(client *c);
void blpopCommand(client *c);
void brpopCommand(client *c);
void brpoplpushCommand(client *c);
void blmoveCommand(client *c);
// t_list end

// t_set start
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
// t_set end

// sort start
void sortCommand(client *c);
// sort end

// expire start
void expireCommand(client *c);
void expireatCommand(client *c);
void pexpireCommand(client *c);
void pexpireatCommand(client *c);
void ttlCommand(client *c);
void touchCommand(client *c);
void pttlCommand(client *c);
void persistCommand(client *c);
// expire end

// t_zset start
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
void zrankCommand(client *c);
void zrevrankCommand(client *c);
void zremrangebyrankCommand(client *c);
void zunionstoreCommand(client *c);
void zinterstoreCommand(client *c);
void zdiffstoreCommand(client *c);
void zunionCommand(client *c);
void zinterCommand(client *c);
void zrangestoreCommand(client *c);
void zdiffCommand(client *c);
void zscanCommand(client *c);
// t_zset end

// t_hash start
void hsetCommand(client *c);
void hsetnxCommand(client *c);
void hgetCommand(client *c);
void hmsetCommand(client *c);
void hmgetCommand(client *c);
void hdelCommand(client *c);
void hlenCommand(client *c);
void hstrlenCommand(client *c);
void hkeysCommand(client *c);
void hvalsCommand(client *c);
void hgetallCommand(client *c);
void hexistsCommand(client *c);
void hscanCommand(client *c);
void hrandfieldCommand(client *c);
void configCommand(client *c);
void hincrbyCommand(client *c);
void hincrbyfloatCommand(client *c);
// t_hash end

// pubsub start
void subscribeCommand(client *c);    // subscribe命令
void unsubscribeCommand(client *c);  // unsubscribe命令
void psubscribeCommand(client *c);   // psubscribe命令
void punsubscribeCommand(client *c); // punsubscribe命令
void publishCommand(client *c);      // publish命令
void pubsubCommand(client *c);       // pubsub命令
// pubsub end

// multi start
void multiCommand(client *c);
void execCommand(client *c);
void discardCommand(client *c);
void watchCommand(client *c);
void unwatchCommand(client *c);
// multi end

// cluster start
void clusterCommand(client *c);
void restoreCommand(client *c);
void migrateCommand(client *c);
void askingCommand(client *c);
void readonlyCommand(client *c);
void readwriteCommand(client *c);
void dumpCommand(client *c);
// cluster end

// object start
void objectCommand(client *c);
void memoryCommand(client *c);
// object end

// networking start
void clientCommand(client *c);
void helloCommand(client *c);
void securityWarningCommand(client *c);
void resetCommand(client *c);
// networking end

// scripting start
void replicaofCommand(client *c);
void evalCommand(client *c);
void evalShaCommand(client *c);
void scriptCommand(client *c);
// scripting end

// replication start
void roleCommand(client *c);
void syncCommand(client *c);
void replconfCommand(client *c);
void waitCommand(client *c);
void failoverCommand(client *c);
// replication end

// hyperloglog start
void pfselftestCommand(client *c);
void pfaddCommand(client *c);
void pfcountCommand(client *c);
void pfmergeCommand(client *c);
void pfdebugCommand(client *c);
// hyperloglog end

// t_stream start
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
// t_stream end

// lolwut start
void lolwutCommand(client *c);
// lolwut end

#endif // SERVER_CAMMAND_DEFINE_H
