/*
 * MessagePack for C FILE* buffer adaptor
 *
 * Copyright (C) 2013 Vladimir Volodko
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef MSGPACK_FBUFFER_H
#define MSGPACK_FBUFFER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup msgpack_fbuffer FILE* buffer
 * @ingroup msgpack_buffer
 * @{
 */

static inline int msgpack_fbuffer_write(void* data, const char* buf, size_t len)
{
    return (len == fwrite(buf, len, 1, (FILE *)data)) ? 0 : -1;
}

/** @} */


#ifdef __cplusplus
}
#endif

#endif /* msgpack/fbuffer.h */
