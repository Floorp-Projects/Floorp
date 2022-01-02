//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_ADAPTOR_BASE_HPP
#define MSGPACK_V3_ADAPTOR_BASE_HPP

#include "msgpack/v3/adaptor/adaptor_base_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond


namespace adaptor {

// Adaptor functors

template <typename T, typename Enabler>
struct convert : v2::adaptor::convert<T, Enabler> {
};

template <typename T, typename Enabler>
struct pack : v2::adaptor::pack<T, Enabler> {
};

template <typename T, typename Enabler>
struct object<
    T,
    Enabler,
    typename msgpack::enable_if<
        !msgpack::is_same<T, std::string>::value &&
        !msgpack::is_array<T>::value
    >::type>
    : v2::adaptor::object<T, Enabler> {
};

template <typename T, typename Enabler>
struct object_with_zone : v2::adaptor::object_with_zone<T, Enabler> {
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

} // namespace msgpack


#endif // MSGPACK_V3_ADAPTOR_BASE_HPP
