//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2018 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_VECTOR_BYTE_HPP
#define MSGPACK_V1_TYPE_VECTOR_BYTE_HPP

#if __cplusplus >= 201703

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#include <vector>
#include <cstring>
#include <cstddef>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

template <typename Alloc>
struct convert<std::vector<std::byte, Alloc> > {
    msgpack::object const& operator()(msgpack::object const& o, std::vector<std::byte, Alloc>& v) const {
        switch (o.type) {
        case msgpack::type::BIN:
            v.resize(o.via.bin.size);
            if (o.via.bin.size != 0) {
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif // (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
                std::memcpy(&v.front(), o.via.bin.ptr, o.via.bin.size);
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif // (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
            }
            break;
        case msgpack::type::STR:
            v.resize(o.via.str.size);
            if (o.via.str.size != 0) {
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif // (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
                std::memcpy(&v.front(), o.via.str.ptr, o.via.str.size);
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif // (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
            }
            break;
        default:
            throw msgpack::type_error();
            break;
        }
        return o;
    }
};

template <typename Alloc>
struct pack<std::vector<std::byte, Alloc> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::vector<std::byte, Alloc>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.pack_bin(size);
        if (size != 0) {
            o.pack_bin_body(reinterpret_cast<char const*>(&v.front()), size);
        }

        return o;
    }
};

template <typename Alloc>
struct object<std::vector<std::byte, Alloc> > {
    void operator()(msgpack::object& o, const std::vector<std::byte, Alloc>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.type = msgpack::type::BIN;
        if (size != 0) {
            o.via.bin.ptr = reinterpret_cast<char const*>(&v.front());
        }
        o.via.bin.size = size;
    }
};

template <typename Alloc>
struct object_with_zone<std::vector<std::byte, Alloc> > {
    void operator()(msgpack::object::with_zone& o, const std::vector<std::byte, Alloc>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.type = msgpack::type::BIN;
        o.via.bin.size = size;
        if (size != 0) {
            char* ptr = static_cast<char*>(o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
            o.via.bin.ptr = ptr;
            std::memcpy(ptr, &v.front(), size);
        }
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // __cplusplus >= 201703

#endif // MSGPACK_V1_TYPE_VECTOR_BYTE_HPP
