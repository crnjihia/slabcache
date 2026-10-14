#include <cassert>

#include "lru.hpp"

int main() {
    slabcache::LRUCache cache(3);

    int v1 = 1, v2 = 2, v3 = 3, v4 = 4;
    cache.put("one", &v1);
    cache.put("two", &v2);
    cache.put("three", &v3);

    // Access "one" to make it most-recent
    assert(cache.get("one") == &v1);

    // Insert fourth entry; evicts "two" (least-recent)
    cache.put("four", &v4);
    assert(cache.get("two") == nullptr);
    assert(cache.get("three") == &v3);
    assert(cache.get("four") == &v4);
    assert(cache.get("one") == &v1);

    // Erase "three"
    bool erased = cache.erase("three");
    assert(erased);
    assert(cache.get("three") == nullptr);

    // Size should now be 2 ("four" and "one")
    assert(cache.size() == 2);
    return 0;
}
