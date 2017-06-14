/*
 * aeskeywrap.c - implement AES Key Wrap algorithm from RFC 3394
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prcpucfg.h"
#if defined(IS_LITTLE_ENDIAN) || defined(SHA_NO_LONG_LONG)
#define BIG_ENDIAN_WITH_64_BIT_REGISTERS 0
#else
#define BIG_ENDIAN_WITH_64_BIT_REGISTERS 1
#endif
#include "prtypes.h" /* for PRUintXX */
#include "secport.h" /* for PORT_XXX */
#include "secerr.h"
#include "blapi.h" /* for AES_ functions */
#include "rijndael.h"

struct AESKeyWrapContextStr {
    AESContext aescx;
    unsigned char iv[AES_KEY_WRAP_IV_BYTES];
    void *mem; /* Pointer to beginning of allocated memory. */
};

/******************************************/
/*
** AES key wrap algorithm, RFC 3394
*/

AESKeyWrapContext *
AESKeyWrap_AllocateContext(void)
{
    /* aligned_alloc is C11 so we have to do it the old way. */
    AESKeyWrapContext *ctx = PORT_ZAlloc(sizeof(AESKeyWrapContext) + 15);
    if (ctx == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }
    ctx->mem = ctx;
    return (AESKeyWrapContext *)(((uintptr_t)ctx + 15) & ~(uintptr_t)0x0F);
}

SECStatus
AESKeyWrap_InitContext(AESKeyWrapContext *cx,
                       const unsigned char *key,
                       unsigned int keylen,
                       const unsigned char *iv,
                       int x1,
                       unsigned int encrypt,
                       unsigned int x2)
{
    SECStatus rv = SECFailure;
    if (!cx) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (iv) {
        memcpy(cx->iv, iv, sizeof cx->iv);
    } else {
        memset(cx->iv, 0xA6, sizeof cx->iv);
    }
    rv = AES_InitContext(&cx->aescx, key, keylen, NULL, NSS_AES, encrypt,
                         AES_BLOCK_SIZE);
    return rv;
}

/*
** Create a new AES context suitable for AES encryption/decryption.
**  "key" raw key data
**  "keylen" the number of bytes of key data (16, 24, or 32)
*/
extern AESKeyWrapContext *
AESKeyWrap_CreateContext(const unsigned char *key, const unsigned char *iv,
                         int encrypt, unsigned int keylen)
{
    SECStatus rv;
    AESKeyWrapContext *cx = AESKeyWrap_AllocateContext();
    if (!cx)
        return NULL; /* error is already set */
    rv = AESKeyWrap_InitContext(cx, key, keylen, iv, 0, encrypt, 0);
    if (rv != SECSuccess) {
        PORT_Free(cx->mem);
        cx = NULL; /* error should already be set */
    }
    return cx;
}

/*
** Destroy a AES KeyWrap context.
**  "cx" the context
**  "freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void
AESKeyWrap_DestroyContext(AESKeyWrapContext *cx, PRBool freeit)
{
    if (cx) {
        AES_DestroyContext(&cx->aescx, PR_FALSE);
        /*  memset(cx, 0, sizeof *cx); */
        if (freeit) {
            PORT_Free(cx->mem);
        }
    }
}

#if !BIG_ENDIAN_WITH_64_BIT_REGISTERS

/* The AES Key Wrap algorithm has 64-bit values that are ALWAYS big-endian
** (Most significant byte first) in memory.  The only ALU operations done
** on them are increment, decrement, and XOR.  So, on little-endian CPUs,
** and on CPUs that lack 64-bit registers, these big-endian 64-bit operations
** are simulated in the following code.  This is thought to be faster and
** simpler than trying to convert the data to little-endian and back.
*/

/* A and T point to two 64-bit values stored most signficant byte first
** (big endian).  This function increments the 64-bit value T, and then
** XORs it with A, changing A.
*/
static void
increment_and_xor(unsigned char *A, unsigned char *T)
{
    if (!++T[7])
        if (!++T[6])
            if (!++T[5])
                if (!++T[4])
                    if (!++T[3])
                        if (!++T[2])
                            if (!++T[1])
                                ++T[0];

    A[0] ^= T[0];
    A[1] ^= T[1];
    A[2] ^= T[2];
    A[3] ^= T[3];
    A[4] ^= T[4];
    A[5] ^= T[5];
    A[6] ^= T[6];
    A[7] ^= T[7];
}

/* A and T point to two 64-bit values stored most signficant byte first
** (big endian).  This function XORs T with A, giving a new A, then
** decrements the 64-bit value T.
*/
static void
xor_and_decrement(PRUint64 *A, PRUint64 *T)
{
    unsigned char *TP = (unsigned char *)T;
    const PRUint64 mask = 0xFF;
    *A = ((*A & mask << 56) ^ (*T & mask << 56)) |
         ((*A & mask << 48) ^ (*T & mask << 48)) |
         ((*A & mask << 40) ^ (*T & mask << 40)) |
         ((*A & mask << 32) ^ (*T & mask << 32)) |
         ((*A & mask << 24) ^ (*T & mask << 23)) |
         ((*A & mask << 16) ^ (*T & mask << 16)) |
         ((*A & mask << 8) ^ (*T & mask << 8)) |
         ((*A & mask) ^ (*T & mask));

    if (!TP[7]--)
        if (!TP[6]--)
            if (!TP[5]--)
                if (!TP[4]--)
                    if (!TP[3]--)
                        if (!TP[2]--)
                            if (!TP[1]--)
                                TP[0]--;
}

/* Given an unsigned long t (in host byte order), store this value as a
** 64-bit big-endian value (MSB first) in *pt.
*/
static void
set_t(unsigned char *pt, unsigned long t)
{
    pt[7] = (unsigned char)t;
    t >>= 8;
    pt[6] = (unsigned char)t;
    t >>= 8;
    pt[5] = (unsigned char)t;
    t >>= 8;
    pt[4] = (unsigned char)t;
    t >>= 8;
    pt[3] = (unsigned char)t;
    t >>= 8;
    pt[2] = (unsigned char)t;
    t >>= 8;
    pt[1] = (unsigned char)t;
    t >>= 8;
    pt[0] = (unsigned char)t;
}

#endif

/*
** Perform AES key wrap.
**  "cx" the context
**  "output" the output buffer to store the encrypted data.
**  "outputLen" how much data is stored in "output". Set by the routine
**     after some data is stored in output.
**  "maxOutputLen" the maximum amount of data that can ever be
**     stored in "output"
**  "input" the input data
**  "inputLen" the amount of input data
*/
extern SECStatus
AESKeyWrap_Encrypt(AESKeyWrapContext *cx, unsigned char *output,
                   unsigned int *pOutputLen, unsigned int maxOutputLen,
                   const unsigned char *input, unsigned int inputLen)
{
    PRUint64 *R = NULL;
    unsigned int nBlocks;
    unsigned int i, j;
    unsigned int aesLen = AES_BLOCK_SIZE;
    unsigned int outLen = inputLen + AES_KEY_WRAP_BLOCK_SIZE;
    SECStatus s = SECFailure;
    /* These PRUint64s are ALWAYS big endian, regardless of CPU orientation. */
    PRUint64 t;
    PRUint64 B[2];

#define A B[0]

    /* Check args */
    if (!inputLen || 0 != inputLen % AES_KEY_WRAP_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return s;
    }
#ifdef maybe
    if (!output && pOutputLen) { /* caller is asking for output size */
        *pOutputLen = outLen;
        return SECSuccess;
    }
#endif
    if (maxOutputLen < outLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return s;
    }
    if (cx == NULL || output == NULL || input == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return s;
    }
    nBlocks = inputLen / AES_KEY_WRAP_BLOCK_SIZE;
    R = PORT_NewArray(PRUint64, nBlocks + 1);
    if (!R)
        return s; /* error is already set. */
    /*
    ** 1) Initialize variables.
    */
    memcpy(&A, cx->iv, AES_KEY_WRAP_IV_BYTES);
    memcpy(&R[1], input, inputLen);
#if BIG_ENDIAN_WITH_64_BIT_REGISTERS
    t = 0;
#else
    memset(&t, 0, sizeof t);
#endif
    /*
    ** 2) Calculate intermediate values.
    */
    for (j = 0; j < 6; ++j) {
        for (i = 1; i <= nBlocks; ++i) {
            B[1] = R[i];
            s = AES_Encrypt(&cx->aescx, (unsigned char *)B, &aesLen,
                            sizeof B, (unsigned char *)B, sizeof B);
            if (s != SECSuccess)
                break;
            R[i] = B[1];
/* here, increment t and XOR A with t (in big endian order); */
#if BIG_ENDIAN_WITH_64_BIT_REGISTERS
            A ^= ++t;
#else
            increment_and_xor((unsigned char *)&A, (unsigned char *)&t);
#endif
        }
    }
    /*
    ** 3) Output the results.
    */
    if (s == SECSuccess) {
        R[0] = A;
        memcpy(output, &R[0], outLen);
        if (pOutputLen)
            *pOutputLen = outLen;
    } else if (pOutputLen) {
        *pOutputLen = 0;
    }
    PORT_ZFree(R, outLen);
    return s;
}
#undef A

/*
** Perform AES key unwrap.
**  "cx" the context
**  "output" the output buffer to store the decrypted data.
**  "outputLen" how much data is stored in "output". Set by the routine
**     after some data is stored in output.
**  "maxOutputLen" the maximum amount of data that can ever be
**     stored in "output"
**  "input" the input data
**  "inputLen" the amount of input data
*/
extern SECStatus
AESKeyWrap_Decrypt(AESKeyWrapContext *cx, unsigned char *output,
                   unsigned int *pOutputLen, unsigned int maxOutputLen,
                   const unsigned char *input, unsigned int inputLen)
{
    PRUint64 *R = NULL;
    unsigned int nBlocks;
    unsigned int i, j;
    unsigned int aesLen = AES_BLOCK_SIZE;
    unsigned int outLen;
    SECStatus s = SECFailure;
    /* These PRUint64s are ALWAYS big endian, regardless of CPU orientation. */
    PRUint64 t;
    PRUint64 B[2];

    /* Check args */
    if (inputLen < 3 * AES_KEY_WRAP_BLOCK_SIZE ||
        0 != inputLen % AES_KEY_WRAP_BLOCK_SIZE) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return s;
    }
    outLen = inputLen - AES_KEY_WRAP_BLOCK_SIZE;
#ifdef maybe
    if (!output && pOutputLen) { /* caller is asking for output size */
        *pOutputLen = outLen;
        return SECSuccess;
    }
#endif
    if (maxOutputLen < outLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return s;
    }
    if (cx == NULL || output == NULL || input == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return s;
    }
    nBlocks = inputLen / AES_KEY_WRAP_BLOCK_SIZE;
    R = PORT_NewArray(PRUint64, nBlocks);
    if (!R)
        return s; /* error is already set. */
    nBlocks--;
    /*
    ** 1) Initialize variables.
    */
    memcpy(&R[0], input, inputLen);
    B[0] = R[0];
#if BIG_ENDIAN_WITH_64_BIT_REGISTERS
    t = 6UL * nBlocks;
#else
    set_t((unsigned char *)&t, 6UL * nBlocks);
#endif
    /*
    ** 2) Calculate intermediate values.
    */
    for (j = 0; j < 6; ++j) {
        for (i = nBlocks; i; --i) {
/* here, XOR A with t (in big endian order) and decrement t; */
#if BIG_ENDIAN_WITH_64_BIT_REGISTERS
            B[0] ^= t--;
#else
            xor_and_decrement(&B[0], &t);
#endif
            B[1] = R[i];
            s = AES_Decrypt(&cx->aescx, (unsigned char *)B, &aesLen,
                            sizeof B, (unsigned char *)B, sizeof B);
            if (s != SECSuccess)
                break;
            R[i] = B[1];
        }
    }
    /*
    ** 3) Output the results.
    */
    if (s == SECSuccess) {
        int bad = memcmp(&B[0], cx->iv, AES_KEY_WRAP_IV_BYTES);
        if (!bad) {
            memcpy(output, &R[1], outLen);
            if (pOutputLen)
                *pOutputLen = outLen;
        } else {
            s = SECFailure;
            PORT_SetError(SEC_ERROR_BAD_DATA);
            if (pOutputLen)
                *pOutputLen = 0;
        }
    } else if (pOutputLen) {
        *pOutputLen = 0;
    }
    PORT_ZFree(R, inputLen);
    return s;
}
#undef A
