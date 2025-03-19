#ifndef PUBSUB_H
#define PUBSUB_H
#include "server.h"
typedef struct client client;
typedef struct redisObject robj;

/* Pub / Sub */
// 发布/订阅
// 这段代码定义了一个名为 pubsubPattern 的结构体，用于表示发布/订阅模式中的模式匹配。
// 结构体包含两个成员：一个指向 client 类型的指针，表示客户端；一个指向 robj 类型的指针，表示模式对象。
typedef struct pubsubPattern
{
    client *client;
    robj *pattern;
} pubsubPattern;

int pubsubUnsubscribeAllChannels(client *c, int notify);
int pubsubUnsubscribeAllPatterns(client *c, int notify);
void freePubsubPattern(void *p);
int listMatchPubsubPattern(void *a, void *b);
int pubsubPublishMessage(robj *channel, robj *message);
void addReplyPubsubMessage(client *c, robj *channel, robj *msg);

#endif
