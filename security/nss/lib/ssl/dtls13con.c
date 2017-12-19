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
    sslSequenceNumber record; /* The record (includes epoch). */
    PRBool acked;             /* Has this packet been acked. */
} DTLSHandshakeRecordEntry;

/* Combine the epoch and sequence number into a single value. */
static inline sslSequenceNumber
dtls_CombineSequenceNumber(DTLSEpoch epoch, sslSequenceNumber seqNum)
{
    PORT_Assert(seqNum <= RECORD_SEQ_MAX);
    return ((sslSequenceNumber)epoch << 48) | seqNum;
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

SECStatus
dtls13_SendAck(sslSocket *ss)
{
    sslBuffer buf = SSL_BUFFER_EMPTY;
    SECStatus rv = SECSuccess;
    PRCList *cursor;
    PRInt32 sent;

    SSL_TRC(10, ("%d: SSL3[%d]: Sending ACK",
                 SSL_GETPID(), ss->fd));

    for (cursor = PR_LIST_HEAD(&ss->ssl3.hs.dtlsRcvdHandshake);
         cursor != &ss->ssl3.hs.dtlsRcvdHandshake;
         cursor = PR_NEXT_LINK(cursor)) {
        DTLSHandshakeRecordEntry *entry = (DTLSHandshakeRecordEntry *)cursor;

        SSL_TRC(10, ("%d: SSL3[%d]: ACK for record=%llx",
                     SSL_GETPID(), ss->fd, entry->record));
        rv = sslBuffer_AppendNumber(&buf, entry->record, 8);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    ssl_GetXmitBufLock(ss);
    sent = ssl3_SendRecord(ss, NULL, content_ack,
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
 * - If it's encrypted, out of epoch stuff causes an error.
 */
SECStatus
dtls13_HandleOutOfEpochRecord(sslSocket *ss, const ssl3CipherSpec *spec,
                              SSL3ContentType rType,
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
    SSL_TRC(10, ("%d: DTLS13[%d]: handle out of epoch record: type=%d", SSL_GETPID(),
                 ss->fd, rType));

    if (rType == content_ack) {
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
             * We just retransmit the previous Finished to let the client
             * complete. */
            if (rType == content_handshake) {
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
            /* Any other epoch is forbidden. */
            break;
    }

    SSL_TRC(10, ("%d: SSL3[%d]: unexpected out of epoch record type %d", SSL_GETPID(),
                 ss->fd, rType));

    (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
    PORT_SetError(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE);
    return SECFailure;
}

SECStatus
dtls13_HandleAck(sslSocket *ss, sslBuffer *databuf)
{
    PRUint8 *b = databuf->buf;
    PRUint32 l = databuf->len;
    SECStatus rv;

    /* Ensure we don't loop. */
    databuf->len = 0;

    PORT_Assert(IS_DTLS(ss));
    if (!tls13_MaybeTls13(ss)) {
        tls13_FatalError(ss, SSL_ERROR_RX_UNKNOWN_RECORD_TYPE, illegal_parameter);
        return SECSuccess;
    }

    SSL_TRC(10, ("%d: SSL3[%d]: Handling ACK", SSL_GETPID(), ss->fd));
    while (l > 0) {
        PRUint64 seq;
        PRCList *cursor;

        rv = ssl3_ConsumeHandshakeNumber64(ss, &seq, 8, &b, &l);
        if (rv != SECSuccess) {
            return SECFailure;
        }

        for (cursor = PR_LIST_HEAD(&ss->ssl3.hs.dtlsSentHandshake);
             cursor != &ss->ssl3.hs.dtlsSentHandshake;
             cursor = PR_NEXT_LINK(cursor)) {
            DTLSHandshakeRecordEntry *entry = (DTLSHandshakeRecordEntry *)cursor;

            if (entry->record == seq) {
                SSL_TRC(10, (
                                "%d: SSL3[%d]: Marking record=%llx message %d offset %d length=%d as ACKed",
                                SSL_GETPID(), ss->fd,
                                seq, entry->messageSeq, entry->offset, entry->length));
                entry->acked = PR_TRUE;
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
            ssl_CipherSpecReleaseByEpoch(ss, CipherSpecRead,
                                         TrafficKeyHandshake);
        }
    }
    return SECSuccess;
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
    ssl_CipherSpecReleaseByEpoch(ss, CipherSpecRead, TrafficKeyHandshake);
    ssl_ClearPRCList(&ss->ssl3.hs.dtlsRcvdHandshake, NULL);
}
