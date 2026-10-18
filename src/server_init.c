/* Redis 服务器运行时初始化。 */
#include <errno.h>
#include <fcntl.h>
#include <sys/syslog.h>

#include "ae.h"
#include "bio.h"
#include "cluster.h"
#include "evict.h"
#include "latency.h"
#include "pubsub.h"
#include "server.h"
#include "slowlog.h"

void initServer(void) {
    int j;

    // 【1】设置UNIX信号处理函数,使Redis服务器收到SIGINT信号后退出程序。
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers(); // 设置信号处理

    // 【2】设置线程随时响应CANCEL信号,终止线程,以便停止程序。
    makeThreadKillable();

    // 【3】如果开启了Unix系统日志,则调用openlog函数与Unix系统日志建立输出连接,以便输出系统日志。
    if (server.syslog_enabled) {
        openlog(server.syslog_ident, LOG_PID | LOG_NDELAY | LOG_NOWAIT, server.syslog_facility);
    }

    /* Initialization after setting defaults from the config system. */
    // 【4】初始化server中负责存储运行时数据的相关属性。
    server.aof_state = server.aof_enabled ? AOF_ON : AOF_OFF;
    server.hz = server.config_hz; // redis 定时任务触发的频率
    server.pid = getpid(); // 主进程ID
    server.in_fork_child = CHILD_TYPE_NONE; // 是否是fork的子进程
    server.main_thread_id = pthread_self(); // 主线程ID
    server.current_client = NULL; // 当前正在执行命令的客户端
    server.errors = raxNew(); // 错误表
    server.fixed_time_expire = 0;
    server.clients = listCreate(); // 客户端
    server.clients_index = raxNew();
    server.clients_to_close = listCreate(); // 要异步关闭的客户端
    server.slaves = listCreate();
    server.monitors = listCreate();
    server.clients_pending_write = listCreate();
    server.clients_pending_read = listCreate();
    server.clients_timeout_table = raxNew();
    server.slaveseldb = -1; /* Force to emit the first SELECT command. */
    server.unblocked_clients = listCreate();
    server.ready_keys = listCreate();
    server.clients_waiting_acks = listCreate();
    server.get_ack_from_slaves = 0;
    server.client_pause_type = 0;
    server.paused_clients = listCreate();
    server.events_processed_while_blocked = 0;
    server.system_memory_size = zmalloc_get_memory_size();
    server.blocked_last_cron = 0;
    server.blocking_op_nesting = 0;

    // 检查tls配置
    if ((server.tls_port || server.tls_replication || server.tls_cluster) &&
        tlsConfigure(&server.tls_ctx_config) == C_ERR) {
        serverLog(LL_WARNING, "Failed to configure TLS. Check logs for more info.");
        exit(1);
    }

    // 【5】createSharedObjects函数创建共享数据集,这些数据可在各场景中共享使用,
    //  如小数字0～9999、常用字符串+OK\r\n(命令处理成功响应字符串)、+PONG\r\n(ping命令响应字符串)。
    //  adjustOpenFilesLimit函数尝试修改环境变量,提高系统允许打开的文件描述符上限,
    //  避免由于大量客户端连接(Socket文件描述符)导致错误。
    createSharedObjects();
    adjustOpenFilesLimit();
    const char *clk_msg = monotonicInit();
    serverLog(LL_NOTICE, "monotonic clock: %s", clk_msg);

    // 【6】创建事件循环器。
    //  初始化server.el ,注意在这里的aeCreateEventLoop内部调用了epoll_create。
    //  传入的最大文件描述符个数为客户端最大连接数+宏定义CONFIG_FDSET_INCR的大小
    server.el = aeCreateEventLoop(server.maxclients + CONFIG_FDSET_INCR);
    if (server.el == NULL) {
        serverLog(LL_WARNING, "Failed creating the event loop. Error message: '%s'", strerror(errno));
        exit(1);
    }

    // 分配数据库空间
    server.db = zmalloc(sizeof(redisDb) * server.dbnum);

    /* 打开用户命令的 TCP 侦听套接字。 */
    /* Open the TCP listening socket for the user commands. */
    // 【7】如果配置了server.port,则开启TCP Socket服务,接收用户请求。
    //  如果配置了server.tls_ port,则开启TLS Socket服务,Redis 6.0开始支持TLS连接。
    //  如果配置了server.unixsocket,则开启UNIX Socket服务。如果上面3个选项都没有配置,则报错退出。
    if (server.port != 0 && listenToPort(server.port, server.ipfd, &server.ipfd_count) == C_ERR) {
        exit(1);
    }
    if (server.tls_port != 0 && listenToPort(server.tls_port, server.tlsfd, &server.tlsfd_count) == C_ERR) {
        exit(1);
    }

    /* Open the listening Unix domain socket. */
    // 打开监听的Unix域套接字。
    if (server.unixsocket != NULL) {
        unlink(server.unixsocket); /* don't care if this fails */
        server.sofd = anetUnixServer(server.neterr, server.unixsocket, server.unixsocketperm, server.tcp_backlog);
        if (server.sofd == ANET_ERR) {
            serverLog(LL_WARNING, "Opening Unix socket: %s", server.neterr);
            exit(1);
        }
        anetNonBlock(NULL, server.sofd);
        anetCloexec(server.sofd);
    }

    /* Abort if there are no listening sockets at all. */
    // 如果根本没有监听套接字,则中止。
    if (server.ipfd_count == 0 && server.tlsfd_count == 0 && server.sofd < 0) {
        serverLog(LL_WARNING, "Configured to not listen anywhere, exiting.");
        exit(1);
    }

    /* Create the Redis databases, and initialize other internal state. */
    // 【8】初始化数据库server.db,并初始化其他内部状态,用于存储数据。
    for (j = 0; j < server.dbnum; j++) {
        server.db[j].dict = dictCreate(&dbDictType, NULL);
        server.db[j].expires = dictCreate(&dbExpiresDictType, NULL);
        server.db[j].expires_cursor = 0;
        server.db[j].blocking_keys = dictCreate(&keylistDictType, NULL);
        server.db[j].ready_keys = dictCreate(&objectKeyPointerValueDictType, NULL);
        server.db[j].watched_keys = dictCreate(&keylistDictType, NULL);
        server.db[j].id = j;
        server.db[j].avg_ttl = 0;
        server.db[j].defrag_later = listCreate();
        listSetFreeMethod(server.db[j].defrag_later, (void (*)(void *)) sdsfree);
    }

    // 【9】evictionPoolAlloc函数初始化LRU/LFU样本池,用于实现LRU/LFU近似算法。
    //  继续初始化server中存储运行时数据的相关属性:
    //  该数组的大小由宏定义 EVPOOL_SIZE(在 evict.c 文件中)决定,默认是 16 个元素,也就是可以保存 16
    //  个待淘汰的候选键值对。
    evictionPoolAlloc(); /* Initialize the LRU keys pool. */

    // 发布订阅相关
    server.pubsub_channels = dictCreate(&keylistDictType, NULL);
    server.pubsub_patterns = listCreate();
    server.pubsub_patterns_dict = dictCreate(&keylistDictType, NULL);
    listSetFreeMethod(server.pubsub_patterns, freePubsubPattern);
    listSetMatchMethod(server.pubsub_patterns, listMatchPubsubPattern);

    server.cronloops = 0;
    server.in_eval = 0;
    server.in_exec = 0;
    server.propagate_in_transaction = 0;
    server.client_pause_in_transaction = 0;
    server.child_pid = -1;
    server.child_type = CHILD_TYPE_NONE;
    server.rdb_child_type = RDB_CHILD_TYPE_NONE;
    server.rdb_pipe_conns = NULL;
    server.rdb_pipe_numconns = 0;
    server.rdb_pipe_numconns_writing = 0;
    server.rdb_pipe_buff = NULL;
    server.rdb_pipe_bufflen = 0;
    server.rdb_bgsave_scheduled = 0;
    server.child_info_pipe[0] = -1;
    server.child_info_pipe[1] = -1;
    server.child_info_nread = 0;
    aofRewriteBufferReset();
    server.aof_buf = sdsempty();
    server.lastsave = time(NULL); /* At startup we consider the DB saved. */
    server.lastbgsave_try = 0; /* At startup we never tried to BGSAVE. */
    server.rdb_save_time_last = -1;
    server.rdb_save_time_start = -1;
    server.dirty = 0;
    // 重置服务器统计
    resetServerStats();

    /* A few stats we don't want to reset: server startup time, and peak mem. */
    // 我们不想重置的一些统计数据:服务器启动时间和峰值内存。
    server.stat_starttime = time(NULL);
    server.stat_peak_memory = 0;
    server.stat_current_cow_bytes = 0;
    server.stat_rdb_cow_bytes = 0;
    server.stat_aof_cow_bytes = 0;
    server.stat_module_cow_bytes = 0;
    for (j = 0; j < CLIENT_TYPE_COUNT; j++) {
        server.stat_clients_type_memory[j] = 0;
    }
    server.cron_malloc_stats.zmalloc_used = 0;
    server.cron_malloc_stats.process_rss = 0;
    server.cron_malloc_stats.allocator_allocated = 0;
    server.cron_malloc_stats.allocator_active = 0;
    server.cron_malloc_stats.allocator_resident = 0;
    server.lastbgsave_status = C_OK;
    server.aof_last_write_status = C_OK;
    server.aof_last_write_errno = 0;
    server.repl_good_slaves_count = 0;

    /* Create the timer callback, this is our way to process many background
     * operations incrementally, like clients timeout, eviction of unaccessed
     * expired keys and so forth. */
    // 【10】创建一个时间事件,执行函数为serverCron,负责处理Redis中的定时任务,如清理过期数据、删除未访问的过期key,生成RDB文件等。
    if (aeCreateTimeEvent(server.el, 1, serverCron, NULL, NULL) == AE_ERR) {
        serverPanic("Can't create event loop timers.");
        exit(1);
    }

    /* Create an event handler for accepting new connections in TCP and Unix
     * domain sockets. */
    /* TCP新连接是可以读事件,这里指定了处理TCP连接的handler为acceptTcpHandler函数*/
    // 【11】分别为TCP Socket、TSL Sockets、UNIX Socket注册监听AE_READABLE类型的文件事件,
    //  事件处理函数分别为acceptTcpHandler、acceptTLSHandler、acceptUnixHandler,这些函数负责接收Socket中的新连接,
    for (j = 0; j < server.ipfd_count; j++) {
        // 注册监听事件,server.ipfd是TCP文件描述符,AE_READABLE可读事件,acceptTcpHandler事件处理回调函数
        if (aeCreateFileEvent(server.el, server.ipfd[j], AE_READABLE, acceptTcpHandler, NULL) == AE_ERR) {
            serverPanic("Unrecoverable error creating server.ipfd file event.");
        }
    }

    for (j = 0; j < server.tlsfd_count; j++) {
        if (aeCreateFileEvent(server.el, server.tlsfd[j], AE_READABLE, acceptTLSHandler, NULL) == AE_ERR) {
            serverPanic("Unrecoverable error creating server.tlsfd file event.");
        }
    }
    if (server.sofd > 0 && aeCreateFileEvent(server.el, server.sofd, AE_READABLE, acceptUnixHandler, NULL) == AE_ERR) {
        serverPanic("Unrecoverable error creating server.sofd file event.");
    }

    /* Register a readable event for the pipe used to awake the event loop
     * when a blocked client in a module needs attention. */
    /* 为管道注册一个用于唤醒事件循环的可读事件,需要注意模块中被阻塞的客户端 */
    if (aeCreateFileEvent(server.el, server.module_blocked_pipe[0], AE_READABLE, moduleBlockedClientPipeReadable,
                          NULL) == AE_ERR) {
        serverPanic("Error registering the readable event for the module "
                    "blocked clients subsystem.");
    }

    /* Register before and after sleep handlers (note this needs to be done
     * before loading persistence since it is used by processEventsWhileBlocked. */
    // 注册before和after睡眠函数(注意要在加载持久化的数据之前进行,因为它会被
    // processEventsWhileBlocked函数用到
    // 【12】注册事件循环器的钩子函数,事件循环器在每次阻塞前后都会调用钩子函数。
    aeSetBeforeSleepProc(server.el, beforeSleep);
    aeSetAfterSleepProc(server.el, afterSleep);

    /* Open the AOF file if needed. */
    // 【13】如果开启了AOF,则预先打开AOF文件。
    if (server.aof_state == AOF_ON) {
        server.aof_fd = open(server.aof_filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (server.aof_fd == -1) {
            serverLog(LL_WARNING, "Can't open the append-only file: %s", strerror(errno));
            exit(1);
        }
    }

    /* 32 bit instances are limited to 4GB of address space, so if there is
     * no explicit limit in the user provided configuration we set a limit
     * at 3 GB using maxmemory with 'noeviction' policy'. This avoids
     * useless crashes of the Redis instance for out of memory. */
    // 如果Redis运行在32位操作系统上,由于32位操作系统内存空间限制为4GB,所以将Redis使用内存限制为3GB,避免Redis服务器因内存不足而崩溃。
    if (server.arch_bits == 32 && server.maxmemory == 0) {
        serverLog(LL_WARNING, "Warning: 32 bit instance detected but no memory limit set. Setting 3 GB maxmemory limit "
                              "with 'noeviction' policy now.");
        server.maxmemory = 3072LL * (1024 * 1024); /* 3 GB */
        server.maxmemory_policy = MAXMEMORY_NO_EVICTION;
    }

    // 【15】如果以Cluster模式启动,则调用clusterInit函数初始化Cluster机制。
    if (server.cluster_enabled) {
        clusterInit();
    }

    // replicationScriptCacheInit函数初始化server.repl_scriptcache_dict属性。
    replicationScriptCacheInit();
    // scriptingInit函数初始化LUA机制。
    scriptingInit(1);
    // slowlogInit函数初始化慢日志机制。
    slowlogInit();
    // latencyMonitorInit函数初始化延迟监控机制。
    latencyMonitorInit();
}



void makeThreadKillable(void) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

// Redis初始化过程
/* Some steps in server initialization need to be done last (after modules
 * are loaded).
 * Specifically, creation of threads due to a race bug in ld.so, in which
 * Thread Local Storage initialization collides with dlopen call.
 * see: https://sourceware.org/bugzilla/show_bug.cgi?id=19329 */
// 服务器初始化中的某些步骤需要最后完成(在模块之后)已加载)。
// 具体来说,由于 ld.so 中的比赛错误而创建线程,其中线程本地存储初始化与 dlopen 调用冲突。
void InitServerLast(void) {
    // 启动后台线程,目前是3个后台线程 bio_close_file  bio_aof_fsync  bio_lazy_free
    bioInit();
    // 调用initThreadedIO函数初始化IO线程
    initThreadedIO();
    // 设置后台jemalloc线程,进行内存管理
    set_jemalloc_bg_thread(server.jemalloc_bg_thread);
    // 设置服务器内存使用量
    server.initial_memory_usage = zmalloc_used_memory();
}



int listenToPort(int port, int *fds, int *count) {
    int j;
    char **bindaddr = server.bindaddr;
    int bindaddr_count = server.bindaddr_count;
    char *default_bindaddr[2] = {"*", "-::*"};

    /* Force binding of 0.0.0.0 if no bind address is specified. */
    // 如果未指定绑定地址,则强制绑定 0.0.0.0。
    if (server.bindaddr_count == 0) {
        bindaddr_count = 2;
        bindaddr = default_bindaddr;
    }

    for (j = 0; j < bindaddr_count; j++) {
        char *addr = bindaddr[j];
        int optional = *addr == '-';
        if (optional)
            addr++;
        if (strchr(addr, ':')) {
            /* Bind IPv6 address. */
            fds[*count] = anetTcp6Server(server.neterr, port, addr, server.tcp_backlog);
        } else {
            /* Bind IPv4 address. */
            fds[*count] = anetTcpServer(server.neterr, port, addr, server.tcp_backlog);
        }
        if (fds[*count] == ANET_ERR) {
            serverLog(LL_WARNING, "Could not create server TCP listening socket %s:%d: %s", addr, port, server.neterr);
            if (errno == EADDRNOTAVAIL && optional)
                continue;
            if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT || errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
                errno == EAFNOSUPPORT)
                continue;
            return C_ERR;
        }
        anetNonBlock(NULL, fds[*count]);
        anetCloexec(fds[*count]);
        (*count)++;
    }
    return C_OK;
}

