/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file holds useful functions and macros for crypto code. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include <stdlib.h>
#include "prtypes.h"

/* Unfortunately this isn't always set when it should be. */
#if defined(HAVE_LONG_LONG)

/*
 * ROTR64/ROTL64(x, n): rotate a 64-bit integer x by n bites to the right/left.
 */
#if defined(_MSC_VER)
#pragma intrinsic(_rotr64, _rotl64)
#define ROTR64(x, n) _rotr64((x), (n))
#define ROTL64(x, n) _rotl64((x), (n))
#else
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define ROTL64(x, n) (((x) << (n)) | ((x) >> (64 - (n))))
#endif

/*
 * FREEBL_HTONLL(x): swap bytes in a 64-bit integer.
 */
#if defined(_MSC_VER)

#pragma intrinsic(_byteswap_uint64)
#define FREEBL_HTONLL(x) _byteswap_uint64(x)

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__x86_64))

PRUint64 swap8b(PRUint64 value);
#define FREEBL_HTONLL(x) swap8b(x)

#else

#define SHA_MASK16 0x0000FFFF0000FFFFULL
#define SHA_MASK8 0x00FF00FF00FF00FFULL
PRUint64 swap8b(PRUint64 x);
#define FREEBL_HTONLL(x) swap8b(x)

#endif /* _MSC_VER */

#endif /* HAVE_LONG_LONG */