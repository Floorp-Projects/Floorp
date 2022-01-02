//
// MessagePack for C++ memory pool
//
// Copyright (C) 2008-2018 FURUHASHI Sadayuki and KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef MSGPACK_V3_ZONE_DECL_HPP
#define MSGPACK_V3_ZONE_DECL_HPP

#include "msgpack/cpp_config.hpp"

#if defined(MSGPACK_USE_CPP03)
#include "msgpack/v3/detail/cpp03_zone_decl.hpp"
#else  // MSGPACK_USE_CPP03
#include "msgpack/v3/detail/cpp11_zone_decl.hpp"
#endif // MSGPACK_USE_CPP03

#endif // MSGPACK_V3_ZONE_DECL_HPP
