/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* Thanks to Thomas Pornin for the ideas how to implement the constat time
 * binary multiplication. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "blapii.h"
#include "blapit.h"
#include "blapi.h"
#include "gcm.h"
#include "ctr.h"
#include "secerr.h"
#include "prtypes.h"
#include "pkcs11t.h"

#include <limits.h>

/* old gcc doesn't support some poly64x2_t intrinsic */
#if defined(__aarch64__) && defined(IS_LITTLE_ENDIAN) && \
    (defined(__clang__) || defined(__GNUC__) && __GNUC__ > 6)
#define USE_ARM_GCM
#elif defined(__arm__) && defined(IS_LITTLE_ENDIAN) && \
    !defined(NSS_DISABLE_ARM32_NEON)
/* We don't test on big endian platform, so disable this on big endian. */
#define USE_ARM_GCM
#endif

/* Forward declarations */
SECStatus gcm_HashInit_hw(gcmHashContext *ghash);
SECStatus gcm_HashWrite_hw(gcmHashContext *ghash, unsigned char *outbuf);
SECStatus gcm_HashMult_hw(gcmHashContext *ghash, const unsigned char *buf,
                          unsigned int count);
SECStatus gcm_HashZeroX_hw(gcmHashContext *ghash);
SECStatus gcm_HashMult_sftw(gcmHashContext *ghash, const unsigned char *buf,
                            unsigned int count);
SECStatus gcm_HashMult_sftw32(gcmHashContext *ghash, const unsigned char *buf,
                              unsigned int count);

/* Stub definitions for the above *_hw functions, which shouldn't be
 * used unless NSS_X86_OR_X64 is defined */
#if !defined(NSS_X86_OR_X64) && !defined(USE_ARM_GCM) && !defined(USE_PPC_CRYPTO)
SECStatus
gcm_HashWrite_hw(gcmHashContext *ghash, unsigned char *outbuf)
{
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}

SECStatus
gcm_HashMult_hw(gcmHashContext *ghash, const unsigned char *buf,
                unsigned int count)
{
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}

SECStatus
gcm_HashInit_hw(gcmHashContext *ghash)
{
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}

SECStatus
gcm_HashZeroX_hw(gcmHashContext *ghash)
{
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}
#endif /* !NSS_X86_OR_X64 && !USE_ARM_GCM && !USE_PPC_CRYPTO */

uint64_t
get64(const unsigned char *bytes)
{
    return ((uint64_t)bytes[0]) << 56 |
           ((uint64_t)bytes[1]) << 48 |
           ((uint64_t)bytes[2]) << 40 |
           ((uint64_t)bytes[3]) << 32 |
           ((uint64_t)bytes[4]) << 24 |
           ((uint64_t)bytes[5]) << 16 |
           ((uint64_t)bytes[6]) << 8 |
           ((uint64_t)bytes[7]);
}

/* Initialize a gcmHashContext */
SECStatus
gcmHash_InitContext(gcmHashContext *ghash, const unsigned char *H, PRBool sw)
{
    SECStatus rv = SECSuccess;

    ghash->cLen = 0;
    ghash->bufLen = 0;
    PORT_Memset(ghash->counterBuf, 0, sizeof(ghash->counterBuf));

    ghash->h_low = get64(H + 8);
    ghash->h_high = get64(H);
#ifdef USE_ARM_GCM
#if defined(__aarch64__)
    if (arm_pmull_support() && !sw) {
#else
    if (arm_neon_support() && !sw) {
#endif
#elif defined(USE_PPC_CRYPTO)
    if (ppc_crypto_support() && !sw) {
#else
    if (clmul_support() && !sw) {
#endif
        rv = gcm_HashInit_hw(ghash);
    } else {
/* We fall back to the software implementation if we can't use / don't
         * want to use pclmul. */
#ifdef HAVE_INT128_SUPPORT
        ghash->ghash_mul = gcm_HashMult_sftw;
#else
        ghash->ghash_mul = gcm_HashMult_sftw32;
#endif
        ghash->x_high = ghash->x_low = 0;
        ghash->hw = PR_FALSE;
    }
    return rv;
}

#ifdef HAVE_INT128_SUPPORT
/* Binary multiplication x * y = r_high << 64 | r_low. */
void
bmul(uint64_t x, uint64_t y, uint64_t *r_high, uint64_t *r_low)
{
    uint128_t x1, x2, x3, x4, x5;
    uint128_t y1, y2, y3, y4, y5;
    uint128_t r, z;

    uint128_t m1 = (uint128_t)0x2108421084210842 << 64 | 0x1084210842108421;
    uint128_t m2 = (uint128_t)0x4210842108421084 << 64 | 0x2108421084210842;
    uint128_t m3 = (uint128_t)0x8421084210842108 << 64 | 0x4210842108421084;
    uint128_t m4 = (uint128_t)0x0842108421084210 << 64 | 0x8421084210842108;
    uint128_t m5 = (uint128_t)0x1084210842108421 << 64 | 0x0842108421084210;

    x1 = x & m1;
    y1 = y & m1;
    x2 = x & m2;
    y2 = y & m2;
    x3 = x & m3;
    y3 = y & m3;
    x4 = x & m4;
    y4 = y & m4;
    x5 = x & m5;
    y5 = y & m5;

    z = (x1 * y1) ^ (x2 * y5) ^ (x3 * y4) ^ (x4 * y3) ^ (x5 * y2);
    r = z & m1;
    z = (x1 * y2) ^ (x2 * y1) ^ (x3 * y5) ^ (x4 * y4) ^ (x5 * y3);
    r |= z & m2;
    z = (x1 * y3) ^ (x2 * y2) ^ (x3 * y1) ^ (x4 * y5) ^ (x5 * y4);
    r |= z & m3;
    z = (x1 * y4) ^ (x2 * y3) ^ (x3 * y2) ^ (x4 * y1) ^ (x5 * y5);
    r |= z & m4;
    z = (x1 * y5) ^ (x2 * y4) ^ (x3 * y3) ^ (x4 * y2) ^ (x5 * y1);
    r |= z & m5;

    *r_high = (uint64_t)(r >> 64);
    *r_low = (uint64_t)r;
}

SECStatus
gcm_HashMult_sftw(gcmHashContext *ghash, const unsigned char *buf,
                  unsigned int count)
{
    uint64_t ci_low, ci_high;
    size_t i;
    uint64_t z2_low, z2_high, z0_low, z0_high, z1a_low, z1a_high;
    uint128_t z_high = 0, z_low = 0;

    ci_low = ghash->x_low;
    ci_high = ghash->x_high;
    for (i = 0; i < count; i++, buf += 16) {
        ci_low ^= get64(buf + 8);
        ci_high ^= get64(buf);

        /* Do binary mult ghash->X = C * ghash->H (Karatsuba). */
        bmul(ci_high, ghash->h_high, &z2_high, &z2_low);
        bmul(ci_low, ghash->h_low, &z0_high, &z0_low);
        bmul(ci_high ^ ci_low, ghash->h_high ^ ghash->h_low, &z1a_high, &z1a_low);
        z1a_high ^= z2_high ^ z0_high;
        z1a_low ^= z2_low ^ z0_low;
        z_high = ((uint128_t)z2_high << 64) | (z2_low ^ z1a_high);
        z_low = (((uint128_t)z0_high << 64) | z0_low) ^ (((uint128_t)z1a_low) << 64);

        /* Shift one (multiply by x) as gcm spec is stupid. */
        z_high = (z_high << 1) | (z_low >> 127);
        z_low <<= 1;

        /* Reduce */
        z_low ^= (z_low << 127) ^ (z_low << 126) ^ (z_low << 121);
        z_high ^= z_low ^ (z_low >> 1) ^ (z_low >> 2) ^ (z_low >> 7);
        ci_low = (uint64_t)z_high;
        ci_high = (uint64_t)(z_high >> 64);
    }
    ghash->x_low = ci_low;
    ghash->x_high = ci_high;
    return SECSuccess;
}
#else
/* Binary multiplication x * y = r_high << 32 | r_low. */
void
bmul32(uint32_t x, uint32_t y, uint32_t *r_high, uint32_t *r_low)
{
    uint32_t x0, x1, x2, x3;
    uint32_t y0, y1, y2, y3;
    uint32_t m1 = (uint32_t)0x11111111;
    uint32_t m2 = (uint32_t)0x22222222;
    uint32_t m4 = (uint32_t)0x44444444;
    uint32_t m8 = (uint32_t)0x88888888;
    uint64_t z0, z1, z2, z3;
    uint64_t z;

    x0 = x & m1;
    x1 = x & m2;
    x2 = x & m4;
    x3 = x & m8;
    y0 = y & m1;
    y1 = y & m2;
    y2 = y & m4;
    y3 = y & m8;
    z0 = ((uint64_t)x0 * y0) ^ ((uint64_t)x1 * y3) ^
         ((uint64_t)x2 * y2) ^ ((uint64_t)x3 * y1);
    z1 = ((uint64_t)x0 * y1) ^ ((uint64_t)x1 * y0) ^
         ((uint64_t)x2 * y3) ^ ((uint64_t)x3 * y2);
    z2 = ((uint64_t)x0 * y2) ^ ((uint64_t)x1 * y1) ^
         ((uint64_t)x2 * y0) ^ ((uint64_t)x3 * y3);
    z3 = ((uint64_t)x0 * y3) ^ ((uint64_t)x1 * y2) ^
         ((uint64_t)x2 * y1) ^ ((uint64_t)x3 * y0);
    z0 &= ((uint64_t)m1 << 32) | m1;
    z1 &= ((uint64_t)m2 << 32) | m2;
    z2 &= ((uint64_t)m4 << 32) | m4;
    z3 &= ((uint64_t)m8 << 32) | m8;
    z = z0 | z1 | z2 | z3;
    *r_high = (uint32_t)(z >> 32);
    *r_low = (uint32_t)z;
}

SECStatus
gcm_HashMult_sftw32(gcmHashContext *ghash, const unsigned char *buf,
                    unsigned int count)
{
    size_t i;
    uint64_t ci_low, ci_high;
    uint64_t z_high_h, z_high_l, z_low_h, z_low_l;
    uint32_t ci_high_h, ci_high_l, ci_low_h, ci_low_l;
    uint32_t b_a_h, b_a_l, a_a_h, a_a_l, b_b_h, b_b_l;
    uint32_t a_b_h, a_b_l, b_c_h, b_c_l, a_c_h, a_c_l, c_c_h, c_c_l;
    uint32_t ci_highXlow_h, ci_highXlow_l, c_a_h, c_a_l, c_b_h, c_b_l;

    uint32_t h_high_h = (uint32_t)(ghash->h_high >> 32);
    uint32_t h_high_l = (uint32_t)ghash->h_high;
    uint32_t h_low_h = (uint32_t)(ghash->h_low >> 32);
    uint32_t h_low_l = (uint32_t)ghash->h_low;
    uint32_t h_highXlow_h = h_high_h ^ h_low_h;
    uint32_t h_highXlow_l = h_high_l ^ h_low_l;
    uint32_t h_highX_xored = h_highXlow_h ^ h_highXlow_l;

    for (i = 0; i < count; i++, buf += 16) {
        ci_low = ghash->x_low ^ get64(buf + 8);
        ci_high = ghash->x_high ^ get64(buf);
        ci_low_h = (uint32_t)(ci_low >> 32);
        ci_low_l = (uint32_t)ci_low;
        ci_high_h = (uint32_t)(ci_high >> 32);
        ci_high_l = (uint32_t)ci_high;
        ci_highXlow_h = ci_high_h ^ ci_low_h;
        ci_highXlow_l = ci_high_l ^ ci_low_l;

        /* Do binary mult ghash->X = C * ghash->H (recursive Karatsuba). */
        bmul32(ci_high_h, h_high_h, &a_a_h, &a_a_l);
        bmul32(ci_high_l, h_high_l, &a_b_h, &a_b_l);
        bmul32(ci_high_h ^ ci_high_l, h_high_h ^ h_high_l, &a_c_h, &a_c_l);
        a_c_h ^= a_a_h ^ a_b_h;
        a_c_l ^= a_a_l ^ a_b_l;
        a_a_l ^= a_c_h;
        a_b_h ^= a_c_l;
        /* ci_high * h_high = a_a_h:a_a_l:a_b_h:a_b_l */

        bmul32(ci_low_h, h_low_h, &b_a_h, &b_a_l);
        bmul32(ci_low_l, h_low_l, &b_b_h, &b_b_l);
        bmul32(ci_low_h ^ ci_low_l, h_low_h ^ h_low_l, &b_c_h, &b_c_l);
        b_c_h ^= b_a_h ^ b_b_h;
        b_c_l ^= b_a_l ^ b_b_l;
        b_a_l ^= b_c_h;
        b_b_h ^= b_c_l;
        /* ci_low * h_low = b_a_h:b_a_l:b_b_h:b_b_l */

        bmul32(ci_highXlow_h, h_highXlow_h, &c_a_h, &c_a_l);
        bmul32(ci_highXlow_l, h_highXlow_l, &c_b_h, &c_b_l);
        bmul32(ci_highXlow_h ^ ci_highXlow_l, h_highX_xored, &c_c_h, &c_c_l);
        c_c_h ^= c_a_h ^ c_b_h;
        c_c_l ^= c_a_l ^ c_b_l;
        c_a_l ^= c_c_h;
        c_b_h ^= c_c_l;
        /* (ci_high ^ ci_low) * (h_high ^ h_low) = c_a_h:c_a_l:c_b_h:c_b_l */

        c_a_h ^= b_a_h ^ a_a_h;
        c_a_l ^= b_a_l ^ a_a_l;
        c_b_h ^= b_b_h ^ a_b_h;
        c_b_l ^= b_b_l ^ a_b_l;
        z_high_h = ((uint64_t)a_a_h << 32) | a_a_l;
        z_high_l = (((uint64_t)a_b_h << 32) | a_b_l) ^
                   (((uint64_t)c_a_h << 32) | c_a_l);
        z_low_h = (((uint64_t)b_a_h << 32) | b_a_l) ^
                  (((uint64_t)c_b_h << 32) | c_b_l);
        z_low_l = ((uint64_t)b_b_h << 32) | b_b_l;

        /* Shift one (multiply by x) as gcm spec is stupid. */
        z_high_h = z_high_h << 1 | z_high_l >> 63;
        z_high_l = z_high_l << 1 | z_low_h >> 63;
        z_low_h = z_low_h << 1 | z_low_l >> 63;
        z_low_l <<= 1;

        /* Reduce */
        z_low_h ^= (z_low_l << 63) ^ (z_low_l << 62) ^ (z_low_l << 57);
        z_high_h ^= z_low_h ^ (z_low_h >> 1) ^ (z_low_h >> 2) ^ (z_low_h >> 7);
        z_high_l ^= z_low_l ^ (z_low_l >> 1) ^ (z_low_l >> 2) ^ (z_low_l >> 7) ^
                    (z_low_h << 63) ^ (z_low_h << 62) ^ (z_low_h << 57);
        ghash->x_high = z_high_h;
        ghash->x_low = z_high_l;
    }
    return SECSuccess;
}
#endif /* HAVE_INT128_SUPPORT */

static SECStatus
gcm_zeroX(gcmHashContext *ghash)
{
    SECStatus rv = SECSuccess;

    if (ghash->hw) {
        rv = gcm_HashZeroX_hw(ghash);
    }

    ghash->x_high = ghash->x_low = 0;
    return rv;
}

/*
 * implement GCM GHASH using the freebl GHASH function. The gcm_HashMult
 * function always takes AES_BLOCK_SIZE lengths of data. gcmHash_Update will
 * format the data properly.
 */
SECStatus
gcmHash_Update(gcmHashContext *ghash, const unsigned char *buf,
               unsigned int len)
{
    unsigned int blocks;
    SECStatus rv;

    ghash->cLen += (len * PR_BITS_PER_BYTE);

    /* first deal with the current buffer of data. Try to fill it out so
     * we can hash it */
    if (ghash->bufLen) {
        unsigned int needed = PR_MIN(len, AES_BLOCK_SIZE - ghash->bufLen);
        if (needed != 0) {
            PORT_Memcpy(ghash->buffer + ghash->bufLen, buf, needed);
        }
        buf += needed;
        len -= needed;
        ghash->bufLen += needed;
        if (len == 0) {
            /* didn't add enough to hash the data, nothing more do do */
            return SECSuccess;
        }
        PORT_Assert(ghash->bufLen == AES_BLOCK_SIZE);
        /* hash the buffer and clear it */
        rv = ghash->ghash_mul(ghash, ghash->buffer, 1);
        PORT_Memset(ghash->buffer, 0, AES_BLOCK_SIZE);
        ghash->bufLen = 0;
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    /* now hash any full blocks remaining in the data stream */
    blocks = len / AES_BLOCK_SIZE;
    if (blocks) {
        rv = ghash->ghash_mul(ghash, buf, blocks);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        buf += blocks * AES_BLOCK_SIZE;
        len -= blocks * AES_BLOCK_SIZE;
    }

    /* save any remainder in the buffer to be hashed with the next call */
    if (len != 0) {
        PORT_Memcpy(ghash->buffer, buf, len);
        ghash->bufLen = len;
    }
    return SECSuccess;
}

/*
 * write out any partial blocks zero padded through the GHASH engine,
 * save the lengths for the final completion of the hash
 */
static SECStatus
gcmHash_Sync(gcmHashContext *ghash)
{
    int i;
    SECStatus rv;

    /* copy the previous counter to the upper block */
    PORT_Memcpy(ghash->counterBuf, &ghash->counterBuf[GCM_HASH_LEN_LEN],
                GCM_HASH_LEN_LEN);
    /* copy the current counter in the lower block */
    for (i = 0; i < GCM_HASH_LEN_LEN; i++) {
        ghash->counterBuf[GCM_HASH_LEN_LEN + i] =
            (ghash->cLen >> ((GCM_HASH_LEN_LEN - 1 - i) * PR_BITS_PER_BYTE)) & 0xff;
    }
    ghash->cLen = 0;

    /* now zero fill the buffer and hash the last block */
    if (ghash->bufLen) {
        PORT_Memset(ghash->buffer + ghash->bufLen, 0, AES_BLOCK_SIZE - ghash->bufLen);
        rv = ghash->ghash_mul(ghash, ghash->buffer, 1);
        PORT_Memset(ghash->buffer, 0, AES_BLOCK_SIZE);
        ghash->bufLen = 0;
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    return SECSuccess;
}

#define WRITE64(x, bytes)   \
    (bytes)[0] = (x) >> 56; \
    (bytes)[1] = (x) >> 48; \
    (bytes)[2] = (x) >> 40; \
    (bytes)[3] = (x) >> 32; \
    (bytes)[4] = (x) >> 24; \
    (bytes)[5] = (x) >> 16; \
    (bytes)[6] = (x) >> 8;  \
    (bytes)[7] = (x);

/*
 * This does the final sync, hashes the lengths, then returns
 * "T", the hashed output.
 */
SECStatus
gcmHash_Final(gcmHashContext *ghash, unsigned char *outbuf,
              unsigned int *outlen, unsigned int maxout)
{
    unsigned char T[MAX_BLOCK_SIZE];
    SECStatus rv;

    rv = gcmHash_Sync(ghash);
    if (rv != SECSuccess) {
        goto cleanup;
    }

    rv = ghash->ghash_mul(ghash, ghash->counterBuf,
                          (GCM_HASH_LEN_LEN * 2) / AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        goto cleanup;
    }

    if (ghash->hw) {
        rv = gcm_HashWrite_hw(ghash, T);
        if (rv != SECSuccess) {
            goto cleanup;
        }
    } else {
        WRITE64(ghash->x_low, T + 8);
        WRITE64(ghash->x_high, T);
    }

    if (maxout > AES_BLOCK_SIZE) {
        maxout = AES_BLOCK_SIZE;
    }
    PORT_Memcpy(outbuf, T, maxout);
    *outlen = maxout;
    rv = SECSuccess;

cleanup:
    PORT_Memset(T, 0, sizeof(T));
    return rv;
}

SECStatus
gcmHash_Reset(gcmHashContext *ghash, const unsigned char *AAD,
              unsigned int AADLen)
{
    SECStatus rv;

    // Limit AADLen in accordance with SP800-38D
    if (sizeof(AADLen) >= 8 && AADLen > (1ULL << 61) - 1) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    ghash->cLen = 0;
    PORT_Memset(ghash->counterBuf, 0, GCM_HASH_LEN_LEN * 2);
    ghash->bufLen = 0;
    rv = gcm_zeroX(ghash);
    if (rv != SECSuccess) {
        return rv;
    }

    /* now kick things off by hashing the Additional Authenticated Data */
    if (AADLen != 0) {
        rv = gcmHash_Update(ghash, AAD, AADLen);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        rv = gcmHash_Sync(ghash);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    return SECSuccess;
}

/**************************************************************************
 *           Now implement the GCM using gcmHash and CTR                  *
 **************************************************************************/

/* state to handle the full GCM operation (hash and counter) */
struct GCMContextStr {
    gcmHashContext *ghash_context;
    CTRContext ctr_context;
    freeblCipherFunc cipher;
    void *cipher_context;
    unsigned long tagBits;
    unsigned char tagKey[MAX_BLOCK_SIZE];
    PRBool ctr_context_init;
    gcmIVContext gcm_iv;
};

SECStatus gcm_InitCounter(GCMContext *gcm, const unsigned char *iv,
                          unsigned int ivLen, unsigned int tagBits,
                          const unsigned char *aad, unsigned int aadLen);

GCMContext *
GCM_CreateContext(void *context, freeblCipherFunc cipher,
                  const unsigned char *params)
{
    GCMContext *gcm = NULL;
    gcmHashContext *ghash = NULL;
    unsigned char H[MAX_BLOCK_SIZE];
    unsigned int tmp;
    const CK_NSS_GCM_PARAMS *gcmParams = (const CK_NSS_GCM_PARAMS *)params;
    SECStatus rv;
#ifdef DISABLE_HW_GCM
    const PRBool sw = PR_TRUE;
#else
    const PRBool sw = PR_FALSE;
#endif

    gcm = PORT_ZNew(GCMContext);
    if (gcm == NULL) {
        return NULL;
    }
    gcm->cipher = cipher;
    gcm->cipher_context = context;
    ghash = PORT_ZNewAligned(gcmHashContext, 16, mem);

    /* first plug in the ghash context */
    gcm->ghash_context = ghash;
    PORT_Memset(H, 0, AES_BLOCK_SIZE);
    rv = (*cipher)(context, H, &tmp, AES_BLOCK_SIZE, H, AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = gcmHash_InitContext(ghash, H, sw);
    if (rv != SECSuccess) {
        goto loser;
    }

    gcm_InitIVContext(&gcm->gcm_iv);
    gcm->ctr_context_init = PR_FALSE;

    /* if gcmPara/ms is NULL, then we are creating an PKCS #11 MESSAGE
     * style context, in which we initialize the key once, then do separate
     * iv/aad's for each message. In that case we only initialize the key
     * and ghash. We initialize the counter in each separate message */
    if (gcmParams == NULL) {
        /* OK we are finished with init, if we are doing MESSAGE interface,
         * return from here */
        return gcm;
    }

    rv = gcm_InitCounter(gcm, gcmParams->pIv, gcmParams->ulIvLen,
                         gcmParams->ulTagBits, gcmParams->pAAD,
                         gcmParams->ulAADLen);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_Memset(H, 0, AES_BLOCK_SIZE);
    gcm->ctr_context_init = PR_TRUE;
    return gcm;

loser:
    PORT_Memset(H, 0, AES_BLOCK_SIZE);
    if (ghash && ghash->mem) {
        void *mem = ghash->mem;
        PORT_Memset(ghash, 0, sizeof(gcmHashContext));
        PORT_Free(mem);
    }
    if (gcm) {
        PORT_ZFree(gcm, sizeof(GCMContext));
    }
    return NULL;
}

SECStatus
gcm_InitCounter(GCMContext *gcm, const unsigned char *iv, unsigned int ivLen,
                unsigned int tagBits, const unsigned char *aad,
                unsigned int aadLen)
{
    gcmHashContext *ghash = gcm->ghash_context;
    unsigned int tmp;
    PRBool freeCtr = PR_FALSE;
    CK_AES_CTR_PARAMS ctrParams;
    SECStatus rv;

    /* Verify our parameters here */
    if (ivLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    if (tagBits != 128 && tagBits != 120 &&
        tagBits != 112 && tagBits != 104 &&
        tagBits != 96 && tagBits != 64 &&
        tagBits != 32) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    /* fill in the Counter context */
    ctrParams.ulCounterBits = 32;
    PORT_Memset(ctrParams.cb, 0, sizeof(ctrParams.cb));
    if (ivLen == 12) {
        PORT_Memcpy(ctrParams.cb, iv, ivLen);
        ctrParams.cb[AES_BLOCK_SIZE - 1] = 1;
    } else {
        rv = gcmHash_Reset(ghash, NULL, 0);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = gcmHash_Update(ghash, iv, ivLen);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = gcmHash_Final(ghash, ctrParams.cb, &tmp, AES_BLOCK_SIZE);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    rv = CTR_InitContext(&gcm->ctr_context, gcm->cipher_context, gcm->cipher,
                         (unsigned char *)&ctrParams);
    if (rv != SECSuccess) {
        goto loser;
    }
    freeCtr = PR_TRUE;

    /* fill in the gcm structure */
    gcm->tagBits = tagBits; /* save for final step */
    /* calculate the final tag key. NOTE: gcm->tagKey is zero to start with.
     * if this assumption changes, we would need to explicitly clear it here */
    PORT_Memset(gcm->tagKey, 0, sizeof(gcm->tagKey));
    rv = CTR_Update(&gcm->ctr_context, gcm->tagKey, &tmp, AES_BLOCK_SIZE,
                    gcm->tagKey, AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* finally mix in the AAD data */
    rv = gcmHash_Reset(ghash, aad, aadLen);
    if (rv != SECSuccess) {
        goto loser;
    }

    PORT_Memset(&ctrParams, 0, sizeof ctrParams);
    return SECSuccess;

loser:
    PORT_Memset(&ctrParams, 0, sizeof ctrParams);
    if (freeCtr) {
        CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
    }
    return SECFailure;
}

void
GCM_DestroyContext(GCMContext *gcm, PRBool freeit)
{
    void *mem = gcm->ghash_context->mem;
    /* ctr_context is statically allocated and will be freed when we free
     * gcm. call their destroy functions to free up any locally
     * allocated data (like mp_int's) */
    if (gcm->ctr_context_init) {
        CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
    }
    PORT_Memset(gcm->ghash_context, 0, sizeof(gcmHashContext));
    PORT_Free(mem);
    PORT_Memset(&gcm->tagBits, 0, sizeof(gcm->tagBits));
    PORT_Memset(gcm->tagKey, 0, sizeof(gcm->tagKey));
    if (freeit) {
        PORT_Free(gcm);
    }
}

static SECStatus
gcm_GetTag(GCMContext *gcm, unsigned char *outbuf,
           unsigned int *outlen, unsigned int maxout)
{
    unsigned int tagBytes;
    unsigned int extra;
    unsigned int i;
    SECStatus rv;

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;
    extra = tagBytes * PR_BITS_PER_BYTE - gcm->tagBits;

    if (outbuf == NULL) {
        *outlen = tagBytes;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    if (maxout < tagBytes) {
        *outlen = tagBytes;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    maxout = tagBytes;
    rv = gcmHash_Final(gcm->ghash_context, outbuf, outlen, maxout);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    for (i = 0; i < *outlen; i++) {
        outbuf[i] ^= gcm->tagKey[i];
    }
    /* mask off any extra bits we got */
    if (extra) {
        outbuf[tagBytes - 1] &= ~((1 << extra) - 1);
    }
    return SECSuccess;
}

/*
 * See The Galois/Counter Mode of Operation, McGrew and Viega.
 *  GCM is basically counter mode with a specific initialization and
 *  built in macing operation.
 */
SECStatus
GCM_EncryptUpdate(GCMContext *gcm, unsigned char *outbuf,
                  unsigned int *outlen, unsigned int maxout,
                  const unsigned char *inbuf, unsigned int inlen,
                  unsigned int blocksize)
{
    SECStatus rv;
    unsigned int tagBytes;
    unsigned int len;

    PORT_Assert(blocksize == AES_BLOCK_SIZE);
    if (blocksize != AES_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    if (!gcm->ctr_context_init) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;
    if (UINT_MAX - inlen < tagBytes) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    if (maxout < inlen + tagBytes) {
        *outlen = inlen + tagBytes;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    rv = CTR_Update(&gcm->ctr_context, outbuf, outlen, maxout,
                    inbuf, inlen, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = gcmHash_Update(gcm->ghash_context, outbuf, *outlen);
    if (rv != SECSuccess) {
        PORT_Memset(outbuf, 0, *outlen); /* clear the output buffer */
        *outlen = 0;
        return SECFailure;
    }
    rv = gcm_GetTag(gcm, outbuf + *outlen, &len, maxout - *outlen);
    if (rv != SECSuccess) {
        PORT_Memset(outbuf, 0, *outlen); /* clear the output buffer */
        *outlen = 0;
        return SECFailure;
    };
    *outlen += len;
    return SECSuccess;
}

/*
 * See The Galois/Counter Mode of Operation, McGrew and Viega.
 *  GCM is basically counter mode with a specific initialization and
 *  built in macing operation. NOTE: the only difference between Encrypt
 *  and Decrypt is when we calculate the mac. That is because the mac must
 *  always be calculated on the cipher text, not the plain text, so for
 *  encrypt, we do the CTR update first and for decrypt we do the mac first.
 */
SECStatus
GCM_DecryptUpdate(GCMContext *gcm, unsigned char *outbuf,
                  unsigned int *outlen, unsigned int maxout,
                  const unsigned char *inbuf, unsigned int inlen,
                  unsigned int blocksize)
{
    SECStatus rv;
    unsigned int tagBytes;
    unsigned char tag[MAX_BLOCK_SIZE];
    const unsigned char *intag;
    unsigned int len;

    PORT_Assert(blocksize == AES_BLOCK_SIZE);
    if (blocksize != AES_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    if (!gcm->ctr_context_init) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;

    /* get the authentication block */
    if (inlen < tagBytes) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    inlen -= tagBytes;
    intag = inbuf + inlen;

    /* verify the block */
    rv = gcmHash_Update(gcm->ghash_context, inbuf, inlen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = gcm_GetTag(gcm, tag, &len, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* Don't decrypt if we can't authenticate the encrypted data!
     * This assumes that if tagBits is not a multiple of 8, intag will
     * preserve the masked off missing bits.  */
    if (NSS_SecureMemcmp(tag, intag, tagBytes) != 0) {
        /* force a CKR_ENCRYPTED_DATA_INVALID error at in softoken */
        PORT_SetError(SEC_ERROR_BAD_DATA);
        PORT_Memset(tag, 0, sizeof(tag));
        return SECFailure;
    }
    PORT_Memset(tag, 0, sizeof(tag));
    /* finish the decryption */
    return CTR_Update(&gcm->ctr_context, outbuf, outlen, maxout,
                      inbuf, inlen, AES_BLOCK_SIZE);
}

void
gcm_InitIVContext(gcmIVContext *gcmIv)
{
    gcmIv->counter = 0;
    gcmIv->max_count = 0;
    gcmIv->ivGen = CKG_GENERATE;
    gcmIv->ivLen = 0;
    gcmIv->fixedBits = 0;
}

/*
 * generate the IV on the fly and return it to the application.
 *   This function keeps a counter, which may be used in the IV
 *   generation, or may be used in simply to make sure we don't
 *   generate to many IV's from this same key.
 *   PKCS #11 defines 4 generating values:
 *       1) CKG_NO_GENERATE: just use the passed in IV as it.
 *       2) CKG_GENERATE: the application doesn't care what generation
 *       scheme is use (we default to counter in this code).
 *       3) CKG_GENERATE_COUNTER: The IV is the value of a counter.
 *       4) CKG_GENERATE_RANDOM: The IV is randomly generated.
 *   We add a fifth rule:
 *       5) CKG_GENERATE_COUNTER_XOR: The Counter value is xor'ed with
 *       the IV.
 *   The value fixedBits specifies the number of bits that will be passed
 *   on from the original IV. The counter or the random data is is loaded
 *   in the remainder of the IV not covered by fixedBits, overwriting any
 *   data there. In the xor case the counter is xor'ed with the data in the
 *   IV. In all cases only bits outside of fixedBits is modified.
 *   The number of IV's we can generate is restricted by the size of the
 *   variable part of the IV and the generation algorithm used. Because of
 *   this, we require subsequent calls on this context to use the same
 *   generator, IV len, and fixed bits as the first call.
 */
SECStatus
gcm_GenerateIV(gcmIVContext *gcmIv, unsigned char *iv, unsigned int ivLen,
               unsigned int fixedBits, CK_GENERATOR_FUNCTION ivGen)
{
    unsigned int i;
    unsigned int flexBits;
    unsigned int ivOffset;
    unsigned int ivNewCount;
    unsigned char ivMask;
    unsigned char ivSave;
    SECStatus rv;

    if (gcmIv->counter != 0) {
        /* If we've already generated a message, make sure all subsequent
         * messages are using the same generator */
        if ((gcmIv->ivGen != ivGen) || (gcmIv->fixedBits != fixedBits) ||
            (gcmIv->ivLen != ivLen)) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
    } else {
        /* remember these values */
        gcmIv->ivGen = ivGen;
        gcmIv->fixedBits = fixedBits;
        gcmIv->ivLen = ivLen;
        /* now calculate how may bits of IV we have to supply */
        flexBits = ivLen * PR_BITS_PER_BYTE; /* bytes->bits */
        /* first make sure we aren't going to overflow */
        if (flexBits < fixedBits) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        flexBits -= fixedBits;
        /* if we are generating a random number reduce the acceptable bits to
         * avoid birthday attacks */
        if (ivGen == CKG_GENERATE_RANDOM) {
            if (flexBits <= GCMIV_RANDOM_BIRTHDAY_BITS) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            /* see freebl/blapit.h for how we calculate
             * GCMIV_RANDOM_BIRTHDAY_BITS */
            flexBits -= GCMIV_RANDOM_BIRTHDAY_BITS;
            flexBits = flexBits >> 1;
        }
        if (flexBits == 0) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        /* Turn those bits into the number of IV's we can safely return */
        if (flexBits >= sizeof(gcmIv->max_count) * PR_BITS_PER_BYTE) {
            gcmIv->max_count = PR_UINT64(0xffffffffffffffff);
        } else {
            gcmIv->max_count = PR_UINT64(1) << flexBits;
        }
    }

    /* no generate, accept the IV from the source */
    if (ivGen == CKG_NO_GENERATE) {
        gcmIv->counter = 1;
        return SECSuccess;
    }

    /* make sure we haven't exceeded the number of IVs we can return
     * for this key, generator, and IV size */
    if (gcmIv->counter >= gcmIv->max_count) {
        /* use a unique error from just bad user input */
        PORT_SetError(SEC_ERROR_EXTRA_INPUT);
        return SECFailure;
    }

    /* build to mask to handle the first byte of the IV */
    ivOffset = fixedBits / PR_BITS_PER_BYTE;
    ivMask = 0xff >> ((8 - (fixedBits & 7)) & 7);
    ivNewCount = ivLen - ivOffset;

    /* finally generate the IV */
    switch (ivGen) {
        case CKG_GENERATE: /* default to counter */
        case CKG_GENERATE_COUNTER:
            iv[ivOffset] = (iv[ivOffset] & ~ivMask) |
                           (PORT_GET_BYTE_BE(gcmIv->counter, 0, ivNewCount) & ivMask);
            for (i = 1; i < ivNewCount; i++) {
                iv[ivOffset + i] = PORT_GET_BYTE_BE(gcmIv->counter, i, ivNewCount);
            }
            break;
        /* for TLS 1.3 */
        case CKG_GENERATE_COUNTER_XOR:
            iv[ivOffset] ^=
                (PORT_GET_BYTE_BE(gcmIv->counter, 0, ivNewCount) & ivMask);
            for (i = 1; i < ivNewCount; i++) {
                iv[ivOffset + i] ^= PORT_GET_BYTE_BE(gcmIv->counter, i, ivNewCount);
            }
            break;
        case CKG_GENERATE_RANDOM:
            ivSave = iv[ivOffset] & ~ivMask;
            rv = RNG_GenerateGlobalRandomBytes(iv + ivOffset, ivNewCount);
            iv[ivOffset] = ivSave | (iv[ivOffset] & ivMask);
            if (rv != SECSuccess) {
                return rv;
            }
            break;
    }
    gcmIv->counter++;
    return SECSuccess;
}

SECStatus
GCM_EncryptAEAD(GCMContext *gcm, unsigned char *outbuf,
                unsigned int *outlen, unsigned int maxout,
                const unsigned char *inbuf, unsigned int inlen,
                void *params, unsigned int paramLen,
                const unsigned char *aad, unsigned int aadLen,
                unsigned int blocksize)
{
    SECStatus rv;
    unsigned int tagBytes;
    unsigned int len;
    const CK_GCM_MESSAGE_PARAMS *gcmParams =
        (const CK_GCM_MESSAGE_PARAMS *)params;

    PORT_Assert(blocksize == AES_BLOCK_SIZE);
    if (blocksize != AES_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* paramLen comes all the way from the application layer, make sure
     * it's correct */
    if (paramLen != sizeof(CK_GCM_MESSAGE_PARAMS)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    /* if we were initialized with the C_EncryptInit, we shouldn't be in this
     * function */
    if (gcm->ctr_context_init) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    if (maxout < inlen) {
        *outlen = inlen;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    rv = gcm_GenerateIV(&gcm->gcm_iv, gcmParams->pIv, gcmParams->ulIvLen,
                        gcmParams->ulIvFixedBits, gcmParams->ivGenerator);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = gcm_InitCounter(gcm, gcmParams->pIv, gcmParams->ulIvLen,
                         gcmParams->ulTagBits, aad, aadLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;

    rv = CTR_Update(&gcm->ctr_context, outbuf, outlen, maxout,
                    inbuf, inlen, AES_BLOCK_SIZE);
    CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = gcmHash_Update(gcm->ghash_context, outbuf, *outlen);
    if (rv != SECSuccess) {
        PORT_Memset(outbuf, 0, *outlen); /* clear the output buffer */
        *outlen = 0;
        return SECFailure;
    }
    rv = gcm_GetTag(gcm, gcmParams->pTag, &len, tagBytes);
    if (rv != SECSuccess) {
        PORT_Memset(outbuf, 0, *outlen); /* clear the output buffer */
        *outlen = 0;
        return SECFailure;
    };
    return SECSuccess;
}

SECStatus
GCM_DecryptAEAD(GCMContext *gcm, unsigned char *outbuf,
                unsigned int *outlen, unsigned int maxout,
                const unsigned char *inbuf, unsigned int inlen,
                void *params, unsigned int paramLen,
                const unsigned char *aad, unsigned int aadLen,
                unsigned int blocksize)
{
    SECStatus rv;
    unsigned int tagBytes;
    unsigned char tag[MAX_BLOCK_SIZE];
    const unsigned char *intag;
    unsigned int len;
    const CK_GCM_MESSAGE_PARAMS *gcmParams =
        (const CK_GCM_MESSAGE_PARAMS *)params;

    PORT_Assert(blocksize == AES_BLOCK_SIZE);
    if (blocksize != AES_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* paramLen comes all the way from the application layer, make sure
     * it's correct */
    if (paramLen != sizeof(CK_GCM_MESSAGE_PARAMS)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    /* if we were initialized with the C_DecryptInit, we shouldn't be in this
     * function */
    if (gcm->ctr_context_init) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    if (maxout < inlen) {
        *outlen = inlen;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    rv = gcm_InitCounter(gcm, gcmParams->pIv, gcmParams->ulIvLen,
                         gcmParams->ulTagBits, aad, aadLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;
    intag = gcmParams->pTag;
    PORT_Assert(tagBytes != 0);

    /* verify the block */
    rv = gcmHash_Update(gcm->ghash_context, inbuf, inlen);
    if (rv != SECSuccess) {
        CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
        return SECFailure;
    }
    rv = gcm_GetTag(gcm, tag, &len, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
        return SECFailure;
    }
    /* Don't decrypt if we can't authenticate the encrypted data!
     * This assumes that if tagBits is may not be a multiple of 8, intag will
     * preserve the masked off missing bits.  */
    if (NSS_SecureMemcmp(tag, intag, tagBytes) != 0) {
        /* force a CKR_ENCRYPTED_DATA_INVALID error at in softoken */
        CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
        PORT_SetError(SEC_ERROR_BAD_DATA);
        PORT_Memset(tag, 0, sizeof(tag));
        return SECFailure;
    }
    PORT_Memset(tag, 0, sizeof(tag));
    /* finish the decryption */
    rv = CTR_Update(&gcm->ctr_context, outbuf, outlen, maxout,
                    inbuf, inlen, AES_BLOCK_SIZE);
    CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
    return rv;
}
