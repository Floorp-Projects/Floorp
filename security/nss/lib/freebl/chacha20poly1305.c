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

#ifndef NSS_DISABLE_CHACHAPOLY
#include "chacha20poly1305.h"
// Forward declaration from "Hacl_Chacha20_Vec128.h".
extern void Hacl_Chacha20_Vec128_chacha20(uint8_t *output, uint8_t *plain,
                                          uint32_t len, uint8_t *k, uint8_t *n1,
                                          uint32_t ctr);
// Forward declaration from "Hacl_Chacha20.h".
extern void Hacl_Chacha20_chacha20(uint8_t *output, uint8_t *plain, uint32_t len,
                                   uint8_t *k, uint8_t *n1, uint32_t ctr);

#if defined(HAVE_INT128_SUPPORT) && (defined(NSS_X86_OR_X64) || defined(__aarch64__))
/* Use HACL* Poly1305 on 64-bit Intel and ARM */
#include "verified/Hacl_Poly1305_64.h"
#define NSS_POLY1305_64 1
#define Hacl_Poly1305_update Hacl_Poly1305_64_update
#define Hacl_Poly1305_mk_state Hacl_Poly1305_64_mk_state
#define Hacl_Poly1305_init Hacl_Poly1305_64_init
#define Hacl_Poly1305_finish Hacl_Poly1305_64_finish
typedef Hacl_Impl_Poly1305_64_State_poly1305_state Hacl_Impl_Poly1305_State_poly1305_state;
#else
/* All other platforms get the 32-bit poly1305 HACL* implementation. */
#include "verified/Hacl_Poly1305_32.h"
#define NSS_POLY1305_32 1
#define Hacl_Poly1305_update Hacl_Poly1305_32_update
#define Hacl_Poly1305_mk_state Hacl_Poly1305_32_mk_state
#define Hacl_Poly1305_init Hacl_Poly1305_32_init
#define Hacl_Poly1305_finish Hacl_Poly1305_32_finish
typedef Hacl_Impl_Poly1305_32_State_poly1305_state Hacl_Impl_Poly1305_State_poly1305_state;
#endif /* HAVE_INT128_SUPPORT */

static void
Poly1305PadUpdate(Hacl_Impl_Poly1305_State_poly1305_state state,
                  unsigned char *block, const unsigned char *p,
                  const unsigned int pLen)
{
    unsigned int pRemLen = pLen % 16;
    Hacl_Poly1305_update(state, (uint8_t *)p, (pLen / 16));
    if (pRemLen > 0) {
        memcpy(block, p + (pLen - pRemLen), pRemLen);
        Hacl_Poly1305_update(state, block, 1);
    }
}

/* Poly1305Do writes the Poly1305 authenticator of the given additional data
 * and ciphertext to |out|. */
static void
Poly1305Do(unsigned char *out, const unsigned char *ad, unsigned int adLen,
           const unsigned char *ciphertext, unsigned int ciphertextLen,
           const unsigned char key[32])
{
#ifdef NSS_POLY1305_64
    uint64_t stateStack[6U] = { 0U };
    size_t offset = 3;
#elif defined NSS_POLY1305_32
    uint32_t stateStack[10U] = { 0U };
    size_t offset = 5;
#else
#error "This can't happen."
#endif
    Hacl_Impl_Poly1305_State_poly1305_state state =
        Hacl_Poly1305_mk_state(stateStack, stateStack + offset);

    unsigned char block[16] = { 0 };
    Hacl_Poly1305_init(state, (uint8_t *)key);

    Poly1305PadUpdate(state, block, ad, adLen);
    memset(block, 0, 16);
    Poly1305PadUpdate(state, block, ciphertext, ciphertextLen);

    unsigned int i;
    unsigned int j;
    for (i = 0, j = adLen; i < 8; i++, j >>= 8) {
        block[i] = j;
    }
    for (i = 8, j = ciphertextLen; i < 16; i++, j >>= 8) {
        block[i] = j;
    }

    Hacl_Poly1305_update(state, block, 1);
    Hacl_Poly1305_finish(state, out, (uint8_t *)(key + 16));
#undef NSS_POLY1305_64
#undef NSS_POLY1305_32
}
#endif /* NSS_DISABLE_CHACHAPOLY */

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
    if (tagLen == 0 || tagLen > 16) {
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
    if (ssse3_support() || arm_neon_support()) {
        Hacl_Chacha20_Vec128_chacha20(output, block, len, k, nonce, ctr);
    } else {
        Hacl_Chacha20_chacha20(output, block, len, k, nonce, ctr);
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
    unsigned char block[64];
    unsigned char tag[16];

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

    PORT_Memset(block, 0, sizeof(block));
    // Generate a block of keystream. The first 32 bytes will be the poly1305
    // key. The remainder of the block is discarded.
    ChaCha20Xor(block, (uint8_t *)block, sizeof(block), (uint8_t *)ctx->key,
                (uint8_t *)nonce, 0);
    ChaCha20Xor(output, (uint8_t *)input, inputLen, (uint8_t *)ctx->key,
                (uint8_t *)nonce, 1);

    Poly1305Do(tag, ad, adLen, output, inputLen, block);
    PORT_Memcpy(output + inputLen, tag, ctx->tagLen);

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
    unsigned char block[64];
    unsigned char tag[16];
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

    PORT_Memset(block, 0, sizeof(block));
    // Generate a block of keystream. The first 32 bytes will be the poly1305
    // key. The remainder of the block is discarded.
    ChaCha20Xor(block, (uint8_t *)block, sizeof(block), (uint8_t *)ctx->key,
                (uint8_t *)nonce, 0);
    Poly1305Do(tag, ad, adLen, input, ciphertextLen, block);
    if (NSS_SecureMemcmp(tag, &input[ciphertextLen], ctx->tagLen) != 0) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    ChaCha20Xor(output, (uint8_t *)input, ciphertextLen, (uint8_t *)ctx->key,
                (uint8_t *)nonce, 1);

    *outputLen = ciphertextLen;
    return SECSuccess;
#endif
}
