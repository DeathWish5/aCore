#include <liburing.h>
#include <stddef.h>
#include <unistd.h>

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

// ssize_t async_call_enter(uint64_t fd, unsigned int to_submit, unsigned int min_complete,
//                                uint64_t flags)
//{
//    return syscall(SYS_async_call_enter, fd, to_submit, min_complete, flags);
//}

int setup_async_call(int req_capacity, int comp_capacity, void* info, size_t info_size)
{
    return syscall(SYS_setup_async_call, req_capacity, comp_capacity, info, info_size);
}
