/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "prtypes.h"
#include "blapit.h"
#include "blapii.h"
#include "ctr.h"
#include "pkcs11t.h"
#include "secerr.h"

#ifdef USE_HW_AES
#include "intel-aes.h"
#include "rijndael.h"
#endif

SECStatus
CTR_InitContext(CTRContext *ctr, void *context, freeblCipherFunc cipher,
                const unsigned char *param)
{
    const CK_AES_CTR_PARAMS *ctrParams = (const CK_AES_CTR_PARAMS *)param;

    if (ctrParams->ulCounterBits == 0 ||
        ctrParams->ulCounterBits > AES_BLOCK_SIZE * PR_BITS_PER_BYTE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Invariant: 0 < ctr->bufPtr <= AES_BLOCK_SIZE */
    ctr->checkWrap = PR_FALSE;
    ctr->bufPtr = AES_BLOCK_SIZE; /* no unused data in the buffer */
    ctr->cipher = cipher;
    ctr->context = context;
    ctr->counterBits = ctrParams->ulCounterBits;
    if (AES_BLOCK_SIZE > sizeof(ctr->counter) ||
        AES_BLOCK_SIZE > sizeof(ctrParams->cb)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    PORT_Memcpy(ctr->counter, ctrParams->cb, AES_BLOCK_SIZE);
    if (ctr->counterBits < 64) {
        PORT_Memcpy(ctr->counterFirst, ctr->counter, AES_BLOCK_SIZE);
        ctr->checkWrap = PR_TRUE;
    }
    return SECSuccess;
}

CTRContext *
CTR_CreateContext(void *context, freeblCipherFunc cipher,
                  const unsigned char *param)
{
    CTRContext *ctr;
    SECStatus rv;

    /* first fill in the Counter context */
    ctr = PORT_ZNew(CTRContext);
    if (ctr == NULL) {
        return NULL;
    }
    rv = CTR_InitContext(ctr, context, cipher, param);
    if (rv != SECSuccess) {
        CTR_DestroyContext(ctr, PR_TRUE);
        ctr = NULL;
    }
    return ctr;
}

void
CTR_DestroyContext(CTRContext *ctr, PRBool freeit)
{
    PORT_Memset(ctr, 0, sizeof(CTRContext));
    if (freeit) {
        PORT_Free(ctr);
    }
}

/*
 * Used by counter mode. Increment the counter block. Not all bits in the
 * counter block are part of the counter, counterBits tells how many bits
 * are part of the counter. The counter block is blocksize long. It's a
 * big endian value.
 *
 * XXX Does not handle counter rollover.
 */
static void
ctr_GetNextCtr(unsigned char *counter, unsigned int counterBits,
               unsigned int blocksize)
{
    unsigned char *counterPtr = counter + blocksize - 1;
    unsigned char mask, count;

    PORT_Assert(counterBits <= blocksize * PR_BITS_PER_BYTE);
    while (counterBits >= PR_BITS_PER_BYTE) {
        if (++(*(counterPtr--))) {
            return;
        }
        counterBits -= PR_BITS_PER_BYTE;
    }
    if (counterBits == 0) {
        return;
    }
    /* increment the final partial byte */
    mask = (1 << counterBits) - 1;
    count = ++(*counterPtr) & mask;
    *counterPtr = ((*counterPtr) & ~mask) | count;
    return;
}

static void
ctr_xor(unsigned char *target, const unsigned char *x,
        const unsigned char *y, unsigned int count)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        *target++ = *x++ ^ *y++;
    }
}

SECStatus
CTR_Update(CTRContext *ctr, unsigned char *outbuf,
           unsigned int *outlen, unsigned int maxout,
           const unsigned char *inbuf, unsigned int inlen,
           unsigned int blocksize)
{
    unsigned int tmp;
    SECStatus rv;

    if (maxout < inlen) {
        *outlen = inlen;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    *outlen = 0;
    if (ctr->bufPtr != blocksize) {
        unsigned int needed = PR_MIN(blocksize - ctr->bufPtr, inlen);
        ctr_xor(outbuf, inbuf, ctr->buffer + ctr->bufPtr, needed);
        ctr->bufPtr += needed;
        outbuf += needed;
        inbuf += needed;
        *outlen += needed;
        inlen -= needed;
        if (inlen == 0) {
            return SECSuccess;
        }
        PORT_Assert(ctr->bufPtr == blocksize);
    }

    while (inlen >= blocksize) {
        rv = (*ctr->cipher)(ctr->context, ctr->buffer, &tmp, blocksize,
                            ctr->counter, blocksize, blocksize);
        ctr_GetNextCtr(ctr->counter, ctr->counterBits, blocksize);
        if (ctr->checkWrap) {
            if (PORT_Memcmp(ctr->counter, ctr->counterFirst, blocksize) == 0) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
        }
        if (rv != SECSuccess) {
            return SECFailure;
        }
        ctr_xor(outbuf, inbuf, ctr->buffer, blocksize);
        outbuf += blocksize;
        inbuf += blocksize;
        *outlen += blocksize;
        inlen -= blocksize;
    }
    if (inlen == 0) {
        return SECSuccess;
    }
    rv = (*ctr->cipher)(ctr->context, ctr->buffer, &tmp, blocksize,
                        ctr->counter, blocksize, blocksize);
    ctr_GetNextCtr(ctr->counter, ctr->counterBits, blocksize);
    if (ctr->checkWrap) {
        if (PORT_Memcmp(ctr->counter, ctr->counterFirst, blocksize) == 0) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
    }
    if (rv != SECSuccess) {
        return SECFailure;
    }
    ctr_xor(outbuf, inbuf, ctr->buffer, inlen);
    ctr->bufPtr = inlen;
    *outlen += inlen;
    return SECSuccess;
}

#if defined(USE_HW_AES) && defined(_MSC_VER)
SECStatus
CTR_Update_HW_AES(CTRContext *ctr, unsigned char *outbuf,
                  unsigned int *outlen, unsigned int maxout,
                  const unsigned char *inbuf, unsigned int inlen,
                  unsigned int blocksize)
{
    unsigned int fullblocks;
    unsigned int tmp;
    SECStatus rv;

    if (maxout < inlen) {
        *outlen = inlen;
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    *outlen = 0;
    if (ctr->bufPtr != blocksize) {
        unsigned int needed = PR_MIN(blocksize - ctr->bufPtr, inlen);
        ctr_xor(outbuf, inbuf, ctr->buffer + ctr->bufPtr, needed);
        ctr->bufPtr += needed;
        outbuf += needed;
        inbuf += needed;
        *outlen += needed;
        inlen -= needed;
        if (inlen == 0) {
            return SECSuccess;
        }
        PORT_Assert(ctr->bufPtr == blocksize);
    }

    intel_aes_ctr_worker(((AESContext *)(ctr->context))->Nr)(
        ctr, outbuf, outlen, maxout, inbuf, inlen, blocksize);
    /* XXX intel_aes_ctr_worker should set *outlen. */
    PORT_Assert(*outlen == 0);
    fullblocks = (inlen / blocksize) * blocksize;
    *outlen += fullblocks;
    outbuf += fullblocks;
    inbuf += fullblocks;
    inlen -= fullblocks;

    if (inlen == 0) {
        return SECSuccess;
    }
    rv = (*ctr->cipher)(ctr->context, ctr->buffer, &tmp, blocksize,
                        ctr->counter, blocksize, blocksize);
    ctr_GetNextCtr(ctr->counter, ctr->counterBits, blocksize);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    ctr_xor(outbuf, inbuf, ctr->buffer, inlen);
    ctr->bufPtr = inlen;
    *outlen += inlen;
    return SECSuccess;
}
#endif
