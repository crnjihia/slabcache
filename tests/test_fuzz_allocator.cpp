#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "buddy_allocator.hpp"
#include "slab_allocator.hpp"
#include "thread_cache.hpp"

int main(int argc, char* argv[]) {
    long iterations = 100000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--iterations=", 0) == 0) {
            iterations = std::stol(arg.substr(13));
        }
    }

    std::printf("Running fuzz allocator test with %ld iterations...\n", iterations);

    std::mt19937_64 rng(1337);
    std::uniform_int_distribution<int> op_dist(0, 3);
    std::uniform_int_distribution<std::size_t> slab_size_dist(1, 512);
    std::uniform_int_distribution<std::size_t> buddy_size_dist(513, 65536);

    std::vector<void*> slab_ptrs;
    std::vector<void*> buddy_ptrs;

    long double_free_detected = 0;

    for (long it = 0; it < iterations; ++it) {
        int op = op_dist(rng);

        if (op == 0) {
            // Allocate slab
            std::size_t sz = slab_size_dist(rng);
            void* p = slabcache::tc_alloc(sz);
            assert(p != nullptr);
            slab_ptrs.push_back(p);
        } else if (op == 1 && !slab_ptrs.empty()) {
            // Free slab
            std::size_t idx = rng() % slab_ptrs.size();
            void* p = slab_ptrs[idx];
            slab_ptrs[idx] = slab_ptrs.back();
            slab_ptrs.pop_back();
            slabcache::tc_free(p);
        } else if (op == 2) {
            // Allocate buddy
            std::size_t sz = buddy_size_dist(rng);
            void* p = slabcache::buddy_alloc(sz);
            if (p) {
                buddy_ptrs.push_back(p);
            }
        } else if (op == 3 && !buddy_ptrs.empty()) {
            // Free buddy + verify double-free detection
            std::size_t idx = rng() % buddy_ptrs.size();
            void* p = buddy_ptrs[idx];
            buddy_ptrs[idx] = buddy_ptrs.back();
            buddy_ptrs.pop_back();

            assert(slabcache::buddy_is_allocated(p));
            bool ok = slabcache::buddy_safe_free(p);
            assert(ok && "First free must succeed");

            // Double-free attempt: must be detected!
            bool double_free_ok = slabcache::buddy_safe_free(p);
            assert(!double_free_ok && "Double-free must be detected and rejected");
            ++double_free_detected;
        }
    }

    // Clean up remaining allocations
    for (void* p : slab_ptrs) {
        slabcache::tc_free(p);
    }
    for (void* p : buddy_ptrs) {
        slabcache::buddy_safe_free(p);
    }

    std::printf("test_fuzz_allocator passed: %ld double-free attempts successfully caught\n",
                double_free_detected);
    return 0;
}
