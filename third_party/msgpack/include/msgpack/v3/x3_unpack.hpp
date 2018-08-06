//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2018 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_X3_UNPACK_HPP
#define MSGPACK_V3_X3_UNPACK_HPP

#if defined(MSGPACK_USE_X3_PARSE)

#include <boost/version.hpp>

#if BOOST_VERSION >= 106100

#include "msgpack/versioning.hpp"
#include "msgpack/create_object_visitor.hpp"
#include "msgpack/x3_unpack_decl.hpp"
#include "msgpack/x3_parse.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond


template <typename Iterator>
inline msgpack::object_handle unpack(
    Iterator&& begin, Iterator&& end,
    bool& referenced,
    unpack_reference_func f, void* user_data,
    unpack_limit const& limit)
{
    msgpack::object obj;
    msgpack::unique_ptr<msgpack::zone> z(new msgpack::zone);
    referenced = false;
    detail::unpack_imp(
        std::forward<Iterator>(begin), std::forward<Iterator>(end), *z, obj, referenced, f, user_data, limit);
    return msgpack::object_handle(obj, msgpack::move(z));
}

template <typename Iterator>
inline msgpack::object_handle unpack(
    Iterator&& begin, Iterator&& end,
    unpack_reference_func f, void* user_data,
    unpack_limit const& limit)
{
    bool referenced;
    return unpack(std::forward<Iterator>(begin), std::forward<Iterator>(end), referenced, f, user_data, limit);
}

template <typename Iterator>
inline msgpack::object unpack(
    msgpack::zone& z,
    Iterator&& begin, Iterator&& end,
    bool& referenced,
    unpack_reference_func f, void* user_data,
    unpack_limit const& limit)
{
    msgpack::object obj;
    referenced = false;
    detail::unpack_imp(
        std::forward<Iterator>(begin), std::forward<Iterator>(end), z, obj, referenced, f, user_data, limit);
    return obj;
}

template <typename Iterator>
inline msgpack::object unpack(
    msgpack::zone& z,
    Iterator&& begin, Iterator&& end,
    unpack_reference_func f, void* user_data,
    unpack_limit const& limit)
{
    bool referenced;
    return unpack(
        z, std::forward<Iterator>(begin), std::forward<Iterator>(end), referenced, f, user_data, limit);
}

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#else  // BOOST_VERSION >= 106100

#error Boost 1.61.0 or later is required to use x3 parse

#endif // BOOST_VERSION >= 106100

#endif // defined(MSGPACK_USE_X3_PARSE)

#endif // MSGPACK_V3_X3_UNPACK_HPP
