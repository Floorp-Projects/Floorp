//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_TYPE_INT_DECL_HPP
#define MSGPACK_V3_TYPE_INT_DECL_HPP

#include "msgpack/v2/adaptor/int_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3){
/// @endcond

namespace type {
namespace detail {


template <typename T, bool Signed>
struct convert_integer_sign;

template <typename T>
struct is_signed;


template <bool Signed>
struct object_char_sign;

//using v2::type::detail::convert_integer_sign;

//using v2::type::detail::is_signed;

using v2::type::detail::convert_integer;

//using v2::type::detail::object_char_sign;

using v2::type::detail::object_char;

}  // namespace detail
}  // namespace type

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V3_TYPE_INT_DECL_HPP
