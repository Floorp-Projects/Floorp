//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_TYPE_FIXINT_DECL_HPP
#define MSGPACK_V3_TYPE_FIXINT_DECL_HPP

#include "msgpack/v2/adaptor/fixint_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

namespace type {

using v2::type::fix_int;

using v2::type::fix_uint8;
using v2::type::fix_uint16;
using v2::type::fix_uint32;
using v2::type::fix_uint64;

using v2::type::fix_int8;
using v2::type::fix_int16;
using v2::type::fix_int32;
using v2::type::fix_int64;

}  // namespace type

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V3_TYPE_FIXINT_DECL_HPP
