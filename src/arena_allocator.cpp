#include "arena_allocator.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/mman.h>
#   include <unistd.h>
#endif

namespace slabcache {

static void* arena_base = nullptr;
static std::size_t arena_capacity = 0;
static std::size_t arena_offset = 0;

// Align size to the maximum alignment required for any type.
static constexpr std::size_t ALIGNMENT = alignof(std::max_align_t);
static std::size_t align_up(std::size_t n) {
    return (n + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

void arena_init(std::size_t capacity) {
    // Disallow double‑initialisation without a destroy.
    assert(arena_base == nullptr && "arena_init called while arena already initialized");
    if (capacity == 0) {
        arena_capacity = 0;
        arena_base = nullptr;
        arena_offset = 0;
        return;
    }
#ifdef _WIN32
    arena_base = VirtualAlloc(nullptr, capacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!arena_base) {
        throw std::bad_alloc();
    }
#else
    // mmap a private anonymous region.
    arena_base = mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arena_base == MAP_FAILED) {
        arena_base = nullptr;
        throw std::bad_alloc();
    }
#endif
    arena_capacity = capacity;
    arena_offset = 0;
}

void arena_reset() {
    assert(arena_base != nullptr && "arena_reset called before arena_init");
    arena_offset = 0;
}

void* arena_alloc(std::size_t size) {
    assert(arena_base != nullptr && "arena_alloc called before arena_init");
    size = align_up(size);
    if (arena_offset + size > arena_capacity) {
        // Not enough space – return nullptr.
        return nullptr;
    }
    void* ptr = static_cast<char*>(arena_base) + arena_offset;
    arena_offset += size;
    return ptr;
}

void arena_destroy() {
    if (!arena_base) {
        return; // nothing allocated
    }
#ifdef _WIN32
    VirtualFree(arena_base, 0, MEM_RELEASE);
#else
    munmap(arena_base, arena_capacity);
#endif
    arena_base = nullptr;
    arena_capacity = 0;
    arena_offset = 0;
}

} // namespace slabcache
