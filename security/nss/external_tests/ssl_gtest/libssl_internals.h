/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef libssl_internals_h_
#define libssl_internals_h_

#include "prio.h"
#include "seccomon.h"
#include "sslt.h"

SECStatus SSLInt_IncrementClientHandshakeVersion(PRFileDesc *fd);

PRUint32 SSLInt_DetermineKEABits(PRUint16 serverKeyBits,
                                 SSLAuthType authAlgorithm);

#endif


