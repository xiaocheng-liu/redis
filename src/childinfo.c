#include <unistd.h>
#include "server.h"

typedef struct
{
    int process_type; /* AOF or RDB child? */
    int on_exit;      /* COW size of active or exited child */
    size_t cow_size;  /* Copy on write size. */
} child_info_data;

/* Open a child-parent channel used in order to move information about the
 * RDB / AOF saving process from the child to the parent (for instance
 * the amount of copy on write memory used) */
void openChildInfoPipe(void)
{
    if (pipe(server.child_info_pipe) == -1)
    {
        /* On error our two file descriptors should be still set to -1,
         * but we call anyway closeChildInfoPipe() since can't hurt. */
        closeChildInfoPipe();
    }
    else if (anetNonBlock(NULL, server.child_info_pipe[0]) != ANET_OK)
    {
        closeChildInfoPipe();
    }
    else
    {
        server.child_info_nread = 0;
    }
}

/* Close the pipes opened with openChildInfoPipe(). */
void closeChildInfoPipe(void)
{
    if (server.child_info_pipe[0] != -1 ||
        server.child_info_pipe[1] != -1)
    {
        close(server.child_info_pipe[0]);
        close(server.child_info_pipe[1]);
        server.child_info_pipe[0] = -1;
        server.child_info_pipe[1] = -1;
        server.child_info_nread = 0;
    }
}

/* Send COW data to parent. */
void sendChildInfo(int process_type, int on_exit, size_t cow_size)
{
    if (server.child_info_pipe[1] == -1)
        return;

    child_info_data buffer = {.process_type = process_type, .on_exit = on_exit, .cow_size = cow_size};
    ssize_t wlen = sizeof(buffer);

    if (write(server.child_info_pipe[1], &buffer, wlen) != wlen)
    {
        /* Nothing to do on error, this will be detected by the other side. */
    }
}

/* Update COW data. */
void updateChildInfo(int process_type, int on_exit, size_t cow_size)
{
    if (!on_exit)
    {
        server.stat_current_cow_bytes = cow_size;
        return;
    }

    if (process_type == CHILD_TYPE_RDB)
    {
        server.stat_rdb_cow_bytes = cow_size;
    }
    else if (process_type == CHILD_TYPE_AOF)
    {
        server.stat_aof_cow_bytes = cow_size;
    }
    else if (process_type == CHILD_TYPE_MODULE)
    {
        server.stat_module_cow_bytes = cow_size;
    }
}

/* Read COW info data from the pipe.
 * if complete data read into the buffer, process type, copy-on-write type and copy-on-write size
 * are stored into *process_type, *on_exit and *cow_size respectively and returns 1.
 * otherwise, the partial data is left in the buffer, waiting for the next read, and returns 0. */
int readChildInfo(int *process_type, int *on_exit, size_t *cow_size)
{
    /* We are using here a static buffer in combination with the server.child_info_nread to handle short reads */
    static child_info_data buffer;
    ssize_t wlen = sizeof(buffer);

    /* Do not overlap */
    if (server.child_info_nread == wlen)
        server.child_info_nread = 0;

    int nread = read(server.child_info_pipe[0], (char *)&buffer + server.child_info_nread, wlen - server.child_info_nread);
    if (nread > 0)
    {
        server.child_info_nread += nread;
    }

    /* We have complete child info */
    if (server.child_info_nread == wlen)
    {
        *process_type = buffer.process_type;
        *on_exit = buffer.on_exit;
        *cow_size = buffer.cow_size;
        return 1;
    }
    else
    {
        return 0;
    }
}

/* Receive COW data from child. */
void receiveChildInfo(void)
{
    if (server.child_info_pipe[0] == -1)
        return;

    int process_type;
    int on_exit;
    size_t cow_size;

    /* Drain the pipe and update child info so that we get the final message. */
    while (readChildInfo(&process_type, &on_exit, &cow_size))
    {
        updateChildInfo(process_type, on_exit, cow_size);
    }
}
