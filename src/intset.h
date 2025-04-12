#ifndef INTSET_H
#define INTSET_H

#include <stdint.h>
#include <stdio.h>

// 用于实现一个整数集合的数据结构。
typedef struct intset
{
    uint32_t encoding; // 保存intset所使用的类型的长度
    uint32_t length;   // 元素的个数
    /**
     * contents 数组的 int8_t 类型声明比较容易让人误解，实际上， intset 并不使用 int8_t 类型来保存任何元素，
     * 结构中的这个类型声明只是作为一个占位符使用：在对 contents 中的元素进行读取或者写入时，
     * 程序并不是直接使用 contents 来对元素进行索引，
     * 而是根据 encoding 的值，对 contents 进行类型转换和指针运算，计算出元素在内存中的正确位置。
     * 在添加新元素，进行内存分配时，分配的空间也是由 encoding 的值决定。
     */
    int8_t contents[]; // 保存元素的数组
} intset;

// 创建一个intset
intset *intsetNew(void);
// 添加一个值到intset中
intset *intsetAdd(intset *is, int64_t value, uint8_t *success);
// 移除给定的值
intset *intsetRemove(intset *is, int64_t value, int *success);
// 查找值是否存在
uint8_t intsetFind(intset *is, int64_t value);
// 从intset中随机获取一个值
int64_t intsetRandom(intset *is);
// 根据给定的位置获取值，当位置超过范围返回0，在范围内返回1
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);
// 返回inset的长度
uint32_t intsetLen(const intset *is);
// 返回inset的字节长度
size_t intsetBlobLen(intset *is);
// 验证整数集合（intset）的完整性，确保数据结构的正确性。
int intsetValidateIntegrity(const unsigned char *is, size_t size, int deep);

#ifdef REDIS_TEST
int intsetTest(int argc, char *argv[]);
#endif

#endif // INTSET_H
