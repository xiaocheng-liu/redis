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
// 双端链表的节点
typedef struct listNode {
    struct listNode *prev; // 前一个节点
    struct listNode *next; // 后一个节点
    void *value;           // 值，可以是任意类型
} listNode;

// Redis为list定义了一个迭代器结构，其能正序和逆序的访问list结构
typedef struct listIter {
    listNode *next;             // 指向下一个节点
    int direction;              // 方向参数，正序和逆序
} listIter;

// 双端链表本身
// list就是一个双向链表
typedef struct list {
    listNode *head;             // 指向链表头节点
    listNode *tail;             // 指向链表尾节点
    void *(*dup)(void *ptr);    // 用来复制节点的函数，主要用于深拷贝
    void (*free)(void *ptr);    // 释放节点的函数
    int (*match)(void *ptr, void *key); // 校验给定的key是否和节点中的node匹配，用于查找
    unsigned long len; //长度
} list;

/* Functions implemented as macros */
// Redis对其结构体提供了一系列的宏定义函数，方便操作其结构体参数
#define listLength(l) ((l)->len)                    // 获取list的长度
#define listFirst(l) ((l)->head)                    // 获取list的头节点
#define listLast(l) ((l)->tail)                     // 获取list的尾节点
#define listPrevNode(n) ((n)->prev)                 // 获取前一个节点
#define listNextNode(n) ((n)->next)                 // 获取后一个节点
#define listNodeValue(n) ((n)->value)               // 获取节点的值

#define listSetDupMethod(l,m) ((l)->dup = (m))      // 设定节点值复制函数
#define listSetFreeMethod(l,m) ((l)->free = (m))    // 设定节点值释放函数
#define listSetMatchMethod(l,m) ((l)->match = (m))  // 设定节点值匹配函数

#define listGetDupMethod(l) ((l)->dup)              // 获取节点值复制函数
#define listGetFreeMethod(l) ((l)->free)            // 获取节点值释放函数
#define listGetMatchMethod(l) ((l)->match)          // 获取节点值匹配函数

/* Prototypes */
// 函数原型
list *listCreate(void);                             // 创建
void listRelease(list *list);                       // 释放
void listEmpty(list *list);                         // 判断是否为空
list *listAddNodeHead(list *list, void *value);     // 向list的头部插入一个节点
list *listAddNodeTail(list *list, void *value);     // 向list的尾部插入一个节点
list *listInsertNode(list *list, listNode *old_node, void *value, int after);   // 将一个包含给定值的新节点添加到给定节点的之前或者之后。
void listDelNode(list *list, listNode *node);       // 删除节点
listIter *listGetIterator(list *list, int direction); // 获取迭代器
listNode *listNext(listIter *iter);                 // 获取下一个节点
void listReleaseIterator(listIter *iter);           // 释放迭代器对象
list *listDup(list *orig);                          // 链表复制函数
listNode *listSearchKey(list *list, void *key);     // 查找某个key的节点
listNode *listIndex(list *list, long index);        // 获取index的节点
void listRewind(list *list, listIter *li);          // 将迭代器的指针指向表头
void listRewindTail(list *list, listIter *li);      // 将迭代器的指针指向表尾
void listRotateTailToHead(list *list);              // 把list的尾节点放到头部      
void listRotateHeadToTail(list *list);              // 把list的头节点放到尾部
void listJoin(list *l, list *o);

/* Directions for iterators */
// 迭代器方向
#define AL_START_HEAD 0                             // 向前
#define AL_START_TAIL 1                             // 向后

#endif /* __ADLIST_H__ */
