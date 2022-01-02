//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_CPP03_DEFINE_MAP_DECL_HPP
#define MSGPACK_V2_CPP03_DEFINE_MAP_DECL_HPP

#include "msgpack/v1/adaptor/detail/cpp03_define_map_decl.hpp"

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond
namespace type {

using v1::type::define_map;

using v1::type::make_define_map;

}  // namespace type
/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond
}  // namespace msgpack

#endif // MSGPACK_V2_CPP03_DEFINE_MAP_DECL_HPP
