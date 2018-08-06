//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2015 FURUHASHI Sadayuki
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_TYPE_TR1_UNORDERED_SET_HPP
#define MSGPACK_TYPE_TR1_UNORDERED_SET_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#if defined(_LIBCPP_VERSION) || (_MSC_VER >= 1700)

#define MSGPACK_HAS_STD_UNORDERED_SET
#include <unordered_set>
#define MSGPACK_STD_TR1 std

#else   // defined(_LIBCPP_VERSION) || (_MSC_VER >= 1700)

#if __GNUC__ >= 4

#define MSGPACK_HAS_STD_TR1_UNORDERED_SET

#include <tr1/unordered_set>
#define MSGPACK_STD_TR1 std::tr1

#endif // __GNUC__ >= 4

#endif  // defined(_LIBCPP_VERSION) || (_MSC_VER >= 1700)

#if defined(MSGPACK_STD_TR1)

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

template <typename T, typename Hash, typename Compare, typename Alloc>
struct convert<MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc> > {
    msgpack::object const& operator()(msgpack::object const& o, MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc>& v) const {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        msgpack::object* p = o.via.array.ptr + o.via.array.size;
        msgpack::object* const pbegin = o.via.array.ptr;
        MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc> tmp;
        while(p > pbegin) {
            --p;
            tmp.insert(p->as<T>());
        }
        tmp.swap(v);
        return o;
    }
};

template <typename T, typename Hash, typename Compare, typename Alloc>
struct pack<MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.pack_array(size);
        for(typename MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc>::const_iterator it(v.begin()), it_end(v.end());
            it != it_end; ++it) {
            o.pack(*it);
        }
        return o;
    }
};

template <typename T, typename Hash, typename Compare, typename Alloc>
struct object_with_zone<MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc> > {
    void operator()(msgpack::object::with_zone& o, const MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc>& v) const {
        o.type = msgpack::type::ARRAY;
        if(v.empty()) {
            o.via.array.ptr = MSGPACK_NULLPTR;
            o.via.array.size = 0;
        } else {
            uint32_t size = checked_get_container_size(v.size());
            msgpack::object* p = static_cast<msgpack::object*>(o.zone.allocate_align(sizeof(msgpack::object)*size, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
            msgpack::object* const pend = p + size;
            o.via.array.ptr = p;
            o.via.array.size = size;
            typename MSGPACK_STD_TR1::unordered_set<T, Hash, Compare, Alloc>::const_iterator it(v.begin());
            do {
                *p = msgpack::object(*it, o.zone);
                ++p;
                ++it;
            } while(p < pend);
        }
    }
};


template <typename T, typename Hash, typename Compare, typename Alloc>
struct convert<MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc> > {
    msgpack::object const& operator()(msgpack::object const& o, MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc>& v) const {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        msgpack::object* p = o.via.array.ptr + o.via.array.size;
        msgpack::object* const pbegin = o.via.array.ptr;
        MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc> tmp;
        while(p > pbegin) {
            --p;
            tmp.insert(p->as<T>());
        }
        tmp.swap(v);
        return o;
    }
};

template <typename T, typename Hash, typename Compare, typename Alloc>
struct pack<MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc> > {
    template <typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.pack_array(size);
        for(typename MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc>::const_iterator it(v.begin()), it_end(v.end());
            it != it_end; ++it) {
            o.pack(*it);
        }
        return o;
    }
};

template <typename T, typename Hash, typename Compare, typename Alloc>
struct object_with_zone<MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc> > {
    void operator()(msgpack::object::with_zone& o, const MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc>& v) const {
        o.type = msgpack::type::ARRAY;
        if(v.empty()) {
            o.via.array.ptr = MSGPACK_NULLPTR;
            o.via.array.size = 0;
        } else {
            uint32_t size = checked_get_container_size(v.size());
            msgpack::object* p = static_cast<msgpack::object*>(o.zone.allocate_align(sizeof(msgpack::object)*size, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
            msgpack::object* const pend = p + size;
            o.via.array.ptr = p;
            o.via.array.size = size;
            typename MSGPACK_STD_TR1::unordered_multiset<T, Hash, Compare, Alloc>::const_iterator it(v.begin());
            do {
                *p = msgpack::object(*it, o.zone);
                ++p;
                ++it;
            } while(p < pend);
        }
    }
};

} // namespace adaptor

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#undef MSGPACK_STD_TR1

#endif // MSGPACK_STD_TR1

#endif // MSGPACK_TYPE_TR1_UNORDERED_SET_HPP
