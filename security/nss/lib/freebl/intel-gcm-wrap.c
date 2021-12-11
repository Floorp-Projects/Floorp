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
    freeblCipherFunc cipher;
    PRBool ctr_context_init;
    gcmIVContext gcm_iv;
};

SECStatus intel_aes_gcmInitCounter(intel_AES_GCMContext *gcm,
                                   const unsigned char *iv,
                                   unsigned long ivLen, unsigned long tagBits,
                                   const unsigned char *aad, unsigned long aadLen);

intel_AES_GCMContext *
intel_AES_GCM_CreateContext(void *context,
                            freeblCipherFunc cipher,
                            const unsigned char *params)
{
    intel_AES_GCMContext *gcm = NULL;
    AESContext *aes = (AESContext *)context;
    const CK_NSS_GCM_PARAMS *gcmParams = (const CK_NSS_GCM_PARAMS *)params;
    SECStatus rv;

    gcm = PORT_ZNew(intel_AES_GCMContext);
    if (gcm == NULL) {
        return NULL;
    }

    /* initialize context fields */
    gcm->aes_context = aes;
    gcm->cipher = cipher;
    gcm->Alen = 0;
    gcm->Mlen = 0;
    gcm->ctr_context_init = PR_FALSE;

    /* first prepare H and its derivatives for ghash */
    intel_aes_gcmINIT(gcm->Htbl, (unsigned char *)aes->k.expandedKey, aes->Nr);

    gcm_InitIVContext(&gcm->gcm_iv);

    /* if gcmParams is NULL, then we are creating an PKCS #11 MESSAGE
     * style context, in which we initialize the key once, then do separate
     * iv/aad's for each message. If we are doing that kind of operation,
     * we've finished with init here. We'll init the Counter in each AEAD
     * call */
    if (gcmParams == NULL) {
        return gcm;
    }

    rv = intel_aes_gcmInitCounter(gcm, gcmParams->pIv,
                                  gcmParams->ulIvLen, gcmParams->ulTagBits,
                                  gcmParams->pAAD, gcmParams->ulAADLen);
    if (rv != SECSuccess) {
        PORT_Free(gcm);
        return NULL;
    }
    gcm->ctr_context_init = PR_TRUE;

    return gcm;
}

SECStatus
intel_aes_gcmInitCounter(intel_AES_GCMContext *gcm,
                         const unsigned char *iv, unsigned long ivLen,
                         unsigned long tagBits,
                         const unsigned char *aad, unsigned long aadLen)
{
    unsigned char buff[AES_BLOCK_SIZE]; /* aux buffer */
    unsigned long IV_whole_len = ivLen & (~0xful);
    unsigned int IV_remainder_len = ivLen & 0xful;
    unsigned long AAD_whole_len = aadLen & (~0xful);
    unsigned int AAD_remainder_len = aadLen & 0xful;
    unsigned int j;
    __m128i BSWAP_MASK = _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __m128i ONE = _mm_set_epi32(0, 0, 0, 1);
    SECStatus rv;

    if (ivLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (tagBits != 128 && tagBits != 120 && tagBits != 112 &&
        tagBits != 104 && tagBits != 96 && tagBits != 64 &&
        tagBits != 32) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    gcm->tagBits = tagBits;

    /* reset the aad and message length counters */
    gcm->Alen = 0;
    gcm->Mlen = 0;

    // Limit AADLen in accordance with SP800-38D
    if (sizeof(AAD_whole_len) >= 8 && AAD_whole_len > (1ULL << 61) - 1) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    /* Initial TAG value is zero */
    _mm_storeu_si128((__m128i *)gcm->T, _mm_setzero_si128());
    _mm_storeu_si128((__m128i *)gcm->X0, _mm_setzero_si128());

    /* Init the counter */
    if (ivLen == 12) {
        _mm_storeu_si128((__m128i *)gcm->CTR,
                         _mm_setr_epi32(((unsigned int *)iv)[0],
                                        ((unsigned int *)iv)[1],
                                        ((unsigned int *)iv)[2],
                                        0x01000000));
    } else {
        /* If IV size is not 96 bits, then the initial counter value is GHASH
         * of the IV */
        intel_aes_gcmAAD(gcm->Htbl, (unsigned char *)iv, IV_whole_len, gcm->T);

        /* Partial block */
        if (IV_remainder_len) {
            PORT_Memset(buff, 0, AES_BLOCK_SIZE);
            PORT_Memcpy(buff, iv + IV_whole_len, IV_remainder_len);
            intel_aes_gcmAAD(gcm->Htbl, buff, AES_BLOCK_SIZE, gcm->T);
        }

        intel_aes_gcmTAG(
            gcm->Htbl,
            gcm->T,
            ivLen,
            0,
            gcm->X0,
            gcm->CTR);

        /* TAG should be zero again */
        _mm_storeu_si128((__m128i *)gcm->T, _mm_setzero_si128());
    }

    /* Encrypt the initial counter, will be used to encrypt the GHASH value,
     * in the end */
    rv = (*gcm->cipher)(gcm->aes_context, gcm->X0, &j, AES_BLOCK_SIZE, gcm->CTR,
                        AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Promote the counter by 1 */
    _mm_storeu_si128((__m128i *)gcm->CTR, _mm_shuffle_epi8(_mm_add_epi32(ONE, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)gcm->CTR), BSWAP_MASK)), BSWAP_MASK));

    /* Now hash AAD - it would actually make sense to seperate the context
     * creation from the AAD, because that would allow to reuse the H, which
     * only changes when the AES key changes, and not every package, like the
     * IV and AAD */
    intel_aes_gcmAAD(gcm->Htbl, (unsigned char *)aad, AAD_whole_len, gcm->T);
    if (AAD_remainder_len) {
        PORT_Memset(buff, 0, AES_BLOCK_SIZE);
        PORT_Memcpy(buff, aad + AAD_whole_len, AAD_remainder_len);
        intel_aes_gcmAAD(gcm->Htbl, buff, AES_BLOCK_SIZE, gcm->T);
    }
    gcm->Alen += aadLen;
    return SECSuccess;
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

    if (!gcm->ctr_context_init) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
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

SECStatus
intel_AES_GCM_EncryptAEAD(intel_AES_GCMContext *gcm,
                          unsigned char *outbuf,
                          unsigned int *outlen, unsigned int maxout,
                          const unsigned char *inbuf, unsigned int inlen,
                          void *params, unsigned int paramLen,
                          const unsigned char *aad, unsigned int aadLen,
                          unsigned int blocksize)
{
    unsigned int tagBytes;
    unsigned char T[AES_BLOCK_SIZE];
    const CK_GCM_MESSAGE_PARAMS *gcmParams =
        (const CK_GCM_MESSAGE_PARAMS *)params;
    SECStatus rv;

    // GCM has a 16 octet block, with a 32-bit block counter
    // Limit in accordance with SP800-38D
    if (sizeof(inlen) > 4 &&
        inlen >= ((1ULL << 32) - 2) * AES_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
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

    rv = intel_aes_gcmInitCounter(gcm, gcmParams->pIv, gcmParams->ulIvLen,
                                  gcmParams->ulTagBits, aad, aadLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;

    intel_aes_gcmENC(inbuf, outbuf, gcm, inlen);

    gcm->Mlen += inlen;

    intel_aes_gcmTAG(gcm->Htbl, gcm->T, gcm->Mlen, gcm->Alen, gcm->X0, T);

    *outlen = inlen;
    PORT_Memcpy(gcmParams->pTag, T, tagBytes);
    return SECSuccess;
}

SECStatus
intel_AES_GCM_DecryptAEAD(intel_AES_GCMContext *gcm,
                          unsigned char *outbuf,
                          unsigned int *outlen, unsigned int maxout,
                          const unsigned char *inbuf, unsigned int inlen,
                          void *params, unsigned int paramLen,
                          const unsigned char *aad, unsigned int aadLen,
                          unsigned int blocksize)
{
    unsigned int tagBytes;
    unsigned char T[AES_BLOCK_SIZE];
    const unsigned char *intag;
    const CK_GCM_MESSAGE_PARAMS *gcmParams =
        (const CK_GCM_MESSAGE_PARAMS *)params;
    SECStatus rv;

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

    rv = intel_aes_gcmInitCounter(gcm, gcmParams->pIv, gcmParams->ulIvLen,
                                  gcmParams->ulTagBits, aad, aadLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;
    intag = gcmParams->pTag;
    PORT_Assert(tagBytes != 0);

    intel_aes_gcmDEC(inbuf, outbuf, gcm, inlen);

    gcm->Mlen += inlen;
    intel_aes_gcmTAG(gcm->Htbl, gcm->T, gcm->Mlen, gcm->Alen, gcm->X0, T);

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
