//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2008-2018 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_PARSE_RETURN_HPP
#define MSGPACK_V3_PARSE_RETURN_HPP

#include "msgpack/v2/parse_return.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v3) {
/// @endcond

using v2::parse_return;

using v2::PARSE_SUCCESS;
using v2::PARSE_EXTRA_BYTES;
using v2::PARSE_CONTINUE;
using v2::PARSE_PARSE_ERROR;
using v2::PARSE_STOP_VISITOR;

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v3)
/// @endcond

}  // namespace msgpack

#endif // MSGPACK_V3_PARSE_RETURN_HPP
