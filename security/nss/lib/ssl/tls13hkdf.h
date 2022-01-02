/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13hkdf_h_
#define __tls13hkdf_h_

#include "keyhi.h"
#include "sslt.h"

#ifdef __cplusplus
extern "C" {
#endif

SECStatus tls13_HkdfExtract(
    PK11SymKey *ikm1, PK11SymKey *ikm2, SSLHashType baseHash,
    PK11SymKey **prkp);
SECStatus tls13_HkdfExpandLabelRaw(
    PK11SymKey *prk, SSLHashType baseHash,
    const PRUint8 *handshakeHash, unsigned int handshakeHashLen,
    const char *label, unsigned int labelLen,
    SSLProtocolVariant variant, unsigned char *output,
    unsigned int outputLen);
SECStatus tls13_HkdfExpandLabel(
    PK11SymKey *prk, SSLHashType baseHash,
    const PRUint8 *handshakeHash, unsigned int handshakeHashLen,
    const char *label, unsigned int labelLen,
    CK_MECHANISM_TYPE algorithm, unsigned int keySize,
    SSLProtocolVariant variant, PK11SymKey **keyp);

#ifdef __cplusplus
}
#endif

#endif
