//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP
#define MSGPACK_V2_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP

#if defined(MSGPACK_USE_BOOST)

#include "msgpack/v1/adaptor/boost/msgpack_variant_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

namespace type {

using v1::type::basic_variant;
using v1::type::variant;
using v1::type::variant_ref;

using v1::type::operator<;

using v1::type::operator==;

} // namespace type

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_USE_BOOST
#endif // MSGPACK_V2_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP
