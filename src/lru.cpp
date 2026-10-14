#include "lru.hpp"

#include <cassert>

namespace slabcache {

LRUCache::LRUCache(std::size_t capacity) : capacity_(capacity) {}

LRUCache::~LRUCache() {
    Node* cur = head_;
    while (cur) {
        Node* next = cur->next;
        delete cur;
        cur = next;
    }
    head_ = tail_ = nullptr;
    size_ = 0;
}

LRUCache::LRUCache(LRUCache&& other) noexcept
    : capacity_(other.capacity_),
      size_(other.size_),
      head_(other.head_),
      tail_(other.tail_),
      map_(std::move(other.map_)) {
    other.head_ = other.tail_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

LRUCache& LRUCache::operator=(LRUCache&& other) noexcept {
    if (this != &other) {
        Node* cur = head_;
        while (cur) {
            Node* next = cur->next;
            delete cur;
            cur = next;
        }
        map_.clear();

        capacity_ = other.capacity_;
        size_ = other.size_;
        head_ = other.head_;
        tail_ = other.tail_;
        map_ = std::move(other.map_);

        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void LRUCache::attach_front(Node* node) {
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

void LRUCache::detach(Node* node) {
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

void LRUCache::evict_if_needed() {
    if (capacity_ == 0) return;  // unbounded
    while (size_ > capacity_) {
        assert(tail_ && "Tail should exist when size exceeds capacity");
        Node* evict = tail_;
        detach(evict);
        map_.erase(evict->key);
        delete evict;
        --size_;
    }
}

void LRUCache::put(const std::string& key, void* value) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        Node* node = it->second;
        node->value = value;
        detach(node);
        attach_front(node);
        return;
    }
    Node* node = new Node{key, value, nullptr, nullptr};
    attach_front(node);
    map_.emplace(key, node);
    ++size_;
    evict_if_needed();
}

void* LRUCache::get(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return nullptr;
    Node* node = it->second;
    detach(node);
    attach_front(node);
    return node->value;
}

bool LRUCache::erase(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    Node* node = it->second;
    detach(node);
    map_.erase(it);
    delete node;
    --size_;
    return true;
}

}  // namespace slabcache
