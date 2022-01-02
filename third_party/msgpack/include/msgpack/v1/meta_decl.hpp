//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_V1_META_DECL_HPP
#define MSGPACK_V1_META_DECL_HPP

#if !defined(MSGPACK_USE_CPP03)

#include "msgpack/versioning.hpp"
#include <type_traits>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace detail {

template<bool...> struct bool_pack;

template<bool...values> struct all_of_imp;

template<bool...values> struct any_of_imp;

} // namespace detail

template<template <class> class T, class... U>
using all_of = detail::all_of_imp<T<U>::value...>;

template<template <class> class T, class... U>
using any_of = detail::any_of_imp<(T<U>::value)...>;

template<std::size_t... Is> struct seq;

template<std::size_t N, std::size_t... Is>
struct gen_seq;

template <typename T>
struct is_non_const_pointer;

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // !defined(MSGPACK_USE_CPP03)

#endif // MSGPACK_V1_META_DECL_HPP
