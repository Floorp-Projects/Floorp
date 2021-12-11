//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2018 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_CARRAY_BYTE_HPP
#define MSGPACK_V1_TYPE_CARRAY_BYTE_HPP

#if __cplusplus >= 201703

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#include <cstring>
#include <cstddef>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

template <std::size_t N>
struct convert<std::byte[N]> {
    msgpack::object const& operator()(msgpack::object const& o, std::byte(&v)[N]) const {
        switch (o.type) {
        case msgpack::type::BIN:
            if (o.via.bin.size > N) { throw msgpack::type_error(); }
            std::memcpy(v, o.via.bin.ptr, o.via.bin.size);
            break;
        case msgpack::type::STR:
            if (o.via.str.size > N) { throw msgpack::type_error(); }
            std::memcpy(v, o.via.str.ptr, o.via.str.size);
            if (o.via.str.size < N) v[o.via.str.size] = std::byte{'\0'};
            break;
        default:
            throw msgpack::type_error();
            break;
        }
        return o;
    }
};

template <std::size_t N>
struct pack<std::byte[N]> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::byte(&v)[N]) const {
        std::byte const* p = v;
        uint32_t size = checked_get_container_size(N);
        o.pack_bin(size);
        o.pack_bin_body(reinterpret_cast<char const*>(p), size);
        return o;
    }
};

template <std::size_t N>
struct pack<const std::byte[N]> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::byte(&v)[N]) const {
        std::byte const* p = v;
        uint32_t size = checked_get_container_size(N);
        o.pack_bin(size);
        o.pack_bin_body(reinterpret_cast<char const*>(p), size);
        return o;
    }
};

template <std::size_t N>
struct object_with_zone<std::byte[N]> {
    void operator()(msgpack::object::with_zone& o, const std::byte(&v)[N]) const {
        uint32_t size = checked_get_container_size(N);
        o.type = msgpack::type::BIN;
        char* ptr = static_cast<char*>(o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
        o.via.bin.ptr = ptr;
        o.via.bin.size = size;
        std::memcpy(ptr, v, size);
    }
};

template <std::size_t N>
struct object_with_zone<const std::byte[N]> {
    void operator()(msgpack::object::with_zone& o, const std::byte(&v)[N]) const {
        uint32_t size = checked_get_container_size(N);
        o.type = msgpack::type::BIN;
        char* ptr = static_cast<char*>(o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
        o.via.bin.ptr = ptr;
        o.via.bin.size = size;
        std::memcpy(ptr, v, size);
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // __cplusplus >= 201703

#endif // MSGPACK_V1_TYPE_CARRAY_BYTE_HPP
