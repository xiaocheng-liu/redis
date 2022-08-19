/* A simple event-driven programming library. Originally I wrote this code
 * for the Jim's event-loop (Jim is a Tcl interpreter) but later translated
 * it in form of a library for easy reuse.
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

#ifndef __AE_H__
#define __AE_H__

#include "monotonic.h"


/* aeResizeSetSize,aeCreateFileEvent,aeDeleteTimeEvent函数的可能返回值*/
#define AE_OK 0
/* 作为众多ae系列函数的返回值使用 */
#define AE_ERR -1

/* 没有事件注册上 */
#define AE_NONE 0       /* No events registered. */
/* 有读事件注册上 */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
/* 有些事件注册上 */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */
/* 
   一般情况先执行readable事件再执行writable事件，
   这里的AE_BARRIER表示事件屏障，设置了AE_BARRIER之后,如果已经有了readable事件之后就不会触发 
   writable事件,在如下场景下适用:
   例如：你想要的以批量的方式先将file同步到磁盘，然后再回复给客户端
   这种情况下先执行writable事件再执行readable事件.
   感觉
    
   这里还需要再多多理解,不是很明白.(本段解释可能不对，感觉这里的英文解释写的不是很好，据下面
   的说，应该就是如果设置了AE_BARRIER，就优先处理写事件）
*/
#define AE_BARRIER 4    /* With WRITABLE, never fire the event if the
                           READABLE event already fired in the same event
                           loop iteration. Useful when you want to persist
                           things to disk before sending replies, and want
                           to do that in a group fashion. */

/* 0001-表示文件事件 */
#define AE_FILE_EVENTS (1<<0)  // redis将事件分为时间事件和文件事件，通过flag位来标识
/* 0010-表示时间事件 */
#define AE_TIME_EVENTS (1<<1)
/* 0011-表示文件事件和时间事件 */
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
/* 0100-表示函数处理完事件后直接返回，不阻塞等待 */
#define AE_DONT_WAIT (1<<2)

/* 
1000-用于aeProcessEvents(aeEventLoop *eventLoop, int flags)等处,如果
flags设置了AE_CALL_BEFORE_SLEEP,eventLoop->beforesleep回调函数会被调用
*/
#define AE_CALL_BEFORE_SLEEP (1<<3)
/* 
10000-用于aeProcessEvents(aeEventLoop *eventLoop, int flags)等处,如果
flags设置了AE_CALL_BEFORE_SLEEP,eventLoop->aftersleep回调函数会被调用
*/
#define AE_CALL_AFTER_SLEEP (1<<4)

/* 表示没有事件了,在processTimeEvents中被用到 */
#define AE_NOMORE -1
/* 表示删除事件ID，在processTimeEvents，aeDeleteTimeEvent等函数中被用到 */
#define AE_DELETED_EVENT_ID -1

/* Macros */
/* 仅仅用作编译器处理，防止因为没有到相关变量而被当做错误 */
#define AE_NOTUSED(V) ((void) V)

struct aeEventLoop;

/* Types and data structures */
// 有IO事件时处理IO事件的函数原型
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
// 有时间事件时处理时间事件的函数原型
typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);
// 一个对eventLoop和clientData处理的函数原型
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);
// 一个对eventLoop处理的函数原型，后面此函数类型具体的对象有beforeSleep和afterSleep
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);

/* File event structure */  // IO事件结构体
typedef struct aeFileEvent {
    /* 文件事件类型：是AE_READABLE,AE_WRITABLE和AE_BARRIER中的一个 */
    int mask; /* one of AE_(READABLE|WRITABLE|BARRIER) */
    aeFileProc *rfileProc;              /* 有可读IO事件时的处理函数 */
    aeFileProc *wfileProc;              /* 有可写IO事件时的处理函数 */
    void *clientData;                   /* 客户端传入的数据 */
} aeFileEvent;

/* Time event structure */      //(时间事件)结构体的定义
typedef struct aeTimeEvent {
    /* 时间事件的唯一id */
    long long id; /* time event identifier. */         
    /* timeEvent下次执行的时间 */
    monotime when;
    /* 时间事件处理函数 */         
    aeTimeProc *timeProc;
    /* 时间事件终结函数 */
    aeEventFinalizerProc *finalizerProc;
    /* 客户端传入的数据 */
    void *clientData;
    /* 指向上一个时间事件的指针 */
    struct aeTimeEvent *prev;
    /* 指向下一个时间事件的指针 */
    struct aeTimeEvent *next;
    /* 引用次数，防止时间事件在多次被调用后被释放*/
    int refcount; /* refcount to prevent timer events from being
  		   * freed in recursive time event calls. */
} aeTimeEvent;

/* A fired event */     
/* 就绪事件 */
typedef struct aeFiredEvent {
    /* 就绪事件的文件描述符 */
    int fd;
    /* 就绪事件类型，如AE_NONE，AE_READABLE，AE_WRITABLE等 */
    int mask;
} aeFiredEvent;

/* State of an event based program */
/* (事件循环)结构体的定义 */
typedef struct aeEventLoop {
    /* 当前已注册的最大的文件描述符 */
    int maxfd;   /* highest file descriptor currently registered */
    /* 文件描述符监听集合的大小 */
    int setsize; /* max number of file descriptors tracked */
    /* 下一个时间事件的ID */
    long long timeEventNextId;
    /* 已注册的事件 */
    aeFileEvent *events; /* Registered events */
    /* 已触发的事件 */
    aeFiredEvent *fired; /* Fired events */
    /* 时间事件的头节点指针 */
    aeTimeEvent *timeEventHead;
    /* 事件处理开关 */
    int stop;
    /* 多路复用库的事件状态数据 */
    void *apidata; /* This is used for polling API specific data */
    /* 执行处理事件之前的函数 */
    aeBeforeSleepProc *beforesleep;
    /* 执行处理事件之后的函数 */
    aeBeforeSleepProc *aftersleep;
    /* 事件循环的标志，初始值赋值为NONE(0)，在aeCreateEventLoop中搜索eventLoop->flags可看到*/
    int flags;
} aeEventLoop;

/* Prototypes */
/* 创建aeEventLoop */
aeEventLoop *aeCreateEventLoop(int setsize);
/* 删除EventLoop，释放相应的事件所占的空间 */
void aeDeleteEventLoop(aeEventLoop *eventLoop);
/* 设置eventLoop中的停止属性为1，服务器中似乎没有用到，压测和客户端中有用到这个函数 */
void aeStop(aeEventLoop *eventLoop);
/* 在eventLoop中创建文件事件 */
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData);
/* 删除文件事件 */
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);
/* 根据文件描述符id，找出文件的属性，是读事件还是写事件 */
int aeGetFileEvents(aeEventLoop *eventLoop, int fd);
/* 在eventLoop中添加时间事件，创建的时间为当前时间加上自己传入的时间 */
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc);
/* 根据时间id，删除时间事件，涉及链表的操作 */
int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id);

/* 处理eventLoop中的所有类型事件 */
int aeProcessEvents(aeEventLoop *eventLoop, int flags);
/* 让某事件等待 */
int aeWait(int fd, int mask, long long milliseconds);
/* ae事件执行主程序 */
void aeMain(aeEventLoop *eventLoop);
/* 获取接口名 */
char *aeGetApiName(void);
/* 设置eventLoop->beforesleep回调函数*/
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep);
/* 设置eventLoop->aftersleep回调函数 */
void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep);

/* 获取eventLoop的长度*/
int aeGetSetSize(aeEventLoop *eventLoop);
/* 设置eventLoop的长度*/
int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);
/* 通知事件的下一个迭代器将超时设置为零，即不等待 */
void aeSetDontWait(aeEventLoop *eventLoop, int noWait);

#endif
