/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "pk11func.h"
#include "secder.h"
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
    /* As of draft-ietf-subcerts-03, only the server may authenticate itself
     * with a DC.
     */
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

    /* Assert that the host is the server (as of draft-ietf-subcerts-03, only
     * the server may authenticate itself with a DC), the certificate has been
     * chosen, TLS 1.3 or higher has been negotiated, and that the set of
     * signature schemes supported by the client is known.
     */
    PORT_Assert(ss->sec.isServer);
    PORT_Assert(ss->sec.serverCert);
    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
    PORT_Assert(ss->xtnData.sigSchemes);

    /* Check that the peer has indicated support and that a DC has been
     * configured for the selected certificate.
     */
    if (!ss->xtnData.peerRequestedDelegCred ||
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
    rv = ssl_PrivateKeySupportsRsaPss(priv, &doesRsaPss);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (!ssl_SignatureSchemeEnabled(ss, scheme) ||
        !ssl_CanUseSignatureScheme(scheme,
                                   ss->xtnData.sigSchemes,
                                   ss->xtnData.numSigSchemes,
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
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
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

    /* Verify the signature of the message. */
    rv = ssl3_VerifySignedHashesWithSpki(
        ss, &cert->subjectPublicKeyInfo, dc->alg, &hash, &dc->signature);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_DC_BAD_SIGNATURE, illegal_parameter);
        goto loser;
    }

    sslBuffer_Clear(&dcBuf);
    return SECSuccess;

loser:
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
        0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xda, 0x4b, 0x2c,
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
    PRTime start, end /* microseconds */;
    CERTCertificate *cert = ss->sec.peerCert;

    rv = DER_DecodeTimeChoice(&start, &cert->validity.notBefore);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        return SECFailure;
    }

    end = start + ((PRTime)dc->validTime * PR_USEC_PER_SEC);
    if (ssl_Time(ss) > end) {
        FATAL_ERROR(ss, SSL_ERROR_DC_EXPIRED, illegal_parameter);
        return SECFailure;
    }

    return SECSuccess;
}

/* Returns SECSucces if |dc| is a DC for the current handshake; otherwise it
 * returns SECFailure. A valid DC meets three requirements: (1) the signature
 * was produced by the peer's end-entity certificate, (2) the end-entity
 * certificate must have the correct key usage, and (3) the DC must not be
 * expired.
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

/* Returns a serialized DC with the given parameters.
 *
 * Note that this function is meant primarily for testing. In particular, it
 * DOES NOT verify any of the following:
 *  - |certPriv| is the private key corresponding to |cert|;
 *  - that |checkCertKeyUsage(cert) == SECSuccess|;
 *  - |dcCertVerifyAlg| is consistent with |dcPub|;
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
    SECItem *tmp = NULL;
    SECKEYPrivateKey *tmpPriv = NULL;
    sslDelegatedCredential *dc = NULL;
    sslBuffer dcBuf = SSL_BUFFER_EMPTY;

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
    dc->expectedCertVerifyAlg = dcCertVerifyAlg;

    tmp = SECKEY_EncodeDERSubjectPublicKeyInfo(dcPub);
    if (!tmp) {
        goto loser;
    }
    /* Transfer |tmp| into |dc->derSpki|. */
    dc->derSpki.type = tmp->type;
    dc->derSpki.data = tmp->data;
    dc->derSpki.len = tmp->len;
    PORT_Free(tmp);

    rv = ssl_SignatureSchemeFromSpki(&cert->subjectPublicKeyInfo,
                                     PR_TRUE /* isTls13 */, &dc->alg);
    if (rv != SECSuccess) {
        goto loser;
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

    SECKEY_DestroyPrivateKey(tmpPriv);
    tls13_DestroyDelegatedCredential(dc);
    sslBuffer_Clear(&dcBuf);
    return SECSuccess;

loser:
    SECKEY_DestroyPrivateKey(tmpPriv);
    tls13_DestroyDelegatedCredential(dc);
    sslBuffer_Clear(&dcBuf);
    return SECFailure;
}
