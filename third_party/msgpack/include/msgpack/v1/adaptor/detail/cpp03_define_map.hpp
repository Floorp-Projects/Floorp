//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_CPP03_DEFINE_MAP_HPP
#define MSGPACK_V1_CPP03_DEFINE_MAP_HPP

#include "msgpack/v1/adaptor/detail/cpp03_define_map_decl.hpp"
#include "msgpack/adaptor/msgpack_tuple.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/object_fwd.hpp"

#include <map>

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond
namespace type {


template <>
struct define_map<> {
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(0);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone&) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = MSGPACK_NULLPTR;
        o->via.map.size = 0;
    }
};

/// @cond

template <typename A0, typename A1>
struct define_map<A0, A1> {
    define_map(A0& _a0, A1& _a1) :
        a0(_a0), a1(_a1) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(1);
        
        pk.pack(a0);
        pk.pack(a1);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*1, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 1;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
    }
    
    A0& a0;
    A1& a1;
};

template <typename A0, typename A1, typename A2, typename A3>
struct define_map<A0, A1, A2, A3> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(2);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*2, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 2;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
struct define_map<A0, A1, A2, A3, A4, A5> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(3);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*3, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 3;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(4);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*4, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 4;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(5);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*5, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 5;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(6);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*6, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 6;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(7);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*7, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 7;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(8);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*8, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 8;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(9);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*9, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 9;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(10);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
        pk.pack(a18);
        pk.pack(a19);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a18);
            if (it != kvmap.end()) {
                it->second->convert(a19);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*10, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 10;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
        o->via.map.ptr[9].key = msgpack::object(a18, z);
        o->via.map.ptr[9].val = msgpack::object(a19, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
    A18& a18;
    A19& a19;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(11);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
        pk.pack(a18);
        pk.pack(a19);
        pk.pack(a20);
        pk.pack(a21);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a18);
            if (it != kvmap.end()) {
                it->second->convert(a19);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a20);
            if (it != kvmap.end()) {
                it->second->convert(a21);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*11, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 11;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
        o->via.map.ptr[9].key = msgpack::object(a18, z);
        o->via.map.ptr[9].val = msgpack::object(a19, z);
        
        o->via.map.ptr[10].key = msgpack::object(a20, z);
        o->via.map.ptr[10].val = msgpack::object(a21, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
    A18& a18;
    A19& a19;
    A20& a20;
    A21& a21;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(12);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
        pk.pack(a18);
        pk.pack(a19);
        pk.pack(a20);
        pk.pack(a21);
        pk.pack(a22);
        pk.pack(a23);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a18);
            if (it != kvmap.end()) {
                it->second->convert(a19);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a20);
            if (it != kvmap.end()) {
                it->second->convert(a21);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a22);
            if (it != kvmap.end()) {
                it->second->convert(a23);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*12, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 12;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
        o->via.map.ptr[9].key = msgpack::object(a18, z);
        o->via.map.ptr[9].val = msgpack::object(a19, z);
        
        o->via.map.ptr[10].key = msgpack::object(a20, z);
        o->via.map.ptr[10].val = msgpack::object(a21, z);
        
        o->via.map.ptr[11].key = msgpack::object(a22, z);
        o->via.map.ptr[11].val = msgpack::object(a23, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
    A18& a18;
    A19& a19;
    A20& a20;
    A21& a21;
    A22& a22;
    A23& a23;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(13);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
        pk.pack(a18);
        pk.pack(a19);
        pk.pack(a20);
        pk.pack(a21);
        pk.pack(a22);
        pk.pack(a23);
        pk.pack(a24);
        pk.pack(a25);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a18);
            if (it != kvmap.end()) {
                it->second->convert(a19);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a20);
            if (it != kvmap.end()) {
                it->second->convert(a21);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a22);
            if (it != kvmap.end()) {
                it->second->convert(a23);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a24);
            if (it != kvmap.end()) {
                it->second->convert(a25);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*13, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 13;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
        o->via.map.ptr[9].key = msgpack::object(a18, z);
        o->via.map.ptr[9].val = msgpack::object(a19, z);
        
        o->via.map.ptr[10].key = msgpack::object(a20, z);
        o->via.map.ptr[10].val = msgpack::object(a21, z);
        
        o->via.map.ptr[11].key = msgpack::object(a22, z);
        o->via.map.ptr[11].val = msgpack::object(a23, z);
        
        o->via.map.ptr[12].key = msgpack::object(a24, z);
        o->via.map.ptr[12].val = msgpack::object(a25, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
    A18& a18;
    A19& a19;
    A20& a20;
    A21& a21;
    A22& a22;
    A23& a23;
    A24& a24;
    A25& a25;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(14);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
        pk.pack(a18);
        pk.pack(a19);
        pk.pack(a20);
        pk.pack(a21);
        pk.pack(a22);
        pk.pack(a23);
        pk.pack(a24);
        pk.pack(a25);
        pk.pack(a26);
        pk.pack(a27);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a18);
            if (it != kvmap.end()) {
                it->second->convert(a19);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a20);
            if (it != kvmap.end()) {
                it->second->convert(a21);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a22);
            if (it != kvmap.end()) {
                it->second->convert(a23);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a24);
            if (it != kvmap.end()) {
                it->second->convert(a25);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a26);
            if (it != kvmap.end()) {
                it->second->convert(a27);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*14, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 14;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
        o->via.map.ptr[9].key = msgpack::object(a18, z);
        o->via.map.ptr[9].val = msgpack::object(a19, z);
        
        o->via.map.ptr[10].key = msgpack::object(a20, z);
        o->via.map.ptr[10].val = msgpack::object(a21, z);
        
        o->via.map.ptr[11].key = msgpack::object(a22, z);
        o->via.map.ptr[11].val = msgpack::object(a23, z);
        
        o->via.map.ptr[12].key = msgpack::object(a24, z);
        o->via.map.ptr[12].val = msgpack::object(a25, z);
        
        o->via.map.ptr[13].key = msgpack::object(a26, z);
        o->via.map.ptr[13].val = msgpack::object(a27, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
    A18& a18;
    A19& a19;
    A20& a20;
    A21& a21;
    A22& a22;
    A23& a23;
    A24& a24;
    A25& a25;
    A26& a26;
    A27& a27;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27, A28& _a28, A29& _a29) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27), a28(_a28), a29(_a29) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(15);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
        pk.pack(a18);
        pk.pack(a19);
        pk.pack(a20);
        pk.pack(a21);
        pk.pack(a22);
        pk.pack(a23);
        pk.pack(a24);
        pk.pack(a25);
        pk.pack(a26);
        pk.pack(a27);
        pk.pack(a28);
        pk.pack(a29);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a18);
            if (it != kvmap.end()) {
                it->second->convert(a19);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a20);
            if (it != kvmap.end()) {
                it->second->convert(a21);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a22);
            if (it != kvmap.end()) {
                it->second->convert(a23);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a24);
            if (it != kvmap.end()) {
                it->second->convert(a25);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a26);
            if (it != kvmap.end()) {
                it->second->convert(a27);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a28);
            if (it != kvmap.end()) {
                it->second->convert(a29);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*15, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 15;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
        o->via.map.ptr[9].key = msgpack::object(a18, z);
        o->via.map.ptr[9].val = msgpack::object(a19, z);
        
        o->via.map.ptr[10].key = msgpack::object(a20, z);
        o->via.map.ptr[10].val = msgpack::object(a21, z);
        
        o->via.map.ptr[11].key = msgpack::object(a22, z);
        o->via.map.ptr[11].val = msgpack::object(a23, z);
        
        o->via.map.ptr[12].key = msgpack::object(a24, z);
        o->via.map.ptr[12].val = msgpack::object(a25, z);
        
        o->via.map.ptr[13].key = msgpack::object(a26, z);
        o->via.map.ptr[13].val = msgpack::object(a27, z);
        
        o->via.map.ptr[14].key = msgpack::object(a28, z);
        o->via.map.ptr[14].val = msgpack::object(a29, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
    A18& a18;
    A19& a19;
    A20& a20;
    A21& a21;
    A22& a22;
    A23& a23;
    A24& a24;
    A25& a25;
    A26& a26;
    A27& a27;
    A28& a28;
    A29& a29;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29, typename A30, typename A31>
struct define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31> {
    define_map(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27, A28& _a28, A29& _a29, A30& _a30, A31& _a31) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27), a28(_a28), a29(_a29), a30(_a30), a31(_a31) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_map(16);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
        pk.pack(a9);
        pk.pack(a10);
        pk.pack(a11);
        pk.pack(a12);
        pk.pack(a13);
        pk.pack(a14);
        pk.pack(a15);
        pk.pack(a16);
        pk.pack(a17);
        pk.pack(a18);
        pk.pack(a19);
        pk.pack(a20);
        pk.pack(a21);
        pk.pack(a22);
        pk.pack(a23);
        pk.pack(a24);
        pk.pack(a25);
        pk.pack(a26);
        pk.pack(a27);
        pk.pack(a28);
        pk.pack(a29);
        pk.pack(a30);
        pk.pack(a31);
    }
    void msgpack_unpack(msgpack::object const& o) const
    {
        if(o.type != msgpack::type::MAP) { throw msgpack::type_error(); }
        std::map<std::string, msgpack::object const*> kvmap;
        for (uint32_t i = 0; i < o.via.map.size; ++i) {
            if (o.via.map.ptr[i].key.type != msgpack::type::STR) { throw msgpack::type_error(); }
            kvmap.insert(
                std::map<std::string, msgpack::object const*>::value_type(
                    std::string(
                        o.via.map.ptr[i].key.via.str.ptr,
                        o.via.map.ptr[i].key.via.str.size),
                    &o.via.map.ptr[i].val
                )
            );
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a0);
            if (it != kvmap.end()) {
                it->second->convert(a1);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a2);
            if (it != kvmap.end()) {
                it->second->convert(a3);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a4);
            if (it != kvmap.end()) {
                it->second->convert(a5);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a6);
            if (it != kvmap.end()) {
                it->second->convert(a7);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a8);
            if (it != kvmap.end()) {
                it->second->convert(a9);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a10);
            if (it != kvmap.end()) {
                it->second->convert(a11);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a12);
            if (it != kvmap.end()) {
                it->second->convert(a13);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a14);
            if (it != kvmap.end()) {
                it->second->convert(a15);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a16);
            if (it != kvmap.end()) {
                it->second->convert(a17);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a18);
            if (it != kvmap.end()) {
                it->second->convert(a19);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a20);
            if (it != kvmap.end()) {
                it->second->convert(a21);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a22);
            if (it != kvmap.end()) {
                it->second->convert(a23);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a24);
            if (it != kvmap.end()) {
                it->second->convert(a25);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a26);
            if (it != kvmap.end()) {
                it->second->convert(a27);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a28);
            if (it != kvmap.end()) {
                it->second->convert(a29);
            }
        }
        
        {
            std::map<std::string, msgpack::object const*>::const_iterator it = kvmap.find(a30);
            if (it != kvmap.end()) {
                it->second->convert(a31);
            }
        }
        
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv)*16, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = 16;
        
        o->via.map.ptr[0].key = msgpack::object(a0, z);
        o->via.map.ptr[0].val = msgpack::object(a1, z);
        
        o->via.map.ptr[1].key = msgpack::object(a2, z);
        o->via.map.ptr[1].val = msgpack::object(a3, z);
        
        o->via.map.ptr[2].key = msgpack::object(a4, z);
        o->via.map.ptr[2].val = msgpack::object(a5, z);
        
        o->via.map.ptr[3].key = msgpack::object(a6, z);
        o->via.map.ptr[3].val = msgpack::object(a7, z);
        
        o->via.map.ptr[4].key = msgpack::object(a8, z);
        o->via.map.ptr[4].val = msgpack::object(a9, z);
        
        o->via.map.ptr[5].key = msgpack::object(a10, z);
        o->via.map.ptr[5].val = msgpack::object(a11, z);
        
        o->via.map.ptr[6].key = msgpack::object(a12, z);
        o->via.map.ptr[6].val = msgpack::object(a13, z);
        
        o->via.map.ptr[7].key = msgpack::object(a14, z);
        o->via.map.ptr[7].val = msgpack::object(a15, z);
        
        o->via.map.ptr[8].key = msgpack::object(a16, z);
        o->via.map.ptr[8].val = msgpack::object(a17, z);
        
        o->via.map.ptr[9].key = msgpack::object(a18, z);
        o->via.map.ptr[9].val = msgpack::object(a19, z);
        
        o->via.map.ptr[10].key = msgpack::object(a20, z);
        o->via.map.ptr[10].val = msgpack::object(a21, z);
        
        o->via.map.ptr[11].key = msgpack::object(a22, z);
        o->via.map.ptr[11].val = msgpack::object(a23, z);
        
        o->via.map.ptr[12].key = msgpack::object(a24, z);
        o->via.map.ptr[12].val = msgpack::object(a25, z);
        
        o->via.map.ptr[13].key = msgpack::object(a26, z);
        o->via.map.ptr[13].val = msgpack::object(a27, z);
        
        o->via.map.ptr[14].key = msgpack::object(a28, z);
        o->via.map.ptr[14].val = msgpack::object(a29, z);
        
        o->via.map.ptr[15].key = msgpack::object(a30, z);
        o->via.map.ptr[15].val = msgpack::object(a31, z);
        
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
    A7& a7;
    A8& a8;
    A9& a9;
    A10& a10;
    A11& a11;
    A12& a12;
    A13& a13;
    A14& a14;
    A15& a15;
    A16& a16;
    A17& a17;
    A18& a18;
    A19& a19;
    A20& a20;
    A21& a21;
    A22& a22;
    A23& a23;
    A24& a24;
    A25& a25;
    A26& a26;
    A27& a27;
    A28& a28;
    A29& a29;
    A30& a30;
    A31& a31;
};

/// @endcond

inline define_map<> make_define_map()
{
    return define_map<>();
}

/// @cond

template <typename A0>
inline define_map<A0> make_define_map(A0& a0)
{
    return define_map<A0>(a0);
}

template <typename A0, typename A1>
inline define_map<A0, A1> make_define_map(A0& a0, A1& a1)
{
    return define_map<A0, A1>(a0, a1);
}

template <typename A0, typename A1, typename A2>
inline define_map<A0, A1, A2> make_define_map(A0& a0, A1& a1, A2& a2)
{
    return define_map<A0, A1, A2>(a0, a1, a2);
}

template <typename A0, typename A1, typename A2, typename A3>
inline define_map<A0, A1, A2, A3> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3)
{
    return define_map<A0, A1, A2, A3>(a0, a1, a2, a3);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4>
inline define_map<A0, A1, A2, A3, A4> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4)
{
    return define_map<A0, A1, A2, A3, A4>(a0, a1, a2, a3, a4);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
inline define_map<A0, A1, A2, A3, A4, A5> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5)
{
    return define_map<A0, A1, A2, A3, A4, A5>(a0, a1, a2, a3, a4, a5);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline define_map<A0, A1, A2, A3, A4, A5, A6> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6>(a0, a1, a2, a3, a4, a5, a6);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7>(a0, a1, a2, a3, a4, a5, a6, a7);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8>(a0, a1, a2, a3, a4, a5, a6, a7, a8);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28, A29& a29)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29, typename A30>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28, A29& a29, A30& a30)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29, typename A30, typename A31>
inline define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31> make_define_map(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28, A29& a29, A30& a30, A31& a31)
{
    return define_map<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31);
}

/// @endcond

}  // namespace type
/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond
}  // namespace msgpack

#endif // MSGPACK_V1_CPP03_DEFINE_MAP_HPP
