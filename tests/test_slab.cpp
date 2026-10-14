#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "slab_allocator.hpp"

int main() {
    // Phase 1: Test allocation across all size classes
    std::size_t sizes[] = {64, 128, 256, 512};
    for (std::size_t sz : sizes) {
        void* p = slabcache::slab_alloc(sz);
        assert(p != nullptr);
        assert(slabcache::slab_owns(p));
        std::memset(p, 0xAB, sz);
        slabcache::slab_free(p);
    }

    // Phase 2: Test free list recycling (intrusive free list)
    void* p1 = slabcache::slab_alloc(64);
    assert(p1 != nullptr);
    slabcache::slab_free(p1);

    void* p2 = slabcache::slab_alloc(64);
    // Intrusive LIFO free list should return the most recently freed object
    assert(p2 == p1);
    slabcache::slab_free(p2);

    // Phase 3: Allocate multiple pages worth of 64-byte objects (4096 / 64 = 64 per page)
    const std::size_t count = 300;
    std::vector<void*> allocated;
    allocated.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        void* ptr = slabcache::slab_alloc(64);
        assert(ptr != nullptr);
        assert(slabcache::slab_owns(ptr));
        allocated.push_back(ptr);
    }

    // Verify all pointers are unique
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            assert(allocated[i] != allocated[j]);
        }
    }

    // Free all objects
    for (void* ptr : allocated) {
        slabcache::slab_free(ptr);
    }

    std::printf("test_slab passed successfully\n");
    return 0;
}
