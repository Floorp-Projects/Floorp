//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2017 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_CPP11_CONVERT_HELPER_HPP
#define MSGPACK_V1_CPP11_CONVERT_HELPER_HPP

#include <tuple>

#include <msgpack/object_fwd.hpp>

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond
namespace type {

template <typename T>
inline typename std::enable_if<
    has_as<T>::value
>::type
convert_helper(msgpack::object const& o, T& t) {
    t = o.as<T>();
}
template <typename T>
inline typename std::enable_if<
    !has_as<T>::value
>::type
convert_helper(msgpack::object const& o, T& t) {
    o.convert(t);
}

}  // namespace type

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond
}  // namespace msgpack

#endif // MSGPACK_V1_CPP11_CONVERT_HELPER_HPP
