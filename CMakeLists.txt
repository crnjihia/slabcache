cmake_minimum_required(VERSION 3.23)

project(slabcache LANGUAGES CXX)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings and treat them as errors
if(MSVC)
    # MSVC specific warnings
    target_compile_options(slabcache PRIVATE /W4 /WX)
else()
    target_compile_options(slabcache PRIVATE -Wall -Wextra -Werror)
endif()

# Add library with core allocator sources (more will be added later)
add_library(slabcache STATIC
    src/slab_allocator.cpp
    src/arena_allocator.cpp
    src/buddy_allocator.cpp
    # Additional source files will be added as they are implemented
    src/thread_cache.cpp
    src/frag_metrics.cpp
    src/hash_table.cpp
    src/lru.cpp
    src/store.cpp
    src/resp3.cpp
)

# Public include directory
target_include_directories(slabcache PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Add a stub executable for the server (main) – will be fleshed out later
add_executable(slabcache_server src/main.cpp)

target_link_libraries(slabcache_server PRIVATE slabcache)

# Enable testing (CTest) – tests will be added later
enable_testing()
add_subdirectory(tests OPTIONAL)
