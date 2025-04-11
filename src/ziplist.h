#ifndef ZIPLIST_H
#define ZIPLIST_H

#include <stdio.h>

#define ZIPLIST_HEAD 0
#define ZIPLIST_TAIL 1

/* Each entry in the ziplist is either a string or an integer. */
// 用于表示 ziplist 中的单个条目。
// ziplist 是一种紧凑的数据结构，用于存储不同类型的元素。
// 该结构体能同时支持字符串和整数两种数据类型。
typedef struct
{
    /* When string is used, it is provided with the length (slen). */ // 使用字符串时，将为其提供长度（slen）
    unsigned char *sval;
    unsigned int slen;
    /* When integer is used, 'sval' is NULL, and lval holds the value. */ // 使用整数时，'sval' 为 NULL，lval 保存值
    long long lval;
} ziplistEntry;

unsigned char *ziplistNew(void);                                                                                            // 新建ziplist
unsigned char *ziplistMerge(unsigned char **first, unsigned char **second);                                                 // 合并两个ziplist
unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where);                              // 在ziplist头部或者尾部push一个节点
unsigned char *ziplistIndex(unsigned char *zl, int index);                                                                  // 找到某个下标的节点
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p);                                                            // 找到p节点的下一个节点
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p);                                                            // 找到p节点的前一个节点
unsigned int ziplistGet(unsigned char *p, unsigned char **sval, unsigned int *slen, long long *lval);                       // 获取entry中存储的具体内容
unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen);                     // 插入
unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p);                                                         // 删除
unsigned char *ziplistDeleteRange(unsigned char *zl, int index, unsigned int num);                                          // 删除某个下标区间内的节点
unsigned int ziplistCompare(unsigned char *p, unsigned char *s, unsigned int slen);                                         // 比较两个节点的大小
unsigned char *ziplistFind(unsigned char *zl, unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip); // 找到某个特定值的节点
unsigned int ziplistLen(unsigned char *zl);                                                                                 // ziplist的长度
size_t ziplistBlobLen(unsigned char *zl);                                                                                   // ziplist的存储空间大小
void ziplistRepr(unsigned char *zl);                                                                                        //

typedef int (*ziplistValidateEntryCB)(unsigned char *p, void *userdata);
int ziplistValidateIntegrity(unsigned char *zl, size_t size, int deep,
                             ziplistValidateEntryCB entry_cb, void *cb_userdata);
void ziplistRandomPair(unsigned char *zl, unsigned long total_count, ziplistEntry *key, ziplistEntry *val);
void ziplistRandomPairs(unsigned char *zl, unsigned int count, ziplistEntry *keys, ziplistEntry *vals);
unsigned int ziplistRandomPairsUnique(unsigned char *zl, unsigned int count, ziplistEntry *keys, ziplistEntry *vals);

#ifdef REDIS_TEST
int ziplistTest(int argc, char *argv[]);
#endif

#endif /* ZIPLIST_H */
