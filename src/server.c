#include <locale.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "ae.h"
#include "crc64.h"
#include "dict.h"
#include "mt19937-64.h"
#include "server.h"
#include "util.h"

/* Our shared "common" objects */
struct sharedObjectsStruct shared;

/* Global vars that are actually used as constants. The following double
 * values are used for double on-disk serialization, and are initialized
 * at runtime to avoid strange compiler optimizations. */
double R_Zero, R_PosInf, R_NegInf, R_Nan;

/*================================= Globals ================================= */

/* Global vars */
struct redisServer server; /* Server global state */

// Redis 服务主入口。
int main(int argc, char **argv) {
    // 精确时间
    struct timeval tv;

    // 用于redis测试数据结构
    redis_test(argc, argv);
    // 此函数的功能可能是初始化一个进程标题替换机制,通常用于设置或修改当前进程的名称,便于调试或监控。
    init_setproctitle_replacement(argc, argv);

    // 设置本地化信息为字符串比较变量中的默认设置
    setlocale(LC_COLLATE, "");
    // 设置时间环境变量
    tzset(); /* Populates 'timezone' global. */
    // 内存超出的handler(记录日志)
    zmalloc_set_oom_handler(redisOutOfMemoryHandler);
    // 使用当前时间和进程ID的异或值作为种子初始化标准库中的随机数生成器。
    srand(time(NULL) ^ getpid());
    // 使用当前时间和进程ID的异或值作为种子初始化POSIX标准的随机数生成器
    srandom(time(NULL) ^ getpid());

    // 精确时间
    gettimeofday(&tv, NULL);
    // 使用种子初始化 mt[NN]
    init_genrand64(((long long) tv.tv_sec * 1000000 + tv.tv_usec) ^ getpid());
    // 初始化 16KB 查找表
    crc64_init();

    /* Store umask value. Because umask(2) only offers a set-and-get API we have
     * to reset it and restore it back. We do this early to avoid a potential
     * race condition with threads that could be creating files or directories.
     */
    // 存储掩码值。因为 umask(2) 只提供了一个设置和获取 API,所以我们必须重置它并恢复它。
    // 我们尽早执行此操作是为了避免可能正在创建文件或目录的线程出现潜在的争用情况。
    umask(server.umask = umask(0777));

    // hash算法种子
    uint8_t hashseed[16];
    // 获取随机字节
    getRandomBytes(hashseed, sizeof(hashseed));
    // 设置哈希种子
    dictSetHashFunctionSeed(hashseed);

    // 检查该Redis服务器是否以sentinel模式启动。
    server.sentinel_mode = checkForSentinelMode(argc, argv);

    // initServerConfig函数将redisServer中记录配置项的属性初始化为默认值。
    initServerConfig();
    // ACLInit函数初始化ACL机制。
    // ACL 子系统必须尽快初始化,因为基本网络代码和客户端创建依赖于它。ASAP: as soon as possible
    ACLInit(); /* The ACL subsystem must be initialized ASAP because the
                   basic networking code and client creation depends on it. */

    // moduleInitModulesSystem函数初始化Module机制。
    moduleInitModulesSystem();
    // tlsInit函数初始化TLS机制。
    tlsInit();
    /* Check if we need to start in redis-check-rdb/aof mode. We just execute
     * the program main. However, the program is part of the Redis executable
     * so that we can easily execute an RDB check on loading errors. */
    // 如果启动程序是redis-check-rdb或redis-check-aof,
    // 则执行redis_check_rdb_main或redis_check_aof_main函数,
    // 它们尝试检验并修复RDB、AOF文件后便退出程序。
    checkRdbOrAof(argc, argv);

    /* We need to init sentinel right now as parsing the configuration file
     * in sentinel mode will have the effect of populating the sentinel
     * data structures with master nodes to monitor.
     */
    // 我们现在需要初始化 sentinel,因为在 sentinel 模式下解析配置文件将具有使用要监控的主节点填充 sentinel
    // 数据结构的效果。 如果以Sentinel模式启动,则初始化Sentinel机制。
    checkAndInitSentenel();

    /* Store the executable path and arguments in a safe place in order
     * to be able to restart the server later. */
    // 记录Redis程序可执行路径及启动参数,以便后续重启服务器。
    parseArgv(argc, argv);

    // server.supervised属性指定是否以upstart服务或systemd服务启动Redis。
    //  如果配置了server.daemonize且没有配置server.supervised,则以守护进程的方式启动Redis。
    server.supervised = redisIsSupervised(server.supervised_mode);
    // 守护进程
    int background = server.daemonize && !server.supervised;
    if (background) {
        // 该函数 daemonize 用于将进程转换为守护进程
        daemonize();
    }

    // 打印启动日志。
    printStartLog(argc, argv);
    // 打印启动ascii_logo
    redisAsciiArt();

    // readOOMScoreAdj() 是一个函数调用,其功能是读取系统的 OOM(Out of
    // Memory)调整值。该值用于控制进程在内存不足时被操作系统杀死的优先级。
    readOOMScoreAdj();

    // initServer函数初始化Redis运行时数据,aeCreateEventLoop函数创建事件循环器,createPidFile函数创建pid文件。
    initServer();

    // 如果设置了后台运行模式或PID文件路径,则创建PID文件
    if (background || server.pidfile) {
        // 创建Pid文件
        createPidFile();
    }
    // 如果设置了进程标题,则设置Redis进程标题
    if (server.set_proc_title) {
        // 设置Redis进程标题
        redisSetProcTitle(NULL);
    }

    // 检查tcp_backlog
    checkTcpBacklogSettings();

    // 初始化其他的机制
    initServerOther();

    /* Warning the user about suspicious maxmemory setting. */
    // 检查最大内存是否小于1M,并给与警告提示
    checkMaxmemory();

    // 尽可能将Redis主线程绑定到server.server_cpulist配置的CPU列表上,
    //  Redis 4开始使用多线程,该操作可以减少不必要的线程切换,提高性能。
    redisSetCpuAffinity(server.server_cpulist);
    // 设置OOM Score调整值
    setOOMScoreAdj(-1);
    // 启动事件循环器。事件循环器是Redis中的重要组件。在Redis运行期间,由事件循环器提供服务。启动eventLoop开始接受请求
    aeMain(server.el);
    // 执行到这里,说明Redis服务已停止,aeDeleteEventLoop函数清除事件循环器中的事件,最后退出程序。
    aeDeleteEventLoop(server.el);
    return 0;
}
/* The End */
