#include <unistd.h>
#include <string.h>
#include "../include/liburing.h"

#define QD	(64)
#define BS	(16 << 10)
#define INSIZE (16 << 20)
#define ID_MAX (INSIZE / BS)

static int infd, outfd;
static unsigned int RID = 0;
static unsigned int WID = 0;
char buffer[ID_MAX][BS];

int io_uring_init(struct io_uring *ring, struct io_uring_info* info) {
    void* sq_ptr, cq_ptr;
    struct io_sqring_offsets* sq_off;
    struct io_cqring_offsets* cq_off;
    struct io_uring_sq* sq;
    struct io_uring_cq* cq;

    sq_ptr = info->sq_ptr;
    cq_ptr = info->cq_ptr;
    sq_off = info->sq_off;
    cq_off = info->cq_off;
    sq = &ring->sq;
    cq = &ring->cq;

    sq->kring_mask = sq_ptr + sq_off->ring_mask;
    sq->kring_entries = sq_ptr + sq_off->ring_entries;
    sq->kflags = sq_ptr + sq_off->flags;
    sq->khead = sq_ptr + sq_off->head;
    sq->ktail = sq_ptr + sq_off->tail;
    sq->sqes = (struct io_uring_sqe *) (sq_ptr + sq_off->sqes);

    cq->kring_mask = cq_ptr + cq_off->ring_mask;
    cq->kring_entries = cq_ptr + cq_off->ring_entries;
    cq->khead = cq_ptr + cq_off->head;
    cq->ktail = cq_ptr + cq_off->tail;
    cq->cqes = (struct io_uring_cqe *) (cq_ptr + cq_off->cqes);

    return 0;
}

int io_uring_queue_init(unsigned int entries, struct io_uring *ring, unsigned long long flags)
{
    struct io_uring_params para;
    struct io_uring_info info;
    int fd;

    memset(&para, 0, sizeof(para));
    p.sqe_entries = p.cqe_entries = entries;
    p.flags = ring->flags = flags;

    if((fd = io_uring_setup(&para, &info)) < 0)
        return fd;

    ring->ring_fd = fd;

    io_uring_init(ring, &info);

    return 0;
}

int setup_context(unsigned entries, struct io_uring *ring)
{
    int ret;

    ret = io_uring_queue_init(entries, ring, 0);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static int queue_read(struct io_uring *ring, int id)
{
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(&ring->sq);
    if (!sqe) {
        return -1;
    }
    io_uring_prep_read(sqe, infd, id * BS, &buffer[id], BS, id);
    return 0;
}

static int queue_write(struct io_uring *ring, int id)
{
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(&ring->sq);
    if (!sqe) {
        return -1;
    }
    io_uring_prep_write(sqe, infd, id * BS, &buffer[id], BS, id);
    return 0;
}

int io_uring_submit(struct io_uring* ring) {
    return io_uring_enter(ring->fd, get_size(ring->sq), 0, 0);
}

int io_uring_wait(struct io_uring* ring, int wait) {
    return io_uring_enter(ring->fd,0, wait, 0);
}

static int copy_file(struct io_uring *ring)
{
    struct io_uring_cqe *cqe;
    int ret;
    int rid, wid;

    wid = rid = 0;

    while (rid < ID_MAX) {
        int queue_remain, i, cq_size;
        queue_remain = QD - get_size(ring->sq);
        for(i = 0; i < queue_remain; ++i) {
            ret = queue_read(ring, rid);
            if(ret < 0) {
                return ret;
            }
            rid++;
        }

        if (get_size(ring->sq) != 0) {
            ret = io_uring_submit(ring);
            if (ret < 0) {
                return ret;
            }
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
            wid += cq_size;
            ring->cq.khaed = ring->cq.ktail;
        }
    }

    if (wid < ID_MAX) {
        int cq_size;
        struct io_uring_cqe* cqe;
        io_uring_wait(ring, ID_MAX - wid);
        cq_size = get_size(ring->cq);
        for(i = 0; i < cq_size; ++i) {
            cqe = io_uring_get_cqe(&ring->cq);
            if (!cqe || cqe->result < 0) {
                return -2;
            }
        }
        wid += get_size(ring->cq);
        if (wid != ID_MAX) {
            return -3;
        }
        ring->cq.khaed = ring->cq.ktail;
    }

    wid = 0;

    while (wid < ID_MAX) {
        int queue_remain, i, cq_size;
        queue_remain = QD - get_size(ring->sq);
        for(i = 0; i < queue_remain; ++i) {
            ret = queue_write(ring, wid);
            if(ret < 0) {
                return ret;
            }
            wid++;
        }

        if (get_size(ring->sq) != 0) {
            ret = io_uring_submit(ring);
            if (ret < 0) {
                return ret;
            }
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
            wid += cq_size;
            ring->cq.khaed = ring->cq.ktail;
        }
    }

    if (wid < ID_MAX) {
        int cq_size;
        struct io_uring_cqe* cqe;
        io_uring_wait(ring, ID_MAX - wid);
        cq_size = get_size(ring->cq);
        for(i = 0; i < cq_size; ++i) {
            cqe = io_uring_get_cqe(&ring->cq);
            if (!cqe || cqe->result < 0) {
                return -2;
            }
        }
        wid += get_size(ring->cq);
        if (wid != ID_MAX) {
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

    if (setup_context(QD, &ring) < 0) {
        // TODO
    }

    // ret = init_file(&ring);

    ret = copy_file(&ring);
    if(ret < 0) {
        // TODO
    }

    // ret = check_file(&ring);

    return ret;
}