#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>
#include <string>

#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"

const int BENCH_ITERATIONS = 500000;
const int BENCH_THREADS = 4;

struct Timer {
    std::chrono::high_resolution_clock::time_point start;
    std::string name;

    Timer(std::string n) : name(n) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double ms = us / 1000.0;
        double ops_sec = (us > 0) ? ((double)BENCH_ITERATIONS * 1000000.0 / us) : 0.0;

        std::cout << "[" << CYAN << name << RESET << "] "
            << ms << " ms  "
            << "(" << (long long)ops_sec << " ops/sec)\n";
    }
};

void touch_memory(void* ptr) {
    reinterpret_cast<volatile char*>(ptr)[0] = 1;
}

void run_small_churn() {
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        int* p = new int(i);
        touch_memory(p);
        delete p;
    }
}

void run_large_churn() {
    int limit = BENCH_ITERATIONS / 10;
    for (int i = 0; i < limit; ++i) {
        char* p = new char[4096];
        touch_memory(p);
        delete[] p;
    }
}

void run_contention_test() {
    std::vector<std::thread> threads;
    for (int i = 0; i < BENCH_THREADS; i++) {
        threads.emplace_back(run_small_churn);
    }
    for (auto& th : threads) th.join();
}

void run_benchmarks() {
    std::cout << "\n" << YELLOW << "=== ALLOCATOR PERFORMANCE BENCHMARK ===" << RESET << "\n";
    std::cout << "Iterations: " << BENCH_ITERATIONS << "\n";
    std::cout << "Threads:    " << BENCH_THREADS << "\n\n";

    { Timer t("Single Thread - Small Object (32B)"); run_small_churn(); }
    { Timer t("Single Thread - Large Object (4KB)"); run_large_churn(); }
    { Timer t("Multi Thread  - Lock Contention");    run_contention_test(); }
}

void print_header(const char* title) {
    std::cout << "\n--------------------------------------------\n";
    std::cout << " SECURITY TEST: " << title << "\n";
    std::cout << "--------------------------------------------\n";
}

void test_uaf() {
    print_header("Use-After-Free (Quarantine Check)");
    struct Secret { int id; char data[16]; };
    Secret* objA = new Secret();
    objA->id = 12345;
    std::cout << "[Step 1] Allocating Object A: " << objA << "\n";
    delete objA;
    std::cout << "[Step 2] Freed Object A.\n";
    Secret* objB = new Secret();
    std::cout << "[Step 3] Allocating Object B: " << objB << "\n";
    if (objA == objB) std::cout << RED << "[FAIL] Reuse!\n" << RESET;
    else std::cout << GREEN << "[PASS] Quarantine Active.\n" << RESET;
    if (objA != objB) delete objB;
}

void test_redzone() {
    print_header("Precise Redzone Corruption");
    volatile char* ptr = new char[24];
    ptr[24] = 0x00;
    std::cout << "EXPECTATION: " << RED << "PANIC" << RESET << "\n";
    delete[](char*)ptr;
    std::cout << RED << "[FAIL] Survived.\n" << RESET;
}

void test_guard() {
    print_header("Guard Page");
    size_t size = 4096;
    char* ptr = new char[size];
    uintptr_t page_boundary = ((uintptr_t)ptr + size + 4095) & ~(4095);
    if (page_boundary == (uintptr_t)ptr + size) page_boundary += 4096;
    std::cout << "Target: " << (void*)page_boundary << "\n";
    std::cout << "EXPECTATION: " << RED << "CRASH" << RESET << "\n";
    *(volatile char*)page_boundary = 'X';
    std::cout << RED << "[FAIL] Survived.\n" << RESET;
    delete[] ptr;
}

void test_double() {
    print_header("Double Free");
    int* p = new int(42);
    delete p;
    std::cout << "EXPECTATION: " << RED << "PANIC" << RESET << "\n";
    delete p;
    std::cout << RED << "[FAIL] Survived.\n" << RESET;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [mode]\n\n";
        std::cout << "Modes:\n";
        std::cout << "  bench         Run performance benchmarks\n";
        std::cout << "  test-uaf      Test Use-After-Free mitigation\n";
        std::cout << "  test-redzone  Test Redzone detection\n";
        std::cout << "  test-guard    Test Guard Page protection\n";
        std::cout << "  test-double   Test Double Free detection\n";
        return 1;
    }
    std::string mode = argv[1];
    if (mode == "bench") run_benchmarks();
    else if (mode == "test-uaf") test_uaf();
    else if (mode == "test-redzone") test_redzone();
    else if (mode == "test-guard") test_guard();
    else if (mode == "test-double") test_double();
    else {
        std::cout << "Unknown mode: " << mode << "\n";
        return 1;
    }
    return 0;
}