#include <asynccall.h>
#include <barrier.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_ENTRIES (64)
//#define BS             (16 << 10)
//#define INSIZE         (16 << 20)
#define BS             (32)
#define INSIZE         (8 * 32)
#define ID_MAX         (INSIZE / BS)
#define min(a, b)      (a > b ? b : a)

#define FD (stdout)

int hash(char* buf) {
    int i, checksum = 0;
    for(i = 0; i < BS / 32; ++i) {
        checksum ^= *(int*)(buf + 4*i);
    }
    return checksum;
}

int rand_buffer(char* buf)
{
    int i;
    for(i = 0; i < BS / 32; ++i) {
        *(int*)(buf + 4*i) = rand();
    }
    return 0;
}

int init_buffer(char* buf, int* check)
{
    int i;
    for (i = 0; i < ID_MAX; ++i) {
        rand_buffer(&buf[BS*i]);
        check[i] = hash(&buf[BS*i]);
    }
    return 0;
}

int run_test(struct async_call_buffer* buffer) {
    char buf[INSIZE];
    int check[ID_MAX], ret;
    memset(buf, 0, INSIZE);
    srand(233);
    init_buffer(buf, check);
    ret = write_file(buffer, buf);
    if(ret < 0)
        return ret;
    memset(buf, 0, INSIZE);
    return check_file(buffer, buf, check);
}

int write_file(struct async_call_buffer* buffer, char* buf)
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
            async_call_write(req, FD, &buf[BS*rid], BS, rid * BS);
            rid++;
            smp_store_release(buffer->req_ring.ktail, cached_tail + 1);
        }
    }
    return 0;
}

int check_file(struct async_call_buffer* buffer, char* buf, int* check)
{
    int rid = 0, cid = 0;
    while (cid < ID_MAX) {
        while (*buffer->comp_ring.khead < smp_load_acquire(buffer->comp_ring.ktail)) {
            int cached_head = *buffer->comp_ring.khead, id;
            struct comp_ring_entry* comp = comp_ring_get_entry(buffer, cached_head);
            id = comp->user_data;
            if (comp->result != BS || hash(buf[BS*id]) != check[id]) {
                return 1;
            }
            smp_store_release(buffer->comp_ring.khead, cached_head + 1);
            cid++;
        }
        while (rid < ID_MAX && *buffer->req_ring.ktail <
                                   smp_load_acquire(buffer->req_ring.khead) + BUFFER_ENTRIES) {
            int cached_tail = *buffer->req_ring.ktail;
            struct req_ring_entry* req = req_ring_get_entry(buffer, cached_tail);
            async_call_read(req, FD, &buf[BS*rid], BS, rid * BS);
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
    ret = async_call_buffer_init(BUFFER_ENTRIES, BUFFER_ENTRIES << 1, &buffer);
    if (ret < 0) {
        puts("something error");
        return ret;
    }
    return run_test(&buffer);
}