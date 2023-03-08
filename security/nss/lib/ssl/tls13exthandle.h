/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13exthandle_h_
#define __tls13exthandle_h_

SECStatus tls13_ServerSendStatusRequestXtn(const sslSocket *ss,
                                           TLSExtensionData *xtnData,
                                           sslBuffer *buf, PRBool *append);
SECStatus tls13_ClientSendKeyShareXtn(const sslSocket *ss,
                                      TLSExtensionData *xtnData,
                                      sslBuffer *buf, PRBool *append);
SECStatus tls13_ClientHandleKeyShareXtn(const sslSocket *ss,
                                        TLSExtensionData *xtnData,
                                        SECItem *data);
SECStatus tls13_ClientHandleKeyShareXtnHrr(const sslSocket *ss,
                                           TLSExtensionData *xtnData,
                                           SECItem *data);
SECStatus tls13_ServerHandleKeyShareXtn(const sslSocket *ss,
                                        TLSExtensionData *xtnData,
                                        SECItem *data);
SECStatus tls13_ServerSendKeyShareXtn(const sslSocket *ss,
                                      TLSExtensionData *xtnData,
                                      sslBuffer *buf, PRBool *append);
SECStatus tls13_ClientSendPreSharedKeyXtn(const sslSocket *ss,
                                          TLSExtensionData *xtnData,
                                          sslBuffer *buf, PRBool *append);
SECStatus tls13_ServerHandlePreSharedKeyXtn(const sslSocket *ss,
                                            TLSExtensionData *xtnData,
                                            SECItem *data);
SECStatus tls13_ClientHandlePreSharedKeyXtn(const sslSocket *ss,
                                            TLSExtensionData *xtnData,
                                            SECItem *data);
SECStatus tls13_ServerSendPreSharedKeyXtn(const sslSocket *ss,
                                          TLSExtensionData *xtnData,
                                          sslBuffer *buf, PRBool *append);
SECStatus tls13_ClientSendEarlyDataXtn(const sslSocket *ss,
                                       TLSExtensionData *xtnData,
                                       sslBuffer *buf, PRBool *append);
SECStatus tls13_ServerHandleEarlyDataXtn(const sslSocket *ss,
                                         TLSExtensionData *xtnData,
                                         SECItem *data);
SECStatus tls13_ClientHandleEarlyDataXtn(const sslSocket *ss,
                                         TLSExtensionData *xtnData,
                                         SECItem *data);
SECStatus tls13_ClientHandleTicketEarlyDataXtn(const sslSocket *ss,
                                               TLSExtensionData *xtnData,
                                               SECItem *data);
SECStatus tls13_ClientSendSupportedVersionsXtn(const sslSocket *ss,
                                               TLSExtensionData *xtnData,
                                               sslBuffer *buf, PRBool *append);
SECStatus tls13_ServerSendSupportedVersionsXtn(const sslSocket *ss,
                                               TLSExtensionData *xtnData,
                                               sslBuffer *buf, PRBool *added);
SECStatus tls13_ClientHandleHrrCookie(const sslSocket *ss,
                                      TLSExtensionData *xtnData,
                                      SECItem *data);
SECStatus tls13_ClientSendHrrCookieXtn(const sslSocket *ss,
                                       TLSExtensionData *xtnData,
                                       sslBuffer *buf, PRBool *append);
SECStatus tls13_ClientSendPskModesXtn(const sslSocket *ss,
                                      TLSExtensionData *xtnData,
                                      sslBuffer *buf, PRBool *append);
SECStatus tls13_ServerHandlePskModesXtn(const sslSocket *ss,
                                        TLSExtensionData *xtnData,
                                        SECItem *data);
SECStatus tls13_SendCertAuthoritiesXtn(const sslSocket *ss,
                                       TLSExtensionData *xtnData,
                                       sslBuffer *buf, PRBool *append);
SECStatus tls13_ClientHandleCertAuthoritiesXtn(const sslSocket *ss,
                                               TLSExtensionData *xtnData,
                                               SECItem *data);
SECStatus tls13_ServerHandleCertAuthoritiesXtn(const sslSocket *ss, TLSExtensionData *xtnData, SECItem *data);

SECStatus tls13_ServerHandleCookieXtn(const sslSocket *ss,
                                      TLSExtensionData *xtnData,
                                      SECItem *data);
SECStatus tls13_ServerSendHrrKeyShareXtn(const sslSocket *ss,
                                         TLSExtensionData *xtnData,
                                         sslBuffer *buf, PRBool *added);
SECStatus tls13_ServerSendHrrCookieXtn(const sslSocket *ss,
                                       TLSExtensionData *xtnData,
                                       sslBuffer *buf, PRBool *added);
SECStatus tls13_DecodeKeyShareEntry(sslReader *rdr, TLS13KeyShareEntry **ksp);
PRUint32 tls13_SizeOfKeyShareEntry(const SECKEYPublicKey *pubKey);
SECStatus tls13_EncodeKeyShareEntry(sslBuffer *buf, SSLNamedGroup group,
                                    SECKEYPublicKey *pubKey);
SECStatus tls13_ServerHandleInnerEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                        SECItem *data);
SECStatus tls13_ServerHandleOuterEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                        SECItem *data);
SECStatus tls13_ServerSendHrrEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                    sslBuffer *buf, PRBool *added);
SECStatus tls13_ServerSendEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                 sslBuffer *buf, PRBool *added);
SECStatus tls13_ClientHandleHrrEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                      SECItem *data);
SECStatus tls13_ClientHandleEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                   SECItem *data);
SECStatus tls13_ClientSendPostHandshakeAuthXtn(const sslSocket *ss,
                                               TLSExtensionData *xtnData,
                                               sslBuffer *buf, PRBool *added);
SECStatus tls13_ServerHandlePostHandshakeAuthXtn(const sslSocket *ss,
                                                 TLSExtensionData *xtnData,
                                                 SECItem *data);
SECStatus tls13_ClientHandleDelegatedCredentialsXtn(const sslSocket *ss,
                                                    TLSExtensionData *xtnData,
                                                    SECItem *data);
SECStatus tls13_ClientSendDelegatedCredentialsXtn(const sslSocket *ss,
                                                  TLSExtensionData *xtnData,
                                                  sslBuffer *buf, PRBool *added);
SECStatus tls13_ServerHandleDelegatedCredentialsXtn(const sslSocket *ss,
                                                    TLSExtensionData *xtnData,
                                                    SECItem *data);
SECStatus tls13_SendEmptyGreaseXtn(const sslSocket *ss,
                                   TLSExtensionData *xtnData,
                                   sslBuffer *buf, PRBool *added);
SECStatus tls13_SendGreaseXtn(const sslSocket *ss,
                              TLSExtensionData *xtnData,
                              sslBuffer *buf, PRBool *added);

#endif
