/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file contains functions for frobbing the internals of libssl */
#include "libssl_internals.h"

#include "seccomon.h"
#include "ssl.h"
#include "sslimpl.h"

SECStatus
SSLInt_IncrementClientHandshakeVersion(PRFileDesc *fd)
{
    sslSocket *ss = (sslSocket *)fd->secret;

    if (!ss) {
        return SECFailure;
    }

    ++ss->clientHelloVersion;

    return SECSuccess;
}

PRUint32
SSLInt_DetermineKEABits(PRUint16 serverKeyBits, SSLAuthType authAlgorithm) {
    // For ECDSA authentication we expect a curve for key exchange with the
    // same strength as the one used for the certificate's signature.
    if (authAlgorithm == ssl_auth_ecdsa) {
        return serverKeyBits;
    }

    PORT_Assert(authAlgorithm == ssl_auth_rsa);
    PRUint32 minKeaBits;
#ifdef NSS_ECC_MORE_THAN_SUITE_B
    // P-192 is the smallest curve we want to use.
    minKeaBits = 192U;
#else
    // P-256 is the smallest supported curve.
    minKeaBits = 256U;
#endif

    return PR_MAX(SSL_RSASTRENGTH_TO_ECSTRENGTH(serverKeyBits), minKeaBits);
}
