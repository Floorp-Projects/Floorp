//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2008-2016 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_CPP03_DEFINE_ARRAY_HPP
#define MSGPACK_V1_CPP03_DEFINE_ARRAY_HPP

#include "msgpack/v1/adaptor/detail/cpp03_define_array_decl.hpp"
#include "msgpack/adaptor/msgpack_tuple.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include "msgpack/object_fwd.hpp"

namespace msgpack {
/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond
namespace type {


template <>
struct define_array<> {
    typedef define_array<> value_type;
    typedef tuple<> tuple_type;
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
        o->via.array.ptr = MSGPACK_NULLPTR;
        o->via.array.size = 0;
    }
};

/// @cond

template <typename A0>
struct define_array<A0> {
    typedef define_array<A0> value_type;
    typedef tuple<A0> tuple_type;
    define_array(A0& _a0) :
        a0(_a0) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(1);
        
        pk.pack(a0);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*1, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 1;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
    }
    
    A0& a0;
};

template <typename A0, typename A1>
struct define_array<A0, A1> {
    typedef define_array<A0, A1> value_type;
    typedef tuple<A0, A1> tuple_type;
    define_array(A0& _a0, A1& _a1) :
        a0(_a0), a1(_a1) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(2);
        
        pk.pack(a0);
        pk.pack(a1);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*2, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 2;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
    }
    
    A0& a0;
    A1& a1;
};

template <typename A0, typename A1, typename A2>
struct define_array<A0, A1, A2> {
    typedef define_array<A0, A1, A2> value_type;
    typedef tuple<A0, A1, A2> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2) :
        a0(_a0), a1(_a1), a2(_a2) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(3);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*3, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 3;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
};

template <typename A0, typename A1, typename A2, typename A3>
struct define_array<A0, A1, A2, A3> {
    typedef define_array<A0, A1, A2, A3> value_type;
    typedef tuple<A0, A1, A2, A3> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(4);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*4, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 4;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4>
struct define_array<A0, A1, A2, A3, A4> {
    typedef define_array<A0, A1, A2, A3, A4> value_type;
    typedef tuple<A0, A1, A2, A3, A4> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(5);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*5, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 5;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
struct define_array<A0, A1, A2, A3, A4, A5> {
    typedef define_array<A0, A1, A2, A3, A4, A5> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(6);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*6, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 6;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
struct define_array<A0, A1, A2, A3, A4, A5, A6> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(7);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*7, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 7;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
    }
    
    A0& a0;
    A1& a1;
    A2& a2;
    A3& a3;
    A4& a4;
    A5& a5;
    A6& a6;
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(8);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*8, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 8;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(9);
        
        pk.pack(a0);
        pk.pack(a1);
        pk.pack(a2);
        pk.pack(a3);
        pk.pack(a4);
        pk.pack(a5);
        pk.pack(a6);
        pk.pack(a7);
        pk.pack(a8);
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*9, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 9;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(10);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*10, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 10;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(11);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*11, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 11;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(12);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*12, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 12;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(13);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*13, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 13;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(14);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*14, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 14;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(15);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*15, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 15;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(16);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*16, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 16;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(17);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*17, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 17;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(18);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*18, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 18;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(19);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*19, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 19;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(20);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*20, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 20;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(21);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*21, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 21;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(22);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*22, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 22;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(23);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*23, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 23;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(24);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*24, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 24;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(25);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*25, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 25;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(26);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 26: ptr[25].convert(a25);
            // fallthrough

            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*26, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 26;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
        o->via.array.ptr[25] = msgpack::object(a25, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(27);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 27: ptr[26].convert(a26);
            // fallthrough

            case 26: ptr[25].convert(a25);
            // fallthrough

            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*27, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 27;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
        o->via.array.ptr[25] = msgpack::object(a25, z);
        o->via.array.ptr[26] = msgpack::object(a26, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(28);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 28: ptr[27].convert(a27);
            // fallthrough

            case 27: ptr[26].convert(a26);
            // fallthrough

            case 26: ptr[25].convert(a25);
            // fallthrough

            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*28, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 28;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
        o->via.array.ptr[25] = msgpack::object(a25, z);
        o->via.array.ptr[26] = msgpack::object(a26, z);
        o->via.array.ptr[27] = msgpack::object(a27, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27, A28& _a28) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27), a28(_a28) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(29);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 29: ptr[28].convert(a28);
            // fallthrough

            case 28: ptr[27].convert(a27);
            // fallthrough

            case 27: ptr[26].convert(a26);
            // fallthrough

            case 26: ptr[25].convert(a25);
            // fallthrough

            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*29, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 29;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
        o->via.array.ptr[25] = msgpack::object(a25, z);
        o->via.array.ptr[26] = msgpack::object(a26, z);
        o->via.array.ptr[27] = msgpack::object(a27, z);
        o->via.array.ptr[28] = msgpack::object(a28, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27, A28& _a28, A29& _a29) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27), a28(_a28), a29(_a29) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(30);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 30: ptr[29].convert(a29);
            // fallthrough

            case 29: ptr[28].convert(a28);
            // fallthrough

            case 28: ptr[27].convert(a27);
            // fallthrough

            case 27: ptr[26].convert(a26);
            // fallthrough

            case 26: ptr[25].convert(a25);
            // fallthrough

            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*30, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 30;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
        o->via.array.ptr[25] = msgpack::object(a25, z);
        o->via.array.ptr[26] = msgpack::object(a26, z);
        o->via.array.ptr[27] = msgpack::object(a27, z);
        o->via.array.ptr[28] = msgpack::object(a28, z);
        o->via.array.ptr[29] = msgpack::object(a29, z);
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

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29, typename A30>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27, A28& _a28, A29& _a29, A30& _a30) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27), a28(_a28), a29(_a29), a30(_a30) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(31);
        
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
    }
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 31: ptr[30].convert(a30);
            // fallthrough

            case 30: ptr[29].convert(a29);
            // fallthrough

            case 29: ptr[28].convert(a28);
            // fallthrough

            case 28: ptr[27].convert(a27);
            // fallthrough

            case 27: ptr[26].convert(a26);
            // fallthrough

            case 26: ptr[25].convert(a25);
            // fallthrough

            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*31, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 31;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
        o->via.array.ptr[25] = msgpack::object(a25, z);
        o->via.array.ptr[26] = msgpack::object(a26, z);
        o->via.array.ptr[27] = msgpack::object(a27, z);
        o->via.array.ptr[28] = msgpack::object(a28, z);
        o->via.array.ptr[29] = msgpack::object(a29, z);
        o->via.array.ptr[30] = msgpack::object(a30, z);
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
};

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29, typename A30, typename A31>
struct define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31> {
    typedef define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31> value_type;
    typedef tuple<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31> tuple_type;
    define_array(A0& _a0, A1& _a1, A2& _a2, A3& _a3, A4& _a4, A5& _a5, A6& _a6, A7& _a7, A8& _a8, A9& _a9, A10& _a10, A11& _a11, A12& _a12, A13& _a13, A14& _a14, A15& _a15, A16& _a16, A17& _a17, A18& _a18, A19& _a19, A20& _a20, A21& _a21, A22& _a22, A23& _a23, A24& _a24, A25& _a25, A26& _a26, A27& _a27, A28& _a28, A29& _a29, A30& _a30, A31& _a31) :
        a0(_a0), a1(_a1), a2(_a2), a3(_a3), a4(_a4), a5(_a5), a6(_a6), a7(_a7), a8(_a8), a9(_a9), a10(_a10), a11(_a11), a12(_a12), a13(_a13), a14(_a14), a15(_a15), a16(_a16), a17(_a17), a18(_a18), a19(_a19), a20(_a20), a21(_a21), a22(_a22), a23(_a23), a24(_a24), a25(_a25), a26(_a26), a27(_a27), a28(_a28), a29(_a29), a30(_a30), a31(_a31) {}
    template <typename Packer>
    void msgpack_pack(Packer& pk) const
    {
        pk.pack_array(32);
        
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
    void msgpack_unpack(msgpack::object const& o)
    {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        const size_t size = o.via.array.size;
        if(size > 0) {
            msgpack::object *ptr = o.via.array.ptr;
            switch(size) {
            default:
            case 32: ptr[31].convert(a31);
            // fallthrough

            case 31: ptr[30].convert(a30);
            // fallthrough

            case 30: ptr[29].convert(a29);
            // fallthrough

            case 29: ptr[28].convert(a28);
            // fallthrough

            case 28: ptr[27].convert(a27);
            // fallthrough

            case 27: ptr[26].convert(a26);
            // fallthrough

            case 26: ptr[25].convert(a25);
            // fallthrough

            case 25: ptr[24].convert(a24);
            // fallthrough

            case 24: ptr[23].convert(a23);
            // fallthrough

            case 23: ptr[22].convert(a22);
            // fallthrough

            case 22: ptr[21].convert(a21);
            // fallthrough

            case 21: ptr[20].convert(a20);
            // fallthrough

            case 20: ptr[19].convert(a19);
            // fallthrough

            case 19: ptr[18].convert(a18);
            // fallthrough

            case 18: ptr[17].convert(a17);
            // fallthrough

            case 17: ptr[16].convert(a16);
            // fallthrough

            case 16: ptr[15].convert(a15);
            // fallthrough

            case 15: ptr[14].convert(a14);
            // fallthrough

            case 14: ptr[13].convert(a13);
            // fallthrough

            case 13: ptr[12].convert(a12);
            // fallthrough

            case 12: ptr[11].convert(a11);
            // fallthrough

            case 11: ptr[10].convert(a10);
            // fallthrough

            case 10: ptr[9].convert(a9);
            // fallthrough

            case 9: ptr[8].convert(a8);
            // fallthrough

            case 8: ptr[7].convert(a7);
            // fallthrough

            case 7: ptr[6].convert(a6);
            // fallthrough

            case 6: ptr[5].convert(a5);
            // fallthrough

            case 5: ptr[4].convert(a4);
            // fallthrough

            case 4: ptr[3].convert(a3);
            // fallthrough

            case 3: ptr[2].convert(a2);
            // fallthrough

            case 2: ptr[1].convert(a1);
            // fallthrough

            case 1: ptr[0].convert(a0);
            // fallthrough

            }
        }
    }
    void msgpack_object(msgpack::object* o, msgpack::zone& z) const
    {
        o->type = msgpack::type::ARRAY;
        o->via.array.ptr = static_cast<msgpack::object*>(z.allocate_align(sizeof(msgpack::object)*32, MSGPACK_ZONE_ALIGNOF(msgpack::object)));
        o->via.array.size = 32;
        
        o->via.array.ptr[0] = msgpack::object(a0, z);
        o->via.array.ptr[1] = msgpack::object(a1, z);
        o->via.array.ptr[2] = msgpack::object(a2, z);
        o->via.array.ptr[3] = msgpack::object(a3, z);
        o->via.array.ptr[4] = msgpack::object(a4, z);
        o->via.array.ptr[5] = msgpack::object(a5, z);
        o->via.array.ptr[6] = msgpack::object(a6, z);
        o->via.array.ptr[7] = msgpack::object(a7, z);
        o->via.array.ptr[8] = msgpack::object(a8, z);
        o->via.array.ptr[9] = msgpack::object(a9, z);
        o->via.array.ptr[10] = msgpack::object(a10, z);
        o->via.array.ptr[11] = msgpack::object(a11, z);
        o->via.array.ptr[12] = msgpack::object(a12, z);
        o->via.array.ptr[13] = msgpack::object(a13, z);
        o->via.array.ptr[14] = msgpack::object(a14, z);
        o->via.array.ptr[15] = msgpack::object(a15, z);
        o->via.array.ptr[16] = msgpack::object(a16, z);
        o->via.array.ptr[17] = msgpack::object(a17, z);
        o->via.array.ptr[18] = msgpack::object(a18, z);
        o->via.array.ptr[19] = msgpack::object(a19, z);
        o->via.array.ptr[20] = msgpack::object(a20, z);
        o->via.array.ptr[21] = msgpack::object(a21, z);
        o->via.array.ptr[22] = msgpack::object(a22, z);
        o->via.array.ptr[23] = msgpack::object(a23, z);
        o->via.array.ptr[24] = msgpack::object(a24, z);
        o->via.array.ptr[25] = msgpack::object(a25, z);
        o->via.array.ptr[26] = msgpack::object(a26, z);
        o->via.array.ptr[27] = msgpack::object(a27, z);
        o->via.array.ptr[28] = msgpack::object(a28, z);
        o->via.array.ptr[29] = msgpack::object(a29, z);
        o->via.array.ptr[30] = msgpack::object(a30, z);
        o->via.array.ptr[31] = msgpack::object(a31, z);
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

inline define_array<> make_define_array()
{
    return define_array<>();
}

/// @cond

template <typename A0>
inline define_array<A0> make_define_array(A0& a0)
{
    return define_array<A0>(a0);
}

template <typename A0, typename A1>
inline define_array<A0, A1> make_define_array(A0& a0, A1& a1)
{
    return define_array<A0, A1>(a0, a1);
}

template <typename A0, typename A1, typename A2>
inline define_array<A0, A1, A2> make_define_array(A0& a0, A1& a1, A2& a2)
{
    return define_array<A0, A1, A2>(a0, a1, a2);
}

template <typename A0, typename A1, typename A2, typename A3>
inline define_array<A0, A1, A2, A3> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3)
{
    return define_array<A0, A1, A2, A3>(a0, a1, a2, a3);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4>
inline define_array<A0, A1, A2, A3, A4> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4)
{
    return define_array<A0, A1, A2, A3, A4>(a0, a1, a2, a3, a4);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
inline define_array<A0, A1, A2, A3, A4, A5> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5)
{
    return define_array<A0, A1, A2, A3, A4, A5>(a0, a1, a2, a3, a4, a5);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline define_array<A0, A1, A2, A3, A4, A5, A6> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6>(a0, a1, a2, a3, a4, a5, a6);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7>(a0, a1, a2, a3, a4, a5, a6, a7);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8>(a0, a1, a2, a3, a4, a5, a6, a7, a8);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28, A29& a29)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29, typename A30>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28, A29& a29, A30& a30)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30);
}

template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16, typename A17, typename A18, typename A19, typename A20, typename A21, typename A22, typename A23, typename A24, typename A25, typename A26, typename A27, typename A28, typename A29, typename A30, typename A31>
inline define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31> make_define_array(A0& a0, A1& a1, A2& a2, A3& a3, A4& a4, A5& a5, A6& a6, A7& a7, A8& a8, A9& a9, A10& a10, A11& a11, A12& a12, A13& a13, A14& a14, A15& a15, A16& a16, A17& a17, A18& a18, A19& a19, A20& a20, A21& a21, A22& a22, A23& a23, A24& a24, A25& a25, A26& a26, A27& a27, A28& a28, A29& a29, A30& a30, A31& a31)
{
    return define_array<A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31>(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31);
}

/// @endcond

}  // namespace type
/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond
}  // namespace msgpack

#endif // MSGPACK_V1_CPP03_DEFINE_ARRAY_HPP
