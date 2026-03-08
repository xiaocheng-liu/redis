#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdio.h>
#include "sds.h"

#define CONN_INFO_LEN 32

typedef struct aeEventLoop aeEventLoop;
typedef struct connection connection;

// 枚举类型 ConnectionState，用于表示连接的状态。
typedef enum
{
    CONN_STATE_NONE = 0,   // 连接状态为空
    CONN_STATE_CONNECTING, // 连接状态为正在连接
    CONN_STATE_ACCEPTING,  // 连接状态为正在接受
    CONN_STATE_CONNECTED,  // 连接状态为已连接
    CONN_STATE_CLOSED,     // 连接状态为已关闭
    CONN_STATE_ERROR       // 连接状态为错误
} ConnectionState;

// 这段代码定义了两个宏，用于表示连接的状态标志位。
// CONN_FLAG_CLOSE_SCHEDULED 表示连接已由某个处理器安排关闭；
// CONN_FLAG_WRITE_BARRIER 表示请求写屏障。
#define CONN_FLAG_CLOSE_SCHEDULED (1 << 0) /* Closed scheduled by a handler */
#define CONN_FLAG_WRITE_BARRIER (1 << 1)   /* Write barrier requested */

// 这段代码定义了两个宏，用于表示连接类型。
// CONN_TYPE_SOCKET 表示使用普通套接字连接，值为1；
// CONN_TYPE_TLS 表示使用TLS加密的连接，值为2。
#define CONN_TYPE_SOCKET 1
#define CONN_TYPE_TLS 2

// 定义了一个函数指针类型 ConnectionCallbackFunc，该函数指针指向一个接受 struct connection *conn 类型参数且返回值为 void 的函数。
// 此函数指针用于回调机制，通常在连接事件发生时调用。
typedef void (*ConnectionCallbackFunc)(connection *conn);

// 这段代码定义了一个名为 ConnectionType 的结构体，用于封装网络连接的各种操作。
// 它包含多个函数指针，每个函数指针对应一种特定的操作，如事件处理、连接、读写、关闭、接受连接、设置读写处理器、获取错误信息、阻塞连接、同步读写等。
typedef struct ConnectionType
{
    void (*ae_handler)(aeEventLoop *el, int fd, void *clientData, int mask);                                                       // 事件处理函数
    int (*connect)(connection *conn, const char *addr, int port, const char *source_addr, ConnectionCallbackFunc connect_handler); // 连接函数
    int (*write)(connection *conn, const void *data, size_t data_len);                                                             // 写函数
    int (*read)(connection *conn, void *buf, size_t buf_len);                                                                      // 读函数
    void (*close)(connection *conn);                                                                                               // 关闭函数
    int (*accept)(connection *conn, ConnectionCallbackFunc accept_handler);                                                        // 接受连接函数
    int (*set_write_handler)(connection *conn, ConnectionCallbackFunc handler, int barrier);                                       // 设置写回调函数
    int (*set_read_handler)(connection *conn, ConnectionCallbackFunc handler);                                                     // 设置读回调函数
    const char *(*get_last_error)(connection *conn);                                                                               // 获取错误码
    int (*blocking_connect)(connection *conn, const char *addr, int port, long long timeout);                                      // 阻塞式连接函数
    ssize_t (*sync_write)(connection *conn, char *ptr, ssize_t size, long long timeout);                                           // 同步写函数
    ssize_t (*sync_read)(connection *conn, char *ptr, ssize_t size, long long timeout);                                            // 同步读函数
    ssize_t (*sync_readline)(connection *conn, char *ptr, ssize_t size, long long timeout);                                        // 同步读行函数
    int (*get_type)(connection *conn);                                                                                             // 获取连接类型函数
} ConnectionType;

// 这段代码定义了一个名为 connection 的结构体，用于表示网络连接。
// 结构体包含连接类型、状态、标志、引用计数、错误码、私有数据、回调函数和文件描述符等成员变量，主要用于管理和处理网络连接的各个属性和行为。
struct connection
{
    ConnectionType *type;                 // 连接类型
    ConnectionState state;                // 连接状态
    short int flags;                      // 标志位
    short int refs;                       // 引用计数
    int last_errno;                       // 错误码
    void *private_data;                   // 定义了一个指向无类型数据的指针，可以指向任何类型的对象或结构体。通常用于存储私有数据或用户自定义数据，具体用途取决于上下文环境。
    ConnectionCallbackFunc conn_handler;  // 连接回调函数
    ConnectionCallbackFunc write_handler; // 写回调函数
    ConnectionCallbackFunc read_handler;  // 读回调函数
    int fd;                               // 文件描述符
};

/* The connection module does not deal with listening and accepting sockets,
 * so we assume we have a socket when an incoming connection is created.
 *
 * The fd supplied should therefore be associated with an already accept()ed
 * socket.
 *
 * connAccept() may directly call accept_handler(), or return and call it
 * at a later time. This behavior is a bit awkward but aims to reduce the need
 * to wait for the next event loop, if no additional handshake is required.
 *
 * IMPORTANT: accept_handler may decide to close the connection, calling connClose().
 * To make this safe, the connection is only marked with CONN_FLAG_CLOSE_SCHEDULED
 * in this case, and connAccept() returns with an error.
 *
 * connAccept() callers must always check the return value and on error (C_ERR)
 * a connClose() must be called.
 */

// 这里会调用到CT_Socket 里面的connSocketAccept方法
static inline int connAccept(connection *conn, ConnectionCallbackFunc accept_handler)
{
    return conn->type->accept(conn, accept_handler);
}

/* Establish a connection.  The connect_handler will be called when the connection
 * is established, or if an error has occurred.
 *
 * The connection handler will be responsible to set up any read/write handlers
 * as needed.
 *
 * If C_ERR is returned, the operation failed and the connection handler shall
 * not be expected.
 */
// 这段代码定义了一个静态内联函数 connConnect，用于连接网络。
// 它调用传入的 connection 对象中指定类型的 connect 方法，传递地址、端口、源地址和连接回调函数作为参数。
static inline int connConnect(connection *conn, const char *addr, int port, const char *src_addr,
                              ConnectionCallbackFunc connect_handler)
{
    return conn->type->connect(conn, addr, port, src_addr, connect_handler);
}

/* Blocking connect.
 *
 * NOTE: This is implemented in order to simplify the transition to the abstract
 * connections, but should probably be refactored out of cluster.c and replication.c,
 * in favor of a pure async implementation.
 */
// 该函数用于连接到指定地址和端口的服务器，并设置连接超时时间。
// 它调用connection对象中type成员的blocking_connect方法来执行实际的连接操作。
static inline int connBlockingConnect(connection *conn, const char *addr, int port, long long timeout)
{
    return conn->type->blocking_connect(conn, addr, port, timeout);
}

/* Write to connection, behaves the same as write(2).
 *
 * Like write(2), a short write is possible. A -1 return indicates an error.
 *
 * The caller should NOT rely on errno. Testing for an EAGAIN-like condition, use
 * connGetState() to see if the connection state is still CONN_STATE_CONNECTED.
 */
static inline int connWrite(connection *conn, const void *data, size_t data_len)
{
    return conn->type->write(conn, data, data_len);
}

/* Read from the connection, behaves the same as read(2).
 *
 * Like read(2), a short read is possible.  A return value of 0 will indicate the
 * connection was closed, and -1 will indicate an error.
 *
 * The caller should NOT rely on errno. Testing for an EAGAIN-like condition, use
 * connGetState() to see if the connection state is still CONN_STATE_CONNECTED.
 */
// 该函数用于从连接中读取数据。它通过调用连接对象 conn 中指定的读取方法来实现，返回读取结果。具体步骤如下：
// 检查连接对象 conn 的类型。
// 调用该类型的读取方法，传入连接对象、缓冲区指针和缓冲区长度。
// 返回读取操作的结果。
static inline int connRead(connection *conn, void *buf, size_t buf_len)
{
    return conn->type->read(conn, buf, buf_len);
}

/* Register a write handler, to be called when the connection is writable.
 * If NULL, the existing handler is removed.
 */
// 这段代码定义了一个内联函数 connSetWriteHandler，用于设置连接的写处理程序。
// 它调用连接类型的 set_write_handler 方法，并传递连接对象、回调函数和一个整数参数 0。
static inline int connSetWriteHandler(connection *conn, ConnectionCallbackFunc func)
{
    return conn->type->set_write_handler(conn, func, 0);
}

/* Register a read handler, to be called when the connection is readable.
 * If NULL, the existing handler is removed.
 */
// 该函数用于设置连接的读取处理器。
// 它通过调用连接对象中指定类型的 set_read_handler 方法，将给定的回调函数设置为读取处理器，并返回该方法的执行结果。
static inline int connSetReadHandler(connection *conn, ConnectionCallbackFunc func)
{
    return conn->type->set_read_handler(conn, func);
}

/* Set a write handler, and possibly enable a write barrier, this flag is
 * cleared when write handler is changed or removed.
 * With barrier enabled, we never fire the event if the read handler already
 * fired in the same event loop iteration. Useful when you want to persist
 * things to disk before sending replies, and want to do that in a group fashion. */
// 该函数用于设置连接的写处理程序，并根据参数决定是否添加内存屏障。具体功能如下：
// 接收三个参数：连接对象 conn，回调函数 func 和屏障标志 barrier
// 调用 conn->type->set_write_handler 方法，传递上述参数并返回其结果
static inline int connSetWriteHandlerWithBarrier(connection *conn, ConnectionCallbackFunc func, int barrier)
{
    return conn->type->set_write_handler(conn, func, barrier);
}

// 该函数用于关闭连接。它调用连接对象 conn 中类型对象 type 的 close 方法来执行具体的关闭操作。
static inline void connClose(connection *conn)
{
    conn->type->close(conn);
}

/* Returns the last error encountered by the connection, as a string.  If no error,
 * a NULL is returned.
 */
// 该函数用于获取连接对象的最后一个错误信息。具体步骤如下：
// 函数接收一个 connection 类型的指针 conn。
// 调用 conn->type->get_last_error(conn)，返回连接对象的最后一个错误信息。
static inline const char *connGetLastError(connection *conn)
{
    return conn->type->get_last_error(conn);
}

// 该函数用于同步写入数据到连接中。
// 它调用连接对象的类型特定的 sync_write 方法，传递连接对象、指针、大小和超时时间作为参数，并返回写入结果。
static inline ssize_t connSyncWrite(connection *conn, char *ptr, ssize_t size, long long timeout)
{
    return conn->type->sync_write(conn, ptr, size, timeout);
}

// 该函数 connSyncRead 是一个内联静态函数，用于从连接中同步读取数据。
// 它调用连接对象 conn 中指定的读取方法 sync_read，并传递指针 ptr、读取大小 size 和超时时间 timeout 参数。
static inline ssize_t connSyncRead(connection *conn, char *ptr, ssize_t size, long long timeout)
{
    return conn->type->sync_read(conn, ptr, size, timeout);
}

// 该函数用于从连接中同步读取一行数据。具体功能如下：
// 函数名为 connSyncReadLine，参数包括连接对象 conn、指向缓冲区的指针 ptr、缓冲区大小 size 和超时时间 timeout。
// 函数通过调用连接对象 conn 中定义的 sync_readline 方法来实现具体读取逻辑，并返回读取的字节数或错误码。
static inline ssize_t connSyncReadLine(connection *conn, char *ptr, ssize_t size, long long timeout)
{
    return conn->type->sync_readline(conn, ptr, size, timeout);
}

/* Return CONN_TYPE_* for the specified connection */
// 该函数用于获取连接对象的类型。它通过调用连接对象中类型结构体的 get_type 方法来返回连接类型。
static inline int connGetType(connection *conn)
{
    return conn->type->get_type(conn);
}

connection *connCreateSocket(void);
connection *connCreateAcceptedSocket(int fd);

connection *connCreateTLS(void);
connection *connCreateAcceptedTLS(int fd, int require_auth);

void connSetPrivateData(connection *conn, void *data);
void *connGetPrivateData(connection *conn);
int connGetState(connection *conn);
int connHasWriteHandler(connection *conn);
int connHasReadHandler(connection *conn);
int connGetSocketError(connection *conn);

/* anet-style wrappers to conns */
int connBlock(connection *conn);
int connNonBlock(connection *conn);
int connEnableTcpNoDelay(connection *conn);
int connDisableTcpNoDelay(connection *conn);
int connKeepAlive(connection *conn, int interval);
int connSendTimeout(connection *conn, long long ms);
int connRecvTimeout(connection *conn, long long ms);
int connPeerToString(connection *conn, char *ip, size_t ip_len, int *port);
int connFormatFdAddr(connection *conn, char *buf, size_t buf_len, int fd_to_str_type);
int connSockName(connection *conn, char *ip, size_t ip_len, int *port);
const char *connGetInfo(connection *conn, char *buf, size_t buf_len);

/* Helpers for tls special considerations */
sds connTLSGetPeerCert(connection *conn);
int tlsHasPendingData(void);
int tlsProcessPendingData(void);

#endif /* CONNECTION_H */
