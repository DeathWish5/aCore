#include <unistd.h>
#include <string.h>
#include <liburing.h>
#include <stdlib.h>

#define QD	(64)
#define BS	(16 << 10)
#define INSIZE (16 << 20)
#define ID_MAX (INSIZE / BS)
#define min(a, b) (a > b ? b : a)

static int infd, outfd;

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

    sq->kring_entries = (uint32_t*)(sq_ptr + sq_off->ring_entries);
    sq->khead = (uint32_t*)(sq_ptr + sq_off->head);
    sq->ktail = (uint32_t*)(sq_ptr + sq_off->tail);
    sq->sqes = (struct io_uring_sqe *) (sq_ptr + sq_off->sqes);
    sq->kflags = (uint64_t*)(sq_ptr + sq_off->flags);

    cq->kring_entries = (uint32_t*)(cq_ptr + cq_off->ring_entries);
    cq->khead = (uint32_t*)(cq_ptr + cq_off->head);
    cq->ktail = (uint32_t*)(cq_ptr + cq_off->tail);
    cq->cqes = (struct io_uring_cqe *) (cq_ptr + cq_off->cqes);

    return 0;
}

int setup_context(unsigned entries, struct io_uring *ring)
{
    struct io_uring_params para;
    struct io_uring_info info;
    int fd;

    memset(&para, 0, sizeof(para));
    para.sqe_entries = para.cqe_entries = entries;
    para.flags = ring->flags = flags;

    if((fd = io_uring_setup(&para, &info)) < 0)
        return fd;

    ring->ring_fd = fd;
    io_uring_init(ring, &info);
    return 0;
}

static char buffer[BS];

int queue_read(struct io_uring *ring, int id)
{
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(&ring->sq);
    if (!sqe) {
        return -1;
    }
    io_uring_prep_read(sqe, infd, id * BS, buffer, BS, id);
    return 0;
}

int queue_write(struct io_uring *ring, int id)
{
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(&ring->sq);
    if (!sqe) {
        return -1;
    }
    io_uring_prep_write(sqe, infd, id * BS, buffer, BS, id);
    return 0;
}

int io_uring_submit(struct io_uring* ring) {
    return io_uring_enter(ring->fd, get_size(ring->sq), 0, 0);
}

int io_uring_submit(struct io_uring* ring, int submit) {
    return io_uring_enter(ring->ring_fd, submit, 0, 0);
}

int io_uring_wait(struct io_uring* ring, int wait) {
    return io_uring_enter(ring->ring_fd, 0, wait, 0);
}

int init_file(struct io_uring* ring)
{
    struct io_uring_cqe *cqe;
    int ret;
    int sid, cid;

    srand(233);
    sid = cid = 0;

    while (sid < ID_MAX) {
        int queue_remain, i, cq_size, sq_size;
        sq_size = get_size(ring->sq);
        if(sq_size == QD) {
            ret = io_uring_submit(ring);
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

        cq_size = get_size(ring->cq);
        if (cq_size != 0) {
            struct io_uring_cqe* cqe;
            for(i = 0; i < cq_size; ++i) {
                cqe = io_uring_get_cqe(&ring->cq);
                if (!cqe || cqe->result < 0) {
                    return -2;
                }
            }
            cid += cq_size;
            ring->cq.khaed = ring->cq.ktail;
        }
    }

    if (cid < ID_MAX) {
        int cq_size;
        struct io_uring_cqe* cqe;
        io_uring_wait(ring, ID_MAX - cid);
        cq_size = get_size(ring->cq);
        for(i = 0; i < cq_size; ++i) {
            cqe = io_uring_get_cqe(&ring->cq);
            if (!cqe || cqe->result < 0) {
                return -2;
            }
        }
        cid += get_size(ring->cq);
        if (cid != ID_MAX) {
            return -3;
        }
        ring->cq.khaed = ring->cq.ktail;
    }
}

int check_file(struct io_uring* ring)
{
    struct io_uring_cqe *cqe;
    int ret;
    int sid, cid;

    srand(233);
    cid = sid = 0;

    while (sid < ID_MAX) {
        int queue_remain, i, cq_size, sq_size;
        sq_size = get_size(ring->sq);
        if(sq_size == QD) {
            ret = io_uring_submit(ring);
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

        cq_size = get_size(ring->cq);
        if (cq_size != 0) {
            struct io_uring_cqe* cqe;
            for(i = 0; i < cq_size ; ++i) {
                cqe = io_uring_get_cqe(&ring->cq);
                if (!cqe || cqe->result < 0) {
                    return -2;
                }
            }
            cid += cq_size;
            ring->cq.khaed = ring->cq.ktail;
        }
    }

    if (cid < ID_MAX) {
        int cq_size;
        struct io_uring_cqe* cqe;
        io_uring_wait(ring, ID_MAX - cid);
        cq_size = get_size(ring->cq);
        for(i = 0; i < cq_size; ++i) {
            cqe = io_uring_get_cqe(&ring->cq);
            if (!cqe || cqe->result < 0) {
                return -2;
            }
        }
        cid += get_size(ring->cq);
        if (cid != ID_MAX) {
            return -3;
        }
        ring->cq.khaed = ring->cq.ktail;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct io_uring ring;
    int ret;

    infd = atoi(argv[1]);
    outfd = infd + 1;

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