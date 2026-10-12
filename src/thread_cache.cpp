#include "thread_cache.hpp"

#include <cstdlib>
#include <cassert>

namespace slabcache {

std::atomic<RemoteNode*> remote_queue_head{ nullptr };

ThreadCache::ThreadCache() = default;
ThreadCache::~ThreadCache() = default;

std::size_t ThreadCache::size_to_index(std::size_t size) {
    if (size <= static_cast<std::size_t>(SizeClass::Bytes64))  return 0;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes128)) return 1;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes256)) return 2;
    // size <= 512 handled here
    return 3;
}

std::size_t ThreadCache::ptr_to_index(void* ptr) {
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);
    if (addr % static_cast<std::size_t>(SizeClass::Bytes512) == 0) return 3;
    if (addr % static_cast<std::size_t>(SizeClass::Bytes256) == 0) return 2;
    if (addr % static_cast<std::size_t>(SizeClass::Bytes128) == 0) return 1;
    return 0; // 64‑byte class
}

void ThreadCache::drain() {
    // Atomically take the current queue.
    RemoteNode* head = remote_queue_head.exchange(nullptr, std::memory_order_acquire);
    while (head) {
        RemoteNode* next = head->next;
        void* obj = head->obj;
        std::size_t idx = ptr_to_index(obj);
        // Push onto the appropriate free list.
        *reinterpret_cast<void**>(obj) = freeLists[idx];
        freeLists[idx] = obj;
        std::free(head);
        head = next;
    }
}

void* ThreadCache::allocate(std::size_t size) {
    // First, absorb any pending remote frees.
    drain();
    std::size_t idx = size_to_index(size);
    if (freeLists[idx]) {
        void* obj = freeLists[idx];
        freeLists[idx] = *reinterpret_cast<void**>(obj);
        return obj;
    }
    // Fallback to global slab allocator.
    return slab_alloc(size);
}

void ThreadCache::free(void* ptr) {
    if (!ptr) return;
    std::size_t idx = ptr_to_index(ptr);
    // Return to the local cache.
    *reinterpret_cast<void**>(ptr) = freeLists[idx];
    freeLists[idx] = ptr;
}

} // namespace slabcache
