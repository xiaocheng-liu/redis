#ifndef SENTINEL_H
#define SENTINEL_H
#include "sds.h"

/* Sentinel */
// 哨兵
void initSentinelConfig(void);
void initSentinel(void);
void sentinelTimer(void);
const char *sentinelHandleConfiguration(char **argv, int argc);
void queueSentinelConfig(sds *argv, int argc, int linenum, sds line);
void loadSentinelConfigFromQueue(void);
void sentinelIsRunning(void);

#endif // SENTINEL_H
