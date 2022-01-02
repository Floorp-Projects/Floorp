//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2018 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_UNPACK_HPP
#define MSGPACK_V3_UNPACK_HPP

#include "msgpack/unpack_decl.hpp"
#include "msgpack/parse.hpp"
#include "msgpack/create_object_visitor.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

inline msgpack::object_handle unpack(
    const char* data, std::size_t len, std::size_t& off, bool& referenced,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit
)
{
    msgpack::object obj;
    msgpack::unique_ptr<msgpack::zone> z(new msgpack::zone);
    referenced = false;
    parse_return ret = detail::unpack_imp(
        data, len, off, *z, obj, referenced, f, user_data, limit);

    switch(ret) {
    case msgpack::PARSE_SUCCESS:
        return msgpack::object_handle(obj, msgpack::move(z));
    case msgpack::PARSE_EXTRA_BYTES:
        return msgpack::object_handle(obj, msgpack::move(z));
    default:
        break;
    }
    return msgpack::object_handle();
}

inline msgpack::object_handle unpack(
    const char* data, std::size_t len, std::size_t& off,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    bool referenced;
    return msgpack::v3::unpack(data, len, off, referenced, f, user_data, limit);
}

inline msgpack::object_handle unpack(
    const char* data, std::size_t len, bool& referenced,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    std::size_t off = 0;
    return msgpack::v3::unpack(data, len, off, referenced, f, user_data, limit);
}

inline msgpack::object_handle unpack(
    const char* data, std::size_t len,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    bool referenced;
    std::size_t off = 0;
    return msgpack::v3::unpack(data, len, off, referenced, f, user_data, limit);
}

inline void unpack(
    msgpack::object_handle& result,
    const char* data, std::size_t len, std::size_t& off, bool& referenced,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    msgpack::object obj;
    msgpack::unique_ptr<msgpack::zone> z(new msgpack::zone);
    referenced = false;
    parse_return ret = detail::unpack_imp(
        data, len, off, *z, obj, referenced, f, user_data, limit);

    switch(ret) {
    case msgpack::PARSE_SUCCESS:
        result.set(obj);
        result.zone() = msgpack::move(z);
        return;
    case msgpack::PARSE_EXTRA_BYTES:
        result.set(obj);
        result.zone() = msgpack::move(z);
        return;
    default:
        return;
    }
}

inline void unpack(
    msgpack::object_handle& result,
    const char* data, std::size_t len, std::size_t& off,
    msgpack::v3::unpack_reference_func f, void* user_data,
            msgpack::unpack_limit const& limit)
{
    bool referenced;
    msgpack::v3::unpack(result, data, len, off, referenced, f, user_data, limit);
}

inline void unpack(
    msgpack::object_handle& result,
    const char* data, std::size_t len, bool& referenced,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    std::size_t off = 0;
    msgpack::v3::unpack(result, data, len, off, referenced, f, user_data, limit);
}

inline void unpack(
    msgpack::object_handle& result,
    const char* data, std::size_t len,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    bool referenced;
    std::size_t off = 0;
    msgpack::v3::unpack(result, data, len, off, referenced, f, user_data, limit);
}


inline msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len, std::size_t& off, bool& referenced,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    msgpack::object obj;
    referenced = false;
    parse_return ret = detail::unpack_imp(
        data, len, off, z, obj, referenced, f, user_data, limit);

    switch(ret) {
    case msgpack::PARSE_SUCCESS:
        return obj;
    case msgpack::PARSE_EXTRA_BYTES:
        return obj;
    default:
        break;
    }
    return obj;
}

inline msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len, std::size_t& off,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    bool referenced;
    return msgpack::v3::unpack(z, data, len, off, referenced, f, user_data, limit);
}

inline msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len, bool& referenced,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    std::size_t off = 0;
    return msgpack::v3::unpack(z, data, len, off, referenced, f, user_data, limit);
}

inline msgpack::object unpack(
    msgpack::zone& z,
    const char* data, std::size_t len,
    msgpack::unpack_reference_func f, void* user_data,
    msgpack::unpack_limit const& limit)
{
    bool referenced;
    std::size_t off = 0;
    return msgpack::v3::unpack(z, data, len, off, referenced, f, user_data, limit);
}

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack


#endif // MSGPACK_V3_UNPACK_HPP
