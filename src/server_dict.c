/* 服务端各类字典的哈希、比较、释放策略,以及渐进式 rehash 维护。 */
#include "server.h"
#include "evict.h"
#include "util.h"

/* 字典回调、类型定义及容量维护。 */
/* This is a hash table type that uses the SDS dynamic strings library as
 * keys and redis objects as values (objects can hold SDS strings,
 * lists, sets). */

void dictVanillaFree(void *privdata, void *val) {
    DICT_NOTUSED(privdata);
    zfree(val);
}

void dictListDestructor(void *privdata, void *val) {
    DICT_NOTUSED(privdata);
    listRelease((list *) val);
}

int dictSdsKeyCompare(void *privdata, const void *key1, const void *key2) {
    int l1, l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds) key1);
    l2 = sdslen((sds) key2);
    if (l1 != l2)
        return 0;
    return memcmp(key1, key2, l1) == 0;
}

/* A case insensitive version used for the command lookup table and other
 * places where case insensitive non binary-safe comparison is needed. */
int dictSdsKeyCaseCompare(void *privdata, const void *key1, const void *key2) {
    DICT_NOTUSED(privdata);

    return strcasecmp(key1, key2) == 0;
}

void dictObjectDestructor(void *privdata, void *val) {
    DICT_NOTUSED(privdata);

    if (val == NULL)
        return; /* Lazy freeing will set value to NULL. */
    decrRefCount(val);
}

void dictSdsDestructor(void *privdata, void *val) {
    DICT_NOTUSED(privdata);

    sdsfree(val);
}

int dictObjKeyCompare(void *privdata, const void *key1, const void *key2) {
    const robj *o1 = key1, *o2 = key2;
    return dictSdsKeyCompare(privdata, o1->ptr, o2->ptr);
}

uint64_t dictObjHash(const void *key) {
    const robj *o = key;
    return dictGenHashFunction(o->ptr, sdslen((sds) o->ptr));
}

uint64_t dictSdsHash(const void *key) { return dictGenHashFunction((unsigned char *) key, sdslen((char *) key)); }

uint64_t dictSdsCaseHash(const void *key) {
    return dictGenCaseHashFunction((unsigned char *) key, sdslen((char *) key));
}

int dictEncObjKeyCompare(void *privdata, const void *key1, const void *key2) {
    robj *o1 = (robj *) key1, *o2 = (robj *) key2;
    int cmp;

    if (o1->encoding == OBJ_ENCODING_INT && o2->encoding == OBJ_ENCODING_INT)
        return o1->ptr == o2->ptr;

    /* Due to OBJ_STATIC_REFCOUNT, we avoid calling getDecodedObject() without
     * good reasons, because it would incrRefCount() the object, which
     * is invalid. So we check to make sure dictFind() works with static
     * objects as well. */
    if (o1->refcount != OBJ_STATIC_REFCOUNT)
        o1 = getDecodedObject(o1);
    if (o2->refcount != OBJ_STATIC_REFCOUNT)
        o2 = getDecodedObject(o2);
    cmp = dictSdsKeyCompare(privdata, o1->ptr, o2->ptr);
    if (o1->refcount != OBJ_STATIC_REFCOUNT)
        decrRefCount(o1);
    if (o2->refcount != OBJ_STATIC_REFCOUNT)
        decrRefCount(o2);
    return cmp;
}

uint64_t dictEncObjHash(const void *key) {
    robj *o = (robj *) key;

    if (sdsEncodedObject(o)) {
        return dictGenHashFunction(o->ptr, sdslen((sds) o->ptr));
    } else {
        if (o->encoding == OBJ_ENCODING_INT) {
            char buf[32];
            int len;

            len = ll2string(buf, 32, (long) o->ptr);
            return dictGenHashFunction((unsigned char *) buf, len);
        } else {
            uint64_t hash;

            o = getDecodedObject(o);
            hash = dictGenHashFunction(o->ptr, sdslen((sds) o->ptr));
            decrRefCount(o);
            return hash;
        }
    }
}

/* Return 1 if currently we allow dict to expand. Dict may allocate huge
 * memory to contain hash buckets when dict expands, that may lead redis
 * rejects user's requests or evicts some keys, we can stop dict to expand
 * provisionally if used memory will be over maxmemory after dict expands,
 * but to guarantee the performance of redis, we still allow dict to expand
 * if dict load factor exceeds HASHTABLE_MAX_LOAD_FACTOR. */
// moreMem :扩展哈希表需要额外分配的内存大小(字节)。
// usedRatio :当前哈希表的负载因子(已用节点数 / 哈希表大小)。
// 条件判断:
// 如果 usedRatio <= HASHTABLE_MAX_LOAD_FACTOR (默认为 1.0),表示负载因子较低,需要检查内存是否足够:
// 调用 overMaxmemoryAfterAlloc(moreMem) 判断分配 moreMem 后是否会超过最大内存限制。
// 如果不超过最大内存,返回 1(允许扩展);否则返回 0(不允许扩展)。
// 如果 usedRatio > HASHTABLE_MAX_LOAD_FACTOR ,表示负载因子过高,直接返回 1(强制扩展以避免性能下降)。
int dictExpandAllowed(size_t moreMem, double usedRatio) {
    if (usedRatio <= HASHTABLE_MAX_LOAD_FACTOR) {
        return !overMaxmemoryAfterAlloc(moreMem);
    } else {
        return 1;
    }
}

/* Generic hash table type where keys are Redis Objects, Values
 * dummy pointers. */
dictType objectKeyPointerValueDictType = {
        dictEncObjHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictEncObjKeyCompare, /* key compare */
        dictObjectDestructor, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Like objectKeyPointerValueDictType(), but values can be destroyed, if
 * not NULL, calling zfree(). */
dictType objectKeyHeapPointerValueDictType = {
        dictEncObjHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictEncObjKeyCompare, /* key compare */
        dictObjectDestructor, /* key destructor */
        dictVanillaFree, /* val destructor */
        NULL /* allow to expand */
};

/* Set dictionary type. Keys are SDS strings, values are not used. */
dictType setDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        NULL /* val destructor */
};

/* Sorted sets hash (note: a skiplist is used in addition to the hash table) */
dictType zsetDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        NULL, /* Note: SDS string shared & freed by skiplist */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Db->dict, keys are sds strings, vals are Redis objects. */
dictType dbDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        dictObjectDestructor, /* val destructor */
        dictExpandAllowed /* allow to expand */
};

/* server.lua_scripts sha (as sds string) -> scripts (as robj) cache. */
dictType shaScriptObjectDictType = {
        dictSdsCaseHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCaseCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        dictObjectDestructor, /* val destructor */
        NULL /* allow to expand */
};

/* Db->expires */
dictType dbExpiresDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        NULL, /* key destructor */
        NULL, /* val destructor */
        dictExpandAllowed /* allow to expand */
};

/* Command table. sds string -> command struct pointer. */
dictType commandTableDictType = {
        dictSdsCaseHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCaseCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Hash type hash table (note that small hashes are represented with ziplists) */
dictType hashDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        dictSdsDestructor, /* val destructor */
        NULL /* allow to expand */
};

/* Dict type without destructor */
dictType sdsReplyDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        NULL, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Keylist hash table type has unencoded redis objects as keys and
 * lists as values. It's used for blocking operations (BLPOP) and to
 * map swapped keys to a list of clients waiting for this keys to be loaded. */
dictType keylistDictType = {
        dictObjHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictObjKeyCompare, /* key compare */
        dictObjectDestructor, /* key destructor */
        dictListDestructor, /* val destructor */
        NULL /* allow to expand */
};

/* Cluster nodes hash table, mapping nodes addresses 1.2.3.4:6379 to
 * clusterNode structures. */
dictType clusterNodesDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Cluster re-addition blacklist. This maps node IDs to the time
 * we can re-add this node. The goal is to avoid readding a removed
 * node for some time. */
dictType clusterNodesBlackListDictType = {
        dictSdsCaseHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCaseCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Modules system dictionary type. Keys are module name,
 * values are pointer to RedisModule struct. */
dictType modulesDictType = {
        dictSdsCaseHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCaseCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Migrate cache dict type. */
dictType migrateCacheDictType = {
        dictSdsHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

/* Replication cached script dict (server.repl_scriptcache_dict).
 * Keys are sds SHA1 strings, while values are not used at all in the current
 * implementation. */
dictType replScriptCacheDictType = {
        dictSdsCaseHash, /* hash function */
        NULL, /* key dup */
        NULL, /* val dup */
        dictSdsKeyCaseCompare, /* key compare */
        dictSdsDestructor, /* key destructor */
        NULL, /* val destructor */
        NULL /* allow to expand */
};

// 判断哈希表是否过于稀疏,需要收缩容量。
int htNeedsResize(dict *dict) {
    long long size, used;

    size = dictSlots(dict);
    used = dictSize(dict);
    return (size > DICT_HT_INITIAL_SIZE && (used * 100 / size < HASHTABLE_MIN_FILL));
}

/* 如果db的中俩hashtable的使用率低于10%(可配),缩小hashtable至实际使用大小以节省内存 */
void tryResizeHashTables(int dbid) {
    if (htNeedsResize(server.db[dbid].dict))
        dictResize(server.db[dbid].dict);
    if (htNeedsResize(server.db[dbid].expires))
        dictResize(server.db[dbid].expires);
}

/* Our hash table implementation performs rehashing incrementally while
 * we write/read from the hash table. Still if the server is idle, the hash
 * table will use two tables for a long time. So we try to use 1 millisecond
 * of CPU time at every call of this function to perform some rehashing.
 *
 * The function returns 1 if some rehashing was performed, otherwise 0
 * is returned. */
/* redis采用增量rehash的方式,每个哈希表(ht)都有ht[0]的ht[1], rehash的过程就是
 * ht[0] ht[1]交替的过程,但时间上redis并不是一次性将所有数据从一个ht切换到另一个ht,
 * 而是采用增量rehash的方式,就是在读写时,或者是服务空闲时将数据迁移
 * dictRehashMilliseconds()就是空闲时增量rehash,但每次只会rehash 1ms的时间。
 */
int incrementallyRehash(int dbid) {
    /* Keys dictionary */
    if (dictIsRehashing(server.db[dbid].dict)) {
        dictRehashMilliseconds(server.db[dbid].dict, 1);
        return 1; /* already used our millisecond for this loop... */
    }
    /* Expires */
    if (dictIsRehashing(server.db[dbid].expires)) {
        dictRehashMilliseconds(server.db[dbid].expires, 1);
        return 1; /* already used our millisecond for this loop... */
    }
    return 0;
}

/* This function is called once a background process of some kind terminates,
 * as we want to avoid resizing the hash tables when there is a child in order
 * to play well with copy-on-write (otherwise when a resize happens lots of
 * memory pages are copied). The goal of this function is to update the ability
 * for dict.c to resize the hash tables accordingly to the fact we have an
 * active fork child running. */
void updateDictResizePolicy(void) {
    if (!hasActiveChildProcess())
        dictEnableResize();
    else
        dictDisableResize();
}

