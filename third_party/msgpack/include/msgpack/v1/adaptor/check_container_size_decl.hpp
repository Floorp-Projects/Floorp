//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_CHECK_CONTAINER_SIZE_DECL_HPP
#define MSGPACK_V1_CHECK_CONTAINER_SIZE_DECL_HPP

#include "msgpack/versioning.hpp"
#include <cstdlib>
#include "msgpack/sysdep.h"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

struct container_size_overflow;

namespace detail {

template <std::size_t N>
inline void check_container_size(std::size_t size);

template <std::size_t N>
inline void check_container_size_for_ext(std::size_t size);

} // namespace detail

template <typename T>
inline uint32_t checked_get_container_size(T size);

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_CHECK_CONTAINER_SIZE_DECL_HPP
