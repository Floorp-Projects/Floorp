//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2009 FURUHASHI Sadayuki
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_PAIR_HPP
#define MSGPACK_V1_TYPE_PAIR_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/meta.hpp"

#include <utility>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

#if !defined(MSGPACK_USE_CPP03)

template <typename T1, typename T2>
struct as<std::pair<T1, T2>,
          typename std::enable_if<msgpack::any_of<msgpack::has_as, T1, T2>::value>::type> {
    std::pair<T1, T2> operator()(msgpack::object const& o) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (o.via.array.size != 2) { throw msgpack::type_error(); }
        return std::make_pair(o.via.array.ptr[0].as<T1>(), o.via.array.ptr[1].as<T2>());
    }
};

#endif // !defined(MSGPACK_USE_CPP03)

template <typename T1, typename T2>
struct convert<std::pair<T1, T2> > {
    msgpack::object const& operator()(msgpack::object const& o, std::pair<T1, T2>& v) const {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if(o.via.array.size != 2) { throw msgpack::type_error(); }
        o.via.array.ptr[0].convert(v.first);
        o.via.array.ptr[1].convert(v.second);
        return o;
    }
};

template <typename T1, typename T2>
struct pack<std::pair<T1, T2> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::pair<T1, T2>& v) const {
        o.pack_array(2);
        o.pack(v.first);
        o.pack(v.second);
        return o;
    }
};

template <typename T1, typename T2>
struct object_with_zone<std::pair<T1, T2> > {
    void operator()(msgpack::object::with_zone& o, const std::pair<T1, T2>& v) const {
        o.type = msgpack::type::ARRAY;
        msgpack::object* p = static_cast<msgpack::object*>(o.zone.allocate_align(sizeof(msgpack::object)*2, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o.via.array.ptr = p;
        o.via.array.size = 2;
        p[0] = msgpack::object(v.first, o.zone);
        p[1] = msgpack::object(v.second, o.zone);
    }
};

} // namespace adaptor

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_TYPE_PAIR_HPP
