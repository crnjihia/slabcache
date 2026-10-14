#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "arena_allocator.hpp"
#include "store.hpp"

namespace slabcache {

enum class RespType {
    SimpleString,
    Error,
    Integer,
    BulkString,
    BlobString,
    Null,
    Array,
    Map,
};

struct RespValue {
    RespType type = RespType::Null;
    std::string str{};
    long long integer = 0;
    std::vector<RespValue> array{};
    std::vector<std::pair<RespValue, RespValue>> map{};

    RespValue() = default;
    explicit RespValue(RespType t) : type(t) {}
    RespValue(RespType t, std::string s) : type(t), str(std::move(s)) {}
    RespValue(RespType t, std::string s, long long i) : type(t), str(std::move(s)), integer(i) {}
};

std::pair<RespValue, size_t> parse_resp(const std::string& data, size_t offset = 0);
std::string serialize_resp(const RespValue& val);
RespValue process_request(const RespValue& request);
std::string handle_request(const std::string& raw);

extern Store global_store;

}  // namespace slabcache
