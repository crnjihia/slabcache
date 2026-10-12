#pragma once

#include <chrono>
#include <string>
#include <string>

namespace slabcache {

/**
 * @brief Manage TTL (time‑to‑live) for keys with a background reaper.
 *
 * The manager stores an absolute expiry timestamp for each key and runs a
 * dedicated thread that removes expired entries as soon as they become due.
 * Clients can query whether a key is still valid or explicitly remove a key.
 */
class TTLManager {
public:
    TTLManager();
    ~TTLManager();

    // No copy – internal state and thread cannot be duplicated safely.
    TTLManager(const TTLManager&) = delete;
    TTLManager& operator=(const TTLManager&) = delete;
    // Allow move.
    TTLManager(TTLManager&&) noexcept = delete; // not needed

    /**
     * @brief Set a TTL for a key (duration from now).
     *
     * If the key already has a TTL, it is overwritten.
     */
    void set(const std::string& key, std::chrono::seconds ttl);

    /**
     * @brief Remove any TTL entry for a key – e.g., when the key is deleted from the store.
     */
    void remove(const std::string& key);

    /**
     * @brief Check whether a key is still valid (i.e., not expired).
     *
     * @return true if the key exists and its expiry time is in the future.
     */
    /**
     * @brief Return remaining time until expiry for a key.
     *
     * @return std::optional<std::chrono::seconds> containing remaining time if key exists and has a TTL,
     *         std::nullopt if the key does not exist,
     *         empty optional (i.e., std::optional<std::chrono::seconds>{}) if the key exists but has no TTL.
     */
    std::optional<std::chrono::seconds> time_left(const std::string& key) const;


    /**
     * @brief Start the background reaper thread. Called automatically on first use.
     */
    void start();

    /**
     * @brief Stop the background reaper thread and clean up.
     */
    void stop();

private:
    void reaper_loop();

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using HeapItem = std::pair<TimePoint, std::string>; // (expiry, key)
    struct Cmp {
        bool operator()(const HeapItem& a, const HeapItem& b) const {
            return a.first > b.first; // min‑heap (earliest expiry at top)
        }
    };

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::unordered_map<std::string, TimePoint> map_; // key → expiry
    std::priority_queue<HeapItem, std::vector<HeapItem>, Cmp> heap_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

// Global manager instance used by the store.
extern TTLManager ttl_manager;

/**
 * Convenience wrappers that operate on the global ttl_manager.
 */
inline void ttl_set(const std::string& key, std::chrono::seconds ttl) { ttl_manager.set(key, ttl); }
inline void ttl_remove(const std::string& key) { ttl_manager.remove(key); }
inline std::optional<std::chrono::seconds> ttl_remaining(const std::string& key) { return ttl_manager.time_left(key); }
inline void start_ttl_reaper() { ttl_manager.start(); }

} // namespace slabcache
