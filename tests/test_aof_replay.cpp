#include <cassert>
#include <cstdio>
#include <ctime>

#include "aof.hpp"
#include "resp3.hpp"
#include "ttl.hpp"

int main() {
    const std::string aof_path = "test_aof.aof";
    std::remove(aof_path.c_str());

    // Initialise logger with NO fsync
    slabcache::aof_logger.init(aof_path, slabcache::FsSyncPolicy::NO);
    slabcache::start_ttl_reaper();

    auto send = [&](const std::string& req) { return slabcache::handle_request(req); };

    // SET foo bar
    std::string set_req = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    std::string set_resp = send(set_req);
    assert(set_resp == "+OK\r\n");

    // DEL foo
    std::string del_req = "*2\r\n$3\r\nDEL\r\n$3\r\nfoo\r\n";
    std::string del_resp = send(del_req);
    assert(del_resp == ":1\r\n");

    // GET foo should be null after delete
    std::string get_req = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    std::string get_resp = send(get_req);
    assert(get_resp == "_\r\n");

    // Replay the AOF – it will re-apply SET then DEL
    slabcache::aof_logger.replay();

    // After replay, GET foo should still be null
    std::string get2_resp = send(get_req);
    assert(get2_resp == "_\r\n");

    slabcache::aof_logger.stop();
    slabcache::stop_ttl_reaper();
    std::remove(aof_path.c_str());

    std::printf("AOF replay test passed\n");
    return 0;
}
