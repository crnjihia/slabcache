#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

#include "resp3.hpp"

namespace slabcache {

enum class FsSyncPolicy {
    ALWAYS,
    EVERYSEC,
    NO,
};

/**
 * @brief Append‑Only File (AOF) logger.
 *
 * Logs write commands in the RESP3 wire format to a file so that the database
 * can be reconstructed after a crash. The logger supports three fsync policies:
 *   - ALWAYS: fsync after each write.
 *   - EVERYSEC: a background thread batches fsync every second.
 *   - NO: never fsync (relies on OS buffering).
 */
class AOFLogger {
public:
    AOFLogger();
    ~AOFLogger();

    // non‑copyable
    AOFLogger(const AOFLogger&) = delete;
    AOFLogger& operator=(const AOFLogger&) = delete;

    /**
     * @brief Initialise the logger with a file path and fsync policy.
     *
     * The file is opened in append mode; it will be created if missing.
     */
    void init(const std::string& path, FsSyncPolicy policy);

    /**
     * @brief Append a raw RESP3 command (including CRLF) to the AOF.
     */
    void append(const std::string& cmd);

    /**
     * @brief Replay the AOF file, re‑applying every command to the in‑memory store.
     *
     * This should be called before any other operations (e.g., at server start‑up).
     */
    void replay();

private:
    void background_fsync();

    std::ofstream ofs_;
    std::string path_;
    FsSyncPolicy policy_ = FsSyncPolicy::NO;
    std::mutex mtx_;
    std::thread fsync_thread_;
    std::atomic<bool> running_{false};
    // During replay we suppress logging to avoid infinite recursion.
    static thread_local bool replaying_; // set while replay is underway.
};

// Global logger instance used by the server.
extern AOFLogger aof_logger;

} // namespace slabcache
