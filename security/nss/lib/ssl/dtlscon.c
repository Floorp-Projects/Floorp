/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * DTLS Protocol
 */

#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"

#ifndef PR_ARRAY_SIZE
#define PR_ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

static SECStatus dtls_TransmitMessageFlight(sslSocket *ss);
static void dtls_RetransmitTimerExpiredCb(sslSocket *ss);
static SECStatus dtls_SendSavedWriteData(sslSocket *ss);

/* -28 adjusts for the IP/UDP header */
static const PRUint16 COMMON_MTU_VALUES[] = {
    1500 - 28,  /* Ethernet MTU */
    1280 - 28,  /* IPv6 minimum MTU */
    576 - 28,   /* Common assumption */
    256 - 28    /* We're in serious trouble now */
};

#define DTLS_COOKIE_BYTES 32

/* List copied from ssl3con.c:cipherSuites */
static const ssl3CipherSuite nonDTLSSuites[] = {
#ifndef NSS_DISABLE_ECC
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
#endif /* NSS_DISABLE_ECC */
    TLS_DHE_DSS_WITH_RC4_128_SHA,
#ifndef NSS_DISABLE_ECC
    TLS_ECDH_RSA_WITH_RC4_128_SHA,
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
#endif /* NSS_DISABLE_ECC */
    TLS_RSA_WITH_RC4_128_MD5,
    TLS_RSA_WITH_RC4_128_SHA,
    TLS_RSA_EXPORT1024_WITH_RC4_56_SHA,
    TLS_RSA_EXPORT_WITH_RC4_40_MD5,
    0 /* End of list marker */
};

/* Map back and forth between TLS and DTLS versions in wire format.
 * Mapping table is:
 *
 * TLS             DTLS
 * 1.1 (0302)      1.0 (feff)
 * 1.2 (0303)      1.2 (fefd)
 * 1.3 (0304)      1.3 (fefc)
 */
SSL3ProtocolVersion
dtls_TLSVersionToDTLSVersion(SSL3ProtocolVersion tlsv)
{
    if (tlsv == SSL_LIBRARY_VERSION_TLS_1_1) {
        return SSL_LIBRARY_VERSION_DTLS_1_0_WIRE;
    }
    if (tlsv == SSL_LIBRARY_VERSION_TLS_1_2) {
        return SSL_LIBRARY_VERSION_DTLS_1_2_WIRE;
    }
    if (tlsv == SSL_LIBRARY_VERSION_TLS_1_3) {
        return SSL_LIBRARY_VERSION_DTLS_1_3_WIRE;
    }

    /* Anything other than TLS 1.1 or 1.2 is an error, so return
     * the invalid version 0xffff. */
    return 0xffff;
}

/* Map known DTLS versions to known TLS versions.
 * - Invalid versions (< 1.0) return a version of 0
 * - Versions > known return a version one higher than we know of
 * to accomodate a theoretically newer version */
SSL3ProtocolVersion
dtls_DTLSVersionToTLSVersion(SSL3ProtocolVersion dtlsv)
{
    if (MSB(dtlsv) == 0xff) {
        return 0;
    }

    if (dtlsv == SSL_LIBRARY_VERSION_DTLS_1_0_WIRE) {
        return SSL_LIBRARY_VERSION_TLS_1_1;
    }
    if (dtlsv == SSL_LIBRARY_VERSION_DTLS_1_2_WIRE) {
        return SSL_LIBRARY_VERSION_TLS_1_2;
    }
    if (dtlsv == SSL_LIBRARY_VERSION_DTLS_1_3_WIRE) {
        return SSL_LIBRARY_VERSION_TLS_1_3;
    }

    /* Return a fictional higher version than we know of */
    return SSL_LIBRARY_VERSION_TLS_1_2 + 1;
}

/* On this socket, Disable non-DTLS cipher suites in the argument's list */
SECStatus
ssl3_DisableNonDTLSSuites(sslSocket * ss)
{
    const ssl3CipherSuite * suite;

    for (suite = nonDTLSSuites; *suite; ++suite) {
        PORT_CheckSuccess(ssl3_CipherPrefSet(ss, *suite, PR_FALSE));
    }
    return SECSuccess;
}

/* Allocate a DTLSQueuedMessage.
 *
 * Called from dtls_QueueMessage()
 */
static DTLSQueuedMessage *
dtls_AllocQueuedMessage(PRUint16 epoch, SSL3ContentType type,
                        const unsigned char *data, PRUint32 len)
{
    DTLSQueuedMessage *msg = NULL;

    msg = PORT_ZAlloc(sizeof(DTLSQueuedMessage));
    if (!msg)
        return NULL;

    msg->data = PORT_Alloc(len);
    if (!msg->data) {
        PORT_Free(msg);
        return NULL;
    }
    PORT_Memcpy(msg->data, data, len);

    msg->len = len;
    msg->epoch = epoch;
    msg->type = type;

    return msg;
}

/*
 * Free a handshake message
 *
 * Called from dtls_FreeHandshakeMessages()
 */
static void
dtls_FreeHandshakeMessage(DTLSQueuedMessage *msg)
{
    if (!msg)
        return;

    PORT_ZFree(msg->data, msg->len);
    PORT_Free(msg);
}

/*
 * Free a list of handshake messages
 *
 * Called from:
 *              dtls_HandleHandshake()
 *              ssl3_DestroySSL3Info()
 */
void
dtls_FreeHandshakeMessages(PRCList *list)
{
    PRCList *cur_p;

    while (!PR_CLIST_IS_EMPTY(list)) {
        cur_p = PR_LIST_TAIL(list);
        PR_REMOVE_LINK(cur_p);
        dtls_FreeHandshakeMessage((DTLSQueuedMessage *)cur_p);
    }
}

/* Called only from ssl3_HandleRecord, for each (deciphered) DTLS record.
 * origBuf is the decrypted ssl record content and is expected to contain
 * complete handshake records
 * Caller must hold the handshake and RecvBuf locks.
 *
 * Note that this code uses msg_len for two purposes:
 *
 * (1) To pass the length to ssl3_HandleHandshakeMessage()
 * (2) To carry the length of a message currently being reassembled
 *
 * However, unlike ssl3_HandleHandshake(), it is not used to carry
 * the state of reassembly (i.e., whether one is in progress). That
 * is carried in recvdHighWater and recvdFragments.
 */
#define OFFSET_BYTE(o) (o/8)
#define OFFSET_MASK(o) (1 << (o%8))

SECStatus
dtls_HandleHandshake(sslSocket *ss, sslBuffer *origBuf)
{
    /* XXX OK for now.
     * This doesn't work properly with asynchronous certificate validation.
     * because that returns a WOULDBLOCK error. The current DTLS
     * applications do not need asynchronous validation, but in the
     * future we will need to add this.
     */
    sslBuffer buf = *origBuf;
    SECStatus rv = SECSuccess;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    while (buf.len > 0) {
        PRUint8 type;
        PRUint32 message_length;
        PRUint16 message_seq;
        PRUint32 fragment_offset;
        PRUint32 fragment_length;
        PRUint32 offset;

        if (buf.len < 12) {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
            rv = SECFailure;
            break;
        }

        /* Parse the header */
        type = buf.buf[0];
        message_length = (buf.buf[1] << 16) | (buf.buf[2] << 8) | buf.buf[3];
        message_seq = (buf.buf[4] << 8) | buf.buf[5];
        fragment_offset = (buf.buf[6] << 16) | (buf.buf[7] << 8) | buf.buf[8];
        fragment_length = (buf.buf[9] << 16) | (buf.buf[10] << 8) | buf.buf[11];

#define MAX_HANDSHAKE_MSG_LEN 0x1ffff   /* 128k - 1 */
        if (message_length > MAX_HANDSHAKE_MSG_LEN) {
            (void)ssl3_DecodeError(ss);
            PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
            return SECFailure;
        }
#undef MAX_HANDSHAKE_MSG_LEN

        buf.buf += 12;
        buf.len -= 12;

        /* This fragment must be complete */
        if (buf.len < fragment_length) {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
            rv = SECFailure;
            break;
        }

        /* Sanity check the packet contents */
        if ((fragment_length + fragment_offset) > message_length) {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
            rv = SECFailure;
            break;
        }

        /* There are three ways we could not be ready for this packet.
         *
         * 1. It's a partial next message.
         * 2. It's a partial or complete message beyond the next
         * 3. It's a message we've already seen
         *
         * If it's the complete next message we accept it right away.
         * This is the common case for short messages
         */
        if ((message_seq == ss->ssl3.hs.recvMessageSeq)
            && (fragment_offset == 0)
            && (fragment_length == message_length)) {
            /* Complete next message. Process immediately */
            ss->ssl3.hs.msg_type = (SSL3HandshakeType)type;
            ss->ssl3.hs.msg_len = message_length;

            /* At this point we are advancing our state machine, so
             * we can free our last flight of messages */
            dtls_FreeHandshakeMessages(&ss->ssl3.hs.lastMessageFlight);
            ss->ssl3.hs.recvdHighWater = -1;
            dtls_CancelTimer(ss);

            /* Reset the timer to the initial value if the retry counter
             * is 0, per Sec. 4.2.4.1 */
            if (ss->ssl3.hs.rtRetries == 0) {
                ss->ssl3.hs.rtTimeoutMs = INITIAL_DTLS_TIMEOUT_MS;
            }

            rv = ssl3_HandleHandshakeMessage(ss, buf.buf, ss->ssl3.hs.msg_len);
            if (rv == SECFailure) {
                /* Do not attempt to process rest of messages in this record */
                break;
            }
        } else {
            if (message_seq < ss->ssl3.hs.recvMessageSeq) {
                /* Case 3: we do an immediate retransmit if we're
                 * in a waiting state*/
                if (ss->ssl3.hs.rtTimerCb == NULL) {
                    /* Ignore */
                } else if (ss->ssl3.hs.rtTimerCb ==
                         dtls_RetransmitTimerExpiredCb) {
                    SSL_TRC(30, ("%d: SSL3[%d]: Retransmit detected",
                                 SSL_GETPID(), ss->fd));
                    /* Check to see if we retransmitted recently. If so,
                     * suppress the triggered retransmit. This avoids
                     * retransmit wars after packet loss.
                     * This is not in RFC 5346 but should be
                     */
                    if ((PR_IntervalNow() - ss->ssl3.hs.rtTimerStarted) >
                        (ss->ssl3.hs.rtTimeoutMs / 4)) {
                            SSL_TRC(30,
                            ("%d: SSL3[%d]: Shortcutting retransmit timer",
                            SSL_GETPID(), ss->fd));

                            /* Cancel the timer and call the CB,
                             * which re-arms the timer */
                            dtls_CancelTimer(ss);
                            dtls_RetransmitTimerExpiredCb(ss);
                            rv = SECSuccess;
                            break;
                        } else {
                            SSL_TRC(30,
                            ("%d: SSL3[%d]: We just retransmitted. Ignoring.",
                            SSL_GETPID(), ss->fd));
                            rv = SECSuccess;
                            break;
                        }
                } else if (ss->ssl3.hs.rtTimerCb == dtls_FinishedTimerCb) {
                    /* Retransmit the messages and re-arm the timer
                     * Note that we are not backing off the timer here.
                     * The spec isn't clear and my reasoning is that this
                     * may be a re-ordered packet rather than slowness,
                     * so let's be aggressive. */
                    dtls_CancelTimer(ss);
                    rv = dtls_TransmitMessageFlight(ss);
                    if (rv == SECSuccess) {
                        rv = dtls_StartTimer(ss, dtls_FinishedTimerCb);
                    }
                    if (rv != SECSuccess)
                        return rv;
                    break;
                }
            } else if (message_seq > ss->ssl3.hs.recvMessageSeq) {
                /* Case 2
                 *
                 * Ignore this message. This means we don't handle out of
                 * order complete messages that well, but we're still
                 * compliant and this probably does not happen often
                 *
                 * XXX OK for now. Maybe do something smarter at some point?
                 */
            } else {
                /* Case 1
                 *
                 * Buffer the fragment for reassembly
                 */
                /* Make room for the message */
                if (ss->ssl3.hs.recvdHighWater == -1) {
                    PRUint32 map_length = OFFSET_BYTE(message_length) + 1;

                    rv = sslBuffer_Grow(&ss->ssl3.hs.msg_body, message_length);
                    if (rv != SECSuccess)
                        break;
                    /* Make room for the fragment map */
                    rv = sslBuffer_Grow(&ss->ssl3.hs.recvdFragments,
                                        map_length);
                    if (rv != SECSuccess)
                        break;

                    /* Reset the reassembly map */
                    ss->ssl3.hs.recvdHighWater = 0;
                    PORT_Memset(ss->ssl3.hs.recvdFragments.buf, 0,
                                ss->ssl3.hs.recvdFragments.space);
                    ss->ssl3.hs.msg_type = (SSL3HandshakeType)type;
                    ss->ssl3.hs.msg_len = message_length;
                }

                /* If we have a message length mismatch, abandon the reassembly
                 * in progress and hope that the next retransmit will give us
                 * something sane
                 */
                if (message_length != ss->ssl3.hs.msg_len) {
                    ss->ssl3.hs.recvdHighWater = -1;
                    PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
                    rv = SECFailure;
                    break;
                }

                /* Now copy this fragment into the buffer */
                PORT_Assert((fragment_offset + fragment_length) <=
                            ss->ssl3.hs.msg_body.space);
                PORT_Memcpy(ss->ssl3.hs.msg_body.buf + fragment_offset,
                            buf.buf, fragment_length);

                /* This logic is a bit tricky. We have two values for
                 * reassembly state:
                 *
                 * - recvdHighWater contains the highest contiguous number of
                 *   bytes received
                 * - recvdFragments contains a bitmask of packets received
                 *   above recvdHighWater
                 *
                 * This avoids having to fill in the bitmask in the common
                 * case of adjacent fragments received in sequence
                 */
                if (fragment_offset <= (unsigned int)ss->ssl3.hs.recvdHighWater) {
                    /* Either this is the adjacent fragment or an overlapping
                     * fragment */
                    ss->ssl3.hs.recvdHighWater = fragment_offset +
                                                 fragment_length;
                } else {
                    for (offset = fragment_offset;
                         offset < fragment_offset + fragment_length;
                         offset++) {
                        ss->ssl3.hs.recvdFragments.buf[OFFSET_BYTE(offset)] |=
                            OFFSET_MASK(offset);
                    }
                }

                /* Now figure out the new high water mark if appropriate */
                for (offset = ss->ssl3.hs.recvdHighWater;
                     offset < ss->ssl3.hs.msg_len; offset++) {
                    /* Note that this loop is not efficient, since it counts
                     * bit by bit. If we have a lot of out-of-order packets,
                     * we should optimize this */
                    if (ss->ssl3.hs.recvdFragments.buf[OFFSET_BYTE(offset)] &
                        OFFSET_MASK(offset)) {
                        ss->ssl3.hs.recvdHighWater++;
                    } else {
                        break;
                    }
                }

                /* If we have all the bytes, then we are good to go */
                if (ss->ssl3.hs.recvdHighWater == ss->ssl3.hs.msg_len) {
                    ss->ssl3.hs.recvdHighWater = -1;

                    rv = ssl3_HandleHandshakeMessage(ss,
                                                     ss->ssl3.hs.msg_body.buf,
                                                     ss->ssl3.hs.msg_len);
                    if (rv == SECFailure)
                        break; /* Skip rest of record */

                    /* At this point we are advancing our state machine, so
                     * we can free our last flight of messages */
                    dtls_FreeHandshakeMessages(&ss->ssl3.hs.lastMessageFlight);
                    dtls_CancelTimer(ss);

                    /* If there have been no retries this time, reset the
                     * timer value to the default per Section 4.2.4.1 */
                    if (ss->ssl3.hs.rtRetries == 0) {
                        ss->ssl3.hs.rtTimeoutMs = INITIAL_DTLS_TIMEOUT_MS;
                    }
                }
            }
        }

        buf.buf += fragment_length;
        buf.len -= fragment_length;
    }

    origBuf->len = 0;   /* So ssl3_GatherAppDataRecord will keep looping. */

    /* XXX OK for now. In future handle rv == SECWouldBlock safely in order
     * to deal with asynchronous certificate verification */
    return rv;
}

/* Enqueue a message (either handshake or CCS)
 *
 * Called from:
 *              dtls_StageHandshakeMessage()
 *              ssl3_SendChangeCipherSpecs()
 */
SECStatus dtls_QueueMessage(sslSocket *ss, SSL3ContentType type,
    const SSL3Opaque *pIn, PRInt32 nIn)
{
    SECStatus rv = SECSuccess;
    DTLSQueuedMessage *msg = NULL;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    msg = dtls_AllocQueuedMessage(ss->ssl3.cwSpec->epoch, type, pIn, nIn);

    if (!msg) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        rv = SECFailure;
    } else {
        PR_APPEND_LINK(&msg->link, &ss->ssl3.hs.lastMessageFlight);
    }

    return rv;
}

/* Add DTLS handshake message to the pending queue
 * Empty the sendBuf buffer.
 * This function returns SECSuccess or SECFailure, never SECWouldBlock.
 * Always set sendBuf.len to 0, even when returning SECFailure.
 *
 * Called from:
 *              ssl3_AppendHandshakeHeader()
 *              dtls_FlushHandshake()
 */
SECStatus
dtls_StageHandshakeMessage(sslSocket *ss)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    /* This function is sometimes called when no data is actually to
     * be staged, so just return SECSuccess. */
    if (!ss->sec.ci.sendBuf.buf || !ss->sec.ci.sendBuf.len)
        return rv;

    rv = dtls_QueueMessage(ss, content_handshake,
                           ss->sec.ci.sendBuf.buf, ss->sec.ci.sendBuf.len);

    /* Whether we succeeded or failed, toss the old handshake data. */
    ss->sec.ci.sendBuf.len = 0;
    return rv;
}

/* Enqueue the handshake message in sendBuf (if any) and then
 * transmit the resulting flight of handshake messages.
 *
 * Called from:
 *              ssl3_FlushHandshake()
 */
SECStatus
dtls_FlushHandshakeMessages(sslSocket *ss, PRInt32 flags)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    rv = dtls_StageHandshakeMessage(ss);
    if (rv != SECSuccess)
        return rv;

    if (!(flags & ssl_SEND_FLAG_FORCE_INTO_BUFFER)) {
        rv = dtls_TransmitMessageFlight(ss);
        if (rv != SECSuccess)
            return rv;

        if (!(flags & ssl_SEND_FLAG_NO_RETRANSMIT)) {
            ss->ssl3.hs.rtRetries = 0;
            rv = dtls_StartTimer(ss, dtls_RetransmitTimerExpiredCb);
        }
    }

    return rv;
}

/* The callback for when the retransmit timer expires
 *
 * Called from:
 *              dtls_CheckTimer()
 *              dtls_HandleHandshake()
 */
static void
dtls_RetransmitTimerExpiredCb(sslSocket *ss)
{
    SECStatus rv = SECFailure;

    ss->ssl3.hs.rtRetries++;

    if (!(ss->ssl3.hs.rtRetries % 3)) {
        /* If one of the messages was potentially greater than > MTU,
         * then downgrade. Do this every time we have retransmitted a
         * message twice, per RFC 6347 Sec. 4.1.1 */
        dtls_SetMTU(ss, ss->ssl3.hs.maxMessageSent - 1);
    }

    rv = dtls_TransmitMessageFlight(ss);
    if (rv == SECSuccess) {

        /* Re-arm the timer */
        rv = dtls_RestartTimer(ss, PR_TRUE, dtls_RetransmitTimerExpiredCb);
    }

    if (rv == SECFailure) {
        /* XXX OK for now. In future maybe signal the stack that we couldn't
         * transmit. For now, let the read handle any real network errors */
    }
}

/* Transmit a flight of handshake messages, stuffing them
 * into as few records as seems reasonable
 *
 * Called from:
 *             dtls_FlushHandshake()
 *             dtls_RetransmitTimerExpiredCb()
 */
static SECStatus
dtls_TransmitMessageFlight(sslSocket *ss)
{
    SECStatus rv = SECSuccess;
    PRCList *msg_p;
    PRUint16 room_left = ss->ssl3.mtu;
    PRInt32 sent;

    ssl_GetXmitBufLock(ss);
    ssl_GetSpecReadLock(ss);

    /* DTLS does not buffer its handshake messages in
     * ss->pendingBuf, but rather in the lastMessageFlight
     * structure. This is just a sanity check that
     * some programming error hasn't inadvertantly
     * stuffed something in ss->pendingBuf
     */
    PORT_Assert(!ss->pendingBuf.len);
    for (msg_p = PR_LIST_HEAD(&ss->ssl3.hs.lastMessageFlight);
         msg_p != &ss->ssl3.hs.lastMessageFlight;
         msg_p = PR_NEXT_LINK(msg_p)) {
        DTLSQueuedMessage *msg = (DTLSQueuedMessage *)msg_p;

        /* The logic here is:
         *
         * 1. If this is a message that will not fit into the remaining
         *    space, then flush.
         * 2. If the message will now fit into the remaining space,
         *    encrypt, buffer, and loop.
         * 3. If the message will not fit, then fragment.
         *
         * At the end of the function, flush.
         */
        if ((msg->len + SSL3_BUFFER_FUDGE) > room_left) {
            /* The message will not fit into the remaining space, so flush */
            rv = dtls_SendSavedWriteData(ss);
            if (rv != SECSuccess)
                break;

            room_left = ss->ssl3.mtu;
        }

        if ((msg->len + SSL3_BUFFER_FUDGE) <= room_left) {
            /* The message will fit, so encrypt and then continue with the
             * next packet */
            sent = ssl3_SendRecord(ss, msg->epoch, msg->type,
                                   msg->data, msg->len,
                                   ssl_SEND_FLAG_FORCE_INTO_BUFFER |
                                   ssl_SEND_FLAG_USE_EPOCH);
            if (sent != msg->len) {
                rv = SECFailure;
                if (sent != -1) {
                    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                }
                break;
            }

            room_left = ss->ssl3.mtu - ss->pendingBuf.len;
        } else {
            /* The message will not fit, so fragment.
             *
             * XXX OK for now. Arrange to coalesce the last fragment
             * of this message with the next message if possible.
             * That would be more efficient.
             */
            PRUint32 fragment_offset = 0;
            unsigned char fragment[DTLS_MAX_MTU]; /* >= than largest
                                                   * plausible MTU */

            /* Assert that we have already flushed */
            PORT_Assert(room_left == ss->ssl3.mtu);

            /* Case 3: We now need to fragment this message
             * DTLS only supports fragmenting handshaking messages */
            PORT_Assert(msg->type == content_handshake);

            /* The headers consume 12 bytes so the smalles possible
             *  message (i.e., an empty one) is 12 bytes
             */
            PORT_Assert(msg->len >= 12);

            while ((fragment_offset + 12) < msg->len) {
                PRUint32 fragment_len;
                const unsigned char *content = msg->data + 12;
                PRUint32 content_len = msg->len - 12;

                /* The reason we use 8 here is that that's the length of
                 * the new DTLS data that we add to the header */
                fragment_len = PR_MIN((PRUint32)room_left - (SSL3_BUFFER_FUDGE + 8),
                                      content_len - fragment_offset);
                PORT_Assert(fragment_len < DTLS_MAX_MTU - 12);
                /* Make totally sure that we are within the buffer.
                 * Note that the only way that fragment len could get
                 * adjusted here is if
                 *
                 * (a) we are in release mode so the PORT_Assert is compiled out
                 * (b) either the MTU table is inconsistent with DTLS_MAX_MTU
                 * or ss->ssl3.mtu has become corrupt.
                 */
                fragment_len = PR_MIN(fragment_len, DTLS_MAX_MTU - 12);

                /* Construct an appropriate-sized fragment */
                /* Type, length, sequence */
                PORT_Memcpy(fragment, msg->data, 6);

                /* Offset */
                fragment[6] = (fragment_offset >> 16) & 0xff;
                fragment[7] = (fragment_offset >> 8) & 0xff;
                fragment[8] = (fragment_offset) & 0xff;

                /* Fragment length */
                fragment[9] = (fragment_len >> 16) & 0xff;
                fragment[10] = (fragment_len >> 8) & 0xff;
                fragment[11] = (fragment_len) & 0xff;

                PORT_Memcpy(fragment + 12, content + fragment_offset,
                            fragment_len);

                /*
                 *  Send the record. We do this in two stages
                 * 1. Encrypt
                 */
                sent = ssl3_SendRecord(ss, msg->epoch, msg->type,
                                       fragment, fragment_len + 12,
                                       ssl_SEND_FLAG_FORCE_INTO_BUFFER |
                                       ssl_SEND_FLAG_USE_EPOCH);
                if (sent != (fragment_len + 12)) {
                    rv = SECFailure;
                    if (sent != -1) {
                        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                    }
                    break;
                }

                /* 2. Flush */
                rv = dtls_SendSavedWriteData(ss);
                if (rv != SECSuccess)
                    break;

                fragment_offset += fragment_len;
            }
        }
    }

    /* Finally, we need to flush */
    if (rv == SECSuccess)
        rv = dtls_SendSavedWriteData(ss);

    /* Give up the locks */
    ssl_ReleaseSpecReadLock(ss);
    ssl_ReleaseXmitBufLock(ss);

    return rv;
}

/* Flush the data in the pendingBuf and update the max message sent
 * so we can adjust the MTU estimate if we need to.
 * Wrapper for ssl_SendSavedWriteData.
 *
 * Called from dtls_TransmitMessageFlight()
 */
static
SECStatus dtls_SendSavedWriteData(sslSocket *ss)
{
    PRInt32 sent;

    sent = ssl_SendSavedWriteData(ss);
    if (sent < 0)
        return SECFailure;

    /* We should always have complete writes b/c datagram sockets
     * don't really block */
    if (ss->pendingBuf.len > 0) {
        ssl_MapLowLevelError(SSL_ERROR_SOCKET_WRITE_FAILURE);
        return SECFailure;
    }

    /* Update the largest message sent so we can adjust the MTU
     * estimate if necessary */
    if (sent > ss->ssl3.hs.maxMessageSent)
        ss->ssl3.hs.maxMessageSent = sent;

    return SECSuccess;
}

/* Compress, MAC, encrypt a DTLS record. Allows specification of
 * the epoch using epoch value. If use_epoch is PR_TRUE then
 * we use the provided epoch. If use_epoch is PR_FALSE then
 * whatever the current value is in effect is used.
 *
 * Called from ssl3_SendRecord()
 */
SECStatus
dtls_CompressMACEncryptRecord(sslSocket *        ss,
                              DTLSEpoch          epoch,
                              PRBool             use_epoch,
                              SSL3ContentType    type,
                              const SSL3Opaque * pIn,
                              PRUint32           contentLen,
                              sslBuffer        * wrBuf)
{
    SECStatus rv = SECFailure;
    ssl3CipherSpec *          cwSpec;

    ssl_GetSpecReadLock(ss);    /********************************/

    /* The reason for this switch-hitting code is that we might have
     * a flight of records spanning an epoch boundary, e.g.,
     *
     * ClientKeyExchange (epoch = 0)
     * ChangeCipherSpec (epoch = 0)
     * Finished (epoch = 1)
     *
     * Thus, each record needs a different cipher spec. The information
     * about which epoch to use is carried with the record.
     */
    if (use_epoch) {
        if (ss->ssl3.cwSpec->epoch == epoch)
            cwSpec = ss->ssl3.cwSpec;
        else if (ss->ssl3.pwSpec->epoch == epoch)
            cwSpec = ss->ssl3.pwSpec;
        else
            cwSpec = NULL;
    } else {
        cwSpec = ss->ssl3.cwSpec;
    }

    if (cwSpec) {
        rv = ssl3_CompressMACEncryptRecord(cwSpec, ss->sec.isServer, PR_TRUE,
                                           PR_FALSE, type, pIn, contentLen,
                                           wrBuf);
    } else {
        PR_NOT_REACHED("Couldn't find a cipher spec matching epoch");
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    }
    ssl_ReleaseSpecReadLock(ss); /************************************/

    return rv;
}

/* Start a timer
 *
 * Called from:
 *             dtls_HandleHandshake()
 *             dtls_FlushHAndshake()
 *             dtls_RestartTimer()
 */
SECStatus
dtls_StartTimer(sslSocket *ss, DTLSTimerCb cb)
{
    PORT_Assert(ss->ssl3.hs.rtTimerCb == NULL);

    ss->ssl3.hs.rtTimerStarted = PR_IntervalNow();
    ss->ssl3.hs.rtTimerCb = cb;

    return SECSuccess;
}

/* Restart a timer with optional backoff
 *
 * Called from dtls_RetransmitTimerExpiredCb()
 */
SECStatus
dtls_RestartTimer(sslSocket *ss, PRBool backoff, DTLSTimerCb cb)
{
    if (backoff) {
        ss->ssl3.hs.rtTimeoutMs *= 2;
        if (ss->ssl3.hs.rtTimeoutMs > MAX_DTLS_TIMEOUT_MS)
            ss->ssl3.hs.rtTimeoutMs = MAX_DTLS_TIMEOUT_MS;
    }

    return dtls_StartTimer(ss, cb);
}

/* Cancel a pending timer
 *
 * Called from:
 *              dtls_HandleHandshake()
 *              dtls_CheckTimer()
 */
void
dtls_CancelTimer(sslSocket *ss)
{
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));

    ss->ssl3.hs.rtTimerCb = NULL;
}

/* Check the pending timer and fire the callback if it expired
 *
 * Called from ssl3_GatherCompleteHandshake()
 */
void
dtls_CheckTimer(sslSocket *ss)
{
    if (!ss->ssl3.hs.rtTimerCb)
        return;

    if ((PR_IntervalNow() - ss->ssl3.hs.rtTimerStarted) >
        PR_MillisecondsToInterval(ss->ssl3.hs.rtTimeoutMs)) {
        /* Timer has expired */
        DTLSTimerCb cb = ss->ssl3.hs.rtTimerCb;

        /* Cancel the timer so that we can call the CB safely */
        dtls_CancelTimer(ss);

        /* Now call the CB */
        cb(ss);
    }
}

/* The callback to fire when the holddown timer for the Finished
 * message expires and we can delete it
 *
 * Called from dtls_CheckTimer()
 */
void
dtls_FinishedTimerCb(sslSocket *ss)
{
    ssl3_DestroyCipherSpec(ss->ssl3.pwSpec, PR_FALSE);
}

/* Cancel the Finished hold-down timer and destroy the
 * pending cipher spec. Note that this means that
 * successive rehandshakes will fail if the Finished is
 * lost.
 *
 * XXX OK for now. Figure out how to handle the combination
 * of Finished lost and rehandshake
 */
void
dtls_RehandshakeCleanup(sslSocket *ss)
{
    dtls_CancelTimer(ss);
    ssl3_DestroyCipherSpec(ss->ssl3.pwSpec, PR_FALSE);
    ss->ssl3.hs.sendMessageSeq = 0;
    ss->ssl3.hs.recvMessageSeq = 0;
}

/* Set the MTU to the next step less than or equal to the
 * advertised value. Also used to downgrade the MTU by
 * doing dtls_SetMTU(ss, biggest packet set).
 *
 * Passing 0 means set this to the largest MTU known
 * (effectively resetting the PMTU backoff value).
 *
 * Called by:
 *            ssl3_InitState()
 *            dtls_RetransmitTimerExpiredCb()
 */
void
dtls_SetMTU(sslSocket *ss, PRUint16 advertised)
{
    int i;

    if (advertised == 0) {
        ss->ssl3.mtu = COMMON_MTU_VALUES[0];
        SSL_TRC(30, ("Resetting MTU to %d", ss->ssl3.mtu));
        return;
    }

    for (i = 0; i < PR_ARRAY_SIZE(COMMON_MTU_VALUES); i++) {
        if (COMMON_MTU_VALUES[i] <= advertised) {
            ss->ssl3.mtu = COMMON_MTU_VALUES[i];
            SSL_TRC(30, ("Resetting MTU to %d", ss->ssl3.mtu));
            return;
        }
    }

    /* Fallback */
    ss->ssl3.mtu = COMMON_MTU_VALUES[PR_ARRAY_SIZE(COMMON_MTU_VALUES)-1];
    SSL_TRC(30, ("Resetting MTU to %d", ss->ssl3.mtu));
}

/* Called from ssl3_HandleHandshakeMessage() when it has deciphered a
 * DTLS hello_verify_request
 * Caller must hold Handshake and RecvBuf locks.
 */
SECStatus
dtls_HandleHelloVerifyRequest(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    int                 errCode = SSL_ERROR_RX_MALFORMED_HELLO_VERIFY_REQUEST;
    SECStatus           rv;
    PRInt32             temp;
    SECItem             cookie = {siBuffer, NULL, 0};
    SSL3AlertDescription desc   = illegal_parameter;

    SSL_TRC(3, ("%d: SSL3[%d]: handle hello_verify_request handshake",
        SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->ssl3.hs.ws != wait_server_hello) {
        errCode = SSL_ERROR_RX_UNEXPECTED_HELLO_VERIFY_REQUEST;
        desc    = unexpected_message;
        goto alert_loser;
    }

    /* The version */
    temp = ssl3_ConsumeHandshakeNumber(ss, 2, &b, &length);
    if (temp < 0) {
        goto loser;     /* alert has been sent */
    }

    if (temp != SSL_LIBRARY_VERSION_DTLS_1_0_WIRE &&
        temp != SSL_LIBRARY_VERSION_DTLS_1_2_WIRE) {
        goto alert_loser;
    }

    /* The cookie */
    rv = ssl3_ConsumeHandshakeVariable(ss, &cookie, 1, &b, &length);
    if (rv != SECSuccess) {
        goto loser;     /* alert has been sent */
    }
    if (cookie.len > DTLS_COOKIE_BYTES) {
        desc = decode_error;
        goto alert_loser;       /* malformed. */
    }

    PORT_Memcpy(ss->ssl3.hs.cookie, cookie.data, cookie.len);
    ss->ssl3.hs.cookieLen = cookie.len;


    ssl_GetXmitBufLock(ss);             /*******************************/

    /* Now re-send the client hello */
    rv = ssl3_SendClientHello(ss, PR_TRUE);

    ssl_ReleaseXmitBufLock(ss);         /*******************************/

    if (rv == SECSuccess)
        return rv;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);

loser:
    errCode = ssl_MapLowLevelError(errCode);
    return SECFailure;
}

/* Initialize the DTLS anti-replay window
 *
 * Called from:
 *              ssl3_SetupPendingCipherSpec()
 *              ssl3_InitCipherSpec()
 */
void
dtls_InitRecvdRecords(DTLSRecvdRecords *records)
{
    PORT_Memset(records->data, 0, sizeof(records->data));
    records->left = 0;
    records->right = DTLS_RECVD_RECORDS_WINDOW - 1;
}

/*
 * Has this DTLS record been received? Return values are:
 * -1 -- out of range to the left
 *  0 -- not received yet
 *  1 -- replay
 *
 *  Called from: dtls_HandleRecord()
 */
int
dtls_RecordGetRecvd(DTLSRecvdRecords *records, PRUint64 seq)
{
    PRUint64 offset;

    /* Out of range to the left */
    if (seq < records->left) {
        return -1;
    }

    /* Out of range to the right; since we advance the window on
     * receipt, that means that this packet has not been received
     * yet */
    if (seq > records->right)
        return 0;

    offset = seq % DTLS_RECVD_RECORDS_WINDOW;

    return !!(records->data[offset / 8] & (1 << (offset % 8)));
}

/* Update the DTLS anti-replay window
 *
 * Called from ssl3_HandleRecord()
 */
void
dtls_RecordSetRecvd(DTLSRecvdRecords *records, PRUint64 seq)
{
    PRUint64 offset;

    if (seq < records->left)
        return;

    if (seq > records->right) {
        PRUint64 new_left;
        PRUint64 new_right;
        PRUint64 right;

        /* Slide to the right; this is the tricky part
         *
         * 1. new_top is set to have room for seq, on the
         *    next byte boundary by setting the right 8
         *    bits of seq
         * 2. new_left is set to compensate.
         * 3. Zero all bits between top and new_top. Since
         *    this is a ring, this zeroes everything as-yet
         *    unseen. Because we always operate on byte
         *    boundaries, we can zero one byte at a time
         */
        new_right = seq | 0x07;
        new_left = (new_right - DTLS_RECVD_RECORDS_WINDOW) + 1;

        for (right = records->right + 8; right <= new_right; right += 8) {
            offset = right % DTLS_RECVD_RECORDS_WINDOW;
            records->data[offset / 8] = 0;
        }

        records->right = new_right;
        records->left = new_left;
    }

    offset = seq % DTLS_RECVD_RECORDS_WINDOW;

    records->data[offset / 8] |= (1 << (offset % 8));
}

SECStatus
DTLS_GetHandshakeTimeout(PRFileDesc *socket, PRIntervalTime *timeout)
{
    sslSocket * ss = NULL;
    PRIntervalTime elapsed;
    PRIntervalTime desired;

    ss = ssl_FindSocket(socket);

    if (!ss)
        return SECFailure;

    if (!IS_DTLS(ss))
        return SECFailure;

    if (!ss->ssl3.hs.rtTimerCb)
        return SECFailure;

    elapsed = PR_IntervalNow() - ss->ssl3.hs.rtTimerStarted;
    desired = PR_MillisecondsToInterval(ss->ssl3.hs.rtTimeoutMs);
    if (elapsed > desired) {
        /* Timer expired */
        *timeout = PR_INTERVAL_NO_WAIT;
    } else {
        *timeout = desired - elapsed;
    }

    return SECSuccess;
}
