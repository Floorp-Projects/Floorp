//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_SIZE_EQUAL_ONLY_HPP
#define MSGPACK_V1_TYPE_SIZE_EQUAL_ONLY_HPP

#include "msgpack/v1/adaptor/size_equal_only_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace type {

template <typename T>
inline std::size_t size(T const& t) {
    return t.size();
}

template <typename T, std::size_t N>
inline std::size_t size(const T(&)[N]) {
    return N;
}


#if !defined(MSGPACK_USE_CPP03)

template <typename... T>
inline std::size_t size(std::tuple<T...> const&) {
    return sizeof...(T);
}

#endif // !defined(MSGPACK_USE_CPP03)


template <typename T>
struct size_equal_only {
    size_equal_only(T& t):m_t(t) {}
    T& m_t;
};

template <typename T>
inline size_equal_only<T> make_size_equal_only(T& t) {
    return size_equal_only<T>(t);
}

template <typename T>
inline bool operator<(size_equal_only<T> const& lhs, size_equal_only<T> const& rhs) {
    return lhs.m_t < rhs.m_t;
}

template <typename T>
inline bool operator==(size_equal_only<T> const& lhs, size_equal_only<T> const& rhs) {
    return lhs.m_t == &rhs.m_t;
}

}  // namespace type

namespace adaptor {

template <typename T>
struct convert<type::size_equal_only<T> > {
    msgpack::object const& operator()(msgpack::object const& o, type::size_equal_only<T>& v) const {
        switch(o.type) {
        case msgpack::type::ARRAY:
            if (o.via.array.size != msgpack::type::size(v.m_t)) throw msgpack::type_error();
            break;
        case msgpack::type::MAP:
            if (o.via.map.size != msgpack::type::size(v.m_t)) throw msgpack::type_error();
            break;
        default:
            throw msgpack::type_error();
        }
        o >> v.m_t;
        return o;
    }
};

template <typename T>
struct pack<type::size_equal_only<T> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const type::size_equal_only<T>& v) const {
        o << v.m_t;
        return o;
    }
};

template <typename T>
struct object<type::size_equal_only<T> > {
    void operator()(msgpack::object& o, type::size_equal_only<T> const& v) const {
        o << v.m_t;
    }
};

template <typename T>
struct object_with_zone<type::size_equal_only<T> > {
    void operator()(msgpack::object::with_zone& o, type::size_equal_only<T> v) const {
        o << v.m_t;
    }
};

} // namespace adaptor

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V1_TYPE_SIZE_EQUAL_ONLY_HPP
