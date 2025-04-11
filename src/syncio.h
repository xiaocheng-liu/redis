#ifndef SYNCIO_H
#define SYNCIO_H

#include <stdio.h>

/* Synchronous I/O with timeout */
// 带超时的同步 IO
ssize_t syncWrite(int fd, char *ptr, ssize_t size, long long timeout);
ssize_t syncRead(int fd, char *ptr, ssize_t size, long long timeout);
ssize_t syncReadLine(int fd, char *ptr, ssize_t size, long long timeout);

#endif
