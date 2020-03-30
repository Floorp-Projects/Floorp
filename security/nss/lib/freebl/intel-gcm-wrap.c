/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* Copyright(c) 2013, Intel Corp. */

/* Wrapper functions for Intel optimized implementation of AES-GCM */

#ifdef USE_HW_AES

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapii.h"
#include "blapit.h"
#include "gcm.h"
#include "ctr.h"
#include "secerr.h"
#include "prtypes.h"
#include "pkcs11t.h"

#include <limits.h>

#include "intel-gcm.h"
#include "rijndael.h"

#include <emmintrin.h>
#include <tmmintrin.h>

struct intel_AES_GCMContextStr {
    unsigned char Htbl[16 * AES_BLOCK_SIZE];
    unsigned char X0[AES_BLOCK_SIZE];
    unsigned char T[AES_BLOCK_SIZE];
    unsigned char CTR[AES_BLOCK_SIZE];
    AESContext *aes_context;
    unsigned long tagBits;
    unsigned long Alen;
    unsigned long Mlen;
};

intel_AES_GCMContext *
intel_AES_GCM_CreateContext(void *context,
                            freeblCipherFunc cipher,
                            const unsigned char *params)
{
    intel_AES_GCMContext *gcm = NULL;
    AESContext *aes = (AESContext *)context;
    const CK_GCM_PARAMS *gcmParams = (const CK_GCM_PARAMS *)params;
    unsigned char buff[AES_BLOCK_SIZE]; /* aux buffer */

    unsigned long IV_whole_len = gcmParams->ulIvLen & (~0xful);
    unsigned int IV_remainder_len = gcmParams->ulIvLen & 0xful;
    unsigned long AAD_whole_len = gcmParams->ulAADLen & (~0xful);
    unsigned int AAD_remainder_len = gcmParams->ulAADLen & 0xful;

    __m128i BSWAP_MASK = _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __m128i ONE = _mm_set_epi32(0, 0, 0, 1);
    unsigned int j;
    SECStatus rv;

    if (gcmParams->ulIvLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    if (gcmParams->ulTagBits != 128 && gcmParams->ulTagBits != 120 &&
        gcmParams->ulTagBits != 112 && gcmParams->ulTagBits != 104 &&
        gcmParams->ulTagBits != 96 && gcmParams->ulTagBits != 64 &&
        gcmParams->ulTagBits != 32) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    // Limit AADLen in accordance with SP800-38D
    if (sizeof(AAD_whole_len) >= 8 && AAD_whole_len > (1ULL << 61) - 1) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return NULL;
    }

    gcm = PORT_ZNew(intel_AES_GCMContext);
    if (gcm == NULL) {
        return NULL;
    }

    /* initialize context fields */
    gcm->aes_context = aes;
    gcm->tagBits = gcmParams->ulTagBits;
    gcm->Alen = 0;
    gcm->Mlen = 0;

    /* first prepare H and its derivatives for ghash */
    intel_aes_gcmINIT(gcm->Htbl, (unsigned char *)aes->k.expandedKey, aes->Nr);

    /* Initial TAG value is zero */
    _mm_storeu_si128((__m128i *)gcm->T, _mm_setzero_si128());
    _mm_storeu_si128((__m128i *)gcm->X0, _mm_setzero_si128());

    /* Init the counter */
    if (gcmParams->ulIvLen == 12) {
        _mm_storeu_si128((__m128i *)gcm->CTR,
                         _mm_setr_epi32(((unsigned int *)gcmParams->pIv)[0],
                                        ((unsigned int *)gcmParams->pIv)[1],
                                        ((unsigned int *)gcmParams->pIv)[2],
                                        0x01000000));
    } else {
        /* If IV size is not 96 bits, then the initial counter value is GHASH
         * of the IV */
        intel_aes_gcmAAD(gcm->Htbl, gcmParams->pIv, IV_whole_len, gcm->T);

        /* Partial block */
        if (IV_remainder_len) {
            PORT_Memset(buff, 0, AES_BLOCK_SIZE);
            PORT_Memcpy(buff, gcmParams->pIv + IV_whole_len, IV_remainder_len);
            intel_aes_gcmAAD(gcm->Htbl, buff, AES_BLOCK_SIZE, gcm->T);
        }

        intel_aes_gcmTAG(
            gcm->Htbl,
            gcm->T,
            gcmParams->ulIvLen,
            0,
            gcm->X0,
            gcm->CTR);

        /* TAG should be zero again */
        _mm_storeu_si128((__m128i *)gcm->T, _mm_setzero_si128());
    }

    /* Encrypt the initial counter, will be used to encrypt the GHASH value,
     * in the end */
    rv = (*cipher)(context, gcm->X0, &j, AES_BLOCK_SIZE, gcm->CTR,
                   AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Promote the counter by 1 */
    _mm_storeu_si128((__m128i *)gcm->CTR, _mm_shuffle_epi8(_mm_add_epi32(ONE, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)gcm->CTR), BSWAP_MASK)), BSWAP_MASK));

    /* Now hash AAD - it would actually make sense to seperate the context
     * creation from the AAD, because that would allow to reuse the H, which
     * only changes when the AES key changes, and not every package, like the
     * IV and AAD */
    intel_aes_gcmAAD(gcm->Htbl, gcmParams->pAAD, AAD_whole_len, gcm->T);
    if (AAD_remainder_len) {
        PORT_Memset(buff, 0, AES_BLOCK_SIZE);
        PORT_Memcpy(buff, gcmParams->pAAD + AAD_whole_len, AAD_remainder_len);
        intel_aes_gcmAAD(gcm->Htbl, buff, AES_BLOCK_SIZE, gcm->T);
    }
    gcm->Alen += gcmParams->ulAADLen;
    return gcm;

loser:
    PORT_Free(gcm);
    return NULL;
}

void
intel_AES_GCM_DestroyContext(intel_AES_GCMContext *gcm, PRBool freeit)
{
    PORT_Memset(gcm, 0, sizeof(intel_AES_GCMContext));
    if (freeit) {
        PORT_Free(gcm);
    }
}

SECStatus
intel_AES_GCM_EncryptUpdate(intel_AES_GCMContext *gcm,
                            unsigned char *outbuf,
                            unsigned int *outlen, unsigned int maxout,
                            const unsigned char *inbuf, unsigned int inlen,
                            unsigned int blocksize)
{
    unsigned int tagBytes;
    unsigned char T[AES_BLOCK_SIZE];
    unsigned int j;

    // GCM has a 16 octet block, with a 32-bit block counter
    // Limit in accordance with SP800-38D
    if (sizeof(inlen) > 4 &&
        inlen >= ((1ULL << 32) - 2) * AES_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
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

    intel_aes_gcmENC(
        inbuf,
        outbuf,
        gcm,
        inlen);

    gcm->Mlen += inlen;

    intel_aes_gcmTAG(
        gcm->Htbl,
        gcm->T,
        gcm->Mlen,
        gcm->Alen,
        gcm->X0,
        T);

    *outlen = inlen + tagBytes;

    for (j = 0; j < tagBytes; j++) {
        outbuf[inlen + j] = T[j];
    }
    return SECSuccess;
}

SECStatus
intel_AES_GCM_DecryptUpdate(intel_AES_GCMContext *gcm,
                            unsigned char *outbuf,
                            unsigned int *outlen, unsigned int maxout,
                            const unsigned char *inbuf, unsigned int inlen,
                            unsigned int blocksize)
{
    unsigned int tagBytes;
    unsigned char T[AES_BLOCK_SIZE];
    const unsigned char *intag;

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;

    /* get the authentication block */
    if (inlen < tagBytes) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    inlen -= tagBytes;
    intag = inbuf + inlen;

    // GCM has a 16 octet block, with a 32-bit block counter
    // Limit in accordance with SP800-38D
    if (sizeof(inlen) > 4 &&
        inlen >= ((1ULL << 32) - 2) * AES_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    if (maxout < inlen) {
        *outlen = inlen;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    intel_aes_gcmDEC(
        inbuf,
        outbuf,
        gcm,
        inlen);

    gcm->Mlen += inlen;
    intel_aes_gcmTAG(
        gcm->Htbl,
        gcm->T,
        gcm->Mlen,
        gcm->Alen,
        gcm->X0,
        T);

    if (NSS_SecureMemcmp(T, intag, tagBytes) != 0) {
        memset(outbuf, 0, inlen);
        *outlen = 0;
        /* force a CKR_ENCRYPTED_DATA_INVALID error at in softoken */
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }
    *outlen = inlen;

    return SECSuccess;
}

#endif
