#include "resp3.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "aof.hpp"
#include "arena_allocator.hpp"
#include "frag_metrics.hpp"
#include "store.hpp"

namespace slabcache {

Store global_store;

static std::string read_line(const std::string& data, size_t& offset) {
    size_t pos = data.find("\r\n", offset);
    if (pos == std::string::npos) {
        throw std::runtime_error("Incomplete RESP data");
    }
    std::string line = data.substr(offset, pos - offset);
    offset = pos + 2;
    return line;
}

static RespValue parse_simple(const std::string& data, size_t& offset, RespType type) {
    RespValue v;
    v.type = type;
    v.str = read_line(data, offset);
    return v;
}

static RespValue parse_integer(const std::string& data, size_t& offset) {
    RespValue v;
    v.type = RespType::Integer;
    std::string line = read_line(data, offset);
    v.integer = std::stoll(line);
    return v;
}

static RespValue parse_bulk(const std::string& data, size_t& offset, RespType type) {
    RespValue v;
    v.type = type;
    std::string len_str = read_line(data, offset);
    long len = std::stol(len_str);
    if (len == -1) {
        v.type = RespType::Null;
        return v;
    }
    if (len < 0) throw std::runtime_error("Invalid bulk length");
    if (static_cast<size_t>(len) + 2 > data.size() - offset) {
        throw std::runtime_error("Incomplete bulk data");
    }
    v.str = data.substr(offset, static_cast<size_t>(len));
    offset += static_cast<size_t>(len);
    if (data.substr(offset, 2) != "\r\n") {
        throw std::runtime_error("Missing CRLF after bulk payload");
    }
    offset += 2;
    return v;
}

std::pair<RespValue, size_t> parse_resp(const std::string& data, size_t offset) {
    if (offset >= data.size()) throw std::runtime_error("Empty RESP data");
    char prefix = data[offset++];
    RespValue v;
    switch (prefix) {
        case '+':
            v = parse_simple(data, offset, RespType::SimpleString);
            break;
        case '-':
            v = parse_simple(data, offset, RespType::Error);
            break;
        case ':':
            v = parse_integer(data, offset);
            break;
        case '$':
            v = parse_bulk(data, offset, RespType::BulkString);
            break;
        case '`':
            v = parse_bulk(data, offset, RespType::BlobString);
            break;
        case '_': {
            read_line(data, offset);
            v.type = RespType::Null;
            break;
        }
        case '*': {
            v.type = RespType::Array;
            std::string count_str = read_line(data, offset);
            int count = std::stoi(count_str);
            if (count < 0) {
                v.type = RespType::Null;
                break;
            }
            v.array.reserve(count);
            for (int i = 0; i < count; ++i) {
                auto [elem, new_off] = parse_resp(data, offset);
                v.array.push_back(std::move(elem));
                offset = new_off;
            }
            break;
        }
        case '%': {
            v.type = RespType::Map;
            std::string count_str = read_line(data, offset);
            int count = std::stoi(count_str);
            v.map.reserve(count);
            for (int i = 0; i < count; ++i) {
                auto [key, k_off] = parse_resp(data, offset);
                offset = k_off;
                auto [val, v_off] = parse_resp(data, offset);
                offset = v_off;
                v.map.emplace_back(std::move(key), std::move(val));
            }
            break;
        }
        default:
            throw std::runtime_error(std::string("Unknown RESP prefix: ") + prefix);
    }
    return {v, offset};
}

std::string serialize_resp(const RespValue& val) {
    switch (val.type) {
        case RespType::SimpleString:
            return "+" + val.str + "\r\n";
        case RespType::Error:
            return "-" + val.str + "\r\n";
        case RespType::Integer:
            return ":" + std::to_string(val.integer) + "\r\n";
        case RespType::BulkString:
        case RespType::BlobString:
            return "$" + std::to_string(val.str.size()) + "\r\n" + val.str + "\r\n";
        case RespType::Null:
            return "_\r\n";  // RESP3 Null
        case RespType::Array: {
            std::string out = "*" + std::to_string(val.array.size()) + "\r\n";
            for (const auto& elem : val.array) {
                out += serialize_resp(elem);
            }
            return out;
        }
        case RespType::Map: {
            std::string out = "%" + std::to_string(val.map.size()) + "\r\n";
            for (const auto& pair : val.map) {
                out += serialize_resp(pair.first);
                out += serialize_resp(pair.second);
            }
            return out;
        }
    }
    return "_\r\n";
}

RespValue process_request(const RespValue& request) {
    if (request.type != RespType::Array) {
        return RespValue{RespType::Error, "ERR expected array request"};
    }
    if (request.array.empty()) {
        return RespValue{RespType::Error, "ERR empty request"};
    }

    const RespValue& cmd_val = request.array[0];
    if (cmd_val.type != RespType::BulkString && cmd_val.type != RespType::SimpleString) {
        return RespValue{RespType::Error, "ERR command must be bulk string"};
    }
    std::string cmd = cmd_val.str;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    std::vector<std::string> args;
    args.reserve(request.array.size() - 1);
    for (size_t i = 1; i < request.array.size(); ++i) {
        args.push_back(request.array[i].str);
    }

    if (cmd == "HELLO") {
        // RESP3 HELLO negotiation for redis-cli -3
        RespValue resp;
        resp.type = RespType::Map;
        resp.map.push_back({RespValue{RespType::SimpleString, "server"},
                            RespValue{RespType::SimpleString, "slabcache"}});
        resp.map.push_back({RespValue{RespType::SimpleString, "version"},
                            RespValue{RespType::SimpleString, "1.0.0"}});
        resp.map.push_back(
            {RespValue{RespType::SimpleString, "proto"}, RespValue{RespType::Integer, "", 3}});
        resp.map.push_back({RespValue{RespType::SimpleString, "mode"},
                            RespValue{RespType::SimpleString, "standalone"}});
        resp.map.push_back({RespValue{RespType::SimpleString, "role"},
                            RespValue{RespType::SimpleString, "master"}});
        return resp;
    }

    if (cmd == "PING") {
        if (!args.empty()) {
            return RespValue{RespType::BulkString, args[0]};
        }
        return RespValue{RespType::SimpleString, "PONG"};
    }

    if (cmd == "QUIT") {
        return RespValue{RespType::SimpleString, "OK"};
    }

    if (cmd == "SET") {
        if (args.size() < 2)
            return RespValue{RespType::Error, "ERR wrong number of arguments for 'SET'"};
        global_store.set(args[0], args[1]);
        return RespValue{RespType::SimpleString, "OK"};
    }

    if (cmd == "GET") {
        if (args.empty())
            return RespValue{RespType::Error, "ERR wrong number of arguments for 'GET'"};
        auto opt = global_store.get(args[0]);
        if (!opt) return RespValue{RespType::Null, ""};
        return RespValue{RespType::BulkString, *opt};
    }

    if (cmd == "DEL") {
        if (args.empty())
            return RespValue{RespType::Error, "ERR wrong number of arguments for 'DEL'"};
        bool removed = global_store.del(args[0]);
        return RespValue{RespType::Integer, "", removed ? 1 : 0};
    }

    if (cmd == "EXPIRE") {
        if (args.size() < 2)
            return RespValue{RespType::Error, "ERR wrong number of arguments for 'EXPIRE'"};
        long secs = std::stol(args[1]);
        bool ok = global_store.expire(args[0], std::chrono::seconds(secs));
        return RespValue{RespType::Integer, "", ok ? 1 : 0};
    }

    if (cmd == "TTL") {
        if (args.empty())
            return RespValue{RespType::Error, "ERR wrong number of arguments for 'TTL'"};
        auto maybe = global_store.ttl(args[0]);
        if (!maybe) {
            return RespValue{RespType::Integer, "", -2};  // Key does not exist
        }
        if (!maybe->has_value()) {
            return RespValue{RespType::Integer, "", -1};  // Key exists with no TTL
        }
        return RespValue{RespType::Integer, "", maybe->value().count()};
    }

    if (cmd == "STATS") {
        Metrics m = get_latest_metrics();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "ext=%.2f%% int=%.2f%%", m.external_frag_percent,
                      m.internal_frag_percent);
        return RespValue{RespType::SimpleString, std::string(buf)};
    }

    if (cmd == "INFO") {
        return RespValue{RespType::BulkString,
                         "# Server\r\nversion:1.0.0\r\nallocator:hybrid-slab\r\n"};
    }

    return RespValue{RespType::Error, "ERR unknown command '" + cmd + "'"};
}

std::string handle_request(const std::string& raw) {
    // Request-scoped scratch memory via Arena
    thread_local Arena req_arena(64 * 1024);
    req_arena.reset();

    start_ttl_reaper();

    auto [val, off] = parse_resp(raw);
    (void)off;

    std::string cmd_name;
    if (val.type == RespType::Array && !val.array.empty()) {
        const RespValue& cv = val.array[0];
        if (cv.type == RespType::BulkString || cv.type == RespType::SimpleString) {
            cmd_name = cv.str;
            std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::toupper);
        }
    }

    bool is_mutating = (cmd_name == "SET" || cmd_name == "DEL" || cmd_name == "EXPIRE");
    if (is_mutating) {
        aof_logger.append(raw);
    }

    RespValue resp = process_request(val);
    return serialize_resp(resp);
}

}  // namespace slabcache
