#pragma once
#include <cstddef>

void* os_alloc_pages(size_t size);
void os_free_pages(void* ptr, size_t size);
void os_protect_page(void* ptr, size_t size);