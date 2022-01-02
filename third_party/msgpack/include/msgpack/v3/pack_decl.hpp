//
// MessagePack for C++ serializing routine
//
// Copyright (C) 2008-2018 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_PACK_DECL_HPP
#define MSGPACK_V3_PACK_DECL_HPP

#include "msgpack/v2/pack_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

using v2::packer;

using v2::pack;

#if MSGPACK_ENDIAN_LITTLE_BYTE

using v2::take8_8;

using v2::take8_16;

using v2::take8_32;

using v2::take8_64;

#elif MSGPACK_ENDIAN_BIG_BYTE

using v2::take8_8;

using v2::take8_16;

using v2::take8_32;

using v2::take8_64;

#else
#error msgpack-c supports only big endian and little endian
#endif

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V3_PACK_DECL_HPP
