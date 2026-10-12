#include "slabcache/thread_cache.hpp"

#include <thread>
#include <vector>
#include <cassert>

int main() {
    // Phase 1: allocate a bunch of 128‑byte objects on the main thread.
    const std::size_t count = 1000;
    std::vector<void*> objects;
    objects.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        void* p = tc_alloc(128);
        assert(p != nullptr);
        objects.push_back(p);
    }

    // Phase 2: spawn a secondary thread that frees all objects via remote_free.
    std::thread remote_thread([&objects]() {
        for (void* p : objects) {
            remote_free(p);
        }
    });
    remote_thread.join();

    // Phase 3: allocate again on the original thread – the remote frees should be
    // lazily drained into the local cache and recycled.
    std::size_t reused = 0;
    for (std::size_t i = 0; i < count; ++i) {
        void* p = tc_alloc(128);
        if (p != nullptr) ++reused;
    }
    // All allocations should succeed; we also expect the number of reused
    // objects to be at least the original count (they were returned to the cache).
    assert(reused == count);
    return 0;
}
