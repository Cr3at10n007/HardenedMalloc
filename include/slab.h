#pragma once
#include "common.h"
#include <cstdint>

struct SlabHeader {
    SlabHeader* next_slab;
    uint64_t free_bitmap;   // 64 slots max
    uint32_t object_size;
    uint32_t magic;         // Security Canary
    uint32_t max_objects;
    char _pad[8];           // Padding for alignment

    // Retrieve SlabHeader from object pointer via page masking
    static SlabHeader* from_ptr(void* ptr) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t header_addr = addr & ~(Config::PAGE_SIZE - 1);
        return reinterpret_cast<SlabHeader*>(header_addr);
    }
};