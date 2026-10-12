#include "slab_allocator.hpp"

#include <cassert>

namespace slabcache {

void* slab_alloc(std::size_t size) {
    // Stub: real implementation will choose the correct size class and manage mmap pools.
    assert(false && "slab_alloc stub called – implementation not provided yet");
    return nullptr;
}

void slab_free(void* ptr) {
    // Stub: real implementation will push the object back onto its free list.
    assert(false && "slab_free stub called – implementation not provided yet");
    (void)ptr; // suppress unused warning
}

} // namespace slabcache
