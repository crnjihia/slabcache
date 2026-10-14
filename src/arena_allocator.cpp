#include "arena_allocator.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace slabcache {

static constexpr std::size_t ALIGNMENT = alignof(std::max_align_t);

static std::size_t align_up(std::size_t n) {
    return (n + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

Arena::Arena(std::size_t capacity) {
    if (capacity == 0) return;
#ifdef _WIN32
    base_ = VirtualAlloc(nullptr, capacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base_) throw std::bad_alloc();
#else
    base_ = mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base_ == MAP_FAILED) {
        base_ = nullptr;
        throw std::bad_alloc();
    }
#endif
    capacity_ = capacity;
    offset_ = 0;
}

Arena::~Arena() {
    if (base_) {
#ifdef _WIN32
        VirtualFree(base_, 0, MEM_RELEASE);
#else
        munmap(base_, capacity_);
#endif
        base_ = nullptr;
        capacity_ = 0;
        offset_ = 0;
    }
}

Arena::Arena(Arena&& other) noexcept
    : base_(other.base_), capacity_(other.capacity_), offset_(other.offset_) {
    other.base_ = nullptr;
    other.capacity_ = 0;
    other.offset_ = 0;
}

Arena& Arena::operator=(Arena&& other) noexcept {
    if (this != &other) {
        this->~Arena();
        base_ = other.base_;
        capacity_ = other.capacity_;
        offset_ = other.offset_;
        other.base_ = nullptr;
        other.capacity_ = 0;
        other.offset_ = 0;
    }
    return *this;
}

void* Arena::alloc(std::size_t size) {
    if (!base_) return nullptr;
    size = align_up(size);
    if (offset_ + size > capacity_) {
        return nullptr;
    }
    void* ptr = static_cast<char*>(base_) + offset_;
    offset_ += size;
    return ptr;
}

void Arena::reset() {
    offset_ = 0;
}

// Global arena instance
static Arena* g_global_arena = nullptr;

void arena_init(std::size_t capacity) {
    if (g_global_arena) {
        delete g_global_arena;
    }
    g_global_arena = new Arena(capacity);
}

void arena_reset() {
    assert(g_global_arena != nullptr && "arena_reset called before arena_init");
    g_global_arena->reset();
}

void* arena_alloc(std::size_t size) {
    assert(g_global_arena != nullptr && "arena_alloc called before arena_init");
    return g_global_arena->alloc(size);
}

void arena_destroy() {
    delete g_global_arena;
    g_global_arena = nullptr;
}

}  // namespace slabcache
