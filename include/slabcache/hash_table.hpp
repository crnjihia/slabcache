#pragma once

#include <cassert>
#include <cstdint>
#include <string_view>
#include <vector>

namespace slabcache {

/**
 * @brief Robin-Hood open-addressing hash table.
 *
 * Stores keys as std::string_view with a 64-bit hash.
 * Features:
 *   - Robin-Hood probing on insert (swap when probe distance is larger).
 *   - Early termination on lookup using probe distance invariant.
 *   - Backward-shift deletion without tombstones.
 *   - O(1) average lookup, insert, erase.
 */
class RobinHoodMap {
   public:
    explicit RobinHoodMap(std::size_t capacity = 64);
    ~RobinHoodMap();

    RobinHoodMap(const RobinHoodMap&) = delete;
    RobinHoodMap& operator=(const RobinHoodMap&) = delete;

    RobinHoodMap(RobinHoodMap&&) noexcept;
    RobinHoodMap& operator=(RobinHoodMap&&) noexcept;

    bool insert(std::string_view key, void* value);
    void* find(std::string_view key) const;
    bool erase(std::string_view key);
    std::size_t size() const noexcept {
        return count_;
    }
    std::size_t capacity() const noexcept {
        return table_.size();
    }

    struct Entry {
        uint64_t hash = 0;  // 0 signals empty slot
        std::string_view key{};
        void* value = nullptr;
    };

    const std::vector<Entry>& entries() const noexcept {
        return table_;
    }

   private:
    std::vector<Entry> table_;
    std::size_t mask_;
    std::size_t count_ = 0;

    static uint64_t hash_key(std::string_view key);
    void maybe_rehash();
    void rehash(std::size_t new_capacity);
};

}  // namespace slabcache
