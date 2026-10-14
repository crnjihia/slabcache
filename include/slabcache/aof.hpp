#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace slabcache {

enum class FsSyncPolicy {
    ALWAYS,
    EVERYSEC,
    NO,
};

/**
 * @brief Append-Only File (AOF) logger.
 *
 * Logs write commands in the RESP3 wire format with configurable fsync policy:
 *   - ALWAYS: fsync after each write.
 *   - EVERYSEC: a background thread fsyncs once per second.
 *   - NO: relies on OS buffering.
 */
class AOFLogger {
   public:
    AOFLogger();
    ~AOFLogger();

    AOFLogger(const AOFLogger&) = delete;
    AOFLogger& operator=(const AOFLogger&) = delete;

    void init(const std::string& path, FsSyncPolicy policy);
    void append(const std::string& cmd);
    void replay();
    void stop();

   private:
    void background_fsync();
    void do_fsync();

    std::ofstream ofs_;
    std::string path_;
    FsSyncPolicy policy_ = FsSyncPolicy::NO;
    std::mutex mtx_;
    std::thread fsync_thread_;
    std::atomic<bool> running_{false};
    static thread_local bool replaying_;
};

extern AOFLogger aof_logger;

}  // namespace slabcache
