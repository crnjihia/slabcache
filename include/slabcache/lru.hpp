#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace slabcache {

/**
 * @brief Simple LRU cache with doubly‑linked list and O(1) operations.
 *
 * The cache stores opaque `void*` values indexed by a string key. When the cache
 * exceeds the configured capacity, the least‑recently used entry is evicted.
 *
 * This implementation is deliberately lightweight and does not own the stored
 * values – callers are responsible for managing the lifetime of the `void*`
 * payloads.
 */
class LRUCache {
public:
    /**
     * @brief Construct a cache with the given capacity.
     *
     * @param capacity Maximum number of entries before eviction.
     *                 A capacity of 0 disables eviction (unbounded).
     */
    explicit LRUCache(std::size_t capacity = 0);
    ~LRUCache();

    // No copy – shallow copy would duplicate raw pointers.
    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    // Allow move.
    LRUCache(LRUCache&&) noexcept;
    LRUCache& operator=(LRUCache&&) noexcept;

    /**
     * @brief Insert or update a key/value pair.
     *
     * If the key already exists, its value is replaced and the entry is moved
     * to the front (most‑recent). If the key is new and the cache is at capacity,
     * the least‑recent entry is evicted.
     *
     * @param key   Key string (copied).
     * @param value Opaque pointer stored.
     */
    void put(const std::string& key, void* value);

    /**
     * @brief Retrieve a value for a key.
     *
     * If the key exists, it becomes most‑recent and the stored pointer is
     * returned; otherwise returns nullptr.
     */
    void* get(const std::string& key);

    /**
     * @brief Erase a key/value entry if present.
     *
     * @param key Key to remove.
     * @return true if an entry was removed, false otherwise.
     */
    bool erase(const std::string& key);

    /**
     * @brief Current number of stored entries.
     */
    std::size_t size() const noexcept { return size_; }

    /**
     * @brief Maximum capacity configured for the cache.
     */
    std::size_t capacity() const noexcept { return capacity_; }

private:
    struct Node {
        std::string key;
        void* value = nullptr;
        Node* prev = nullptr;
        Node* next = nullptr;
    };

    // Internal helpers.
    void attach_front(Node* node);
    void detach(Node* node);
    void evict_if_needed();

    std::size_t capacity_ = 0;               // 0 = unbounded.
    std::size_t size_ = 0;
    Node* head_ = nullptr; // Most‑recent.
    Node* tail_ = nullptr; // Least‑recent.
    std::unordered_map<std::string, Node*> map_;
};

} // namespace slabcache
