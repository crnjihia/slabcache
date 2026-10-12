#pragma once

#include <cstddef>
#include <cstdint>

namespace slabcache {

/**
 * @brief Size classes (in bytes) supported by the slab allocator.
 *
 * The allocator provides separate pools for each of these fixed sizes.
 */
enum class SizeClass : std::size_t {
    Bytes64  = 64,
    Bytes128 = 128,
    Bytes256 = 256,
    Bytes512 = 512,
};

/**
 * @brief Allocate an object from the appropriate slab pool.
 *
 * @param size Requested size in bytes. Must be one of the supported size classes.
 * @return Pointer to a usable memory region, or nullptr on failure.
 *
 * @note This is a stub implementation – it aborts if called.
 */
void* slab_alloc(std::size_t size);

/**
 * @brief Return an object to its originating slab pool.
 *
 * @param ptr Pointer previously returned by slab_alloc.
 *
 * @note This is a stub implementation – it aborts if called.
 */
void slab_free(void* ptr);

} // namespace slabcache
