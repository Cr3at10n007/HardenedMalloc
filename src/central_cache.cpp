#include "central_cache.h"
#include "os_mem.h"
#include <mutex>

CentralCache g_central_cache;

int CentralCache::find_set_bit(uint64_t val) {
#ifdef _MSC_VER
    unsigned long b_idx;
    if (_BitScanForward64(&b_idx, val)) return b_idx;
    return -1;
#else
    if (val == 0) return -1;
    return __builtin_ctzll(val);
#endif
}

static uint32_t xorshift32(uint32_t& state) {
    uint32_t x = state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return state = x;
}

int CentralCache::fetch_bulk(int idx, size_t size, void** results, int max_count) {
    std::lock_guard<SpinLock> guard(size_classes[idx].lock);

    int fetched = 0;
    static thread_local uint32_t rng_state = 0x12345678;

    while (fetched < max_count) {
        // Expand slab list if empty
        if (!size_classes[idx].head) {
            void* raw = os_alloc_pages(Config::PAGE_SIZE);
            if (!raw) break; 

            SlabHeader* new_s = new (raw) SlabHeader();
            new_s->magic = Config::MAGIC_COOKIE;
            new_s->object_size = (uint32_t)size;

            size_t avail = Config::PAGE_SIZE - sizeof(SlabHeader);
            size_t count = avail / size;
            if (count > 64) count = 64;

            new_s->free_bitmap = (count == 64) ? ~0ULL : ((1ULL << count) - 1);
            new_s->next_slab = nullptr;

            size_classes[idx].head = new_s;
        }

        SlabHeader* s = size_classes[idx].head;

        while (fetched < max_count && s->free_bitmap != 0) {
            // Randomized slot selection
            uint64_t bitmap = s->free_bitmap;
            int bit = -1;
            int offset = xorshift32(rng_state) % 64;
            uint64_t rotated = (bitmap >> offset) | (bitmap << (64 - offset));
            int found = find_set_bit(rotated);

            if (found != -1) bit = (found + offset) % 64;
            else bit = find_set_bit(bitmap);

            s->free_bitmap &= ~(1ULL << bit);

            uintptr_t base = reinterpret_cast<uintptr_t>(s);
            results[fetched++] = reinterpret_cast<void*>(base + sizeof(SlabHeader) + (bit * size));
        }

        // Retire full slabs
        if (s->free_bitmap == 0) {
            size_classes[idx].head = s->next_slab;
            s->next_slab = nullptr;
        }
        else {
            if (fetched == max_count) break;
        }
    }
    return fetched;
}

void CentralCache::release(void* ptr, int idx) {
    std::lock_guard<SpinLock> guard(size_classes[idx].lock);

    SlabHeader* h = SlabHeader::from_ptr(ptr);
    bool was_full = (h->free_bitmap == 0);

    uintptr_t p_addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t h_addr = reinterpret_cast<uintptr_t>(h);

    // Calculate index from offset
    int bit = (int)((p_addr - (h_addr + sizeof(SlabHeader))) / h->object_size);

    h->free_bitmap |= (1ULL << bit);

    if (was_full) {
        h->next_slab = size_classes[idx].head;
        size_classes[idx].head = h;
    }
}