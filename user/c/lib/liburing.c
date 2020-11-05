#include <liburing.h>

struct request_ring_entry* request_ring_get_entry(struct request_ring* rr)
{
    unsigned int next;
    struct request_ring_entry* rre = NULL;

    next = (*rr->ktail + 1) % *rr->entries;

    if (rr->khead != rr->ktail) {
        rre = &rr->rres[*rr->ktail];
        *rr->ktail = next;
    }
    return rre;
}

struct complete_ring_entry* complete_ring_get_entry(struct complete_ring* cr)
{
    unsigned int next;
    struct complete_ring_entry* cre = NULL;

    next = (*cr->khead + 1) % *cr->entries;

    if (cr->khead != cr->ktail) {
        cre = &cr->cres[*cr->khead];
        *cr->khead = next;
    }
    return cre;
}

int async_call_buffer_init(struct async_call_buffer* buffer, struct async_call_buffer_info* info)
{
    char *rr_ptr, *cr_ptr;
    struct request_ring_offsets* rr_off;
    struct complete_ring_offsets* cr_off;
    struct request_ring* rr;
    struct complete_ring* cr;

    rr_ptr = (char*)info->shared_memory_start + info->rr_ptr;
    cr_ptr = (char*)info->shared_memory_start + info->cr_ptr;
    rr_off = &info->rr_off;
    cr_off = &info->cr_off;
    rr = &buffer->rr;
    cr = &buffer->cr;

    rr->entries = (uint32_t*)(rr_ptr + rr_off->entries);
    rr->khead = (uint32_t*)(rr_ptr + rr_off->head);
    rr->ktail = (uint32_t*)(rr_ptr + rr_off->tail);
    rr->rres = (struct request_ring_entry*)(rr_ptr + rr_off->rres);
    rr->kflags = (uint64_t*)(rr_ptr + rr_off->flags);

    cr->entries = (uint32_t*)(cr_ptr + cr_off->entries);
    cr->khead = (uint32_t*)(cr_ptr + cr_off->head);
    cr->ktail = (uint32_t*)(cr_ptr + cr_off->tail);
    cr->cres = (struct complete_ring_entry*)(cr_ptr + cr_off->cres);

    return 0;
}

int reserve_rr(struct async_call_buffer* buffer, int reserve)
{
    // return async_call_buffer_enter(buffer->buffer_fd, submit, 0, 0);
    int rr_size, rr_entries;
    rr_entries = *buffer->rr.entries;
    do {
        rr_size = get_size(buffer->rr);
    } while (rr_size > rr_entries - reserve);
    return rr_size;
}

int wait_cr(struct async_call_buffer* buffer, int wait)
{
    // return async_call_buffer_enter(buffer->buffer_fd, 0, wait, 0);
    int cr_size;
    do {
        cr_size = get_size(buffer->cr);
    } while (cr_size < wait);
    return cr_size;
}
