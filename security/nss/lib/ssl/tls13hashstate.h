/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13hashstate_h_
#define __tls13hashstate_h_

#include "ssl.h"
#include "sslt.h"
#include "sslimpl.h"

SECStatus tls13_MakeHrrCookie(sslSocket *ss, const sslNamedGroupDef *selectedGroup,
                              const PRUint8 *appToken, unsigned int appTokenLen,
                              PRUint8 *buf, unsigned int *len, unsigned int maxlen);
SECStatus tls13_GetHrrCookieLength(sslSocket *ss, unsigned int *length);
SECStatus tls13_RecoverHashState(sslSocket *ss,
                                 unsigned char *cookie, unsigned int cookieLen,
                                 ssl3CipherSuite *previousCipherSuite,
                                 const sslNamedGroupDef **previousGroup,
                                 PRBool *previousEchOffered);
#endif
