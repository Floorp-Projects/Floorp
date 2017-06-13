/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __selfencrypt_h_
#define __selfencrypt_h_

#include "secmodt.h"

SECStatus ssl_SelfEncryptProtect(
    sslSocket *ss, const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen);
SECStatus ssl_SelfEncryptUnprotect(
    sslSocket *ss, const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen);

/* Exported for use in unit tests.*/
SECStatus ssl_SelfEncryptProtectInt(
    PK11SymKey *encKey, PK11SymKey *macKey, const unsigned char *keyName,
    const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen);
SECStatus ssl_SelfEncryptUnprotectInt(
    PK11SymKey *encKey, PK11SymKey *macKey, const unsigned char *keyName,
    const PRUint8 *in, unsigned int inLen,
    PRUint8 *out, unsigned int *outLen, unsigned int maxOutLen);

#endif
