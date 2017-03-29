/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ssl3exthandle_h_
#define __ssl3exthandle_h_

PRInt32 ssl3_SendRenegotiationInfoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                      PRBool append, PRUint32 maxBytes);
SECStatus ssl3_HandleRenegotiationInfoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                          PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ClientHandleNextProtoNegoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ClientHandleAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                       PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ServerHandleNextProtoNegoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ServerHandleAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                       SECItem *data);
PRInt32 ssl3_ClientSendNextProtoNegoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                        PRUint32 maxBytes);
PRInt32 ssl3_ClientSendAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                   PRUint32 maxBytes);
PRInt32 ssl3_ServerSendAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                   PRUint32 maxBytes);
PRInt32 ssl3_ClientSendUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                  PRUint32 maxBytes);
PRInt32 ssl3_ServerSendUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                  PRUint32 maxBytes);
SECStatus ssl3_ClientHandleUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                      SECItem *data);
SECStatus ssl3_ServerHandleUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                      SECItem *data);
PRInt32 ssl3_ServerSendStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                        PRBool append, PRUint32 maxBytes);
SECStatus ssl3_ServerHandleStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ClientHandleStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type,
                                            SECItem *data);
PRInt32 ssl3_ClientSendStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                        PRUint32 maxBytes);
PRInt32 ssl3_ClientSendSigAlgsXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                  PRUint32 maxBytes);
SECStatus ssl3_ServerHandleSigAlgsXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                      SECItem *data);

PRInt32 ssl3_ClientSendPaddingExtension(const sslSocket *ss, TLSExtensionData *xtnData,
                                        PRBool append, PRUint32 maxBytes);

PRInt32 ssl3_ClientSendSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                              PRBool append,
                                              PRUint32 maxBytes);
SECStatus ssl3_ClientHandleSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                                  PRUint16 ex_type,
                                                  SECItem *data);
PRInt32 ssl3_ServerSendSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                              PRBool append,
                                              PRUint32 maxBytes);
SECStatus ssl3_ServerHandleSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                                  PRUint16 ex_type,
                                                  SECItem *data);
PRInt32 ssl3_SendExtendedMasterSecretXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                         PRUint32 maxBytes);
SECStatus ssl3_HandleExtendedMasterSecretXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                             PRUint16 ex_type,
                                             SECItem *data);
SECStatus ssl3_ProcessSessionTicketCommon(sslSocket *ss, SECItem *data);
PRInt32 ssl3_SendServerNameXtn(const sslSocket *ss,
                               TLSExtensionData *xtnData,
                               PRBool append,
                               PRUint32 maxBytes);
SECStatus ssl3_HandleServerNameXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                   PRUint16 ex_type, SECItem *data);
SECStatus ssl_HandleSupportedGroupsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                       PRUint16 ex_type, SECItem *data);
SECStatus ssl3_HandleSupportedPointFormatsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                              PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ClientHandleSessionTicketXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ServerHandleSessionTicketXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                            PRUint16 ex_type, SECItem *data);
PRInt32 ssl3_SendSessionTicketXtn(const sslSocket *ss,
                                  TLSExtensionData *xtnData,
                                  PRBool append,
                                  PRUint32 maxBytes);

PRInt32 ssl_SendSupportedGroupsXtn(const sslSocket *ss,
                                   TLSExtensionData *xtnData,
                                   PRBool append, PRUint32 maxBytes);
PRInt32 ssl3_SendSupportedPointFormatsXtn(const sslSocket *ss,
                                          TLSExtensionData *xtnData,
                                          PRBool append, PRUint32 maxBytes);
#endif
