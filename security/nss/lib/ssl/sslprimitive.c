/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SSL Primitives: Public HKDF and AEAD Functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "keyhi.h"
#include "pk11pub.h"
#include "sechash.h"
#include "ssl.h"
#include "sslexp.h"
#include "sslerr.h"
#include "sslproto.h"

#include "sslimpl.h"
#include "tls13con.h"
#include "tls13hkdf.h"

struct SSLAeadContextStr {
    CK_MECHANISM_TYPE mech;
    ssl3KeyMaterial keys;
};

static SECStatus
tls13_GetHashAndCipher(PRUint16 version, PRUint16 cipherSuite,
                       SSLHashType *hash, const ssl3BulkCipherDef **cipher)
{
    if (version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    // Lookup and check the suite.
    SSLVersionRange vrange = { version, version };
    if (!ssl3_CipherSuiteAllowedForVersionRange(cipherSuite, &vrange)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    const ssl3CipherSuiteDef *suiteDef = ssl_LookupCipherSuiteDef(cipherSuite);
    const ssl3BulkCipherDef *cipherDef = ssl_GetBulkCipherDef(suiteDef);
    if (cipherDef->type != type_aead) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    *hash = suiteDef->prf_hash;
    *cipher = cipherDef;
    return SECSuccess;
}

SECStatus
SSLExp_MakeAead(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *secret,
                const char *labelPrefix, unsigned int labelPrefixLen,
                SSLAeadContext **ctx)
{
    SSLAeadContext *out = NULL;
    char label[255]; // Maximum length label.
    static const char *const keySuffix = "key";
    static const char *const ivSuffix = "iv";

    PORT_Assert(strlen(keySuffix) >= strlen(ivSuffix));
    if (secret == NULL || ctx == NULL ||
        (labelPrefix == NULL && labelPrefixLen > 0) ||
        labelPrefixLen + strlen(keySuffix) > sizeof(label)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    SSLHashType hash;
    const ssl3BulkCipherDef *cipher;
    SECStatus rv = tls13_GetHashAndCipher(version, cipherSuite,
                                          &hash, &cipher);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }

    out = PORT_ZNew(SSLAeadContext);
    if (out == NULL) {
        goto loser;
    }
    out->mech = ssl3_Alg2Mech(cipher->calg);

    memcpy(label, labelPrefix, labelPrefixLen);
    memcpy(label + labelPrefixLen, ivSuffix, strlen(ivSuffix));
    unsigned int labelLen = labelPrefixLen + strlen(ivSuffix);
    unsigned int ivLen = cipher->iv_size + cipher->explicit_nonce_size;
    rv = tls13_HkdfExpandLabelRaw(secret, hash,
                                  NULL, 0, // Handshake hash.
                                  label, labelLen,
                                  out->keys.iv, ivLen);
    if (rv != SECSuccess) {
        goto loser;
    }

    memcpy(label + labelPrefixLen, keySuffix, strlen(keySuffix));
    labelLen = labelPrefixLen + strlen(keySuffix);
    rv = tls13_HkdfExpandLabel(secret, hash,
                               NULL, 0, // Handshake hash.
                               label, labelLen,
                               out->mech, cipher->key_size, &out->keys.key);
    if (rv != SECSuccess) {
        goto loser;
    }

    *ctx = out;
    return SECSuccess;

loser:
    SSLExp_DestroyAead(out);
    return SECFailure;
}

SECStatus
SSLExp_DestroyAead(SSLAeadContext *ctx)
{
    if (!ctx) {
        return SECSuccess;
    }

    PK11_FreeSymKey(ctx->keys.key);
    PORT_ZFree(ctx, sizeof(*ctx));
    return SECSuccess;
}

/* Bug 1529440 exists to refactor this and the other AEAD uses. */
static SECStatus
ssl_AeadInner(const SSLAeadContext *ctx, PRBool decrypt, PRUint64 counter,
              const PRUint8 *aad, unsigned int aadLen,
              const PRUint8 *plaintext, unsigned int plaintextLen,
              PRUint8 *out, unsigned int *outLen, unsigned int maxOut)
{
    if (ctx == NULL || (aad == NULL && aadLen > 0) || plaintext == NULL ||
        out == NULL || outLen == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    // Setup the nonce.
    PRUint8 nonce[12] = { 0 };
    sslBuffer nonceBuf = SSL_BUFFER_FIXED(nonce + sizeof(nonce) - sizeof(counter),
                                          sizeof(counter));
    SECStatus rv = sslBuffer_AppendNumber(&nonceBuf, counter, sizeof(counter));
    if (rv != SECSuccess) {
        PORT_Assert(0);
        return SECFailure;
    }
    for (int i = 0; i < sizeof(nonce); ++i) {
        nonce[i] ^= ctx->keys.iv[i];
    }

    // Build AEAD parameters.
    CK_GCM_PARAMS gcmParams = { 0 };
    CK_NSS_AEAD_PARAMS aeadParams = { 0 };
    unsigned char *params;
    unsigned int paramsLen;
    switch (ctx->mech) {
        case CKM_AES_GCM:
            gcmParams.pIv = nonce;
            gcmParams.ulIvLen = sizeof(nonce);
            gcmParams.pAAD = (unsigned char *)aad; // const cast :(
            gcmParams.ulAADLen = aadLen;
            gcmParams.ulTagBits = 128; // GCM measures in bits.
            params = (unsigned char *)&gcmParams;
            paramsLen = sizeof(gcmParams);
            break;

        case CKM_NSS_CHACHA20_POLY1305:
            aeadParams.pNonce = nonce;
            aeadParams.ulNonceLen = sizeof(nonce);
            aeadParams.pAAD = (unsigned char *)aad; // const cast :(
            aeadParams.ulAADLen = aadLen;
            aeadParams.ulTagLen = 16; // AEAD measures in octets.
            params = (unsigned char *)&aeadParams;
            paramsLen = sizeof(aeadParams);
            break;

        default:
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
    }

    return tls13_AEAD(&ctx->keys, decrypt, out, outLen, maxOut,
                      plaintext, plaintextLen, ctx->mech, params, paramsLen);
}

SECStatus
SSLExp_AeadEncrypt(const SSLAeadContext *ctx, PRUint64 counter,
                   const PRUint8 *aad, unsigned int aadLen,
                   const PRUint8 *plaintext, unsigned int plaintextLen,
                   PRUint8 *out, unsigned int *outLen, unsigned int maxOut)
{
    // false == encrypt
    return ssl_AeadInner(ctx, PR_FALSE, counter, aad, aadLen,
                         plaintext, plaintextLen, out, outLen, maxOut);
}

SECStatus
SSLExp_AeadDecrypt(const SSLAeadContext *ctx, PRUint64 counter,
                   const PRUint8 *aad, unsigned int aadLen,
                   const PRUint8 *plaintext, unsigned int plaintextLen,
                   PRUint8 *out, unsigned int *outLen, unsigned int maxOut)
{
    // true == decrypt
    return ssl_AeadInner(ctx, PR_TRUE, counter, aad, aadLen,
                         plaintext, plaintextLen, out, outLen, maxOut);
}

SECStatus
SSLExp_HkdfExtract(PRUint16 version, PRUint16 cipherSuite,
                   PK11SymKey *salt, PK11SymKey *ikm, PK11SymKey **keyp)
{
    if (keyp == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    SSLHashType hash;
    const ssl3BulkCipherDef *cipher; /* Unused here. */
    SECStatus rv = tls13_GetHashAndCipher(version, cipherSuite,
                                          &hash, &cipher);
    if (rv != SECSuccess) {
        return SECFailure; /* Code already set. */
    }
    return tls13_HkdfExtract(salt, ikm, hash, keyp);
}

SECStatus
SSLExp_HkdfDeriveSecret(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *prk,
                        const char *label, unsigned int labelLen,
                        PK11SymKey **keyp)
{
    if (prk == NULL || keyp == NULL ||
        label == NULL || labelLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    SSLHashType hash;
    const ssl3BulkCipherDef *cipher; /* Unused here. */
    SECStatus rv = tls13_GetHashAndCipher(version, cipherSuite,
                                          &hash, &cipher);
    if (rv != SECSuccess) {
        return SECFailure; /* Code already set. */
    }
    return tls13_HkdfExpandLabel(prk, hash, NULL, 0, label, labelLen,
                                 tls13_GetHkdfMechanismForHash(hash),
                                 tls13_GetHashSizeForHash(hash), keyp);
}
