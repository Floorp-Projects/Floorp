/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __sslcert_h_
#define __sslcert_h_

#include "cert.h"
#include "secitem.h"
#include "keyhi.h"

/* This type is a bitvector that is indexed by SSLAuthType values.  Note that
 * the bit for ssl_auth_null(0) - the least significant bit - isn't used. */
typedef PRUint16 sslAuthTypeMask;
PR_STATIC_ASSERT(sizeof(sslAuthTypeMask) * 8 >= ssl_auth_size);

typedef struct sslServerCertStr {
    PRCList link; /* The linked list link */

    /* The auth types that this certificate provides. */
    sslAuthTypeMask authTypes;
    /* For ssl_auth_ecdsa and ssl_auth_ecdh_*.  This is only the named curve
     * of the end-entity certificate key.  The keys in other certificates in
     * the chain aren't directly relevant to the operation of TLS (though it
     * might make certificate validation difficult, libssl doesn't care). */
    const sslNamedGroupDef *namedCurve;

    /* Configuration state for server sockets */
    CERTCertificate *serverCert;
    CERTCertificateList *serverCertChain;
    sslKeyPair *serverKeyPair;
    unsigned int serverKeyBits;
    /* Each certificate needs its own status. */
    SECItemArray *certStatusArray;
    /* Serialized signed certificate timestamps to be sent to the client
    ** in a TLS extension (server only). Each certificate needs its own
    ** timestamps item.
    */
    SECItem signedCertTimestamps;

    /* The delegated credential (DC) to send to clients who indicate support for
     * the ietf-draft-tls-subcerts extension.
     */
    SECItem delegCred;
    /* The key pair used to sign the handshake when serving a DC. */
    sslKeyPair *delegCredKeyPair;
} sslServerCert;

#define SSL_CERT_IS(c, t) ((c)->authTypes & (1 << (t)))
#define SSL_CERT_IS_ONLY(c, t) ((c)->authTypes == (1 << (t)))
#define SSL_CERT_IS_EC(c)                         \
    ((c)->authTypes & ((1 << ssl_auth_ecdsa) |    \
                       (1 << ssl_auth_ecdh_rsa) | \
                       (1 << ssl_auth_ecdh_ecdsa)))

extern sslServerCert *ssl_NewServerCert();
extern sslServerCert *ssl_CopyServerCert(const sslServerCert *oc);
extern const sslServerCert *ssl_FindServerCert(
    const sslSocket *ss, SSLAuthType authType,
    const sslNamedGroupDef *namedCurve);
extern void ssl_FreeServerCert(sslServerCert *sc);

#endif /* __sslcert_h_ */
