/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __dtls13con_h_
#define __dtls13con_h_

/*
The structure ssl3CipherSpecStr represents epoch as uint16 (DTLSEpoch epoch),
So the maximum epoch is 2 ^ 16 - 1
See bug: https://bugzilla.mozilla.org/show_bug.cgi?id=1809196 */
#define RECORD_EPOCH_MAX UINT16_MAX

SECStatus dtls13_InsertCipherTextHeader(const sslSocket *ss,
                                        const ssl3CipherSpec *cwSpec,
                                        sslBuffer *wrBuf,
                                        PRBool *needsLength);
SECStatus dtls13_RememberFragment(sslSocket *ss, PRCList *list,
                                  PRUint32 sequence, PRUint32 offset,
                                  PRUint32 length, DTLSEpoch epoch,
                                  sslSequenceNumber record);
PRBool dtls_NextUnackedRange(sslSocket *ss, PRUint16 msgSeq, PRUint32 offset,
                             PRUint32 len, PRUint32 *startOut, PRUint32 *endOut);
SECStatus dtls13_SetupAcks(sslSocket *ss);
SECStatus dtls13_HandleOutOfEpochRecord(sslSocket *ss, const ssl3CipherSpec *spec,
                                        SSLContentType rType,
                                        sslBuffer *databuf);
SECStatus dtls13_HandleAck(sslSocket *ss, sslBuffer *databuf);

SECStatus dtls13_SendAck(sslSocket *ss);
void dtls13_SendAckCb(sslSocket *ss);
void dtls13_HolddownTimerCb(sslSocket *ss);
void dtls_ReceivedFirstMessageInFlight(sslSocket *ss);
SECStatus dtls13_MaskSequenceNumber(sslSocket *ss, ssl3CipherSpec *spec,
                                    PRUint8 *hdr, PRUint8 *cipherText, PRUint32 cipherTextLen);
PRBool dtls13_AeadLimitReached(ssl3CipherSpec *spec);

CK_MECHANISM_TYPE tls13_SequenceNumberEncryptionMechanism(SSLCipherAlgorithm bulkAlgorithm);
SECStatus dtls13_MaybeSendKeyUpdate(sslSocket *ss, tls13KeyUpdateRequest request, PRBool buffer);
SECStatus dtls13_HandleKeyUpdate(sslSocket *ss, PRUint8 *b, unsigned int length, PRBool update_requested);
#endif
