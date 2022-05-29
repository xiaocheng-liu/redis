/* Hash Tables Implementation.
 *
 * This file implements in memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include "fmacros.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>

#include "dict.h"
#include "zmalloc.h"
#ifndef DICT_BENCHMARK_MAIN
#include "redisassert.h"
#else
#include <assert.h>
#endif

/* Using dictEnableResize() / dictDisableResize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio. */
/* 
 * redis所有对dict的操作都在这里了，dict其实本质上是一个hashtable，redis在处理hash冲突的时候
 * 也是采用了开链的方式 
 */ 
// 静态变量。哈希表大小是否可调整。但该值为0，也不是所有调整都被阻止。
static int dict_can_resize = 1;
// 负载因子
// 静态变量。哈希表大小强制调整比例。 used/size，如果该比例大于 dict_force_resize_ratio，就需要强制调整。
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static long _dictKeyIndex(dict *ht, const void *key, uint64_t hash, dictEntry **existing);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */
// 静态变量。哈希函数种子。
static uint8_t dict_hash_function_seed[16];

// 设置哈希种子。使用 memcpy 实现。
void dictSetHashFunctionSeed(uint8_t *seed) {
    memcpy(dict_hash_function_seed,seed,sizeof(dict_hash_function_seed));
}

// 获取哈希种子。直接返回 dict_hash_function_seed 即可。
uint8_t *dictGetHashFunctionSeed(void) {
    return dict_hash_function_seed;
}

/* The default hashing function uses SipHash implementation
 * in siphash.c. */
// 哈希函数。套壳 siphash 实现，具体实现在 siphash.c 文件。
uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
// 也是哈希函数。套壳 siphash_nocase 实现，具体实现在 siphash.c 文件。
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);

uint64_t dictGenHashFunction(const void *key, int len) {
    return siphash(key,len,dict_hash_function_seed);
}

uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len) {
    return siphash_nocase(buf,len,dict_hash_function_seed);
}

/* ----------------------------- API implementation ------------------------- */

/* Reset a hash table already initialized with ht_init().
 * NOTE: This function should only be called by ht_destroy(). */
static void _dictReset(dictht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* 创建一个新的dict */
dict *dictCreate(dictType *type,
        void *privDataPtr)
{
    // 分配内存
    dict *d = zmalloc(sizeof(*d));

    _dictInit(d,type,privDataPtr);
    return d;
}

/* 初始化dict */
int _dictInit(dict *d, dictType *type,
        void *privDataPtr)
{
    // 初始化两个哈希表的各项属性值
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);
    // 设置类型特定函数
    d->type = type;
    // 设置私有数据
    d->privdata = privDataPtr;
    // 设置哈希表 rehash 状态
    d->rehashidx = -1;
    // 设置字典的安全迭代器数量
    d->iterators = 0;
    return DICT_OK;
}

/* 将dict大小重置，主要是缩容，以减少内存空间的占用，缩容也会保持在used/size <=1 */
int dictResize(dict *d)
{
    unsigned long minimal;  // 最小值

    // 如果不支持调整哈希表大小或正在rehash过程，直接返回
    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;
    // 设置最小值为已有的键值对数量
    minimal = d->ht[0].used;   
    // 如果已使用键值对大小对小于hashtable的初始数组大小，设置最小值为tashtable初始数组大小
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;
    return dictExpand(d, minimal);
}

/* Expand or create the hash table,
 * when malloc_failed is non-NULL, it'll avoid panic if malloc fails (in which case it'll be set to 1).
 * Returns DICT_OK if expand was performed, and DICT_ERR if skipped. */
/* dict的创建和扩容 */ 
// 当 malloc_failed 为 non-NULL 时，分配内存失败时不会出现终止程序（在这种情况下，它就是1）。
int _dictExpand(dict *d, unsigned long size, int* malloc_failed)
{
    // 重置内存分配标识
    if (malloc_failed) *malloc_failed = 0;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
    // 不能在字典正在 rehash 时进行
    // size 的值也不能小于0号哈希表的当前已使用节点
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;

    dictht n; /* 新的hashtable */
    // 扩容时新table容量是大于当前size的最小2的幂次方，但有上限 
    unsigned long realsize = _dictNextPower(size);

    // 如果新容量和旧容量一致，没有必要继续执行了，返回err
    if (realsize == d->ht[0].size) return DICT_ERR;

    /* 新建一个容量更大的hashtable */
    // 为新hashtable赋值
    n.size = realsize;
    n.sizemask = realsize-1;
    if (malloc_failed) {
        n.table = ztrycalloc(realsize*sizeof(dictEntry*));
        *malloc_failed = n.table == NULL;
        if (*malloc_failed)
            return DICT_ERR;
    } else
        n.table = zcalloc(realsize*sizeof(dictEntry*));

    n.used = 0;

    // 如果是dict初始化的情况，直接把新建的hashtable赋值给ht[0]就行 
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    // 非初始化的情况，将新表赋值给ht[1], 然后标记rehashidx 0
    d->ht[1] = n;
    d->rehashidx = 0; // rehashidx表示当前rehash到ht[0]的下标位置 
    return DICT_OK;
}

/* return DICT_ERR if expand was not performed */
// 创建或扩展哈希表。直接调用 _dictExpand 实现，malloc_failed=NULL。
int dictExpand(dict *d, unsigned long size) {
    return _dictExpand(d, size, NULL);
}

/* return DICT_ERR if expand failed due to memory allocation failure */
// 尝试创建或扩展哈希表。如果分配失败，返回 DICT_ERR。
int dictTryExpand(dict *d, unsigned long size) {
    int malloc_failed; // 未初始化
    _dictExpand(d, size, &malloc_failed);
    return malloc_failed? DICT_ERR : DICT_OK;
}

/* Performs N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 *
 * Note that a rehashing step consists in moving a bucket (that may have more
 * than one key as we use chaining) from the old to the new hash table, however
 * since part of the hash table may be composed of empty spaces, it is not
 * guaranteed that this function will rehash even a single bucket, since it
 * will visit at max N*10 empty buckets in total, otherwise the amount of
 * work it does would be unbound and the function may block for a long time. 
 * redis渐进式hash，采用分批的方式，逐渐将ht[0]依下标转移到ht[2],避免了hashtable扩容时大量
 * 数据迁移导致的性能问题
 * 参数n是指这次rehash只做n个bucket */

// 执行n个步骤的增量rehash过程（渐进式）。如果仍有键要从旧哈希表移动到新哈希表，则返回1，否则返回0。
int dictRehash(dict *d, int n) {
    /* 最大空bucket数量，如果遇到empty_visits个空bucket，直接结束当前rehash的过程 */
    int empty_visits = n*10; 
    if (!dictIsRehashing(d)) return 0;

    // n不为0，且第一个哈希表已使用插槽量非0
    while(n-- && d->ht[0].used != 0) {
        dictEntry *de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        // rehashidx 不能溢出，保证了第一个哈希表还有节点，因为 ht[0].used!=0
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
        // 空头节点过滤。非密集？往后偏移，直到非空头节点。如果empty_visits为0，表示没有了。
        while(d->ht[0].table[d->rehashidx] == NULL) {
            d->rehashidx++;
            if (--empty_visits == 0) return 1; // 如果遇到了empty_visits个空的bucket，直接结束 
        }
        // 遍历当前bucket中的链表，直接将其移动到新的hashtable中  
        de = d->ht[0].table[d->rehashidx];
        /* 把所有的key从旧的hash桶移到新的hash桶中 */
        while(de) {
            uint64_t h;

            // 获取下一个哈希节点，循环变量。
            nextde = de->next;
            /* 获取到key在新hashtable中的下标 ,获取新的哈希索引*/
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;
            // 使用头插法
            // 将正在迁移的节点的下一个节点指向ht[1]的头节点
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            d->ht[0].used--;
            d->ht[1].used++;

            // 继续ht[0]中的下一个节点
            de = nextde;
        }
        d->ht[0].table[d->rehashidx] = NULL;
        d->rehashidx++;
    }

    /* 检测是否已对全表做完了rehash */
    // 检测是否完全转移
    if (d->ht[0].used == 0) {
        zfree(d->ht[0].table);  // 释放旧ht所占用的内存空间  
        d->ht[0] = d->ht[1];  // ht[0]始终是在用ht，ht[1]始终是新ht，ht0全迁移到ht1后会交换下  
        // // 重置 ht[1]
        _dictReset(&d->ht[1]);
        // 标识 rehash 结束
        d->rehashidx = -1;   
        return 0;  // 如果全表hash完，返回0
    }

    /* 还需要继续做hash返回1 */
    return 1;
}

// 获取毫秒级时间。
long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

/* 在空闲之余分部分cpu时间执行渐进式hash  
 * Rehash in ms+"delta" milliseconds. The value of "delta" is larger 
 * than 0, and is smaller than 1 in most cases. The exact upper bound 
 * depends on the running time of dictRehash(d,100).*/
int dictRehashMilliseconds(dict *d, int ms) {
    if (d->iterators > 0) return 0;

    long long start = timeInMilliseconds();
    int rehashes = 0;

    while(dictRehash(d,100)) {
        rehashes += 100;
        if (timeInMilliseconds()-start > ms) break;
    }
    return rehashes;
}

/* This function performs just a step of rehashing, and only if there are
 * no safe iterators bound to our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise
 * some element can be missed or duplicated.
 *
 * This function is called by common lookup or update operations in the
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used. 
 * 调用一次此函数，可以执行一步渐进式hash */
// 单节点 rehash。如果 pauserehash==0。
static void _dictRehashStep(dict *d) {
    if (d->iterators == 0) dictRehash(d,1);
}

/* 向dict中添加元素 */
int dictAdd(dict *d, void *key, void *val)
{
    dictEntry *entry = dictAddRaw(d,key,NULL);  
    // 
    if (!entry) return DICT_ERR;  
    dictSetVal(d, entry, val);
    return DICT_OK;
}

/* 添加和查找的底层实现：  
 * 这个函数只会返回key对应的entry，并不会设置key对应的value，而是把设值权交给调用者。 
 * 
 * 这个函数也作为一个API直接暴露给用户调用，主要是为了在dict中存储非指针类的数据，比如
 * entry = dictAddRaw(dict,mykey,NULL);
 * if (entry != NULL) dictSetSignedIntegerVal(entry,1000);
 *
 * 返回值:
 * 如果key已经存在于dict中了，直接返回null，并把已经存在的entry指针放到&existing里。否则
 * 为key新建一个entry并返回其指针。 
*/
// 更底层的添加或查找哈希节点。
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing)
{
    long index;
    dictEntry *entry;
    dictht *ht;

    // 正在 rehash 过程 单节点操作
    if (dictIsRehashing(d)) _dictRehashStep(d);

    /* 获取到新元素的下标，如果返回-1标识该元素已经存在于dict中了，直接返回null */
    if ((index = _dictKeyIndex(d, key, dictHashKey(d,key), existing)) == -1)
        return NULL;

    /* 否则就给新元素分配内存，并将其插入到链表的头部(一般新插入的数据被访问的频次会更高)*/
    // 定位哈希表。rehash过程就为ht[1]，否则就是ht[0]
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
    // 分配节点空间
    entry = zmalloc(sizeof(*entry));
    // 头节点
    entry->next = ht->table[index];
    // 填入哈希节点
    ht->table[index] = entry;
    ht->used++;

    /* 如果是新建的entry，需要把key填进去 */
    dictSetKey(d, entry, key);
    return entry;
}

/* 添加新的key-value, 或者覆盖掉dict中key对应的值，如果是添加返回1，如果仅仅是覆盖 返回0 */
int dictReplace(dict *d, void *key, void *val)
{
    dictEntry *entry, *existing, auxentry;

    /* dictAddRaw会返回对key新建的entry， */
    entry = dictAddRaw(d,key,&existing);
    if (entry) {
        // 因为dictAddRaw只有在没有entry的时候才会返回，所以这里肯定是新增
        dictSetVal(d, entry, val);
        return 1;
    }

    /*  走到这里了，说明是更新，注意这里需要释放旧的val。
     * 注意：
     * 这里有可能新旧都是同一个val，所以只能先设新值(引用+1)，然后再删旧值(引用-1)，不能反。
     * 因为如果先-1的话，可能对应的val引用为0了就被释放掉了。 */
    auxentry = *existing; 
    dictSetVal(d, existing, val);  // 设置新的val 
    dictFreeVal(d, &auxentry);  // 释放旧的val 
    return 0;
}

/* 添加或者查找： 
 * dictAddOrFind是dictAddRaw的简易版本  */
// 添加或查找哈希节点。直接调用 dictAddRaw 实现，返回值非空就是添加，否则就是查找 existing。
dictEntry *dictAddOrFind(dict *d, void *key) {
    dictEntry *entry, *existing;
    entry = dictAddRaw(d,key,&existing);
    return entry ? entry : existing;
}

/* 查找并删除一个元素，是dictDelete()和dictUnlink()的辅助函数。*/
static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree) {
    uint64_t h, idx;
    dictEntry *he, *prevHe;
    int table;

    // 空哈希表,直接返回
    if (d->ht[0].used == 0 && d->ht[1].used == 0) return NULL;

    // 如果正在rehash，做一步rehash
    if (dictIsRehashing(d)) _dictRehashStep(d);

    // 根据key生成对应哈希值
    h = dictHashKey(d, key);

    // 这里也是需要考虑到rehash的情况，ht[0]和ht[1]中的数据都要删除掉 
    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];
        prevHe = NULL;
        while(he) {
            if (key==he->key || dictCompareKeys(d, key, he->key)) {
                /* 从列表中unlink掉元素 */
                if (prevHe)
                    prevHe->next = he->next;
                else
                    d->ht[table].table[idx] = he->next;
                // 如果nofree是0，需要释放k和v对应的内存空间 
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                    zfree(he);
                }
                d->ht[table].used--;
                return he;
            }
            prevHe = he;
            he = he->next;
        }
        if (!dictIsRehashing(d)) break;
    }
    return NULL; /* 没找到key对应的数据 */
}

/* 删除方法，成功返回DICK_OK，否则返回DICK_ERR */
// 删除哈希节点。直接调用 dictGenericDelete，其中 nofree=0。
int dictDelete(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,0) ? DICT_OK : DICT_ERR;
}

/* Remove an element from the table, but without actually releasing
 * the key, value and dictionary entry. The dictionary entry is returned
 * if the element was found (and unlinked from the table), and the user
 * should later call `dictFreeUnlinkedEntry()` with it in order to release it.
 * Otherwise if the key is not found, NULL is returned.
 *
 * This function is useful when we want to remove something from the hash
 * table but want to use its value before actually deleting the entry.
 * Without this function the pattern would require two lookups:
 *
 *  entry = dictFind(...);
 *  // Do something with entry
 *  dictDelete(dictionary,entry);
 *
 * Thanks to this function it is possible to avoid this, and use
 * instead:
 *
 * entry = dictUnlink(dictionary,entry);
 * // Do something with entry
 * dictFreeUnlinkedEntry(entry); // <- This does not need to lookup again.
 * 将key对应的dictEntry从dict中移除，但并未释放空间，只会导致查不到而已 
 */
// 接触哈希节点绑定。不需要实际释放对应哈希键值节点。也是直接调用 dictGenericDelete 实现，其中 nofree=1 表示不需要释放对应空间。
dictEntry *dictUnlink(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,1);
}

/* You need to call this function to really free the entry after a call
 * to dictUnlink(). It's safe to call this function with 'he' = NULL. */
// 释放调用 dictUnlink 解绑的哈希节点。
void dictFreeUnlinkedEntry(dict *d, dictEntry *he) {
    if (he == NULL) return;
    dictFreeKey(d, he);
    dictFreeVal(d, he);
    zfree(he);
}

// 销毁整个字典。
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {
    unsigned long i;

    /* Free all the elements */
    // // 释放所有哈希节点
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dictEntry *he, *nextHe;

        // 对字典私有数据的处理，i=0时处理，仅一次
        if (callback && (i & 65535) == 0) callback(d->privdata);

        // 当前头节点为空，直接跳过
        if ((he = ht->table[i]) == NULL) continue;
        while(he) {
            nextHe = he->next;
            // 释放 键 值 节点
            dictFreeKey(d, he);
            dictFreeVal(d, he);
            zfree(he);
            // 更新可使用插槽量，和循环变量he
            ht->used--;
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
    // 释放哈希表
    zfree(ht->table);
    /* Re-initialize the table */
    // 重新初始化哈希表
    _dictReset(ht);
    return DICT_OK; /* never fails */
}

/* Clear & Release the hash table */
// 销毁并释放哈希表
void dictRelease(dict *d)
{
    _dictClear(d,&d->ht[0],NULL);
    _dictClear(d,&d->ht[1],NULL);
    zfree(d);
}

// 查找哈希节点。根据键。
dictEntry *dictFind(dict *d, const void *key)
{
    dictEntry *he;
    uint64_t h, idx, table;

    if (dictSize(d) == 0) return NULL; /* dict为空 */
    // 如果正在rehash，做一步rehash
    if (dictIsRehashing(d)) _dictRehashStep(d);
    h = dictHashKey(d, key);
    // 查找的过程中，可能正在rehash中，所以新老两个hashtable都需要查 
    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];
        while(he) {
            if (key==he->key || dictCompareKeys(d, key, he->key))
                return he;
            he = he->next;
        }
        // 如果ht[0]中没找到，且不再rehas中，就不需要继续找了ht[1]了。 
        if (!dictIsRehashing(d)) return NULL;
    }
    return NULL;
}

// 获取对应哈希节点的值。
void *dictFetchValue(dict *d, const void *key) {
    dictEntry *he;

    he = dictFind(d,key);
    return he ? dictGetVal(he) : NULL;
}

/* A fingerprint is a 64 bit number that represents the state of the dictionary
 * at a given time, it's just a few dict properties xored together.
 * When an unsafe iterator is initialized, we get the dict fingerprint, and check
 * the fingerprint again when the iterator is released.
 * If the two fingerprints are different it means that the user of the iterator
 * performed forbidden operations against the dictionary while iterating. */
// 字典签名。
long long dictFingerprint(dict *d) {
    long long integers[6], hash = 0;
    int j;

    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

// 获取字典迭代器
dictIterator *dictGetIterator(dict *d)
{
    dictIterator *iter = zmalloc(sizeof(*iter));

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

// 获取字典安全迭代器。调用 dictGetIterator 后将 iter->safe 设置为1即可。
dictIterator *dictGetSafeIterator(dict *d) {
    dictIterator *i = dictGetIterator(d);

    i->safe = 1;
    return i;
}

// 迭代循环。
dictEntry *dictNext(dictIterator *iter)
{
    while (1) {
        if (iter->entry == NULL) { // 当前节点为NULL，说明首次迭代过程？不能。链切换，也会如此。
            dictht *ht = &iter->d->ht[iter->table]; // 获取对应哈希表
            if (iter->index == -1 && iter->table == 0) {// 这里可证为首次迭代过程
                if (iter->safe) // 如果为安全迭代器，暂停 rehash 过程
                    iter->d->iterators++;
                else            // 否则，检测签名
                    iter->fingerprint = dictFingerprint(iter->d);
            }
            iter->index++;  // 更新迭代索引
            if (iter->index >= (long) ht->size) {   // 如果溢出，检测，是否当前哈希表涉及到 rehash 判断后，切换表，和重置索引
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;  // 切换哈希表，和重置索引
                    iter->index = 0;
                    ht = &iter->d->ht[1];   // 更新ht为ht[1]
                } else {
                    break;  // 如果不是，结束
                }   
            }
            iter->entry = ht->table[iter->index];   // 更新当前节点
        } else {
            iter->entry = iter->nextEntry;  // 更新当前节点
        }
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            // 保存下一个节点，迭代器调用者可能会删除当前返回节点
            iter->nextEntry = iter->entry->next;
            // 返回当前迭代节点
            return iter->entry;
        }
    }
    // 迭代结束 NULL
    return NULL;
}

// 释放指定迭代器。
void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) { // 已经迭代过
        if (iter->safe) // 安全迭代器，需要主动重启 rehash 过程
            iter->d->iterators--;
        else
            // 验证迭代器签名
            assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    // 释放迭代器空间
    zfree(iter);
}

/* 从dict中随机返回一个key */
dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned long h;
    int listlen, listele;

    if (dictSize(d) == 0) return NULL;
    if (dictIsRehashing(d)) _dictRehashStep(d);
    if (dictIsRehashing(d)) {
        // 在 rehash 过程
        do {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
            h = d->rehashidx + (randomULong() % (dictSlots(d) - d->rehashidx));
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
        } while(he == NULL);
    } else {
        do {
            h = randomULong() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
    // 找到一个非空桶，但是它是一个链表，需要从这个链表中获取一个随机元素
    // 链表长度
    listlen = 0;
    // 原始节点
    orighe = he;
    while(he) { // next 直到空 计算链表长度
        he = he->next;
        listlen++;
    }
    listele = random() % listlen;   // 随机节点位置
    he = orighe;    // 归还
    while(listele--) he = he->next; // 循环到目标节点为止
    return he;
}

/* This function samples the dictionary to return a few keys from random
 * locations.
 *
 * It does not guarantee to return all the keys specified in 'count', nor
 * it does guarantee to return non-duplicated elements, however it will make
 * some effort to do both things.
 *
 * Returned pointers to hash table entries are stored into 'des' that
 * points to an array of dictEntry pointers. The array must have room for
 * at least 'count' elements, that is the argument we pass to the function
 * to tell how many random elements we need.
 *
 * The function returns the number of items stored into 'des', that may
 * be less than 'count' if the hash table has less than 'count' elements
 * inside, or if not enough elements were found in a reasonable amount of
 * steps.
 *
 * Note that this function is not suitable when you need a good distribution
 * of the returned items, but only when you need to "sample" a given number
 * of continuous elements to run some kind of algorithm or to produce
 * statistics. However the function is much faster than dictGetRandomKey()
 * at producing N elements.
 * 随机返回dict中的部分key，返回值是返回key的数量 */
// 对字典进行采样，随机返回几个键。
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count) {
    unsigned long j; /* internal hash table id, 0 or 1. */
    unsigned long tables; /* 1 or 2 tables? */
    unsigned long stored = 0, maxsizemask;
    unsigned long maxsteps;

    if (dictSize(d) < count) count = dictSize(d);
    maxsteps = count*10;

    /* Try to do a rehashing work proportional to 'count'. */
    for (j = 0; j < count; j++) {
        if (dictIsRehashing(d))
            _dictRehashStep(d);
        else
            break;
    }

    tables = dictIsRehashing(d) ? 2 : 1;
    maxsizemask = d->ht[0].sizemask;
    if (tables > 1 && maxsizemask < d->ht[1].sizemask)
        maxsizemask = d->ht[1].sizemask;

    /* Pick a random point inside the larger table. */
    unsigned long i = randomULong() & maxsizemask;
    unsigned long emptylen = 0; /* Continuous empty entries so far. */
    while(stored < count && maxsteps--) {
        for (j = 0; j < tables; j++) {
            /* Invariant of the dict.c rehashing: up to the indexes already
             * visited in ht[0] during the rehashing, there are no populated
             * buckets, so we can skip ht[0] for indexes between 0 and idx-1. */
            if (tables == 2 && j == 0 && i < (unsigned long) d->rehashidx) {
                /* Moreover, if we are currently out of range in the second
                 * table, there will be no elements in both tables up to
                 * the current rehashing index, so we jump if possible.
                 * (this happens when going from big to small table). */
                if (i >= d->ht[1].size)
                    i = d->rehashidx;
                else
                    continue;
            }
            if (i >= d->ht[j].size) continue; /* Out of range for this table. */
            dictEntry *he = d->ht[j].table[i];

            /* Count contiguous empty buckets, and jump to other
             * locations if they reach 'count' (with a minimum of 5). */
            if (he == NULL) {
                emptylen++;
                if (emptylen >= 5 && emptylen > count) {
                    i = randomULong() & maxsizemask;
                    emptylen = 0;
                }
            } else {
                emptylen = 0;
                while (he) {
                    /* Collect all the elements of the buckets found non
                     * empty while iterating. */
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count) return stored;
                }
            }
        }
        i = (i+1) & maxsizemask;
    }
    return stored;
}

/* This is like dictGetRandomKey() from the POV of the API, but will do more
 * work to ensure a better distribution of the returned element.
 *
 * This function improves the distribution because the dictGetRandomKey()
 * problem is that it selects a random bucket, then it selects a random
 * element from the chain in the bucket. However elements being in different
 * chain lengths will have different probabilities of being reported. With
 * this function instead what we do is to consider a "linear" range of the table
 * that may be constituted of N buckets with chains of different lengths
 * appearing one after the other. Then we report a random element in the range.
 * In this way we smooth away the problem of different chain lengths. */
#define GETFAIR_NUM_ENTRIES 15
// 获取一个更合理的随机节点，相比于 dictGetRandomKey 而言，多了从更多桶抽取的可能性。
dictEntry *dictGetFairRandomKey(dict *d) {
    dictEntry *entries[GETFAIR_NUM_ENTRIES];
    unsigned int count = dictGetSomeKeys(d,entries,GETFAIR_NUM_ENTRIES);
    /* Note that dictGetSomeKeys() may return zero elements in an unlucky
     * run() even if there are actually elements inside the hash table. So
     * when we get zero, we call the true dictGetRandomKey() that will always
     * yield the element if the hash table has at least one. */
    if (count == 0) return dictGetRandomKey(d);
    unsigned int idx = rand() % count;
    return entries[idx];
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
// 反转位，工具函数。
static unsigned long rev(unsigned long v) {
    unsigned long s = CHAR_BIT * sizeof(v); // bit size; must be power of 2
    unsigned long mask = ~0UL;
    while ((s >>= 1) > 0) {
        mask ^= (mask << s);
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/* dictScan() is used to iterate over the elements of a dictionary.
 *
 * Iterating works the following way:
 *
 * 1) Initially you call the function using a cursor (v) value of 0.
 * 2) The function performs one step of the iteration, and returns the
 *    new cursor value you must use in the next call.
 * 3) When the returned cursor is 0, the iteration is complete.
 *
 * The function guarantees all elements present in the
 * dictionary get returned between the start and end of the iteration.
 * However it is possible some elements get returned multiple times.
 *
 * For every element returned, the callback argument 'fn' is
 * called with 'privdata' as first argument and the dictionary entry
 * 'de' as second argument.
 *
 * HOW IT WORKS.
 *
 * The iteration algorithm was designed by Pieter Noordhuis.
 * The main idea is to increment a cursor starting from the higher order
 * bits. That is, instead of incrementing the cursor normally, the bits
 * of the cursor are reversed, then the cursor is incremented, and finally
 * the bits are reversed again.
 *
 * This strategy is needed because the hash table may be resized between
 * iteration calls.
 *
 * dict.c hash tables are always power of two in size, and they
 * use chaining, so the position of an element in a given table is given
 * by computing the bitwise AND between Hash(key) and SIZE-1
 * (where SIZE-1 is always the mask that is equivalent to taking the rest
 *  of the division between the Hash of the key and SIZE).
 *
 * For example if the current hash table size is 16, the mask is
 * (in binary) 1111. The position of a key in the hash table will always be
 * the last four bits of the hash output, and so forth.
 *
 * WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
 *
 * If the hash table grows, elements can go anywhere in one multiple of
 * the old bucket: for example let's say we already iterated with
 * a 4 bit cursor 1100 (the mask is 1111 because hash table size = 16).
 *
 * If the hash table will be resized to 64 elements, then the new mask will
 * be 111111. The new buckets you obtain by substituting in ??1100
 * with either 0 or 1 can be targeted only by keys we already visited
 * when scanning the bucket 1100 in the smaller hash table.
 *
 * By iterating the higher bits first, because of the inverted counter, the
 * cursor does not need to restart if the table size gets bigger. It will
 * continue iterating using cursors without '1100' at the end, and also
 * without any other combination of the final 4 bits already explored.
 *
 * Similarly when the table size shrinks over time, for example going from
 * 16 to 8, if a combination of the lower three bits (the mask for size 8
 * is 111) were already completely explored, it would not be visited again
 * because we are sure we tried, for example, both 0111 and 1111 (all the
 * variations of the higher bit) so we don't need to test it again.
 *
 * WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
 *
 * Yes, this is true, but we always iterate the smaller table first, then
 * we test all the expansions of the current cursor into the larger
 * table. For example if the current cursor is 101 and we also have a
 * larger table of size 16, we also test (0)101 and (1)101 inside the larger
 * table. This reduces the problem back to having only one table, where
 * the larger one, if it exists, is just an expansion of the smaller one.
 *
 * LIMITATIONS
 *
 * This iterator is completely stateless, and this is a huge advantage,
 * including no additional memory used.
 *
 * The disadvantages resulting from this design are:
 *
 * 1) It is possible we return elements more than once. However this is usually
 *    easy to deal with in the application level.
 * 2) The iterator must return multiple elements per call, as it needs to always
 *    return all the keys chained in a given bucket, and all the expansions, so
 *    we are sure we don't miss keys moving during rehashing.
 * 3) The reverse cursor is somewhat hard to understand at first, but this
 *    comment is supposed to help.
 */
// 迭代字典的元素。
unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       dictScanBucketFunction* bucketfn,
                       void *privdata)
{
    dictht *t0, *t1;
    const dictEntry *de, *next;
    unsigned long m0, m1;

    if (dictSize(d) == 0) return 0;

    /* Having a safe iterator means no rehashing can happen, see _dictRehashStep.
     * This is needed in case the scan callback tries to do dictFind or alike. */
    d->iterators++;

    if (!dictIsRehashing(d)) {
        t0 = &(d->ht[0]);
        m0 = t0->sizemask;

        /* Emit entries at cursor */
        if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de) {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Set unmasked bits so incrementing the reversed cursor
         * operates on the masked bits */
        v |= ~m0;

        /* Increment the reverse cursor */
        v = rev(v);
        v++;
        v = rev(v);

    } else {
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
        if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de) {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
        do {
            /* Emit entries at cursor */
            if (bucketfn) bucketfn(privdata, &t1->table[v & m1]);
            de = t1->table[v & m1];
            while (de) {
                next = de->next;
                fn(privdata, de);
                de = next;
            }

            /* Increment the reverse cursor not covered by the smaller mask.*/
            v |= ~m1;
            v = rev(v);
            v++;
            v = rev(v);

            /* Continue while bits covered by mask difference is non-zero */
        } while (v & (m0 ^ m1));
    }

    /* undo the ++ at the top */
    d->iterators--;

    return v;
}

/* ------------------------- dict私有方法 ------------------------------ */

/* Because we may need to allocate huge memory chunk at once when dict
 * expands, we will check this allocation is allowed or not if the dict
 * type has expandAllowed member function. */
// 判断指定字典是否允许哈希表扩展。
static int dictTypeExpandAllowed(dict *d) {
    // 没设置，默认可以
    if (d->type->expandAllowed == NULL) return 1;
    return d->type->expandAllowed(
                    _dictNextPower(d->ht[0].used + 1) * sizeof(dictEntry*),
                    (double)d->ht[0].used / d->ht[0].size);
}

/* 检查是否dict需要扩容 */
static int _dictExpandIfNeeded(dict *d)
{
    /* 已经在渐进式hash的流程中了，直接返回 */
    if (dictIsRehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    // 如果是空hashtable，直接扩容到初始大小DICT_HT_INITIAL_SIZE
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    // 条件一 哈希表已使用大小 >= 哈希表大小，说明已经饱和或溢出
    // 条件二 dict_can_resize 是否支持大小调整 或 已达到强制扩展比例（负载因子）
    // 条件三 该字典类型允许扩展调整
    // 扩展到 used+1 大小。。。这样该函数触发可能比较频繁。只增已有节点+1。
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used/d->ht[0].size > dict_force_resize_ratio) &&
        dictTypeExpandAllowed(d))
    {
        return dictExpand(d, d->ht[0].used + 1);
    }
    return DICT_OK;
}

/* 当前size的下一个2的幂次方 */
// 将初始哈希表大小做幂级扩展，直到大于等于给定 size。
static unsigned long _dictNextPower(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX + 1LU;
    while(1) {
        // 判断当前是否大于等于给定的size
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* 
 * 返回key对应的下标，已经有key对应的dictentry，将其指针存放到**existing里，供调用者使用。
 * 
 * 注意，如果是在rehash过程中，这个函数始终会返回在第二个hashtale(xin)中的下标。 */
static long _dictKeyIndex(dict *d, const void *key, uint64_t hash, dictEntry **existing)
{
    // hash 由 key 生成。那这里为什么不调用哈希函数生成，反而传参？
    unsigned long idx, table;
    // 哈希节点
    dictEntry *he;
    // 已存在哈希节点。强行重置。
    if (existing) *existing = NULL;

    /* 是否需要扩容 */
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;
    
    // 在ht[0]和ht[1]中检索，可能在rehash过程。需要循环
    for (table = 0; table <= 1; table++) {
        // 索引 哈希值和哈希表大小掩码
        idx = hash & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        // 获取相应哈希头节点，开启循环检索 key 毕竟单向链表
        he = d->ht[table].table[idx];
        while(he) {
            if (key==he->key || dictCompareKeys(d, key, he->key)) {
                // 存在 返回 -1
                if (existing) *existing = he;
                return -1;
            }
            // 继续往下一个节点遍历
            he = he->next;
        }
        // 如果不在 rehash 过程。直接 break。
        if (!dictIsRehashing(d)) break;
    }
    return idx;
}

// 重置字典。
void dictEmpty(dict *d, void(callback)(void*)) {
    _dictClear(d,&d->ht[0],callback);
    _dictClear(d,&d->ht[1],callback);
    d->rehashidx = -1;
    d->iterators = 0;
}

// 启用字典大小调整参数，dict_can_resize = 1。
void dictEnableResize(void) {
    dict_can_resize = 1;
}

// 禁用字典大小调整参数，dict_can_resize = 0。
void dictDisableResize(void) {
    dict_can_resize = 0;
}

// 获取键的哈希值，直接调用 dictHashKey 实现。
uint64_t dictGetHash(dict *d, const void *key) {
    return dictHashKey(d, key);
}

/* Finds the dictEntry reference by using pointer and pre-calculated hash.
 * oldkey is a dead pointer and should not be accessed.
 * the hash value should be provided using dictGetHash.
 * no string / key comparison is performed.
 * return value is the reference to the dictEntry if found, or NULL if not found. */
// 根据指针和哈希值查找节点的引用？
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash) {
    dictEntry *he, **heref;
    unsigned long idx, table;

    if (dictSize(d) == 0) return NULL; /* dict is empty */
    for (table = 0; table <= 1; table++) {
        idx = hash & d->ht[table].sizemask;
        heref = &d->ht[table].table[idx];
        he = *heref;
        while(he) {
            if (oldptr==he->key)
                return heref;
            heref = &he->next;
            he = *heref;
        }
        if (!dictIsRehashing(d)) return NULL;
    }
    return NULL;
}

/* ------------------------------- Debugging ---------------------------------*/

#define DICT_STATS_VECTLEN 50
size_t _dictGetStatsHt(char *buf, size_t bufsize, dictht *ht, int tableid) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];
    size_t l = 0;

    if (ht->used == 0) {
        return snprintf(buf,bufsize,
            "No stats available for empty dictionaries\n");
    }

    /* Compute stats. */
    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    for (i = 0; i < ht->size; i++) {
        dictEntry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while(he) {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
        if (chainlen > maxchainlen) maxchainlen = chainlen;
        totchainlen += chainlen;
    }

    /* Generate human readable stats. */
    l += snprintf(buf+l,bufsize-l,
        "Hash table %d stats (%s):\n"
        " table size: %lu\n"
        " number of elements: %lu\n"
        " different slots: %lu\n"
        " max chain length: %lu\n"
        " avg chain length (counted): %.02f\n"
        " avg chain length (computed): %.02f\n"
        " Chain length distribution:\n",
        tableid, (tableid == 0) ? "main hash table" : "rehashing target",
        ht->size, ht->used, slots, maxchainlen,
        (float)totchainlen/slots, (float)ht->used/slots);

    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        if (l >= bufsize) break;
        l += snprintf(buf+l,bufsize-l,
            "   %s%ld: %ld (%.02f%%)\n",
            (i == DICT_STATS_VECTLEN-1)?">= ":"",
            i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }

    /* Unlike snprintf(), return the number of characters actually written. */
    if (bufsize) buf[bufsize-1] = '\0';
    return strlen(buf);
}

void dictGetStats(char *buf, size_t bufsize, dict *d) {
    size_t l;
    char *orig_buf = buf;
    size_t orig_bufsize = bufsize;

    l = _dictGetStatsHt(buf,bufsize,&d->ht[0],0);
    buf += l;
    bufsize -= l;
    if (dictIsRehashing(d) && bufsize > 0) {
        _dictGetStatsHt(buf,bufsize,&d->ht[1],1);
    }
    /* Make sure there is a NULL term at the end. */
    if (orig_bufsize) orig_buf[orig_bufsize-1] = '\0';
}

/* ------------------------------- Benchmark ---------------------------------*/

#ifdef DICT_BENCHMARK_MAIN

#include "sds.h"

uint64_t hashCallback(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

int compareCallback(void *privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void freeCallback(void *privdata, void *val) {
    DICT_NOTUSED(privdata);

    sdsfree(val);
}

dictType BenchmarkDictType = {
    hashCallback,
    NULL,
    NULL,
    compareCallback,
    freeCallback,
    NULL,
    NULL
};

#define start_benchmark() start = timeInMilliseconds()
#define end_benchmark(msg) do { \
    elapsed = timeInMilliseconds()-start; \
    printf(msg ": %ld items in %lld ms\n", count, elapsed); \
} while(0)

/* dict-benchmark [count] */
int main(int argc, char **argv) {
    long j;
    long long start, elapsed;
    dict *dict = dictCreate(&BenchmarkDictType,NULL);
    long count = 0;

    if (argc == 2) {
        count = strtol(argv[1],NULL,10);
    } else {
        count = 5000000;
    }

    start_benchmark();
    for (j = 0; j < count; j++) {
        int retval = dictAdd(dict,sdsfromlonglong(j),(void*)j);
        assert(retval == DICT_OK);
    }
    end_benchmark("Inserting");
    assert((long)dictSize(dict) == count);

    /* Wait for rehashing. */
    while (dictIsRehashing(dict)) {
        dictRehashMilliseconds(dict,100);
    }

    start_benchmark();
    for (j = 0; j < count; j++) {
        sds key = sdsfromlonglong(j);
        dictEntry *de = dictFind(dict,key);
        assert(de != NULL);
        sdsfree(key);
    }
    end_benchmark("Linear access of existing elements");

    start_benchmark();
    for (j = 0; j < count; j++) {
        sds key = sdsfromlonglong(j);
        dictEntry *de = dictFind(dict,key);
        assert(de != NULL);
        sdsfree(key);
    }
    end_benchmark("Linear access of existing elements (2nd round)");

    start_benchmark();
    for (j = 0; j < count; j++) {
        sds key = sdsfromlonglong(rand() % count);
        dictEntry *de = dictFind(dict,key);
        assert(de != NULL);
        sdsfree(key);
    }
    end_benchmark("Random access of existing elements");

    start_benchmark();
    for (j = 0; j < count; j++) {
        dictEntry *de = dictGetRandomKey(dict);
        assert(de != NULL);
    }
    end_benchmark("Accessing random keys");

    start_benchmark();
    for (j = 0; j < count; j++) {
        sds key = sdsfromlonglong(rand() % count);
        key[0] = 'X';
        dictEntry *de = dictFind(dict,key);
        assert(de == NULL);
        sdsfree(key);
    }
    end_benchmark("Accessing missing");

    start_benchmark();
    for (j = 0; j < count; j++) {
        sds key = sdsfromlonglong(j);
        int retval = dictDelete(dict,key);
        assert(retval == DICT_OK);
        key[0] += 17; /* Change first number to letter. */
        retval = dictAdd(dict,key,(void*)j);
        assert(retval == DICT_OK);
    }
    end_benchmark("Removing and adding");
}
#endif

/**
 * 通过对源码的局部解读，可以看到字典的实现基于哈希表，而C语言没有这类型，Redis 自行实现了一套，采用 dict > ht > table > headptr 的结构。
 * 然而哈希冲突是个很重要的问题（当有两个或以上数量的键被分配到了哈希表数组的同一个索引上面的情况），这里采用了公开链地址法解决该问题，
 * 由于每个哈希节点都一个 next 指针，所以当多个节点分配到同一个索引上时，可形成单向链表。这也难免在查找采用循环结构。
 * 不过这也是添加新节点总是加到链表头部的原因，不可能迭代到尾节点追加，太废了。
 *
 * 在哈希节点增加或减少时，也会触发 rehash 过程，对哈希表的大小进行相应的调整，ht[1] 就是使用在该情况的。
 * 在扩展时，ht[1] 的大小总是第一个大于等于 ht[0].used+1的2^n。如果收缩，ht[1].size=ht[0].used*2。
 * 这样可有效避免对哈希表进行频繁的调整，造成不必要的损耗。rehash 过程就是，重新计算键的哈希值和索引值，
 * 将键值对放置到 ht[1] 的相应位置，都从 ht[0] 迁移到 ht[1] 后，释放 ht[0]，将 ht[1] 设置为 ht[0]，ht[1] 会创建新的空白哈希表，为下一次准备。
 * 当然迁移过程并不是一次性完成，而是渐进式完成。主要是键值对过多时，该过程对机器性能影响太大。
 * 
 */