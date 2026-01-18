cmake_minimum_required(VERSION 3.10)
project(HardenedMalloc VERSION 4.0.0 LANGUAGES CXX)

# --- Compiler Settings ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # Windows: Max optimization (/O2)
    add_compile_options(/O2 /W4 /PERMISSIVE-)
else()
    # Linux/Mac: Max optimization (-O3) + Threading support
    add_compile_options(-O3 -Wall -Wextra -pthread)
endif()

include_directories(include)

add_executable(HSA_Secure
    src/main.cpp
    src/allocator.cpp
    src/central_cache.cpp
    src/os_mem.cpp
)

add_executable(HSA_Standard
    src/main.cpp
)

# --- Linker Settings ---
if(UNIX)
    target_link_libraries(HSA_Secure PRIVATE pthread)
    target_link_libraries(HSA_Standard PRIVATE pthread)
endif()