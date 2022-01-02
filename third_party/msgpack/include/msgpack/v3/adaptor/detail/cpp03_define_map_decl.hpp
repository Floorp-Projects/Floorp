//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_CPP03_DEFINE_MAP_DECL_HPP
#define MSGPACK_V3_CPP03_DEFINE_MAP_DECL_HPP

#include "msgpack/v2/adaptor/detail/cpp03_define_map_decl.hpp"

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond
namespace type {

using v2::type::define_map;

using v2::type::make_define_map;

}  // namespace type
/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond
}  // namespace msgpack

#endif // MSGPACK_V3_CPP03_DEFINE_MAP_DECL_HPP
