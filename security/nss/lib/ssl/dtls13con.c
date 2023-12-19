/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * DTLS 1.3 Protocol
 */

#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "keyhi.h"
#include "pk11func.h"

/*Figure 3: DTLS 1.3 Unified Header */

/*
 * 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+
 * |0|0|1|C|S|L|E E|
 * +-+-+-+-+-+-+-+-+
 * | Connection ID |   Legend:
 * | (if any,      |
 * /  length as    /   C   - CID present
 * |  negotiated)  |   S   - Sequence number length
 * +-+-+-+-+-+-+-+-+   L   - Length present
 * |  8 or 16 bit  |   E   - Epoch
 * |Sequence Number|
 * +-+-+-+-+-+-+-+-+
 * | 16 bit Length |
 * | (if present)  |
 * +-+-+-+-+-+-+-+-+
 */

// E:  The two low bits (0x03) include the low order two bits of the epoch.
#define MASK_TWO_LOW_BITS 0x3
// Fixed Bits:  The three high bits of the first byte of the unified header are set to 001.
// The C bit is set if the Connection ID is present.
// The S bit (0x08) indicates the size of the sequence number, here 1 stands for 16 bits
// The L bit (0x04) is set if the length is present.
// The EE bits - mask of the epoch

// 0x2c = 0b001-0-1-1-00
//          001-C-S-L-EE
#define UNIFIED_HEADER_LONG 0x2c
// 0x20 = 0b001-0-0-1-00
//          001-C-S-L-EE
// The difference between the long and short header is in the S bit (1 for long, 0 for short).
// The S bit (0x08) indicates the size of the sequence number, here 0 stands for 8 bits
#define UNIFIED_HEADER_SHORT 0x20

// The masks to get the 8 (MASK_SEQUENCE_NUMBER_SHORT) or 16 bits (MASK_SEQUENCE_NUMBER_LONG) of the record sequence number.
#define MASK_SEQUENCE_NUMBER_SHORT 0xff
#define MASK_SEQUENCE_NUMBER_LONG 0xffff

/*The DTLS Record Layer - Figure 3 and further*/
SECStatus
dtls13_InsertCipherTextHeader(const sslSocket *ss, const ssl3CipherSpec *cwSpec,
                              sslBuffer *wrBuf, PRBool *needsLength)
{
    /* Avoid using short records for the handshake.  We pack multiple records
     * into the one datagram for the handshake. */

    /* The short header here means that the S bit is set to 0 (8-bit sequence number) */
    if (ss->opt.enableDtlsShortHeader &&
        cwSpec->epoch > TrafficKeyHandshake) {
        *needsLength = PR_FALSE;
        /* The short header is comprised of two octets in the form
         * 0b001000eessssssss where 'e' is the low two bits of the
         * epoch and 's' is the low 8 bits of the sequence number. */
        PRUint8 ct = UNIFIED_HEADER_SHORT | ((uint64_t)cwSpec->epoch & MASK_TWO_LOW_BITS);
        if (sslBuffer_AppendNumber(wrBuf, ct, sizeof(ct)) != SECSuccess) {
            return SECFailure;
        }
        PRUint8 seq = cwSpec->nextSeqNum & MASK_SEQUENCE_NUMBER_SHORT;
        return sslBuffer_AppendNumber(wrBuf, seq, sizeof(seq));
    }

    PRUint8 ct = UNIFIED_HEADER_LONG | ((PRUint8)cwSpec->epoch & MASK_TWO_LOW_BITS);
    if (sslBuffer_AppendNumber(wrBuf, ct, sizeof(ct)) != SECSuccess) {
        return SECFailure;
    }
    PRUint16 seq = cwSpec->nextSeqNum & MASK_SEQUENCE_NUMBER_LONG;
    if (sslBuffer_AppendNumber(wrBuf, seq, sizeof(seq)) != SECSuccess) {
        return SECFailure;
    }
    *needsLength = PR_TRUE;
    return SECSuccess;
}

/* DTLS 1.3 Record map for ACK processing.
 * This represents a single fragment, so a record which includes
 * multiple fragments will have one entry for each fragment on the
 * sender. We use the same structure on the receiver for convenience
 * but the only value we actually use is |record|.
 */
typedef struct DTLSHandshakeRecordEntryStr {
    PRCList link;
    PRUint16 messageSeq;      /* The handshake message sequence number. */
    PRUint32 offset;          /* The offset into the handshake message. */
    PRUint32 length;          /* The length of the fragment. */
                              /* DTLS adds an epoch and sequence number to the TLS record header. */
    sslSequenceNumber record; /* The record (includes epoch). */
    PRBool acked;             /* Has this packet been acked. */
} DTLSHandshakeRecordEntry;

/*The sequence number is set to be the low order 48
       bits of the 64 bit sequence number.*/
#define LENGTH_SEQ_NUMBER 48

/* Combine the epoch and sequence number into a single value. */
static inline sslSequenceNumber
dtls_CombineSequenceNumber(DTLSEpoch epoch, sslSequenceNumber seqNum)
{
    PORT_Assert(seqNum <= RECORD_SEQ_MAX);
    return ((sslSequenceNumber)epoch << LENGTH_SEQ_NUMBER) | seqNum;
}

SECStatus
dtls13_RememberFragment(sslSocket *ss,
                        PRCList *list,
                        PRUint32 sequence,
                        PRUint32 offset,
                        PRUint32 length,
                        DTLSEpoch epoch,
                        sslSequenceNumber record)
{
    DTLSHandshakeRecordEntry *entry;

    PORT_Assert(IS_DTLS(ss));
    /* We should never send an empty fragment with offset > 0. */
    PORT_Assert(length || !offset);

    if (!tls13_MaybeTls13(ss)) {
        return SECSuccess;
    }

    SSL_TRC(20, ("%d: SSL3[%d]: %s remembering %s record=%llx msg=%d offset=%d",
                 SSL_GETPID(), ss->fd,
                 SSL_ROLE(ss),
                 list == &ss->ssl3.hs.dtlsSentHandshake ? "sent" : "received",
                 dtls_CombineSequenceNumber(epoch, record), sequence, offset));

    entry = PORT_ZAlloc(sizeof(DTLSHandshakeRecordEntry));
    if (!entry) {
        return SECFailure;
    }

    entry->messageSeq = sequence;
    entry->offset = offset;
    entry->length = length;
    entry->record = dtls_CombineSequenceNumber(epoch, record);
    entry->acked = PR_FALSE;

    PR_APPEND_LINK(&entry->link, list);

    return SECSuccess;
}

/* RFC9147; section 7.1 */
SECStatus
dtls13_SendAck(sslSocket *ss)
{
    sslBuffer buf = SSL_BUFFER_EMPTY;
    SECStatus rv = SECSuccess;
    PRCList *cursor;
    PRInt32 sent;
    unsigned int offset = 0;

    SSL_TRC(10, ("%d: SSL3[%d]: Sending ACK",
                 SSL_GETPID(), ss->fd));

    /*    RecordNumber record_numbers<0..2^16-1>;
    2 length bytes for the list of ACKs*/
    PRUint32 sizeOfListACK = 2;
    rv = sslBuffer_Skip(&buf, sizeOfListACK, &offset);
    if (rv != SECSuccess) {
        goto loser;
    }
    for (cursor = PR_LIST_HEAD(&ss->ssl3.hs.dtlsRcvdHandshake);
         cursor != &ss->ssl3.hs.dtlsRcvdHandshake;
         cursor = PR_NEXT_LINK(cursor)) {
        DTLSHandshakeRecordEntry *entry = (DTLSHandshakeRecordEntry *)cursor;

        SSL_TRC(10, ("%d: SSL3[%d]: ACK for record=%llx",
                     SSL_GETPID(), ss->fd, entry->record));

        /*See dtls_CombineSequenceNumber function */
        PRUint64 epoch = entry->record >> 48;
        PRUint64 seqNum = entry->record & 0xffffffffffff;

        rv = sslBuffer_AppendNumber(&buf, epoch, 8);
        if (rv != SECSuccess) {
            goto loser;
        }

        rv = sslBuffer_AppendNumber(&buf, seqNum, 8);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    rv = sslBuffer_InsertLength(&buf, offset, sizeOfListACK);
    if (rv != SECSuccess) {
        goto loser;
    }

    ssl_GetXmitBufLock(ss);
    sent = ssl3_SendRecord(ss, NULL, ssl_ct_ack,
                           buf.buf, buf.len, 0);
    ssl_ReleaseXmitBufLock(ss);
    if (sent != buf.len) {
        rv = SECFailure;
        if (sent != -1) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        }
    }

loser:
    sslBuffer_Clear(&buf);
    return rv;
}

void
dtls13_SendAckCb(sslSocket *ss)
{
    if (!IS_DTLS(ss)) {
        return;
    }
    (void)dtls13_SendAck(ss);
}

/* Limits from RFC9147; section 4.5.3. */
PRBool
dtls13_AeadLimitReached(ssl3CipherSpec *spec)
{
    if (spec->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        switch (spec->cipherDef->calg) {
            case ssl_calg_chacha20:
            case ssl_calg_aes_gcm:
                return spec->deprotectionFailures >= (1ULL << 36);
#ifdef UNSAFE_FUZZER_MODE
            case ssl_calg_null:
                return PR_FALSE;
#endif
            default:
                PORT_Assert(0);
                break;
        }
    }
    return PR_FALSE;
}

/* Zero length messages are very simple to check. */
static PRBool
dtls_IsEmptyMessageAcknowledged(sslSocket *ss, PRUint16 msgSeq, PRUint32 offset)
{
    PRCList *cursor;

    for (cursor = PR_LIST_HEAD(&ss->ssl3.hs.dtlsSentHandshake);
         cursor != &ss->ssl3.hs.dtlsSentHandshake;
         cursor = PR_NEXT_LINK(cursor)) {
        DTLSHandshakeRecordEntry *entry = (DTLSHandshakeRecordEntry *)cursor;
        if (!entry->acked || msgSeq != entry->messageSeq) {
            continue;
        }
        /* Empty fragments are always offset 0. */
        if (entry->length == 0) {
            PORT_Assert(!entry->offset);
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

/* Take a range starting at |*start| and that start forwards based on the
 * contents of the acknowedgement in |entry|. Only move if the acknowledged
 * range overlaps |*start|. Return PR_TRUE if it moves. */
static PRBool
dtls_MoveUnackedStartForward(DTLSHandshakeRecordEntry *entry, PRUint32 *start)
{
    /* This entry starts too late. */
    if (*start < entry->offset) {
        return PR_FALSE;
    }
    /* This entry ends too early. */
    if (*start >= entry->offset + entry->length) {
        return PR_FALSE;
    }
    *start = entry->offset + entry->length;
    return PR_TRUE;
}

/* Take a range ending at |*end| and move that end backwards based on the
 * contents of the acknowedgement in |entry|. Only move if the acknowledged
 * range overlaps |*end|. Return PR_TRUE if it moves. */
static PRBool
dtls_MoveUnackedEndBackward(DTLSHandshakeRecordEntry *entry, PRUint32 *end)
{
    /* This entry ends too early. */
    if (*end > entry->offset + entry->length) {
        return PR_FALSE;
    }
    /* This entry starts too late. */
    if (*end <= entry->offset) {
        return PR_FALSE;
    }
    *end = entry->offset;
    return PR_TRUE;
}

/* Get the next contiguous range of unacknowledged bytes from the handshake
 * message identified by |msgSeq|.  The search starts at the offset in |offset|.
 * |len| contains the full length of the message.
 *
 * Returns PR_TRUE if there is an unacknowledged range.  In this case, values at
 * |start| and |end| are modified to contain the range.
 *
 * Returns PR_FALSE if the message is entirely acknowledged from |offset|
 * onwards.
 */
PRBool
dtls_NextUnackedRange(sslSocket *ss, PRUint16 msgSeq, PRUint32 offset,
                      PRUint32 len, PRUint32 *startOut, PRUint32 *endOut)
{
    PRCList *cur_p;
    PRBool done = PR_FALSE;
    DTLSHandshakeRecordEntry *entry;
    PRUint32 start;
    PRUint32 end;

    PORT_Assert(IS_DTLS(ss));

    *startOut = offset;
    *endOut = len;
    if (!tls13_MaybeTls13(ss)) {
        return PR_TRUE;
    }

    /* The message is empty. Use a simple search. */
    if (!len) {
        PORT_Assert(!offset);
        return !dtls_IsEmptyMessageAcknowledged(ss, msgSeq, offset);
    }

    /* This iterates multiple times over the acknowledgments and only terminates
     * when an entire iteration happens without start or end moving.  If that
     * happens without start and end crossing each other, then there is a range
     * of unacknowledged data.  If they meet, then the message is fully
     * acknowledged. */
    start = offset;
    end = len;
    while (!done) {
        done = PR_TRUE;
        for (cur_p = PR_LIST_HEAD(&ss->ssl3.hs.dtlsSentHandshake);
             cur_p != &ss->ssl3.hs.dtlsSentHandshake;
             cur_p = PR_NEXT_LINK(cur_p)) {
            entry = (DTLSHandshakeRecordEntry *)cur_p;
            if (!entry->acked || msgSeq != entry->messageSeq) {
                continue;
            }

            if (dtls_MoveUnackedStartForward(entry, &start) ||
                dtls_MoveUnackedEndBackward(entry, &end)) {
                if (start >= end) {
                    /* The message is all acknowledged. */
                    return PR_FALSE;
                }
                /* Start over again and keep going until we don't move either
                 * start or end. */
                done = PR_FALSE;
                break;
            }
        }
    }
    PORT_Assert(start < end);

    *startOut = start;
    *endOut = end;
    return PR_TRUE;
}

SECStatus
dtls13_SetupAcks(sslSocket *ss)
{
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    if (ss->ssl3.hs.endOfFlight) {
        dtls_CancelTimer(ss, ss->ssl3.hs.ackTimer);

        if (ss->ssl3.hs.ws == idle_handshake && ss->sec.isServer) {
            SSL_TRC(10, ("%d: SSL3[%d]: dtls_HandleHandshake, sending ACK",
                         SSL_GETPID(), ss->fd));
            return dtls13_SendAck(ss);
        }
        return SECSuccess;
    }

    /* We need to send an ACK. */
    if (!ss->ssl3.hs.ackTimer->cb) {
        /* We're not armed, so arm. */
        SSL_TRC(10, ("%d: SSL3[%d]: dtls_HandleHandshake, arming ack timer",
                     SSL_GETPID(), ss->fd));
        return dtls_StartTimer(ss, ss->ssl3.hs.ackTimer,
                               DTLS_RETRANSMIT_INITIAL_MS / 4,
                               dtls13_SendAckCb);
    }
    /* The ack timer is already armed, so just return. */
    return SECSuccess;
}

/*
 * Special case processing for out-of-epoch records.
 * This can only handle ACKs for now and everything else generates
 * an error. In future, may also handle KeyUpdate.
 *
 * The error checking here is as follows:
 *
 * - If it's not encrypted, out of epoch stuff is just discarded.
 * - If it's encrypted and the message is a piece of an application data, it's discarded.
 * - Else out of epoch stuff causes an error.
 *
 */
SECStatus
dtls13_HandleOutOfEpochRecord(sslSocket *ss, const ssl3CipherSpec *spec,
                              SSLContentType rType,
                              sslBuffer *databuf)
{
    SECStatus rv;
    sslBuffer buf = *databuf;

    databuf->len = 0; /* Discard data whatever happens. */
    PORT_Assert(IS_DTLS(ss));
    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
    /* Can't happen, but double check. */
    if (!IS_DTLS(ss) || (ss->version < SSL_LIBRARY_VERSION_TLS_1_3)) {
        tls13_FatalError(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }
    SSL_TRC(30, ("%d: DTLS13[%d]: %s handles out of epoch record: type=%d", SSL_GETPID(),
                 ss->fd, SSL_ROLE(ss), rType));

    if (rType == ssl_ct_ack) {
        ssl_GetSSL3HandshakeLock(ss);
        rv = dtls13_HandleAck(ss, &buf);
        ssl_ReleaseSSL3HandshakeLock(ss);
        PORT_Assert(databuf->len == 0);
        return rv;
    }

    switch (spec->epoch) {

        case TrafficKeyClearText:
            /* Drop. */
            return SECSuccess;

        case TrafficKeyHandshake:
            /* Drop out of order handshake messages, but if we are the
             * server, we might have processed the client's Finished and
             * moved on to application data keys, but the client has
             * retransmitted Finished (e.g., because our ACK got lost.)
             * We just retransmit the ACK to let the client complete. */
            if (rType == ssl_ct_handshake) {
                if ((ss->sec.isServer) &&
                    (ss->ssl3.hs.ws == idle_handshake)) {
                    PORT_Assert(dtls_TimerActive(ss, ss->ssl3.hs.hdTimer));
                    return dtls13_SendAck(ss);
                }
                return SECSuccess;
            }

            /* This isn't a handshake record, so shouldn't be encrypted
             * under the handshake key. */
            break;

        default:
            if (rType == ssl_ct_application_data) {
                return SECSuccess;
            }
            break;
    }

    SSL_TRC(10, ("%d: SSL3[%d]: unexpected out of epoch record type %d", SSL_GETPID(),
                 ss->fd, rType));

    (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
    PORT_SetError(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE);
    return SECFailure;
}

/* KeyUpdate in DTLS1.3 is required to be ACKed.
The dtls13_maybeProcessKeyUpdateAck function is called when we receive a message acknowledging KeyUpdate.
The function will then update the writing keys of the party started KeyUpdate.
*/
SECStatus
dtls13_maybeProcessKeyUpdateAck(sslSocket *ss, PRUint16 entrySeq)
{
    /*    RFC 9147. Section 8.
    Due to the possibility of an ACK message for a KeyUpdate being lost
    and thereby preventing the sender of KeyUpdate from updating its
    keying material, receivers MUST retain the pre-update keying material
    until receipt and successful decryption of a message using the new
    keys.*/

    if (ss->ssl3.hs.isKeyUpdateInProgress && entrySeq == ss->ssl3.hs.dtlsHandhakeKeyUpdateMessage) {
        SSL_TRC(30, ("%d: DTLS13[%d]: %s key update is completed", SSL_GETPID(), ss->fd, SSL_ROLE(ss)));
        PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

        SECStatus rv = SECSuccess;
        rv = tls13_UpdateTrafficKeys(ss, ssl_secret_write);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        PORT_Assert(ss->ssl3.hs.isKeyUpdateInProgress);
        ss->ssl3.hs.isKeyUpdateInProgress = PR_FALSE;

        return rv;
    }

    else
        return SECSuccess;
}

SECStatus
dtls13_HandleAck(sslSocket *ss, sslBuffer *databuf)
{
    PRUint8 *b = databuf->buf;
    PRUint32 l = databuf->len;
    unsigned int length;
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    /* Ensure we don't loop. */
    databuf->len = 0;

    PORT_Assert(IS_DTLS(ss));
    if (!tls13_MaybeTls13(ss)) {
        tls13_FatalError(ss, SSL_ERROR_RX_UNKNOWN_RECORD_TYPE, illegal_parameter);
        return SECFailure;
    }

    SSL_TRC(10, ("%d: SSL3[%d]: Handling ACK", SSL_GETPID(), ss->fd));
    rv = ssl3_ConsumeHandshakeNumber(ss, &length, 2, &b, &l);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (length != l) {
        goto loser;
    }

    while (l > 0) {
        PRUint64 seq;
        PRUint64 epoch;
        PRCList *cursor;

        rv = ssl3_ConsumeHandshakeNumber64(ss, &epoch, 8, &b, &l);
        if (rv != SECSuccess) {
            goto loser;
        }

        rv = ssl3_ConsumeHandshakeNumber64(ss, &seq, 8, &b, &l);
        if (rv != SECSuccess) {
            goto loser;
        }

        if (epoch > RECORD_EPOCH_MAX) {
            SSL_TRC(50, ("%d: SSL3[%d]: ACK message was rejected: the epoch exceeds the limit", SSL_GETPID(), ss->fd));
            continue;
        }
        if (seq > RECORD_SEQ_MAX) {
            SSL_TRC(50, ("%d: SSL3[%d]: ACK message was rejected: the sequence number exceeds the limit", SSL_GETPID(), ss->fd));
            continue;
        }

        seq = dtls_CombineSequenceNumber(epoch, seq);

        for (cursor = PR_LIST_HEAD(&ss->ssl3.hs.dtlsSentHandshake);
             cursor != &ss->ssl3.hs.dtlsSentHandshake;
             cursor = PR_NEXT_LINK(cursor)) {
            DTLSHandshakeRecordEntry *entry = (DTLSHandshakeRecordEntry *)cursor;

            if (entry->record == seq) {
                SSL_TRC(30, (
                                "%d: DTLS13[%d]: Marking record=%llx message %d offset %d length=%d as ACKed",
                                SSL_GETPID(), ss->fd,
                                entry->record, entry->messageSeq, entry->offset, entry->length));
                entry->acked = PR_TRUE;

                /* When we sent a KeyUpdate message, we have recorded the identifier of the message.
                During the HandleACK we check if we received an ack for the KeyUpdate message we sent.*/
                rv = dtls13_maybeProcessKeyUpdateAck(ss, entry->messageSeq);
                if (rv != SECSuccess) {
                    return SECFailure;
                }
            }
        }
    }
    /* Try to flush. */
    rv = dtls_TransmitMessageFlight(ss);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Reset the retransmit timer. */
    if (ss->ssl3.hs.rtTimer->cb) {
        (void)dtls_RestartTimer(ss, ss->ssl3.hs.rtTimer);
    }

    /* If there are no more messages to send, cleanup. */
    if (PR_CLIST_IS_EMPTY(&ss->ssl3.hs.lastMessageFlight)) {
        SSL_TRC(10, ("%d: SSL3[%d]: No more unacked handshake messages",
                     SSL_GETPID(), ss->fd));

        dtls_CancelTimer(ss, ss->ssl3.hs.rtTimer);
        ssl_ClearPRCList(&ss->ssl3.hs.dtlsSentHandshake, NULL);
        /* If the handshake is finished, and we're the client then
         * also clean up the handshake read cipher spec. Any ACKs
         * we receive will be with the application data cipher spec.
         * The server needs to keep the handshake cipher spec around
         * for the holddown period to process retransmitted Finisheds.
         */
        if (!ss->sec.isServer && (ss->ssl3.hs.ws == idle_handshake)) {
            ssl_CipherSpecReleaseByEpoch(ss, ssl_secret_read,
                                         TrafficKeyHandshake);
        }
    }
    return SECSuccess;

loser:
    /* Due to bug 1829391 we may incorrectly send an alert rather than
     * ignore an invalid record here. */
    SSL_TRC(11, ("%d: SSL3[%d]: Error processing DTLS1.3 ACK.",
                 SSL_GETPID(), ss->fd));
    PORT_SetError(SSL_ERROR_RX_MALFORMED_DTLS_ACK);
    return SECFailure;
}

/* Clean up the read timer for the handshake cipher suites on the
 * server.
 *
 * In DTLS 1.3, the client speaks last (Finished), and will retransmit
 * until the server ACKs that message (using application data cipher
 * suites). I.e.,
 *
 * - The client uses the retransmit timer and retransmits using the
 *   saved write handshake cipher suite.
 * - The server keeps the saved read handshake cipher suite around
 *   for the holddown period in case it needs to read the Finished.
 *
 * After the holddown period, the server assumes the client is happy
 * and discards the handshake read cipher suite.
 */

void
dtls13_HolddownTimerCb(sslSocket *ss)
{
    SSL_TRC(10, ("%d: SSL3[%d]: holddown timer fired",
                 SSL_GETPID(), ss->fd));
    ssl_CipherSpecReleaseByEpoch(ss, ssl_secret_read, TrafficKeyHandshake);
    ssl_ClearPRCList(&ss->ssl3.hs.dtlsRcvdHandshake, NULL);
}

/*RFC 9147. 4.2.3.  Record Number Encryption*/
SECStatus
dtls13_MaskSequenceNumber(sslSocket *ss, ssl3CipherSpec *spec,
                          PRUint8 *hdr, PRUint8 *cipherText, PRUint32 cipherTextLen)
{
    PORT_Assert(IS_DTLS(ss));
    if (spec->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    if (spec->maskContext) {
#ifdef UNSAFE_FUZZER_MODE
        /* Use a null mask. */
        PRUint8 mask[2] = { 0 };
#else
        /* "This procedure requires the ciphertext length be at least 16 bytes.
         * Receivers MUST reject shorter records as if they had failed
         * deprotection, as described in Section 4.5.2." */
        if (cipherTextLen < 16) {
            PORT_SetError(SSL_ERROR_BAD_MAC_READ);
            return SECFailure;
        }

        PRUint8 mask[2];
        SECStatus rv = ssl_CreateMaskInner(spec->maskContext, cipherText, cipherTextLen, mask, sizeof(mask));

        if (rv != SECSuccess) {
            PORT_SetError(SSL_ERROR_BAD_MAC_READ);
            return SECFailure;
        }
#endif

        /*
        The encrypted sequence number is computed by XORing the leading bytes
        of the mask with the on-the-wire representation of the sequence
        number.  Decryption is accomplished by the same process.
        */

        PRUint32 maskSBitIsSet = 0x08;
        hdr[1] ^= mask[0];
        if (hdr[0] & maskSBitIsSet) {
            hdr[2] ^= mask[1];
        }
    }
    return SECSuccess;
}

CK_MECHANISM_TYPE
tls13_SequenceNumberEncryptionMechanism(SSLCipherAlgorithm bulkAlgorithm)
{
    /*
    When the AEAD is based on AES, then the mask is generated by
        computing AES-ECB on the first 16 bytes of the ciphertext:

    When the AEAD is based on ChaCha20, then the mask is generated by
    treating the first 4 bytes of the ciphertext as the block counter and
    the next 12 bytes as the nonce, passing them to the ChaCha20 block
    function.
    */

    switch (bulkAlgorithm) {
        case ssl_calg_aes_gcm:
            return CKM_AES_ECB;
        case ssl_calg_chacha20:
            return CKM_NSS_CHACHA20_CTR;
        default:
            PORT_Assert(PR_FALSE);
    }
    return CKM_INVALID_MECHANISM;
}

/* The function constucts the KeyUpdate Message.
The structure is presented in RFC 9147 Section 5.2. */

SECStatus
dtls13_EnqueueKeyUpdateMessage(sslSocket *ss, tls13KeyUpdateRequest request)
{
    SECStatus rv = SECFailure;
    /*
    The epoch number is initially zero and is incremented each time
    keying material changes and a sender aims to rekey.
    More details are provided in RFC 9147 Section 6.1.*/
    rv = ssl3_AppendHandshakeHeaderAndStashSeqNum(ss, ssl_hs_key_update, 1, &ss->ssl3.hs.dtlsHandhakeKeyUpdateMessage);
    if (rv != SECSuccess) {
        return rv; /* error code set by ssl3_AppendHandshakeHeader, if applicable. */
    }
    rv = ssl3_AppendHandshakeNumber(ss, request, 1);
    if (rv != SECSuccess) {
        return rv; /* error code set by ssl3_AppendHandshakeNumber, if applicable. */
    }

    return SECSuccess;
}

/* The ssl3CipherSpecStr (sslspec.h) structure describes a spec for r/w records.
For the specification, the epoch is defined as uint16 value,
So the maximum epoch is 2 ^ 16 - 1*/
#define DTLS13_MAX_EPOCH_TYPE PR_UINT16_MAX
/*RFC 9147. Section 8.
In order to provide an extra margin of security,
sending implementations MUST NOT allow the epoch to exceed 2^48-1.*/
#define DTLS13_MAX_EPOCH ((0x1ULL << 48) - 1)

SECStatus
dtls13_MaybeSendKeyUpdate(sslSocket *ss, tls13KeyUpdateRequest request, PRBool buffer)
{

    SSL_TRC(30, ("%d: DTLS13[%d]: %s sends key update, response %s",
                 SSL_GETPID(), ss->fd, SSL_ROLE(ss),
                 (request == update_requested) ? "requested"
                                               : "not requested"));

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    SECStatus rv = SECFailure;
    /*
    For the specification, the epoch is defined as uint16 value (see bug 1809872)
    and the sendKeyUpdate will update the writing keys
    so, if the epoch is already maximum, KeyUpdate will be cancelled.*/

    ssl_GetSpecWriteLock(ss);
    /* This check is done as well in the updateTrafficKey function */
    if (ss->ssl3.cwSpec->epoch >= DTLS13_MAX_EPOCH_TYPE) {
        ssl_ReleaseSpecWriteLock(ss);
        SSL_TRC(30, ("%d: DTLS13[%d]: %s keyUpdate request was cancelled, as the writing epoch arrived to the maximum possible",
                     SSL_GETPID(), ss->fd, SSL_ROLE(ss)));
        PORT_SetError(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED);
        return SECFailure;
    } else {
        ssl_ReleaseSpecWriteLock(ss);
    }

    PORT_Assert(DTLS13_MAX_EPOCH_TYPE <= DTLS13_MAX_EPOCH);

    ssl_GetSpecReadLock(ss);
    /* TODO(AW) - See bug 1809872. */
    if (request == update_requested && ss->ssl3.crSpec->epoch >= DTLS13_MAX_EPOCH_TYPE) {
        SSL_TRC(30, ("%d: DTLS13[%d]: %s keyUpdate request update_requested was cancelled, as the reading epoch arrived to the maximum possible",
                     SSL_GETPID(), ss->fd, SSL_ROLE(ss)));
        request = update_not_requested;
    }
    ssl_ReleaseSpecReadLock(ss);

    /*    RFC 9147. Section 5.8.4.
    In contrast, implementations MUST NOT send KeyUpdate, NewConnectionId, or
    RequestConnectionId messages if an earlier message of the same type
    has not yet been acknowledged.*/
    if (ss->ssl3.hs.isKeyUpdateInProgress) {
        SSL_TRC(30, ("%d: DTLS13[%d]: the previous %s KeyUpdate message was not yet ack-ed, dropping",
                     SSL_GETPID(), ss->fd, SSL_ROLE(ss), ss->ssl3.hs.sendMessageSeq));
        return SECSuccess;
    }

    ssl_GetXmitBufLock(ss);
    rv = dtls13_EnqueueKeyUpdateMessage(ss, request);
    if (rv != SECSuccess) {
        return rv; /* error code already set */
    }

    /* Trying to send the message - without buffering. */
    /* TODO[AW]: As I just emulated the API, I am not sure that it's necessary to buffer. */
    rv = ssl3_FlushHandshake(ss, 0);
    if (rv != SECSuccess) {
        return SECFailure; /* error code set by ssl3_FlushHandshake */
    }
    ssl_ReleaseXmitBufLock(ss);

    /* The keyUpdate is started. */
    PORT_Assert(ss->ssl3.hs.isKeyUpdateInProgress == PR_FALSE);
    ss->ssl3.hs.isKeyUpdateInProgress = PR_TRUE;

    SSL_TRC(30, ("%d: DTLS13[%d]: %s has just sent keyUpdate request #%d and is waiting for ack",
                 SSL_GETPID(), ss->fd, SSL_ROLE(ss), ss->ssl3.hs.dtlsHandhakeKeyUpdateMessage));
    return SECSuccess;
}

SECStatus
dtls13_HandleKeyUpdate(sslSocket *ss, PRUint8 *b, unsigned int length, PRBool update)
{
    SSL_TRC(10, ("%d: DTLS13[%d]: %s handles Key Update",
                 SSL_GETPID(), ss->fd, SSL_ROLE(ss)));

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    SECStatus rv = SECSuccess;
    if (update == update_requested) {
        /* Respond immediately (don't buffer). */
        rv = tls13_SendKeyUpdate(ss, update_not_requested, PR_FALSE);
        if (rv != SECSuccess) {
            return SECFailure; /* Error already set. */
        }
    }

    SSL_TRC(30, ("%d: DTLS13[%d]: now %s is allowing the messages from the previous epoch",
                 SSL_GETPID(), ss->fd, SSL_ROLE(ss)));
    ss->ssl3.hs.allowPreviousEpoch = PR_TRUE;
    /* Updating the reading key. */
    rv = tls13_UpdateTrafficKeys(ss, ssl_secret_read);
    if (rv != SECSuccess) {
        return SECFailure; /* Error code set by tls13_UpdateTrafficKeys. */
    }
    return SECSuccess;
}