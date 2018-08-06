//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP
#define MSGPACK_V3_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP

#if defined(MSGPACK_USE_BOOST)

#include "msgpack/v2/adaptor/boost/msgpack_variant_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

namespace type {

using v2::type::basic_variant;
using v2::type::variant;
using v2::type::variant_ref;

using v2::type::operator<;

using v2::type::operator==;

} // namespace type

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_USE_BOOST
#endif // MSGPACK_V3_TYPE_BOOST_MSGPACK_VARIANT_DECL_HPP
