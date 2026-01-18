#pragma once
#include "common.h"
#include "os_mem.h" 
#include "central_cache.h"
#include <mutex>
#include <cstring>

struct QuarantineBatch {
    static const int CAPACITY = 128;
    void* ptrs[CAPACITY];
    size_t count = 0;
    size_t total_bytes = 0;
    QuarantineBatch* next = nullptr;
};

class GlobalQuarantine {
    SpinLock lock;
    QuarantineBatch* head = nullptr;
    QuarantineBatch* tail = nullptr;
    size_t current_usage = 0;

public:
    void push(QuarantineBatch* batch) {
        // Poison memory pattern for UAF detection
        for (size_t i = 0; i < batch->count; i++) {
            uint64_t* p = (uint64_t*)batch->ptrs[i];
            *p = 0xDEADDEADDEADDEAD;
        }

        std::lock_guard<SpinLock> guard(lock);

        if (tail) tail->next = batch;
        else head = batch;
        tail = batch;

        current_usage += batch->total_bytes;

        if (current_usage > Config::QUARANTINE_THRESHOLD) {
            purge();
        }
    }

private:
    void purge() {
        while (current_usage > Config::QUARANTINE_THRESHOLD && head) {
            QuarantineBatch* old = head;
            head = head->next;
            if (!head) tail = nullptr;

            current_usage -= old->total_bytes;

            for (size_t i = 0; i < old->count; i++) {
                void* ptr = old->ptrs[i];
                SlabHeader* h = SlabHeader::from_ptr(ptr);

                size_t size = h->object_size;
                int idx = -1;
                
                for (int k = 0; k < Config::MAX_SIZE_CLASSES; k++) {
                    if ((size_t)(32 << k) == size) { idx = k; break; }
                }

                if (idx >= 0) {
                    g_central_cache.release(ptr, idx);
                }
            }
            os_free_pages(old, sizeof(QuarantineBatch));
        }
    }
};