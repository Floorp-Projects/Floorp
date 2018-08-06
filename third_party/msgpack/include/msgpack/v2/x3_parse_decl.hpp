//
// MessagePack for C++ deserializing routine
//
// Copyright (C) 2017 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V2_X3_PARSE_DECL_HPP
#define MSGPACK_V2_X3_PARSE_DECL_HPP

#if defined(MSGPACK_USE_X3_PARSE)

#include "msgpack/versioning.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v2) {
/// @endcond


template <typename Iterator, typename Visitor>
bool parse(Iterator&& begin, Iterator&& end, Visitor&& vis);

/// @cond
}  // MSGPACK_API_VERSION_NAMESPACE(v2)
/// @endcond

}  // namespace msgpack


#endif // defined(MSGPACK_USE_X3_PARSE)

#endif // MSGPACK_V2_X3_PARSE_DECL_HPP
