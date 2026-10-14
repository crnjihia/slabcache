#include <cassert>
#include <cstdio>
#include <cstring>

#include "arena_allocator.hpp"

int main() {
    // Phase 1: Test global arena
    slabcache::arena_init(1024 * 64);

    void* p1 = slabcache::arena_alloc(128);
    assert(p1 != nullptr);
    std::memset(p1, 0x42, 128);

    void* p2 = slabcache::arena_alloc(256);
    assert(p2 != nullptr);
    assert(p2 > p1);

    // O(1) Reset
    slabcache::arena_reset();

    // After reset, next allocation starts at the beginning
    void* p3 = slabcache::arena_alloc(128);
    assert(p3 == p1);

    slabcache::arena_destroy();

    // Phase 2: Test RAII Arena class
    {
        slabcache::Arena local_arena(4096);
        assert(local_arena.capacity() == 4096);
        assert(local_arena.used() == 0);

        void* a = local_arena.alloc(500);
        assert(a != nullptr);
        assert(local_arena.used() >= 500);

        local_arena.reset();
        assert(local_arena.used() == 0);

        void* b = local_arena.alloc(500);
        assert(b == a);
    }

    std::printf("test_arena_reset passed successfully\n");
    return 0;
}
