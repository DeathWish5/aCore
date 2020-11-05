#include <liburing.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_ENTRIES (64)
#define BS             (16 << 10)
#define INSIZE         (16 << 20)
#define ID_MAX         (INSIZE / BS)
#define min(a, b)      (a > b ? b : a)

int FD;
char content[ID_MAX][BS];
char buf[BS];

int setup_context(unsigned entries, struct async_call_buffer* buffer)
{
    struct async_call_buffer_info info;
    int fd;

    if ((fd = setup_async_call(entries, entries, 0, &info)) < 0)
        return fd;

    buffer->buffer_fd = fd;
    async_call_buffer_init(buffer, &info);
    return 0;
}

int hash_content(char* content)
{
    int hash, j;
    hash = rand();
    j = 0;
    while (hash > 0) {
        content[j] = hash & 1 ? '1' : '0';
        hash >>= 1;
        ++j;
    }
    return 0;
}

inline void reset()
{
    memset(content, 0, INSIZE);
    srand(233);
}

int init_buffer()
{
    int i;
    reset();
    for (i = 0; i < ID_MAX; ++i) {
        hash_content(content[i]);
    }
    return 0;
}

int check_buffer(int id)
{
    memset(buf, 0, BS);
    hash_content(buf);
    return strcmp(buf, content[id]);
}

int queue_read(struct async_call_buffer* buffer, int id)
{
    struct request_ring_entry* rre;
    rre = request_ring_get_entry(&buffer->rr);
    if (!rre) {
        return -1;
    }
    request_ring_prep_read(rre, FD, id * BS, content[id], BS, id);
    return 0;
}

int queue_write(struct async_call_buffer* buffer, int id)
{
    struct request_ring_entry* rre;
    rre = request_ring_get_entry(&buffer->rr);
    if (!rre) {
        return -1;
    }
    request_ring_prep_write(rre, FD, id * BS, content[id], BS, id);
    return 0;
}

int handle_read_rst(struct async_call_buffer* buffer, int cid)
{
    struct complete_ring_entry* cre;
    cre = complete_ring_get_entry(&buffer->cr);
    if (!cre || cre->result != BS) {
        return -2;
    }
    if (check_buffer(cid) != 0)
        return -4;
    return 0;
}

int handle_write_rst(struct async_call_buffer* buffer, int cid)
{
    struct complete_ring_entry* cre;
    cre = complete_ring_get_entry(&buffer->cr);
    if (!cre || cre->result != BS) {
        return -2;
    }
    return 0;
}

int init_file(struct async_call_buffer* buffer)
{
    int ret, rid, cid;

    init_buffer();
    rid = cid = 0;

    while (rid < ID_MAX) {
        int queue_remain, i, cr_size, rr_size;

        cr_size = get_size(buffer->cr);
        for (i = 0; i < cr_size; ++i) {
            ret = handle_write_rst(buffer, cid++);
            if (ret < 0)
                return ret;
        }

        rr_size = reserve_rr(buffer, 1);
        queue_remain = min(BUFFER_ENTRIES - rr_size, ID_MAX - rid);
        for (i = 0; i < queue_remain; ++i) {
            ret = queue_write(buffer, rid);
            if (ret < 0) {
                return ret;
            }
            rid++;
        }
    }

    if (cid < ID_MAX) {
        int cr_size, i;
        cr_size = wait_cr(buffer, ID_MAX - cid);
        for (i = 0; i < cr_size; ++i) {
            ret = handle_write_rst(buffer, cid++);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int check_file(struct async_call_buffer* buffer)
{
    int ret, rid, cid;

    reset();
    cid = rid = 0;

    while (rid < ID_MAX) {
        int queue_remain, i, cr_size, rr_size;
        cr_size = get_size(buffer->cr);
        for (i = 0; i < cr_size; ++i) {
            ret = handle_read_rst(buffer, cid++);
            if (ret < 0) {
                return ret;
            }
        }
        rr_size = reserve_rr(buffer, 1);
        queue_remain = min(BUFFER_ENTRIES - rr_size, ID_MAX - rid);
        for (i = 0; i < queue_remain; ++i) {
            ret = queue_read(buffer, rid++);
            if (ret < 0) {
                return ret;
            }
        }
    }

    if (cid < ID_MAX) {
        int cr_size, i;
        cr_size = wait_cr(buffer, ID_MAX - cid);
        for (i = 0; i < cr_size; ++i) {
            ret = handle_read_rst(buffer, cid++);
            if (ret < 0) {
                return ret;
            }
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    struct async_call_buffer buffer;
    int ret;
    FD = atoi(argv[1]);
    ret = setup_context(BUFFER_ENTRIES, &buffer);
    if (ret < 0) {
        return ret;
    }
    ret = init_file(&buffer);
    if (ret < 0) {
        return ret;
    }
    ret = check_file(&buffer);
    return ret;
}