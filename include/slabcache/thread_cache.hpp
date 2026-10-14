#pragma once

#include <atomic>
#include <cstddef>

#include "slab_allocator.hpp"

namespace slabcache {

// Global lock-free queue for cross-thread frees.
struct RemoteNode {
    void* obj;
    RemoteNode* next;
};

extern std::atomic<RemoteNode*> remote_queue_head;

/**
 * @brief Thread-local cache for slab-allocated objects.
 *
 * Each thread maintains its own free lists for the four slab size classes.
 * Cross-thread frees push to the lock-free remote queue which is lazily drained.
 */
class ThreadCache {
   public:
    ThreadCache();
    ~ThreadCache();

    void* allocate(std::size_t size);
    void free(void* ptr);
    void drain();

   private:
    void* freeLists[4] = {nullptr, nullptr, nullptr, nullptr};
    static std::size_t size_to_index(std::size_t size);
    static std::size_t ptr_to_index(void* ptr);
};

void* tc_alloc(std::size_t size);
void tc_free(void* ptr);
void remote_free(void* ptr);

}  // namespace slabcache
