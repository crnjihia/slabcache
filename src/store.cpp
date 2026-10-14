#include "store.hpp"

#include <cassert>
#include <new>

namespace slabcache {

Store::Store(std::size_t max_items) : map_(64), max_items_(max_items) {}

Store::~Store() {
    clear();
}

void Store::attach_front(CacheNode* node) {
    node->prev = nullptr;
    node->next = head_;
    if (head_) {
        head_->prev = node;
    }
    head_ = node;
    if (!tail_) {
        tail_ = node;
    }
}

void Store::detach(CacheNode* node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        head_ = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        tail_ = node->prev;
    }
    node->prev = node->next = nullptr;
}

bool Store::is_expired(const CacheNode* node) const {
    if (!node || !node->has_ttl) return false;
    return std::chrono::steady_clock::now() >= node->expiry;
}

void Store::free_node(CacheNode* node) {
    if (!node) return;
    node->~CacheNode();
    if (slab_owns(node)) {
        slab_free(node);
    } else {
        std::free(node);
    }
}

void Store::evict_lru() {
    if (!tail_) return;
    CacheNode* evict = tail_;
    detach(evict);
    map_.erase(evict->key);
    free_node(evict);
    --count_;
}

void Store::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    CacheNode* existing = static_cast<CacheNode*>(map_.find(key));
    if (existing) {
        existing->value = value;
        detach(existing);
        attach_front(existing);
        return;
    }

    if (max_items_ > 0 && count_ >= max_items_) {
        evict_lru();
    }

    // Allocate from slab thread cache
    void* mem = tc_alloc(sizeof(CacheNode));
    if (!mem) {
        mem = std::malloc(sizeof(CacheNode));
    }
    CacheNode* node = new (mem) CacheNode();
    node->key = key;
    node->value = value;

    attach_front(node);
    map_.insert(node->key, node);
    ++count_;
}

std::optional<std::string> Store::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheNode* node = static_cast<CacheNode*>(map_.find(key));
    if (!node) return std::nullopt;

    if (is_expired(node)) {
        detach(node);
        map_.erase(node->key);
        free_node(node);
        --count_;
        return std::nullopt;
    }

    detach(node);
    attach_front(node);
    return node->value;
}

bool Store::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheNode* node = static_cast<CacheNode*>(map_.find(key));
    if (!node) return false;

    detach(node);
    map_.erase(node->key);
    free_node(node);
    --count_;
    return true;
}

bool Store::expire(const std::string& key, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheNode* node = static_cast<CacheNode*>(map_.find(key));
    if (!node) return false;

    if (ttl.count() <= 0) {
        detach(node);
        map_.erase(node->key);
        free_node(node);
        --count_;
        return true;
    }

    node->has_ttl = true;
    node->expiry = std::chrono::steady_clock::now() + ttl;
    ttl_set(key, ttl);
    return true;
}

std::optional<std::optional<std::chrono::seconds>> Store::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheNode* node = static_cast<CacheNode*>(map_.find(key));
    if (!node) return std::nullopt;

    if (is_expired(node)) {
        detach(node);
        map_.erase(node->key);
        free_node(node);
        --count_;
        return std::nullopt;
    }

    if (!node->has_ttl) {
        return std::optional<std::optional<std::chrono::seconds>>(std::nullopt);
    }

    auto now = std::chrono::steady_clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(node->expiry - now);
    if (secs.count() < 0) secs = std::chrono::seconds(0);
    return std::optional<std::optional<std::chrono::seconds>>(secs);
}

std::size_t Store::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

void Store::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheNode* cur = head_;
    while (cur) {
        CacheNode* next = cur->next;
        free_node(cur);
        cur = next;
    }
    head_ = tail_ = nullptr;
    count_ = 0;
}

}  // namespace slabcache
