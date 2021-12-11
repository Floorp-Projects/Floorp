//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2014-2015 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_CPP11_UNORDERED_SET_HPP
#define MSGPACK_V1_TYPE_CPP11_UNORDERED_SET_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/adaptor/check_container_size.hpp"

#include <unordered_set>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace adaptor {

template <typename Key, typename Hash, typename Compare, typename Alloc>
struct as<std::unordered_set<Key, Hash, Compare, Alloc>, typename std::enable_if<msgpack::has_as<Key>::value>::type> {
    std::unordered_set<Key, Hash, Compare, Alloc> operator()(msgpack::object const& o) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        msgpack::object* p = o.via.array.ptr + o.via.array.size;
        msgpack::object* const pbegin = o.via.array.ptr;
        std::unordered_set<Key, Hash, Compare, Alloc> v;
        while (p > pbegin) {
            --p;
            v.insert(p->as<Key>());
        }
        return v;
    }
};

template <typename Key, typename Hash, typename Compare, typename Alloc>
struct convert<std::unordered_set<Key, Hash, Compare, Alloc>> {
    msgpack::object const& operator()(msgpack::object const& o, std::unordered_set<Key, Hash, Compare, Alloc>& v) const {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        msgpack::object* p = o.via.array.ptr + o.via.array.size;
        msgpack::object* const pbegin = o.via.array.ptr;
        std::unordered_set<Key, Hash, Compare, Alloc> tmp;
        while(p > pbegin) {
            --p;
            tmp.insert(p->as<Key>());
        }
        v = std::move(tmp);
        return o;
    }
};

template <typename Key, typename Hash, typename Compare, typename Alloc>
struct pack<std::unordered_set<Key, Hash, Compare, Alloc>> {
    template <typename Stream>
        msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::unordered_set<Key, Hash, Compare, Alloc>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.pack_array(size);
        for(typename std::unordered_set<Key, Hash, Compare, Alloc>::const_iterator it(v.begin()), it_end(v.end());
            it != it_end; ++it) {
            o.pack(*it);
        }
        return o;
    }
};

template <typename Key, typename Hash, typename Compare, typename Alloc>
struct object_with_zone<std::unordered_set<Key, Hash, Compare, Alloc>> {
    void operator()(msgpack::object::with_zone& o, const std::unordered_set<Key, Hash, Compare, Alloc>& v) const {
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
            typename std::unordered_set<Key, Hash, Compare, Alloc>::const_iterator it(v.begin());
            do {
                *p = msgpack::object(*it, o.zone);
                ++p;
                ++it;
            } while(p < pend);
        }
    }
};


template <typename Key, typename Hash, typename Compare, typename Alloc>
struct as<std::unordered_multiset<Key, Hash, Compare, Alloc>, typename std::enable_if<msgpack::has_as<Key>::value>::type> {
    std::unordered_multiset<Key, Hash, Compare, Alloc> operator()(msgpack::object const& o) const {
        if (o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        msgpack::object* p = o.via.array.ptr + o.via.array.size;
        msgpack::object* const pbegin = o.via.array.ptr;
        std::unordered_multiset<Key, Hash, Compare, Alloc> v;
        while (p > pbegin) {
            --p;
            v.insert(p->as<Key>());
        }
        return v;
    }
};

template <typename Key, typename Hash, typename Compare, typename Alloc>
struct convert<std::unordered_multiset<Key, Hash, Compare, Alloc>> {
    msgpack::object const& operator()(msgpack::object const& o, std::unordered_multiset<Key, Hash, Compare, Alloc>& v) const {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        msgpack::object* p = o.via.array.ptr + o.via.array.size;
        msgpack::object* const pbegin = o.via.array.ptr;
        std::unordered_multiset<Key, Hash, Compare, Alloc> tmp;
        while(p > pbegin) {
            --p;
            tmp.insert(p->as<Key>());
        }
        v = std::move(tmp);
        return o;
    }
};

template <typename Key, typename Hash, typename Compare, typename Alloc>
struct pack<std::unordered_multiset<Key, Hash, Compare, Alloc>> {
    template <typename Stream>
        msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const std::unordered_multiset<Key, Hash, Compare, Alloc>& v) const {
        uint32_t size = checked_get_container_size(v.size());
        o.pack_array(size);
        for(typename std::unordered_multiset<Key, Hash, Compare, Alloc>::const_iterator it(v.begin()), it_end(v.end());
            it != it_end; ++it) {
            o.pack(*it);
        }
        return o;
    }
};

template <typename Key, typename Hash, typename Compare, typename Alloc>
struct object_with_zone<std::unordered_multiset<Key, Hash, Compare, Alloc>> {
    void operator()(msgpack::object::with_zone& o, const std::unordered_multiset<Key, Hash, Compare, Alloc>& v) const {
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
            typename std::unordered_multiset<Key, Hash, Compare, Alloc>::const_iterator it(v.begin());
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

#endif // MSGPACK_V1_TYPE_CPP11_UNORDERED_SET_HPP
