#include "buddy_allocator.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace slabcache {

static constexpr std::size_t MIN_ORDER = 10;  // 1024 bytes
static constexpr std::size_t MAX_ORDER = 26;  // 64 MiB
static constexpr std::size_t TOTAL_SIZE = std::size_t(1) << MAX_ORDER;

struct BlockHeader {
    std::size_t order;
    bool allocated;
    BlockHeader* next;
};

static void* buddy_base = nullptr;
static BlockHeader* free_list[MAX_ORDER + 1] = {};
static bool initialized = false;
static std::mutex buddy_mutex;

static std::size_t order_of(std::size_t sz) {
    sz += sizeof(BlockHeader);
    std::size_t order = MIN_ORDER;
    std::size_t block = std::size_t(1) << order;
    while (block < sz) {
        ++order;
        block <<= 1;
        assert(order <= MAX_ORDER && "Requested size exceeds buddy allocator capacity");
    }
    return order;
}

static void init_pool() {
    assert(!initialized && "Buddy pool already initialized");
#ifdef _WIN32
    buddy_base = VirtualAlloc(nullptr, TOTAL_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buddy_base) throw std::bad_alloc();
#else
    buddy_base =
        mmap(nullptr, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buddy_base == MAP_FAILED) {
        buddy_base = nullptr;
        throw std::bad_alloc();
    }
#endif
    BlockHeader* initial = static_cast<BlockHeader*>(buddy_base);
    initial->order = MAX_ORDER;
    initial->allocated = false;
    initial->next = nullptr;
    free_list[MAX_ORDER] = initial;
    initialized = true;
}

static bool remove_from_freelist(BlockHeader* block, std::size_t order) {
    BlockHeader* prev = nullptr;
    BlockHeader* cur = free_list[order];
    while (cur) {
        if (cur == block) {
            if (prev) {
                prev->next = cur->next;
            } else {
                free_list[order] = cur->next;
            }
            cur->next = nullptr;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

static void insert_into_freelist(BlockHeader* block) {
    block->allocated = false;
    block->next = free_list[block->order];
    free_list[block->order] = block;
}

static BlockHeader* buddy_of(BlockHeader* block) {
    std::uintptr_t offset =
        reinterpret_cast<std::uintptr_t>(block) - reinterpret_cast<std::uintptr_t>(buddy_base);
    std::uintptr_t buddy_offset = offset ^ (std::size_t(1) << block->order);
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(buddy_base) + buddy_offset);
}

static bool is_valid_ptr(void* ptr) {
    if (!ptr || !initialized || !buddy_base) return false;
    std::uintptr_t p = reinterpret_cast<std::uintptr_t>(ptr);
    std::uintptr_t base = reinterpret_cast<std::uintptr_t>(buddy_base);
    return (p >= base + sizeof(BlockHeader) && p < base + TOTAL_SIZE);
}

void* buddy_alloc(std::size_t size) {
    std::lock_guard<std::mutex> lock(buddy_mutex);
    if (!initialized) {
        init_pool();
    }
    assert(size > 512 && "buddy_alloc called for size <= 512 – use slab allocator");

    std::size_t needed_order = order_of(size);
    std::size_t cur_order = needed_order;
    while (cur_order <= MAX_ORDER && free_list[cur_order] == nullptr) {
        ++cur_order;
    }
    if (cur_order > MAX_ORDER) {
        return nullptr;
    }

    BlockHeader* block = free_list[cur_order];
    free_list[cur_order] = block->next;
    block->next = nullptr;

    while (cur_order > needed_order) {
        --cur_order;
        BlockHeader* buddy = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(block) +
                                                            (std::size_t(1) << cur_order));
        buddy->order = cur_order;
        buddy->allocated = false;
        buddy->next = free_list[cur_order];
        free_list[cur_order] = buddy;
        block->order = cur_order;
    }

    block->allocated = true;
    return static_cast<void*>(reinterpret_cast<char*>(block) + sizeof(BlockHeader));
}

bool buddy_safe_free(void* ptr) {
    std::lock_guard<std::mutex> lock(buddy_mutex);
    if (!is_valid_ptr(ptr)) return false;

    BlockHeader* block =
        reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));
    if (!block->allocated) {
        return false;  // Double free detected!
    }
    block->allocated = false;

    while (block->order < MAX_ORDER) {
        BlockHeader* buddy = buddy_of(block);
        if (!buddy->allocated && buddy->order == block->order) {
            bool removed = remove_from_freelist(buddy, buddy->order);
            if (!removed) break;
            block = (block < buddy) ? block : buddy;
            ++block->order;
        } else {
            break;
        }
    }
    insert_into_freelist(block);
    return true;
}

void buddy_free(void* ptr) {
    if (!ptr) return;
    bool ok = buddy_safe_free(ptr);
    (void)ok;
    assert(ok && "buddy_free detected double-free or invalid pointer");
}

bool buddy_is_allocated(void* ptr) {
    std::lock_guard<std::mutex> lock(buddy_mutex);
    if (!is_valid_ptr(ptr)) return false;
    BlockHeader* block =
        reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));
    return block->allocated;
}

void buddy_teardown() {
    std::lock_guard<std::mutex> lock(buddy_mutex);
    if (initialized && buddy_base) {
#ifdef _WIN32
        VirtualFree(buddy_base, 0, MEM_RELEASE);
#else
        munmap(buddy_base, TOTAL_SIZE);
#endif
        buddy_base = nullptr;
        for (std::size_t i = 0; i <= MAX_ORDER; ++i) {
            free_list[i] = nullptr;
        }
        initialized = false;
    }
}

}  // namespace slabcache
