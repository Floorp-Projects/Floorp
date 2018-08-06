//
// MessagePack for C++ memory pool
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_CPP03_ZONE_DECL_HPP
#define MSGPACK_V1_CPP03_ZONE_DECL_HPP

#include <cstdlib>
#include <memory>
#include <vector>

#include "msgpack/versioning.hpp"

#ifndef MSGPACK_ZONE_CHUNK_SIZE
#define MSGPACK_ZONE_CHUNK_SIZE 8192
#endif

#ifndef MSGPACK_ZONE_ALIGN
#define MSGPACK_ZONE_ALIGN sizeof(void*)
#endif

#if defined(_MSC_VER)
#define MSGPACK_ZONE_ALIGNOF(type) __alignof(type)
#else
#define MSGPACK_ZONE_ALIGNOF(type) __alignof__(type)
#endif
// For a compiler that doesn't support __alignof__:
// #define MSGPACK_ZONE_ALIGNOF(type) MSGPACK_ZONE_ALIGN


namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

class zone;

std::size_t aligned_size(
    std::size_t size,
    std::size_t align = MSGPACK_ZONE_ALIGN);

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_CPP03_ZONE_DECL_HPP
