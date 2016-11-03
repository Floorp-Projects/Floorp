/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13exthandle_h_
#define __tls13exthandle_h_

PRInt32 tls13_ServerSendStatusRequestXtn(sslSocket *ss,
                                         PRBool append, PRUint32 maxBytes);
PRInt32 tls13_ClientSendKeyShareXtn(sslSocket *ss, PRBool append,
                                    PRUint32 maxBytes);
SECStatus tls13_ClientHandleKeyShareXtn(sslSocket *ss,
                                        PRUint16 ex_type,
                                        SECItem *data);
SECStatus tls13_ClientHandleKeyShareXtnHrr(sslSocket *ss,
                                           PRUint16 ex_type,
                                           SECItem *data);
SECStatus tls13_ServerHandleKeyShareXtn(sslSocket *ss,
                                        PRUint16 ex_type,
                                        SECItem *data);
PRInt32 tls13_ClientSendPreSharedKeyXtn(sslSocket *ss, PRBool append,
                                        PRUint32 maxBytes);
SECStatus tls13_ServerHandlePreSharedKeyXtn(sslSocket *ss,
                                            PRUint16 ex_type,
                                            SECItem *data);
SECStatus tls13_ClientHandlePreSharedKeyXtn(sslSocket *ss,
                                            PRUint16 ex_type,
                                            SECItem *data);
PRInt32 tls13_ClientSendEarlyDataXtn(sslSocket *ss,
                                     PRBool append,
                                     PRUint32 maxBytes);
SECStatus tls13_ServerHandleEarlyDataXtn(sslSocket *ss, PRUint16 ex_type,
                                         SECItem *data);
SECStatus tls13_ClientHandleEarlyDataXtn(sslSocket *ss, PRUint16 ex_type,
                                         SECItem *data);
SECStatus tls13_ClientHandleTicketEarlyDataInfoXtn(
    sslSocket *ss, PRUint16 ex_type,
    SECItem *data);
SECStatus tls13_ClientHandleSigAlgsXtn(sslSocket *ss, PRUint16 ex_type,
                                       SECItem *data);
PRInt32 tls13_ClientSendSupportedVersionsXtn(sslSocket *ss,
                                             PRBool append,
                                             PRUint32 maxBytes);
SECStatus tls13_ClientHandleHrrCookie(sslSocket *ss, PRUint16 ex_type,
                                      SECItem *data);
PRInt32 tls13_ClientSendHrrCookieXtn(sslSocket *ss,
                                     PRBool append,
                                     PRUint32 maxBytes);

#endif
