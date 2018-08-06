//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_CPP03_DEFINE_ARRAY_DECL_HPP
#define MSGPACK_V3_CPP03_DEFINE_ARRAY_DECL_HPP

#include "msgpack/v2/adaptor/detail/cpp03_define_array_decl.hpp"

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond
namespace type {

using v2::type::define_array;

using v2::type::make_define_array;

}  // namespace type
/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond
}  // namespace msgpack

#endif // MSGPACK_V3_CPP03_DEFINE_ARRAY_DECL_HPP
