/* adlist.h - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */
typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

// Redis为adlist定义了一个迭代器结构，其能正序和逆序的访问list结构
typedef struct listIter {
    listNode *next;             // 指向下一个节点
    int direction;              // 方向参数，正序和逆序
} listIter;

typedef struct list {
    listNode *head;           // 指向链表头节点
    listNode *tail;           // 指向链表尾节点
    void *(*dup)(void *ptr);  // 用来复制节点的函数，主要用于深拷贝 
    void (*free)(void *ptr);  // 释放节点的函数
    int (*match)(void *ptr, void *key); // 校验给定的key是否和节点中的node匹配，用于查找
    unsigned long len; //长度
} list; // list就是一个双向链表 

/* Functions implemented as macros 
    Redis对其结构体提供了一系列的宏定义函数，方便操作其结构体参数
*/
#define listLength(l) ((l)->len)
#define listFirst(l) ((l)->head)
#define listLast(l) ((l)->tail)
#define listPrevNode(n) ((n)->prev)
#define listNextNode(n) ((n)->next)
#define listNodeValue(n) ((n)->value)

#define listSetDupMethod(l,m) ((l)->dup = (m))      // 设定节点值复制函数
#define listSetFreeMethod(l,m) ((l)->free = (m))    // 设定节点值释放函数
#define listSetMatchMethod(l,m) ((l)->match = (m))  // 设定节点值匹配函数

#define listGetDupMethod(l) ((l)->dup)              // 获取节点值复制函数
#define listGetFreeMethod(l) ((l)->free)            // 获取节点值释放函数
#define listGetMatchMethod(l) ((l)->match)          // 获取节点值匹配函数

/* Prototypes */
list *listCreate(void);                             // 创建
void listRelease(list *list);                       // 释放
void listEmpty(list *list);                         // 判断是否为空
list *listAddNodeHead(list *list, void *value);     // 向list的头部插入一个节点
list *listAddNodeTail(list *list, void *value);     // 向list的尾部插入一个节点
list *listInsertNode(list *list, listNode *old_node, void *value, int after);
void listDelNode(list *list, listNode *node);       // 删除节点
listIter *listGetIterator(list *list, int direction); // 获取迭代器
listNode *listNext(listIter *iter);
void listReleaseIterator(listIter *iter);
list *listDup(list *orig);                          // 链表复制函数
listNode *listSearchKey(list *list, void *key);     // 查找某个key的节点
listNode *listIndex(list *list, long index);        // 获取index的节点
void listRewind(list *list, listIter *li);
void listRewindTail(list *list, listIter *li);
void listRotateTailToHead(list *list);
void listRotateHeadToTail(list *list);
void listJoin(list *l, list *o);

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */
