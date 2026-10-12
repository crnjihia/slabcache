#include "store.hpp"
#include "ttl.hpp"

#include <unordered_map>
#include <optional>
#include <chrono>

namespace slabcache {

void Store::set(const std::string& key, const std::string& value) {
    kv_[key] = value;
    // Ensure any existing TTL is retained (do not clear).
}

std::optional<std::string> Store::get(const std::string& key) {
    // First check TTL validity.
    if (!ttl_is_valid(key)) {
        // Expired – clean up.
        kv_.erase(key);
        ttl_remove(key);
        return std::nullopt;
    }
    auto it = kv_.find(key);
    if (it == kv_.end()) return std::nullopt;
    return it->second;
}

bool Store::del(const std::string& key) {
    bool existed = kv_.erase(key) > 0;
    ttl_remove(key);
    return existed;
}

bool Store::expire(const std::string& key, std::chrono::seconds ttl) {
    if (kv_.find(key) == kv_.end()) return false;
    ttl_set(key, ttl);
    return true;
}

std::optional<std::optional<std::chrono::seconds>> Store::ttl(const std::string& key) const {
    // Return nullopt if key not in store.
    if (kv_.find(key) == kv_.end()) return std::nullopt;
    // Query remaining TTL via global manager.
    auto remaining_opt = ttl_remaining(key);
    if (!remaining_opt) {
        // No TTL record for this key (key exists but no expiry).
        return std::optional<std::optional<std::chrono::seconds>>(std::nullopt);
    }
    // remaining_opt may be empty optional indicating expired (should have been cleaned up already).
    if (!*remaining_opt) {
        // Expired – clean up store.
        // Note: ttl_is_valid would have already removed on get, but we ensure.
        const_cast<Store*>(this)->kv_.erase(key);
        ttl_remove(key);
        return std::nullopt;
    }
    // TTL present, return the remaining seconds.
    return std::optional<std::optional<std::chrono::seconds>>(std::make_optional(*remaining_opt));
}


} // namespace slabcache
