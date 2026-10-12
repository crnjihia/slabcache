#pragma once

#include <string>
#include <optional>
#include <time>
#include <string_view>

namespace slabcache {

/**
 * @brief Simple in‑memory key‑value store used by the RESP3 server.
 *
 * Stores string keys and string values. Integrates with the global TTLManager
 * for expiry and leaves LRU eviction hooks for future use.
 */
class Store {
public:
    Store() = default;
    ~Store() = default;

    // No copy.
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    // Set a key/value pair (overwrites if exists).
    void set(const std::string& key, const std::string& value);

    // Get a value; returns empty optional if missing or expired.
    std::optional<std::string> get(const std::string& key);

    // Delete a key; returns true if the key existed.
    bool del(const std::string& key);

    // Set a TTL for a key (seconds). Returns true if key exists.
    bool expire(const std::string& key, std::chrono::seconds ttl);

    // Retrieve remaining TTL for a key.
    // Returns nullopt if key does not exist.
    // Returns empty optional if key exists but has no TTL.
    // Returns value if TTL present.
    std::optional<std::optional<std::chrono::seconds>> ttl(const std::string& key) const;

private:
    std::unordered_map<std::string, std::string> kv_;
};

} // namespace slabcache
