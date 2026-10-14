cmake_minimum_required(VERSION 3.23)

project(slabcache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SLABCACHE_ENABLE_BENCHMARKS "Enable benchmarks" OFF)
option(COVERAGE "Enable coverage build" OFF)
set(SLABCACHE_BACKEND "native" CACHE STRING "Allocator backend (native, malloc, jemalloc, tcmalloc)")

if(COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

find_package(Threads REQUIRED)

add_library(slabcache STATIC
    src/slab_allocator.cpp
    src/arena_allocator.cpp
    src/buddy_allocator.cpp
    src/thread_cache.cpp
    src/frag_metrics.cpp
    src/hash_table.cpp
    src/lru.cpp
    src/ttl.cpp
    src/resp3.cpp
    src/aof.cpp
    src/store.cpp
)

target_include_directories(slabcache PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/slabcache>
)

target_link_libraries(slabcache PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(slabcache PRIVATE /W4 /WX)
else()
    target_compile_options(slabcache PRIVATE -Wall -Wextra -Werror)
endif()



# Allocator backend support for comparison
if(SLABCACHE_BACKEND STREQUAL "jemalloc")
    find_library(JEMALLOC_LIB jemalloc REQUIRED)
    target_link_libraries(slabcache PUBLIC ${JEMALLOC_LIB})
    target_compile_definitions(slabcache PUBLIC SLABCACHE_USE_JEMALLOC)
elseif(SLABCACHE_BACKEND STREQUAL "tcmalloc")
    find_library(TCMALLOC_LIB tcmalloc_minimal REQUIRED)
    target_link_libraries(slabcache PUBLIC ${TCMALLOC_LIB})
    target_compile_definitions(slabcache PUBLIC SLABCACHE_USE_TCMALLOC)
endif()

# Executable for server
add_executable(slabcache_server src/main.cpp)
target_link_libraries(slabcache_server PRIVATE slabcache)
if(NOT MSVC)
    target_compile_options(slabcache_server PRIVATE -Wall -Wextra -Werror)
endif()

# Benchmarks
if(SLABCACHE_ENABLE_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_executable(bench_alloc benchmarks/bench_alloc.cpp)
    target_link_libraries(bench_alloc PRIVATE slabcache benchmark::benchmark)
    target_include_directories(bench_alloc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/include/slabcache)

    add_executable(bench_kv benchmarks/bench_kv.cpp)
    target_link_libraries(bench_kv PRIVATE slabcache benchmark::benchmark)
    target_include_directories(bench_kv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/include/slabcache)
endif()

enable_testing()
add_subdirectory(tests)
