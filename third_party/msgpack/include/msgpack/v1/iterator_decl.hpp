//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015-2016 MIZUKI Hirata
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_V1_ITERATOR_DECL_HPP
#define MSGPACK_V1_ITERATOR_DECL_HPP
#if !defined(MSGPACK_USE_CPP03)

#include <msgpack/object_fwd.hpp>

namespace msgpack {

/// @cond
MSGPACK_API_VERSION_NAMESPACE(v1) {
/// @endcond

msgpack::object_kv* begin(msgpack::object_map &map);
const msgpack::object_kv* begin(const msgpack::object_map &map);
msgpack::object_kv* end(msgpack::object_map &map);
const msgpack::object_kv* end(const msgpack::object_map &map);

msgpack::object* begin(msgpack::object_array &array);
const msgpack::object* begin(const msgpack::object_array &array);
msgpack::object* end(msgpack::object_array &array);
const msgpack::object* end(const msgpack::object_array &array);

/// @cond
}
/// @endcond

}

#endif // !defined(MSGPACK_USE_CPP03)
#endif // MSGPACK_V1_ITERATOR_DECL_HPP
