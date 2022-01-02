/*
 * MessagePack for C++ version information
 *
 * Copyright (C) 2008-2013 FURUHASHI Sadayuki and Takatoshi Kondo
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef MSGPACK_VERSION_HPP
#define MSGPACK_VERSION_HPP

#include "version_master.h"

#define MSGPACK_STR(v) #v
#define MSGPACK_VERSION_I(maj, min, rev) MSGPACK_STR(maj) "." MSGPACK_STR(min) "." MSGPACK_STR(rev)

#define MSGPACK_VERSION MSGPACK_VERSION_I(MSGPACK_VERSION_MAJOR, MSGPACK_VERSION_MINOR, MSGPACK_VERSION_REVISION)

inline const char* msgpack_version(void) {
    return MSGPACK_VERSION;
}

inline int msgpack_version_major(void) {
    return MSGPACK_VERSION_MAJOR;
}

inline int msgpack_version_minor(void) {
    return MSGPACK_VERSION_MINOR;
}

inline int msgpack_version_revision(void) {
    return MSGPACK_VERSION_REVISION;
}

#endif /* msgpack/version.hpp */
