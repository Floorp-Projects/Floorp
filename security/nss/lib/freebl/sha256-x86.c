/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef USE_HW_SHA2

#include <immintrin.h>

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapii.h"
#include "prcpucfg.h"
#include "prtypes.h" /* for PRUintXX */
#include "prlong.h"
#include "blapi.h"
#include "sha256.h"

/* SHA-256 constants, K256. */
pre_align static const PRUint32 K256[64] post_align = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROUND(n, a, b, c, d)                                \
    {                                                       \
        __m128i t = _mm_add_epi32(a, k##n);                 \
        w1 = _mm_sha256rnds2_epu32(w1, w0, t);              \
        t = _mm_shuffle_epi32(t, 0x0e);                     \
        w0 = _mm_sha256rnds2_epu32(w0, w1, t);              \
        if (n < 12) {                                       \
            a = _mm_sha256msg1_epu32(a, b);                 \
            a = _mm_add_epi32(a, _mm_alignr_epi8(d, c, 4)); \
            a = _mm_sha256msg2_epu32(a, d);                 \
        }                                                   \
    }

void
SHA256_Compress_Native(SHA256Context *ctx)
{
    __m128i h0, h1, th;
    __m128i a, b, c, d;
    __m128i w0, w1;
    const __m128i shuffle = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);

    const __m128i *K = (__m128i *)K256;
    const __m128i k0 = _mm_load_si128(K);
    const __m128i k1 = _mm_load_si128(K + 1);
    const __m128i k2 = _mm_load_si128(K + 2);
    const __m128i k3 = _mm_load_si128(K + 3);
    const __m128i k4 = _mm_load_si128(K + 4);
    const __m128i k5 = _mm_load_si128(K + 5);
    const __m128i k6 = _mm_load_si128(K + 6);
    const __m128i k7 = _mm_load_si128(K + 7);
    const __m128i k8 = _mm_load_si128(K + 8);
    const __m128i k9 = _mm_load_si128(K + 9);
    const __m128i k10 = _mm_load_si128(K + 10);
    const __m128i k11 = _mm_load_si128(K + 11);
    const __m128i k12 = _mm_load_si128(K + 12);
    const __m128i k13 = _mm_load_si128(K + 13);
    const __m128i k14 = _mm_load_si128(K + 14);
    const __m128i k15 = _mm_load_si128(K + 15);

    const __m128i *input = (__m128i *)ctx->u.b;

    h0 = _mm_loadu_si128((__m128i *)(ctx->h));
    h1 = _mm_loadu_si128((__m128i *)(ctx->h + 4));

    /* H0123:4567 -> H01256:H2367 */
    th = _mm_shuffle_epi32(h0, 0xb1);
    h1 = _mm_shuffle_epi32(h1, 0x1b);
    h0 = _mm_alignr_epi8(th, h1, 8);
    h1 = _mm_blend_epi16(h1, th, 0xf0);

    a = _mm_shuffle_epi8(_mm_loadu_si128(input), shuffle);
    b = _mm_shuffle_epi8(_mm_loadu_si128(input + 1), shuffle);
    c = _mm_shuffle_epi8(_mm_loadu_si128(input + 2), shuffle);
    d = _mm_shuffle_epi8(_mm_loadu_si128(input + 3), shuffle);

    w0 = h0;
    w1 = h1;

    ROUND(0, a, b, c, d)
    ROUND(1, b, c, d, a)
    ROUND(2, c, d, a, b)
    ROUND(3, d, a, b, c)
    ROUND(4, a, b, c, d)
    ROUND(5, b, c, d, a)
    ROUND(6, c, d, a, b)
    ROUND(7, d, a, b, c)
    ROUND(8, a, b, c, d)
    ROUND(9, b, c, d, a)
    ROUND(10, c, d, a, b)
    ROUND(11, d, a, b, c)
    ROUND(12, a, b, c, d)
    ROUND(13, b, c, d, a)
    ROUND(14, c, d, a, b)
    ROUND(15, d, a, b, c)

    h0 = _mm_add_epi32(h0, w0);
    h1 = _mm_add_epi32(h1, w1);

    /* H0145:2367 -> H0123:4567 */
    th = _mm_shuffle_epi32(h0, 0x1b);
    h1 = _mm_shuffle_epi32(h1, 0xb1);
    h0 = _mm_blend_epi16(th, h1, 0xf0);
    h1 = _mm_alignr_epi8(h1, th, 8);

    _mm_storeu_si128((__m128i *)ctx->h, h0);
    _mm_storeu_si128((__m128i *)(ctx->h + 4), h1);
}

void
SHA256_Update_Native(SHA256Context *ctx, const unsigned char *input,
                     unsigned int inputLen)
{
    __m128i h0, h1, th;
    const __m128i shuffle = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);

    const __m128i *K = (__m128i *)K256;
    const __m128i k0 = _mm_load_si128(K);
    const __m128i k1 = _mm_load_si128(K + 1);
    const __m128i k2 = _mm_load_si128(K + 2);
    const __m128i k3 = _mm_load_si128(K + 3);
    const __m128i k4 = _mm_load_si128(K + 4);
    const __m128i k5 = _mm_load_si128(K + 5);
    const __m128i k6 = _mm_load_si128(K + 6);
    const __m128i k7 = _mm_load_si128(K + 7);
    const __m128i k8 = _mm_load_si128(K + 8);
    const __m128i k9 = _mm_load_si128(K + 9);
    const __m128i k10 = _mm_load_si128(K + 10);
    const __m128i k11 = _mm_load_si128(K + 11);
    const __m128i k12 = _mm_load_si128(K + 12);
    const __m128i k13 = _mm_load_si128(K + 13);
    const __m128i k14 = _mm_load_si128(K + 14);
    const __m128i k15 = _mm_load_si128(K + 15);

    unsigned int inBuf = ctx->sizeLo & 0x3f;
    if (!inputLen) {
        return;
    }

    /* Add inputLen into the count of bytes processed, before processing */
    if ((ctx->sizeLo += inputLen) < inputLen) {
        ctx->sizeHi++;
    }

    /* if data already in buffer, attempt to fill rest of buffer */
    if (inBuf) {
        unsigned int todo = SHA256_BLOCK_LENGTH - inBuf;
        if (inputLen < todo) {
            todo = inputLen;
        }
        memcpy(ctx->u.b + inBuf, input, todo);
        input += todo;
        inputLen -= todo;
        if (inBuf + todo == SHA256_BLOCK_LENGTH) {
            SHA256_Compress_Native(ctx);
        }
    }

    h0 = _mm_loadu_si128((__m128i *)(ctx->h));
    h1 = _mm_loadu_si128((__m128i *)(ctx->h + 4));

    /* H0123:4567 -> H01256:H2367 */
    th = _mm_shuffle_epi32(h0, 0xb1);
    h1 = _mm_shuffle_epi32(h1, 0x1b);
    h0 = _mm_alignr_epi8(th, h1, 8);
    h1 = _mm_blend_epi16(h1, th, 0xf0);

    /* if enough data to fill one or more whole buffers, process them. */
    while (inputLen >= SHA256_BLOCK_LENGTH) {
        __m128i a, b, c, d;
        __m128i w0, w1;
        a = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)input), shuffle);
        b = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)(input + 16)), shuffle);
        c = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)(input + 32)), shuffle);
        d = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)(input + 48)), shuffle);
        input += SHA256_BLOCK_LENGTH;
        inputLen -= SHA256_BLOCK_LENGTH;

        w0 = h0;
        w1 = h1;

        ROUND(0, a, b, c, d)
        ROUND(1, b, c, d, a)
        ROUND(2, c, d, a, b)
        ROUND(3, d, a, b, c)
        ROUND(4, a, b, c, d)
        ROUND(5, b, c, d, a)
        ROUND(6, c, d, a, b)
        ROUND(7, d, a, b, c)
        ROUND(8, a, b, c, d)
        ROUND(9, b, c, d, a)
        ROUND(10, c, d, a, b)
        ROUND(11, d, a, b, c)
        ROUND(12, a, b, c, d)
        ROUND(13, b, c, d, a)
        ROUND(14, c, d, a, b)
        ROUND(15, d, a, b, c)

        h0 = _mm_add_epi32(h0, w0);
        h1 = _mm_add_epi32(h1, w1);
    }

    // H01234567 -> H01256 and H2367
    th = _mm_shuffle_epi32(h0, 0x1b);
    h1 = _mm_shuffle_epi32(h1, 0xb1);
    h0 = _mm_blend_epi16(th, h1, 0xf0);
    h1 = _mm_alignr_epi8(h1, th, 8);

    _mm_storeu_si128((__m128i *)ctx->h, h0);
    _mm_storeu_si128((__m128i *)(ctx->h + 4), h1);

    /* if data left over, fill it into buffer */
    if (inputLen) {
        memcpy(ctx->u.b, input, inputLen);
    }
}

#endif /* USE_HW_SHA2 */
