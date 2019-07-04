/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13subcerts_h_
#define __tls13subcerts_h_

struct sslDelegatedCredentialStr {
    /* The number of seconds for which the delegated credential (DC) is valid
     * following the notBefore parameter of the delegation certificate.
     */
    PRUint32 validTime;

    /* The signature algorithm of the DC public key. This expected to the same
     * as CertificateVerify.scheme.
     */
    SSLSignatureScheme expectedCertVerifyAlg;

    /* The DER-encoded SubjectPublicKeyInfo, the DC public key.
     */
    SECItem derSpki;

    /* The decoded SubjectPublicKeyInfo parsed from |derSpki|. */
    CERTSubjectPublicKeyInfo *spki;

    /* The signature algorithm used to verify the DC signature. */
    SSLSignatureScheme alg;

    /* The DC signature. */
    SECItem signature;
};

SECStatus tls13_ReadDelegatedCredential(PRUint8 *b,
                                        PRUint32 length,
                                        sslDelegatedCredential **dcp);
void tls13_DestroyDelegatedCredential(sslDelegatedCredential *dc);

PRBool tls13_IsVerifyingWithDelegatedCredential(const sslSocket *ss);
PRBool tls13_IsSigningWithDelegatedCredential(const sslSocket *ss);
SECStatus tls13_MaybeSetDelegatedCredential(sslSocket *ss);
SECStatus tls13_VerifyDelegatedCredential(sslSocket *ss,
                                          sslDelegatedCredential *dc);

SECStatus SSLExp_DelegateCredential(const CERTCertificate *cert,
                                    const SECKEYPrivateKey *certPriv,
                                    const SECKEYPublicKey *dcPub,
                                    SSLSignatureScheme dcCertVerifyAlg,
                                    PRUint32 dcValidFor,
                                    PRTime now,
                                    SECItem *out);

#endif
