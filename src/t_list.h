//
// Created by 刘晓成 on 2025-02-14.
//

#ifndef T_LIST_H
#define T_LIST_H

#include "server.h"
#include "quicklist.h"
typedef struct redisObject robj;
typedef struct client client;

#define LIST_HEAD 0
#define LIST_TAIL 1

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

/* List data type */
// 列表数据类型
void listTypeTryConversion(robj *subject, robj *value);
void listTypePush(robj *subject, robj *value, int where);                                   // push
robj *listTypePop(robj *subject, int where);                                                // pop
unsigned long listTypeLength(const robj *subject);                                          // 获取list长度
listTypeIterator *listTypeInitIterator(robj *subject, long index, unsigned char direction); // 获取list迭代器
void listTypeReleaseIterator(listTypeIterator *li);                                         // 释放list迭代器内存
int listTypeNext(listTypeIterator *li, listTypeEntry *entry);                               // 当前节点是否有下一个节点
robj *listTypeGet(listTypeEntry *entry);
void listTypeInsert(listTypeEntry *entry, robj *value, int where); // list插入
int listTypeEqual(listTypeEntry *entry, robj *o);                  // 判断给定的对象是否是当前节点
void listTypeDelete(listTypeIterator *iter, listTypeEntry *entry); // list删除
void listTypeConvert(robj *subject, int enc);
robj *listTypeDup(robj *o); // 复制
void unblockClientWaitingData(client *c);
void pushGenericCommand(client *c, int where, int xx);
void popGenericCommand(client *c, int where);
void listElementsRemoved(client *c, robj *key, int where, robj *o, long count);

#endif // T_LIST_H
