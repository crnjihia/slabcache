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
    Bytes64 = 64,
    Bytes128 = 128,
    Bytes256 = 256,
    Bytes512 = 512,
};

/**
 * @brief Map requested byte size to the nearest supported SizeClass.
 */
SizeClass size_to_class(std::size_t size);

/**
 * @brief Determine if a pointer belongs to any slab pool.
 */
bool slab_owns(void* ptr);

/**
 * @brief Get the size class index (0..3) for a pointer belonging to a slab.
 */
std::size_t slab_ptr_to_index(void* ptr);

/**
 * @brief Allocate an object from the appropriate slab pool.
 *
 * @param size Requested size in bytes (must be <= 512).
 * @return Pointer to a usable memory region, or nullptr on failure.
 */
void* slab_alloc(std::size_t size);

/**
 * @brief Return an object to its originating slab pool.
 *
 * @param ptr Pointer previously returned by slab_alloc.
 */
void slab_free(void* ptr);

/**
 * @brief Reset / free all slab pools (primarily for testing and shutdown).
 */
void slab_teardown();

}  // namespace slabcache
