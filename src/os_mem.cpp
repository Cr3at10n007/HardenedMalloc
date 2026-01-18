#include "os_mem.h"
#include "common.h"

#ifdef _WIN32
    #include <windows.h>
    void safe_print(const char* msg) {
        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)strlen(msg), nullptr, nullptr);
    }
    void* os_alloc_pages(size_t size) {
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    void os_free_pages(void* ptr, size_t size) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
    void os_protect_page(void* ptr, size_t size) {
        DWORD old;
        VirtualProtect(ptr, size, PAGE_NOACCESS, &old);
    }
#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include <cstring>
    void safe_print(const char* msg) {
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    void* os_alloc_pages(size_t size) {
        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return (ptr == MAP_FAILED) ? nullptr : ptr;
    }
    void os_free_pages(void* ptr, size_t size) {
        munmap(ptr, size);
    }
    void os_protect_page(void* ptr, size_t size) {
        mprotect(ptr, size, PROT_NONE);
    }
#endif

// Linker definitions
constexpr size_t Config::PAGE_SIZE;
constexpr size_t Config::MAX_SMALL_SIZE;
constexpr uint32_t Config::MAGIC_COOKIE;
constexpr size_t Config::QUARANTINE_THRESHOLD;
constexpr size_t Config::REDZONE_SIZE;
constexpr uint8_t Config::REDZONE_BYTE;