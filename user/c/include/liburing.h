#ifndef __LIBURING_H__
#define __LIBURING_H__

enum IO_URING_OP {
    IO_URING_OP_READ,
    IO_URING_OP_WRIT,
};

struct io_uring_sqe {
    uint8_t opcode;		/* type of operation for this sqe */
    uint8_t flags;		/* flags */
    uint16_t ioprio;	/* io priority for the request */
    int32_t fd;		    /* file descriptor to do IO on */
    uint64_t off;	    /* offset into file */
    uint64_t addr;      /* pointer to buffer or iovecs */
    uint32_t len;		/* buffer size or number of iovecs */
    uint32_t pad1;
    uint64_t user_data;	/* data to be passed back at completion time */
};

struct io_uring_cqe {
    uint64_t user_data; /* sqe->data submission passed back */
    int32_t result;     /* result code for this event */
    uint32_t pad1;
};

struct io_uring_sq {
    uint32_t *entries;
    uint32_t *khead;    /* index of queue head */
    uint32_t *ktail;    /* index of queue tail */
    uint64_t *kflags;
    struct io_uring_sqe *sqes;
};

struct io_uring_cq {
    uint32_t *entries;
    uint32_t *khead;    /* index of queue head */
    uint32_t *ktail;    /* index of queue tail */
    struct io_uring_cqe *cqes;
};

struct io_uring {
    struct io_uring_sq sq;
    struct io_uring_cq cq;
    uint64_t flags;
    uint64_t ring_fd;
};

struct io_sqring_offsets {
    uint32_t head;
    uint32_t tail;
    uint32_t entries;
    uint32_t sqes;
    uint32_t flags;
};

struct io_cqring_offsets {
    uint32_t head;
    uint32_t tail;
    uint32_t entries;
    uint32_t cqes;
};

struct io_uring_info {
    void* shared_memory_start;
    ssize_t shared_memory_length;
    ssize_t sq_ptr;
    ssize_t cq_ptr;
    struct io_sqring_offsets sq_off;
    struct io_cqring_offsets cq_off;
};

inline void io_uring_prep_rw(int op, struct io_uring_sqe *sqe, int fd, uint64_t off,
                                void *addr, unsigned int len, uint64_t id)
{
    sqe->opcode = op;
    sqe->flags = 0;
    sqe->ioprio = 0;
    sqe->fd = fd;
    sqe->off = off;
    sqe->addr = (uint64_t) addr;
    sqe->len = len;
    sqe->user_data = id;
    sqe->pad1 = 0;
}

inline void io_uring_prep_read(struct io_uring_sqe *sqe, int fd, uint64_t offset,
                                       char* buffer, unsigned int len, uint64_t id)
{
    io_uring_prep_rw(IO_URING_OP_READ, sqe, fd, offset, buffer, len, id);
}

inline void io_uring_prep_write(struct io_uring_sqe *sqe, int fd, uint64_t offset,
                               const char* buffer, unsigned int len, uint64_t id)
{
    io_uring_prep_rw(IO_URING_OP_WRIT, sqe, fd, offset, buffer, len, id);
}

inline int ring_buffer_size(unsigned int head, unsigned int tail, unsigned int capacity) {
    return (tail > head) ? tail - head : capacity + tail - head;
}

#define get_size(obj) (ring_buffer_size(*obj.ktail, *obj.khead, *obj.entries))

inline struct io_uring_sqe* io_uring_get_sqe(struct io_uring_sq* sq)
{
    unsigned int next;
    struct io_uring_sqe *sqe = NULL;

    next = (*sq->ktail + 1) % *sq->entries;

    if (sq->khead != sq->ktail) {
        sqe = &sq->sqes[*sq->ktail];
        *sq->ktail = next;
    }
    return sqe;
}

inline struct io_uring_cqe* io_uring_get_cqe(struct io_uring_cq* cq)
{
    unsigned int next;
    struct io_uring_cqe *cqe = NULL;

    next = (*cq->khead + 1) % *cq->entries;

    if (cq->khead != cq->ktail) {
        cqe = &cq->cqes[*cq->khead];
        *cq->khead = next;
    }
    return cqe;
}

#endif // __LIBURING_H__
