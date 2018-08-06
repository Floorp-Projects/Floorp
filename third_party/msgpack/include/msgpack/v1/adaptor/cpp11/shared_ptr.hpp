//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_V1_TYPE_CPP11_SHARED_PTR_HPP
#define MSGPACK_V1_TYPE_CPP11_SHARED_PTR_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#include <memory>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

template <typename T>
struct as<std::shared_ptr<T>, typename std::enable_if<msgpack::has_as<T>::value>::type> {
    std::shared_ptr<T> operator()(msgpack::object const& o) const {
        if(o.is_nil()) return MSGPACK_NULLPTR;
        return std::make_shared<T>(o.as<T>());
    }
};

template <typename T>
struct convert<std::shared_ptr<T>> {
    msgpack::object const& operator()(msgpack::object const& o, std::shared_ptr<T>& v) const {
        if(o.is_nil()) v.reset();
        else {
            v = std::make_shared<T>();
            msgpack::adaptor::convert<T>()(o, *v);
        }
        return o;
    }
};

template <typename T>
struct pack<std::shared_ptr<T>> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::shared_ptr<T>& v) const {
        if (v) o.pack(*v);
        else o.pack_nil();
        return o;
    }
};

template <typename T>
struct object<std::shared_ptr<T> > {
    void operator()(msgpack::object& o, const std::shared_ptr<T>& v) const {
        if (v) msgpack::adaptor::object<T>()(o, *v);
        else o.type = msgpack::type::NIL;
    }
};

template <typename T>
struct object_with_zone<std::shared_ptr<T>> {
    void operator()(msgpack::object::with_zone& o, const std::shared_ptr<T>& v) const {
        if (v) msgpack::adaptor::object_with_zone<T>()(o, *v);
        else o.type = msgpack::type::NIL;
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V1_TYPE_CPP11_SHARED_PTR_HPP
