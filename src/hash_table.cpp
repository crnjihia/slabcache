#include "hash_table.hpp"

#include <algorithm>
#include <functional>
#include <utility>

namespace slabcache {

uint64_t RobinHoodMap::hash_key(std::string_view key) {
    uint64_t h = static_cast<uint64_t>(std::hash<std::string_view>{}(key));
    if (h == 0) h = 1;
    return h;
}

RobinHoodMap::RobinHoodMap(std::size_t capacity) {
    // Round up capacity to next power of two, minimum 8
    std::size_t cap = 8;
    while (cap < capacity) {
        cap <<= 1;
    }
    table_.resize(cap);
    mask_ = cap - 1;
    count_ = 0;
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
            cur = new_entry;
            ++count_;
            return true;
        }
        if (cur.hash == h && cur.key == key) {
            cur.value = value;
            return false;
        }
        std::size_t cur_ideal = static_cast<std::size_t>(cur.hash) & mask_;
        std::size_t cur_probe = (idx + table_.size() - cur_ideal) & mask_;
        if (probe > cur_probe) {
            std::swap(cur, new_entry);
            probe = cur_probe;
        }
        ++probe;
        idx = (idx + 1) & mask_;
    }
}

void* RobinHoodMap::find(std::string_view key) const {
    if (count_ == 0) return nullptr;
    uint64_t h = hash_key(key);
    std::size_t idx = static_cast<std::size_t>(h) & mask_;
    std::size_t probe = 0;

    while (true) {
        const Entry& cur = table_[idx];
        if (cur.hash == 0) {
            return nullptr;
        }
        if (cur.hash == h && cur.key == key) {
            return cur.value;
        }
        std::size_t cur_ideal = static_cast<std::size_t>(cur.hash) & mask_;
        std::size_t cur_probe = (idx + table_.size() - cur_ideal) & mask_;
        if (probe > cur_probe) {
            return nullptr;  // Robin Hood invariant
        }
        ++probe;
        idx = (idx + 1) & mask_;
        if (probe > table_.size()) return nullptr;
    }
}

bool RobinHoodMap::erase(std::string_view key) {
    if (count_ == 0) return false;
    uint64_t h = hash_key(key);
    std::size_t idx = static_cast<std::size_t>(h) & mask_;
    std::size_t probe = 0;

    while (true) {
        const Entry& cur = table_[idx];
        if (cur.hash == 0) {
            return false;
        }
        if (cur.hash == h && cur.key == key) {
            break;
        }
        std::size_t cur_ideal = static_cast<std::size_t>(cur.hash) & mask_;
        std::size_t cur_probe = (idx + table_.size() - cur_ideal) & mask_;
        if (probe > cur_probe) {
            return false;
        }
        ++probe;
        idx = (idx + 1) & mask_;
        if (probe > table_.size()) return false;
    }

    std::size_t del_idx = idx;
    table_[del_idx].hash = 0;
    table_[del_idx].key = {};
    table_[del_idx].value = nullptr;

    std::size_t next_idx = (del_idx + 1) & mask_;
    while (true) {
        Entry& cur = table_[next_idx];
        if (cur.hash == 0) break;
        std::size_t ideal_idx = static_cast<std::size_t>(cur.hash) & mask_;
        if (ideal_idx == next_idx) break;

        table_[del_idx] = cur;
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
    if (count_ * 10 >= table_.size() * 7) {
        rehash(table_.size() * 2);
    }
}

void RobinHoodMap::rehash(std::size_t new_capacity) {
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

}  // namespace slabcache
