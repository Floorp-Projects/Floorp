//
// MessagePack for C++ C++03/C++11 Adaptation
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_CPP_CONFIG_DECL_HPP
#define MSGPACK_V2_CPP_CONFIG_DECL_HPP

#include "msgpack/v1/cpp_config_decl.hpp"

#if defined(MSGPACK_USE_CPP03)

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

using v1::unique_ptr;

using v1::move;

using v1::enable_if;

using v1::integral_constant;

using v1::is_same;

using v1::underlying_type;

using v1::is_array;

using v1::remove_const;
using v1::remove_volatile;
using v1::remove_cv;

using v1::is_pointer;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack


#else  // MSGPACK_USE_CPP03

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

// unique_ptr
using v1::unique_ptr;
// using v1::make_unique; // since C++14
using v1::hash;

// utility
using v1::move;
using v1::swap;
using v1::enable_if;
using v1::is_same;
using v1::underlying_type;
using v1::is_array;

using v1::remove_const;
using v1::remove_volatile;
using v1::remove_cv;

using v1::is_pointer;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond
}  // namespace msgpack


#endif // MSGPACK_USE_CPP03

#endif // MSGPACK_V2_CPP_CONFIG_DECL_HPP
