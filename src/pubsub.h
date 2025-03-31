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

// 取消订阅所有频道。返回客户端订阅的频道数。
int pubsubUnsubscribeAllChannels(client *c, int notify);
// 取消订阅所有模式。返回客户端订阅的模式数。
int pubsubUnsubscribeAllPatterns(client *c, int notify);
// 释放发布/订阅模式结构体
void freePubsubPattern(void *p);
// 发布/订阅模式匹配
int listMatchPubsubPattern(void *a, void *b);
// 发布消息
int pubsubPublishMessage(robj *channel, robj *message);
// 回复发布/订阅消息
void addReplyPubsubMessage(client *c, robj *channel, robj *msg);
// 获取客户端订阅的频道和模式总数
int clientSubscriptionsCount(client *c);

#endif
