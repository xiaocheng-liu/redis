/*
 * Kqueue(2)-based ae.c module
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "ae.h"
#include "anet.h"
#include "zmalloc.h"

// 保存基于 kqueue 的事件驱动机制的状态信息。
typedef struct aeApiState
{
    int kqfd;              // 实例文件描述符
    struct kevent *events; // 记录就绪的事件
} aeApiState;

// 创建并初始化一个 kqueue 事件驱动机制所需的状态数据，关联到传入的事件循环对象（eventLoop）。
static int aeApiCreate(aeEventLoop *eventLoop)
{
    // 分配内存
    aeApiState *state = zmalloc(sizeof(aeApiState));

    if (!state)
        return -1;
    // 为kqueue事件分配内存
    state->events = zmalloc(sizeof(struct kevent) * eventLoop->setsize);
    if (!state->events)
    {
        zfree(state);
        return -1;
    }
    // 通过 kqueue() 系统调用创建一个 kqueue 实例，并将返回的文件描述符保存到 state->kqfd 中。
    state->kqfd = kqueue();
    if (state->kqfd == -1)
    {
        zfree(state->events);
        zfree(state);
        return -1;
    }
    anetCloexec(state->kqfd);
    // 将aeApiState设置到eventLoop的apidata
    eventLoop->apidata = state;
    return 0;
}

// 在事件循环中调整 kqueue 事件数组的大小。
static int aeApiResize(aeEventLoop *eventLoop, int setsize)
{
    aeApiState *state = eventLoop->apidata;

    state->events = zrealloc(state->events, sizeof(struct kevent) * setsize);
    return 0;
}

// 释放与 kqueue 事件驱动机制相关的所有资源。
static void aeApiFree(aeEventLoop *eventLoop)
{
    aeApiState *state = eventLoop->apidata;

    close(state->kqfd);
    zfree(state->events);
    zfree(state);
}

// 通过 kqueue 机制为给定的文件描述符（fd）添加读或写事件。
static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask)
{
    // 获取aeApiState
    aeApiState *state = eventLoop->apidata;
    struct kevent ke;

    // 判断检查 mask 是否包含 AE_READABLE
    if (mask & AE_READABLE)
    {
        // 设置 kevent 结构体的成员变量
        // EVFILT_READ 参数指定了要监听的事件类型，即读事件，这意味着当文件描述符 fd 上有数据可读时，系统会发出通知。
        // EV_ADD 则表示将这个事件注册添加到 kqueue 的事件列表中。
        // 后面的参数 0, 0, NULL 分别用于设置过滤器选项、事件标志和用户数据，在这种简单的用法中，这些值通常保留为默认。
        EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        // 向 kqueue 事件通知系统注册一个读事件监听。
        if (kevent(state->kqfd, &ke, 1, NULL, 0, NULL) == -1)
            return -1;
    }
    // 判断检查 mask 是否包含 AE_WRITABLE
    if (mask & AE_WRITABLE)
    {
        // EVFILT_WRITE 参数指定了要监听的事件类型，即写事件，这意味着当文件描述符 fd 可以写入数据时，系统会发出通知。
        // EV_ADD 则表示将这个事件注册添加到 kqueue 的事件列表中。
        EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        // 向 kqueue 事件通知系统注册一个写事件监听。
        if (kevent(state->kqfd, &ke, 1, NULL, 0, NULL) == -1)
            return -1;
    }
    return 0;
}

// 通过 kqueue 机制从事件循环中删除指定文件描述符（fd）的读和/或写事件。
static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int mask)
{
    aeApiState *state = eventLoop->apidata;
    struct kevent ke;

    // 判断检测 mask 是否包含表示读事件的标志 AE_READABLE
    if (mask & AE_READABLE)
    {
        // 设置 kevent 结构体的成员变量
        // EV_DELETE 表示删除事件监听，而不是添加。
        EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        // 删除 kqueue 中的读事件监听。
        kevent(state->kqfd, &ke, 1, NULL, 0, NULL);
    }
    // 判断检测 mask 是否包含表示写事件的标志 AE_WRITABLE；
    if (mask & AE_WRITABLE)
    {
        // EV_DELETE 表示删除事件监听，而不是添加。
        EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        // 删除 kqueue 中的写事件监听。
        kevent(state->kqfd, &ke, 1, NULL, 0, NULL);
    }
}

// 在事件循环中通过 kqueue 机制等待 I/O 事件，并将检测到的事件以适当的格式传递给事件循环进行处理。
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp)
{
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    // 若 tvp 不为空，则创建一个 struct timespec 结构体 timeout，将 tvp 中的秒数和微秒数转换为秒和纳秒，然后调用 kevent 传入这个超时值，使得事件等待在超时后返回。
    if (tvp != NULL)
    {
        struct timespec timeout;
        timeout.tv_sec = tvp->tv_sec;
        timeout.tv_nsec = tvp->tv_usec * 1000;
        retval = kevent(state->kqfd, NULL, 0, state->events, eventLoop->setsize,
                        &timeout);
    }
    else // 若 tvp 为空，说明无限等待，此时 kevent 的超时参数传入 NULL。
    {
        retval = kevent(state->kqfd, NULL, 0, state->events, eventLoop->setsize,
                        NULL);
    }

    // 调用 kevent 后，返回的结果 retval 即为检测到的事件数量。
    if (retval > 0)
    {
        int j;

        numevents = retval;
        for (j = 0; j < numevents; j++)
        {
            int mask = 0;
            struct kevent *e = state->events + j;
            // 检测由 kqueue 返回的事件类型，并设置相应的事件标志。
            if (e->filter == EVFILT_READ)
                mask |= AE_READABLE;
            if (e->filter == EVFILT_WRITE)
                mask |= AE_WRITABLE;
            eventLoop->fired[j].fd = e->ident;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}
// 回一个指向字符数组的指针。用于标识当前使用的事件驱动接口或后端。
static char *aeApiName(void)
{
    return "kqueue";
}
