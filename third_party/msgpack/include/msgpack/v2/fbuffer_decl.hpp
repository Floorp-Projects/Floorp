//
// MessagePack for C++ FILE* buffer adaptor
//
// Copyright (C) 2013-2016 Vladimir Volodko and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_FBUFFER_DECL_HPP
#define MSGPACK_V2_FBUFFER_DECL_HPP

#include "msgpack/v1/fbuffer_decl.hpp"

#include <cstdio>
#include <stdexcept>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

using v1::fbuffer;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V2_FBUFFER_DECL_HPP
