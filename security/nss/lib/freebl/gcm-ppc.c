/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "gcm.h"
#include "secerr.h"

#if defined(USE_PPC_CRYPTO)

SECStatus
gcm_HashWrite_hw(gcmHashContext *ghash, unsigned char *outbuf)
{
    vec_xst_be((vec_u8)ghash->x, 0, outbuf);
    return SECSuccess;
}

static vec_u64
vpmsumd(const vec_u64 a, const vec_u64 b)
{
#if defined(__clang__)
    /* Clang uses a different name */
    return __builtin_altivec_crypto_vpmsumd(a, b);
#elif (__GNUC__ >= 10) || (__GNUC__ == 9 && __GNUC_MINOR__ >= 3) || \
    (__GNUC__ == 8 && __GNUC_MINOR__ >= 4) ||                       \
    (__GNUC__ == 7 && __GNUC_MINOR__ >= 5)
    /* GCC versions not affected by https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91275 */
    return __builtin_crypto_vpmsumd(a, b);
#else
    /* GCC versions where this builtin is buggy */
    vec_u64 vr;
    __asm("vpmsumd %0, %1, %2"
          : "=v"(vr)
          : "v"(a), "v"(b));
    return vr;
#endif
}

SECStatus
gcm_HashMult_hw(gcmHashContext *ghash, const unsigned char *buf,
                unsigned int count)
{
    const vec_u8 leftshift = vec_splat_u8(1);
    const vec_u64 onebit = (vec_u64){ 1, 0 };
    const unsigned long long pd = 0xc2LLU << 56;

    vec_u64 ci, v, r0, r1;
    vec_u64 hibit;
    unsigned i;

    ci = ghash->x;

    for (i = 0; i < count; i++, buf += 16) {
        /* clang needs the following cast away from const; maybe a bug in 7.0.0 */
        v = (vec_u64)vec_xl_be(0, (unsigned char *)buf);
        ci ^= v;

        /* Do binary mult ghash->X = C * ghash->H (Karatsuba). */
        r0 = vpmsumd((vec_u64){ ci[0], 0 }, (vec_u64){ ghash->h[0], 0 });
        r1 = vpmsumd((vec_u64){ ci[1], 0 }, (vec_u64){ ghash->h[1], 0 });
        v = (vec_u64){ ci[0] ^ ci[1], ghash->h[0] ^ ghash->h[1] };
        v = vpmsumd((vec_u64){ v[0], 0 }, (vec_u64){ v[1], 0 });
        v ^= r0;
        v ^= r1;
        r0 ^= (vec_u64){ 0, v[0] };
        r1 ^= (vec_u64){ v[1], 0 };

        /* Shift one (multiply by x) as gcm spec is stupid. */
        hibit = (vec_u64)vec_splat((vec_u8)r0, 15);
        hibit = (vec_u64)vec_rl((vec_u8)hibit, leftshift);
        hibit &= onebit;
        r0 = vec_sll(r0, leftshift);
        r1 = vec_sll(r1, leftshift);
        r1 |= hibit;

        /* Reduce */
        v = vpmsumd((vec_u64){ r0[0], 0 }, (vec_u64){ pd, 0 });
        r0 ^= (vec_u64){ 0, v[0] };
        r1 ^= (vec_u64){ v[1], 0 };
        v = vpmsumd((vec_u64){ r0[1], 0 }, (vec_u64){ pd, 0 });
        r1 ^= v;
        ci = r0 ^ r1;
    }

    ghash->x = ci;

    return SECSuccess;
}

SECStatus
gcm_HashInit_hw(gcmHashContext *ghash)
{
    ghash->x = (vec_u64)vec_splat_u32(0);
    ghash->h = (vec_u64){ ghash->h_low, ghash->h_high };
    ghash->ghash_mul = gcm_HashMult_hw;
    ghash->hw = PR_TRUE;
    return SECSuccess;
}

SECStatus
gcm_HashZeroX_hw(gcmHashContext *ghash)
{
    ghash->x = (vec_u64)vec_splat_u32(0);
    return SECSuccess;
}

#endif /* defined(USE_PPC_CRYPTO) */
