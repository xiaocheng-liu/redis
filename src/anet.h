/*
 * Basic TCP socket stuff made a bit less boring
 */

#ifndef ANET_H
#define ANET_H

#include <sys/types.h>

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

/* Flags used with certain functions. */
#define ANET_NONE 0
#define ANET_IP_ONLY (1 << 0)

#if defined(__sun) || defined(_AIX)
#define AF_LOCAL AF_UNIX
#endif

#ifdef _AIX
#undef ip_len
#endif

/* FD to address string conversion types */
#define FD_TO_PEER_NAME 0
#define FD_TO_SOCK_NAME 1

// 创建一个TCP连接
int anetTcpConnect(char *err, const char *addr, int port);
// 创建一个非阻塞的TCP连接。
int anetTcpNonBlockConnect(char *err, const char *addr, int port);
int anetTcpNonBlockBindConnect(char *err, const char *addr, int port, const char *source_addr);
int anetTcpNonBlockBestEffortBindConnect(char *err, const char *addr, int port, const char *source_addr);
int anetUnixConnect(char *err, const char *path);
int anetUnixNonBlockConnect(char *err, const char *path);

// 从给定的文件描述符 fd 中读取数据。
int anetRead(int fd, char *buf, int count);
// 向指定的文件描述符中写入数据。
int anetWrite(int fd, char *buf, int count);

int anetResolve(char *err, char *host, char *ipbuf, size_t ipbuf_len, int flags);
// 创建一个TCP服务器
int anetTcpServer(char *err, int port, char *bindaddr, int backlog);
// 创建一个基于 IPv6 的 TCP 服务器
int anetTcp6Server(char *err, int port, char *bindaddr, int backlog);
// 创建并配置一个基于 Unix 域套接字的服务器。
int anetUnixServer(char *err, char *path, mode_t perm, int backlog);
// 用于在 TCP 服务器端接受新的连接请求，并将客户端的 IP 地址和端口返回给调用者。
int anetTcpAccept(char *err, int serversock, char *ip, size_t ip_len, int *port);
// 用于接受基于 Unix 域套接字建立的连接。
int anetUnixAccept(char *err, int serversock);

// 该函数 anetNonBlock 用于将文件描述符设置为非阻塞模式。它接受两个参数：一个错误信息字符串指针 err 和一个文件描述符 fd。如果设置成功返回 0，否则返回 -1 并设置错误信息。
int anetNonBlock(char *err, int fd);
// 该函数用于将文件描述符设置为阻塞模式。参数err用于返回错误信息，fd为文件描述符。函数返回值为0表示成功，非0表示失败。
int anetBlock(char *err, int fd);
// 为给定的文件描述符设置 close-on-exec 标志（FD_CLOEXEC）
int anetCloexec(int fd);
// 该函数用于启用TCP_NODELAY选项，禁用Nagle算法，减少网络延迟。参数err用于返回错误信息，fd为文件描述符。函数返回值为0表示成功，非0表示失败。
int anetEnableTcpNoDelay(char *err, int fd);
// 该函数用于禁用TCP_NODELAY选项，启用Nagle算法。参数err用于返回错误信息，fd为文件描述符。函数返回值为0表示成功，非0表示失败。
int anetDisableTcpNoDelay(char *err, int fd);
int anetTcpKeepAlive(char *err, int fd);
int anetSendTimeout(char *err, int fd, long long ms);
int anetRecvTimeout(char *err, int fd, long long ms);
int anetFdToString(int fd, char *ip, size_t ip_len, int *port, int fd_to_str_type);
// 该函数 anetKeepAlive 用于设置套接字的保活选项。它接受三个参数：错误信息字符串指针 err，文件描述符 fd 和保活间隔时间 interval。函数返回一个整数，表示操作是否成功。
int anetKeepAlive(char *err, int fd, int interval);
int anetFormatAddr(char *fmt, size_t fmt_len, char *ip, int port);
int anetFormatFdAddr(int fd, char *buf, size_t buf_len, int fd_to_str_type);

#endif // ANET_H
