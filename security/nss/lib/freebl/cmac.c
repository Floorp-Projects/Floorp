/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "rijndael.h"
#include "blapi.h"
#include "cmac.h"
#include "secerr.h"
#include "nspr.h"

struct CMACContextStr {
    /* Information about the block cipher to use internally. The cipher should
     * be placed in ECB mode so that we can use it to directly encrypt blocks.
     *
     *
     * To add a new cipher, add an entry to CMACCipher, update CMAC_Init,
     * cmac_Encrypt, and CMAC_Destroy methods to handle the new cipher, and
     * add a new Context pointer to the cipher union with the correct type. */
    CMACCipher cipherType;
    union {
        AESContext *aes;
    } cipher;
    unsigned int blockSize;

    /* Internal keys which are conditionally used by the algorithm. Derived
     * from encrypting the NULL block. We leave the storing of (and the
     * cleanup of) the CMAC key to the underlying block cipher. */
    unsigned char k1[MAX_BLOCK_SIZE];
    unsigned char k2[MAX_BLOCK_SIZE];

    /* When Update is called with data which isn't a multiple of the block
     * size, we need a place to put it. HMAC handles this by passing it to
     * the underlying hash function right away; we can't do that as the
     * contract on the cipher object is different. */
    unsigned int partialIndex;
    unsigned char partialBlock[MAX_BLOCK_SIZE];

    /* Last encrypted block. This gets xor-ed with partialBlock prior to
     * encrypting it. NIST defines this to be the empty string to begin. */
    unsigned char lastBlock[MAX_BLOCK_SIZE];
};

static void
cmac_ShiftLeftOne(unsigned char *out, const unsigned char *in, int length)
{
    int i = 0;
    for (; i < length - 1; i++) {
        out[i] = in[i] << 1;
        out[i] |= in[i + 1] >> 7;
    }
    out[i] = in[i] << 1;
}

static SECStatus
cmac_Encrypt(CMACContext *ctx, unsigned char *output,
             const unsigned char *input,
             unsigned int inputLen)
{
    if (ctx->cipherType == CMAC_AES) {
        unsigned int tmpOutputLen;
        SECStatus rv = AES_Encrypt(ctx->cipher.aes, output, &tmpOutputLen,
                                   ctx->blockSize, input, inputLen);

        /* Assumption: AES_Encrypt (when in ECB mode) always returns an
         * output of length equal to blockSize (what was pass as the value
         * of the maxOutputLen parameter). */
        PORT_Assert(tmpOutputLen == ctx->blockSize);
        return rv;
    }

    return SECFailure;
}

/* NIST SP.800-38B, 6.1 Subkey Generation */
static SECStatus
cmac_GenerateSubkeys(CMACContext *ctx)
{
    unsigned char null_block[MAX_BLOCK_SIZE] = { 0 };
    unsigned char L[MAX_BLOCK_SIZE];
    unsigned char v;
    unsigned char i;

    /* Step 1: L = AES(key, null_block) */
    if (cmac_Encrypt(ctx, L, null_block, ctx->blockSize) != SECSuccess) {
        return SECFailure;
    }

    /* In the following, some effort has been made to be constant time. Rather
     * than conditioning on the value of the MSB (of L or K1), we use the loop
     * to build a mask for the conditional constant. */

    /* Step 2: If MSB(L) = 0, K1 = L << 1. Else, K1 = (L << 1) ^ R_b. */
    cmac_ShiftLeftOne(ctx->k1, L, ctx->blockSize);
    v = L[0] >> 7;
    for (i = 1; i <= 7; i <<= 1) {
        v |= (v << i);
    }
    ctx->k1[ctx->blockSize - 1] ^= (0x87 & v);

    /* Step 3: If MSB(K1) = 0, K2 = K1 << 1. Else, K2 = (K1 <, 1) ^ R_b. */
    cmac_ShiftLeftOne(ctx->k2, ctx->k1, ctx->blockSize);
    v = ctx->k1[0] >> 7;
    for (i = 1; i <= 7; i <<= 1) {
        v |= (v << i);
    }
    ctx->k2[ctx->blockSize - 1] ^= (0x87 & v);

    /* Any intermediate value in the computation of the subkey shall be
     * secret. */
    PORT_Memset(null_block, 0, MAX_BLOCK_SIZE);
    PORT_Memset(L, 0, MAX_BLOCK_SIZE);

    /* Step 4: Return the values. */
    return SECSuccess;
}

/* NIST SP.800-38B, 6.2 MAC Generation step 6 */
static SECStatus
cmac_UpdateState(CMACContext *ctx)
{
    if (ctx == NULL || ctx->partialIndex != ctx->blockSize) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Step 6: C_i = CIPHER(key, C_{i-1} ^ M_i)  for 1 <= i <= n, and
     *         C_0 is defined as the empty string. */

    for (unsigned int index = 0; index < ctx->blockSize; index++) {
        ctx->partialBlock[index] ^= ctx->lastBlock[index];
    }

    return cmac_Encrypt(ctx, ctx->lastBlock, ctx->partialBlock, ctx->blockSize);
}

SECStatus
CMAC_Init(CMACContext *ctx, CMACCipher type,
          const unsigned char *key, unsigned int key_len)
{
    if (ctx == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    /* We only currently support AES-CMAC. */
    if (type != CMAC_AES) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    PORT_Memset(ctx, 0, sizeof(*ctx));

    ctx->blockSize = AES_BLOCK_SIZE;
    ctx->cipherType = CMAC_AES;
    ctx->cipher.aes = AES_CreateContext(key, NULL, NSS_AES, 1, key_len,
                                        ctx->blockSize);
    if (ctx->cipher.aes == NULL) {
        return SECFailure;
    }

    return CMAC_Begin(ctx);
}

CMACContext *
CMAC_Create(CMACCipher type, const unsigned char *key,
            unsigned int key_len)
{
    CMACContext *result = PORT_New(CMACContext);

    if (CMAC_Init(result, type, key, key_len) != SECSuccess) {
        CMAC_Destroy(result, PR_TRUE);
        return NULL;
    }

    return result;
}

SECStatus
CMAC_Begin(CMACContext *ctx)
{
    if (ctx == NULL) {
        return SECFailure;
    }

    /* Ensure that our blockSize is less than the maximum. When this fails,
     * a cipher with a larger block size was added and MAX_BLOCK_SIZE needs
     * to be updated accordingly. */
    PORT_Assert(ctx->blockSize <= MAX_BLOCK_SIZE);

    if (cmac_GenerateSubkeys(ctx) != SECSuccess) {
        return SECFailure;
    }

    /* Set the index to write partial blocks at to zero. This saves us from
     * having to clear ctx->partialBlock. */
    ctx->partialIndex = 0;

    /* Step 5: Let C_0 = 0^b. */
    PORT_Memset(ctx->lastBlock, 0, ctx->blockSize);

    return SECSuccess;
}

/* NIST SP.800-38B, 6.2 MAC Generation */
SECStatus
CMAC_Update(CMACContext *ctx, const unsigned char *data,
            unsigned int data_len)
{
    unsigned int data_index = 0;
    if (ctx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (data == NULL || data_len == 0) {
        return SECSuccess;
    }

    /* Copy as many bytes from data into ctx->partialBlock as we can, up to
     * the maximum of the remaining data and the remaining space in
     * ctx->partialBlock.
     *
     * Note that we swap the order (encrypt *then* copy) because the last
     * block is different from the rest. If we end on an even multiple of
     * the block size, we have to be able to XOR it with K1. But we won't know
     * that it is the last until CMAC_Finish is called (and by then, CMAC_Update
     * has already returned). */
    while (data_index < data_len) {
        if (ctx->partialIndex == ctx->blockSize) {
            if (cmac_UpdateState(ctx) != SECSuccess) {
                return SECFailure;
            }

            ctx->partialIndex = 0;
        }

        unsigned int copy_len = data_len - data_index;
        if (copy_len > (ctx->blockSize - ctx->partialIndex)) {
            copy_len = ctx->blockSize - ctx->partialIndex;
        }

        PORT_Memcpy(ctx->partialBlock + ctx->partialIndex, data + data_index, copy_len);
        data_index += copy_len;
        ctx->partialIndex += copy_len;
    }

    return SECSuccess;
}

/* NIST SP.800-38B, 6.2 MAC Generation */
SECStatus
CMAC_Finish(CMACContext *ctx, unsigned char *result,
            unsigned int *result_len,
            unsigned int max_result_len)
{
    if (ctx == NULL || result == NULL || max_result_len == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (max_result_len > ctx->blockSize) {
        /* This is a weird situation. The PKCS #11 soft tokencode passes
         * sizeof(result) here, which is hard-coded as SFTK_MAX_MAC_LENGTH.
         * This later gets truncated to min(SFTK_MAX_MAC_LENGTH, requested). */
        max_result_len = ctx->blockSize;
    }

    /* Step 4: If M_n* is a complete block, M_n = K1 ^ M_n*. Else,
     * M_n = K2 ^ (M_n* || 10^j). */
    if (ctx->partialIndex == ctx->blockSize) {
        /* XOR in K1. */
        for (unsigned int index = 0; index < ctx->blockSize; index++) {
            ctx->partialBlock[index] ^= ctx->k1[index];
        }
    } else {
        /* Use 10* padding on the partial block. */
        ctx->partialBlock[ctx->partialIndex++] = 0x80;
        PORT_Memset(ctx->partialBlock + ctx->partialIndex, 0,
                    ctx->blockSize - ctx->partialIndex);
        ctx->partialIndex = ctx->blockSize;

        /* XOR in K2. */
        for (unsigned int index = 0; index < ctx->blockSize; index++) {
            ctx->partialBlock[index] ^= ctx->k2[index];
        }
    }

    /* Encrypt the block. */
    if (cmac_UpdateState(ctx) != SECSuccess) {
        return SECFailure;
    }

    /* Step 7 & 8: T = MSB_tlen(C_n); return T. */
    PORT_Memcpy(result, ctx->lastBlock, max_result_len);
    if (result_len != NULL) {
        *result_len = max_result_len;
    }
    return SECSuccess;
}

void
CMAC_Destroy(CMACContext *ctx, PRBool free_it)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->cipherType == CMAC_AES && ctx->cipher.aes != NULL) {
        AES_DestroyContext(ctx->cipher.aes, PR_TRUE);
    }

    /* Destroy everything in the context. This includes sensitive data in
     * K1, K2, and lastBlock. */
    PORT_Memset(ctx, 0, sizeof(*ctx));

    if (free_it == PR_TRUE) {
        PORT_Free(ctx);
    }
}
