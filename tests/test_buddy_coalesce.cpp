#include "slabcache/buddy_allocator.hpp"

#include <cassert>
#include <cstdio>

int main() {
    // Allocate two 1 KiB blocks (order will be 11 => 2048 bytes total after header).
    void* a = buddy_alloc(1024);
    void* b = buddy_alloc(1024);
    assert(a != nullptr && b != nullptr);

    // Free both blocks – they should coalesce into a single 2 KiB block.
    buddy_free(a);
    buddy_free(b);

    // Now allocate a 2 KiB block (size 2048). The allocator should be able to provide it.
    void* c = buddy_alloc(2048);
    assert(c != nullptr);

    // Clean up.
    buddy_free(c);

    std::printf("buddy allocator coalescing test passed\n");
    return 0;
}
