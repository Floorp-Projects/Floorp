/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ssl3exthandle_h_
#define __ssl3exthandle_h_

PRInt32 ssl3_SendRenegotiationInfoXtn(sslSocket *ss,
                                      PRBool append, PRUint32 maxBytes);
SECStatus ssl3_HandleRenegotiationInfoXtn(sslSocket *ss,
                                          PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ClientHandleNextProtoNegoXtn(sslSocket *ss,
                                            PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ClientHandleAppProtoXtn(sslSocket *ss,
                                       PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ServerHandleNextProtoNegoXtn(sslSocket *ss,
                                            PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ServerHandleAppProtoXtn(sslSocket *ss, PRUint16 ex_type,
                                       SECItem *data);
PRInt32 ssl3_ClientSendNextProtoNegoXtn(sslSocket *ss, PRBool append,
                                        PRUint32 maxBytes);
PRInt32 ssl3_ClientSendAppProtoXtn(sslSocket *ss, PRBool append,
                                   PRUint32 maxBytes);
PRInt32 ssl3_ServerSendAppProtoXtn(sslSocket *ss, PRBool append,
                                   PRUint32 maxBytes);
PRInt32 ssl3_ClientSendUseSRTPXtn(sslSocket *ss, PRBool append,
                                  PRUint32 maxBytes);
PRInt32 ssl3_ServerSendUseSRTPXtn(sslSocket *ss, PRBool append,
                                  PRUint32 maxBytes);
SECStatus ssl3_ClientHandleUseSRTPXtn(sslSocket *ss, PRUint16 ex_type,
                                      SECItem *data);
SECStatus ssl3_ServerHandleUseSRTPXtn(sslSocket *ss, PRUint16 ex_type,
                                      SECItem *data);
PRInt32 ssl3_ServerSendStatusRequestXtn(sslSocket *ss,
                                        PRBool append, PRUint32 maxBytes);
SECStatus ssl3_ServerHandleStatusRequestXtn(sslSocket *ss,
                                            PRUint16 ex_type, SECItem *data);
SECStatus ssl3_ClientHandleStatusRequestXtn(sslSocket *ss,
                                            PRUint16 ex_type,
                                            SECItem *data);
PRInt32 ssl3_ClientSendStatusRequestXtn(sslSocket *ss, PRBool append,
                                        PRUint32 maxBytes);
PRInt32 ssl3_ClientSendSigAlgsXtn(sslSocket *ss, PRBool append,
                                  PRUint32 maxBytes);
SECStatus ssl3_ServerHandleSigAlgsXtn(sslSocket *ss, PRUint16 ex_type,
                                      SECItem *data);

PRInt32 ssl3_ClientSendSignedCertTimestampXtn(sslSocket *ss,
                                              PRBool append,
                                              PRUint32 maxBytes);
SECStatus ssl3_ClientHandleSignedCertTimestampXtn(sslSocket *ss,
                                                  PRUint16 ex_type,
                                                  SECItem *data);
PRInt32 ssl3_ServerSendSignedCertTimestampXtn(sslSocket *ss,
                                              PRBool append,
                                              PRUint32 maxBytes);
SECStatus ssl3_ServerHandleSignedCertTimestampXtn(sslSocket *ss,
                                                  PRUint16 ex_type,
                                                  SECItem *data);
PRInt32 ssl3_SendExtendedMasterSecretXtn(sslSocket *ss, PRBool append,
                                         PRUint32 maxBytes);
SECStatus ssl3_HandleExtendedMasterSecretXtn(sslSocket *ss,
                                             PRUint16 ex_type,
                                             SECItem *data);
SECStatus ssl3_ProcessSessionTicketCommon(sslSocket *ss, SECItem *data);

#endif
