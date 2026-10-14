#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace slabcache {

/**
 * @brief Manage TTL (time-to-live) for keys with a background reaper.
 *
 * Stores absolute expiry timestamps for keys and runs a dedicated reaper
 * thread that periodically purges expired keys.
 */
class TTLManager {
   public:
    TTLManager();
    ~TTLManager();

    TTLManager(const TTLManager&) = delete;
    TTLManager& operator=(const TTLManager&) = delete;

    void set(const std::string& key, std::chrono::seconds ttl);
    void remove(const std::string& key);
    bool is_valid(const std::string& key) const;
    std::optional<std::chrono::seconds> time_left(const std::string& key) const;

    void start();
    void stop();

   private:
    void reaper_loop();

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using HeapItem = std::pair<TimePoint, std::string>;
    struct Cmp {
        bool operator()(const HeapItem& a, const HeapItem& b) const {
            return a.first > b.first;
        }
    };

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::unordered_map<std::string, TimePoint> map_;
    std::priority_queue<HeapItem, std::vector<HeapItem>, Cmp> heap_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

extern TTLManager ttl_manager;

inline void ttl_set(const std::string& key, std::chrono::seconds ttl) {
    ttl_manager.set(key, ttl);
}
inline void ttl_remove(const std::string& key) {
    ttl_manager.remove(key);
}
inline bool ttl_is_valid(const std::string& key) {
    return ttl_manager.is_valid(key);
}
inline std::optional<std::chrono::seconds> ttl_remaining(const std::string& key) {
    return ttl_manager.time_left(key);
}
inline void start_ttl_reaper() {
    ttl_manager.start();
}
inline void stop_ttl_reaper() {
    ttl_manager.stop();
}

}  // namespace slabcache
