/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SSL Primitives: Public HKDF and AEAD Functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapit.h"
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
    SECStatus rv = tls13_GetHashAndCipher(version, cipherSuite,
                                          &hash, NULL);
    if (rv != SECSuccess) {
        return SECFailure; /* Code already set. */
    }
    return tls13_HkdfExtract(salt, ikm, hash, keyp);
}

SECStatus
SSLExp_HkdfExpandLabel(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *prk,
                       const PRUint8 *hsHash, unsigned int hsHashLen,
                       const char *label, unsigned int labelLen,
                       PK11SymKey **keyp)
{
    if (prk == NULL || keyp == NULL ||
        label == NULL || labelLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    SSLHashType hash;
    SECStatus rv = tls13_GetHashAndCipher(version, cipherSuite,
                                          &hash, NULL);
    if (rv != SECSuccess) {
        return SECFailure; /* Code already set. */
    }
    return tls13_HkdfExpandLabel(prk, hash, hsHash, hsHashLen, label, labelLen,
                                 tls13_GetHkdfMechanismForHash(hash),
                                 tls13_GetHashSizeForHash(hash), keyp);
}

SECStatus
SSLExp_HkdfExpandLabelWithMech(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *prk,
                               const PRUint8 *hsHash, unsigned int hsHashLen,
                               const char *label, unsigned int labelLen,
                               CK_MECHANISM_TYPE mech, unsigned int keySize,
                               PK11SymKey **keyp)
{
    if (prk == NULL || keyp == NULL ||
        label == NULL || labelLen == 0 ||
        mech == CKM_INVALID_MECHANISM || keySize == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    SSLHashType hash;
    SECStatus rv = tls13_GetHashAndCipher(version, cipherSuite,
                                          &hash, NULL);
    if (rv != SECSuccess) {
        return SECFailure; /* Code already set. */
    }
    return tls13_HkdfExpandLabel(prk, hash, hsHash, hsHashLen, label, labelLen,
                                 mech, keySize, keyp);
}

SECStatus
ssl_CreateMaskingContextInner(PRUint16 version, PRUint16 cipherSuite,
                              PK11SymKey *secret,
                              const char *label,
                              unsigned int labelLen,
                              SSLMaskingContext **ctx)
{
    if (!secret || !ctx || (!label && labelLen)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    SSLMaskingContext *out = PORT_ZNew(SSLMaskingContext);
    if (out == NULL) {
        goto loser;
    }

    SSLHashType hash;
    const ssl3BulkCipherDef *cipher;
    SECStatus rv = tls13_GetHashAndCipher(version, cipherSuite,
                                          &hash, &cipher);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser; /* Code already set. */
    }

    out->mech = tls13_SequenceNumberEncryptionMechanism(cipher->calg);
    if (out->mech == CKM_INVALID_MECHANISM) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    // Derive the masking key
    rv = tls13_HkdfExpandLabel(secret, hash,
                               NULL, 0, // Handshake hash.
                               label, labelLen,
                               out->mech,
                               cipher->key_size, &out->secret);
    if (rv != SECSuccess) {
        goto loser;
    }

    out->version = version;
    out->cipherSuite = cipherSuite;

    *ctx = out;
    return SECSuccess;
loser:
    SSLExp_DestroyMaskingContext(out);
    return SECFailure;
}

SECStatus
ssl_CreateMaskInner(SSLMaskingContext *ctx, const PRUint8 *sample,
                    unsigned int sampleLen, PRUint8 *outMask,
                    unsigned int maskLen)
{
    if (!ctx || !sample || !sampleLen || !outMask || !maskLen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (ctx->secret == NULL) {
        PORT_SetError(SEC_ERROR_NO_KEY);
        return SECFailure;
    }

    SECStatus rv = SECFailure;
    unsigned int outMaskLen = 0;

    /* Internal output len/buf, for use if the caller allocated and requested
     * less than one block of output. |oneBlock| should have size equal to the
     * largest block size supported below. */
    PRUint8 oneBlock[AES_BLOCK_SIZE];
    PRUint8 *outMask_ = outMask;
    unsigned int maskLen_ = maskLen;

    switch (ctx->mech) {
        case CKM_AES_ECB:
            if (sampleLen < AES_BLOCK_SIZE) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            if (maskLen_ < AES_BLOCK_SIZE) {
                outMask_ = oneBlock;
                maskLen_ = sizeof(oneBlock);
            }
            rv = PK11_Encrypt(ctx->secret,
                              ctx->mech,
                              NULL,
                              outMask_, &outMaskLen, maskLen_,
                              sample, AES_BLOCK_SIZE);
            if (rv == SECSuccess &&
                maskLen < AES_BLOCK_SIZE) {
                memcpy(outMask, outMask_, maskLen);
            }
            break;
        case CKM_NSS_CHACHA20_CTR:
            if (sampleLen < 16) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }

            SECItem param;
            param.type = siBuffer;
            param.len = 16;
            param.data = (PRUint8 *)sample; // const-cast :(
            unsigned char zeros[128] = { 0 };

            if (maskLen > sizeof(zeros)) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }

            rv = PK11_Encrypt(ctx->secret,
                              ctx->mech,
                              &param,
                              outMask, &outMaskLen,
                              maskLen,
                              zeros, maskLen);
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
    }

    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_PKCS11_FUNCTION_FAILED);
        return SECFailure;
    }

    // Ensure we produced at least as much material as requested.
    if (outMaskLen < maskLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }

    return SECSuccess;
}

SECStatus
ssl_DestroyMaskingContextInner(SSLMaskingContext *ctx)
{
    if (!ctx) {
        return SECSuccess;
    }

    PK11_FreeSymKey(ctx->secret);
    PORT_ZFree(ctx, sizeof(*ctx));
    return SECSuccess;
}

SECStatus
SSLExp_CreateMask(SSLMaskingContext *ctx, const PRUint8 *sample,
                  unsigned int sampleLen, PRUint8 *outMask,
                  unsigned int maskLen)
{
    return ssl_CreateMaskInner(ctx, sample, sampleLen, outMask, maskLen);
}

SECStatus
SSLExp_CreateMaskingContext(PRUint16 version, PRUint16 cipherSuite,
                            PK11SymKey *secret,
                            const char *label,
                            unsigned int labelLen,
                            SSLMaskingContext **ctx)
{
    return ssl_CreateMaskingContextInner(version, cipherSuite, secret, label, labelLen, ctx);
}

SECStatus
SSLExp_DestroyMaskingContext(SSLMaskingContext *ctx)
{
    return ssl_DestroyMaskingContextInner(ctx);
}
