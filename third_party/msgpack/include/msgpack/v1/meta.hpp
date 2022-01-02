//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_V1_META_HPP
#define MSGPACK_V1_META_HPP

#if !defined(MSGPACK_USE_CPP03)

#include "msgpack/v1/meta_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond



namespace detail {

template<bool...values> struct all_of_imp
    : std::is_same<bool_pack<values..., true>, bool_pack<true, values...>>{};

template<bool...values> struct any_of_imp {
    static const bool value = !std::is_same<bool_pack<values..., false>, bool_pack<false, values...>>::value;
};

} // namespace detail

template<std::size_t... Is> struct seq {};

template<std::size_t N, std::size_t... Is>
struct gen_seq : gen_seq<N-1, N-1, Is...> {};

template<std::size_t... Is>
struct gen_seq<0, Is...> : seq<Is...> {};

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // !defined(MSGPACK_USE_CPP03)

#endif // MSGPACK_V1_META_HPP
