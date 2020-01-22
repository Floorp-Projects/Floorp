/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "rijndael.h"
#include "secerr.h"

#include <wmmintrin.h> /* aes-ni */

#define EXPAND_KEY128(k, rcon, res)                   \
    tmp_key = _mm_aeskeygenassist_si128(k, rcon);     \
    tmp_key = _mm_shuffle_epi32(tmp_key, 0xFF);       \
    tmp = _mm_xor_si128(k, _mm_slli_si128(k, 4));     \
    tmp = _mm_xor_si128(tmp, _mm_slli_si128(tmp, 4)); \
    tmp = _mm_xor_si128(tmp, _mm_slli_si128(tmp, 4)); \
    res = _mm_xor_si128(tmp, tmp_key)

static void
native_key_expansion128(AESContext *cx, const unsigned char *key)
{
    __m128i *keySchedule = cx->k.keySchedule;
    pre_align __m128i tmp_key post_align;
    pre_align __m128i tmp post_align;
    keySchedule[0] = _mm_loadu_si128((__m128i *)key);
    EXPAND_KEY128(keySchedule[0], 0x01, keySchedule[1]);
    EXPAND_KEY128(keySchedule[1], 0x02, keySchedule[2]);
    EXPAND_KEY128(keySchedule[2], 0x04, keySchedule[3]);
    EXPAND_KEY128(keySchedule[3], 0x08, keySchedule[4]);
    EXPAND_KEY128(keySchedule[4], 0x10, keySchedule[5]);
    EXPAND_KEY128(keySchedule[5], 0x20, keySchedule[6]);
    EXPAND_KEY128(keySchedule[6], 0x40, keySchedule[7]);
    EXPAND_KEY128(keySchedule[7], 0x80, keySchedule[8]);
    EXPAND_KEY128(keySchedule[8], 0x1B, keySchedule[9]);
    EXPAND_KEY128(keySchedule[9], 0x36, keySchedule[10]);
}

#define EXPAND_KEY192_PART1(res, k0, kt, rcon)                                \
    tmp2 = _mm_slli_si128(k0, 4);                                             \
    tmp1 = _mm_xor_si128(k0, tmp2);                                           \
    tmp2 = _mm_slli_si128(tmp2, 4);                                           \
    tmp1 = _mm_xor_si128(_mm_xor_si128(tmp1, tmp2), _mm_slli_si128(tmp2, 4)); \
    tmp2 = _mm_aeskeygenassist_si128(kt, rcon);                               \
    res = _mm_xor_si128(tmp1, _mm_shuffle_epi32(tmp2, 0x55))

#define EXPAND_KEY192_PART2(res, k1, k2)             \
    tmp2 = _mm_xor_si128(k1, _mm_slli_si128(k1, 4)); \
    res = _mm_xor_si128(tmp2, _mm_shuffle_epi32(k2, 0xFF))

#define EXPAND_KEY192(k0, res1, res2, res3, carry, rcon1, rcon2)         \
    EXPAND_KEY192_PART1(tmp3, k0, res1, rcon1);                          \
    EXPAND_KEY192_PART2(carry, res1, tmp3);                              \
    res1 = _mm_castpd_si128(_mm_shuffle_pd(_mm_castsi128_pd(res1),       \
                                           _mm_castsi128_pd(tmp3), 0));  \
    res2 = _mm_castpd_si128(_mm_shuffle_pd(_mm_castsi128_pd(tmp3),       \
                                           _mm_castsi128_pd(carry), 1)); \
    EXPAND_KEY192_PART1(res3, tmp3, carry, rcon2)

static void
native_key_expansion192(AESContext *cx, const unsigned char *key)
{
    __m128i *keySchedule = cx->k.keySchedule;
    pre_align __m128i tmp1 post_align;
    pre_align __m128i tmp2 post_align;
    pre_align __m128i tmp3 post_align;
    pre_align __m128i carry post_align;
    keySchedule[0] = _mm_loadu_si128((__m128i *)key);
    keySchedule[1] = _mm_loadu_si128((__m128i *)(key + 16));
    EXPAND_KEY192(keySchedule[0], keySchedule[1], keySchedule[2],
                  keySchedule[3], carry, 0x1, 0x2);
    EXPAND_KEY192_PART2(keySchedule[4], carry, keySchedule[3]);
    EXPAND_KEY192(keySchedule[3], keySchedule[4], keySchedule[5],
                  keySchedule[6], carry, 0x4, 0x8);
    EXPAND_KEY192_PART2(keySchedule[7], carry, keySchedule[6]);
    EXPAND_KEY192(keySchedule[6], keySchedule[7], keySchedule[8],
                  keySchedule[9], carry, 0x10, 0x20);
    EXPAND_KEY192_PART2(keySchedule[10], carry, keySchedule[9]);
    EXPAND_KEY192(keySchedule[9], keySchedule[10], keySchedule[11],
                  keySchedule[12], carry, 0x40, 0x80);
}

#define EXPAND_KEY256_PART(res, rconx, k1x, k2x, X)                           \
    tmp_key = _mm_shuffle_epi32(_mm_aeskeygenassist_si128(k2x, rconx), X);    \
    tmp2 = _mm_slli_si128(k1x, 4);                                            \
    tmp1 = _mm_xor_si128(k1x, tmp2);                                          \
    tmp2 = _mm_slli_si128(tmp2, 4);                                           \
    tmp1 = _mm_xor_si128(_mm_xor_si128(tmp1, tmp2), _mm_slli_si128(tmp2, 4)); \
    res = _mm_xor_si128(tmp1, tmp_key);

#define EXPAND_KEY256(res1, res2, k1, k2, rcon)   \
    EXPAND_KEY256_PART(res1, rcon, k1, k2, 0xFF); \
    EXPAND_KEY256_PART(res2, 0x00, k2, res1, 0xAA)

static void
native_key_expansion256(AESContext *cx, const unsigned char *key)
{
    __m128i *keySchedule = cx->k.keySchedule;
    pre_align __m128i tmp_key post_align;
    pre_align __m128i tmp1 post_align;
    pre_align __m128i tmp2 post_align;
    keySchedule[0] = _mm_loadu_si128((__m128i *)key);
    keySchedule[1] = _mm_loadu_si128((__m128i *)(key + 16));
    EXPAND_KEY256(keySchedule[2], keySchedule[3], keySchedule[0],
                  keySchedule[1], 0x01);
    EXPAND_KEY256(keySchedule[4], keySchedule[5], keySchedule[2],
                  keySchedule[3], 0x02);
    EXPAND_KEY256(keySchedule[6], keySchedule[7], keySchedule[4],
                  keySchedule[5], 0x04);
    EXPAND_KEY256(keySchedule[8], keySchedule[9], keySchedule[6],
                  keySchedule[7], 0x08);
    EXPAND_KEY256(keySchedule[10], keySchedule[11], keySchedule[8],
                  keySchedule[9], 0x10);
    EXPAND_KEY256(keySchedule[12], keySchedule[13], keySchedule[10],
                  keySchedule[11], 0x20);
    EXPAND_KEY256_PART(keySchedule[14], 0x40, keySchedule[12],
                       keySchedule[13], 0xFF);
}

/*
 * AES key expansion using aes-ni instructions.
 */
void
rijndael_native_key_expansion(AESContext *cx, const unsigned char *key,
                              unsigned int Nk)
{
    switch (Nk) {
        case 4:
            native_key_expansion128(cx, key);
            return;
        case 6:
            native_key_expansion192(cx, key);
            return;
        case 8:
            native_key_expansion256(cx, key);
            return;
        default:
            /* This shouldn't happen (checked by the caller). */
            return;
    }
}

void
rijndael_native_encryptBlock(AESContext *cx,
                             unsigned char *output,
                             const unsigned char *input)
{
    int i;
    pre_align __m128i m post_align = _mm_loadu_si128((__m128i *)input);
    m = _mm_xor_si128(m, cx->k.keySchedule[0]);
    for (i = 1; i < cx->Nr; ++i) {
        m = _mm_aesenc_si128(m, cx->k.keySchedule[i]);
    }
    m = _mm_aesenclast_si128(m, cx->k.keySchedule[cx->Nr]);
    _mm_storeu_si128((__m128i *)output, m);
}

void
rijndael_native_decryptBlock(AESContext *cx,
                             unsigned char *output,
                             const unsigned char *input)
{
    int i;
    pre_align __m128i m post_align = _mm_loadu_si128((__m128i *)input);
    m = _mm_xor_si128(m, cx->k.keySchedule[cx->Nr]);
    for (i = cx->Nr - 1; i > 0; --i) {
        m = _mm_aesdec_si128(m, cx->k.keySchedule[i]);
    }
    m = _mm_aesdeclast_si128(m, cx->k.keySchedule[0]);
    _mm_storeu_si128((__m128i *)output, m);
}

// out = a ^ b
void
native_xorBlock(unsigned char *out,
                const unsigned char *a,
                const unsigned char *b)
{
    pre_align __m128i post_align in1 = _mm_loadu_si128((__m128i *)(a));
    pre_align __m128i post_align in2 = _mm_loadu_si128((__m128i *)(b));
    in1 = _mm_xor_si128(in1, in2);
    _mm_storeu_si128((__m128i *)(out), in1);
}
