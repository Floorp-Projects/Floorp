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
