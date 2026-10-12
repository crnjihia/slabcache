#include "arena_allocator.hpp"

#include <cassert>
#include <cstddef>

namespace slabcache {

static void* arena_base = nullptr;
static std::size_t arena_capacity = 0;
static std::size_t arena_offset = 0;

void arena_init(std::size_t capacity) {
    assert(false && "arena_init stub called – implementation not provided yet");
    (void)capacity;
}

void arena_reset() {
    assert(false && "arena_reset stub called – implementation not provided yet");
}

void* arena_alloc(std::size_t size) {
    assert(false && "arena_alloc stub called – implementation not provided yet");
    (void)size;
    return nullptr;
}

void arena_destroy() {
    assert(false && "arena_destroy stub called – implementation not provided yet");
}

} // namespace slabcache
