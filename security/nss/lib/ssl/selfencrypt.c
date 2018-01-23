/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "blapit.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslt.h"
#include "sslimpl.h"
#include "selfencrypt.h"

static SECStatus
ssl_MacBuffer(PK11SymKey *key, CK_MECHANISM_TYPE mech,
              const unsigned char *in, unsigned int len,
              unsigned char *mac, unsigned int *macLen, unsigned int maxMacLen)
{
    PK11Context *ctx;
    SECItem macParam = { 0, NULL, 0 };
    unsigned int computedLen;
    SECStatus rv;

    ctx = PK11_CreateContextBySymKey(mech, CKA_SIGN, key, &macParam);
    if (!ctx) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    rv = PK11_DigestBegin(ctx);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser;
    }

    rv = PK11_DigestOp(ctx, in, len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser;
    }

    rv = PK11_DigestFinal(ctx, mac, &computedLen, maxMacLen);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser;
    }

    *macLen = maxMacLen;
    PK11_DestroyContext(ctx, PR_TRUE);
    return SECSuccess;

loser:
    PK11_DestroyContext(ctx, PR_TRUE);
    return SECFailure;
}

#ifdef UNSAFE_FUZZER_MODE
SECStatus
ssl_SelfEncryptProtectInt(
    PK11SymKey *encKey, PK11SymKey *macKey,
    const unsigned char *keyName,
    const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen)
{
    if (inLen > maxOutLen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    PORT_Memcpy(out, in, inLen);
    *outLen = inLen;

    return 0;
}

SECStatus
ssl_SelfEncryptUnprotectInt(
    PK11SymKey *encKey, PK11SymKey *macKey, const unsigned char *keyName,
    const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen)
{
    if (inLen > maxOutLen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    PORT_Memcpy(out, in, inLen);
    *outLen = inLen;

    return 0;
}

#else
/*
 * Structure is.
 *
 * struct {
 *   opaque keyName[16];
 *   opaque iv[16];
 *   opaque ciphertext<16..2^16-1>;
 *   opaque mac[32];
 * } SelfEncrypted;
 *
 * We are using AES-CBC + HMAC-SHA256 in Encrypt-then-MAC mode for
 * two reasons:
 *
 * 1. It's what we already used for tickets.
 * 2. We don't have to worry about nonce collisions as much
 *    (the chance is lower because we have a random 128-bit nonce
 *    and they are less serious than with AES-GCM).
 */
SECStatus
ssl_SelfEncryptProtectInt(
    PK11SymKey *encKey, PK11SymKey *macKey,
    const unsigned char *keyName,
    const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen)
{
    unsigned int len;
    unsigned int lenOffset;
    unsigned char iv[AES_BLOCK_SIZE];
    SECItem ivItem = { siBuffer, iv, sizeof(iv) };
    /* Write directly to out. */
    sslBuffer buf = SSL_BUFFER_FIXED(out, maxOutLen);
    SECStatus rv;

    /* Generate a random IV */
    rv = PK11_GenerateRandom(iv, sizeof(iv));
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* Add header. */
    rv = sslBuffer_Append(&buf, keyName, SELF_ENCRYPT_KEY_NAME_LEN);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Append(&buf, iv, sizeof(iv));
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Leave space for the length of the ciphertext. */
    rv = sslBuffer_Skip(&buf, 2, &lenOffset);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Encode the ciphertext in place. */
    rv = PK11_Encrypt(encKey, CKM_AES_CBC_PAD, &ivItem,
                      SSL_BUFFER_NEXT(&buf), &len,
                      SSL_BUFFER_SPACE(&buf), in, inLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Skip(&buf, len, NULL);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = sslBuffer_InsertLength(&buf, lenOffset, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* MAC the entire output buffer into the output. */
    PORT_Assert(buf.space - buf.len >= SHA256_LENGTH);
    rv = ssl_MacBuffer(macKey, CKM_SHA256_HMAC,
                       SSL_BUFFER_BASE(&buf), /* input */
                       SSL_BUFFER_LEN(&buf),
                       SSL_BUFFER_NEXT(&buf), &len, /* output */
                       SHA256_LENGTH);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Skip(&buf, len, NULL);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *outLen = SSL_BUFFER_LEN(&buf);
    return SECSuccess;
}

SECStatus
ssl_SelfEncryptUnprotectInt(
    PK11SymKey *encKey, PK11SymKey *macKey, const unsigned char *keyName,
    const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen)
{
    sslReader reader = SSL_READER(in, inLen);

    sslReadBuffer encodedKeyNameBuffer = { 0 };
    SECStatus rv = sslRead_Read(&reader, SELF_ENCRYPT_KEY_NAME_LEN,
                                &encodedKeyNameBuffer);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    sslReadBuffer ivBuffer = { 0 };
    rv = sslRead_Read(&reader, AES_BLOCK_SIZE, &ivBuffer);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    PRUint64 cipherTextLen = 0;
    rv = sslRead_ReadNumber(&reader, 2, &cipherTextLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    sslReadBuffer cipherTextBuffer = { 0 };
    rv = sslRead_Read(&reader, (unsigned int)cipherTextLen, &cipherTextBuffer);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    unsigned int bytesToMac = reader.offset;

    sslReadBuffer encodedMacBuffer = { 0 };
    rv = sslRead_Read(&reader, SHA256_LENGTH, &encodedMacBuffer);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Make sure we're at the end of the block. */
    if (reader.offset != reader.buf.len) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    /* Now that everything is decoded, we can make progress. */
    /* 1. Check that we have the right key. */
    if (PORT_Memcmp(keyName, encodedKeyNameBuffer.buf, SELF_ENCRYPT_KEY_NAME_LEN)) {
        PORT_SetError(SEC_ERROR_NOT_A_RECIPIENT);
        return SECFailure;
    }

    /* 2. Check the MAC */
    unsigned char computedMac[SHA256_LENGTH];
    unsigned int computedMacLen = 0;
    rv = ssl_MacBuffer(macKey, CKM_SHA256_HMAC, in, bytesToMac,
                       computedMac, &computedMacLen, sizeof(computedMac));
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PORT_Assert(computedMacLen == SHA256_LENGTH);
    if (NSS_SecureMemcmp(computedMac, encodedMacBuffer.buf, computedMacLen) != 0) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    /* 3. OK, it verifies, now decrypt. */
    SECItem ivItem = { siBuffer, (unsigned char *)ivBuffer.buf, AES_BLOCK_SIZE };
    rv = PK11_Decrypt(encKey, CKM_AES_CBC_PAD, &ivItem,
                      out, outLen, maxOutLen, cipherTextBuffer.buf, cipherTextLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}
#endif

/* Predict the size of the encrypted data, including padding */
unsigned int
ssl_SelfEncryptGetProtectedSize(unsigned int inLen)
{
    return SELF_ENCRYPT_KEY_NAME_LEN +
           AES_BLOCK_SIZE +
           2 +
           ((inLen / AES_BLOCK_SIZE) + 1) * AES_BLOCK_SIZE + /* Padded */
           SHA256_LENGTH;
}

SECStatus
ssl_SelfEncryptProtect(
    sslSocket *ss, const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen)
{
    PRUint8 keyName[SELF_ENCRYPT_KEY_NAME_LEN];
    PK11SymKey *encKey;
    PK11SymKey *macKey;
    SECStatus rv;

    /* Get session ticket keys. */
    rv = ssl_GetSelfEncryptKeys(ss, keyName, &encKey, &macKey);
    if (rv != SECSuccess) {
        SSL_DBG(("%d: SSL[%d]: Unable to get/generate self-encrypt keys.",
                 SSL_GETPID(), ss->fd));
        return SECFailure;
    }

    return ssl_SelfEncryptProtectInt(encKey, macKey, keyName,
                                     in, inLen, out, outLen, maxOutLen);
}

SECStatus
ssl_SelfEncryptUnprotect(
    sslSocket *ss, const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen)
{
    PRUint8 keyName[SELF_ENCRYPT_KEY_NAME_LEN];
    PK11SymKey *encKey;
    PK11SymKey *macKey;
    SECStatus rv;

    /* Get session ticket keys. */
    rv = ssl_GetSelfEncryptKeys(ss, keyName, &encKey, &macKey);
    if (rv != SECSuccess) {
        SSL_DBG(("%d: SSL[%d]: Unable to get/generate self-encrypt keys.",
                 SSL_GETPID(), ss->fd));
        return SECFailure;
    }

    return ssl_SelfEncryptUnprotectInt(encKey, macKey, keyName,
                                       in, inLen, out, outLen, maxOutLen);
}
