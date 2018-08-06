//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_CHECK_CONTAINER_SIZE_DECL_HPP
#define MSGPACK_V3_CHECK_CONTAINER_SIZE_DECL_HPP

#include "msgpack/v2/adaptor/check_container_size_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

using v2::container_size_overflow;

namespace detail {

using v2::detail::check_container_size;

using v2::detail::check_container_size_for_ext;

} // namespace detail

using v2::checked_get_container_size;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V3_CHECK_CONTAINER_SIZE_DECL_HPP
