#include <stddef.h>
#include <unistd.h>
#include <liburing.h>

#include "syscall.h"

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

ssize_t io_uring_setup(unsigned int sqe_entries, unsigned cqe_entries, uint64_t flags, const void* info)
{
    return syscall(SYS_io_uring_setup, sqe_entries, cqe_entries, flags, info);
}

ssize_t io_uring_enter(uint64_t fd, unsigned int to_submit, unsigned int min_complete, uint64_t flags) {
    return syscall(SYS_io_uring_enter, fd, to_submit, min_complete, flags);
}

