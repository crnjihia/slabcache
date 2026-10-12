#pragma once

#include <cstddef>
#include <atomic>

#include "slab_allocator.hpp"

namespace slabcache {

/**
 * @brief Thread‑local cache for slab‑allocated objects.
 *
 * Each thread maintains its own free lists for the four slab size classes.
 * When a thread frees an object that does not belong to its cache, it should
 * call `remote_free` (defined below) which enqueues the pointer onto a global
 * lock‑free queue. The cache lazily drains that queue on the next allocation.
 */
class ThreadCache {
public:
    ThreadCache();
    ~ThreadCache();

    // Allocate an object of at least `size` bytes (must be one of the slab classes).
    void* allocate(std::size_t size);
    // Return an object to the thread‑local cache.
    void free(void* ptr);

    // Drain pending remote frees into this cache.
    void drain();

private:
    // Intrusive free list heads for each size class (index 0..3).
    void* freeLists[4] = { nullptr, nullptr, nullptr, nullptr };

    // Helper to map a size to the corresponding free‑list index.
    static std::size_t size_to_index(std::size_t size);
    // Helper to infer the size‑class index from an object pointer.
    static std::size_t ptr_to_index(void* ptr);
};

// Global lock‑free queue for cross‑thread frees.
struct RemoteNode {
    void* obj;
    RemoteNode* next;
};

// The atomic head of the remote‑free queue.
extern std::atomic<RemoteNode*> remote_queue_head;

// Convenience wrappers used by client code.

/**
 * Allocate an object using the thread‑local cache.
 */
inline void* tc_alloc(std::size_t size) {
    thread_local ThreadCache cache;
    return cache.allocate(size);
}

/**
 * Return an object to the thread‑local cache.
 */
inline void tc_free(void* ptr) {
    if (!ptr) return;
    thread_local ThreadCache cache;
    cache.free(ptr);
}

/**
 * Enqueue a pointer for remote freeing – typically called from a thread that did
 * not allocate the object.
 */
inline void remote_free(void* ptr) {
    if (!ptr) return;
    // Allocate a node using the system allocator – we intentionally avoid the
    // slab cache to prevent recursion.
    RemoteNode* node = static_cast<RemoteNode*>(std::malloc(sizeof(RemoteNode)));
    node->obj = ptr;
    // Push onto the atomic stack.
    RemoteNode* old = remote_queue_head.load(std::memory_order_relaxed);
    do {
        node->next = old;
    } while (!remote_queue_head.compare_exchange_weak(old, node,
                std::memory_order_release,
                std::memory_order_relaxed));
}

} // namespace slabcache
