#include "lru.hpp"

#include <cassert>

int main() {
    // Capacity 3, will evict least‑recent.
    slabcache::LRUCache cache(3);

    int v1 = 1, v2 = 2, v3 = 3, v4 = 4;
    cache.put("one", &v1);
    cache.put("two", &v2);
    cache.put("three", &v3);

    // Access "one" to make it most‑recent.
    assert(cache.get("one") == &v1);

    // Insert fourth entry; should evict "two" (least‑recent after access).
    cache.put("four", &v4);
    assert(cache.get("two") == nullptr); // evicted
    assert(cache.get("three") == &v3);
    assert(cache.get("four") == &v4);
    assert(cache.get("one") == &v1);

    // Erase "three" and ensure it disappears.
    bool erased = cache.erase("three");
    assert(erased);
    assert(cache.get("three") == nullptr);

    // Size should now be 3 (one, four, maybe others). Verify size.
    assert(cache.size() == 3);
    return 0;
}
