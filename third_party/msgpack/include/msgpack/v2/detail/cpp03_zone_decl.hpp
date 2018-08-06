//
// MessagePack for C++ memory pool
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_CPP03_ZONE_DECL_HPP
#define MSGPACK_V2_CPP03_ZONE_DECL_HPP

#include "msgpack/v1/detail/cpp03_zone_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

using v1::zone;

using v1::aligned_size;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V2_CPP03_ZONE_DECL_HPP
