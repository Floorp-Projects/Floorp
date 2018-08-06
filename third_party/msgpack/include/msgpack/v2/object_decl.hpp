//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_OBJECT_DECL_HPP
#define MSGPACK_V2_OBJECT_DECL_HPP

#include "msgpack/v1/object_decl.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond

using v1::object_handle;

namespace detail {

using v1::detail::add_ext_type_size;

} // namespace detail

using v1::aligned_zone_size;

using v1::clone;

namespace detail {

using v1::detail::packer_serializer;

} // namespace detail

using v1::object_parser;
using v1::object_pack_visitor;
using v1::object_stringize_visitor;

using v1::operator==;
using v1::operator!=;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V2_OBJECT_DECL_HPP
