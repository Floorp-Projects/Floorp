//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2017 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_V1_TYPE_CPP11_CHRONO_HPP
#define MSGPACK_V1_TYPE_CPP11_CHRONO_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#include <chrono>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

template <>
struct as<std::chrono::system_clock::time_point> {
    typename std::chrono::system_clock::time_point operator()(msgpack::object const& o) const {
        if(o.type != msgpack::type::EXT) { throw msgpack::type_error(); }
        if(o.via.ext.type() != -1) { throw msgpack::type_error(); }
        std::chrono::system_clock::time_point tp;
        switch(o.via.ext.size) {
        case 4: {
            uint32_t sec;
            _msgpack_load32(uint32_t, o.via.ext.data(), &sec);
            tp += std::chrono::seconds(sec);
        } break;
        case 8: {
            uint64_t value;
            _msgpack_load64(uint64_t, o.via.ext.data(), &value);
            uint32_t nanosec = static_cast<uint32_t>(value >> 34);
            uint64_t sec = value & 0x00000003ffffffffL;
            tp += std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(nanosec));
            tp += std::chrono::seconds(sec);
        } break;
        case 12: {
            uint32_t nanosec;
            _msgpack_load32(uint32_t, o.via.ext.data(), &nanosec);
            int64_t sec;
            _msgpack_load64(int64_t, o.via.ext.data() + 4, &sec);
            tp += std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(nanosec));
            tp += std::chrono::seconds(sec);
        } break;
        default:
            throw msgpack::type_error();
        }
        return tp;
    }
};

template <>
struct convert<std::chrono::system_clock::time_point> {
    msgpack::object const& operator()(msgpack::object const& o, std::chrono::system_clock::time_point& v) const {
        if(o.type != msgpack::type::EXT) { throw msgpack::type_error(); }
        if(o.via.ext.type() != -1) { throw msgpack::type_error(); }
        std::chrono::system_clock::time_point tp;
        switch(o.via.ext.size) {
        case 4: {
            uint32_t sec;
            _msgpack_load32(uint32_t, o.via.ext.data(), &sec);
            tp += std::chrono::seconds(sec);
            v = tp;
        } break;
        case 8: {
            uint64_t value;
            _msgpack_load64(uint64_t, o.via.ext.data(), &value);
            uint32_t nanosec = static_cast<uint32_t>(value >> 34);
            uint64_t sec = value & 0x00000003ffffffffL;
            tp += std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(nanosec));
            tp += std::chrono::seconds(sec);
            v = tp;
        } break;
        case 12: {
            uint32_t nanosec;
            _msgpack_load32(uint32_t, o.via.ext.data(), &nanosec);
            int64_t sec;
            _msgpack_load64(int64_t, o.via.ext.data() + 4, &sec);
            tp += std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::nanoseconds(nanosec));
            tp += std::chrono::seconds(sec);
            v = tp;
        } break;
        default:
            throw msgpack::type_error();
        }
        return o;
    }
};

template <>
struct pack<std::chrono::system_clock::time_point> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::chrono::system_clock::time_point& v) const {
        int64_t count = static_cast<int64_t>(v.time_since_epoch().count());

        int64_t nano_num =
            std::chrono::system_clock::duration::period::ratio::num *
            (1000000000 / std::chrono::system_clock::duration::period::ratio::den);

        int64_t nanosec = count % (1000000000 / nano_num) * nano_num;
        int64_t sec = 0;
        if (nanosec < 0) {
            nanosec = 1000000000 + nanosec;
            --sec;
        }
        sec += count
            * std::chrono::system_clock::duration::period::ratio::num
            / std::chrono::system_clock::duration::period::ratio::den;
        if ((sec >> 34) == 0) {
            uint64_t data64 = (nanosec << 34) | sec;
            if ((data64 & 0xffffffff00000000L) == 0) {
                // timestamp 32
                o.pack_ext(4, -1);
                uint32_t data32 = static_cast<uint32_t>(data64);
                char buf[4];
                _msgpack_store32(buf, data32);
                o.pack_ext_body(buf, 4);
            }
            else {
                // timestamp 64
                o.pack_ext(8, -1);
                char buf[8];
                _msgpack_store64(buf, data64);
                o.pack_ext_body(buf, 8);
            }
        }
        else {
            // timestamp 96
            o.pack_ext(12, -1);
            char buf[12];
            _msgpack_store32(&buf[0], static_cast<uint32_t>(nanosec));
            _msgpack_store64(&buf[4], sec);
            o.pack_ext_body(buf, 12);
        }
        return o;
    }
};

template <>
struct object_with_zone<std::chrono::system_clock::time_point> {
    void operator()(msgpack::object::with_zone& o, const std::chrono::system_clock::time_point& v) const {
        int64_t count = static_cast<int64_t>(v.time_since_epoch().count());

        int64_t nano_num =
            std::chrono::system_clock::duration::period::ratio::num *
            (1000000000 / std::chrono::system_clock::duration::period::ratio::den);

        int64_t nanosec = count % (1000000000 / nano_num) * nano_num;
        int64_t sec = 0;
        if (nanosec < 0) {
            nanosec = 1000000000 + nanosec;
            --sec;
        }
        sec += count
            * std::chrono::system_clock::duration::period::ratio::num
            / std::chrono::system_clock::duration::period::ratio::den;
        if ((sec >> 34) == 0) {
            uint64_t data64 = (nanosec << 34) | sec;
            if ((data64 & 0xffffffff00000000L) == 0) {
                // timestamp 32
                o.type = msgpack::type::EXT;
                o.via.ext.size = 4;
                char* p = static_cast<char*>(o.zone.allocate_no_align(o.via.ext.size + 1));
                p[0] = -1;
                uint32_t data32 = static_cast<uint32_t>(data64);
                _msgpack_store32(&p[1], data32);
                o.via.ext.ptr = p;
            }
            else {
                // timestamp 64
                o.type = msgpack::type::EXT;
                o.via.ext.size = 8;
                char* p = static_cast<char*>(o.zone.allocate_no_align(o.via.ext.size + 1));
                p[0] = -1;
                _msgpack_store64(&p[1], data64);
                o.via.ext.ptr = p;
            }
        }
        else {
            // timestamp 96
            o.type = msgpack::type::EXT;
            o.via.ext.size = 12;
            char* p = static_cast<char*>(o.zone.allocate_no_align(o.via.ext.size + 1));
            p[0] = -1;
            _msgpack_store32(&p[1], static_cast<uint32_t>(nanosec));
            _msgpack_store64(&p[1 + 4], sec);
            o.via.ext.ptr = p;
        }
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V1_TYPE_CPP11_CHRONO_HPP
