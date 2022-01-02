/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "gcm.h"
#include "secerr.h"

/* old gcc doesn't support some poly64x2_t intrinsic */
#if defined(__aarch64__) && defined(IS_LITTLE_ENDIAN) && \
    (defined(__clang__) || defined(__GNUC__) && __GNUC__ > 6)

#include <arm_neon.h>

SECStatus
gcm_HashWrite_hw(gcmHashContext *ghash, unsigned char *outbuf)
{
    uint8x16_t ci = vrbitq_u8(vreinterpretq_u8_u64(ghash->x));
    vst1q_u8(outbuf, ci);
    return SECSuccess;
}

SECStatus
gcm_HashMult_hw(gcmHashContext *ghash, const unsigned char *buf,
                unsigned int count)
{
    const poly64x2_t p = vdupq_n_p64(0x87);
    const uint8x16_t zero = vdupq_n_u8(0);
    const uint64x2_t h = ghash->h;
    uint64x2_t ci = ghash->x;
    unsigned int i;
    uint8x16_t z_low, z_high;
    uint8x16_t t_low, t_high;
    poly64x2_t t1;
    uint8x16_t t2;

    for (i = 0; i < count; i++, buf += 16) {
        ci = vreinterpretq_u64_u8(veorq_u8(vreinterpretq_u8_u64(ci),
                                           vrbitq_u8(vld1q_u8(buf))));

        /* Do binary mult ghash->X = Ci * ghash->H. */
        z_low = vreinterpretq_u8_p128(
            vmull_p64((poly64_t)vget_low_p64(vreinterpretq_p64_u64(ci)),
                      (poly64_t)vget_low_p64(vreinterpretq_p64_u64(h))));
        z_high = vreinterpretq_u8_p128(
            vmull_high_p64(vreinterpretq_p64_u64(ci), vreinterpretq_p64_u64(h)));
        t1 = vreinterpretq_p64_u8(
            vextq_u8(vreinterpretq_u8_u64(h), vreinterpretq_u8_u64(h), 8));
        t_low = vreinterpretq_u8_p128(
            vmull_p64((poly64_t)vget_low_p64(vreinterpretq_p64_u64(ci)),
                      (poly64_t)vget_low_p64(t1)));
        t_high = vreinterpretq_u8_p128(vmull_high_p64(vreinterpretq_p64_u64(ci), t1));
        t2 = veorq_u8(t_high, t_low);
        z_low = veorq_u8(z_low, vextq_u8(zero, t2, 8));
        z_high = veorq_u8(z_high, vextq_u8(t2, zero, 8));

        /* polynomial reduction */
        t2 = vreinterpretq_u8_p128(vmull_high_p64(vreinterpretq_p64_u8(z_high), p));
        z_high = veorq_u8(z_high, vextq_u8(t2, zero, 8));
        z_low = veorq_u8(z_low, vextq_u8(zero, t2, 8));
        ci = veorq_u64(vreinterpretq_u64_u8(z_low),
                       vreinterpretq_u64_p128(
                           vmull_p64((poly64_t)vget_low_p64(vreinterpretq_p64_u8(z_high)),
                                     (poly64_t)vget_low_p64(p))));
    }

    ghash->x = ci;
    return SECSuccess;
}

SECStatus
gcm_HashInit_hw(gcmHashContext *ghash)
{
    /* Workaround of "used uninitialized in this function" error */
    uint64x2_t h = vdupq_n_u64(0);

    ghash->ghash_mul = gcm_HashMult_hw;
    ghash->x = vdupq_n_u64(0);
    h = vsetq_lane_u64(__builtin_bswap64(ghash->h_low), h, 1);
    h = vsetq_lane_u64(__builtin_bswap64(ghash->h_high), h, 0);
    h = vreinterpretq_u64_u8(vrbitq_u8(vreinterpretq_u8_u64(h)));
    ghash->h = h;
    ghash->hw = PR_TRUE;
    return SECSuccess;
}

SECStatus
gcm_HashZeroX_hw(gcmHashContext *ghash)
{
    ghash->x = vdupq_n_u64(0);
    return SECSuccess;
}

#endif /* defined(__clang__) || (defined(__GNUC__) && __GNUC__ > 6) */
