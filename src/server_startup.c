/* 启动日志、帮助信息和启动模式检测。 */
#include <errno.h>

#include "sentinel.h"
#include "cluster.h"
#include "server.h"
#include "util.h"
#include "version.h"

void printStartLog(int argc, char **argv) {
    // serverLog(LL_WARNING, "Redis服务正在启动。。。");
    serverLog(LL_WARNING, "oO0OoO0OoO0Oo Redis is starting oO0OoO0OoO0Oo");
    serverLog(LL_WARNING, "Redis version=%s, bits=%d, commit=%s, modified=%d, pid=%d, just started", REDIS_VERSION,
              (sizeof(long) == 8) ? 64 : 32, redisGitSHA1(), strtol(redisGitDirty(), NULL, 10) > 0, (int) getpid());

    if (argc == 1) {
        serverLog(LL_WARNING,
                  "Warning: no config file specified, using the default config. In order to specify a config file use "
                  "%s /path/to/%s.conf",
                  argv[0], server.sentinel_mode ? "sentinel" : "redis");
    } else {
        serverLog(LL_WARNING, "Configuration loaded");
    }
}

// 打印版本
void version(void) {
    printf("Redis server v=%s sha=%s:%d malloc=%s bits=%d build=%llx\n", REDIS_VERSION, redisGitSHA1(),
           atoi(redisGitDirty()) > 0, ZMALLOC_LIB, sizeof(long) == 4 ? 32 : 64, (unsigned long long) redisBuildId());
    exit(0);
}

// 打印帮助
void usage(void) {
    fprintf(stderr, "Usage: ./redis-server [/path/to/redis.conf] [options] [-]\n");
    fprintf(stderr, "       ./redis-server - (read config from stdin)\n");
    fprintf(stderr, "       ./redis-server -v or --version\n");
    fprintf(stderr, "       ./redis-server -h or --help\n");
    fprintf(stderr, "       ./redis-server --test-memory <megabytes>\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "       ./redis-server (run the server with default conf)\n");
    fprintf(stderr, "       ./redis-server /etc/redis/6379.conf\n");
    fprintf(stderr, "       ./redis-server --port 7777\n");
    fprintf(stderr, "       ./redis-server --port 7777 --replicaof 127.0.0.1 8888\n");
    fprintf(stderr, "       ./redis-server /etc/myredis.conf --loglevel verbose -\n");
    fprintf(stderr, "       ./redis-server /etc/myredis.conf --loglevel verbose\n\n");
    fprintf(stderr, "Sentinel mode:\n");
    fprintf(stderr, "       ./redis-server /etc/sentinel.conf --sentinel\n");
    exit(1);
}

// 打印Art
void redisAsciiArt(void) {
#include "asciilogo.h"
    char *buf = zmalloc(1024 * 16);
    char *mode;

    if (server.cluster_enabled) // 判断是否为集群模式
        mode = "cluster";
    else if (server.sentinel_mode) // 判断是否为哨兵模式
        mode = "sentinel";
    else
        mode = "standalone";

    /* Show the ASCII logo if: log file is stdout AND stdout is a
     * tty AND syslog logging is disabled. Also show logo if the user
     * forced us to do so via redis.conf. */
    int show_logo = ((!server.syslog_enabled && server.logfile[0] == '\0' && isatty(fileno(stdout))) ||
                     server.always_show_logo);

    if (!show_logo) // 不显示logo
    {
        serverLog(LL_NOTICE, "Running mode=%s, port=%d.", mode, server.port ? server.port : server.tls_port);
    } else // 显示logo
    {
        snprintf(buf, 1024 * 16, ascii_logo, REDIS_VERSION, redisGitSHA1(), strtol(redisGitDirty(), NULL, 10) > 0,
                 (sizeof(long) == 8) ? "64" : "32", mode, server.port ? server.port : server.tls_port, (long) getpid());
        serverLogRaw(LL_NOTICE | LL_RAW, buf);
    }
    zfree(buf); // 释放buf
}

/* Returns 1 if there is --sentinel among the arguments or if
 * argv[0] contains "redis-sentinel". */
// 如果参数中有 --sentinel,或者 argv[0] 包含 "redis-sentinel",则返回 1。
int checkForSentinelMode(int argc, char **argv) {
    int j;
    // 查看参数中是否有redis-sentinel子串
    if (strstr(argv[0], "redis-sentinel") != NULL)
        return 1;
    // 逐个比较是否有--sentinel参数
    for (j = 1; j < argc; j++)
        if (!strcmp(argv[j], "--sentinel"))
            return 1;
    return 0;
}

/* Function called at startup to load RDB or AOF file in memory. */
// 启动时调用的函数以在内存中加载 RDB 或 AOF 文件。


void parseArgv(int argc, char **argv) {
    int j = 0;
    /* Store the executable path and arguments in a safe place in order
     * to be able to restart the server later. */
    // 记录Redis程序可执行路径及启动参数,以便后续重启服务器。
    server.executable = getAbsolutePath(argv[0]);

    server.exec_argv = zmalloc(sizeof(char *) * (argc + 1));
    server.exec_argv[argc] = NULL;
    // serverLog(LL_WARNING, "参数个数:%d", argc);
    for (j = 0; j < argc; j++) {
        // serverLog(LL_WARNING, "参数[%d]:%s", j, argv[j]);
        server.exec_argv[j] = zstrdup(argv[j]);
    }

    if (argc >= 2) // 如果启动参数大于等于2
    {
        char config_from_stdin = 0;
        j = 1; /* First option to parse in argv[] */ // 在 argv 中解析的第一个选项
        sds options = sdsempty();

        /* Handle special options --help and --version */
        // 对-v、--version、--help、-h、--test-memory等命令进行优先处理。
        // strcmp函数比较两个字符串str1、str2,若str1=str2,则返回零;若str1 != str2,则返回正数。
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            version();
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage();
        }
        if (strcmp(argv[1], "--test-memory") == 0) {
            if (argc == 3) // 如果参数个数为3个, 进行内存测试
            {
                memtest(atoi(argv[2]), 50);
                exit(0);
            } else // 参数不是3个,给错误提示
            {
                fprintf(stderr, "Please specify the amount of memory to test in megabytes.\n");
                fprintf(stderr, "Example: ./redis-server --test-memory 4096\n\n");
                exit(1);
            }
        }

        /* Parse command line options
         * Precedence wise, File, stdin, explicit options -- last config is the one that matters.
         *
         * First argument is the config file name? */
        // 如果启动命令的第二个参数不是以"-"开头的,则是配置文件参数,将配置文件路径转化为绝对路径,存入server.configfile中
        if (argv[1][0] != '-') {
            /* Replace the config file in server.exec_argv with its absolute path. */
            server.configfile = getAbsolutePath(argv[1]);
            zfree(server.exec_argv[1]);
            server.exec_argv[1] = zstrdup(server.configfile);
            j = 2; // Skip this arg when parsing options
        }

        // 【8】读取启动命令中的启动配置项,并将它们拼接到一个字符串中。
        while (j < argc) {
            /* Either first or last argument - Should we read config from stdin? */
            // 第一个或最后一个参数 - 我们应该从标准输入中读取配置吗？
            if (argv[j][0] == '-' && argv[j][1] == '\0' && (j == 1 || j == argc - 1)) {
                config_from_stdin = 1;
            }
            /* All the other options are parsed and conceptually appended to the
             * configuration file. For instance --port 6380 will generate the
             * string "port 6380\n" to be parsed after the actual config file
             * and stdin input are parsed (if they exist). */
            else if (argv[j][0] == '-' && argv[j][1] == '-') {
                /* Option name */
                if (sdslen(options))
                    options = sdscat(options, "\n");
                options = sdscat(options, argv[j] + 2);
                options = sdscat(options, " ");
            } else {
                /* Option argument */
                options = sdscatrepr(options, argv[j], strlen(argv[j]));
                options = sdscat(options, " ");
            }
            j++;
        }

        // 【9】以Sentinel模式启动,必须指定配置文件,否则直接报错退出。
        if (server.sentinel_mode && !server.configfile) {
            serverLog(LL_WARNING, "Sentinel needs config file on disk to save state.  Exiting...");
            exit(1);
        }
        // 【10】resetServerSaveParams函数重置server.saveparams属性(该属性存放RDB SAVE配置)。
        // loadServerConfig函数从配置文件中加载所有配置项,并使用启动命令配置项覆盖配置文件中的配置项。
        loadServerConfig(server.configfile, config_from_stdin, options);

        // 如果是哨兵模式,加载哨兵配置
        if (server.sentinel_mode) {
            loadSentinelConfigFromQueue();
        }
        sdsfree(options);
    }
}



void redisOutOfMemoryHandler(size_t allocation_size) {
    serverLog(LL_WARNING, "Out Of Memory allocating %zu bytes!", allocation_size);
    serverPanic("Redis aborting for OUT OF MEMORY. Allocating %zu bytes!", allocation_size);
}


void redis_test(int const argc, char *const *argv) {
    UNUSED(argc);
    UNUSED(argv);
#ifdef REDIS_TEST
    if (argc == 3 && !strcasecmp(argv[1], "test")) {
        if (!strcasecmp(argv[2], "ziplist")) {
            return ziplistTest(argc, argv);
        } else if (!strcasecmp(argv[2], "quicklist")) {
            quicklistTest(argc, argv);
        } else if (!strcasecmp(argv[2], "intset")) {
            return intsetTest(argc, argv);
        } else if (!strcasecmp(argv[2], "zipmap")) {
            return zipmapTest(argc, argv);
        } else if (!strcasecmp(argv[2], "sha1test")) {
            return sha1Test(argc, argv);
        } else if (!strcasecmp(argv[2], "util")) {
            return utilTest(argc, argv);
        } else if (!strcasecmp(argv[2], "endianconv")) {
            return endianconvTest(argc, argv);
        } else if (!strcasecmp(argv[2], "crc64")) {
            return crc64Test(argc, argv);
        } else if (!strcasecmp(argv[2], "zmalloc")) {
            return zmalloc_test(argc, argv);
        } else if (!strcasecmp(argv[2], "sds")) {
            return sdsTest(argc, argv);
        }

        return -1; /* test not found */
    }
#endif
}



void loadDataFromDisk(void) {
    long long start = ustime();
    //  如果AOF开启
    if (server.aof_state == AOF_ON) {
        // 加载AOF文件
        serverLog(LL_NOTICE, "Starting AOF loading from '%s'", server.aof_filename);
        if (loadAppendOnlyFile(server.aof_filename) == C_OK) {
            serverLog(LL_NOTICE, "DB loaded from append only file: %.3f seconds", (float) (ustime() - start) / 1000000);
        }
    } else {
        rdbSaveInfo rsi = RDB_SAVE_INFO_INIT;
        errno = 0; /* Prevent a stale value from affecting error checking */
        // 加载RDB文件
        serverLog(LL_NOTICE, "Starting RDB loading from '%s'", server.rdb_filename);
        if (rdbLoad(server.rdb_filename, &rsi, RDBFLAGS_NONE) == C_OK) {
            serverLog(LL_NOTICE, "DB loaded from disk: %.3f seconds", (float) (ustime() - start) / 1000000);

            /* Restore the replication ID / offset from the RDB file. */
            // 从 RDB 文件还原复制 ID 偏移量。
            // nodeIsSlave 判断是否是从节点
            if ((server.masterhost || (server.cluster_enabled && nodeIsSlave(server.cluster->myself))) &&
                rsi.repl_id_is_set && rsi.repl_offset != -1 &&
                /* Note that older implementations may save a repl_stream_db
                 * of -1 inside the RDB file in a wrong way, see more
                 * information in function rdbPopulateSaveInfo. */
                rsi.repl_stream_db != -1) {
                memcpy(server.replid, rsi.repl_id, sizeof(server.replid));
                server.master_repl_offset = rsi.repl_offset;
                /* If we are a slave, create a cached master from this
                 * information, in order to allow partial resynchronizations
                 * with masters. */
                // 如果我们是从属服务器,请根据此信息创建一个缓存的主节点,以允许与主服务器进行部分重新同步。
                replicationCacheMasterUsingMyself();
                selectDb(server.cached_master, rsi.repl_stream_db);
            }
        } else if (errno != ENOENT) {
            serverLog(LL_WARNING, "Fatal error loading the DB: %s. Exiting.", strerror(errno));
            exit(1);
        }
    }
}

// Redis内存溢出处理函数
/* Callback for sdstemplate on proc-title-template. See redis.conf for
 * supported variables.
 */
/* 测试 */
// redis模式运行
void redisRun(void) {
    /* Things not needed when running in Sentinel mode. */
    // 在哨兵模式下运行时不需要的东西。
    serverLog(LL_WARNING, "Server initialized");
#ifdef __linux__
    linuxMemoryWarnings();
#if defined(__arm64__)
    int ret;
    if ((ret = linuxMadvFreeForkBugCheck())) {
        if (ret == 1)
            serverLog(LL_WARNING,
                      "WARNING Your kernel has a bug that could lead to data corruption during background save. "
                      "Please upgrade to the latest stable kernel.");
        else
            serverLog(LL_WARNING,
                      "Failed to test the kernel for a bug that could lead to data corruption during background save. "
                      "Your system could be affected, please report this error.");
        if (!checkIgnoreWarning("ARM64-COW-BUG")) {
            serverLog(LL_WARNING, "Redis will now exit to prevent data corruption. "
                                  "Note that it is possible to suppress this warning by setting the following config: "
                                  "ignore-warnings ARM64-COW-BUG");
            exit(1);
        }
    }
#endif /* __arm64__ */
#endif /* __linux__ */

    // 最后初始化Module机制。
    moduleInitModulesSystemLast();
    // 【14】如果非Sentinel模式启动,则完成以下操作:
    //  (1)moduleLoadFromQueue函数加载配置文件指定的Module模块;
    moduleLoadFromQueue();
    // (2)ACLLoadUsersAtStartup函数加载ACL用户控制列表;
    ACLLoadUsersAtStartup();
    // (3)InitServerLast函数负责创建后台线程、I/O线程,该步骤需在Module模块加载后再执行;
    InitServerLast();
    // (4)开启aof或者rdb持久化时,会尝试从文件中恢复之前的redis数据
    loadDataFromDisk();
    // (5)如果以Cluster模式启动,那么还需要验证加载的数据是否正确。集群模式下配置文件的检测
    if (server.cluster_enabled) {
        if (verifyClusterConfigWithData() == C_ERR) {
            serverLog(LL_WARNING, "You can't have keys in a DB different than DB 0 when in "
                                  "Cluster mode. Exiting.");
            exit(1);
        }
    }
    if (server.ipfd_count > 0 || server.tlsfd_count > 0) {
        serverLog(LL_NOTICE, "Ready to accept connections");
    }
    if (server.sofd > 0) {
        serverLog(LL_NOTICE, "The server is now ready to accept connections at %s", server.unixsocket);
    }
    if (server.supervised_mode == SUPERVISED_SYSTEMD) {
        if (!server.masterhost) {
            redisCommunicateSystemd("STATUS=Ready to accept connections\n");
            redisCommunicateSystemd("READY=1\n");
        } else {
            redisCommunicateSystemd("STATUS=Waiting for MASTER <-> REPLICA sync\n");
        }
    }
}

// sentinel模式运行
void sentinelRun(void) {
    // 加载ACL用户控制列表
    ACLLoadUsersAtStartup();
    // 【15】如果以Sentinel模式启动,则调用sentinelIsRunning函数启动Sentinel机制。
    InitServerLast(); // 初始化一些后台线程
    sentinelIsRunning(); // sentinel模式的配置初始化操作
    if (server.supervised_mode == SUPERVISED_SYSTEMD) {
        redisCommunicateSystemd("STATUS=Ready to accept connections\n");
        redisCommunicateSystemd("READY=1\n");
    }
}

// 初始化其他的
void initServerOther(void) {
    // 如果不是哨兵模式
    if (!server.sentinel_mode) {
        redisRun();
    } else {
        sentinelRun();
    }
}

/**
 * @brief
 *
 * @param argc
 * @param argv
 * @return int
 */
// server.c/main是Redis启动方法,负责加载配置,初始化数据库,启动网络服务,创建并启动事件循环器。
