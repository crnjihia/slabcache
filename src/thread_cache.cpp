#include "thread_cache.hpp"

#include <cassert>
#include <cstdlib>

namespace slabcache {

std::atomic<RemoteNode*> remote_queue_head{nullptr};

ThreadCache::ThreadCache() = default;

ThreadCache::~ThreadCache() {
    // Return all remaining cached objects to the global slab allocator
    for (std::size_t i = 0; i < 4; ++i) {
        void* cur = freeLists[i];
        while (cur) {
            void* next = *reinterpret_cast<void**>(cur);
            slab_free(cur);
            cur = next;
        }
        freeLists[i] = nullptr;
    }
}

std::size_t ThreadCache::size_to_index(std::size_t size) {
    if (size <= static_cast<std::size_t>(SizeClass::Bytes64)) return 0;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes128)) return 1;
    if (size <= static_cast<std::size_t>(SizeClass::Bytes256)) return 2;
    return 3;
}

std::size_t ThreadCache::ptr_to_index(void* ptr) {
    return slab_ptr_to_index(ptr);
}

void ThreadCache::drain() {
    RemoteNode* head = remote_queue_head.exchange(nullptr, std::memory_order_acquire);
    while (head) {
        RemoteNode* next = head->next;
        void* obj = head->obj;
        if (slab_owns(obj)) {
            std::size_t idx = ptr_to_index(obj);
            *reinterpret_cast<void**>(obj) = freeLists[idx];
            freeLists[idx] = obj;
        } else {
            // If not a slab object, just free normally
            std::free(obj);
        }
        std::free(head);
        head = next;
    }
}

void* ThreadCache::allocate(std::size_t size) {
    drain();
    if (size <= 512) {
        std::size_t idx = size_to_index(size);
        if (freeLists[idx]) {
            void* obj = freeLists[idx];
            freeLists[idx] = *reinterpret_cast<void**>(obj);
            return obj;
        }
    }
    return slab_alloc(size);
}

void ThreadCache::free(void* ptr) {
    if (!ptr) return;
    if (slab_owns(ptr)) {
        std::size_t idx = ptr_to_index(ptr);
        *reinterpret_cast<void**>(ptr) = freeLists[idx];
        freeLists[idx] = ptr;
    } else {
        slab_free(ptr);
    }
}

static thread_local ThreadCache t_cache;

void* tc_alloc(std::size_t size) {
    return t_cache.allocate(size);
}

void tc_free(void* ptr) {
    if (!ptr) return;
    t_cache.free(ptr);
}

void remote_free(void* ptr) {
    if (!ptr) return;
    RemoteNode* node = static_cast<RemoteNode*>(std::malloc(sizeof(RemoteNode)));
    node->obj = ptr;
    RemoteNode* old = remote_queue_head.load(std::memory_order_relaxed);
    do {
        node->next = old;
    } while (!remote_queue_head.compare_exchange_weak(old, node, std::memory_order_release,
                                                      std::memory_order_relaxed));
}

}  // namespace slabcache
