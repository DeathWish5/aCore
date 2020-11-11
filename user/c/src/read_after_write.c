#include <asynccall.h>
#include <barrier.h>
#include <stdio.h>
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
char check[ID_MAX][BS];

int hash_content(char* content)
{
    int hash = rand(), j = 0;
    while (hash > 0) {
        content[j] = hash & 1 ? '1' : '0';
        hash >>= 1;
        ++j;
    }
    return 0;
}

int init_buffer()
{
    int i;
    memset(content, 0, INSIZE);
    memset(check, 0, INSIZE);
    srand(233);
    for (i = 0; i < ID_MAX; ++i) {
        hash_content(content[i]);
    }
    return 0;
}

int init_file(struct async_call_buffer* buffer)
{
    int rid = 0, cid = 0;
    while (cid < ID_MAX) {
        while (*buffer->comp_ring.khead < smp_load_acquire(buffer->comp_ring.ktail)) {
            int cached_head = *buffer->comp_ring.khead;
            struct comp_ring_entry* comp = comp_ring_get_entry(buffer, cached_head);
            if (comp->result != BS) {
                return 1;
            }
            smp_store_release(buffer->comp_ring.khead, cached_head + 1);
            cid++;
        }
        while (rid < ID_MAX && *buffer->req_ring.ktail <
                                   smp_load_acquire(buffer->req_ring.khead) + BUFFER_ENTRIES) {
            int cached_tail = *buffer->req_ring.ktail;
            struct req_ring_entry* req = req_ring_get_entry(buffer, cached_tail);
            async_call_write(req, FD, content[rid], BS, rid * BS);
            rid++;
            smp_store_release(buffer->req_ring.ktail, cached_tail + 1);
        }
    }
    return 0;
}

int check_file(struct async_call_buffer* buffer)
{
    int rid = ID_MAX, cid = ID_MAX;
    while (cid < ID_MAX * 2) {
        while (*buffer->comp_ring.khead < smp_load_acquire(buffer->comp_ring.ktail)) {
            int cached_head = *buffer->comp_ring.khead, id;
            struct comp_ring_entry* comp = comp_ring_get_entry(buffer, cached_head);
            id = comp->user_data;
            if (comp->result != BS || strncmp(content[id], check[id], BS)) {
                return 1;
            }
            smp_store_release(buffer->comp_ring.khead, cached_head + 1);
            cid++;
        }
        while (rid < ID_MAX * 2 && *buffer->req_ring.ktail <
                                   smp_load_acquire(buffer->req_ring.khead) + BUFFER_ENTRIES) {
            int cached_tail = *buffer->req_ring.ktail;
            struct req_ring_entry* req = req_ring_get_entry(buffer, cached_tail);
            async_call_read(req, FD, check[rid], BS, rid * BS);
            rid++;
            smp_store_release(buffer->req_ring.ktail, cached_tail + 1);
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    struct async_call_buffer buffer;
    int ret = 0;
    FD = atoi(argv[1]);
    ret = async_call_buffer_init(BUFFER_ENTRIES, BUFFER_ENTRIES << 1, &buffer);
    if (ret < 0) {
        return ret;
    }
    init_buffer();
    ret = init_file(&buffer);
    if (ret < 0) {
        return ret;
    }
    ret = check_file(&buffer);
    return ret;
}