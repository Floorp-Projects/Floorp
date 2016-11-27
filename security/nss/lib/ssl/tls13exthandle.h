/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13exthandle_h_
#define __tls13exthandle_h_

PRInt32 tls13_ServerSendStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                         PRBool append, PRUint32 maxBytes);
PRInt32 tls13_ClientSendKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                    PRUint32 maxBytes);
SECStatus tls13_ClientHandleKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                        PRUint16 ex_type,
                                        SECItem *data);
SECStatus tls13_ClientHandleKeyShareXtnHrr(const sslSocket *ss, TLSExtensionData *xtnData,
                                           PRUint16 ex_type,
                                           SECItem *data);
SECStatus tls13_ServerHandleKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                        PRUint16 ex_type,
                                        SECItem *data);
PRInt32 tls13_ServerSendKeyShareXtn(const sslSocket *ss,
                                    TLSExtensionData *xtnData,
                                    PRBool append,
                                    PRUint32 maxBytes);
PRInt32 tls13_ClientSendPreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                        PRUint32 maxBytes);
SECStatus tls13_ServerHandlePreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type,
                                            SECItem *data);
SECStatus tls13_ClientHandlePreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type,
                                            SECItem *data);
PRInt32 tls13_ServerSendPreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                        PRBool append,
                                        PRUint32 maxBytes);
PRInt32 tls13_ClientSendEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                     PRBool append,
                                     PRUint32 maxBytes);
SECStatus tls13_ServerHandleEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                         SECItem *data);
SECStatus tls13_ClientHandleEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                         SECItem *data);
PRInt32 tls13_ServerSendEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                     PRBool append,
                                     PRUint32 maxBytes);
SECStatus tls13_ClientHandleTicketEarlyDataInfoXtn(
    const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
    SECItem *data);
PRInt32 tls13_ClientSendSupportedVersionsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                             PRBool append,
                                             PRUint32 maxBytes);
SECStatus tls13_ClientHandleHrrCookie(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                      SECItem *data);
PRInt32 tls13_ClientSendHrrCookieXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                     PRBool append,
                                     PRUint32 maxBytes);
PRInt32 tls13_ClientSendPskKeyExchangeModesXtn(const sslSocket *ss,
                                               TLSExtensionData *xtnData,
                                               PRBool append, PRUint32 maxBytes);
SECStatus tls13_ServerHandlePskKeyExchangeModesXtn(const sslSocket *ss,
                                                   TLSExtensionData *xtnData,
                                                   PRUint16 ex_type, SECItem *data);

#endif
