#pragma once
#include "common.h"
#include "quarantine.h"
#include "central_cache.h"
#include "os_mem.h"

void push_global_quarantine(QuarantineBatch* b);

inline int get_size_idx(size_t size) {
    size_t required = size + Config::REDZONE_SIZE;
    if (required <= 32)   return 0;
    if (required <= 64)   return 1;
    if (required <= 128)  return 2;
    if (required <= 256)  return 3;
    if (required <= 512)  return 4;
    if (required <= 1024) return 5;
    return -1;
}

inline size_t get_size_from_idx(int idx) {
    return 32 << idx;
}

struct TLAB {
    void* free_list[Config::BATCH_SIZE];
    int count = 0;
};

class ThreadCache {
    QuarantineBatch* local_batch = nullptr;
    bool initializing = false;
    TLAB tlabs[Config::MAX_SIZE_CLASSES];

public:
    void* allocate(size_t size) {
        int idx = get_size_idx(size);
        if (idx < 0) return nullptr;

        TLAB& tlab = tlabs[idx];
        void* ptr = nullptr;

        if (tlab.count > 0) {
            ptr = tlab.free_list[--tlab.count];
        }
        else {
            size_t slot_size = get_size_from_idx(idx);
            int fetched = g_central_cache.fetch_bulk(
                idx,
                slot_size,
                tlab.free_list,
                Config::BATCH_SIZE
            );

            if (fetched > 0) {
                tlab.count = fetched;
                ptr = tlab.free_list[--tlab.count];
            }
        }

        if (ptr) {
            fill_redzone(ptr, get_size_from_idx(idx));
        }

        return ptr;
    }

    void deallocate(void* ptr) {
        if (!local_batch) {
            if (initializing) return;
            initializing = true;
            local_batch = (QuarantineBatch*)os_alloc_pages(sizeof(QuarantineBatch));
            if (local_batch) new (local_batch) QuarantineBatch();
            initializing = false;
        }

        if (!local_batch) return;

        SlabHeader* h = SlabHeader::from_ptr(ptr);
        if (h) {
            check_redzone(ptr, h->object_size);
            local_batch->total_bytes += h->object_size;
        }

        local_batch->ptrs[local_batch->count++] = ptr;

        if (local_batch->count >= QuarantineBatch::CAPACITY) {
            push_global_quarantine(local_batch);
            local_batch = nullptr;
        }
    }
};