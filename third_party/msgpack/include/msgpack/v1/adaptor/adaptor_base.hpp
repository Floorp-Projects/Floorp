//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_ADAPTOR_BASE_HPP
#define MSGPACK_V1_ADAPTOR_BASE_HPP

#include "msgpack/v1/adaptor/adaptor_base_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond


namespace adaptor {

// Adaptor functors

template <typename T, typename Enabler>
struct convert {
    msgpack::object const& operator()(msgpack::object const& o, T& v) const;
};

template <typename T, typename Enabler>
struct pack {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, T const& v) const;
};

template <typename T, typename Enabler>
struct object {
    void operator()(msgpack::object& o, T const& v) const;
};

template <typename T, typename Enabler>
struct object_with_zone {
    void operator()(msgpack::object::with_zone& o, T const& v) const;
};

} // namespace adaptor

// operators

template <typename T>
inline
typename msgpack::enable_if<
    !is_array<T>::value,
    msgpack::object const&
>::type
operator>> (msgpack::object const& o, T& v) {
    return msgpack::adaptor::convert<T>()(o, v);
}
template <typename T, std::size_t N>
inline
msgpack::object const& operator>> (msgpack::object const& o, T(&v)[N]) {
    return msgpack::adaptor::convert<T[N]>()(o, v);
}

template <typename Stream, typename T>
inline
typename msgpack::enable_if<
    !is_array<T>::value,
    msgpack::packer<Stream>&
>::type
operator<< (msgpack::packer<Stream>& o, T const& v) {
    return msgpack::adaptor::pack<T>()(o, v);
}
template <typename Stream, typename T, std::size_t N>
inline
msgpack::packer<Stream>& operator<< (msgpack::packer<Stream>& o, const T(&v)[N]) {
    return msgpack::adaptor::pack<T[N]>()(o, v);
}

template <typename T>
inline
typename msgpack::enable_if<
    !is_array<T>::value
>::type
operator<< (msgpack::object& o, T const& v) {
    msgpack::adaptor::object<T>()(o, v);
}
template <typename T, std::size_t N>
inline
void operator<< (msgpack::v1::object& o, const T(&v)[N]) {
    msgpack::v1::adaptor::object<T[N]>()(o, v);
}

template <typename T>
inline
typename msgpack::enable_if<
    !is_array<T>::value
>::type
operator<< (msgpack::object::with_zone& o, T const& v) {
    msgpack::adaptor::object_with_zone<T>()(o, v);
}
template <typename T, std::size_t N>
inline
void operator<< (msgpack::object::with_zone& o, const T(&v)[N]) {
    msgpack::adaptor::object_with_zone<T[N]>()(o, v);
}

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack


#endif // MSGPACK_V1_ADAPTOR_BASE_HPP
