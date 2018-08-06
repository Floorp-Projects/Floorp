//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_CPP11_MSGPACK_TUPLE_DECL_HPP
#define MSGPACK_V2_CPP11_MSGPACK_TUPLE_DECL_HPP

#include "msgpack/v1/adaptor/detail/cpp11_msgpack_tuple_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

namespace type {

using v1::type::get;
using v1::type::tuple_size;
using v1::type::tuple_element;
using v1::type::uses_allocator;
using v1::type::ignore;
using v1::type::swap;

using v1::type::tuple;

using v1::type::make_tuple;
using v1::type::forward_as_tuple;
using v1::type::tuple_cat;
using v1::type::tie;

} // namespace type

// --- Pack from tuple to packer stream ---

using v1::MsgpackTuplePacker;

// --- Convert from tuple to object ---
using v1::MsgpackTupleAs;

using v1::MsgpackTupleAsImpl;

using v1::MsgpackTupleConverter;

// --- Convert from tuple to object with zone ---
using v1::MsgpackTupleToObjectWithZone;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
///@endcond

}  // namespace msgpack

#endif // MSGPACK_V2_CPP11_MSGPACK_TUPLE_DECL_HPP
