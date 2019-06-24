/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

/* This file holds useful functions and macros for crypto code. */
#include "crypto_primitives.h"

/*
 * FREEBL_HTONLL(x): swap bytes in a 64-bit integer.
 */
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__x86_64))

__inline__ PRUint64
swap8b(PRUint64 value)
{
    __asm__("bswapq %0"
            : "+r"(value));
    return (value);
}

#elif defined(IS_LITTLE_ENDIAN) && !defined(_MSC_VER) && !__has_builtin(__builtin_bswap64) && !((defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))))

PRUint64
swap8b(PRUint64 x)
{
    PRUint64 t1 = x;
    t1 = ((t1 & SHA_MASK8) << 8) | ((t1 >> 8) & SHA_MASK8);
    t1 = ((t1 & SHA_MASK16) << 16) | ((t1 >> 16) & SHA_MASK16);
    return (t1 >> 32) | (t1 << 32);
}

#endif
