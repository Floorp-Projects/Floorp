//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V1_TYPE_EXT_DECL_HPP
#define MSGPACK_V1_TYPE_EXT_DECL_HPP

#include "msgpack/versioning.hpp"
#include "msgpack/adaptor/adaptor_base.hpp"
#include <cstring>
#include <string>
#include <cassert>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

namespace type {

class ext_ref;
class ext;

} // namespace type

/// @cond
} // MSGPACK_API_VERSION_NAMESPACE(v1)
/// @endcond

} // namespace msgpack

#endif // MSGPACK_V1_TYPE_EXT_DECL_HPP
