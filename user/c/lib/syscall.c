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

ssize_t setup_async_call(unsigned int rre_entries, unsigned int cre_entries, uint64_t flags,
                                void* info)
{
    return syscall(SYS_setup_async_call, rre_entries, cre_entries, flags, info);
}

// ssize_t async_call_enter(uint64_t fd, unsigned int to_submit, unsigned int min_complete,
//                                uint64_t flags)
//{
//    return syscall(SYS_async_call_enter, fd, to_submit, min_complete, flags);
//}
