//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_ARRAY_REF_HPP
#define MSGPACK_V1_TYPE_ARRAY_REF_HPP

#include "msgpack/v1/adaptor/array_ref.hpp"
#include "msgpack/adaptor/check_container_size.hpp"
#include "msgpack/cpp_config.hpp"
#include <cstring>
#include <string>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace type {

template <typename T>
struct array_ref {
    array_ref() : data(MSGPACK_NULLPTR) {}
    array_ref(T& t) : data(&t) {}

    T* data;

    std::size_t size() const {
        return data->size();
    }

    template <typename U>
    bool operator==(array_ref<U> const& t) const {
        return *data == *t.data;
    }
    template <typename U>
    bool operator!=(array_ref<U> const& t) const {
        return !(*data == *t.data);
    }
    template <typename U>
    bool operator< (array_ref<U> const& t) const
    {
        return *data < *t.data;
    }
    template <typename U>
    bool operator> (array_ref<U> const& t) const
    {
        return *t.data < *data;
    }
    template <typename U>
    bool operator<= (array_ref<U> const& t) const
    {
        return !(*t.data < *data);
    }
    template <typename U>
    bool operator>= (array_ref<U> const& t) const
    {
        return !(*data < *t.data);
    }
};

template <typename T, std::size_t N>
struct array_ref<T[N]> {
    array_ref() : data(MSGPACK_NULLPTR) {}
    array_ref(T(&t)[N]) : data(t) {}

    T* data;

    std::size_t size() const {
        return N;
    }

    template <typename U>
    bool operator==(array_ref<U> const& t) const {
        if (N != t.size()) return false;
        T const* pself = data;
        U const* pother = t.data;
        for (; pself != &data[N]; ++pself, ++pother) {
            if (*pself != *pother) return false;
        }
        return true;
    }
    template <typename U>
    bool operator!=(array_ref<U> const& t) const {
        return !(*this == t);
    }
    template <typename U>
    bool operator< (array_ref<U> const& t) const
    {
        T const* pself = data;
        U const* pother = t.data;
        for (; pself != &data[N] && pother != t.data[t.size()]; ++pself, ++pother) {
            if (*pself < *pother) return true;
        }
        if (N < t.size()) return true;
        return false;
    }
    template <typename U>
    bool operator> (array_ref<U> const& t) const
    {
        return t.data < data;
    }
    template <typename U>
    bool operator<= (array_ref<U> const& t) const
    {
        return !(t.data < data);
    }
    template <typename U>
    bool operator>= (array_ref<U> const& t) const
    {
        return !(data < t.data);
    }
};

template <typename T>
inline
typename msgpack::enable_if<
    !msgpack::is_array<T const>::value,
    array_ref<T const>
>::type
make_array_ref(const T& t) {
    return array_ref<T const>(t);
}

template <typename T>
inline
typename msgpack::enable_if<
    !msgpack::is_array<T>::value,
    array_ref<T>
>::type
make_array_ref(T& t) {
    return array_ref<T>(t);
}

template <typename T, std::size_t N>
inline array_ref<const T[N]> make_array_ref(const T(&t)[N]) {
    return array_ref<const T[N]>(t);
}

template <typename T, std::size_t N>
inline array_ref<T[N]> make_array_ref(T(&t)[N]) {
    return array_ref<T[N]>(t);
}

} // namespace type

namespace adaptor {

template <typename T>
struct convert<msgpack::type::array_ref<T> > {
    msgpack::object const& operator()(msgpack::object const& o, msgpack::type::array_ref<T>& v) const {
        if (!v.data) { throw msgpack::type_error(); }
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (v.size() < o.via.bin.size) { throw msgpack::type_error(); }
        if (o.via.array.size > 0) {
            msgpack::object* p = o.via.array.ptr;
            msgpack::object* const pend = o.via.array.ptr + o.via.array.size;
            typename T::iterator it = v.data->begin();
            do {
                p->convert(*it);
                ++p;
                ++it;
            } while(p < pend);
        }
        return o;
    }
};

template <typename T, std::size_t N>
struct convert<msgpack::type::array_ref<T[N]> > {
    msgpack::object const& operator()(msgpack::object const& o, msgpack::type::array_ref<T[N]>& v) const {
        if (!v.data) { throw msgpack::type_error(); }
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        if (v.size() < o.via.bin.size) { throw msgpack::type_error(); }
        if (o.via.array.size > 0) {
            msgpack::object* p = o.via.array.ptr;
            msgpack::object* const pend = o.via.array.ptr + o.via.array.size;
            T* it = v.data;
            do {
                p->convert(*it);
                ++p;
                ++it;
            } while(p < pend);
        }
        return o;
    }
};

template <typename T>
struct convert<msgpack::type::array_ref<std::vector<T> > > {
    msgpack::object const& operator()(msgpack::object const& o, msgpack::type::array_ref<std::vector<T> >& v) const {
        if (!v.data) { throw msgpack::type_error(); }
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        v.data->resize(o.via.bin.size);
        if (o.via.array.size > 0) {
            msgpack::object* p = o.via.array.ptr;
            msgpack::object* const pend = o.via.array.ptr + o.via.array.size;
            typename std::vector<T>::iterator it = v.data->begin();
            do {
                p->convert(*it);
                ++p;
                ++it;
            } while(p < pend);
        }
        return o;
    }
};

template <typename T>
struct pack<msgpack::type::array_ref<T> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const msgpack::type::array_ref<T>& v) const {
        if (!v.data) { throw msgpack::type_error(); }
        uint32_t size = checked_get_container_size(v.size());
        o.pack_array(size);
        for (typename T::const_iterator it(v.data->begin()), it_end(v.data->end());
            it != it_end; ++it) {
            o.pack(*it);
        }
        return o;
    }
};

template <typename T, std::size_t N>
struct pack<msgpack::type::array_ref<T[N]> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const msgpack::type::array_ref<T[N]>& v) const {
        if (!v.data) { throw msgpack::type_error(); }
        uint32_t size = checked_get_container_size(v.size());
        o.pack_array(size);
        for (T const* it = v.data;
             it != &v.data[v.size()]; ++it) {
            o.pack(*it);
        }
        return o;
    }
};

template <typename T>
struct object_with_zone<msgpack::type::array_ref<T> > {
    void operator()(msgpack::object::with_zone& o, const msgpack::type::array_ref<T>& v) const {
        if (!v.data) { throw msgpack::type_error(); }
        o.type = msgpack::type::ARRAY;
        if (v.data->empty()) {
            o.via.array.ptr = MSGPACK_NULLPTR;
            o.via.array.size = 0;
        }
        else {
            uint32_t size = checked_get_container_size(v.size());
            msgpack::object* p = static_cast<msgpack::object*>(o.zone.allocate_align(sizeof(msgpack::object)*size, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
            msgpack::object* const pend = p + size;
            o.via.array.ptr = p;
            o.via.array.size = size;
            typename T::const_iterator it(v.data->begin());
            do {
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif // (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
                *p = msgpack::object(*it, o.zone);
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif // (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__clang__)
                ++p;
                ++it;
            } while(p < pend);
        }
    }
};

template <typename T, std::size_t N>
struct object_with_zone<msgpack::type::array_ref<T[N]> > {
    void operator()(msgpack::object::with_zone& o, const msgpack::type::array_ref<T[N]>& v) const {
        if (!v.data) { throw msgpack::type_error(); }
        o.type = msgpack::type::ARRAY;
        uint32_t size = checked_get_container_size(v.size());
        msgpack::object* p = static_cast<msgpack::object*>(o.zone.allocate_align(sizeof(msgpack::object)*size, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        msgpack::object* const pend = p + size;
        o.via.array.ptr = p;
        o.via.array.size = size;
        T const* it = v.data;
        do {
            *p = msgpack::object(*it, o.zone);
            ++p;
            ++it;
        } while(p < pend);
    }
};


} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V1_TYPE_ARRAY_REF_HPP
