#pragma once

#include <cstddef>

namespace slabcache {

/**
 * @brief Simple arena (bump-pointer) allocator.
 *
 * Allocation is O(1); reset frees all memory in O(1) as well.
 * Used by the RESP3 parser for per-request scratch buffers.
 */
class Arena {
   public:
    explicit Arena(std::size_t capacity = 1024 * 1024);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&& other) noexcept;
    Arena& operator=(Arena&& other) noexcept;

    void* alloc(std::size_t size);
    void reset();
    std::size_t capacity() const noexcept {
        return capacity_;
    }
    std::size_t used() const noexcept {
        return offset_;
    }

   private:
    void* base_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t offset_ = 0;
};

// Global arena functions for backward-compatibility with tests / stubs
void arena_init(std::size_t capacity);
void arena_reset();
void* arena_alloc(std::size_t size);
void arena_destroy();

}  // namespace slabcache
