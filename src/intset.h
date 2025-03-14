/*
 * Copyright (c) 2009-2012, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#ifndef INTSET_H
#define INTSET_H
#include <stdint.h>
#include <stdio.h>

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
int64_t intsetRandom(intset *is);
// 根据给定的位置获取值，当位置超过范围返回0，在范围内返回1
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);
// 返回inset的长度
uint32_t intsetLen(const intset *is);
// 返回inset的字节长度
size_t intsetBlobLen(intset *is);
int intsetValidateIntegrity(const unsigned char *is, size_t size, int deep);

#ifdef REDIS_TEST
int intsetTest(int argc, char *argv[]);
#endif

#endif // INTSET_H
