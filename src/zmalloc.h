/* zmalloc - total amount of allocated memory aware version of malloc()
 *
 * Copyright (c) 2009-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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
// 一般C文件都是在声明一个文件标识, 用于避免文件重复引用
#ifndef __ZMALLOC_H
#define __ZMALLOC_H

/* Double expansion needed for stringification of macro values. */
#define __xstr(s) __str(s)
//将s变成字符串
#define __str(s) #s

// 分别判断使用tcmalloc库/jemalloc库/苹果库哪个作为底层的malloc函数调用
#if defined(USE_TCMALLOC)
// 拼接 ZMALLOC_LIB 字符串
#define ZMALLOC_LIB ("tcmalloc-" __xstr(TC_VERSION_MAJOR) "." __xstr(TC_VERSION_MINOR))
#include <google/tcmalloc.h>
// 限定使用的版本号
#if (TC_VERSION_MAJOR == 1 && TC_VERSION_MINOR >= 6) || (TC_VERSION_MAJOR > 1)
#define HAVE_MALLOC_SIZE 1
// 定义获取指针对应的内存大小
#define zmalloc_size(p) tc_malloc_size(p)
#else
#error "Newer version of tcmalloc required"
#endif

#elif defined(USE_JEMALLOC)
//拼接ZMALLOC_LIB字符串
#define ZMALLOC_LIB ("jemalloc-" __xstr(JEMALLOC_VERSION_MAJOR) "." __xstr(JEMALLOC_VERSION_MINOR) "." __xstr(JEMALLOC_VERSION_BUGFIX))
#include <jemalloc/jemalloc.h>
#if (JEMALLOC_VERSION_MAJOR == 2 && JEMALLOC_VERSION_MINOR >= 1) || (JEMALLOC_VERSION_MAJOR > 2)
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) je_malloc_usable_size(p)
#else
#error "Newer version of jemalloc required"
#endif

// mac的库
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) malloc_size(p)
#endif

// 判断是否声明了分配内存的库
#ifndef ZMALLOC_LIB
// 定义ZMALLOC_LIB为"libc"
#define ZMALLOC_LIB "libc"
#ifdef __GLIBC__
#include <malloc.h>
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) malloc_usable_size(p)
#endif
#endif

/* We can enable the Redis defrag capabilities only if we are using Jemalloc
 * and the version used is our special version modified for Redis having
 * the ability to return per-allocation fragmentation hints. */
#if defined(USE_JEMALLOC) && defined(JEMALLOC_FRAG_HINT)
// 定义是否支持内存碎片整理
#define HAVE_DEFRAG
#endif

void *zmalloc(size_t size);                 // 调用zmalloc函数，申请size大小的内存空间，进行初始化, 有可能有脏数据
void *zcalloc(size_t size);                 // 调用zcalloc函数，申请size大小的内存空间，并初始化为0
void *zrealloc(void *ptr, size_t size);     // 原内存重新调整为size空间的大小
void *ztrymalloc(size_t size);              // 尝试用malloc申请size大小的内存
void *ztrycalloc(size_t size);              // 床上用calloc申请size大小的内存
void *ztryrealloc(void *ptr, size_t size);
void zfree(void *ptr);                      // 释放内存
void *zmalloc_usable(size_t size, size_t *usable);
void *zcalloc_usable(size_t size, size_t *usable);
void *zrealloc_usable(void *ptr, size_t size, size_t *usable);
void *ztrymalloc_usable(size_t size, size_t *usable);
void *ztrycalloc_usable(size_t size, size_t *usable);
void *ztryrealloc_usable(void *ptr, size_t size, size_t *usable);
void zfree_usable(void *ptr, size_t *usable);
char *zstrdup(const char *s);               // 字符串复制函数
size_t zmalloc_used_memory(void);           // 获取当前以及占用内存大小
void zmalloc_set_oom_handler(void (*oom_handler)(size_t)); // 可自定义设置内存溢出的处理方法
size_t zmalloc_get_rss(void);               // 获取RSS信息(Resident Set Size) 常驻内存集
int zmalloc_get_allocator_info(size_t *allocated, size_t *active, size_t *resident);
void set_jemalloc_bg_thread(int enable);
int jemalloc_purge();
size_t zmalloc_get_private_dirty(long pid); // 获得实际内存大小
size_t zmalloc_get_smap_bytes_by_field(char *field, long pid); // 获取/proc/self/smaps字段的字节数
size_t zmalloc_get_memory_size(void);       // 获取物理内存大小
void zlibc_free(void *ptr);                 // 原始系统free释放方法

// 如果开启了内存碎片整理
#ifdef HAVE_DEFRAG
void zfree_no_tcache(void *ptr);
void *zmalloc_no_tcache(size_t size);
#endif

// 没有获取已分配内存大小的方法, 则声明两个函数, 给 zmalloc.c 进行手动实现, 这里有点像java的抽象方法
#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr);
size_t zmalloc_usable_size(void *ptr);
#else
// 将 zmalloc_size 方法重定义为 zmalloc_usable_size, 用于获取指针对象大小
#define zmalloc_usable_size(p) zmalloc_size(p)
#endif

#ifdef REDIS_TEST
int zmalloc_test(int argc, char **argv);
#endif

#endif /* __ZMALLOC_H */
