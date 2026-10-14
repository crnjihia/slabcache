#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "hash_table.hpp"
#include "slab_allocator.hpp"
#include "thread_cache.hpp"
#include "ttl.hpp"

namespace slabcache {

/**
 * @brief Node stored in the Slabcache key-value store.
 *
 * Links together the Robin Hood hash table and the LRU doubly-linked list.
 */
struct CacheNode {
    std::string key;
    std::string value;
    CacheNode* prev = nullptr;
    CacheNode* next = nullptr;
    std::chrono::steady_clock::time_point expiry{};
    bool has_ttl = false;
};

/**
 * @brief In-memory key-value store simulating a Kenyan telco's USSD session cache.
 *
 * Integrates:
 *   - Robin Hood hash table with 64-bit string_view keys
 *   - O(1) LRU eviction list threaded through nodes
 *   - TTL expiration with lazy access checks and background reaper
 *   - Slab / ThreadCache memory allocation
 */
class Store {
   public:
    explicit Store(std::size_t max_items = 100000);
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool expire(const std::string& key, std::chrono::seconds ttl);
    std::optional<std::optional<std::chrono::seconds>> ttl(const std::string& key);
    std::size_t size() const;
    void clear();

   private:
    void attach_front(CacheNode* node);
    void detach(CacheNode* node);
    void evict_lru();
    bool is_expired(const CacheNode* node) const;
    void free_node(CacheNode* node);

    mutable std::mutex mutex_;
    RobinHoodMap map_;
    CacheNode* head_ = nullptr;  // MRU
    CacheNode* tail_ = nullptr;  // LRU
    std::size_t max_items_;
    std::size_t count_ = 0;
};

}  // namespace slabcache
