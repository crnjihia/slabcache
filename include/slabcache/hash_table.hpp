#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <cassert>

namespace slabcache {

/**
 * @brief Simple Robin‑Hood open‑addressing hash table.
 *
 * Stores keys as std::string_view (the caller must ensure the underlying string
 * outlives the table) and a generic void* value pointer. The hash is a 64‑bit
 * value computed with std::hash<std::string_view>.
 *
 * Features:
 *   - Robin‑Hood probing on insert (swap when probe distance is larger).
 *   - Backward‑shift deletion to fill gaps without leaving tombstones.
 *   - O(1) average lookup, insert, erase.
 */
class RobinHoodMap {
public:
    // Construct with an initial capacity (must be a power of two). Default 64.
    explicit RobinHoodMap(std::size_t capacity = 64);
    ~RobinHoodMap();

    // No copy – avoid shallow copies of string_view pointers.
    RobinHoodMap(const RobinHoodMap&) = delete;
    RobinHoodMap& operator=(const RobinHoodMap&) = delete;

    // Allow move.
    RobinHoodMap(RobinHoodMap&&) noexcept;
    RobinHoodMap& operator=(RobinHoodMap&&) noexcept;

    // Insert a key/value pair. Returns true if a new entry was added, false if the key
    // already existed (in which case the value is overwritten).
    bool insert(std::string_view key, void* value);

    // Find the value associated with a key. Returns nullptr if not found.
    void* find(std::string_view key) const;

    // Erase a key/value pair. Returns true if an entry was removed.
    bool erase(std::string_view key);

    // Current number of stored elements.
    std::size_t size() const noexcept { return count_; }

private:
    struct Entry {
        uint64_t hash = 0;               // 0 signals empty slot (hash of empty key never used).
        std::string_view key{};         // Valid only if hash != 0.
        void* value = nullptr;
    };

    std::vector<Entry> table_;            // Power‑of‑two sized table.
    std::size_t mask_;                   // table_.size() - 1 for fast modulo.
    std::size_t count_ = 0;              // Number of occupied slots.

    // Compute 64‑bit hash for a key.
    static uint64_t hash_key(std::string_view key) {
        return static_cast<uint64_t>(std::hash<std::string_view>{}(key));
    }

    // Resize the table when load factor exceeds 0.7.
    void maybe_rehash();                  // Implementation defined below.
    void rehash(std::size_t new_capacity);
};

} // namespace slabcache
