/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * listRelease(), but private value of every node need to be freed
 * by the user before to call listRelease(), or by setting a free method using
 * listSetFreeMethod.
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
// 创建新的list
list *listCreate(void)
{
    struct list *list;

    // 分配内存
    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    
    // 设置属性
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* 清空list，但不销毁list */
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    // 指向头指针
    current = list->head;
    len = list->len;
    // 遍历整个链表
    while(len--) {
        next = current->next;
        // 如果设置了值释放函数，就调用
        if (list->free) list->free(current->value);
        // 释放节点
        zfree(current);
        // 当前节点设置为下一个节点
        current = next;
    }
    // 清空链表
    list->head = list->tail = NULL;
    list->len = 0;
}

/* 清空并销毁list */
void listRelease(list *list)
{
    // 清空list
    listEmpty(list);
    // 释放链表
    zfree(list);
}

/* 在list头部增加一个新节点，就是一次双向链表的头部插入，插入成功返回新的head*/
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    // 为新节点分配内存
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    // 保存值指针
    node->value = value;
    if (list->len == 0) {
        // 如果链表为空
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        // 如果链表非空
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    // 更新链表长度
    list->len++;
    return list;
}

/* 双向链表尾部插入 */
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    // 为新节点分配内存
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    // 保存值指针
    node->value = value;
    if (list->len == 0) {
        // 如果链表为空
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        // 如果链表非空
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    // 更新链表长度
    list->len++;
    return list;
}

// 将一个包含给定值的新节点添加到给定节点的之前或者之后。
/*
 * 创建一个包含值 value 的新节点，并将它插入到 old_node 的之前或之后
 *
 * 如果 after 为 0 ，将新节点插入到 old_node 之前。
 * 如果 after 为 1 ，将新节点插入到 old_node 之后。
 *
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    // 为新节点分配内存
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    // 保存值指针
    node->value = value;
    if (after) {
        // 如果插入到之后
        // 将插入节点的前一个节点设置为老节点
        node->prev = old_node;
        // 将插入节点的后一个节点设置为老节点的后一个节点
        node->next = old_node->next;
        if (list->tail == old_node) {
            // 如果链表的尾节点就是被插入的节点，将链表的尾节点设置为插入节点
            list->tail = node;
        }
    } else {
        // 如果插入到之前
        // 将插入节点的下一个节点设置为老节点
        node->next = old_node;
        // 将插入节点的前一个节点设置为老节点的前一个节点
        node->prev = old_node->prev;
        if (list->head == old_node) {
            // 如果链表的头节点就是被插入的节点，将链表的头节点设置为插入节点
            list->head = node;
        }
    }
    // 更新插入节点的前置节点
    if (node->prev != NULL) {
        // 如果插入节点的前一个节点不为NULL，将前一个节点的后一个节点设置为插入节点
        node->prev->next = node;
    }
    // 更新插入节点的后置节点
    if (node->next != NULL) {
        // 如果插入节点的后一个节点不为NULL，将后一个节点的前一个节点设置为插入节点
        node->next->prev = node;
    }
    // 更新链表长度
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
// 删除节点
void listDelNode(list *list, listNode *node)
{
    // 调整前置节点的指针
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    // 调整后置节点的指针
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    // 释放值
    if (list->free) list->free(node->value);
    // 释放节点
    zfree(node);
    // 更新链表长度
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 * 
 * This function can't fail. 
 * list的迭代器，就是有个指针 */
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;

    // 为迭代器分配内存
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    // 根据迭代方向，设置起始节点
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;
    // 记录迭代方向
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
// 释放迭代器
void listReleaseIterator(listIter *iter) {
    // 释放内存
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
// 将迭代器的指针指向表头，并设置迭代方向
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

// 将迭代器的指针指向表尾，并设置迭代方向
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage
 * pattern is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
// 迭代器获取下一个节点
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        // 根据方向选择下一个节点
        if (iter->direction == AL_START_HEAD)
            // 保存下一个节点，防止当前节点被删除而造成指针丢失
            iter->next = current->next;
        else
            // 保存下一个节点，防止当前节点被删除而造成指针丢失
            iter->next = current->prev;
    }
    return current;
}

/* 复制整个list，如果内存不足返回null，否则返回复制出来的新list
 * 
 * 如果设置了List的dup方法，新list里的ListNode里的val才是新复制处理来的，
 * 否则就直接是旧ListNode里val的一个指针，这样的问题是如果新旧list其中一
 * 个有变动，都会影响到另外一个list。
 *
 * 不管成功与否，原始链表都不会被改动 */
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    // 创建新链表
    if ((copy = listCreate()) == NULL)
        return NULL;
    // 设置节点值处理函数
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    // 将节点指向表头
    listRewind(orig, &iter);
    // 迭代整个输入链表
    while((node = listNext(&iter)) != NULL) {
        void *value;

        // 复制节点值到新值
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                return NULL;
            }
        } else
            value = node->value;
        
        // 将节点添加到链表
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            return NULL;
        }
    }
    // 返回复制的list
    return copy;
}

/*  在list中查找指定的key，如果list指定了match方法，则使用match方法来判定
 * 是否匹配，否则直接比较节点中的val和key指向的是否是同一个地址。 
 *
 * 如果找到匹配的，返回第一个匹配的节点，否则返回null，本质上就是一个双向链表的查找 */
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    listRewind(list, &iter);
    // 迭代整个链表
    while((node = listNext(&iter)) != NULL) {
        // 对比
        if (list->match) {
            if (list->match(node->value, key)) {
                // 找到
                return node;
            }
        } else {
            if (key == node->value) {
                // 找到
                return node;
            }
        }
    }
    // 未找到
    return NULL;
}

/* 查找list中在第index位的节点 */
listNode *listIndex(list *list, long index) {
    listNode *n;

    
    if (index < 0) { 
        // 如果index是负，执行逆向查找
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {
        // // 如果index是正，执行正向查找
        n = list->head;
        while(index-- && n) n = n->next;
    }
    // 返回节点
    return n;
}

/* 把list的尾节点放到头部 */
void listRotateTailToHead(list *list) {
    if (listLength(list) <= 1) return;

    /* Detach current tail */
    // 取出表尾节点
    listNode *tail = list->tail;
    // 将链表尾指针指向当前取出节点的前节点
    list->tail = tail->prev;
    // 将链表的尾节点的后一个节点指向NULL
    list->tail->next = NULL;
    /* Move it as head */
    // 将链表的头节点的前一个节点指向取出的节点
    list->head->prev = tail;
    // 将取出的节点的前一个节点指向NULL
    tail->prev = NULL;
    // 将取出的节点的后一个节点指向链表的头节点
    tail->next = list->head;
    // 将链表的头节点指向取出的节点
    list->head = tail;
}

/* 把list中的头部节点放到尾部. */
void listRotateHeadToTail(list *list) {
    if (listLength(list) <= 1) return;

    // 取出表头节点
    /* Detach current head */
    listNode *head = list->head;
    // 将链表的头指针指向取出节点的下一个节点
    list->head = head->next;
    // 将链表的头指针指向取出节点的前一个节点
    list->head->prev = NULL;
    /* Move it as tail */
    // 将表尾节点的后一个节点指向当前取出的节点
    list->tail->next = head;
    // 将取出的节点的后一个节点指向NULL
    head->next = NULL;
    // 将取出的节点的前一个节点指向链表的尾节点
    head->prev = list->tail;
    // 将链表的尾节点指向取出的节点
    list->tail = head;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. 
 * 把l和o两个list拼接在一起，*/
void listJoin(list *l, list *o) {
    // 如果o为空，直接返回
    if (o->len == 0) return;

    // 将o的头节点的前一个节点指向l的尾节点
    o->head->prev = l->tail;

    if (l->tail)
        // 如果l的尾节点存在，将l的尾节点的下一个节点指向o的头节点
        l->tail->next = o->head;
    else
        // 不存在，说明l为空，直接设置l的头节点为o的头节点
        l->head = o->head;

    // l的尾节点设置为o的尾节点
    l->tail = o->tail;
    // l的长度加上o的长度
    l->len += o->len;

    /* Setup other as an empty list. */
    // 将o设置为空链表
    o->head = o->tail = NULL;
    o->len = 0;
}
