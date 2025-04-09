#ifndef SENTINEL_H
#define SENTINEL_H
#include "sds.h"

// 此函数使用哨兵特定的默认值覆盖一些正常的
void initSentinelConfig(void);
// 执行哨兵模式初始化。
void initSentinel(void);
// 哨兵模式下运行的定时任务
void sentinelTimer(void);
const char *sentinelHandleConfiguration(char **argv, int argc);
void queueSentinelConfig(sds *argv, int argc, int linenum, sds line);
void loadSentinelConfigFromQueue(void);
void sentinelIsRunning(void);

#endif // SENTINEL_H
