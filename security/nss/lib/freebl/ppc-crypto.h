/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PPC_CRYPTO_H
#define PPC_CRYPTO_H 1

#if defined(__powerpc64__) && defined(__ALTIVEC__) && \
    !defined(NSS_DISABLE_ALTIVEC)
#include "altivec-types.h"

/* The ghash freebl test tries to use this in C++, and gcc defines conflict. */
#ifdef __cplusplus
#undef pixel
#undef vector
#undef bool
#endif

/*
 * PPC CRYPTO requires at least gcc 8 or clang. The LE check is purely
 * because it's only been tested on LE. If you're interested in BE,
 * please send a patch.
 */
#if (defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 8)) && \
    defined(IS_LITTLE_ENDIAN) && defined(__VSX__)
#define USE_PPC_CRYPTO
#endif

#endif /* defined(__powerpc64__) && !defined(NSS_DISABLE_ALTIVEC) && defined(__ALTIVEC__) */

#endif
