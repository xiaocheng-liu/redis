/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#ifndef __BIO_H
#define __BIO_H

#include <pthread.h>
#include "adlist.h"

typedef void lazy_free_fn(void *args[]);

/* Exported API */
/* 初始化后台系统，生成线程。*/
void bioInit(void);
unsigned long long bioPendingJobsOfType(int type);
unsigned long long bioWaitStepOfType(int type);
time_t bioOlderJobOfType(int type);
void bioKillThreads(void);
void bioCreateCloseJob(int fd);
void bioCreateFsyncJob(int fd);
void bioCreateLazyFreeJob(lazy_free_fn free_fn, int arg_count, ...);

/* Background job opcodes */
#define BIO_CLOSE_FILE    0 /* Deferred close(2) syscall. */    // 关闭文件任务
#define BIO_AOF_FSYNC     1 /* Deferred AOF fsync. */           // AOF新增数据刷盘任务
#define BIO_LAZY_FREE     2 /* Deferred objects freeing. */     // 惰性删除任务
#define BIO_NUM_OPS       3                                     // BIO线程数量，也就是任务类型数量

// 保存线程的数组
static pthread_t bio_threads[BIO_NUM_OPS];
// 保存互斥锁的数组，针对bio_jobs的锁
static pthread_mutex_t bio_mutex[BIO_NUM_OPS];
// 保存条件变量的两个数组
// 用于BIO线程等待任务：每创建一个任务(bioCreateBackgroundJob)则唤醒线程；在bioProcessBackgroundJobs中如果没有任务则等待
static pthread_cond_t bio_newjob_cond[BIO_NUM_OPS];
// 用于其它线程等待BIO线程：当其他线程等待某任务执行完成，可以调用bioWaitStepOfType进行等待，在bioProcessBackgroundJobs中最后会取消阻塞在
static pthread_cond_t bio_step_cond[BIO_NUM_OPS];
// 以后台线程方式运行的任务列表，每个数组元素是一个list
static list *bio_jobs[BIO_NUM_OPS];
/* The following array is used to hold the number of pending jobs for every
 * OP type. This allows us to export the bioPendingJobsOfType() API that is
 * useful when the main thread wants to perform some operation that may involve
 * objects shared with the background thread. The main thread will just wait
 * that there are no longer jobs of this type to be executed before performing
 * the sensible operation. This data is also useful for reporting. */
// 用来表示每种任务中，处于等待状态的任务个数。将该数组每个元素初始化为 0，其实就是表示初始时，每种任务都没有待处理的具体任务。
static unsigned long long bio_pending[BIO_NUM_OPS];

/* This structure represents a background Job. It is only used locally to this
 * file as the API does not expose the internals at all. */
struct bio_job {
    time_t time; /* Time at which the job was created. */   // 任务创建时间
    /* Job specific arguments.*/
    int fd; /* Fd for file based background jobs */
    lazy_free_fn *free_fn; /* Function that will free the provided arguments */
    void *free_args[]; /* List of arguments to be passed to the free function */
};

void *bioProcessBackgroundJobs(void *arg);

/* Make sure we have enough stack to perform all the things we do in the
 * main thread. */
// 确保我们有足够的堆栈来执行我们在主线程中执行的所有操作。
#define REDIS_THREAD_STACK_SIZE (1024*1024*4)

#endif
