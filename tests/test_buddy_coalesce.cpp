#include <cassert>
#include <cstdio>

#include "buddy_allocator.hpp"

using namespace slabcache;

int main() {
    // Allocate two 1 KiB blocks
    void* a = buddy_alloc(1024);
    void* b = buddy_alloc(1024);
    assert(a != nullptr && b != nullptr);

    // Free both blocks – they should coalesce into a single block
    buddy_free(a);
    buddy_free(b);

    // Now allocate a 2 KiB block (size 2048). The allocator should satisfy it
    void* c = buddy_alloc(2048);
    assert(c != nullptr);

    buddy_free(c);

    std::printf("buddy allocator coalescing test passed\n");
    return 0;
}
