#include "hash_table.hpp"

#include <utility>
#include <algorithm>

namespace slabcache {

RobinHoodMap::RobinHoodMap(std::size_t capacity) : table_(capacity), mask_(capacity - 1) {
    // Ensure capacity is power of two.
    assert((capacity != 0) && ((capacity & (capacity - 1)) == 0) && "capacity must be power of two");
}

RobinHoodMap::~RobinHoodMap() = default;

RobinHoodMap::RobinHoodMap(RobinHoodMap&& other) noexcept
    : table_(std::move(other.table_)), mask_(other.mask_), count_(other.count_) {
    other.count_ = 0;
    other.mask_ = 0;
}

RobinHoodMap& RobinHoodMap::operator=(RobinHoodMap&& other) noexcept {
    if (this != &other) {
        table_ = std::move(other.table_);
        mask_ = other.mask_;
        count_ = other.count_;
        other.count_ = 0;
        other.mask_ = 0;
    }
    return *this;
}

bool RobinHoodMap::insert(std::string_view key, void* value) {
    maybe_rehash();
    uint64_t h = hash_key(key);
    std::size_t idx = static_cast<std::size_t>(h) & mask_;
    std::size_t probe = 0;
    Entry new_entry{h, key, value};

    while (true) {
        Entry& cur = table_[idx];
        if (cur.hash == 0) {
            // Empty slot – place the new entry.
            cur = new_entry;
            ++count_;
            return true;
        }
        // If keys match, replace value.
        if (cur.hash == h && cur.key == key) {
            cur.value = value;
            return false; // existing entry overwritten
        }
        // Compute probe distance of current entry.
        std::size_t cur_probe = (idx + table_.size() - static_cast<std::size_t>(cur.hash) & mask_) & mask_;
        if (probe > cur_probe) {
            // Robin‑hood swap – incoming entry has larger probe distance.
            std::swap(cur, new_entry);
            probe = cur_probe; // continue probing with the displaced entry.
        }
        // Advance.
        ++probe;
        idx = (idx + 1) & mask_;
    }
}

void* RobinHoodMap::find(std::string_view key) const {
    uint64_t h = hash_key(key);
    std::size_t idx = static_cast<std::size_t>(h) & mask_;
    std::size_t probe = 0;
    while (true) {
        const Entry& cur = table_[idx];
        if (cur.hash == 0) {
            return nullptr; // empty slot means key not present.
        }
        if (cur.hash == h && cur.key == key) {
            return cur.value;
        }
        // Stop if probe distance exceeds possible maximum (optional optimization).
        ++probe;
        idx = (idx + 1) & mask_;
        if (probe > table_.size()) return nullptr; // safety guard.
    }
}

bool RobinHoodMap::erase(std::string_view key) {
    uint64_t h = hash_key(key);
    std::size_t idx = static_cast<std::size_t>(h) & mask_;
    std::size_t probe = 0;
    // Locate the entry.
    while (true) {
        const Entry& cur = table_[idx];
        if (cur.hash == 0) {
            return false; // not found
        }
        if (cur.hash == h && cur.key == key) {
            break; // found
        }
        ++probe;
        idx = (idx + 1) & mask_;
        if (probe > table_.size()) return false; // safety
    }
    // Delete entry and perform backward‑shift deletion.
    std::size_t del_idx = idx;
    table_[del_idx].hash = 0;
    table_[del_idx].key = {};
    table_[del_idx].value = nullptr;

    // Shift subsequent entries back until we hit an empty slot or an entry that is
    // at its ideal position (probe distance 0).
    std::size_t next_idx = (del_idx + 1) & mask_;
    while (true) {
        Entry& cur = table_[next_idx];
        if (cur.hash == 0) break; // stop at empty slot.
        // Compute the ideal slot for this entry.
        std::size_t ideal_idx = static_cast<std::size_t>(cur.hash) & mask_;
        // If the current entry is already at its ideal position, stop.
        if (ideal_idx == next_idx) break;
        // Move entry back one slot.
        table_[del_idx] = cur;
        // Clear moved slot.
        cur.hash = 0;
        cur.key = {};
        cur.value = nullptr;
        del_idx = next_idx;
        next_idx = (next_idx + 1) & mask_;
    }
    --count_;
    return true;
}

void RobinHoodMap::maybe_rehash() {
    // Rehash when load factor > 0.7.
    if (count_ * 10 >= table_.size() * 7) {
        rehash(table_.size() * 2);
    }
}

void RobinHoodMap::rehash(std::size_t new_capacity) {
    // Ensure power of two.
    assert((new_capacity & (new_capacity - 1)) == 0);
    std::vector<Entry> old = std::move(table_);
    table_.assign(new_capacity, {});
    mask_ = new_capacity - 1;
    count_ = 0;
    for (const Entry& e : old) {
        if (e.hash != 0) {
            insert(e.key, e.value);
        }
    }
}

} // namespace slabcache
