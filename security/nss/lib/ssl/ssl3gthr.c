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
    return (PRBool)(buf[0] >= content_change_cipher_spec &&
                    buf[0] <= content_application_data &&
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

                /* This is the max length for an encrypted SSLv3+ fragment. */
                if (!v2HdrLength &&
                    gs->remainder > (MAX_FRAGMENT_LENGTH + 2048)) {
                    SSL3_SendAlert(ss, alert_fatal, record_overflow);
                    gs->state = GS_INIT;
                    PORT_SetError(SSL_ERROR_RX_RECORD_TOO_LONG);
                    return SECFailure;
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
    int err;
    int rv = 1;

    SSL_TRC(30, ("dtls_GatherData"));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));

    gs->state = GS_HEADER;
    gs->offset = 0;

    if (gs->dtlsPacketOffset == gs->dtlsPacket.len) { /* No data left */
        gs->dtlsPacketOffset = 0;
        gs->dtlsPacket.len = 0;

        /* Resize to the maximum possible size so we can fit a full datagram */
        /* This is the max fragment length for an encrypted fragment
        ** plus the size of the record header.
        ** This magic constant is copied from ssl3_GatherData, with 5 changed
        ** to 13 (the size of the record header).
        */
        if (gs->dtlsPacket.space < MAX_FRAGMENT_LENGTH + 2048 + 13) {
            err = sslBuffer_Grow(&gs->dtlsPacket,
                                 MAX_FRAGMENT_LENGTH + 2048 + 13);
            if (err) { /* realloc has set error code to no mem. */
                return err;
            }
        }

        /* recv() needs to read a full datagram at a time */
        nb = ssl_DefRecv(ss, gs->dtlsPacket.buf, gs->dtlsPacket.space, flags);

        if (nb > 0) {
            PRINT_BUF(60, (ss, "raw gather data:", gs->dtlsPacket.buf, nb));
        } else if (nb == 0) {
            /* EOF */
            SSL_TRC(30, ("%d: SSL3[%d]: EOF", SSL_GETPID(), ss->fd));
            rv = 0;
            return rv;
        } else /* if (nb < 0) */ {
            SSL_DBG(("%d: SSL3[%d]: recv error %d", SSL_GETPID(), ss->fd,
                     PR_GetError()));
            rv = SECFailure;
            return rv;
        }

        gs->dtlsPacket.len = nb;
    }

    /* At this point we should have >=1 complete records lined up in
     * dtlsPacket. Read off the header.
     */
    if ((gs->dtlsPacket.len - gs->dtlsPacketOffset) < 13) {
        SSL_DBG(("%d: SSL3[%d]: rest of DTLS packet "
                 "too short to contain header",
                 SSL_GETPID(), ss->fd));
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        gs->dtlsPacketOffset = 0;
        gs->dtlsPacket.len = 0;
        rv = SECFailure;
        return rv;
    }
    memcpy(gs->hdr, gs->dtlsPacket.buf + gs->dtlsPacketOffset, 13);
    gs->dtlsPacketOffset += 13;

    /* Have received SSL3 record header in gs->hdr. */
    gs->remainder = (gs->hdr[11] << 8) | gs->hdr[12];

    if ((gs->dtlsPacket.len - gs->dtlsPacketOffset) < gs->remainder) {
        SSL_DBG(("%d: SSL3[%d]: rest of DTLS packet too short "
                 "to contain rest of body",
                 SSL_GETPID(), ss->fd));
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        gs->dtlsPacketOffset = 0;
        gs->dtlsPacket.len = 0;
        rv = SECFailure;
        return rv;
    }

    /* OK, we have at least one complete packet, copy into inbuf */
    if (gs->remainder > gs->inbuf.space) {
        err = sslBuffer_Grow(&gs->inbuf, gs->remainder);
        if (err) { /* realloc has set error code to no mem. */
            return err;
        }
    }

    SSL_TRC(20, ("%d: SSL3[%d]: dtls gathered record type=%d len=%d",
                 SSL_GETPID(), ss->fd, gs->hdr[0], gs->inbuf.len));

    memcpy(gs->inbuf.buf, gs->dtlsPacket.buf + gs->dtlsPacketOffset,
           gs->remainder);
    gs->inbuf.len = gs->remainder;
    gs->offset = gs->remainder;
    gs->dtlsPacketOffset += gs->remainder;
    gs->state = GS_INIT;

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
 * Returns -2 on SECWouldBlock return from ssl3_HandleRecord.
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
        return SECFailure;
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
        PRBool handleRecordNow = PR_FALSE;
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
            return (int)SECFailure;
        }

        /* Treat an empty msgState like a NULL msgState. (Most of the time
         * when ssl3_HandleHandshake returns SECWouldBlock, it leaves
         * behind a non-NULL but zero-length msgState).
         * Test: async_cert_restart_server_sends_hello_request_first_in_separate_record
         */
        if (ss->ssl3.hs.msgState.buf) {
            if (ss->ssl3.hs.msgState.len == 0) {
                ss->ssl3.hs.msgState.buf = NULL;
            } else {
                handleRecordNow = PR_TRUE;
            }
        }

        ssl_ReleaseSSL3HandshakeLock(ss);

        if (handleRecordNow) {
            /* ssl3_HandleHandshake previously returned SECWouldBlock and the
             * as-yet-unprocessed plaintext of that previous handshake record.
             * We need to process it now before we overwrite it with the next
             * handshake record.
             */
            rv = ssl3_HandleRecord(ss, NULL, &ss->gs.buf);
        } else {
            /* State for SSLv2 client hello support. */
            ssl2Gather ssl2gs = { PR_FALSE, 0 };
            ssl2Gather *ssl2gs_ptr = NULL;

            /* If we're a server and waiting for a client hello, accept v2. */
            if (ss->sec.isServer && ss->ssl3.hs.ws == wait_client_hello) {
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
                 */
                cText.type = (SSL3ContentType)ss->gs.hdr[0];
                cText.version = (ss->gs.hdr[1] << 8) | ss->gs.hdr[2];

                if (IS_DTLS(ss)) {
                    sslSequenceNumber seq_num;

                    /* DTLS sequence number */
                    PORT_Memcpy(&seq_num, &ss->gs.hdr[3], sizeof(seq_num));
                    cText.seq_num = PR_ntohll(seq_num);
                }

                cText.buf = &ss->gs.inbuf;
                rv = ssl3_HandleRecord(ss, &cText, &ss->gs.buf);
            }
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
            PORT_Assert(cText.type == content_application_data);
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
            return SECWouldBlock;
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
 * Returns -2 on SECWouldBlock return from ssl3_HandleRecord.
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
