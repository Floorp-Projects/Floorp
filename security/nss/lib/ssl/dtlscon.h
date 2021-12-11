/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __dtlscon_h_
#define __dtlscon_h_

extern void dtls_FreeHandshakeMessage(DTLSQueuedMessage *msg);
extern void dtls_FreeHandshakeMessages(PRCList *lst);
SECStatus dtls_TransmitMessageFlight(sslSocket *ss);
void dtls_InitTimers(sslSocket *ss);
SECStatus dtls_StartTimer(sslSocket *ss, dtlsTimer *timer,
                          PRUint32 time, DTLSTimerCb cb);
SECStatus dtls_RestartTimer(sslSocket *ss, dtlsTimer *timer);
PRBool dtls_TimerActive(sslSocket *ss, dtlsTimer *timer);
extern SECStatus dtls_HandleHandshake(sslSocket *ss, DTLSEpoch epoch,
                                      sslSequenceNumber seqNum,
                                      sslBuffer *origBuf);
extern SECStatus dtls_HandleHelloVerifyRequest(sslSocket *ss,
                                               PRUint8 *b, PRUint32 length);
extern SECStatus dtls_StageHandshakeMessage(sslSocket *ss);
extern SECStatus dtls_QueueMessage(sslSocket *ss, SSLContentType type,
                                   const PRUint8 *pIn, PRInt32 nIn);
extern SECStatus dtls_FlushHandshakeMessages(sslSocket *ss, PRInt32 flags);
SECStatus ssl3_DisableNonDTLSSuites(sslSocket *ss);
extern SECStatus dtls_StartHolddownTimer(sslSocket *ss);
extern void dtls_CheckTimer(sslSocket *ss);
extern void dtls_CancelTimer(sslSocket *ss, dtlsTimer *timer);
extern void dtls_SetMTU(sslSocket *ss, PRUint16 advertised);
extern void dtls_InitRecvdRecords(DTLSRecvdRecords *records);
extern int dtls_RecordGetRecvd(const DTLSRecvdRecords *records,
                               sslSequenceNumber seq);
extern void dtls_RecordSetRecvd(DTLSRecvdRecords *records,
                                sslSequenceNumber seq);
extern void dtls_RehandshakeCleanup(sslSocket *ss);
extern SSL3ProtocolVersion
dtls_TLSVersionToDTLSVersion(SSL3ProtocolVersion tlsv);
extern SSL3ProtocolVersion
dtls_DTLSVersionToTLSVersion(SSL3ProtocolVersion dtlsv);
DTLSEpoch dtls_ReadEpoch(const ssl3CipherSpec *crSpec, const PRUint8 *hdr);
extern PRBool dtls_IsRelevant(sslSocket *ss, const ssl3CipherSpec *spec,
                              const SSL3Ciphertext *cText,
                              sslSequenceNumber *seqNum);
void dtls_ReceivedFirstMessageInFlight(sslSocket *ss);
PRBool dtls_IsLongHeader(SSL3ProtocolVersion version, PRUint8 firstOctet);
PRBool dtls_IsDtls13Ciphertext(SSL3ProtocolVersion version, PRUint8 firstOctet);
#endif
