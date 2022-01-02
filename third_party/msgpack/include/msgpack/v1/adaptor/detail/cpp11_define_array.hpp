//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_CPP11_DEFINE_ARRAY_HPP
#define MSGPACK_V1_CPP11_DEFINE_ARRAY_HPP

#include "msgpack/v1/adaptor/detail/cpp11_define_array_decl.hpp"
#include "msgpack/v1/adaptor/detail/cpp11_convert_helper.hpp"

#include <tuple>

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond
namespace type {

template <typename Tuple, std::size_t N>
struct define_array_imp {
    template <typename Packer>
    static void pack(Packer& pk, Tuple const& t) {
        define_array_imp<Tuple, N-1>::pack(pk, t);
        pk.pack(std::get<N-1>(t));
    }
    static void unpack(msgpack::object const& o, Tuple& t) {
        define_array_imp<Tuple, N-1>::unpack(o, t);
        const size_t size = o.via.array.size;
        if(size <= N-1) { return; }
        convert_helper(o.via.array.ptr[N-1], std::get<N-1>(t));
    }
    static void object(msgpack::object* o, msgpack::zone& z, Tuple const& t) {
        define_array_imp<Tuple, N-1>::object(o, z, t);
        o->via.array.ptr[N-1] = msgpack::object(std::get<N-1>(t), z);
    }
};

template <typename Tuple>
struct define_array_imp<Tuple, 1> {
    template <typename Packer>
    static void pack(Packer& pk, Tuple const& t) {
        pk.pack(std::get<0>(t));
    }
    static void unpack(msgpack::object const& o, Tuple& t) {
        const size_t size = o.via.array.size;
        if(size <= 0) { return; }
        convert_helper(o.via.array.ptr[0], std::get<0>(t));
    }
    static void object(msgpack::object* o, msgpack::zone& z, Tuple const& t) {
        o->via.array.ptr[0] = msgpack::object(std::get<0>(t), z);
    }
};

template <typename... Args>
struct define_array {
    typedef define_array<Args...> value_type;
    typedef std::tuple<Args...> tuple_type;
    define_array(Args&... args) :
        a(args...) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(sizeof...(Args));

        define_array_imp<std::tuple<Args&...>, sizeof...(Args)>::pack(pk, a);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }

        define_array_imp<std::tuple<Args&...>, sizeof...(Args)>::unpack(o, a);
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*sizeof...(Args), MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = sizeof...(Args);

        define_array_imp<std::tuple<Args&...>, sizeof...(Args)>::object(o, z, a);
    }

    std::tuple<Args&...> a;
};

template <>
struct define_array<> {
    typedef define_array<> value_type;
    typedef std::tuple<> tuple_type;
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(0);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone&) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = NULL;
        o->via.array.size = 0;
    }
};

inline define_array<> make_define_array()
{
    return define_array<>();
}

template <typename... Args>
inline define_array<Args...> make_define_array(Args&... args)
{
    return define_array<Args...>(args...);
}

}  // namespace type
/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond
}  // namespace msgpack

#endif // MSGPACK_V1_CPP11_DEFINE_ARRAY_HPP
