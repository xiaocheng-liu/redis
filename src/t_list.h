//
// Created by 刘晓成 on 2025-02-14.
//

#ifndef T_LIST_H
#define T_LIST_H

#include "server.h"

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

#endif //T_LIST_H
