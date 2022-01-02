//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_UNPACK_EXCEPTION_HPP
#define MSGPACK_V1_UNPACK_EXCEPTION_HPP

#include "msgpack/versioning.hpp"

#include <string>
#include <stdexcept>


namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

struct unpack_error : public std::runtime_error {
    explicit unpack_error(const std::string& msg)
        :std::runtime_error(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    explicit unpack_error(const char* msg):
        std::runtime_error(msg) {}
#endif // !defined(MSGPACK_USE_CPP03)
};

struct parse_error : public unpack_error {
    explicit parse_error(const std::string& msg)
        :unpack_error(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    explicit parse_error(const char* msg)
        :unpack_error(msg) {}
#endif // !defined(MSGPACK_USE_CPP03)
};

struct insufficient_bytes : public unpack_error {
    explicit insufficient_bytes(const std::string& msg)
        :unpack_error(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    explicit insufficient_bytes(const char* msg)
        :unpack_error(msg) {}
#endif // !defined(MSGPACK_USE_CPP03)
};

struct size_overflow : public unpack_error {
    explicit size_overflow(const std::string& msg)
        :unpack_error(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    explicit size_overflow(const char* msg)
        :unpack_error(msg) {}
#endif
};

struct array_size_overflow : public size_overflow {
    array_size_overflow(const std::string& msg)
        :size_overflow(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    array_size_overflow(const char* msg)
        :size_overflow(msg) {}
#endif
};

struct map_size_overflow : public size_overflow {
    map_size_overflow(const std::string& msg)
        :size_overflow(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    map_size_overflow(const char* msg)
        :size_overflow(msg) {}
#endif
};

struct str_size_overflow : public size_overflow {
    str_size_overflow(const std::string& msg)
        :size_overflow(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    str_size_overflow(const char* msg)
        :size_overflow(msg) {}
#endif
};

struct bin_size_overflow : public size_overflow {
    bin_size_overflow(const std::string& msg)
        :size_overflow(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    bin_size_overflow(const char* msg)
        :size_overflow(msg) {}
#endif
};

struct ext_size_overflow : public size_overflow {
    ext_size_overflow(const std::string& msg)
        :size_overflow(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    ext_size_overflow(const char* msg)
        :size_overflow(msg) {}
#endif
};

struct depth_size_overflow : public size_overflow {
    depth_size_overflow(const std::string& msg)
        :size_overflow(msg) {}
#if !defined(MSGPACK_USE_CPP03)
    depth_size_overflow(const char* msg)
        :size_overflow(msg) {}
#endif
};

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack


#endif // MSGPACK_V1_UNPACK_EXCEPTION_HPP
