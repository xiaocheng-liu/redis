/*
 * Background I/O service for Redis.
 */
#include "server.h"
#include "bio.h"

#include <sys/errno.h>

/* Initialize the background system, spawning the thread. */
// 初始化后台系统，生成线程。
void bioInit(void) {
    pthread_attr_t attr;
    pthread_t thread;
    size_t stacksize;
    int j;

    /* Initialization of state vars and objects */
    /* 状态变量和对象的初始化 */
    for (j = 0; j < BIO_NUM_OPS; j++) {
        // 首先初始化互斥锁数组和条件变量数组
        pthread_mutex_init(&bio_mutex[j],NULL);
        pthread_cond_init(&bio_newjob_cond[j],NULL);
        pthread_cond_init(&bio_step_cond[j],NULL);
        // 调用 listCreate 函数，给 bio_jobs 这个数组的每个元素创建一个列表
        bio_jobs[j] = listCreate();
        //将 bio_pending 数组的每个元素赋值为 0
        bio_pending[j] = 0;
    }

    /* Set the stack size as by default it may be small in some system */
    // 将堆栈大小设置为默认情况下在某些系统中可能很小
    // 初始化线程属性
    pthread_attr_init(&attr);
    // 获取线程的栈大小这一属性的当前值，并根据当前栈大小和 REDIS_THREAD_STACK_SIZE 宏定义的大小（默认值为 4MB），来计算最终的栈大小属性值。
    pthread_attr_getstacksize(&attr,&stacksize);
    if (!stacksize) stacksize = 1; /* The world is full of Solaris Fixes */
    while (stacksize < REDIS_THREAD_STACK_SIZE) stacksize *= 2;
    // 设置栈大小这一属性值
    pthread_attr_setstacksize(&attr, stacksize);

    /* Ready to spawn our threads. We use the single argument the thread
     * function accepts in order to pass the job ID the thread is
     * responsible of. */
    // 依次为每种后台任务创建一个线程。循环的次数是由 BIO_NUM_OPS 宏定义决定的，也就是 3 次。
    // 相应的，bioInit 函数就会调用 3 次 pthread_create 函数，并创建 3 个线程。
    for (j = 0; j < BIO_NUM_OPS; j++) {
        void *arg = (void*)(unsigned long) j;
        // bioInit 函数让这 3 个线程执行的函数都是 bioProcessBackgroundJobs。
        // 在这三次线程的创建过程中，传给这个函数的参数分别是 0、1、2，
        // 因为三种后台任务类型 BIO_CLOSE_FILE、BIO_AOF_FSYNC 和 BIO_LAZY_FREE 对应的操作码，它们的取值分别为 0、1、2。
        if (pthread_create(&thread,&attr,bioProcessBackgroundJobs,arg) != 0) {
            serverLog(LL_WARNING,"Fatal: Can't initialize Background Jobs.");
            exit(1);
        }
        bio_threads[j] = thread;
    }
}

void bioSubmitJob(int type, struct bio_job *job) {
    job->time = time(NULL);
    // 多线程需要加锁，把待处理的job添加到队列末尾
    pthread_mutex_lock(&bio_mutex[type]);
    listAddNodeTail(bio_jobs[type],job);
    bio_pending[type]++;
    pthread_cond_signal(&bio_newjob_cond[type]);
    pthread_mutex_unlock(&bio_mutex[type]);
}

void bioCreateLazyFreeJob(lazy_free_fn free_fn, int arg_count, ...) {
    va_list valist;
    /* Allocate memory for the job structure and all required
     * arguments */
    struct bio_job *job = zmalloc(sizeof(*job) + sizeof(void *) * (arg_count));
    job->free_fn = free_fn;

    va_start(valist, arg_count);
    for (int i = 0; i < arg_count; i++) {
        job->free_args[i] = va_arg(valist, void *);
    }
    va_end(valist);
    bioSubmitJob(BIO_LAZY_FREE, job);
}

void bioCreateCloseJob(int fd) {
    struct bio_job *job = zmalloc(sizeof(*job));
    job->fd = fd;

    bioSubmitJob(BIO_CLOSE_FILE, job);
}

void bioCreateFsyncJob(int fd) {
    struct bio_job *job = zmalloc(sizeof(*job));
    job->fd = fd;

    bioSubmitJob(BIO_AOF_FSYNC, job);
}

/* 根据参数处理不同的后台线程任务 */
void *bioProcessBackgroundJobs(void *arg) {
    struct bio_job *job;
    // 获取当前函数要处理的任务类型
    // 三种后台任务类型 BIO_CLOSE_FILE、BIO_AOF_FSYNC 和 BIO_LAZY_FREE 对应的操作码，它们的取值分别为 0、1、2。
    unsigned long type = (unsigned long) arg;
    sigset_t sigset;

    /* Check that the type is within the right interval. */
    if (type >= BIO_NUM_OPS) {
        serverLog(LL_WARNING,
            "Warning: bio thread started with wrong type %lu",type);
        return NULL;
    }

    switch (type) {
        case BIO_CLOSE_FILE:
            redis_set_thread_title("bio_close_file");
            break;
        case BIO_AOF_FSYNC:
            redis_set_thread_title("bio_aof_fsync");
            break;
        case BIO_LAZY_FREE:
            redis_set_thread_title("bio_lazy_free");
            break;
        default:
                break;
    }

    redisSetCpuAffinity(server.bio_cpulist);

    makeThreadKillable();

    // 获取锁
    pthread_mutex_lock(&bio_mutex[type]);
    /* Block SIGALRM so we are sure that only the main thread will
     * receive the watchdog signal. */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL))
        serverLog(LL_WARNING,
            "Warning: can't mask SIGALRM in bio.c thread: %s", strerror(errno));

    while(1) {
        listNode *ln;

        /* The loop always starts with the lock hold. */
        // 如果没有当前线程要处理的任务，则根据对应的条件变量进行等待
        if (listLength(bio_jobs[type]) == 0) {
            // 在pthread_cond_wait之前必须获取该共享数据的互斥锁，线程挂起时释放锁，并在满足条件离开条件变量时重新加锁
            pthread_cond_wait(&bio_newjob_cond[type],&bio_mutex[type]);
            // 被唤醒了，说明有任务了，进入下一次循环
            continue;
        }
        /* Pop the job from the queue. */
        // 获取当前类型的任务集合中第一个任务
        ln = listFirst(bio_jobs[type]);
        job = ln->value;
        /* It is now possible to unlock the background system as we know have
         * a stand alone job structure to process.*/
        // 释放锁
        pthread_mutex_unlock(&bio_mutex[type]);

        /* 根据任务类型执行不同的逻辑 */
        if (type == BIO_CLOSE_FILE) {
            // 如果是关闭文件任务，那就调用close函数
            close(job->fd);
        } else if (type == BIO_AOF_FSYNC) {
            // 如果是AOF同步写任务，那就调用redis_fsync函数(其实就是fsync)
            redis_fsync(job->fd);
        } else if (type == BIO_LAZY_FREE) {
            /*
             * 如果是惰性删除任务，那根据任务的参数分别调用不同的惰性删除函数执行。
             */
            job->free_fn(job->free_args);
        } else {
            serverPanic("Wrong job type in bioProcessBackgroundJobs().");
        }
        zfree(job);

        /* Lock again before reiterating the loop, if there are no longer
         * jobs to process we'll block again in pthread_cond_wait(). */
        pthread_mutex_lock(&bio_mutex[type]);
        // 任务执行完成后，调用listDelNode在任务队列中删除该任务
        listDelNode(bio_jobs[type],ln);
        // 将对应的等待任务个数减一
        bio_pending[type]--;

        /* Unblock threads blocked on bioWaitStepOfType() if any. */
        // 取消阻塞在 bioWaitStepOfType() 上阻塞线程（如果有）
        pthread_cond_broadcast(&bio_step_cond[type]);
    }
}

/* Return the number of pending jobs of the specified type. */
unsigned long long bioPendingJobsOfType(int type) {
    unsigned long long val;
    pthread_mutex_lock(&bio_mutex[type]);
    val = bio_pending[type];
    pthread_mutex_unlock(&bio_mutex[type]);
    return val;
}

/* If there are pending jobs for the specified type, the function blocks
 * and waits that the next job was processed. Otherwise the function
 * does not block and returns ASAP.
 *
 * The function returns the number of jobs still to process of the
 * requested type.
 *
 * This function is useful when from another thread, we want to wait
 * a bio.c thread to do more work in a blocking way.
 */
unsigned long long bioWaitStepOfType(int type) {
    unsigned long long val;
    pthread_mutex_lock(&bio_mutex[type]);
    val = bio_pending[type];
    if (val != 0) {
        pthread_cond_wait(&bio_step_cond[type],&bio_mutex[type]);
        val = bio_pending[type];
    }
    pthread_mutex_unlock(&bio_mutex[type]);
    return val;
}

/* Kill the running bio threads in an unclean way. This function should be
 * used only when it's critical to stop the threads for some reason.
 * Currently Redis does this only on crash (for instance on SIGSEGV) in order
 * to perform a fast memory check without other threads messing with memory. */
void bioKillThreads(void) {
    int err, j;

    for (j = 0; j < BIO_NUM_OPS; j++) {
        if (bio_threads[j] == pthread_self()) continue;
        if (bio_threads[j] && pthread_cancel(bio_threads[j]) == 0) {
            if ((err = pthread_join(bio_threads[j],NULL)) != 0) {
                serverLog(LL_WARNING,
                    "Bio thread for job type #%d can not be joined: %s",
                        j, strerror(err));
            } else {
                serverLog(LL_WARNING,
                    "Bio thread for job type #%d terminated",j);
            }
        }
    }
}
