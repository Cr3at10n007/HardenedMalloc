#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <new>
#include <thread>
#include <immintrin.h> 
#include <cstdlib>     

struct Config {
    // System Constants
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t CACHE_LINE = 64;

    // Allocator Thresholds
    static constexpr size_t MAX_SMALL_SIZE = 1024;
    static constexpr int MAX_SIZE_CLASSES = 6;

    // Security Parameters
    static constexpr uint32_t MAGIC_COOKIE = 0xDEADBEEF;
    static constexpr size_t QUARANTINE_THRESHOLD = 4 * 1024 * 1024; // 4MB
    static constexpr size_t REDZONE_SIZE = 8;
    static constexpr uint8_t REDZONE_BYTE = 0xCD;

    // Performance Tuning
    static constexpr int BATCH_SIZE = 32;
};

// Utilities
void safe_print(const char* msg);

inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

inline void fill_redzone(void* slot_ptr, size_t slot_size) {
    uint8_t* p = (uint8_t*)slot_ptr + slot_size - Config::REDZONE_SIZE;
    for (size_t i = 0; i < Config::REDZONE_SIZE; i++) p[i] = Config::REDZONE_BYTE;
}

inline void check_redzone(void* slot_ptr, size_t slot_size) {
    uint8_t* p = (uint8_t*)slot_ptr + slot_size - Config::REDZONE_SIZE;
    for (size_t i = 0; i < Config::REDZONE_SIZE; i++) {
        if (p[i] != Config::REDZONE_BYTE) {
            safe_print("\n[HSA] SECURITY PANIC: Redzone Corrupted\n");
            std::abort();
        }
    }
}

class SpinLock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        int spins = 0;
        while (flag.test_and_set(std::memory_order_acquire)) {
            _mm_pause();
            if (spins++ > 100) {
                std::this_thread::yield();
                spins = 0;
            }
        }
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};