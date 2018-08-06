//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_CPP03_MSGPACK_TUPLE_DECL_HPP
#define MSGPACK_V3_CPP03_MSGPACK_TUPLE_DECL_HPP

#include "msgpack/v2/adaptor/detail/cpp03_msgpack_tuple_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

namespace type {

using v2::type::tuple;

using v2::type::tuple_element;

using v2::type::const_tuple_element;

using v2::type::tuple_type;

using v2::type::get;

using v2::type::make_tuple;

}  // namespace type

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V3_CPP03_MSGPACK_TUPLE_DECL_HPP
