#include <stddef.h>
#include <unistd.h>

#include "syscall.h"

int open(const char* path, size_t count, int mode) {
    return syscall(SYS_openat, path, count, mode);
}

int close(int fd) {
    return syscall(SYS_close, fd);
}

ssize_t write(int fd, const void* buf, size_t count)
{
    return syscall(SYS_write, fd, buf, count);
}

int getpid(void)
{
    return syscall(SYS_getpid);
}

int sched_yield(void)
{
    return syscall(SYS_sched_yield);
}

void exit(int code)
{
    syscall(SYS_exit, code);
}

int setup_async_call(int req_capacity, int comp_capacity, void* info, size_t info_size)
{
    return syscall(SYS_setup_async_call, req_capacity, comp_capacity, info, info_size);
}