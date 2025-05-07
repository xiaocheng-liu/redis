/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 */
// 哈希表实现。
// 字典是Redis中的一个非常重要的底层数据结构，其应用相当广泛。
// Redis的数据库就是使用字典作为底层实现的，对数据库的增、删、查、改都是建立在对字典的操作上。
// 此外，字典还是Redis中哈希键的底层实现，当一个哈希键包含的键值对比较多，或者键值对中的元素都是比较长的字符串时，Redis就会使用字典作为哈希键的底层实现。
#ifndef DICT_H
#define DICT_H

#include "mt19937-64.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

// 宏定义常量。表示字典处理成功。
#define DICT_OK 0
// 宏定义常量。表示字典处理失败。
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
// 未使用的参数会生成烦人的警告...
#define DICT_NOTUSED(V) ((void)V)

// hash表中的实体，保存KV信息
typedef struct dictEntry
{
    void *key; // 键

    // 值
    union
    { // dictEntry在不同用途时存储不同的数据
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    struct dictEntry *next; // hash冲突时开链，单链表的next指针
} dictEntry;

// 字典类型函数
typedef struct dictType
{
    uint64_t (*hashFunction)(const void *key);                             // 对key生成hash值
    void *(*keyDup)(void *privdata, const void *key);                      // 对key进行拷贝
    void *(*valDup)(void *privdata, const void *obj);                      // 对val进行拷贝
    int (*keyCompare)(void *privdata, const void *key1, const void *key2); // 两个key的对比函数
    void (*keyDestructor)(void *privdata, void *key);                      // key的销毁
    void (*valDestructor)(void *privdata, void *obj);                      // val的销毁
    int (*expandAllowed)(size_t moreMem, double usedRatio);                // 判断指定字典是否允许哈希表扩展。
} dictType;

// 保存每个hashtable的数据信息，当前大小 hash掩码 使用量
typedef struct dictht
{
    dictEntry **table;      // hashtable中的连续空间
    unsigned long size;     // table的大小
    unsigned long sizemask; // hashtable的掩码,总是等于 size - 1，用于计算索引值
    unsigned long used;     // 哈希表实际存储的 dictEntry 数量
} dictht;

// 字典, 每个字典有两个hash表，用于实现渐进式rehash
typedef struct dict
{
    dictType *type;                                                      // dictType结构的指针，封装了很多数据操作的函数指针，使得dict能处理任意数据类型（类似面向对象语言的interface，可以重载其方法）
    void *privdata;                                                      // 一个私有数据指针(privdata),由调用者在创建dict的时候传进来。
    dictht ht[2];                                                        // 两个hashtable，ht[0]为主，ht[1]在渐进式hash的过程中才会用到。
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */   // 增量hash过程过程中记录rehash执行到第几个bucket了，当rehashidx == -1表示没有在做rehash
    unsigned long iterators; /* number of iterators currently running */ // 正在运行的迭代器数量
} dict;

// 字典的迭代器。如果safe为1，说明他是一个安全的迭代器，可以调用dictAdd、dictFind或者其他dict函数。
// 否则，说明当前迭代器是非安全的，只能调用dictNext()方法
typedef struct dictIterator
{
    dict *d;                                               // 正在迭代的字典
    long index;                                            // 正在迭代哈希表的索引
    int table, safe;                                       // table: 正在迭代哈希表的的号码（0或者1）；safe: 是否安全
    dictEntry *entry, *nextEntry;                          // entry: 当前哈希表节点；nextEntry: 当前哈希表节点的后继节点
    /* unsafe iterator fingerprint for misuse detection.*/ // 不安全迭代器的指纹，用于误用检测
    long long fingerprint;
} dictIterator;

typedef void(dictScanFunction)(void *privdata, const dictEntry *de);
typedef void(dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
// 每个hashtable的初始化大小
// 宏定义常量。哈希表初始化大小。
#define DICT_HT_INITIAL_SIZE 4

/* ------------------------------- Macros ------------------------------------*/
// 释放给定字典节点的值
#define dictFreeVal(d, entry)     \
    if ((d)->type->valDestructor) \
    (d)->type->valDestructor((d)->privdata, (entry)->v.val)

// 设置给定字典节点的值
#define dictSetVal(d, entry, _val_)                                   \
    do                                                                \
    {                                                                 \
        if ((d)->type->valDup)                                        \
            (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
        else                                                          \
            (entry)->v.val = (_val_);                                 \
    } while (0)

// 将一个有符号整数设为节点的值
#define dictSetSignedIntegerVal(entry, _val_) \
    do                                        \
    {                                         \
        (entry)->v.s64 = _val_;               \
    } while (0)

// 将一个无符号整数设为节点的值
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do                                          \
    {                                           \
        (entry)->v.u64 = _val_;                 \
    } while (0)

#define dictSetDoubleVal(entry, _val_) \
    do                                 \
    {                                  \
        (entry)->v.d = _val_;          \
    } while (0)

// 释放给定字典节点的键
#define dictFreeKey(d, entry)     \
    if ((d)->type->keyDestructor) \
    (d)->type->keyDestructor((d)->privdata, (entry)->key)

// 设置给定字典节点的键
#define dictSetKey(d, entry, _key_)                                 \
    do                                                              \
    {                                                               \
        if ((d)->type->keyDup)                                      \
            (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
        else                                                        \
            (entry)->key = (_key_);                                 \
    } while (0)

// 比对两个键
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? (d)->type->keyCompare((d)->privdata, key1, key2) : (key1) == (key2))

// 计算给定键的哈希值
#define dictHashKey(d, key) (d)->type->hashFunction(key)
// 返回获取给定节点的键
#define dictGetKey(he) ((he)->key)
// 返回获取给定节点的值
#define dictGetVal(he) ((he)->v.val)
// 返回获取给定节点的有符号整数值
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
// 返回给定节点的无符号整数值
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
// 返回给定节点的double值
#define dictGetDoubleVal(he) ((he)->v.d)
// 返回给定字典的大小
#define dictSlots(d) ((d)->ht[0].size + (d)->ht[1].size)
// 返回字典的已有节点数量
#define dictSize(d) ((d)->ht[0].used + (d)->ht[1].used)
// 查看字典是否正在 rehash，rehashidx ！= -1 表示没有在rehash
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* If our unsigned long type can store a 64 bit number, use a 64 bit PRNG. */
#if ULONG_MAX >= 0xffffffffffffffff
#define randomULong() ((unsigned long)genrand64_int64())
#else
#define randomULong() random()
#endif

/* dict所有的API */
dict *dictCreate(dictType *type, void *privDataPtr); // 创建dict
int dictExpand(dict *d, unsigned long size);         // 扩缩容
int dictTryExpand(dict *d, unsigned long size);
int dictAdd(dict *d, void *key, void *val);                      // 添加k-v
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing); // 添加的key对应的dictEntry
dictEntry *dictAddOrFind(dict *d, void *key);                    // 添加或者查找
int dictReplace(dict *d, void *key, void *val);                  // 替换key对应的value，如果没有就添加新的k-v
int dictDelete(dict *d, const void *key);                        // 删除某个key对应的数据
dictEntry *dictUnlink(dict *ht, const void *key);                // 卸载某个key对应的entry
void dictFreeUnlinkedEntry(dict *d, dictEntry *he);              // 卸载并清除key对应的entry
void dictRelease(dict *d);                                       // 释放整个dict
dictEntry *dictFind(dict *d, const void *key);                   // 数据查找
void *dictFetchValue(dict *d, const void *key);                  // 获取key对应的value
int dictResize(dict *d);                                         // 重设dict的大小，主要是缩容用的

/************    迭代器相关     *********** */
dictIterator *dictGetIterator(dict *d);       // 创建一个不安全的迭代器
dictIterator *dictGetSafeIterator(dict *d);   // 创建一个安全的迭代器
dictEntry *dictNext(dictIterator *iter);      // 返回迭代器指向的当前节点，如果迭代完毕返回NULL
void dictReleaseIterator(dictIterator *iter); // 释放迭代器

dictEntry *dictGetRandomKey(dict *d);                                       // 随机返回一个entry
dictEntry *dictGetFairRandomKey(dict *d);                                   // 随机返回一个entry，但返回每个entry的概率会更均匀
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count); // 获取dict中的部分数据
void dictGetStats(char *buf, size_t bufsize, dict *d);
uint64_t dictGenHashFunction(const void *key, int len);
uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void *)); // 重置字典。
void dictEnableResize(void);                     // 启用字典大小调整参数，dict_can_resize = 1。
void dictDisableResize(void);                    // 禁用字典大小调整参数，dict_can_resize = 0。
int dictRehash(dict *d, int n);                  // 执行渐进式hash
int dictRehashMilliseconds(dict *d, int ms);     // 在空闲之余分部分cpu时间执行渐进式hash
void dictSetHashFunctionSeed(uint8_t *seed);     // 设置哈希种子。
uint8_t *dictGetHashFunctionSeed(void);          // 获取哈希种子。
// 迭代字典的元素。
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
uint64_t dictGetHash(dict *d, const void *key);                                       // 获取键的哈希值
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash); // 根据指针和哈希值查找节点的引用

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* DICT_H */
