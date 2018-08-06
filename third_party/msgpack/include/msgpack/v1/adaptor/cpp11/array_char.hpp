//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2014-2015 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_CPP11_ARRAY_CHAR_HPP
#define MSGPACK_V1_TYPE_CPP11_ARRAY_CHAR_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#include <array>
#include <cstring>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

template <std::size_t N>
struct convert<std::array<char, N>> {
    msgpack::object const& operator()(msgpack::object const& o, std::array<char, N>& v) const {
        switch (o.type) {
        case msgpack::type::BIN:
            if(o.via.bin.size > N) { throw msgpack::type_error(); }
            std::memcpy(v.data(), o.via.bin.ptr, o.via.bin.size);
            break;
        case msgpack::type::STR:
            if(o.via.str.size > N) { throw msgpack::type_error(); }
            std::memcpy(v.data(), o.via.str.ptr, N);
            break;
        default:
            throw msgpack::type_error();
            break;
        }
        return o;
    }
};

template <>
struct convert<std::array<char, 0>> {
    msgpack::object const& operator()(msgpack::object const& o, std::array<char, 0>&) const {
        return o;
    }
};

template <std::size_t N>
struct pack<std::array<char, N>> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::array<char, N>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.pack_bin(size);
        o.pack_bin_body(v.data(), size);

        return o;
    }
};

template <std::size_t N>
struct object<std::array<char, N>> {
    void operator()(msgpack::object& o, const std::array<char, N>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.type = msgpack::type::BIN;
        o.via.bin.ptr = v.data();
        o.via.bin.size = size;
    }
};

template <std::size_t N>
struct object_with_zone<std::array<char, N>> {
    void operator()(msgpack::object::with_zone& o, const std::array<char, N>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.type = msgpack::type::BIN;
        char* ptr = static_cast<char*>(o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
        o.via.bin.ptr = ptr;
        o.via.bin.size = size;
        std::memcpy(ptr, v.data(), size);
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V1_TYPE_CPP11_ARRAY_CHAR_HPP
