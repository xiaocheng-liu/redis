#ifndef T_ZSET_H
#define T_ZSET_H

#include "server.h"
typedef struct redisObject robj;
typedef struct client client;

#define ZSET_MIN 0
#define ZSET_MAX 1

/* ZSETs use a specialized version of Skiplists
 * skiplist节点定义 */
typedef struct zskiplistNode
{
    sds ele;                        // 成员对象
    double score;                   // 分值
    struct zskiplistNode *backward; // 前向指针
    // 层
    struct zskiplistLevel
    {
        struct zskiplistNode *forward; // 每一层的后向指针
        unsigned long span;            // 下一个节点的跨度
    } level[];
} zskiplistNode;

// skiplist定义
// 跳跃表的定义
typedef struct zskiplist
{
    struct zskiplistNode *header, *tail; // 跳表的头节点和尾节点
    unsigned long length;                // 节点数量
    int level;                           // 层数
} zskiplist;

// 有序集
typedef struct zset
{
    dict *dict;     // 字典
    zskiplist *zsl; // 跳跃表
} zset;

/* Struct to hold an inclusive/exclusive range spec by score comparison. */
typedef struct
{
    double min, max;
    int minex, maxex; /* are min or max exclusive? */
} zrangespec;

/* Struct to hold an inclusive/exclusive range spec by lexicographic comparison. */
typedef struct
{
    sds min, max;     /* May be set to shared.(minstring|maxstring) */
    int minex, maxex; /* are min or max exclusive? */
} zlexrangespec;

zskiplist *zslCreate(void);
void zslFree(zskiplist *zsl);
zskiplistNode *zslInsert(zskiplist *zsl, double score, sds ele);
unsigned char *zzlInsert(unsigned char *zl, sds ele, double score);
int zslDelete(zskiplist *zsl, double score, sds ele, zskiplistNode **node);
zskiplistNode *zslFirstInRange(zskiplist *zsl, zrangespec *range);
zskiplistNode *zslLastInRange(zskiplist *zsl, zrangespec *range);
double zzlGetScore(unsigned char *sptr);
void zzlNext(unsigned char *zl, unsigned char **eptr, unsigned char **sptr);
void zzlPrev(unsigned char *zl, unsigned char **eptr, unsigned char **sptr);
unsigned char *zzlFirstInRange(unsigned char *zl, zrangespec *range);
unsigned char *zzlLastInRange(unsigned char *zl, zrangespec *range);
unsigned long zsetLength(const robj *zobj);
void zsetConvert(robj *zobj, int encoding);
void zsetConvertToZiplistIfNeeded(robj *zobj, size_t maxelelen);
int zsetScore(robj *zobj, sds member, double *score);
unsigned long zslGetRank(zskiplist *zsl, double score, sds o);
int zsetAdd(robj *zobj, double score, sds ele, int *flags, double *newscore);
long zsetRank(robj *zobj, sds ele, int reverse);
int zsetDel(robj *zobj, sds ele);
robj *zsetDup(robj *o);
int zsetZiplistValidateIntegrity(unsigned char *zl, size_t size, int deep);
void genericZpopCommand(client *c, robj **keyv, int keyc, int where, int emitkey, robj *countarg);
sds ziplistGetObject(unsigned char *sptr);
int zslValueGteMin(double value, zrangespec *spec);
int zslValueLteMax(double value, zrangespec *spec);
void zslFreeLexRange(zlexrangespec *spec);
int zslParseLexRange(robj *min, robj *max, zlexrangespec *spec);
unsigned char *zzlFirstInLexRange(unsigned char *zl, zlexrangespec *range);
unsigned char *zzlLastInLexRange(unsigned char *zl, zlexrangespec *range);
zskiplistNode *zslFirstInLexRange(zskiplist *zsl, zlexrangespec *range);
zskiplistNode *zslLastInLexRange(zskiplist *zsl, zlexrangespec *range);
int zzlLexValueGteMin(unsigned char *p, zlexrangespec *spec);
int zzlLexValueLteMax(unsigned char *p, zlexrangespec *spec);
int zslLexValueGteMin(sds value, zlexrangespec *spec);
int zslLexValueLteMax(sds value, zlexrangespec *spec);

#endif
