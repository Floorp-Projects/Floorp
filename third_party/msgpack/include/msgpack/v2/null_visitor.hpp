//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2017 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_NULL_VISITOR_HPP
#define MSGPACK_V2_NULL_VISITOR_HPP

#include "msgpack/v2/null_visitor_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

struct null_visitor {
    bool visit_nil() {
        return true;
    }
    bool visit_boolean(bool /*v*/) {
        return true;
    }
    bool visit_positive_integer(uint64_t /*v*/) {
        return true;
    }
    bool visit_negative_integer(int64_t /*v*/) {
        return true;
    }
    bool visit_float32(float /*v*/) {
        return true;
    }
    bool visit_float64(double /*v*/) {
        return true;
    }
    bool visit_str(const char* /*v*/, uint32_t /*size*/) {
        return true;
    }
    bool visit_bin(const char* /*v*/, uint32_t /*size*/) {
        return true;
    }
    bool visit_ext(const char* /*v*/, uint32_t /*size*/) {
        return true;
    }
    bool start_array(uint32_t /*num_elements*/) {
        return true;
    }
    bool start_array_item() {
        return true;
    }
    bool end_array_item() {
        return true;
    }
    bool end_array() {
        return true;
    }
    bool start_map(uint32_t /*num_kv_pairs*/) {
        return true;
    }
    bool start_map_key() {
        return true;
    }
    bool end_map_key() {
        return true;
    }
    bool start_map_value() {
        return true;
    }
    bool end_map_value() {
        return true;
    }
    bool end_map() {
        return true;
    }
    void parse_error(size_t /*parsed_offset*/, size_t /*error_offset*/) {
    }
    void insufficient_bytes(size_t /*parsed_offset*/, size_t /*error_offset*/) {
    }
    bool referenced() const {
        return false;
    }
    void set_referenced(bool /*referenced*/) {
    }
};

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V2_NULL_VISITOR_HPP
