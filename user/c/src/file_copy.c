#include <unistd.h>
#include <string.h>
#include <liburing.h>
#include <stdlib.h>

#define QD	(64)
#define BS	(16 << 10)
#define INSIZE (16 << 20)
#define ID_MAX (INSIZE / BS)
#define min(a, b) (a > b ? b : a)

static int FD;

int io_uring_init(struct io_uring *ring, struct io_uring_info* info) {
    char* sq_ptr, cq_ptr;
    struct io_sqring_offsets* sq_off;
    struct io_cqring_offsets* cq_off;
    struct io_uring_sq* sq;
    struct io_uring_cq* cq;

    sq_ptr = (char*)info->shared_memory_start + info->sq_ptr;
    cq_ptr = (char*)info->shared_memory_start + info->cq_ptr;
    sq_off = &info->sq_off;
    cq_off = &info->cq_off;
    sq = &ring->sq;
    cq = &ring->cq;

    sq->entries = (uint32_t*)(sq_ptr + sq_off->entries);
    sq->khead = (uint32_t*)(sq_ptr + sq_off->head);
    sq->ktail = (uint32_t*)(sq_ptr + sq_off->tail);
    sq->sqes = (struct io_uring_sqe *) (sq_ptr + sq_off->sqes);
    sq->kflags = (uint64_t*)(sq_ptr + sq_off->flags);

    cq->entries = (uint32_t*)(cq_ptr + cq_off->entries);
    cq->khead = (uint32_t*)(cq_ptr + cq_off->head);
    cq->ktail = (uint32_t*)(cq_ptr + cq_off->tail);
    cq->cqes = (struct io_uring_cqe *) (cq_ptr + cq_off->cqes);

    return 0;
}

int setup_context(unsigned entries, struct io_uring *ring)
{
    struct io_uring_info info;
    int fd;

    if((fd = io_uring_setup(entries, entries, 0, &info)) < 0)
        return fd;

    ring->ring_fd = fd;
    io_uring_init(ring, &info);
    return 0;
}

static char buffer[ID_MAX][BS];
static char buf[BS];

int hash_buffer(char* buffer) {
    int hash, j;
    hash = rand();
    j = 0;
    while (hash > 0) {
        buffer[j] = hash & 1 ? '1' : '0';
        hash >>= 1;
        ++j;
    }
    return 0;
}

inline void reset() {
    memset(buffer, 0, INSIZE);
    srand(233);
}

int init_buffer() {
    int i;
    reset();
    for(i = 0; i < ID_MAX; ++i) {
        hash_buffer(buffer[i]);
    }
    return 0;
}

int check_buffer(int id) {
    memset(buf, 0, BS);
    hash_buffer(buf);
    return strcmp(buf, buffer[id]);
}

int queue_read(struct io_uring *ring, int id)
{
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(&ring->sq);
    if (!sqe) {
        return -1;
    }
    io_uring_prep_read(sqe, FD, id * BS, buffer[id], BS, id);
    return 0;
}

int queue_write(struct io_uring *ring, int id)
{
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(&ring->sq);
    if (!sqe) {
        return -1;
    }
    io_uring_prep_write(sqe, FD, id * BS, buffer[id], BS, id);
    return 0;
}

int io_uring_submit(struct io_uring* ring, int submit) {
    return io_uring_enter(ring->ring_fd, submit, 0, 0);
}

int io_uring_wait(struct io_uring* ring, int wait) {
    return io_uring_enter(ring->ring_fd, 0, wait, 0);
}

int init_file(struct io_uring* ring)
{
    int ret;
    int sid, cid;

    init_buffer();
    sid = cid = 0;

    while (sid < ID_MAX) {
        int queue_remain, i, cq_size, sq_size;

        cq_size = get_size(ring->cq);
        if (cq_size != 0) {
            struct io_uring_cqe* cqe;
            for(i = 0; i < cq_size; ++i) {
                cqe = io_uring_get_cqe(&ring->cq);
                if (!cqe || cqe->result != BS) {
                    return -2;
                }
            }
            cid += cq_size;
        }

        sq_size = get_size(ring->sq);
        if(sq_size == QD) {
            ret = io_uring_submit(ring, sq_size);
            if (ret < 0) {
                return ret;
            }
        }

        queue_remain = min(QD - sq_size, ID_MAX - sid);
        for(i = 0; i < queue_remain; ++i) {
            ret = queue_write(ring, sid);
            if(ret < 0) {
                return ret;
            }
            sid++;
        }
    }

    if (cid < ID_MAX) {
        int cq_size, i;
        struct io_uring_cqe* cqe;
        io_uring_wait(ring, ID_MAX - cid);
        cq_size = get_size(ring->cq);
        for(i = 0; i < cq_size; ++i) {
            cqe = io_uring_get_cqe(&ring->cq);
            if (!cqe || cqe->result != BS) {
                return -2;
            }
        }
        cid += cq_size;
        if (cid != ID_MAX) {
            return -3;
        }
    }
    return 0;
}

int check_file(struct io_uring* ring)
{
    int ret;
    int sid, cid;

    reset();
    cid = sid = 0;

    while (sid < ID_MAX) {
        cq_size = get_size(ring->cq);
        if (cq_size != 0) {
            struct io_uring_cqe* cqe;
            for(i = 0; i < cq_size ; ++i) {
                cqe = io_uring_get_cqe(&ring->cq);
                if (!cqe || cqe->result != BS) {
                    return -2;
                }
                if(check_buffer(cid++) != 0)
                    return -4;
            }
        }

        int queue_remain, i, cq_size, sq_size;
        sq_size = get_size(ring->sq);
        if(sq_size == QD) {
            ret = io_uring_submit(ring, sq_size);
            if (ret < 0) {
                return ret;
            }
        }

        queue_remain = min(QD - sq_size, ID_MAX - sid);
        for(i = 0; i < queue_remain; ++i) {
            ret = queue_read(ring, sid);
            if(ret < 0) {
                return ret;
            }
            sid++;
        }
    }

    if (cid < ID_MAX) {
        int cq_size, i;
        struct io_uring_cqe* cqe;
        io_uring_wait(ring, ID_MAX - cid);
        cq_size = get_size(ring->cq);
        for(i = 0; i < cq_size; ++i) {
            cqe = io_uring_get_cqe(&ring->cq);
            if (!cqe || cqe->result != BS) {
                return -2;
            }
            if(check_buffer(cid++) != 0)
                return -4;
        }
        if (cid != ID_MAX) {
            return -3;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct io_uring ring;
    int ret;

    FD = atoi(argv[1]);

    ret = setup_context(QD, &ring);
    if (ret < 0) {
        return ret;
    }

    ret = init_file(&ring);

    if(ret < 0) {
        return ret;
    }

    ret = check_file(&ring);

    return ret;
}