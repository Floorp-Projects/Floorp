/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef USE_HW_SHA1

#ifndef __ARM_FEATURE_CRYPTO
#error "Compiler option is invalid"
#endif

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include <arm_neon.h>
#include <memory.h>
#include "blapi.h"
#include "sha_fast.h"

#if !defined(SHA_PUT_W_IN_STACK)
#define H2X 11
#else
#define H2X 0
#endif

static void shaCompress(SHA_HW_t *X, const PRUint32 *datain);

void
SHA1_Compress_Native(SHA1Context *ctx)
{
    shaCompress(&ctx->H[H2X], ctx->u.w);
}

/*
 *  SHA: Add data to context.
 */
void
SHA1_Update_Native(SHA1Context *ctx, const unsigned char *dataIn, unsigned int len)
{
    unsigned int lenB;
    unsigned int togo;

    if (!len) {
        return;
    }

    /* accumulate the byte count. */
    lenB = (unsigned int)(ctx->size) & 63U;

    ctx->size += len;

    /*
   *  Read the data into W and process blocks as they get full
   */
    if (lenB > 0) {
        togo = 64U - lenB;
        if (len < togo) {
            togo = len;
        }
        memcpy(ctx->u.b + lenB, dataIn, togo);
        len -= togo;
        dataIn += togo;
        lenB = (lenB + togo) & 63U;
        if (!lenB) {
            shaCompress(&ctx->H[H2X], ctx->u.w);
        }
    }

    while (len >= 64U) {
        len -= 64U;
        shaCompress(&ctx->H[H2X], (PRUint32 *)dataIn);
        dataIn += 64U;
    }

    if (len) {
        memcpy(ctx->u.b, dataIn, len);
    }
}

/*
 *  SHA: Compression function, unrolled.
 */
static void
shaCompress(SHA_HW_t *X, const PRUint32 *inbuf)
{
#define XH(n) X[n - H2X]

    const uint32x4_t K0 = vdupq_n_u32(0x5a827999);
    const uint32x4_t K1 = vdupq_n_u32(0x6ed9eba1);
    const uint32x4_t K2 = vdupq_n_u32(0x8f1bbcdc);
    const uint32x4_t K3 = vdupq_n_u32(0xca62c1d6);

    uint32x4_t abcd = vld1q_u32(&XH(0));
    PRUint32 e = XH(4);

    const uint32x4_t origABCD = abcd;
    const PRUint32 origE = e;

    uint32x4_t w0 = vld1q_u32(inbuf);
    uint32x4_t w1 = vld1q_u32(inbuf + 4);
    uint32x4_t w2 = vld1q_u32(inbuf + 8);
    uint32x4_t w3 = vld1q_u32(inbuf + 12);

    w0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w0)));
    w1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w1)));
    w2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w2)));
    w3 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w3)));

    uint32x4_t t0 = vaddq_u32(w0, K0);
    uint32x4_t t1 = vaddq_u32(w1, K0);

    PRUint32 tmpE;

    /*
         * Using the following ARM instructions to accelerate SHA1
         *
         * sha1c for round 0 - 20
         * sha1p for round 20 - 40
         * sha1m for round 40 - 60
         * sha1p for round 60 - 80
         * sha1su0 and shasu1 for message schedule
         * sha1h for rotate left 30
         */

    /* Round 0-3 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1cq_u32(abcd, e, t0);
    t0 = vaddq_u32(w2, K0);
    w0 = vsha1su0q_u32(w0, w1, w2);

    /* Round 4-7 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1cq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w3, K0);
    w0 = vsha1su1q_u32(w0, w3);
    w1 = vsha1su0q_u32(w1, w2, w3);

    /* Round 8-11 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1cq_u32(abcd, e, t0);
    t0 = vaddq_u32(w0, K0);
    w1 = vsha1su1q_u32(w1, w0);
    w2 = vsha1su0q_u32(w2, w3, w0);

    /* Round 12-15 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1cq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w1, K1);
    w2 = vsha1su1q_u32(w2, w1);
    w3 = vsha1su0q_u32(w3, w0, w1);

    /* Round 16-19 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1cq_u32(abcd, e, t0);
    t0 = vaddq_u32(w2, K1);
    w3 = vsha1su1q_u32(w3, w2);
    w0 = vsha1su0q_u32(w0, w1, w2);

    /* Round 20-23 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w3, K1);
    w0 = vsha1su1q_u32(w0, w3);
    w1 = vsha1su0q_u32(w1, w2, w3);

    /* Round 24-27 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, e, t0);
    t0 = vaddq_u32(w0, K1);
    w1 = vsha1su1q_u32(w1, w0);
    w2 = vsha1su0q_u32(w2, w3, w0);

    /* Round 28-31 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w1, K1);
    w2 = vsha1su1q_u32(w2, w1);
    w3 = vsha1su0q_u32(w3, w0, w1);

    /* Round 32-35 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, e, t0);
    t0 = vaddq_u32(w2, K2);
    w3 = vsha1su1q_u32(w3, w2);
    w0 = vsha1su0q_u32(w0, w1, w2);

    /* Round 36-39 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w3, K2);
    w0 = vsha1su1q_u32(w0, w3);
    w1 = vsha1su0q_u32(w1, w2, w3);

    /* Round 40-43 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1mq_u32(abcd, e, t0);
    t0 = vaddq_u32(w0, K2);
    w1 = vsha1su1q_u32(w1, w0);
    w2 = vsha1su0q_u32(w2, w3, w0);

    /* Round 44-47 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1mq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w1, K2);
    w2 = vsha1su1q_u32(w2, w1);
    w3 = vsha1su0q_u32(w3, w0, w1);

    /* Round 48-51 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1mq_u32(abcd, e, t0);
    t0 = vaddq_u32(w2, K2);
    w3 = vsha1su1q_u32(w3, w2);
    w0 = vsha1su0q_u32(w0, w1, w2);

    /* Round 52-55 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1mq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w3, K3);
    w0 = vsha1su1q_u32(w0, w3);
    w1 = vsha1su0q_u32(w1, w2, w3);

    /* Round 56-59 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1mq_u32(abcd, e, t0);
    t0 = vaddq_u32(w0, K3);
    w1 = vsha1su1q_u32(w1, w0);
    w2 = vsha1su0q_u32(w2, w3, w0);

    /* Round 60-63 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w1, K3);
    w2 = vsha1su1q_u32(w2, w1);
    w3 = vsha1su0q_u32(w3, w0, w1);

    /* Round 64-67 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, e, t0);
    t0 = vaddq_u32(w2, K3);
    w3 = vsha1su1q_u32(w3, w2);
    w0 = vsha1su0q_u32(w0, w1, w2);

    /* Round 68-71 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, tmpE, t1);
    t1 = vaddq_u32(w3, K3);
    w0 = vsha1su1q_u32(w0, w3);

    /* Round 72-75 */
    tmpE = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, e, t0);

    /* Round 76-79 */
    e = vsha1h_u32(vgetq_lane_u32(abcd, 0));
    abcd = vsha1pq_u32(abcd, tmpE, t1);

    e += origE;
    abcd = vaddq_u32(origABCD, abcd);

    vst1q_u32(&XH(0), abcd);
    XH(4) = e;
}

#endif /* USE_HW_SHA1 */
