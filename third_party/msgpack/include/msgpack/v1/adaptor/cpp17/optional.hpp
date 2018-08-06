//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2017 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_OPTIONAL_HPP
#define MSGPACK_V1_TYPE_OPTIONAL_HPP

#if __cplusplus >= 201703

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#include <optional>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

#if !defined (MSGPACK_USE_CPP03)

template <typename T>
struct as<std::optional<T>, typename std::enable_if<msgpack::has_as<T>::value>::type> {
    std::optional<T> operator()(msgpack::object const& o) const {
        if(o.is_nil()) return std::nullopt;
        return o.as<T>();
    }
};

#endif // !defined (MSGPACK_USE_CPP03)

template <typename T>
struct convert<std::optional<T> > {
    msgpack::object const& operator()(msgpack::object const& o, std::optional<T>& v) const {
        if(o.is_nil()) v = std::nullopt;
        else {
            T t;
            msgpack::adaptor::convert<T>()(o, t);
            v = t;
        }
        return o;
    }
};

template <typename T>
struct pack<std::optional<T> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::optional<T>& v) const {
        if (v) o.pack(*v);
        else o.pack_nil();
        return o;
    }
};

template <typename T>
struct object<std::optional<T> > {
    void operator()(msgpack::object& o, const std::optional<T>& v) const {
        if (v) msgpack::adaptor::object<T>()(o, *v);
        else o.type = msgpack::type::NIL;
    }
};

template <typename T>
struct object_with_zone<std::optional<T> > {
    void operator()(msgpack::object::with_zone& o, const std::optional<T>& v) const {
        if (v) msgpack::adaptor::object_with_zone<T>()(o, *v);
        else o.type = msgpack::type::NIL;
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // __cplusplus >= 201703

#endif // MSGPACK_V1_TYPE_OPTIONAL_HPP
