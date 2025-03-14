#ifndef EVICT_H
#define EVICT_H

#include "server.h"

/* ----------------------------------------------------------------------------
 * Data structures
 * --------------------------------------------------------------------------*/

/* To improve the quality of the LRU approximation we take a set of keys
 * that are good candidate for eviction across performEvictions() calls.
 *
 * Entries inside the eviction pool are taken ordered by idle time, putting
 * greater idle times to the right (ascending order).
 *
 * When an LFU policy is used instead, a reverse frequency indication is used
 * instead of the idle time, so that we still evict by larger value (larger
 * inverse frequency means to evict keys with the least frequent accesses).
 *
 * Empty entries have the key pointer set to NULL. */
#define EVPOOL_SIZE 16
#define EVPOOL_CACHED_SDS_SIZE 255
struct evictionPoolEntry
{
    unsigned long long idle; /* 对象的空闲时间 (inverse frequency for LFU) */
    sds key;                 /* key的名字 待淘汰的键值对的key*/
    sds cached;              /* Cached SDS object for key name. 缓存的SDS对象*/
    int dbid;                /* Key DB number. 待淘汰键值对的key所在的数据库ID*/
};

static struct evictionPoolEntry *EvictionPoolLRU;

int getMaxmemoryState(size_t *total, size_t *logical, size_t *tofree, float *level);
size_t freeMemoryGetNotCountedMemory(void);
int overMaxmemoryAfterAlloc(size_t moremem);

#endif // EVICT_H
