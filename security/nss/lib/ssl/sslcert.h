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

/* The following struct identifies a single slot into which a certificate can be
** loaded.  The authType field determines the basic slot, then additional
** parameters further narrow the slot.
**
** An EC key (ssl_auth_ecdsa or ssl_auth_ecdh_*) is assigned to a slot based on
** the named curve of the key.
*/
typedef struct sslServerCertTypeStr {
    SSLAuthType authType;
    /* For ssl_auth_ecdsa and ssl_auth_ecdh_*.  This is only the named curve
     * of the end-entity certificate key.  The keys in other certificates in
     * the chain aren't directly relevant to the operation of TLS (though it
     * might make certificate validation difficult, libssl doesn't care). */
    const sslNamedGroupDef *namedCurve;
} sslServerCertType;

typedef struct sslServerCertStr {
    PRCList link; /* The linked list link */

    sslServerCertType certType; /* The certificate slot this occupies */

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
} sslServerCert;

extern sslServerCert *ssl_NewServerCert(const sslServerCertType *slot);
extern sslServerCert *ssl_CopyServerCert(const sslServerCert *oc);
extern sslServerCert *ssl_FindServerCert(const sslSocket *ss,
                                         const sslServerCertType *slot);
extern sslServerCert *ssl_FindServerCertByAuthType(const sslSocket *ss,
                                                   SSLAuthType authType);
extern void ssl_FreeServerCert(sslServerCert *sc);

#endif /* __sslcert_h_ */
