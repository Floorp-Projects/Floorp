//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2018 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_OBJECT_DECL_HPP
#define MSGPACK_V3_OBJECT_DECL_HPP

#include "msgpack/v2/object_decl.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

using v2::object_handle;

namespace detail {

using v2::detail::add_ext_type_size;

} // namespace detail

using v2::aligned_zone_size;

using v2::clone;

namespace detail {

using v2::detail::packer_serializer;

} // namespace detail

using v2::object_parser;
using v2::object_pack_visitor;
using v2::object_stringize_visitor;

using v2::operator==;
using v2::operator!=;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V3_OBJECT_DECL_HPP
