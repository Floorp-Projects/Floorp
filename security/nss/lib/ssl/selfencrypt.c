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
#include "ssl3encode.h"
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
    unsigned char iv[AES_BLOCK_SIZE];
    SECItem ivItem = { siBuffer, iv, sizeof(iv) };
    unsigned char mac[SHA256_LENGTH]; /* SHA-256 */
    unsigned int macLen;
    SECItem outItem = { siBuffer, out, maxOutLen };
    SECItem lengthBytesItem;
    SECStatus rv;

    /* Generate a random IV */
    rv = PK11_GenerateRandom(iv, sizeof(iv));
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* Add header. */
    rv = ssl3_AppendToItem(&outItem, keyName, SELF_ENCRYPT_KEY_NAME_LEN);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = ssl3_AppendToItem(&outItem, iv, sizeof(iv));
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Skip forward by two so we can encode the ciphertext in place. */
    lengthBytesItem = outItem;
    rv = ssl3_AppendNumberToItem(&outItem, 0, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = PK11_Encrypt(encKey, CKM_AES_CBC_PAD, &ivItem,
                      outItem.data, &len, outItem.len, in, inLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    outItem.data += len;
    outItem.len -= len;

    /* Now encode the ciphertext length. */
    rv = ssl3_AppendNumberToItem(&lengthBytesItem, len, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* MAC the entire output buffer and append the MAC to the end. */
    rv = ssl_MacBuffer(macKey, CKM_SHA256_HMAC,
                       out, outItem.data - out,
                       mac, &macLen, sizeof(mac));
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PORT_Assert(macLen == sizeof(mac));

    rv = ssl3_AppendToItem(&outItem, mac, macLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *outLen = outItem.data - out;
    return SECSuccess;
}

SECStatus
ssl_SelfEncryptUnprotectInt(
    PK11SymKey *encKey, PK11SymKey *macKey, const unsigned char *keyName,
    const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen)
{
    unsigned char *encodedKeyName;
    unsigned char *iv;
    SECItem ivItem = { siBuffer, NULL, 0 };
    SECItem inItem = { siBuffer, (unsigned char *)in, inLen };
    unsigned char *cipherText;
    PRUint32 cipherTextLen;
    unsigned char *encodedMac;
    unsigned char computedMac[SHA256_LENGTH];
    unsigned int computedMacLen;
    unsigned int bytesToMac;
    SECStatus rv;

    rv = ssl3_ConsumeFromItem(&inItem, &encodedKeyName,
                              SELF_ENCRYPT_KEY_NAME_LEN);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = ssl3_ConsumeFromItem(&inItem, &iv, AES_BLOCK_SIZE);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = ssl3_ConsumeNumberFromItem(&inItem, &cipherTextLen, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = ssl3_ConsumeFromItem(&inItem, &cipherText, cipherTextLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    bytesToMac = inItem.data - in;

    rv = ssl3_ConsumeFromItem(&inItem, &encodedMac, SHA256_LENGTH);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Make sure we're at the end of the block. */
    if (inItem.len) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    /* Now that everything is decoded, we can make progress. */
    /* 1. Check that we have the right key. */
    if (PORT_Memcmp(keyName, encodedKeyName, SELF_ENCRYPT_KEY_NAME_LEN)) {
        PORT_SetError(SEC_ERROR_NOT_A_RECIPIENT);
        return SECFailure;
    }

    /* 2. Check the MAC */
    rv = ssl_MacBuffer(macKey, CKM_SHA256_HMAC, in, bytesToMac,
                       computedMac, &computedMacLen, sizeof(computedMac));
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PORT_Assert(computedMacLen == SHA256_LENGTH);
    if (NSS_SecureMemcmp(computedMac, encodedMac, computedMacLen) != 0) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    /* 3. OK, it verifies, now decrypt. */
    ivItem.data = iv;
    ivItem.len = AES_BLOCK_SIZE;
    rv = PK11_Decrypt(encKey, CKM_AES_CBC_PAD, &ivItem,
                      out, outLen, maxOutLen, cipherText, cipherTextLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}
#endif

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
