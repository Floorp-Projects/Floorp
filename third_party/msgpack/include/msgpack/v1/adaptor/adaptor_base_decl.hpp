//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_ADAPTOR_BASE_DECL_HPP
#define MSGPACK_V1_ADAPTOR_BASE_DECL_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/object_fwd.hpp"
#include "msgpack/pack.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

template <typename Stream>
class packer;

namespace adaptor {

// Adaptor functors

template <typename T, typename Enabler = void>
struct convert;

template <typename T, typename Enabler = void>
struct pack;

template <typename T, typename Enabler = void>
struct object;

template <typename T, typename Enabler = void>
struct object_with_zone;

} // namespace adaptor

// operators

template <typename T>
typename msgpack::enable_if<
    !is_array<T>::value,
    msgpack::object const&
>::type
operator>> (msgpack::object const& o, T& v);
template <typename T, std::size_t N>
msgpack::object const& operator>> (msgpack::object const& o, T(&v)[N]);

template <typename Stream, typename T>
typename msgpack::enable_if<
    !is_array<T>::value,
    msgpack::packer<Stream>&
>::type
operator<< (msgpack::packer<Stream>& o, T const& v);
template <typename Stream, typename T, std::size_t N>
msgpack::packer<Stream>& operator<< (msgpack::packer<Stream>& o, const T(&v)[N]);

template <typename T>
typename msgpack::enable_if<
    !is_array<T>::value
>::type
operator<< (msgpack::object& o, T const& v);
template <typename T, std::size_t N>
void operator<< (msgpack::object& o, const T(&v)[N]);

template <typename T>
typename msgpack::enable_if<
    !is_array<T>::value
>::type
operator<< (msgpack::object::with_zone& o, T const& v);
template <typename T, std::size_t N>
void operator<< (msgpack::object::with_zone& o, const T(&v)[N]);

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V1_ADAPTOR_BASE_DECL_HPP
