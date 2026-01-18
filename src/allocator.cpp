#include "thread_cache.h"
#include "os_mem.h"
#include <new>
#include <cstdlib>

thread_local ThreadCache t_cache;
GlobalQuarantine g_quarantine_instance;

void push_global_quarantine(QuarantineBatch* b) {
    g_quarantine_instance.push(b);
}

struct LargeHeader {
    size_t size;
    uint32_t magic;
};

void* operator new(size_t size) {
    // Large Allocation (Guard Page Path)
    if ((size + Config::REDZONE_SIZE) > Config::MAX_SMALL_SIZE) {
        size_t alloc_size = size + sizeof(LargeHeader);
        size_t total_pages = align_up(alloc_size, Config::PAGE_SIZE) + Config::PAGE_SIZE;

        void* ptr = os_alloc_pages(total_pages);
        if (!ptr) throw std::bad_alloc();

        // Guard Page: Protect the last page
        void* guard_addr = (char*)ptr + total_pages - Config::PAGE_SIZE;
        os_protect_page(guard_addr, Config::PAGE_SIZE);

        LargeHeader* h = (LargeHeader*)ptr;
        h->size = total_pages;
        h->magic = Config::MAGIC_COOKIE;

        return (char*)ptr + sizeof(LargeHeader);
    }

    // Small Allocation (Slab Path)
    void* p = t_cache.allocate(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* ptr) noexcept {
    if (!ptr) return;

    // Check if large object (page aligned header)
    LargeHeader* potential_lh = (LargeHeader*)((char*)ptr - sizeof(LargeHeader));

    if ((((uintptr_t)potential_lh) & (Config::PAGE_SIZE - 1)) == 0) {
        if (potential_lh->magic == Config::MAGIC_COOKIE) {
            os_free_pages(potential_lh, potential_lh->size);
            return;
        }
    }

    // Check if small object (valid slab header)
    SlabHeader* sh = SlabHeader::from_ptr(ptr);
    if (sh && sh->magic == Config::MAGIC_COOKIE) {
        t_cache.deallocate(ptr);
        return;
    }

    safe_print("\n[HSA] SECURITY PANIC: Invalid Free (No Magic Found)\n");
    std::abort();
}

void* operator new[](size_t size) { return operator new(size); }
void operator delete[](void* p) noexcept { operator delete(p); }