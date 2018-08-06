//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_SIZE_EQUAL_ONLY_DECL_HPP
#define MSGPACK_V1_TYPE_SIZE_EQUAL_ONLY_DECL_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/msgpack_tuple.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace type {

template <typename T>
struct size_equal_only;

template <typename T>
size_equal_only<T> make_size_equal_only(T& t);

template <typename T>
std::size_t size(T const& t);

template <typename T, std::size_t N>
std::size_t size(const T(&)[N]);

#if !defined(MSGPACK_USE_CPP03)

template <typename... T>
std::size_t size(std::tuple<T...> const&);

#endif // !defined(MSGPACK_USE_CPP03)

} // namespace type

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V1_TYPE_SIZE_EQUAL_ONLY_DECL_HPP
