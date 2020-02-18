/*
 * blake2b.c - definitions for the blake2b hash function
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "secerr.h"
#include "blapi.h"
#include "blake2b.h"
#include "crypto_primitives.h"

/**
 * This contains the BLAKE2b initialization vectors.
 */
static const uint64_t iv[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL,
    0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

/**
 * This contains the table of permutations for blake2b compression function.
 */
static const uint8_t sigma[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 }
};

/**
 * This function increments the blake2b ctx counter.
 */
void
blake2b_IncrementCounter(BLAKE2BContext* ctx, const uint64_t inc)
{
    ctx->t[0] += inc;
    ctx->t[1] += ctx->t[0] < inc;
}

/**
 * This macro implements the blake2b mixing function which mixes two 8-byte
 * words from the message into the hash.
 */
#define G(a, b, c, d, x, y) \
    a += b + x;             \
    d = ROTR64(d ^ a, 32);  \
    c += d;                 \
    b = ROTR64(b ^ c, 24);  \
    a += b + y;             \
    d = ROTR64(d ^ a, 16);  \
    c += d;                 \
    b = ROTR64(b ^ c, 63)

#define ROUND(i)                                                   \
    G(v[0], v[4], v[8], v[12], m[sigma[i][0]], m[sigma[i][1]]);    \
    G(v[1], v[5], v[9], v[13], m[sigma[i][2]], m[sigma[i][3]]);    \
    G(v[2], v[6], v[10], v[14], m[sigma[i][4]], m[sigma[i][5]]);   \
    G(v[3], v[7], v[11], v[15], m[sigma[i][6]], m[sigma[i][7]]);   \
    G(v[0], v[5], v[10], v[15], m[sigma[i][8]], m[sigma[i][9]]);   \
    G(v[1], v[6], v[11], v[12], m[sigma[i][10]], m[sigma[i][11]]); \
    G(v[2], v[7], v[8], v[13], m[sigma[i][12]], m[sigma[i][13]]);  \
    G(v[3], v[4], v[9], v[14], m[sigma[i][14]], m[sigma[i][15]])

/**
 * The blake2b compression function which takes a full 128-byte chunk of the
 * input message and mixes it into the ongoing ctx array, i.e., permute the
 * ctx while xoring in the block of data.
 */
void
blake2b_Compress(BLAKE2BContext* ctx, const uint8_t* block)
{
    size_t i;
    uint64_t v[16], m[16];

    PORT_Memcpy(m, block, BLAKE2B_BLOCK_LENGTH);
#if !defined(IS_LITTLE_ENDIAN)
    for (i = 0; i < 16; ++i) {
        m[i] = FREEBL_HTONLL(m[i]);
    }
#endif

    PORT_Memcpy(v, ctx->h, 8 * 8);
    PORT_Memcpy(v + 8, iv, 8 * 8);

    v[12] ^= ctx->t[0];
    v[13] ^= ctx->t[1];
    v[14] ^= ctx->f;

    ROUND(0);
    ROUND(1);
    ROUND(2);
    ROUND(3);
    ROUND(4);
    ROUND(5);
    ROUND(6);
    ROUND(7);
    ROUND(8);
    ROUND(9);
    ROUND(10);
    ROUND(11);

    for (i = 0; i < 8; i++) {
        ctx->h[i] ^= v[i] ^ v[i + 8];
    }
}

/**
 * This function can be used for both keyed and unkeyed version.
 */
BLAKE2BContext*
BLAKE2B_NewContext()
{
    return PORT_ZNew(BLAKE2BContext);
}

/**
 * Zero and free the context and can be used for both keyed and unkeyed version.
 */
void
BLAKE2B_DestroyContext(BLAKE2BContext* ctx, PRBool freeit)
{
    PORT_Memset(ctx, 0, sizeof(*ctx));
    if (freeit) {
        PORT_Free(ctx);
    }
}

/**
 * This function initializes blake2b ctx and can be used for both keyed and
 * unkeyed version. It also checks ctx and sets error states.
 */
static SECStatus
blake2b_Begin(BLAKE2BContext* ctx, uint8_t outlen, const uint8_t* key,
              size_t keylen)
{
    if (!ctx) {
        goto failure_noclean;
    }
    if (outlen == 0 || outlen > BLAKE2B512_LENGTH) {
        goto failure;
    }
    if (key && keylen > BLAKE2B_KEY_SIZE) {
        goto failure;
    }
    /* Note: key can be null if it's unkeyed. */
    if ((key == NULL && keylen > 0) || keylen > BLAKE2B_KEY_SIZE ||
        (key != NULL && keylen == 0)) {
        goto failure;
    }

    /* Mix key size(keylen) and desired hash length(outlen) into h0 */
    uint64_t param = outlen ^ (keylen << 8) ^ (1 << 16) ^ (1 << 24);
    PORT_Memcpy(ctx->h, iv, 8 * 8);
    ctx->h[0] ^= param;
    ctx->outlen = outlen;

    /* This updates the context for only the keyed version */
    if (keylen > 0 && keylen <= BLAKE2B_KEY_SIZE && key) {
        uint8_t block[BLAKE2B_BLOCK_LENGTH] = { 0 };
        PORT_Memcpy(block, key, keylen);
        BLAKE2B_Update(ctx, block, BLAKE2B_BLOCK_LENGTH);
        PORT_Memset(block, 0, BLAKE2B_BLOCK_LENGTH);
    }

    return SECSuccess;

failure:
    PORT_Memset(ctx, 0, sizeof(*ctx));
failure_noclean:
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
}

SECStatus
BLAKE2B_Begin(BLAKE2BContext* ctx)
{
    return blake2b_Begin(ctx, BLAKE2B512_LENGTH, NULL, 0);
}

SECStatus
BLAKE2B_MAC_Begin(BLAKE2BContext* ctx, const PRUint8* key, const size_t keylen)
{
    PORT_Assert(key != NULL);
    if (!key) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    return blake2b_Begin(ctx, BLAKE2B512_LENGTH, (const uint8_t*)key, keylen);
}

static void
blake2b_IncrementCompress(BLAKE2BContext* ctx, size_t blockLength,
                          const unsigned char* input)
{
    blake2b_IncrementCounter(ctx, blockLength);
    blake2b_Compress(ctx, input);
}

/**
 * This function updates blake2b ctx and can be used for both keyed and unkeyed
 * version.
 */
SECStatus
BLAKE2B_Update(BLAKE2BContext* ctx, const unsigned char* in,
               unsigned int inlen)
{
    /* Nothing to do if there's nothing. */
    if (inlen == 0) {
        return SECSuccess;
    }

    if (!ctx || !in) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Is this a reused context? */
    if (ctx->f) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    size_t left = ctx->buflen;
    PORT_Assert(left <= BLAKE2B_BLOCK_LENGTH);
    size_t fill = BLAKE2B_BLOCK_LENGTH - left;

    if (inlen > fill) {
        if (ctx->buflen) {
            /* There's some remaining data in ctx->buf that we have to prepend
             * to in. */
            PORT_Memcpy(ctx->buf + left, in, fill);
            ctx->buflen = 0;
            blake2b_IncrementCompress(ctx, BLAKE2B_BLOCK_LENGTH, ctx->buf);
            in += fill;
            inlen -= fill;
        }
        while (inlen > BLAKE2B_BLOCK_LENGTH) {
            blake2b_IncrementCompress(ctx, BLAKE2B_BLOCK_LENGTH, in);
            in += BLAKE2B_BLOCK_LENGTH;
            inlen -= BLAKE2B_BLOCK_LENGTH;
        }
    }

    /* Store the remaining data from in in ctx->buf to process later.
     * Note that ctx->buflen can be BLAKE2B_BLOCK_LENGTH. We can't process that
     * here because we have to update ctx->f before compressing the last block.
     */
    PORT_Assert(inlen <= BLAKE2B_BLOCK_LENGTH);
    PORT_Memcpy(ctx->buf + ctx->buflen, in, inlen);
    ctx->buflen += inlen;

    return SECSuccess;
}

/**
 * This function finalizes ctx, pads final block and stores hash.
 * It can be used for both keyed and unkeyed version.
 */
SECStatus
BLAKE2B_End(BLAKE2BContext* ctx, unsigned char* out,
            unsigned int* digestLen, size_t maxDigestLen)
{
    size_t i;
    unsigned int outlen = PR_MIN(BLAKE2B512_LENGTH, maxDigestLen);

    /* Argument checks */
    if (!ctx || !out) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Sanity check against outlen in context. */
    if (ctx->outlen < outlen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Is this a reused context? */
    if (ctx->f != 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Process the remaining data from ctx->buf (padded with 0). */
    blake2b_IncrementCounter(ctx, ctx->buflen);
    /* BLAKE2B_BLOCK_LENGTH - ctx->buflen can be 0. */
    PORT_Memset(ctx->buf + ctx->buflen, 0, BLAKE2B_BLOCK_LENGTH - ctx->buflen);
    ctx->f = UINT64_MAX;
    blake2b_Compress(ctx, ctx->buf);

    /* Write out the blake2b context(ctx). */
    for (i = 0; i < outlen; ++i) {
        out[i] = ctx->h[i / 8] >> ((i % 8) * 8);
    }

    if (digestLen) {
        *digestLen = outlen;
    }

    return SECSuccess;
}

SECStatus
blake2b_HashBuf(uint8_t* output, const uint8_t* input, uint8_t outlen,
                size_t inlen, const uint8_t* key, size_t keylen)
{
    SECStatus rv = SECFailure;
    BLAKE2BContext ctx = { { 0 } };

    if (inlen != 0) {
        PORT_Assert(input != NULL);
        if (input == NULL) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            goto done;
        }
    }

    PORT_Assert(output != NULL);
    if (output == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto done;
    }

    if (blake2b_Begin(&ctx, outlen, key, keylen) != SECSuccess) {
        goto done;
    }

    if (BLAKE2B_Update(&ctx, input, inlen) != SECSuccess) {
        goto done;
    }

    if (BLAKE2B_End(&ctx, output, NULL, outlen) != SECSuccess) {
        goto done;
    }
    rv = SECSuccess;

done:
    PORT_Memset(&ctx, 0, sizeof ctx);
    return rv;
}

SECStatus
BLAKE2B_Hash(unsigned char* dest, const char* src)
{
    return blake2b_HashBuf(dest, (const unsigned char*)src, BLAKE2B512_LENGTH,
                           PORT_Strlen(src), NULL, 0);
}

SECStatus
BLAKE2B_HashBuf(unsigned char* output, const unsigned char* input, PRUint32 inlen)
{
    return blake2b_HashBuf(output, input, BLAKE2B512_LENGTH, inlen, NULL, 0);
}

SECStatus
BLAKE2B_MAC_HashBuf(unsigned char* output, const unsigned char* input,
                    unsigned int inlen, const unsigned char* key,
                    unsigned int keylen)
{
    PORT_Assert(key != NULL);
    if (!key && keylen <= BLAKE2B_KEY_SIZE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    return blake2b_HashBuf(output, input, BLAKE2B512_LENGTH, inlen, key, keylen);
}

unsigned int
BLAKE2B_FlattenSize(BLAKE2BContext* ctx)
{
    return sizeof(BLAKE2BContext);
}

SECStatus
BLAKE2B_Flatten(BLAKE2BContext* ctx, unsigned char* space)
{
    PORT_Assert(space != NULL);
    if (!space) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    PORT_Memcpy(space, ctx, sizeof(BLAKE2BContext));
    return SECSuccess;
}

BLAKE2BContext*
BLAKE2B_Resurrect(unsigned char* space, void* arg)
{
    PORT_Assert(space != NULL);
    if (!space) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    BLAKE2BContext* ctx = BLAKE2B_NewContext();
    if (ctx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    PORT_Memcpy(ctx, space, sizeof(BLAKE2BContext));
    return ctx;
}

void
BLAKE2B_Clone(BLAKE2BContext* dest, BLAKE2BContext* src)
{
    PORT_Assert(dest != NULL);
    PORT_Assert(src != NULL);
    if (!dest || !src) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return;
    }
    PORT_Memcpy(dest, src, sizeof(BLAKE2BContext));
}
