/*
 * The following is the NetBSD libc qsort implementation modified in order to
 * support partial sorting of ranges for Redis.
 *
 * See the pqsort.c file for the original copyright notice.
 */
#ifndef PQSORT_H
#define PQSORT_H

#include <stdio.h>

void pqsort(void *a, size_t n, size_t es,
            int (*cmp)(const void *, const void *), size_t lrange, size_t rrange);

#endif // PQSORT_H
