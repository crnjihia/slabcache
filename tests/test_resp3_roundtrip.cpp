#include <cassert>
#include <cstdio>
#include <string>

#include "resp3.hpp"
#include "store.hpp"
#include "ttl.hpp"

int main() {
    slabcache::start_ttl_reaper();

    auto send = [&](const std::string& req) { return slabcache::handle_request(req); };

    // Test HELLO 3 negotiation (returns RESP3 Map %)
    std::string hello_req = "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n";
    std::string hello_resp = send(hello_req);
    assert(hello_resp.rfind("%", 0) == 0 && "HELLO 3 must return RESP3 map");

    // Test SET foo bar
    std::string set_req = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    std::string set_resp = send(set_req);
    assert(set_resp == "+OK\r\n");

    // Test GET foo
    std::string get_req = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    std::string get_resp = send(get_req);
    assert(get_resp == "$3\r\nbar\r\n");

    // Test DEL foo
    std::string del_req = "*2\r\n$3\r\nDEL\r\n$3\r\nfoo\r\n";
    std::string del_resp = send(del_req);
    assert(del_resp == ":1\r\n");

    // GET after delete should be RESP3 Null ("_\r\n")
    std::string get2_resp = send(get_req);
    assert(get2_resp == "_\r\n");

    // Test EXPIRE and TTL
    send(set_req);
    std::string expire_req = "*3\r\n$6\r\nEXPIRE\r\n$3\r\nfoo\r\n$1\r\n5\r\n";
    std::string expire_resp = send(expire_req);
    assert(expire_resp == ":1\r\n");

    std::string ttl_req = "*2\r\n$3\r\nTTL\r\n$3\r\nfoo\r\n";
    std::string ttl_resp = send(ttl_req);
    assert(ttl_resp.rfind(":", 0) == 0);
    long ttl_val = std::stol(ttl_resp.substr(1, ttl_resp.find("\r\n") - 1));
    assert(ttl_val >= 1 && ttl_val <= 5);

    // Test PING
    std::string ping_req = "*1\r\n$4\r\nPING\r\n";
    std::string ping_resp = send(ping_req);
    assert(ping_resp == "+PONG\r\n");

    // Test QUIT
    std::string quit_req = "*1\r\n$4\r\nQUIT\r\n";
    std::string quit_resp = send(quit_req);
    assert(quit_resp == "+OK\r\n");

    slabcache::stop_ttl_reaper();
    std::printf("test_resp3_roundtrip passed\n");
    return 0;
}
