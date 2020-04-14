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
    /* sigh, the API creates a single context, but then uses either encrypt
     * and decrypt on that context. We should take an encrypt/decrypt
     * variable here, but for now create two contexts. */
    PK11Context *encryptContext;
    PK11Context *decryptContext;
    int tagLen;
    int ivLen;
    unsigned char iv[MAX_IV_LENGTH];
};

SECStatus
SSLExp_MakeVariantAead(PRUint16 version, PRUint16 cipherSuite, SSLProtocolVariant variant,
                       PK11SymKey *secret, const char *labelPrefix,
                       unsigned int labelPrefixLen, SSLAeadContext **ctx)
{
    SSLAeadContext *out = NULL;
    char label[255]; // Maximum length label.
    static const char *const keySuffix = "key";
    static const char *const ivSuffix = "iv";
    CK_MECHANISM_TYPE mech;
    SECItem nullParams = { siBuffer, NULL, 0 };
    PK11SymKey *key = NULL;

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
    mech = ssl3_Alg2Mech(cipher->calg);
    out->ivLen = cipher->iv_size + cipher->explicit_nonce_size;
    out->tagLen = cipher->tag_size;

    memcpy(label, labelPrefix, labelPrefixLen);
    memcpy(label + labelPrefixLen, ivSuffix, strlen(ivSuffix));
    unsigned int labelLen = labelPrefixLen + strlen(ivSuffix);
    unsigned int ivLen = cipher->iv_size + cipher->explicit_nonce_size;
    rv = tls13_HkdfExpandLabelRaw(secret, hash,
                                  NULL, 0, // Handshake hash.
                                  label, labelLen, variant,
                                  out->iv, ivLen);
    if (rv != SECSuccess) {
        goto loser;
    }

    memcpy(label + labelPrefixLen, keySuffix, strlen(keySuffix));
    labelLen = labelPrefixLen + strlen(keySuffix);
    rv = tls13_HkdfExpandLabel(secret, hash,
                               NULL, 0, // Handshake hash.
                               label, labelLen, mech, cipher->key_size,
                               variant, &key);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* We really need to change the API to Create a context for each
     * encrypt and decrypt rather than a single call that does both. it's
     * almost certain that the underlying application tries to use the same
     * context for both. */
    out->encryptContext = PK11_CreateContextBySymKey(mech,
                                                     CKA_NSS_MESSAGE | CKA_ENCRYPT,
                                                     key, &nullParams);
    if (out->encryptContext == NULL) {
        goto loser;
    }

    out->decryptContext = PK11_CreateContextBySymKey(mech,
                                                     CKA_NSS_MESSAGE | CKA_DECRYPT,
                                                     key, &nullParams);
    if (out->decryptContext == NULL) {
        goto loser;
    }

    PK11_FreeSymKey(key);
    *ctx = out;
    return SECSuccess;

loser:
    PK11_FreeSymKey(key);
    SSLExp_DestroyAead(out);
    return SECFailure;
}

SECStatus
SSLExp_MakeAead(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *secret,
                const char *labelPrefix, unsigned int labelPrefixLen, SSLAeadContext **ctx)
{
    return SSLExp_MakeVariantAead(version, cipherSuite, ssl_variant_stream, secret,
                                  labelPrefix, labelPrefixLen, ctx);
}

SECStatus
SSLExp_DestroyAead(SSLAeadContext *ctx)
{
    if (!ctx) {
        return SECSuccess;
    }
    if (ctx->encryptContext) {
        PK11_DestroyContext(ctx->encryptContext, PR_TRUE);
    }
    if (ctx->decryptContext) {
        PK11_DestroyContext(ctx->decryptContext, PR_TRUE);
    }

    PORT_ZFree(ctx, sizeof(*ctx));
    return SECSuccess;
}

/* Bug 1529440 exists to refactor this and the other AEAD uses. */
static SECStatus
ssl_AeadInner(const SSLAeadContext *ctx, PK11Context *context,
              PRBool decrypt, PRUint64 counter,
              const PRUint8 *aad, unsigned int aadLen,
              const PRUint8 *in, unsigned int inLen,
              PRUint8 *out, unsigned int *outLen, unsigned int maxOut)
{
    if (ctx == NULL || (aad == NULL && aadLen > 0) || in == NULL ||
        out == NULL || outLen == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    // Setup the nonce.
    PRUint8 nonce[sizeof(counter)] = { 0 };
    sslBuffer nonceBuf = SSL_BUFFER_FIXED(nonce, sizeof(counter));
    SECStatus rv = sslBuffer_AppendNumber(&nonceBuf, counter, sizeof(counter));
    if (rv != SECSuccess) {
        PORT_Assert(0);
        return SECFailure;
    }
    /* at least on encrypt, we should not be using CKG_NO_GENERATE, but
     * the current experimental API has the application tracking the counter
     * rather than token. We should look at the QUIC code and see if the
     * counter can be moved internally where it belongs. That would
     * also get rid of the  formatting code above and have the API
     * call tls13_AEAD directly in SSLExp_Aead* */
    return tls13_AEAD(context, decrypt, CKG_NO_GENERATE, 0, ctx->iv, NULL,
                      ctx->ivLen, nonce, sizeof(counter), aad, aadLen,
                      out, outLen, maxOut, ctx->tagLen, in, inLen);
}

SECStatus
SSLExp_AeadEncrypt(const SSLAeadContext *ctx, PRUint64 counter,
                   const PRUint8 *aad, unsigned int aadLen,
                   const PRUint8 *plaintext, unsigned int plaintextLen,
                   PRUint8 *out, unsigned int *outLen, unsigned int maxOut)
{
    // false == encrypt
    return ssl_AeadInner(ctx, ctx->encryptContext, PR_FALSE, counter,
                         aad, aadLen, plaintext, plaintextLen,
                         out, outLen, maxOut);
}

SECStatus
SSLExp_AeadDecrypt(const SSLAeadContext *ctx, PRUint64 counter,
                   const PRUint8 *aad, unsigned int aadLen,
                   const PRUint8 *ciphertext, unsigned int ciphertextLen,
                   PRUint8 *out, unsigned int *outLen, unsigned int maxOut)
{
    // true == decrypt
    return ssl_AeadInner(ctx, ctx->decryptContext, PR_TRUE, counter,
                         aad, aadLen, ciphertext, ciphertextLen,
                         out, outLen, maxOut);
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
                       const char *label, unsigned int labelLen, PK11SymKey **keyp)
{
    return SSLExp_HkdfVariantExpandLabel(version, cipherSuite, prk, hsHash, hsHashLen,
                                         label, labelLen, ssl_variant_stream, keyp);
}

SECStatus
SSLExp_HkdfVariantExpandLabel(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *prk,
                              const PRUint8 *hsHash, unsigned int hsHashLen,
                              const char *label, unsigned int labelLen,
                              SSLProtocolVariant variant, PK11SymKey **keyp)
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
                                 CKM_HKDF_DERIVE,
                                 tls13_GetHashSizeForHash(hash), variant, keyp);
}

SECStatus
SSLExp_HkdfExpandLabelWithMech(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *prk,
                               const PRUint8 *hsHash, unsigned int hsHashLen,
                               const char *label, unsigned int labelLen,
                               CK_MECHANISM_TYPE mech, unsigned int keySize,
                               PK11SymKey **keyp)
{
    return SSLExp_HkdfVariantExpandLabelWithMech(version, cipherSuite, prk, hsHash, hsHashLen,
                                                 label, labelLen, mech, keySize,
                                                 ssl_variant_stream, keyp);
}

SECStatus
SSLExp_HkdfVariantExpandLabelWithMech(PRUint16 version, PRUint16 cipherSuite, PK11SymKey *prk,
                                      const PRUint8 *hsHash, unsigned int hsHashLen,
                                      const char *label, unsigned int labelLen,
                                      CK_MECHANISM_TYPE mech, unsigned int keySize,
                                      SSLProtocolVariant variant, PK11SymKey **keyp)
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
                                 mech, keySize, variant, keyp);
}

SECStatus
ssl_CreateMaskingContextInner(PRUint16 version, PRUint16 cipherSuite,
                              SSLProtocolVariant variant,
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
                               cipher->key_size, variant,
                               &out->secret);
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
    int paramLen = 0;

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
            paramLen = 16;
        /* fall through */
        case CKM_CHACHA20:
            paramLen = (paramLen) ? paramLen : sizeof(CK_CHACHA20_PARAMS);
            if (sampleLen < paramLen) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }

            SECItem param;
            param.type = siBuffer;
            param.len = paramLen;
            param.data = (PRUint8 *)sample; // const-cast :(
            unsigned char zeros[128] = { 0 };

            if (maskLen > sizeof(zeros)) {
                PORT_SetError(SEC_ERROR_OUTPUT_LEN);
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
    return ssl_CreateMaskingContextInner(version, cipherSuite, ssl_variant_stream, secret,
                                         label, labelLen, ctx);
}

SECStatus
SSLExp_CreateVariantMaskingContext(PRUint16 version, PRUint16 cipherSuite,
                                   SSLProtocolVariant variant,
                                   PK11SymKey *secret,
                                   const char *label,
                                   unsigned int labelLen,
                                   SSLMaskingContext **ctx)
{
    return ssl_CreateMaskingContextInner(version, cipherSuite, variant, secret,
                                         label, labelLen, ctx);
}

SECStatus
SSLExp_DestroyMaskingContext(SSLMaskingContext *ctx)
{
    return ssl_DestroyMaskingContextInner(ctx);
}
