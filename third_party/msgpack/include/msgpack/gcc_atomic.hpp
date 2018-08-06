//
// MessagePack for C++ old gcc workaround for atomic operation
//
// Copyright (C) 2008-2013 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MSGPACK_GCC_ATOMIC_HPP
#define MSGPACK_GCC_ATOMIC_HPP

#if defined(__GNUC__) && ((__GNUC__*10 + __GNUC_MINOR__) < 41)

#include "msgpack/gcc_atomic.h"
#include <bits/atomicity.h>

int _msgpack_sync_decr_and_fetch(volatile _msgpack_atomic_counter_t* ptr)
{
    return  __gnu_cxx::__exchange_and_add(ptr, -1) - 1;
}

int _msgpack_sync_incr_and_fetch(volatile _msgpack_atomic_counter_t* ptr)
{
    return  __gnu_cxx::__exchange_and_add(ptr, 1) + 1;
}

#endif // old gcc workaround

#endif /* gcc_atomic.hpp */
