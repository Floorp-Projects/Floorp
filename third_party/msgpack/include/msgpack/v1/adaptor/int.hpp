//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_INT_HPP
#define MSGPACK_V1_TYPE_INT_HPP

#include "msgpack/v1/adaptor/int_decl.hpp"
#include "msgpack/object.hpp"

#include <limits>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1){
/// @endcond

namespace type {
namespace detail {

template <typename T>
struct convert_integer_sign<T, true> {
    static T convert(msgpack::object const& o) {
        if(o.type == msgpack::type::POSITIVE_INTEGER) {
            if(o.via.u64 > static_cast<uint64_t>(std::numeric_limits<T>::max()))
            { throw msgpack::type_error(); }
            return static_cast<T>(o.via.u64);
        } else if(o.type == msgpack::type::NEGATIVE_INTEGER) {
            if(o.via.i64 < static_cast<int64_t>(std::numeric_limits<T>::min()))
            { throw msgpack::type_error(); }
            return static_cast<T>(o.via.i64);
        }
        throw msgpack::type_error();
    }
};

template <typename T>
struct convert_integer_sign<T, false> {
    static T convert(msgpack::object const& o) {
        if(o.type == msgpack::type::POSITIVE_INTEGER) {
            if(o.via.u64 > static_cast<uint64_t>(std::numeric_limits<T>::max()))
            { throw msgpack::type_error(); }
            return static_cast<T>(o.via.u64);
        }
        throw msgpack::type_error();
    }
};

template <typename T>
struct is_signed {
    static const bool value = std::numeric_limits<T>::is_signed;
};

template <typename T>
inline T convert_integer(msgpack::object const& o)
{
    return detail::convert_integer_sign<T, is_signed<T>::value>::convert(o);
}

template <>
struct object_char_sign<true> {
    template <typename T>
    static typename msgpack::enable_if<msgpack::is_same<T, char>::value>::type
    make(msgpack::object& o, T v) {
        if (v < 0) {
            o.type = msgpack::type::NEGATIVE_INTEGER;
            o.via.i64 = v;
        }
        else {
            o.type = msgpack::type::POSITIVE_INTEGER;
            o.via.u64 = v;
        }
    }
};

template <>
struct object_char_sign<false> {
    static void make(msgpack::object& o, char v) {
        o.type = msgpack::type::POSITIVE_INTEGER;
        o.via.u64 = v;
    }
};

inline void object_char(msgpack::object& o, char v) {
    return object_char_sign<is_signed<char>::value>::make(o, v);
}

}  // namespace detail
}  // namespace type

namespace adaptor {

template <>
struct convert<char> {
    msgpack::object const& operator()(msgpack::object const& o, char& v) const
    { v = type::detail::convert_integer<char>(o); return o; }
};

template <>
struct convert<signed char> {
    msgpack::object const& operator()(msgpack::object const& o, signed char& v) const
    { v = type::detail::convert_integer<signed char>(o); return o; }
};

template <>
struct convert<signed short> {
    msgpack::object const& operator()(msgpack::object const& o, signed short& v) const
    { v = type::detail::convert_integer<signed short>(o); return o; }
};

template <>
struct convert<signed int> {
    msgpack::object const& operator()(msgpack::object const& o, signed int& v) const
    { v = type::detail::convert_integer<signed int>(o); return o; }
};

template <>
struct convert<signed long> {
    msgpack::object const& operator()(msgpack::object const& o, signed long& v) const
    { v = type::detail::convert_integer<signed long>(o); return o; }
};

template <>
struct convert<signed long long> {
    msgpack::object const& operator()(msgpack::object const& o, signed long long& v) const
    { v = type::detail::convert_integer<signed long long>(o); return o; }
};


template <>
struct convert<unsigned char> {
    msgpack::object const& operator()(msgpack::object const& o, unsigned char& v) const
    { v = type::detail::convert_integer<unsigned char>(o); return o; }
};

template <>
struct convert<unsigned short> {
    msgpack::object const& operator()(msgpack::object const& o, unsigned short& v) const
    { v = type::detail::convert_integer<unsigned short>(o); return o; }
};

template <>
struct convert<unsigned int> {
    msgpack::object const& operator()(msgpack::object const& o, unsigned int& v) const
    { v = type::detail::convert_integer<unsigned int>(o); return o; }
};

template <>
struct convert<unsigned long> {
    msgpack::object const& operator()(msgpack::object const& o, unsigned long& v) const
    { v = type::detail::convert_integer<unsigned long>(o); return o; }
};

template <>
struct convert<unsigned long long> {
    msgpack::object const& operator()(msgpack::object const& o, unsigned long long& v) const
    { v = type::detail::convert_integer<unsigned long long>(o); return o; }
};


template <>
struct pack<char> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, char v) const
    { o.pack_char(v); return o; }
};

template <>
struct pack<signed char> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, signed char v) const
    { o.pack_signed_char(v); return o; }
};

template <>
struct pack<signed short> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, signed short v) const
    { o.pack_short(v); return o; }
};

template <>
struct pack<signed int> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, signed int v) const
    { o.pack_int(v); return o; }
};

template <>
struct pack<signed long> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, signed long v) const
    { o.pack_long(v); return o; }
};

template <>
struct pack<signed long long> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, signed long long v) const
    { o.pack_long_long(v); return o; }
};


template <>
struct pack<unsigned char> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, unsigned char v) const
    { o.pack_unsigned_char(v); return o; }
};

template <>
struct pack<unsigned short> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, unsigned short v) const
    { o.pack_unsigned_short(v); return o; }
};

template <>
struct pack<unsigned int> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, unsigned int v) const
    { o.pack_unsigned_int(v); return o; }
};

template <>
struct pack<unsigned long> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, unsigned long v) const
    { o.pack_unsigned_long(v); return o; }
};

template <>
struct pack<unsigned long long> {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, unsigned long long v) const
    { o.pack_unsigned_long_long(v); return o; }
};


template <>
struct object<char> {
    void operator()(msgpack::object& o, char v) const
    { type::detail::object_char(o, v); }
};

template <>
struct object<signed char> {
    void operator()(msgpack::object& o, signed char v) const {
        if (v < 0) {
            o.type = msgpack::type::NEGATIVE_INTEGER;
            o.via.i64 = v;
        }
        else {
            o.type = msgpack::type::POSITIVE_INTEGER;
            o.via.u64 = v;
        }
    }
};

template <>
struct object<signed short> {
    void operator()(msgpack::object& o, signed short v) const {
        if (v < 0) {
            o.type = msgpack::type::NEGATIVE_INTEGER;
            o.via.i64 = v;
        }
        else {
            o.type = msgpack::type::POSITIVE_INTEGER;
            o.via.u64 = v;
        }
    }
};

template <>
struct object<signed int> {
    void operator()(msgpack::object& o, signed int v) const {
        if (v < 0) {
            o.type = msgpack::type::NEGATIVE_INTEGER;
            o.via.i64 = v;
        }
        else {
            o.type = msgpack::type::POSITIVE_INTEGER;
            o.via.u64 = v;
        }
    }
};

template <>
struct object<signed long> {
    void operator()(msgpack::object& o, signed long v) const {
        if (v < 0) {
            o.type = msgpack::type::NEGATIVE_INTEGER;
            o.via.i64 = v;
        }
        else {
            o.type = msgpack::type::POSITIVE_INTEGER;
            o.via.u64 = v;
        }
    }
};

template <>
struct object<signed long long> {
    void operator()(msgpack::object& o, signed long long v) const {
        if (v < 0) {
            o.type = msgpack::type::NEGATIVE_INTEGER;
            o.via.i64 = v;
        }
        else{
            o.type = msgpack::type::POSITIVE_INTEGER;
            o.via.u64 = v;
        }
    }
};

template <>
struct object<unsigned char> {
    void operator()(msgpack::object& o, unsigned char v) const {
        o.type = msgpack::type::POSITIVE_INTEGER;
        o.via.u64 = v;
    }
};

template <>
struct object<unsigned short> {
    void operator()(msgpack::object& o, unsigned short v) const {
        o.type = msgpack::type::POSITIVE_INTEGER;
        o.via.u64 = v;
    }
};

template <>
struct object<unsigned int> {
    void operator()(msgpack::object& o, unsigned int v) const {
        o.type = msgpack::type::POSITIVE_INTEGER;
        o.via.u64 = v;
    }
};

template <>
struct object<unsigned long> {
    void operator()(msgpack::object& o, unsigned long v) const {
        o.type = msgpack::type::POSITIVE_INTEGER;
        o.via.u64 = v;
    }
};

template <>
struct object<unsigned long long> {
    void operator()(msgpack::object& o, unsigned long long v) const {
        o.type = msgpack::type::POSITIVE_INTEGER;
        o.via.u64 = v;
    }
};


template <>
struct object_with_zone<char> {
    void operator()(msgpack::object::with_zone& o, char v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<signed char> {
    void operator()(msgpack::object::with_zone& o, signed char v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<signed short> {
    void operator()(msgpack::object::with_zone& o, signed short v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<signed int> {
    void operator()(msgpack::object::with_zone& o, signed int v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<signed long> {
    void operator()(msgpack::object::with_zone& o, signed long v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<signed long long> {
    void operator()(msgpack::object::with_zone& o, const signed long long& v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<unsigned char> {
    void operator()(msgpack::object::with_zone& o, unsigned char v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<unsigned short> {
    void operator()(msgpack::object::with_zone& o, unsigned short v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<unsigned int> {
    void operator()(msgpack::object::with_zone& o, unsigned int v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<unsigned long> {
    void operator()(msgpack::object::with_zone& o, unsigned long v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

template <>
struct object_with_zone<unsigned long long> {
    void operator()(msgpack::object::with_zone& o, const unsigned long long& v) const {
        static_cast<msgpack::object&>(o) << v;
    }
};

} // namespace adaptor

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_TYPE_INT_HPP
