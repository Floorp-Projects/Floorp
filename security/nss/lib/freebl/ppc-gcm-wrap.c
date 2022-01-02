/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* Copyright(c) 2013, Intel Corp. */

/* Wrapper functions for PowerPC optimized implementation of AES-GCM */

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
#include <stdio.h>

#include "ppc-gcm.h"
#include "rijndael.h"

struct ppc_AES_GCMContextStr {
    unsigned char Htbl[8 * AES_BLOCK_SIZE];
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

SECStatus ppc_aes_gcmInitCounter(ppc_AES_GCMContext *gcm,
                                 const unsigned char *iv,
                                 unsigned long ivLen, unsigned long tagBits,
                                 const unsigned char *aad, unsigned long aadLen);

ppc_AES_GCMContext *
ppc_AES_GCM_CreateContext(void *context,
                          freeblCipherFunc cipher,
                          const unsigned char *params)
{
    ppc_AES_GCMContext *gcm = NULL;
    AESContext *aes = (AESContext *)context;
    const CK_NSS_GCM_PARAMS *gcmParams = (const CK_NSS_GCM_PARAMS *)params;
    SECStatus rv;

    gcm = PORT_ZNew(ppc_AES_GCMContext);
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
    ppc_aes_gcmINIT(gcm->Htbl, aes->k.expandedKey, aes->Nr);

    gcm_InitIVContext(&gcm->gcm_iv);

    /* if gcmParams is NULL, then we are creating an PKCS #11 MESSAGE
     * style context, in which we initialize the key once, then do separate
     * iv/aad's for each message. If we are doing that kind of operation,
     * we've finished with init here. We'll init the Counter in each AEAD
     * call */
    if (gcmParams == NULL) {
        return gcm;
    }

    rv = ppc_aes_gcmInitCounter(gcm, gcmParams->pIv,
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
ppc_aes_gcmInitCounter(ppc_AES_GCMContext *gcm,
                       const unsigned char *iv, unsigned long ivLen,
                       unsigned long tagBits,
                       const unsigned char *aad, unsigned long aadLen)
{
    unsigned int j;
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

    /* Initial TAG value is zero */
    PORT_Memset(gcm->T, 0, AES_BLOCK_SIZE);
    PORT_Memset(gcm->X0, 0, AES_BLOCK_SIZE);

    /* Init the counter */
    if (ivLen == 12) {
        PORT_Memcpy(gcm->CTR, iv, AES_BLOCK_SIZE - 4);
        gcm->CTR[12] = 0;
        gcm->CTR[13] = 0;
        gcm->CTR[14] = 0;
        gcm->CTR[15] = 1;
    } else {
        /* If IV size is not 96 bits, then the initial counter value is GHASH
         * of the IV */
        ppc_aes_gcmHASH(gcm->Htbl, iv, ivLen, gcm->T);

        ppc_aes_gcmTAG(
            gcm->Htbl,
            gcm->T,
            ivLen,
            0,
            gcm->X0,
            gcm->CTR);

        /* TAG should be zero again */
        PORT_Memset(gcm->T, 0, AES_BLOCK_SIZE);
    }

    /* Encrypt the initial counter, will be used to encrypt the GHASH value,
     * in the end */
    rv = (*gcm->cipher)(gcm->aes_context, gcm->X0, &j, AES_BLOCK_SIZE, gcm->CTR,
                        AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Promote the counter by 1 */
    gcm->CTR[14] += !(++gcm->CTR[15]);
    gcm->CTR[13] += !(gcm->CTR[15]) && !(gcm->CTR[14]);
    gcm->CTR[12] += !(gcm->CTR[15]) && !(gcm->CTR[14]) && !(gcm->CTR[13]);

    /* Now hash AAD - it would actually make sense to seperate the context
     * creation from the AAD, because that would allow to reuse the H, which
     * only changes when the AES key changes, and not every package, like the
     * IV and AAD */
    ppc_aes_gcmHASH(gcm->Htbl, aad, aadLen, gcm->T);
    gcm->Alen += aadLen;
    return SECSuccess;
}

void
ppc_AES_GCM_DestroyContext(ppc_AES_GCMContext *gcm, PRBool freeit)
{
    PORT_Memset(gcm, 0, sizeof(ppc_AES_GCMContext));
    if (freeit) {
        PORT_Free(gcm);
    }
}

SECStatus
ppc_AES_GCM_EncryptUpdate(ppc_AES_GCMContext *gcm,
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

    ppc_aes_gcmCRYPT(
        inbuf,
        outbuf,
        inlen,
        gcm->CTR,
        gcm->aes_context->k.expandedKey,
        gcm->aes_context->Nr);
    ppc_aes_gcmHASH(
        gcm->Htbl,
        outbuf,
        inlen,
        gcm->T);

    gcm->Mlen += inlen;

    ppc_aes_gcmTAG(
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
ppc_AES_GCM_DecryptUpdate(ppc_AES_GCMContext *gcm,
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

    ppc_aes_gcmHASH(
        gcm->Htbl,
        inbuf,
        inlen,
        gcm->T);
    ppc_aes_gcmCRYPT(
        inbuf,
        outbuf,
        inlen,
        gcm->CTR,
        gcm->aes_context->k.expandedKey,
        gcm->aes_context->Nr);

    gcm->Mlen += inlen;
    ppc_aes_gcmTAG(
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
ppc_AES_GCM_EncryptAEAD(ppc_AES_GCMContext *gcm,
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

    rv = ppc_aes_gcmInitCounter(gcm, gcmParams->pIv, gcmParams->ulIvLen,
                                gcmParams->ulTagBits, aad, aadLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;

    ppc_aes_gcmCRYPT(inbuf, outbuf, inlen, gcm->CTR, gcm->aes_context->k.expandedKey,
                     gcm->aes_context->Nr);
    ppc_aes_gcmHASH(gcm->Htbl, outbuf, inlen, gcm->T);

    gcm->Mlen += inlen;

    ppc_aes_gcmTAG(gcm->Htbl, gcm->T, gcm->Mlen, gcm->Alen, gcm->X0, T);

    *outlen = inlen;
    PORT_Memcpy(gcmParams->pTag, T, tagBytes);
    return SECSuccess;
}

SECStatus
ppc_AES_GCM_DecryptAEAD(ppc_AES_GCMContext *gcm,
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

    rv = ppc_aes_gcmInitCounter(gcm, gcmParams->pIv, gcmParams->ulIvLen,
                                gcmParams->ulTagBits, aad, aadLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE - 1)) / PR_BITS_PER_BYTE;
    intag = gcmParams->pTag;
    PORT_Assert(tagBytes != 0);

    ppc_aes_gcmHASH(gcm->Htbl, inbuf, inlen, gcm->T);
    ppc_aes_gcmCRYPT(inbuf, outbuf, inlen, gcm->CTR, gcm->aes_context->k.expandedKey,
                     gcm->aes_context->Nr);

    gcm->Mlen += inlen;
    ppc_aes_gcmTAG(gcm->Htbl, gcm->T, gcm->Mlen, gcm->Alen, gcm->X0, T);

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
