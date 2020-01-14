/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "blapii.h"
#include "blapit.h"
#include "gcm.h"
#include "secerr.h"
#include "prtypes.h"

#if defined(IS_LITTLE_ENDIAN)

#include <arm_neon.h>

SECStatus
gcm_HashWrite_hw(gcmHashContext *ghash, unsigned char *outbuf)
{
    vst1_u8(outbuf, vrev64_u8(vcreate_u8(ghash->x_high)));
    vst1_u8(outbuf + 8, vrev64_u8(vcreate_u8(ghash->x_low)));
    return SECSuccess;
}

/* Carry-less multiplication. a * b = ret. */
static inline uint8x16_t
clmul(const uint8x8_t a, const uint8x8_t b)
{
    uint8x16_t d, e, f, g, h, i, j, k, l, m, n;
    uint8x8_t t_high, t_low;
    uint8x16_t t0, t1, t2, t3;
    const uint8x8_t k16 = vcreate_u8(0xffff);
    const uint8x8_t k32 = vcreate_u8(0xffffffff);
    const uint8x8_t k48 = vcreate_u8(0xffffffffffff);

    // D = A * B
    d = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(a),
                                      vreinterpret_p8_u8(b)));
    // E = A * B1
    e = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(a),
                                      vreinterpret_p8_u8(vext_u8(b, b, 1))));
    // F = A1 * B
    f = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(vext_u8(a, a, 1)),
                                      vreinterpret_p8_u8(b)));
    // G = A * B2
    g = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(a),
                                      vreinterpret_p8_u8(vext_u8(b, b, 2))));
    // H = A2 * B
    h = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(vext_u8(a, a, 2)),
                                      vreinterpret_p8_u8(b)));
    // I = A * B3
    i = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(a),
                                      vreinterpret_p8_u8(vext_u8(b, b, 3))));
    // J = A3 * B
    j = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(vext_u8(a, a, 3)),
                                      vreinterpret_p8_u8(b)));
    // K = A * B4
    k = vreinterpretq_u8_p16(vmull_p8(vreinterpret_p8_u8(a),
                                      vreinterpret_p8_u8(vext_u8(b, b, 4))));
    // L = E + F
    l = veorq_u8(e, f);
    // M = G + H
    m = veorq_u8(g, h);
    // N = I + J
    n = veorq_u8(i, j);

    // t0 = (L) (P0 + P1) << 8
    t_high = vget_high_u8(l);
    t_low = vget_low_u8(l);
    t_low = veor_u8(t_low, t_high);
    t_high = vand_u8(t_high, k48);
    t_low = veor_u8(t_low, t_high);
    t0 = vcombine_u8(t_low, t_high);
    t0 = vextq_u8(t0, t0, 15);

    // t1 = (M) (P2 + P3) << 16
    t_high = vget_high_u8(m);
    t_low = vget_low_u8(m);
    t_low = veor_u8(t_low, t_high);
    t_high = vand_u8(t_high, k32);
    t_low = veor_u8(t_low, t_high);
    t1 = vcombine_u8(t_low, t_high);
    t1 = vextq_u8(t1, t1, 14);

    // t2 = (N) (P4 + P5) << 24
    t_high = vget_high_u8(n);
    t_low = vget_low_u8(n);
    t_low = veor_u8(t_low, t_high);
    t_high = vand_u8(t_high, k16);
    t_low = veor_u8(t_low, t_high);
    t2 = vcombine_u8(t_low, t_high);
    t2 = vextq_u8(t2, t2, 13);

    // t3 = (K) (P6 + P7) << 32
    t_high = vget_high_u8(k);
    t_low = vget_low_u8(k);
    t_low = veor_u8(t_low, t_high);
    t_high = vdup_n_u8(0);
    t3 = vcombine_u8(t_low, t_high);
    t3 = vextq_u8(t3, t3, 12);

    t0 = veorq_u8(t0, t1);
    t2 = veorq_u8(t2, t3);
    return veorq_u8(veorq_u8(d, t0), t2);
}

SECStatus
gcm_HashMult_hw(gcmHashContext *ghash, const unsigned char *buf,
                unsigned int count)
{
    const uint8x8_t h_low = vcreate_u8(ghash->h_low);
    const uint8x8_t h_high = vcreate_u8(ghash->h_high);
    uint8x16_t ci;
    uint8x8_t ci_low;
    uint8x8_t ci_high;
    uint8x16_t z0, z2, z1a;
    uint8x16_t z_high, z_low;
    uint8x16_t t;
    int64x2_t t1, t2, t3;
    uint64x2_t z_low_l, z_low_r, z_high_l, z_high_r;
    size_t i;

    ci = vcombine_u8(vcreate_u8(ghash->x_low), vcreate_u8(ghash->x_high));

    for (i = 0; i < count; i++, buf += 16) {
        ci = veorq_u8(ci, vcombine_u8(vrev64_u8(vld1_u8(buf + 8)),
                                      vrev64_u8(vld1_u8(buf))));
        ci_high = vget_high_u8(ci);
        ci_low = vget_low_u8(ci);

        /* Do binary mult ghash->X = C * ghash->H (Karatsuba). */
        z0 = clmul(ci_low, h_low);
        z2 = clmul(ci_high, h_high);
        z1a = clmul(veor_u8(ci_high, ci_low), veor_u8(h_high, h_low));
        z1a = veorq_u8(z0, z1a);
        z1a = veorq_u8(z2, z1a);
        z_high = vcombine_u8(veor_u8(vget_low_u8(z2), vget_high_u8(z1a)),
                             vget_high_u8(z2));
        z_low = vcombine_u8(vget_low_u8(z0),
                            veor_u8(vget_high_u8(z0), vget_low_u8(z1a)));

        /* Shift one (multiply by x) as gcm spec is stupid. */
        z_low_l = vshlq_n_u64(vreinterpretq_u64_u8(z_low), 1);
        z_low_r = vshrq_n_u64(vreinterpretq_u64_u8(z_low), 63);
        z_high_l = vshlq_n_u64(vreinterpretq_u64_u8(z_high), 1);
        z_high_r = vshrq_n_u64(vreinterpretq_u64_u8(z_high), 63);
        z_low = vreinterpretq_u8_u64(
            vcombine_u64(vget_low_u64(z_low_l),
                         vorr_u64(vget_high_u64(z_low_l),
                                  vget_low_u64(z_low_r))));
        z_high = vreinterpretq_u8_u64(
            vcombine_u64(vorr_u64(vget_low_u64(z_high_l),
                                  vget_high_u64(z_low_r)),
                         vorr_u64(vget_high_u64(z_high_l),
                                  vget_low_u64(z_high_r))));

        /* Reduce */
        t1 = vshlq_n_s64(vreinterpretq_s64_u8(z_low), 57);
        t2 = vshlq_n_s64(vreinterpretq_s64_u8(z_low), 62);
        t3 = vshlq_n_s64(vreinterpretq_s64_u8(z_low), 63);
        t = vreinterpretq_u8_s64(veorq_s64(t1, veorq_s64(t2, t3)));

        z_low = vcombine_u8(vget_low_u8(z_low),
                            veor_u8(vget_high_u8(z_low), vget_low_u8(t)));
        z_high = vcombine_u8(veor_u8(vget_low_u8(z_high), vget_high_u8(t)),
                             vget_high_u8(z_high));

        t = vreinterpretq_u8_u64(vshrq_n_u64(vreinterpretq_u64_u8(z_low), 1));
        z_high = veorq_u8(z_high, z_low);
        z_low = veorq_u8(z_low, t);
        t = vreinterpretq_u8_u64(vshrq_n_u64(vreinterpretq_u64_u8(t), 6));
        z_low = vreinterpretq_u8_u64(
            vshrq_n_u64(vreinterpretq_u64_u8(z_low), 1));
        z_low = veorq_u8(z_low, z_high);
        ci = veorq_u8(z_low, t);
    }

    vst1_u8((uint8_t *)&ghash->x_high, vget_high_u8(ci));
    vst1_u8((uint8_t *)&ghash->x_low, vget_low_u8(ci));
    return SECSuccess;
}

SECStatus
gcm_HashInit_hw(gcmHashContext *ghash)
{
    ghash->ghash_mul = gcm_HashMult_hw;
    ghash->x_low = 0;
    ghash->x_high = 0;
    ghash->hw = PR_TRUE;
    return SECSuccess;
}

SECStatus
gcm_HashZeroX_hw(gcmHashContext *ghash)
{
    ghash->x_low = 0;
    ghash->x_high = 0;
    return SECSuccess;
}

#endif /* IS_LITTLE_ENDIAN */
