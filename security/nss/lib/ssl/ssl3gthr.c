/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Gather (Read) entire SSL3 records from socket into buffer.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "ssl3prot.h"

struct ssl2GatherStr {
    /* true when ssl3_GatherData encounters an SSLv2 handshake */
    PRBool isV2;

    /* number of bytes of padding appended to the message content */
    PRUint8 padding;
};

typedef struct ssl2GatherStr ssl2Gather;

/* Caller should hold RecvBufLock. */
SECStatus
ssl3_InitGather(sslGather *gs)
{
    SECStatus status;

    gs->state = GS_INIT;
    gs->writeOffset = 0;
    gs->readOffset = 0;
    gs->dtlsPacketOffset = 0;
    gs->dtlsPacket.len = 0;
    gs->rejectV2Records = PR_FALSE;
    status = sslBuffer_Grow(&gs->buf, 4096);
    return status;
}

/* Caller must hold RecvBufLock. */
void
ssl3_DestroyGather(sslGather *gs)
{
    if (gs) { /* the PORT_*Free functions check for NULL pointers. */
        PORT_ZFree(gs->buf.buf, gs->buf.space);
        PORT_Free(gs->inbuf.buf);
        PORT_Free(gs->dtlsPacket.buf);
    }
}

/* Checks whether a given buffer is likely an SSLv3 record header.  */
PRBool
ssl3_isLikelyV3Hello(const unsigned char *buf)
{
    /* Even if this was a V2 record header we couldn't possibly parse it
     * correctly as the second bit denotes a vaguely-defined security escape. */
    if (buf[0] & 0x40) {
        return PR_TRUE;
    }

    /* Check for a typical V3 record header. */
    return (PRBool)(buf[0] >= ssl_ct_change_cipher_spec &&
                    buf[0] <= ssl_ct_application_data &&
                    buf[1] == MSB(SSL_LIBRARY_VERSION_3_0));
}

/*
 * Attempt to read in an entire SSL3 record.
 * Blocks here for blocking sockets, otherwise returns -1 with
 *  PR_WOULD_BLOCK_ERROR when socket would block.
 *
 * returns  1 if received a complete SSL3 record.
 * returns  0 if recv returns EOF
 * returns -1 if recv returns < 0
 *  (The error value may have already been set to PR_WOULD_BLOCK_ERROR)
 *
 * Caller must hold the recv buf lock.
 *
 * The Gather state machine has 3 states:  GS_INIT, GS_HEADER, GS_DATA.
 * GS_HEADER: waiting for the 5-byte SSL3 record header to come in.
 * GS_DATA:   waiting for the body of the SSL3 record   to come in.
 *
 * This loop returns when either
 *      (a) an error or EOF occurs,
 *  (b) PR_WOULD_BLOCK_ERROR,
 *  (c) data (entire SSL3 record) has been received.
 */
static int
ssl3_GatherData(sslSocket *ss, sslGather *gs, int flags, ssl2Gather *ssl2gs)
{
    unsigned char *bp;
    unsigned char *lbp;
    int nb;
    int err;
    int rv = 1;
    PRUint8 v2HdrLength = 0;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    if (gs->state == GS_INIT) {
        gs->state = GS_HEADER;
        gs->remainder = 5;
        gs->offset = 0;
        gs->writeOffset = 0;
        gs->readOffset = 0;
        gs->inbuf.len = 0;
    }

    lbp = gs->inbuf.buf;
    for (;;) {
        SSL_TRC(30, ("%d: SSL3[%d]: gather state %d (need %d more)",
                     SSL_GETPID(), ss->fd, gs->state, gs->remainder));
        bp = ((gs->state != GS_HEADER) ? lbp : gs->hdr) + gs->offset;
        nb = ssl_DefRecv(ss, bp, gs->remainder, flags);

        if (nb > 0) {
            PRINT_BUF(60, (ss, "raw gather data:", bp, nb));
        } else if (nb == 0) {
            /* EOF */
            SSL_TRC(30, ("%d: SSL3[%d]: EOF", SSL_GETPID(), ss->fd));
            rv = 0;
            break;
        } else /* if (nb < 0) */ {
            SSL_DBG(("%d: SSL3[%d]: recv error %d", SSL_GETPID(), ss->fd,
                     PR_GetError()));
            rv = SECFailure;
            break;
        }

        PORT_Assert((unsigned int)nb <= gs->remainder);
        if ((unsigned int)nb > gs->remainder) {
            /* ssl_DefRecv is misbehaving!  this error is fatal to SSL. */
            gs->state = GS_INIT; /* so we don't crash next time */
            rv = SECFailure;
            break;
        }

        gs->offset += nb;
        gs->remainder -= nb;
        if (gs->state == GS_DATA)
            gs->inbuf.len += nb;

        /* if there's more to go, read some more. */
        if (gs->remainder > 0) {
            continue;
        }

        /* have received entire record header, or entire record. */
        switch (gs->state) {
            case GS_HEADER:
                /* Check for SSLv2 handshakes. Always assume SSLv3 on clients,
                 * support SSLv2 handshakes only when ssl2gs != NULL.
                 * Always assume v3 after we received the first record. */
                if (!ssl2gs ||
                    ss->gs.rejectV2Records ||
                    ssl3_isLikelyV3Hello(gs->hdr)) {
                    /* Should have a non-SSLv2 record header in gs->hdr. Extract
                     * the length of the following encrypted data, and then
                     * read in the rest of the record into gs->inbuf. */
                    gs->remainder = (gs->hdr[3] << 8) | gs->hdr[4];
                    gs->hdrLen = SSL3_RECORD_HEADER_LENGTH;
                } else {
                    /* Probably an SSLv2 record header. No need to handle any
                     * security escapes (gs->hdr[0] & 0x40) as we wouldn't get
                     * here if one was set. See ssl3_isLikelyV3Hello(). */
                    gs->remainder = ((gs->hdr[0] & 0x7f) << 8) | gs->hdr[1];
                    ssl2gs->isV2 = PR_TRUE;
                    v2HdrLength = 2;

                    /* Is it a 3-byte header with padding? */
                    if (!(gs->hdr[0] & 0x80)) {
                        ssl2gs->padding = gs->hdr[2];
                        v2HdrLength++;
                    }
                }

                /* If it is NOT an SSLv2 header */
                if (!v2HdrLength) {
                    /* Check if default RFC specified max ciphertext/record
                     * limits are respected. Checks for used record size limit
                     * extension boundaries are done in
                     * ssl3con.c/ssl3_HandleRecord() for tls and dtls records.
                     *
                     * -> For TLS 1.2 records MUST NOT be longer than
                     * 2^14 + 2048 bytes.
                     * -> For TLS 1.3 records MUST NOT exceed 2^14 + 256 bytes.
                     * -> For older versions this MAY be enforced, we do it.
                     * [RFC8446 Section 5.2, RFC5246 Section 6.2.3]. */
                    if (gs->remainder > TLS_1_2_MAX_CTEXT_LENGTH ||
                        (gs->remainder > TLS_1_3_MAX_CTEXT_LENGTH &&
                         ss->version >= SSL_LIBRARY_VERSION_TLS_1_3)) {
                        SSL3_SendAlert(ss, alert_fatal, record_overflow);
                        gs->state = GS_INIT;
                        PORT_SetError(SSL_ERROR_RX_RECORD_TOO_LONG);
                        return SECFailure;
                    }
                }

                gs->state = GS_DATA;
                gs->offset = 0;
                gs->inbuf.len = 0;

                if (gs->remainder > gs->inbuf.space) {
                    err = sslBuffer_Grow(&gs->inbuf, gs->remainder);
                    if (err) { /* realloc has set error code to no mem. */
                        return err;
                    }
                    lbp = gs->inbuf.buf;
                }

                /* When we encounter an SSLv2 hello we've read 2 or 3 bytes too
                 * many into the gs->hdr[] buffer. Copy them over into inbuf so
                 * that we can properly process the hello record later. */
                if (v2HdrLength) {
                    /* Reject v2 records that don't even carry enough data to
                     * resemble a valid ClientHello header. */
                    if (gs->remainder < SSL_HL_CLIENT_HELLO_HBYTES) {
                        SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
                        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
                        return SECFailure;
                    }

                    PORT_Assert(lbp);
                    gs->inbuf.len = 5 - v2HdrLength;
                    PORT_Memcpy(lbp, gs->hdr + v2HdrLength, gs->inbuf.len);
                    gs->remainder -= gs->inbuf.len;
                    lbp += gs->inbuf.len;
                }

                if (gs->remainder > 0) {
                    break; /* End this case.  Continue around the loop. */
                }

            /* FALL THROUGH if (gs->remainder == 0) as we just received
                 * an empty record and there's really no point in calling
                 * ssl_DefRecv() with buf=NULL and len=0. */

            case GS_DATA:
                /*
                ** SSL3 record has been completely received.
                */
                SSL_TRC(10, ("%d: SSL[%d]: got record of %d bytes",
                             SSL_GETPID(), ss->fd, gs->inbuf.len));

                /* reject any v2 records from now on */
                ss->gs.rejectV2Records = PR_TRUE;

                gs->state = GS_INIT;
                return 1;
        }
    }

    return rv;
}

/*
 * Read in an entire DTLS record.
 *
 * Blocks here for blocking sockets, otherwise returns -1 with
 *  PR_WOULD_BLOCK_ERROR when socket would block.
 *
 * This is simpler than SSL because we are reading on a datagram socket
 * and datagrams must contain >=1 complete records.
 *
 * returns  1 if received a complete DTLS record.
 * returns  0 if recv returns EOF
 * returns -1 if recv returns < 0
 *  (The error value may have already been set to PR_WOULD_BLOCK_ERROR)
 *
 * Caller must hold the recv buf lock.
 *
 * This loop returns when either
 *      (a) an error or EOF occurs,
 *  (b) PR_WOULD_BLOCK_ERROR,
 *  (c) data (entire DTLS record) has been received.
 */
static int
dtls_GatherData(sslSocket *ss, sslGather *gs, int flags)
{
    int nb;
    PRUint8 contentType;
    unsigned int headerLen;
    SECStatus rv = SECSuccess;
    PRBool dtlsLengthPresent = PR_TRUE;

    SSL_TRC(30, ("dtls_GatherData"));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));

    gs->state = GS_HEADER;
    gs->offset = 0;

    if (gs->dtlsPacketOffset == gs->dtlsPacket.len) { /* No data left */
        gs->dtlsPacketOffset = 0;
        gs->dtlsPacket.len = 0;

        /* Resize to the maximum possible size so we can fit a full datagram.
         * This leads to record_overflow errors if records/ciphertexts greater
         * than the buffer (= maximum record) size are to be received.
         * DTLS Record errors are dropped silently. [RFC6347, Section 4.1.2.7].
         * Checks for record size limit extension boundaries are performed in
         * ssl3con.c/ssl3_HandleRecord() for tls and dtls records.
         *
         * -> For TLS 1.2 records MUST NOT be longer than 2^14 + 2048 bytes.
         * -> For TLS 1.3 records MUST NOT exceed 2^14 + 256 bytes.
         * -> For older versions this MAY be enforced, we do it.
         * [RFC8446 Section 5.2, RFC5246 Section 6.2.3]. */
        if (ss->version <= SSL_LIBRARY_VERSION_TLS_1_2) {
            if (gs->dtlsPacket.space < DTLS_1_2_MAX_PACKET_LENGTH) {
                rv = sslBuffer_Grow(&gs->dtlsPacket, DTLS_1_2_MAX_PACKET_LENGTH);
            }
        } else { /* version >= TLS 1.3 */
            if (gs->dtlsPacket.space != DTLS_1_3_MAX_PACKET_LENGTH) {
                /* During Hello and version negotiation older DTLS versions with
                 * greater possible packets are used. The buffer must therefore
                 * be "truncated" by clearing and reallocating it */
                sslBuffer_Clear(&gs->dtlsPacket);
                rv = sslBuffer_Grow(&gs->dtlsPacket, DTLS_1_3_MAX_PACKET_LENGTH);
            }
        }

        if (rv != SECSuccess) {
            return -1; /* Code already set. */
        }

        /* recv() needs to read a full datagram at a time */
        nb = ssl_DefRecv(ss, gs->dtlsPacket.buf, gs->dtlsPacket.space, flags);
        if (nb > 0) {
            PRINT_BUF(60, (ss, "raw gather data:", gs->dtlsPacket.buf, nb));
        } else if (nb == 0) {
            /* EOF */
            SSL_TRC(30, ("%d: SSL3[%d]: EOF", SSL_GETPID(), ss->fd));
            return 0;
        } else /* if (nb < 0) */ {
            SSL_DBG(("%d: SSL3[%d]: recv error %d", SSL_GETPID(), ss->fd,
                     PR_GetError()));
            /* DTLS Record Errors, including overlong records, are silently
             * dropped [RFC6347, Section 4.1.2.7]. */
            return -1;
        }

        gs->dtlsPacket.len = nb;
    }

    contentType = gs->dtlsPacket.buf[gs->dtlsPacketOffset];
    if (dtls_IsLongHeader(ss->version, contentType)) {
        headerLen = 13;
    } else if (contentType == ssl_ct_application_data) {
        headerLen = 7;
    } else if (dtls_IsDtls13Ciphertext(ss->version, contentType)) {
        /* We don't support CIDs. */
        if (contentType & 0x10) {
            PORT_Assert(PR_FALSE);
            PORT_SetError(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE);
            gs->dtlsPacketOffset = 0;
            gs->dtlsPacket.len = 0;
            return -1;
        }

        dtlsLengthPresent = (contentType & 0x04) == 0x04;
        PRUint8 dtlsSeqNoSize = (contentType & 0x08) ? 2 : 1;
        PRUint8 dtlsLengthBytes = dtlsLengthPresent ? 2 : 0;
        headerLen = 1 + dtlsSeqNoSize + dtlsLengthBytes;
    } else {
        SSL_DBG(("%d: SSL3[%d]: invalid first octet (%d) for DTLS",
                 SSL_GETPID(), ss->fd, contentType));
        PORT_SetError(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE);
        gs->dtlsPacketOffset = 0;
        gs->dtlsPacket.len = 0;
        return -1;
    }

    /* At this point we should have >=1 complete records lined up in
     * dtlsPacket. Read off the header.
     */
    if ((gs->dtlsPacket.len - gs->dtlsPacketOffset) < headerLen) {
        SSL_DBG(("%d: SSL3[%d]: rest of DTLS packet "
                 "too short to contain header",
                 SSL_GETPID(), ss->fd));
        PORT_SetError(PR_WOULD_BLOCK_ERROR);
        gs->dtlsPacketOffset = 0;
        gs->dtlsPacket.len = 0;
        return -1;
    }
    memcpy(gs->hdr, SSL_BUFFER_BASE(&gs->dtlsPacket) + gs->dtlsPacketOffset,
           headerLen);
    gs->hdrLen = headerLen;
    gs->dtlsPacketOffset += headerLen;

    /* Have received SSL3 record header in gs->hdr. */
    if (dtlsLengthPresent) {
        gs->remainder = (gs->hdr[headerLen - 2] << 8) |
                        gs->hdr[headerLen - 1];
    } else {
        gs->remainder = gs->dtlsPacket.len - gs->dtlsPacketOffset;
    }

    if ((gs->dtlsPacket.len - gs->dtlsPacketOffset) < gs->remainder) {
        SSL_DBG(("%d: SSL3[%d]: rest of DTLS packet too short "
                 "to contain rest of body",
                 SSL_GETPID(), ss->fd));
        PORT_SetError(PR_WOULD_BLOCK_ERROR);
        gs->dtlsPacketOffset = 0;
        gs->dtlsPacket.len = 0;
        return -1;
    }

    /* OK, we have at least one complete packet, copy into inbuf */
    gs->inbuf.len = 0;
    rv = sslBuffer_Append(&gs->inbuf,
                          SSL_BUFFER_BASE(&gs->dtlsPacket) + gs->dtlsPacketOffset,
                          gs->remainder);
    if (rv != SECSuccess) {
        return -1; /* code already set. */
    }
    gs->offset = gs->remainder;
    gs->dtlsPacketOffset += gs->remainder;
    gs->state = GS_INIT;

    SSL_TRC(20, ("%d: SSL3[%d]: dtls gathered record type=%d len=%d",
                 SSL_GETPID(), ss->fd, contentType, gs->inbuf.len));
    return 1;
}

/* Gather in a record and when complete, Handle that record.
 * Repeat this until the handshake is complete,
 * or until application data is available.
 *
 * Returns  1 when the handshake is completed without error, or
 *                 application data is available.
 * Returns  0 if ssl3_GatherData hits EOF.
 * Returns -1 on read error, or PR_WOULD_BLOCK_ERROR, or handleRecord error.
 *
 * Called from ssl_GatherRecord1stHandshake       in sslcon.c,
 *    and from SSL_ForceHandshake in sslsecur.c
 *    and from ssl3_GatherAppDataRecord below (<- DoRecv in sslsecur.c).
 *
 * Caller must hold the recv buf lock.
 */
int
ssl3_GatherCompleteHandshake(sslSocket *ss, int flags)
{
    int rv;
    SSL3Ciphertext cText;
    PRBool keepGoing = PR_TRUE;

    if (ss->ssl3.fatalAlertSent) {
        SSL_TRC(3, ("%d: SSL3[%d] Cannot gather data; fatal alert already sent",
                    SSL_GETPID(), ss->fd));
        PORT_SetError(SSL_ERROR_HANDSHAKE_FAILED);
        return -1;
    }

    SSL_TRC(30, ("%d: SSL3[%d]: ssl3_GatherCompleteHandshake",
                 SSL_GETPID(), ss->fd));

    /* ssl3_HandleRecord may end up eventually calling ssl_FinishHandshake,
     * which requires the 1stHandshakeLock, which must be acquired before the
     * RecvBufLock.
     */
    PORT_Assert(ss->opt.noLocks || ssl_Have1stHandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));

    do {
        PRBool processingEarlyData;

        ssl_GetSSL3HandshakeLock(ss);

        processingEarlyData = ss->ssl3.hs.zeroRttState == ssl_0rtt_accepted;

        /* Without this, we may end up wrongly reporting
         * SSL_ERROR_RX_UNEXPECTED_* errors if we receive any records from the
         * peer while we are waiting to be restarted.
         */
        if (ss->ssl3.hs.restartTarget) {
            ssl_ReleaseSSL3HandshakeLock(ss);
            PORT_SetError(PR_WOULD_BLOCK_ERROR);
            return -1;
        }

        /* If we have a detached record layer, don't ever gather. */
        if (ss->recordWriteCallback) {
            PRBool done = ss->firstHsDone;
            ssl_ReleaseSSL3HandshakeLock(ss);
            if (done) {
                return 1;
            }
            PORT_SetError(PR_WOULD_BLOCK_ERROR);
            return -1;
        }

        ssl_ReleaseSSL3HandshakeLock(ss);

        /* State for SSLv2 client hello support. */
        ssl2Gather ssl2gs = { PR_FALSE, 0 };
        ssl2Gather *ssl2gs_ptr = NULL;

        /* If we're a server and waiting for a client hello, accept v2. */
        if (ss->sec.isServer && ss->opt.enableV2CompatibleHello &&
            ss->ssl3.hs.ws == wait_client_hello) {
            ssl2gs_ptr = &ssl2gs;
        }

        /* bring in the next sslv3 record. */
        if (ss->recvdCloseNotify) {
            /* RFC 5246 Section 7.2.1:
             *   Any data received after a closure alert is ignored.
             */
            return 0;
        }

        if (!IS_DTLS(ss)) {
            /* If we're a server waiting for a ClientHello then pass
             * ssl2gs to support SSLv2 ClientHello messages. */
            rv = ssl3_GatherData(ss, &ss->gs, flags, ssl2gs_ptr);
        } else {
            rv = dtls_GatherData(ss, &ss->gs, flags);

            /* If we got a would block error, that means that no data was
             * available, so we check the timer to see if it's time to
             * retransmit */
            if (rv == SECFailure &&
                (PORT_GetError() == PR_WOULD_BLOCK_ERROR)) {
                dtls_CheckTimer(ss);
                /* Restore the error in case something succeeded */
                PORT_SetError(PR_WOULD_BLOCK_ERROR);
            }
        }

        if (rv <= 0) {
            return rv;
        }

        if (ssl2gs.isV2) {
            rv = ssl3_HandleV2ClientHello(ss, ss->gs.inbuf.buf,
                                          ss->gs.inbuf.len,
                                          ssl2gs.padding);
            if (rv < 0) {
                return rv;
            }
        } else {
            /* decipher it, and handle it if it's a handshake.
             * If it's application data, ss->gs.buf will not be empty upon return.
             * If it's a change cipher spec, alert, or handshake message,
             * ss->gs.buf.len will be 0 when ssl3_HandleRecord returns SECSuccess.
             *
             * cText only needs to be valid for this next function call, so
             * it can borrow gs.hdr.
             */
            cText.hdr = ss->gs.hdr;
            cText.hdrLen = ss->gs.hdrLen;
            cText.buf = &ss->gs.inbuf;
            rv = ssl3_HandleRecord(ss, &cText);
        }
        if (rv < 0) {
            return ss->recvdCloseNotify ? 0 : rv;
        }
        if (ss->gs.buf.len > 0) {
            /* We have application data to return to the application. This
             * prioritizes returning application data to the application over
             * completing any renegotiation handshake we may be doing.
             */
            PORT_Assert(ss->firstHsDone);
            break;
        }

        PORT_Assert(keepGoing);
        ssl_GetSSL3HandshakeLock(ss);
        if (ss->ssl3.hs.ws == idle_handshake) {
            /* We are done with the current handshake so stop trying to
             * handshake. Note that it would be safe to test ss->firstHsDone
             * instead of ss->ssl3.hs.ws. By testing ss->ssl3.hs.ws instead,
             * we prioritize completing a renegotiation handshake over sending
             * application data.
             */
            PORT_Assert(ss->firstHsDone);
            PORT_Assert(!ss->ssl3.hs.canFalseStart);
            keepGoing = PR_FALSE;
        } else if (ss->ssl3.hs.canFalseStart) {
            /* Prioritize sending application data over trying to complete
             * the handshake if we're false starting.
             *
             * If we were to do this check at the beginning of the loop instead
             * of here, then this function would become be a no-op after
             * receiving the ServerHelloDone in the false start case, and we
             * would never complete the handshake.
             */
            PORT_Assert(!ss->firstHsDone);

            if (ssl3_WaitingForServerSecondRound(ss)) {
                keepGoing = PR_FALSE;
            } else {
                ss->ssl3.hs.canFalseStart = PR_FALSE;
            }
        } else if (processingEarlyData &&
                   ss->ssl3.hs.zeroRttState == ssl_0rtt_done &&
                   !PR_CLIST_IS_EMPTY(&ss->ssl3.hs.bufferedEarlyData)) {
            /* If we were processing early data and we are no longer, then force
             * the handshake to block.  This ensures that early data is
             * delivered to the application before the handshake completes. */
            ssl_ReleaseSSL3HandshakeLock(ss);
            PORT_SetError(PR_WOULD_BLOCK_ERROR);
            return -1;
        }
        ssl_ReleaseSSL3HandshakeLock(ss);
    } while (keepGoing);

    /* Service the DTLS timer so that the post-handshake timers
     * fire. */
    if (IS_DTLS(ss) && (ss->ssl3.hs.ws == idle_handshake)) {
        dtls_CheckTimer(ss);
    }
    ss->gs.readOffset = 0;
    ss->gs.writeOffset = ss->gs.buf.len;
    return 1;
}

/* Repeatedly gather in a record and when complete, Handle that record.
 * Repeat this until some application data is received.
 *
 * Returns  1 when application data is available.
 * Returns  0 if ssl3_GatherData hits EOF.
 * Returns -1 on read error, or PR_WOULD_BLOCK_ERROR, or handleRecord error.
 *
 * Called from DoRecv in sslsecur.c
 * Caller must hold the recv buf lock.
 */
int
ssl3_GatherAppDataRecord(sslSocket *ss, int flags)
{
    int rv;

    /* ssl3_GatherCompleteHandshake requires both of these locks. */
    PORT_Assert(ss->opt.noLocks || ssl_Have1stHandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));

    do {
        rv = ssl3_GatherCompleteHandshake(ss, flags);
    } while (rv > 0 && ss->gs.buf.len == 0);

    return rv;
}

static SECStatus
ssl_HandleZeroRttRecordData(sslSocket *ss, const PRUint8 *data, unsigned int len)
{
    PORT_Assert(ss->sec.isServer);
    if (ss->ssl3.hs.zeroRttState == ssl_0rtt_accepted) {
        sslBuffer buf = { CONST_CAST(PRUint8, data), len, len, PR_TRUE };
        return tls13_HandleEarlyApplicationData(ss, &buf);
    }
    if (ss->ssl3.hs.zeroRttState == ssl_0rtt_ignored &&
        ss->ssl3.hs.zeroRttIgnore != ssl_0rtt_ignore_none) {
        /* We're ignoring 0-RTT so drop this record quietly. */
        return SECSuccess;
    }
    PORT_SetError(SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA);
    return SECFailure;
}

/* Ensure that application data in the wrong epoch is blocked. */
static PRBool
ssl_IsApplicationDataPermitted(sslSocket *ss, PRUint16 epoch)
{
    /* Epoch 0 is never OK. */
    if (epoch == 0) {
        return PR_FALSE;
    }
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return ss->firstHsDone;
    }
    /* TLS 1.3 application data. */
    if (epoch >= TrafficKeyApplicationData) {
        return ss->firstHsDone;
    }
    /* TLS 1.3 early data is server only. Further checks aren't needed
     * as those are handled in ssl_HandleZeroRttRecordData. */
    if (epoch == TrafficKeyEarlyApplicationData) {
        return ss->sec.isServer;
    }
    return PR_FALSE;
}

SECStatus
SSLExp_RecordLayerData(PRFileDesc *fd, PRUint16 epoch,
                       SSLContentType contentType,
                       const PRUint8 *data, unsigned int len)
{
    SECStatus rv;
    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure;
    }
    if (IS_DTLS(ss) || data == NULL || len == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Run any handshake function.  If SSL_RecordLayerData is the only way that
     * the handshake is driven, then this is necessary to ensure that
     * ssl_BeginClientHandshake or ssl_BeginServerHandshake is called. Note that
     * the other function that might be set to ss->handshake,
     * ssl3_GatherCompleteHandshake, does nothing when this function is used. */
    ssl_Get1stHandshakeLock(ss);
    rv = ssl_Do1stHandshake(ss);
    if (rv != SECSuccess && PORT_GetError() != PR_WOULD_BLOCK_ERROR) {
        goto early_loser; /* Rely on the existing code. */
    }

    if (contentType == ssl_ct_application_data &&
        !ssl_IsApplicationDataPermitted(ss, epoch)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto early_loser;
    }

    /* Then we can validate the epoch. */
    PRErrorCode epochError;
    ssl_GetSpecReadLock(ss);
    if (epoch < ss->ssl3.crSpec->epoch) {
        epochError = SEC_ERROR_INVALID_ARGS; /* Too c/old. */
    } else if (epoch > ss->ssl3.crSpec->epoch) {
        /* If a TLS 1.3 server is not expecting EndOfEarlyData,
         * moving from 1 to 2 is a signal to execute the code
         * as though that message had been received. Let that pass. */
        if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 &&
            ss->opt.suppressEndOfEarlyData &&
            ss->sec.isServer &&
            ss->ssl3.crSpec->epoch == TrafficKeyEarlyApplicationData &&
            epoch == TrafficKeyHandshake) {
            epochError = 0;
        } else {
            epochError = PR_WOULD_BLOCK_ERROR; /* Too warm/new. */
        }
    } else {
        epochError = 0; /* Just right. */
    }
    ssl_ReleaseSpecReadLock(ss);
    if (epochError) {
        PORT_SetError(epochError);
        goto early_loser;
    }

    /* If the handshake is still running, we need to run that. */
    rv = ssl_Do1stHandshake(ss);
    if (rv != SECSuccess && PORT_GetError() != PR_WOULD_BLOCK_ERROR) {
        goto early_loser;
    }

    /* 0-RTT needs its own special handling here. */
    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 &&
        epoch == TrafficKeyEarlyApplicationData &&
        contentType == ssl_ct_application_data) {
        rv = ssl_HandleZeroRttRecordData(ss, data, len);
        ssl_Release1stHandshakeLock(ss);
        return rv;
    }

    /* Finally, save the data... */
    ssl_GetRecvBufLock(ss);
    rv = sslBuffer_Append(&ss->gs.buf, data, len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* ...and process it.  Just saving application data is enough for it to be
     * available to PR_Read(). */
    if (contentType != ssl_ct_application_data) {
        rv = ssl3_HandleNonApplicationData(ss, contentType, 0, 0, &ss->gs.buf);
        /* This occasionally blocks, but that's OK here. */
        if (rv != SECSuccess && PORT_GetError() != PR_WOULD_BLOCK_ERROR) {
            goto loser;
        }
    }

    ssl_ReleaseRecvBufLock(ss);
    ssl_Release1stHandshakeLock(ss);
    return SECSuccess;

loser:
    /* Make sure that any data is not used again. */
    ss->gs.buf.len = 0;
    ssl_ReleaseRecvBufLock(ss);
early_loser:
    ssl_Release1stHandshakeLock(ss);
    return SECFailure;
}

SECStatus
SSLExp_GetCurrentEpoch(PRFileDesc *fd, PRUint16 *readEpoch,
                       PRUint16 *writeEpoch)
{
    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure;
    }

    ssl_GetSpecReadLock(ss);
    if (readEpoch) {
        *readEpoch = ss->ssl3.crSpec->epoch;
    }
    if (writeEpoch) {
        *writeEpoch = ss->ssl3.cwSpec->epoch;
    }
    ssl_ReleaseSpecReadLock(ss);
    return SECSuccess;
}
