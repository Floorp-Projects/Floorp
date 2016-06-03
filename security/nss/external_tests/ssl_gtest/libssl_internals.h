/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef libssl_internals_h_
#define libssl_internals_h_

#include <stdint.h>

#include "prio.h"
#include "seccomon.h"
#include "sslt.h"

SECStatus SSLInt_IncrementClientHandshakeVersion(PRFileDesc *fd);

PRUint32 SSLInt_DetermineKEABits(PRUint16 serverKeyBits,
                                 SSLAuthType authAlgorithm);

SECStatus SSLInt_UpdateSSLv2ClientRandom(PRFileDesc *fd,
                                         uint8_t *rnd, size_t rnd_len,
                                         uint8_t *msg, size_t msg_len);

PRBool SSLInt_ExtensionNegotiated(PRFileDesc *fd, PRUint16 ext);
void SSLInt_ClearSessionTicketKey();
PRInt32 SSLInt_CountTls13CipherSpecs(PRFileDesc *fd);
void SSLInt_ForceTimerExpiry(PRFileDesc *fd);
SECStatus SSLInt_SetMTU(PRFileDesc *fd, PRUint16 mtu);

#endif // ndef libssl_internals_h_
