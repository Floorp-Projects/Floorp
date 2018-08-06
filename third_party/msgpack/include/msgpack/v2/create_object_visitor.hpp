//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2017 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_CREATE_OBJECT_VISITOR_HPP
#define MSGPACK_V2_CREATE_OBJECT_VISITOR_HPP

#include "msgpack/unpack_decl.hpp"
#include "msgpack/unpack_exception.hpp"
#include "msgpack/v2/create_object_visitor_decl.hpp"
#include "msgpack/v2/null_visitor.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

namespace detail {

class create_object_visitor : public msgpack::v2::null_visitor {
public:
    create_object_visitor(unpack_reference_func f, void* user_data, unpack_limit const& limit)
        :m_func(f), m_user_data(user_data), m_limit(limit) {
        m_stack.reserve(MSGPACK_EMBED_STACK_SIZE);
        m_stack.push_back(&m_obj);
    }

#if !defined(MSGPACK_USE_CPP03)
    create_object_visitor(create_object_visitor&& other)
        :m_func(other.m_func),
         m_user_data(other.m_user_data),
         m_limit(std::move(other.m_limit)),
         m_stack(std::move(other.m_stack)),
         m_zone(other.m_zone),
         m_referenced(other.m_referenced) {
        other.m_zone = MSGPACK_NULLPTR;
        m_stack[0] = &m_obj;
    }
    create_object_visitor& operator=(create_object_visitor&& other) {
        this->~create_object_visitor();
        new (this) create_object_visitor(std::move(other));
        return *this;
    }
#endif // !defined(MSGPACK_USE_CPP03)

    void init() {
        m_stack.resize(1);
        m_obj = msgpack::object();
        m_stack[0] = &m_obj;
    }
    msgpack::object const& data() const
    {
        return m_obj;
    }
    msgpack::zone const& zone() const { return *m_zone; }
    msgpack::zone& zone() { return *m_zone; }
    void set_zone(msgpack::zone& zone) { m_zone = &zone; }
    bool referenced() const { return m_referenced; }
    void set_referenced(bool referenced) { m_referenced = referenced; }
    // visit functions
    bool visit_nil() {
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::NIL;
        return true;
    }
    bool visit_boolean(bool v) {
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::BOOLEAN;
        obj->via.boolean = v;
        return true;
    }
    bool visit_positive_integer(uint64_t v) {
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::POSITIVE_INTEGER;
        obj->via.u64 = v;
        return true;
    }
    bool visit_negative_integer(int64_t v) {
        msgpack::object* obj = m_stack.back();
        if(v >= 0) {
            obj->type = msgpack::type::POSITIVE_INTEGER;
            obj->via.u64 = v;
        }
        else {
            obj->type = msgpack::type::NEGATIVE_INTEGER;
            obj->via.i64 = v;
        }
        return true;
    }
    bool visit_float32(float v) {
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::FLOAT32;
        obj->via.f64 = v;
        return true;
    }
    bool visit_float64(double v) {
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::FLOAT64;
        obj->via.f64 = v;
        return true;
    }
    bool visit_str(const char* v, uint32_t size) {
        if (size > m_limit.str()) throw msgpack::str_size_overflow("str size overflow");
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::STR;
        if (m_func && m_func(obj->type, size, m_user_data)) {
            obj->via.str.ptr = v;
            set_referenced(true);
        }
        else {
            char* tmp = static_cast<char*>(zone().allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
            std::memcpy(tmp, v, size);
            obj->via.str.ptr = tmp;
        }
        obj->via.str.size = size;
        return true;
    }
    bool visit_bin(const char* v, uint32_t size) {
        if (size > m_limit.bin()) throw msgpack::bin_size_overflow("bin size overflow");
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::BIN;
        if (m_func && m_func(obj->type, size, m_user_data)) {
            obj->via.bin.ptr = v;
            set_referenced(true);
        }
        else {
            char* tmp = static_cast<char*>(zone().allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
            std::memcpy(tmp, v, size);
            obj->via.bin.ptr = tmp;
        }
        obj->via.bin.size = size;
        return true;
    }
    bool visit_ext(const char* v, uint32_t size) {
        if (size > m_limit.ext()) throw msgpack::ext_size_overflow("ext size overflow");
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::EXT;
        if (m_func && m_func(obj->type, size, m_user_data)) {
            obj->via.ext.ptr = v;
            set_referenced(true);
        }
        else {
            char* tmp = static_cast<char*>(zone().allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
            std::memcpy(tmp, v, size);
            obj->via.ext.ptr = tmp;
        }
        obj->via.ext.size = static_cast<uint32_t>(size - 1);
        return true;
    }
    bool start_array(uint32_t num_elements) {
        if (num_elements > m_limit.array()) throw msgpack::array_size_overflow("array size overflow");
        if (m_stack.size() > m_limit.depth()) throw msgpack::depth_size_overflow("depth size overflow");
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::ARRAY;
        obj->via.array.size = num_elements;
        if (num_elements == 0) {
            obj->via.array.ptr = MSGPACK_NULLPTR;
        }
        else {
            size_t size = num_elements*sizeof(msgpack::object);
            if (size / sizeof(msgpack::object) != num_elements) {
                throw msgpack::array_size_overflow("array size overflow");
            }
            obj->via.array.ptr =
                static_cast<msgpack::object*>(m_zone->allocate_align(size, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        }
        m_stack.push_back(obj->via.array.ptr);
        return true;
    }
    bool start_array_item() {
        return true;
    }
    bool end_array_item() {
        ++m_stack.back();
        return true;
    }
    bool end_array() {
        m_stack.pop_back();
        return true;
    }
    bool start_map(uint32_t num_kv_pairs) {
        if (num_kv_pairs > m_limit.map()) throw msgpack::map_size_overflow("map size overflow");
        if (m_stack.size() > m_limit.depth()) throw msgpack::depth_size_overflow("depth size overflow");
        msgpack::object* obj = m_stack.back();
        obj->type = msgpack::type::MAP;
        obj->via.map.size = num_kv_pairs;
        if (num_kv_pairs == 0) {
            obj->via.map.ptr = MSGPACK_NULLPTR;
        }
        else {
            size_t size = num_kv_pairs*sizeof(msgpack::object_kv);
            if (size / sizeof(msgpack::object_kv) != num_kv_pairs) {
                throw msgpack::map_size_overflow("map size overflow");
            }
            obj->via.map.ptr =
                static_cast<msgpack::object_kv*>(m_zone->allocate_align(size, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        }
        m_stack.push_back(reinterpret_cast<msgpack::object*>(obj->via.map.ptr));
        return true;
    }
    bool start_map_key() {
        return true;
    }
    bool end_map_key() {
        ++m_stack.back();
        return true;
    }
    bool start_map_value() {
        return true;
    }
    bool end_map_value() {
        ++m_stack.back();
        return true;
    }
    bool end_map() {
        m_stack.pop_back();
        return true;
    }
    void parse_error(size_t /*parsed_offset*/, size_t /*error_offset*/) {
        throw msgpack::parse_error("parse error");
    }
    void insufficient_bytes(size_t /*parsed_offset*/, size_t /*error_offset*/) {
        throw msgpack::insufficient_bytes("insufficient bytes");
    }
private:
public:
    unpack_reference_func m_func;
    void* m_user_data;
    unpack_limit m_limit;
    msgpack::object m_obj;
    std::vector<msgpack::object*> m_stack;
    msgpack::zone* m_zone;
    bool m_referenced;
};

} // detail

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V2_CREATE_OBJECT_VISITOR_HPP
