#pragma once

#include <cstddef>

namespace slabcache {

/**
 * @brief Allocate a block of at least `size` bytes using a buddy system.
 *
 * The allocator handles sizes > 512 bytes, rounding up to the next power‑of‑2.
 *
 * @param size Minimum number of bytes requested.
 * @return Pointer to a usable region, or nullptr on failure.
 */
void* buddy_alloc(std::size_t size);

/**
 * @brief Free a block previously allocated with buddy_alloc.
 *
 * @param ptr Pointer returned by buddy_alloc.
 */
void buddy_free(void* ptr);

/**
 * @brief Safe free that returns false on double-free or invalid pointer instead of asserting.
 */
bool buddy_safe_free(void* ptr);

/**
 * @brief Check if a pointer is currently a valid allocated buddy block.
 */
bool buddy_is_allocated(void* ptr);

/**
 * @brief Teardown the buddy allocator pool (cleanup).
 */
void buddy_teardown();

}  // namespace slabcache
