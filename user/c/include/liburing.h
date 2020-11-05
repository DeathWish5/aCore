#ifndef __LIBURING_H__
#define __LIBURING_H__

#include "stddef.h"

enum ASYNC_CALL_BUFFER_OP {
    ASYNC_CALL_BUFFER_OP_READ,
    ASYNC_CALL_BUFFER_OP_WRIT,
};

struct request_ring_entry {
    uint8_t opcode;  /* type of operation for this rre */
    uint8_t flags;   /* flags */
    uint16_t ioprio; /* io priority for the request */
    int32_t fd;      /* file descriptor to do IO on */
    uint64_t off;    /* offset into file */
    uint64_t addr;   /* pointer to buffer or iovecs */
    uint32_t len;    /* buffer size or number of iovecs */
    uint32_t pad1;
    uint64_t user_data; /* data to be passed back at completion time */
};

struct complete_ring_entry {
    uint64_t user_data; /* rre->data submission passed back */
    int32_t result;     /* result code for this event */
    uint32_t pad1;
};

struct request_ring {
    uint32_t* entries;
    uint32_t* khead; /* index of queue head */
    uint32_t* ktail; /* index of queue tail */
    uint64_t* kflags;
    struct request_ring_entry* rres;
};

struct complete_ring {
    uint32_t* entries;
    uint32_t* khead; /* index of queue head */
    uint32_t* ktail; /* index of queue tail */
    struct complete_ring_entry* cres;
};

struct async_call_buffer {
    struct request_ring rr;
    struct complete_ring cr;
    uint64_t flags;
    uint64_t buffer_fd;
};

struct request_ring_offsets {
    uint32_t head;
    uint32_t tail;
    uint32_t entries;
    uint32_t rres;
    uint32_t flags;
};

struct complete_ring_offsets {
    uint32_t head;
    uint32_t tail;
    uint32_t entries;
    uint32_t cres;
};

struct async_call_buffer_info {
    void* shared_memory_start;
    ssize_t shared_memory_length;
    ssize_t rr_ptr;
    ssize_t cr_ptr;
    struct request_ring_offsets rr_off;
    struct complete_ring_offsets cr_off;
};

inline void request_ring_prep_rw(int op, struct request_ring_entry* rre, int fd, uint64_t off,
                                 void* addr, unsigned int len, uint64_t id)
{
    rre->opcode = op;
    rre->flags = 0;
    rre->ioprio = 0;
    rre->fd = fd;
    rre->off = off;
    rre->addr = (uint64_t)addr;
    rre->len = len;
    rre->user_data = id;
    rre->pad1 = 0;
}

inline void request_ring_prep_read(struct request_ring_entry* rre, int fd, uint64_t offset,
                                   char* buffer, unsigned int len, uint64_t id)
{
    request_ring_prep_rw(ASYNC_CALL_BUFFER_OP_READ, rre, fd, offset, (void*)buffer, len, id);
}

inline void request_ring_prep_write(struct request_ring_entry* rre, int fd, uint64_t offset,
                                    const char* buffer, unsigned int len, uint64_t id)
{
    request_ring_prep_rw(ASYNC_CALL_BUFFER_OP_WRIT, rre, fd, offset, (void*)buffer, len, id);
}

inline int ring_buffer_size(unsigned int head, unsigned int tail, unsigned int capacity)
{
    return (tail > head) ? tail - head : capacity + tail - head;
}

#define get_size(obj) (ring_buffer_size(*obj.ktail, *obj.khead, *obj.entries))

struct request_ring_entry* request_ring_get_entry(struct request_ring* rr);

struct complete_ring_entry* complete_ring_get_entry(struct complete_ring* cr);

int async_call_buffer_init(struct async_call_buffer* buffer, struct async_call_buffer_info* info);

#endif // __LIBURING_H__
