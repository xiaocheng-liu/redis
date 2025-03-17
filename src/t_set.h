#ifndef T_SET_H
#define T_SET_H

#include "server.h"
typedef struct redisObject robj;

/* Set data type */
typedef struct
{
    robj *subject;
    int encoding;
    int ii; /* intset iterator */ // 集成迭代器
    dictIterator *di;
} setTypeIterator;
// Set数据类型
robj *setTypeCreate(sds value);
int setTypeAdd(robj *subject, sds value);
int setTypeRemove(robj *subject, sds value);
int setTypeIsMember(robj *subject, sds value);
setTypeIterator *setTypeInitIterator(robj *subject);
void setTypeReleaseIterator(setTypeIterator *si);
int setTypeNext(setTypeIterator *si, sds *sdsele, int64_t *llele);
sds setTypeNextObject(setTypeIterator *si);
int setTypeRandomElement(robj *setobj, sds *sdsele, int64_t *llele);
unsigned long setTypeRandomElements(robj *set, unsigned long count, robj *aux_set);
unsigned long setTypeSize(const robj *subject);
void setTypeConvert(robj *subject, int enc);
robj *setTypeDup(robj *o);

#endif
