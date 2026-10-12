#include "buddy_allocator.hpp"

#include <cassert>

namespace slabcache {

void* buddy_alloc(std::size_t size) {
    assert(false && "buddy_alloc stub called – implementation not provided yet");
    (void)size;
    return nullptr;
}

void buddy_free(void* ptr) {
    assert(false && "buddy_free stub called – implementation not provided yet");
    (void)ptr;
}

} // namespace slabcache
