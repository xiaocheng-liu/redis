#include <stdio.h>
typedef struct dictEntry {
    void *key;//8字节
    void *val;//8字节
    struct dictEntry *next; //8字节
} dictEntry;

typedef struct redisObject {
    unsigned type:4; //4位
    unsigned encoding:4; //4位
    unsigned lru:24;  // 24位
    int refcount; //4字节
    void *ptr; // 8字节
} robj;//一个robj 16直接

int main()
{
    dictEntry *entry;
    robj *robj;
    //获取entry的指针大小
    int length = sizeof(entry);
    printf("entry point size :%d \n",length);
    //获取entry结构体大小
    length = sizeof(*entry);
    printf("entry size :%d \n",length);
    length = sizeof(*robj);
    printf("robj size :%d \n",length);
    return 0;
}