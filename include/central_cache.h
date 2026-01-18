#pragma once
#include "slab.h"
#include "common.h"

struct alignas(Config::CACHE_LINE) SizeClass {
    SpinLock lock;
    SlabHeader* head;
};

class CentralCache {
public:
    SizeClass size_classes[Config::MAX_SIZE_CLASSES];

    CentralCache() {
        for (int i = 0; i < Config::MAX_SIZE_CLASSES; i++) {
            size_classes[i].head = nullptr;
        }
    }

    int fetch_bulk(int idx, size_t size, void** results, int max_count);
    void release(void* ptr, int idx);

private:
    int find_set_bit(uint64_t val);
};

extern CentralCache g_central_cache;