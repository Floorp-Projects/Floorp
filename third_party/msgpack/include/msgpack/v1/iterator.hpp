//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 MIZUKI Hirata
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_V1_ITERATOR_HPP
#define MSGPACK_V1_ITERATOR_HPP
#if !defined(MSGPACK_USE_CPP03)

#include "msgpack/v1/fbuffer_decl.hpp"

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

inline msgpack::object_kv* begin(msgpack::object_map &map) { return map.ptr; }
inline const msgpack::object_kv* begin(const msgpack::object_map &map) { return map.ptr; }
inline msgpack::object_kv* end(msgpack::object_map &map) { return map.ptr + map.size; }
inline const msgpack::object_kv* end(const msgpack::object_map &map) { return map.ptr + map.size; }

inline msgpack::object* begin(msgpack::object_array &array) { return array.ptr; }
inline const msgpack::object* begin(const msgpack::object_array &array) { return array.ptr; }
inline msgpack::object* end(msgpack::object_array &array) { return array.ptr + array.size; }
inline const msgpack::object* end(const msgpack::object_array &array) { return array.ptr + array.size; }

/// @cond
}
/// @endcond

}

#endif // !defined(MSGPACK_USE_CPP03)
#endif // MSGPACK_V1_ITERATOR_HPP
