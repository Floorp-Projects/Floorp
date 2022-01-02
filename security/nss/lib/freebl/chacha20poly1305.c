/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include <string.h>
#include <stdio.h>

#include "seccomon.h"
#include "secerr.h"
#include "blapit.h"
#include "blapii.h"
#include "chacha20poly1305.h"

// There are three implementations of ChaCha20Poly1305:
// 1) 128-bit with AVX hardware acceleration used on x64
// 2) 256-bit with AVX2 hardware acceleration used on x64
// 3) 32-bit used on all other platforms

// On x64 when AVX2 and other necessary registers are available,
// the 256bit-verctorized version will be used. When AVX2 features
// are unavailable or disabled but AVX registers are available, the
// 128bit-vectorized version will be used. In all other cases the
// scalar version of the HACL* code will be used.

// Instead of including the headers (they bring other things we don't want),
// we declare the functions here.
// Usage is guarded by runtime checks of required hardware features.

// Forward declaration from Hacl_Chacha20_Vec128.h and Hacl_Chacha20Poly1305_128.h.
extern void Hacl_Chacha20_Vec128_chacha20_encrypt_128(uint32_t len, uint8_t *out,
                                                      uint8_t *text, uint8_t *key,
                                                      uint8_t *n1, uint32_t ctr);
extern void
Hacl_Chacha20Poly1305_128_aead_encrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                       uint8_t *aad, uint32_t mlen, uint8_t *m,
                                       uint8_t *cipher, uint8_t *mac);
extern uint32_t
Hacl_Chacha20Poly1305_128_aead_decrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                       uint8_t *aad, uint32_t mlen, uint8_t *m,
                                       uint8_t *cipher, uint8_t *mac);

// Forward declaration from Hacl_Chacha20_Vec256.h and Hacl_Chacha20Poly1305_256.h.
extern void Hacl_Chacha20_Vec256_chacha20_encrypt_256(uint32_t len, uint8_t *out,
                                                      uint8_t *text, uint8_t *key,
                                                      uint8_t *n1, uint32_t ctr);
extern void
Hacl_Chacha20Poly1305_256_aead_encrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                       uint8_t *aad, uint32_t mlen, uint8_t *m,
                                       uint8_t *cipher, uint8_t *mac);
extern uint32_t
Hacl_Chacha20Poly1305_256_aead_decrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                       uint8_t *aad, uint32_t mlen, uint8_t *m,
                                       uint8_t *cipher, uint8_t *mac);

// Forward declaration from Hacl_Chacha20.h and Hacl_Chacha20Poly1305_32.h.
extern void Hacl_Chacha20_chacha20_encrypt(uint32_t len, uint8_t *out,
                                           uint8_t *text, uint8_t *key,
                                           uint8_t *n1, uint32_t ctr);
extern void
Hacl_Chacha20Poly1305_32_aead_encrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                      uint8_t *aad, uint32_t mlen, uint8_t *m,
                                      uint8_t *cipher, uint8_t *mac);
extern uint32_t
Hacl_Chacha20Poly1305_32_aead_decrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                      uint8_t *aad, uint32_t mlen, uint8_t *m,
                                      uint8_t *cipher, uint8_t *mac);

// Forward declaration from chacha20-ppc64le.S
void chacha20vsx(uint32_t len, uint8_t *output, uint8_t *block, uint8_t *k,
                 uint8_t *nonce, uint32_t ctr);

// Forward declaration from chacha20poly1305-ppc.c
extern void
Chacha20Poly1305_vsx_aead_encrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                  uint8_t *aad, uint32_t mlen, uint8_t *m,
                                  uint8_t *cipher, uint8_t *mac);
extern uint32_t
Chacha20Poly1305_vsx_aead_decrypt(uint8_t *k, uint8_t *n1, uint32_t aadlen,
                                  uint8_t *aad, uint32_t mlen, uint8_t *m,
                                  uint8_t *cipher, uint8_t *mac);

SECStatus
ChaCha20_InitContext(ChaCha20Context *ctx, const unsigned char *key,
                     unsigned int keyLen, const unsigned char *nonce,
                     unsigned int nonceLen, PRUint32 ctr)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return SECFailure;
#else
    if (keyLen != 32) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }
    if (nonceLen != 12) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    ctx->counter = ctr;
    PORT_Memcpy(ctx->key, key, sizeof(ctx->key));
    PORT_Memcpy(ctx->nonce, nonce, sizeof(ctx->nonce));

    return SECSuccess;
#endif
}

ChaCha20Context *
ChaCha20_CreateContext(const unsigned char *key, unsigned int keyLen,
                       const unsigned char *nonce, unsigned int nonceLen,
                       PRUint32 ctr)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return NULL;
#else
    ChaCha20Context *ctx;

    ctx = PORT_New(ChaCha20Context);
    if (ctx == NULL) {
        return NULL;
    }

    if (ChaCha20_InitContext(ctx, key, keyLen, nonce, nonceLen, ctr) != SECSuccess) {
        PORT_Free(ctx);
        ctx = NULL;
    }

    return ctx;
#endif
}

void
ChaCha20_DestroyContext(ChaCha20Context *ctx, PRBool freeit)
{
#ifndef NSS_DISABLE_CHACHAPOLY
    PORT_Memset(ctx, 0, sizeof(*ctx));
    if (freeit) {
        PORT_Free(ctx);
    }
#endif
}

SECStatus
ChaCha20Poly1305_InitContext(ChaCha20Poly1305Context *ctx,
                             const unsigned char *key, unsigned int keyLen,
                             unsigned int tagLen)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return SECFailure;
#else
    if (keyLen != 32) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }
    if (tagLen != 16) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    PORT_Memcpy(ctx->key, key, sizeof(ctx->key));
    ctx->tagLen = tagLen;

    return SECSuccess;
#endif
}

ChaCha20Poly1305Context *
ChaCha20Poly1305_CreateContext(const unsigned char *key, unsigned int keyLen,
                               unsigned int tagLen)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return NULL;
#else
    ChaCha20Poly1305Context *ctx;

    ctx = PORT_New(ChaCha20Poly1305Context);
    if (ctx == NULL) {
        return NULL;
    }

    if (ChaCha20Poly1305_InitContext(ctx, key, keyLen, tagLen) != SECSuccess) {
        PORT_Free(ctx);
        ctx = NULL;
    }

    return ctx;
#endif
}

void
ChaCha20Poly1305_DestroyContext(ChaCha20Poly1305Context *ctx, PRBool freeit)
{
#ifndef NSS_DISABLE_CHACHAPOLY
    PORT_Memset(ctx, 0, sizeof(*ctx));
    if (freeit) {
        PORT_Free(ctx);
    }
#endif
}

#ifndef NSS_DISABLE_CHACHAPOLY
void
ChaCha20Xor(uint8_t *output, uint8_t *block, uint32_t len, uint8_t *k,
            uint8_t *nonce, uint32_t ctr)
{
#ifdef NSS_X64
    if (ssse3_support() && sse4_1_support() && avx_support()) {
#ifdef NSS_DISABLE_AVX2
        Hacl_Chacha20_Vec128_chacha20_encrypt_128(len, output, block, k, nonce, ctr);
#else
        if (avx2_support()) {
            Hacl_Chacha20_Vec256_chacha20_encrypt_256(len, output, block, k, nonce, ctr);
        } else {
            Hacl_Chacha20_Vec128_chacha20_encrypt_128(len, output, block, k, nonce, ctr);
        }
#endif
    } else
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__) && \
    !defined(NSS_DISABLE_ALTIVEC) && !defined(NSS_DISABLE_CRYPTO_VSX)
    if (ppc_crypto_support()) {
        chacha20vsx(len, output, block, k, nonce, ctr);
    } else
#endif
    {
        Hacl_Chacha20_chacha20_encrypt(len, output, block, k, nonce, ctr);
    }
}
#endif /* NSS_DISABLE_CHACHAPOLY */

SECStatus
ChaCha20_Xor(unsigned char *output, const unsigned char *block, unsigned int len,
             const unsigned char *k, const unsigned char *nonce, PRUint32 ctr)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return SECFailure;
#else
    // ChaCha has a 64 octet block, with a 32-bit block counter.
    if (sizeof(len) > 4 && len >= (1ULL << (6 + 32))) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    ChaCha20Xor(output, (uint8_t *)block, len, (uint8_t *)k,
                (uint8_t *)nonce, ctr);
    return SECSuccess;
#endif
}

SECStatus
ChaCha20Poly1305_Seal(const ChaCha20Poly1305Context *ctx, unsigned char *output,
                      unsigned int *outputLen, unsigned int maxOutputLen,
                      const unsigned char *input, unsigned int inputLen,
                      const unsigned char *nonce, unsigned int nonceLen,
                      const unsigned char *ad, unsigned int adLen)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return SECFailure;
#else

    if (nonceLen != 12) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    // ChaCha has a 64 octet block, with a 32-bit block counter.
    if (sizeof(inputLen) > 4 && inputLen >= (1ULL << (6 + 32))) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    if (maxOutputLen < inputLen + ctx->tagLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

#ifdef NSS_X64
    if (ssse3_support() && sse4_1_support() && avx_support()) {
#ifdef NSS_DISABLE_AVX2
        Hacl_Chacha20Poly1305_128_aead_encrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
            (uint8_t *)input, output, output + inputLen);
#else
        if (avx2_support()) {
            Hacl_Chacha20Poly1305_256_aead_encrypt(
                (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
                (uint8_t *)input, output, output + inputLen);
        } else {
            Hacl_Chacha20Poly1305_128_aead_encrypt(
                (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
                (uint8_t *)input, output, output + inputLen);
        }
#endif
    } else
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__) && \
    !defined(NSS_DISABLE_ALTIVEC) && !defined(NSS_DISABLE_CRYPTO_VSX)
    if (ppc_crypto_support()) {
        Chacha20Poly1305_vsx_aead_encrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
            (uint8_t *)input, output, output + inputLen);
    } else
#endif
    {
        Hacl_Chacha20Poly1305_32_aead_encrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
            (uint8_t *)input, output, output + inputLen);
    }

    *outputLen = inputLen + ctx->tagLen;
    return SECSuccess;
#endif
}

SECStatus
ChaCha20Poly1305_Open(const ChaCha20Poly1305Context *ctx, unsigned char *output,
                      unsigned int *outputLen, unsigned int maxOutputLen,
                      const unsigned char *input, unsigned int inputLen,
                      const unsigned char *nonce, unsigned int nonceLen,
                      const unsigned char *ad, unsigned int adLen)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return SECFailure;
#else
    unsigned int ciphertextLen;

    if (nonceLen != 12) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    if (inputLen < ctx->tagLen) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    ciphertextLen = inputLen - ctx->tagLen;
    if (maxOutputLen < ciphertextLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    // ChaCha has a 64 octet block, with a 32-bit block counter.
    if (inputLen >= (1ULL << (6 + 32)) + ctx->tagLen) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    uint32_t res = 1;
#ifdef NSS_X64
    if (ssse3_support() && sse4_1_support() && avx_support()) {
#ifdef NSS_DISABLE_AVX2
        res = Hacl_Chacha20Poly1305_128_aead_decrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
            (uint8_t *)output, (uint8_t *)input, (uint8_t *)input + ciphertextLen);
#else
        if (avx2_support()) {
            res = Hacl_Chacha20Poly1305_256_aead_decrypt(
                (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
                (uint8_t *)output, (uint8_t *)input, (uint8_t *)input + ciphertextLen);
        } else {
            res = Hacl_Chacha20Poly1305_128_aead_decrypt(
                (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
                (uint8_t *)output, (uint8_t *)input, (uint8_t *)input + ciphertextLen);
        }
#endif
    } else
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__) && \
    !defined(NSS_DISABLE_ALTIVEC) && !defined(NSS_DISABLE_CRYPTO_VSX)
    if (ppc_crypto_support()) {
        res = Chacha20Poly1305_vsx_aead_decrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
            (uint8_t *)output, (uint8_t *)input, (uint8_t *)input + ciphertextLen);
    } else
#endif
    {
        res = Hacl_Chacha20Poly1305_32_aead_decrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
            (uint8_t *)output, (uint8_t *)input, (uint8_t *)input + ciphertextLen);
    }

    if (res) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    *outputLen = ciphertextLen;
    return SECSuccess;
#endif
}

SECStatus
ChaCha20Poly1305_Encrypt(const ChaCha20Poly1305Context *ctx,
                         unsigned char *output, unsigned int *outputLen,
                         unsigned int maxOutputLen, const unsigned char *input,
                         unsigned int inputLen, const unsigned char *nonce,
                         unsigned int nonceLen, const unsigned char *ad,
                         unsigned int adLen, unsigned char *outTag)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return SECFailure;
#else

    if (nonceLen != 12) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    // ChaCha has a 64 octet block, with a 32-bit block counter.
    if (sizeof(inputLen) > 4 && inputLen >= (1ULL << (6 + 32))) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    if (maxOutputLen < inputLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

#ifdef NSS_X64
    if (ssse3_support() && sse4_1_support() && avx_support()) {
        Hacl_Chacha20Poly1305_128_aead_encrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
            (uint8_t *)input, output, outTag);
    } else
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__) && \
    !defined(NSS_DISABLE_ALTIVEC) && !defined(NSS_DISABLE_CRYPTO_VSX)
    if (ppc_crypto_support()) {
        Chacha20Poly1305_vsx_aead_encrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
            (uint8_t *)input, output, outTag);
    } else
#endif
    {
        Hacl_Chacha20Poly1305_32_aead_encrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, inputLen,
            (uint8_t *)input, output, outTag);
    }

    *outputLen = inputLen;
    return SECSuccess;
#endif
}

SECStatus
ChaCha20Poly1305_Decrypt(const ChaCha20Poly1305Context *ctx,
                         unsigned char *output, unsigned int *outputLen,
                         unsigned int maxOutputLen, const unsigned char *input,
                         unsigned int inputLen, const unsigned char *nonce,
                         unsigned int nonceLen, const unsigned char *ad,
                         unsigned int adLen, const unsigned char *tagIn)
{
#ifdef NSS_DISABLE_CHACHAPOLY
    return SECFailure;
#else
    unsigned int ciphertextLen;

    if (nonceLen != 12) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }
    ciphertextLen = inputLen;
    if (maxOutputLen < ciphertextLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    // ChaCha has a 64 octet block, with a 32-bit block counter.
    if (sizeof(inputLen) > 4 && inputLen >= (1ULL << (6 + 32))) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    uint32_t res = 1;
#ifdef NSS_X64
    if (ssse3_support() && sse4_1_support() && avx_support()) {
        res = Hacl_Chacha20Poly1305_128_aead_decrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
            (uint8_t *)output, (uint8_t *)input, (uint8_t *)tagIn);
    } else
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__) && \
    !defined(NSS_DISABLE_ALTIVEC) && !defined(NSS_DISABLE_CRYPTO_VSX)
    if (ppc_crypto_support()) {
        res = Chacha20Poly1305_vsx_aead_decrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
            (uint8_t *)output, (uint8_t *)input, (uint8_t *)tagIn);
    } else
#endif
    {
        res = Hacl_Chacha20Poly1305_32_aead_decrypt(
            (uint8_t *)ctx->key, (uint8_t *)nonce, adLen, (uint8_t *)ad, ciphertextLen,
            (uint8_t *)output, (uint8_t *)input, (uint8_t *)tagIn);
    }

    if (res) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    *outputLen = ciphertextLen;
    return SECSuccess;
#endif
}
