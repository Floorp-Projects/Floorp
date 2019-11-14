/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GCM_H
#define GCM_H 1

#include "blapii.h"
#include <stdint.h>

#ifdef NSS_X86_OR_X64
/* GCC <= 4.8 doesn't support including emmintrin.h without enabling SSE2 */
#if !defined(__clang__) && defined(__GNUC__) && defined(__GNUC_MINOR__) && \
    (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ <= 8))
#pragma GCC push_options
#pragma GCC target("sse2")
#undef NSS_DISABLE_SSE2
#define NSS_DISABLE_SSE2 1
#endif /* GCC <= 4.8 */

#include <emmintrin.h> /* __m128i */

#ifdef NSS_DISABLE_SSE2
#undef NSS_DISABLE_SSE2
#pragma GCC pop_options
#endif /* NSS_DISABLE_SSE2 */
#endif

#ifdef __aarch64__
#include <arm_neon.h>
#endif

SEC_BEGIN_PROTOS

#ifdef HAVE_INT128_SUPPORT
typedef unsigned __int128 uint128_t;
#endif

typedef struct GCMContextStr GCMContext;

/*
 * The context argument is the inner cipher context to use with cipher. The
 * GCMContext does not own context. context needs to remain valid for as long
 * as the GCMContext is valid.
 *
 * The cipher argument is a block cipher in the ECB encrypt mode.
 */
GCMContext *GCM_CreateContext(void *context, freeblCipherFunc cipher,
                              const unsigned char *params);
void GCM_DestroyContext(GCMContext *gcm, PRBool freeit);
SECStatus GCM_EncryptUpdate(GCMContext *gcm, unsigned char *outbuf,
                            unsigned int *outlen, unsigned int maxout,
                            const unsigned char *inbuf, unsigned int inlen,
                            unsigned int blocksize);
SECStatus GCM_DecryptUpdate(GCMContext *gcm, unsigned char *outbuf,
                            unsigned int *outlen, unsigned int maxout,
                            const unsigned char *inbuf, unsigned int inlen,
                            unsigned int blocksize);

/* These functions are here only so we can test them */
#define GCM_HASH_LEN_LEN 8 /* gcm hash defines lengths to be 64 bits */
typedef struct gcmHashContextStr gcmHashContext;
typedef SECStatus (*ghash_t)(gcmHashContext *, const unsigned char *,
                             unsigned int);
pre_align struct gcmHashContextStr {
#ifdef NSS_X86_OR_X64
    __m128i x, h;
#elif defined(__aarch64__)
    uint64x2_t x, h;
#endif
    uint64_t x_low, x_high, h_high, h_low;
    unsigned char buffer[MAX_BLOCK_SIZE];
    unsigned int bufLen;
    uint8_t counterBuf[16];
    uint64_t cLen;
    ghash_t ghash_mul;
    PRBool hw;
    gcmHashContext *mem;
} post_align;

SECStatus gcmHash_Update(gcmHashContext *ghash, const unsigned char *buf,
                         unsigned int len);
SECStatus gcmHash_InitContext(gcmHashContext *ghash, const unsigned char *H,
                              PRBool sw);
SECStatus gcmHash_Reset(gcmHashContext *ghash, const unsigned char *AAD,
                        unsigned int AADLen);
SECStatus gcmHash_Final(gcmHashContext *ghash, unsigned char *outbuf,
                        unsigned int *outlen, unsigned int maxout);

SEC_END_PROTOS

#endif
