/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "gcm.h"
#include "secerr.h"

#include <wmmintrin.h> /* clmul */

#define WRITE64(x, bytes)   \
    (bytes)[0] = (x) >> 56; \
    (bytes)[1] = (x) >> 48; \
    (bytes)[2] = (x) >> 40; \
    (bytes)[3] = (x) >> 32; \
    (bytes)[4] = (x) >> 24; \
    (bytes)[5] = (x) >> 16; \
    (bytes)[6] = (x) >> 8;  \
    (bytes)[7] = (x);

SECStatus
gcm_HashWrite_hw(gcmHashContext *ghash, unsigned char *outbuf)
{
    uint64_t tmp_out[2];
    _mm_storeu_si128((__m128i *)tmp_out, ghash->x);
    /* maxout must be larger than 16 byte (checked by the caller). */
    WRITE64(tmp_out[0], outbuf + 8);
    WRITE64(tmp_out[1], outbuf);
    return SECSuccess;
}

SECStatus
gcm_HashMult_hw(gcmHashContext *ghash, const unsigned char *buf,
                unsigned int count)
{
    size_t i;
    pre_align __m128i z_high post_align;
    pre_align __m128i z_low post_align;
    pre_align __m128i C post_align;
    pre_align __m128i D post_align;
    pre_align __m128i E post_align;
    pre_align __m128i F post_align;
    pre_align __m128i bin post_align;
    pre_align __m128i Ci post_align;
    pre_align __m128i tmp post_align;

    for (i = 0; i < count; i++, buf += 16) {
        bin = _mm_set_epi16(((uint16_t)buf[0] << 8) | buf[1],
                            ((uint16_t)buf[2] << 8) | buf[3],
                            ((uint16_t)buf[4] << 8) | buf[5],
                            ((uint16_t)buf[6] << 8) | buf[7],
                            ((uint16_t)buf[8] << 8) | buf[9],
                            ((uint16_t)buf[10] << 8) | buf[11],
                            ((uint16_t)buf[12] << 8) | buf[13],
                            ((uint16_t)buf[14] << 8) | buf[15]);
        Ci = _mm_xor_si128(bin, ghash->x);

        /* Do binary mult ghash->X = Ci * ghash->H. */
        C = _mm_clmulepi64_si128(Ci, ghash->h, 0x00);
        D = _mm_clmulepi64_si128(Ci, ghash->h, 0x11);
        E = _mm_clmulepi64_si128(Ci, ghash->h, 0x01);
        F = _mm_clmulepi64_si128(Ci, ghash->h, 0x10);
        tmp = _mm_xor_si128(E, F);
        z_high = _mm_xor_si128(tmp, _mm_slli_si128(D, 8));
        z_high = _mm_unpackhi_epi64(z_high, D);
        z_low = _mm_xor_si128(_mm_slli_si128(tmp, 8), C);
        z_low = _mm_unpackhi_epi64(_mm_slli_si128(C, 8), z_low);

        /* Shift one to the left (multiply by x) as gcm spec is stupid. */
        C = _mm_slli_si128(z_low, 8);
        E = _mm_srli_epi64(C, 63);
        D = _mm_slli_si128(z_high, 8);
        F = _mm_srli_epi64(D, 63);
        /* Carry over */
        C = _mm_srli_si128(z_low, 8);
        D = _mm_srli_epi64(C, 63);
        z_low = _mm_or_si128(_mm_slli_epi64(z_low, 1), E);
        z_high = _mm_or_si128(_mm_or_si128(_mm_slli_epi64(z_high, 1), F), D);

        /* Reduce */
        C = _mm_slli_si128(z_low, 8);
        /* D = z_low << 127 */
        D = _mm_slli_epi64(C, 63);
        /* E = z_low << 126 */
        E = _mm_slli_epi64(C, 62);
        /* F = z_low << 121 */
        F = _mm_slli_epi64(C, 57);
        /* z_low ^= (z_low << 127) ^ (z_low << 126) ^ (z_low << 121); */
        z_low = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(z_low, D), E), F);
        C = _mm_srli_si128(z_low, 8);
        /* D = z_low >> 1 */
        D = _mm_slli_epi64(C, 63);
        D = _mm_or_si128(_mm_srli_epi64(z_low, 1), D);
        /* E = z_low >> 2 */
        E = _mm_slli_epi64(C, 62);
        E = _mm_or_si128(_mm_srli_epi64(z_low, 2), E);
        /* F = z_low >> 7 */
        F = _mm_slli_epi64(C, 57);
        F = _mm_or_si128(_mm_srli_epi64(z_low, 7), F);
        /* ghash->x ^= z_low ^ (z_low >> 1) ^ (z_low >> 2) ^ (z_low >> 7); */
        ghash->x = _mm_xor_si128(_mm_xor_si128(
                                     _mm_xor_si128(_mm_xor_si128(z_high, z_low), D), E),
                                 F);
    }
    return SECSuccess;
}

SECStatus
gcm_HashInit_hw(gcmHashContext *ghash)
{
    ghash->ghash_mul = gcm_HashMult_hw;
    ghash->x = _mm_setzero_si128();
    /* MSVC requires __m64 to load epi64. */
    ghash->h = _mm_set_epi32(ghash->h_high >> 32, (uint32_t)ghash->h_high,
                             ghash->h_low >> 32, (uint32_t)ghash->h_low);
    ghash->hw = PR_TRUE;
    return SECSuccess;
}

SECStatus
gcm_HashZeroX_hw(gcmHashContext *ghash)
{
    ghash->x = _mm_setzero_si128();
    return SECSuccess;
}
