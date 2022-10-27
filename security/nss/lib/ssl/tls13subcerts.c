/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "pk11func.h"
#include "secder.h"
#include "sechash.h"
#include "ssl.h"
#include "sslproto.h"
#include "sslimpl.h"
#include "ssl3exthandle.h"
#include "tls13exthandle.h"
#include "tls13hkdf.h"
#include "tls13subcerts.h"

/* Parses the delegated credential (DC) from the raw extension |b| of length
 * |length|. Memory for the DC is allocated and set to |*dcp|.
 *
 * It's the caller's responsibility to invoke |tls13_DestroyDelegatedCredential|
 * when this data is no longer needed.
 */
SECStatus
tls13_ReadDelegatedCredential(PRUint8 *b, PRUint32 length,
                              sslDelegatedCredential **dcp)
{
    sslDelegatedCredential *dc = NULL;
    SECStatus rv;
    PRUint64 n;
    sslReadBuffer tmp;
    sslReader rdr = SSL_READER(b, length);

    PORT_Assert(!*dcp);

    dc = PORT_ZNew(sslDelegatedCredential);
    if (!dc) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* Read the valid_time field of DelegatedCredential.cred. */
    rv = sslRead_ReadNumber(&rdr, 4, &n);
    if (rv != SECSuccess) {
        goto loser;
    }
    dc->validTime = n;

    /* Read the expected_cert_verify_algorithm field of
     * DelegatedCredential.cred. */
    rv = sslRead_ReadNumber(&rdr, 2, &n);
    if (rv != SECSuccess) {
        goto loser;
    }
    dc->expectedCertVerifyAlg = n;

    /* Read the ASN1_subjectPublicKeyInfo field of DelegatedCredential.cred. */
    rv = sslRead_ReadVariable(&rdr, 3, &tmp);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECITEM_MakeItem(NULL, &dc->derSpki, tmp.buf, tmp.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Parse the DER-encoded SubjectPublicKeyInfo. */
    dc->spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&dc->derSpki);
    if (!dc->spki) {
        goto loser;
    }

    /* Read the algorithm field of the DelegatedCredential. */
    rv = sslRead_ReadNumber(&rdr, 2, &n);
    if (rv != SECSuccess) {
        goto loser;
    }
    dc->alg = n;

    /* Read the signature field of the DelegatedCredential. */
    rv = sslRead_ReadVariable(&rdr, 2, &tmp);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECITEM_MakeItem(NULL, &dc->signature, tmp.buf, tmp.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* There should be nothing left to read. */
    if (SSL_READER_REMAINING(&rdr) > 0) {
        goto loser;
    }

    *dcp = dc;
    return SECSuccess;

loser:
    tls13_DestroyDelegatedCredential(dc);
    *dcp = NULL;
    return SECFailure;
}

/* Frees |dc| from the heap. */
void
tls13_DestroyDelegatedCredential(sslDelegatedCredential *dc)
{
    if (!dc) {
        return;
    }

    SECKEY_DestroySubjectPublicKeyInfo(dc->spki);
    SECITEM_FreeItem(&dc->derSpki, PR_FALSE);
    SECITEM_FreeItem(&dc->signature, PR_FALSE);
    PORT_ZFree(dc, sizeof(sslDelegatedCredential));
}

/* Sets |*certVerifyAlg| to the expected_cert_verify_algorithm field from the
 * serialized DC |in|. Returns SECSuccess upon success; SECFailure indicates a
 * decoding failure or the input wasn't long enough.
 */
static SECStatus
tls13_GetExpectedCertVerifyAlg(SECItem in, SSLSignatureScheme *certVerifyAlg)
{
    SECStatus rv;
    PRUint64 n;
    sslReader rdr = SSL_READER(in.data, in.len);

    if (in.len < 6) { /* Buffer too short to contain the first two params. */
        return SECFailure;
    }

    rv = sslRead_ReadNumber(&rdr, 4, &n);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = sslRead_ReadNumber(&rdr, 2, &n);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    *certVerifyAlg = n;

    return SECSuccess;
}

/* Returns PR_TRUE if the host is verifying the handshake with a DC. */
PRBool
tls13_IsVerifyingWithDelegatedCredential(const sslSocket *ss)
{
    /* We currently do not support client-delegated credentials. */
    if (ss->sec.isServer ||
        !ss->opt.enableDelegatedCredentials ||
        !ss->xtnData.peerDelegCred) {
        return PR_FALSE;
    }

    return PR_TRUE;
}

/* Returns PR_TRUE if the host is signing the handshake with a DC. */
PRBool
tls13_IsSigningWithDelegatedCredential(const sslSocket *ss)
{
    if (!ss->sec.isServer ||
        !ss->xtnData.sendingDelegCredToPeer ||
        !ss->xtnData.peerRequestedDelegCred) {
        return PR_FALSE;
    }

    return PR_TRUE;
}

/* Commits to authenticating with a DC if all of the following conditions hold:
 *  - the negotiated protocol is TLS 1.3 or newer;
 *  - the selected certificate has a DC configured;
 *  - the peer has indicated support for this extension;
 *  - the peer has indicated support for the DC signature scheme; and
 *  - the host supports the DC signature scheme.
 *
 * It's the caller's responsibility to ensure that the version has been
 * negotiated and the certificate has been selected.
 */
SECStatus
tls13_MaybeSetDelegatedCredential(sslSocket *ss)
{
    SECStatus rv;
    PRBool doesRsaPss;
    SECKEYPrivateKey *priv;
    SSLSignatureScheme scheme;

    /* Assert that the host is the server (we do not currently support
     * client-delegated credentials), the certificate has been
     * chosen, TLS 1.3 or higher has been negotiated, and that the set of
     * signature schemes supported by the client is known.
     */
    PORT_Assert(ss->sec.isServer);
    PORT_Assert(ss->sec.serverCert);
    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
    PORT_Assert(ss->xtnData.peerRequestedDelegCred == !!ss->xtnData.delegCredSigSchemes);

    /* Check that the peer has indicated support and that a DC has been
     * configured for the selected certificate.
     */
    if (!ss->xtnData.peerRequestedDelegCred ||
        !ss->xtnData.delegCredSigSchemes ||
        !ss->sec.serverCert->delegCred.len ||
        !ss->sec.serverCert->delegCredKeyPair) {
        return SECSuccess;
    }

    /* Check that the host and peer both support the signing algorithm used with
     * the DC.
     */
    rv = tls13_GetExpectedCertVerifyAlg(ss->sec.serverCert->delegCred,
                                        &scheme);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    priv = ss->sec.serverCert->delegCredKeyPair->privKey;
    rv = ssl_PrivateKeySupportsRsaPss(priv, NULL, NULL, &doesRsaPss);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (!ssl_SignatureSchemeEnabled(ss, scheme) ||
        !ssl_CanUseSignatureScheme(scheme,
                                   ss->xtnData.delegCredSigSchemes,
                                   ss->xtnData.numDelegCredSigSchemes,
                                   PR_FALSE /* requireSha1 */,
                                   doesRsaPss)) {
        return SECSuccess;
    }

    /* Commit to sending a DC and set the handshake signature scheme to the
     * indicated algorithm.
     */
    ss->xtnData.sendingDelegCredToPeer = PR_TRUE;
    ss->ssl3.hs.signatureScheme = scheme;
    return SECSuccess;
}

/* Serializes the DC up to the signature. */
static SECStatus
tls13_AppendCredentialParams(sslBuffer *buf, sslDelegatedCredential *dc)
{
    SECStatus rv;
    rv = sslBuffer_AppendNumber(buf, dc->validTime, 4);
    if (rv != SECSuccess) {
        return SECFailure; /* Error set by caller. */
    }

    rv = sslBuffer_AppendNumber(buf, dc->expectedCertVerifyAlg, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = sslBuffer_AppendVariable(buf, dc->derSpki.data, dc->derSpki.len, 3);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = sslBuffer_AppendNumber(buf, dc->alg, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

/* Serializes the DC signature. */
static SECStatus
tls13_AppendCredentialSignature(sslBuffer *buf, sslDelegatedCredential *dc)
{
    SECStatus rv;
    rv = sslBuffer_AppendVariable(buf, dc->signature.data,
                                  dc->signature.len, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

/* Hashes the message used to sign/verify the DC. */
static SECStatus
tls13_HashCredentialSignatureMessage(SSL3Hashes *hash,
                                     SSLSignatureScheme scheme,
                                     const CERTCertificate *cert,
                                     const sslBuffer *dcBuf)
{
    SECStatus rv;
    PK11Context *ctx = NULL;
    unsigned int hashLen;

    /* Set up hash context. */
    hash->hashAlg = ssl_SignatureSchemeToHashType(scheme);
    ctx = PK11_CreateDigestContext(ssl3_HashTypeToOID(hash->hashAlg));
    if (!ctx) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    static const PRUint8 kCtxStrPadding[64] = {
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
    };

    static const PRUint8 kCtxStr[] = "TLS, server delegated credentials";

    /* Hash the message signed by the peer. */
    rv = SECSuccess;
    rv |= PK11_DigestBegin(ctx);
    rv |= PK11_DigestOp(ctx, kCtxStrPadding, sizeof kCtxStrPadding);
    rv |= PK11_DigestOp(ctx, kCtxStr, 1 /* 0-byte */ + strlen((const char *)kCtxStr));
    rv |= PK11_DigestOp(ctx, cert->derCert.data, cert->derCert.len);
    rv |= PK11_DigestOp(ctx, dcBuf->buf, dcBuf->len);
    rv |= PK11_DigestFinal(ctx, hash->u.raw, &hashLen, sizeof hash->u.raw);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_SHA_DIGEST_FAILURE);
        goto loser;
    }

    hash->len = hashLen;
    if (ctx) {
        PK11_DestroyContext(ctx, PR_TRUE);
    }
    return SECSuccess;

loser:
    if (ctx) {
        PK11_DestroyContext(ctx, PR_TRUE);
    }
    return SECFailure;
}

/* Verifies the DC signature. */
static SECStatus
tls13_VerifyCredentialSignature(sslSocket *ss, sslDelegatedCredential *dc)
{
    SECStatus rv = SECSuccess;
    SSL3Hashes hash;
    sslBuffer dcBuf = SSL_BUFFER_EMPTY;
    CERTCertificate *cert = ss->sec.peerCert;
    SECKEYPublicKey *pubKey = NULL;

    /* Serialize the DC parameters. */
    rv = tls13_AppendCredentialParams(&dcBuf, dc);
    if (rv != SECSuccess) {
        goto loser; /* Error set by caller. */
    }

    /* Hash the message that was signed by the delegator. */
    rv = tls13_HashCredentialSignatureMessage(&hash, dc->alg, cert, &dcBuf);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        goto loser;
    }

    pubKey = SECKEY_ExtractPublicKey(&cert->subjectPublicKeyInfo);
    if (pubKey == NULL) {
        FATAL_ERROR(ss, SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE, internal_error);
        goto loser;
    }

    /* Verify the signature of the message. */
    rv = ssl_VerifySignedHashesWithPubKey(ss, pubKey, dc->alg,
                                          &hash, &dc->signature);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_DC_BAD_SIGNATURE, illegal_parameter);
        goto loser;
    }

    SECOidTag spkiAlg = SECOID_GetAlgorithmTag(&(dc->spki->algorithm));
    if (spkiAlg == SEC_OID_PKCS1_RSA_ENCRYPTION) {
        FATAL_ERROR(ss, SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM, illegal_parameter);
        goto loser;
    }

    SECKEY_DestroyPublicKey(pubKey);
    sslBuffer_Clear(&dcBuf);
    return SECSuccess;

loser:
    SECKEY_DestroyPublicKey(pubKey);
    sslBuffer_Clear(&dcBuf);
    return SECFailure;
}

/* Checks that the peer's end-entity certificate has the correct key usage. */
static SECStatus
tls13_CheckCertDelegationUsage(sslSocket *ss)
{
    int i;
    PRBool found;
    CERTCertExtension *ext;
    SECItem delegUsageOid = { siBuffer, NULL, 0 };
    const CERTCertificate *cert = ss->sec.peerCert;

    /* 1.3.6.1.4.1.44363.44, as defined in draft-ietf-tls-subcerts. */
    static unsigned char kDelegationUsageOid[] = {
        0x2b,
        0x06,
        0x01,
        0x04,
        0x01,
        0x82,
        0xda,
        0x4b,
        0x2c
    };

    delegUsageOid.data = kDelegationUsageOid;
    delegUsageOid.len = sizeof kDelegationUsageOid;

    /* The certificate must have the delegationUsage extension that authorizes
     * it to negotiate delegated credentials.
     */
    found = PR_FALSE;
    for (i = 0; cert->extensions[i] != NULL; i++) {
        ext = cert->extensions[i];
        if (SECITEM_CompareItem(&ext->id, &delegUsageOid) == SECEqual) {
            found = PR_TRUE;
            break;
        }
    }

    /* The certificate must also have the digitalSignature keyUsage set. */
    if (!found ||
        !cert->keyUsagePresent ||
        !(cert->keyUsage & KU_DIGITAL_SIGNATURE)) {
        FATAL_ERROR(ss, SSL_ERROR_DC_INVALID_KEY_USAGE, illegal_parameter);
        return SECFailure;
    }

    return SECSuccess;
}

static SECStatus
tls13_CheckCredentialExpiration(sslSocket *ss, sslDelegatedCredential *dc)
{
    SECStatus rv;
    CERTCertificate *cert = ss->sec.peerCert;
    /* 7 days in microseconds */
    static const PRTime kMaxDcValidity = ((PRTime)7 * 24 * 60 * 60 * PR_USEC_PER_SEC);
    PRTime start, now, end; /* microseconds */

    rv = DER_DecodeTimeChoice(&start, &cert->validity.notBefore);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        return SECFailure;
    }

    end = start + ((PRTime)dc->validTime * PR_USEC_PER_SEC);
    now = ssl_Time(ss);
    if (now > end || end < 0) {
        FATAL_ERROR(ss, SSL_ERROR_DC_EXPIRED, illegal_parameter);
        return SECFailure;
    }

    /* Not more than 7 days remaining in the validity period. */
    if (end - now > kMaxDcValidity) {
        FATAL_ERROR(ss, SSL_ERROR_DC_INAPPROPRIATE_VALIDITY_PERIOD, illegal_parameter);
        return SECFailure;
    }

    return SECSuccess;
}

/* Returns SECSucces if |dc| is a DC for the current handshake; otherwise it
 * returns SECFailure. A valid DC meets three requirements: (1) the signature
 * was produced by the peer's end-entity certificate, (2) the end-entity
 * certificate must have the correct key usage, and (3) the DC must not be
 * expired and its remaining TTL must be <= the maximum validity period (fixed
 * as 7 days).
 *
 * This function calls FATAL_ERROR() when an error occurs.
 */
SECStatus
tls13_VerifyDelegatedCredential(sslSocket *ss,
                                sslDelegatedCredential *dc)
{
    SECStatus rv;
    PRTime start;
    PRExplodedTime end;
    CERTCertificate *cert = ss->sec.peerCert;
    char endStr[256];

    rv = DER_DecodeTimeChoice(&start, &cert->validity.notBefore);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        return SECFailure;
    }

    PR_ExplodeTime(start + (dc->validTime * PR_USEC_PER_SEC),
                   PR_GMTParameters, &end);
    if (PR_FormatTime(endStr, sizeof(endStr), "%a %b %d %H:%M:%S %Y", &end)) {
        SSL_TRC(20, ("%d: TLS13[%d]: Received delegated credential (expires %s)",
                     SSL_GETPID(), ss->fd, endStr));
    } else {
        SSL_TRC(20, ("%d: TLS13[%d]: Received delegated credential",
                     SSL_GETPID(), ss->fd));
    }

    rv = SECSuccess;
    rv |= tls13_VerifyCredentialSignature(ss, dc);
    rv |= tls13_CheckCertDelegationUsage(ss);
    rv |= tls13_CheckCredentialExpiration(ss, dc);
    return rv;
}

static CERTSubjectPublicKeyInfo *
tls13_MakePssSpki(const SECKEYPublicKey *pub, SECOidTag hashOid)
{
    SECStatus rv;
    PLArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        goto loser; /* Code already set. */
    }
    CERTSubjectPublicKeyInfo *spki = PORT_ArenaZNew(arena, CERTSubjectPublicKeyInfo);
    if (!spki) {
        goto loser; /* Code already set. */
    }
    spki->arena = arena;

    SECKEYRSAPSSParams params = { 0 };
    params.hashAlg = PORT_ArenaZNew(arena, SECAlgorithmID);
    rv = SECOID_SetAlgorithmID(arena, params.hashAlg, hashOid, NULL);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }

    /* Set the mask hash algorithm too, which is an argument to
     * a SEC_OID_PKCS1_MGF1 value. */
    SECAlgorithmID maskHashAlg;
    memset(&maskHashAlg, 0, sizeof(maskHashAlg));
    rv = SECOID_SetAlgorithmID(arena, &maskHashAlg, hashOid, NULL);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }
    SECItem *maskHashAlgItem =
        SEC_ASN1EncodeItem(arena, NULL, &maskHashAlg,
                           SEC_ASN1_GET(SECOID_AlgorithmIDTemplate));
    if (!maskHashAlgItem) {
        /* Probably OOM, but not certain. */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser;
    }

    params.maskAlg = PORT_ArenaZNew(arena, SECAlgorithmID);
    rv = SECOID_SetAlgorithmID(arena, params.maskAlg, SEC_OID_PKCS1_MGF1,
                               maskHashAlgItem);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }

    /* Always include saltLength: all hashes are larger than 20. */
    unsigned int saltLength = HASH_ResultLenByOidTag(hashOid);
    PORT_Assert(saltLength > 20);
    if (!SEC_ASN1EncodeInteger(arena, &params.saltLength, saltLength)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser;
    }
    /* Omit the trailerField always. */

    SECItem *algorithmItem =
        SEC_ASN1EncodeItem(arena, NULL, &params,
                           SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
    if (!algorithmItem) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser; /* Code already set. */
    }
    rv = SECOID_SetAlgorithmID(arena, &spki->algorithm,
                               SEC_OID_PKCS1_RSA_PSS_SIGNATURE, algorithmItem);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }

    SECItem *pubItem = SEC_ASN1EncodeItem(arena, &spki->subjectPublicKey, pub,
                                          SEC_ASN1_GET(SECKEY_RSAPublicKeyTemplate));
    if (!pubItem) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser;
    }
    spki->subjectPublicKey.len *= 8; /* Key length is in bits. */
    return spki;

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

static CERTSubjectPublicKeyInfo *
tls13_MakeDcSpki(const SECKEYPublicKey *dcPub, SSLSignatureScheme dcCertVerifyAlg)
{
    switch (SECKEY_GetPublicKeyType(dcPub)) {
        case rsaKey: {
            SECOidTag hashOid;
            switch (dcCertVerifyAlg) {
                /* Note: RSAE schemes are NOT permitted within DC SPKIs. However,
                 * support for their issuance remains so as to enable negative
                 * testing of client behavior. */
                case ssl_sig_rsa_pss_rsae_sha256:
                case ssl_sig_rsa_pss_rsae_sha384:
                case ssl_sig_rsa_pss_rsae_sha512:
                    return SECKEY_CreateSubjectPublicKeyInfo(dcPub);
                case ssl_sig_rsa_pss_pss_sha256:
                    hashOid = SEC_OID_SHA256;
                    break;
                case ssl_sig_rsa_pss_pss_sha384:
                    hashOid = SEC_OID_SHA384;
                    break;
                case ssl_sig_rsa_pss_pss_sha512:
                    hashOid = SEC_OID_SHA512;
                    break;

                default:
                    PORT_SetError(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM);
                    return NULL;
            }
            return tls13_MakePssSpki(dcPub, hashOid);
        }

        case ecKey: {
            const sslNamedGroupDef *group = ssl_ECPubKey2NamedGroup(dcPub);
            if (!group) {
                PORT_SetError(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM);
                return NULL;
            }
            SSLSignatureScheme keyScheme;
            switch (group->name) {
                case ssl_grp_ec_secp256r1:
                    keyScheme = ssl_sig_ecdsa_secp256r1_sha256;
                    break;
                case ssl_grp_ec_secp384r1:
                    keyScheme = ssl_sig_ecdsa_secp384r1_sha384;
                    break;
                case ssl_grp_ec_secp521r1:
                    keyScheme = ssl_sig_ecdsa_secp521r1_sha512;
                    break;
                default:
                    PORT_SetError(SEC_ERROR_INVALID_KEY);
                    return NULL;
            }
            if (keyScheme != dcCertVerifyAlg) {
                PORT_SetError(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM);
                return NULL;
            }
            return SECKEY_CreateSubjectPublicKeyInfo(dcPub);
        }

        default:
            break;
    }

    PORT_SetError(SEC_ERROR_INVALID_KEY);
    return NULL;
}

/* Returns a serialized DC with the given parameters.
 *
 * Note that this function is meant primarily for testing. In particular, it
 * DOES NOT verify any of the following:
 *  - |certPriv| is the private key corresponding to |cert|;
 *  - that |checkCertKeyUsage(cert) == SECSuccess|;
 *  - |dcValidFor| is less than 7 days (the maximum permitted by the spec); or
 *  - validTime doesn't overflow a PRUint32.
 *
 * These conditions are things we want to test for, which is why we allow them
 * here. A real API for creating DCs would want to explicitly check ALL of these
 * conditions are met.
 */
SECStatus
SSLExp_DelegateCredential(const CERTCertificate *cert,
                          const SECKEYPrivateKey *certPriv,
                          const SECKEYPublicKey *dcPub,
                          SSLSignatureScheme dcCertVerifyAlg,
                          PRUint32 dcValidFor,
                          PRTime now,
                          SECItem *out)
{
    SECStatus rv;
    SSL3Hashes hash;
    CERTSubjectPublicKeyInfo *spki = NULL;
    SECKEYPrivateKey *tmpPriv = NULL;
    sslDelegatedCredential *dc = NULL;
    sslBuffer dcBuf = SSL_BUFFER_EMPTY;

    if (!cert || !certPriv || !dcPub || !out) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    dc = PORT_ZNew(sslDelegatedCredential);
    if (!dc) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    /* Serialize the DC parameters. */
    PRTime start;
    rv = DER_DecodeTimeChoice(&start, &cert->validity.notBefore);
    if (rv != SECSuccess) {
        goto loser;
    }
    dc->validTime = ((now - start) / PR_USEC_PER_SEC) + dcValidFor;

    /* Building the SPKI also validates |dcCertVerifyAlg|. */
    spki = tls13_MakeDcSpki(dcPub, dcCertVerifyAlg);
    if (!spki) {
        goto loser;
    }
    dc->expectedCertVerifyAlg = dcCertVerifyAlg;

    SECItem *spkiDer =
        SEC_ASN1EncodeItem(NULL /*arena*/, &dc->derSpki, spki,
                           SEC_ASN1_GET(CERT_SubjectPublicKeyInfoTemplate));
    if (!spkiDer) {
        goto loser;
    }

    rv = ssl_SignatureSchemeFromSpki(&cert->subjectPublicKeyInfo,
                                     PR_TRUE /* isTls13 */, &dc->alg);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (dc->alg == ssl_sig_none) {
        SECOidTag spkiOid = SECOID_GetAlgorithmTag(&cert->subjectPublicKeyInfo.algorithm);
        /* If the Cert SPKI contained an AlgorithmIdentifier of "rsaEncryption", set a
         * default rsa_pss_rsae_sha256 scheme. NOTE: RSAE SPKIs are not permitted within
         * "real" Delegated Credentials. However, since this function is primarily used for
         * testing, we retain this support in order to verify that these DCs are rejected
         * by tls13_VerifyDelegatedCredential. */
        if (spkiOid == SEC_OID_PKCS1_RSA_ENCRYPTION) {
            SSLSignatureScheme scheme = ssl_sig_rsa_pss_rsae_sha256;
            if (ssl_SignatureSchemeValid(scheme, spkiOid, PR_TRUE /* isTls13 */)) {
                dc->alg = scheme;
            }
        }
    }
    PORT_Assert(dc->alg != ssl_sig_none);

    rv = tls13_AppendCredentialParams(&dcBuf, dc);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Hash signature message. */
    rv = tls13_HashCredentialSignatureMessage(&hash, dc->alg, cert, &dcBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Sign the hash with the delegation key.
     *
     * The PK11 API discards const qualifiers, so we have to make a copy of
     * |certPriv| and pass the copy to |ssl3_SignHashesWithPrivKey|.
     */
    tmpPriv = SECKEY_CopyPrivateKey(certPriv);
    rv = ssl3_SignHashesWithPrivKey(&hash, tmpPriv, dc->alg,
                                    PR_TRUE /* isTls */, &dc->signature);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Serialize the DC signature. */
    rv = tls13_AppendCredentialSignature(&dcBuf, dc);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Copy the serialized DC to |out|. */
    rv = SECITEM_MakeItem(NULL, out, dcBuf.buf, dcBuf.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    PRINT_BUF(20, (NULL, "delegated credential", dcBuf.buf, dcBuf.len));

    SECKEY_DestroySubjectPublicKeyInfo(spki);
    SECKEY_DestroyPrivateKey(tmpPriv);
    tls13_DestroyDelegatedCredential(dc);
    sslBuffer_Clear(&dcBuf);
    return SECSuccess;

loser:
    SECKEY_DestroySubjectPublicKeyInfo(spki);
    SECKEY_DestroyPrivateKey(tmpPriv);
    tls13_DestroyDelegatedCredential(dc);
    sslBuffer_Clear(&dcBuf);
    return SECFailure;
}
