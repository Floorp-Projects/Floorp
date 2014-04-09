/*
 * Gather (Read) entire SSL2 records from socket into buffer.  
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"

/* Forward static declarations */
static SECStatus ssl2_HandleV3HandshakeRecord(sslSocket *ss);

/*
** Gather a single record of data from the receiving stream. This code
** first gathers the header (2 or 3 bytes long depending on the value of
** the most significant bit in the first byte) then gathers up the data
** for the record into gs->buf. This code handles non-blocking I/O
** and is to be called multiple times until ss->sec.recordLen != 0.
** This function decrypts the gathered record in place, in gs_buf.
 *
 * Caller must hold RecvBufLock. 
 *
 * Returns +1 when it has gathered a complete SSLV2 record.
 * Returns  0 if it hits EOF.
 * Returns -1 (SECFailure)    on any error
 * Returns -2 (SECWouldBlock) when it gathers an SSL v3 client hello header.
**
** The SSL2 Gather State machine has 4 states:
** GS_INIT   - Done reading in previous record.  Haven't begun to read in
**             next record.  When ssl2_GatherData is called with the machine
**             in this state, the machine will attempt to read the first 3
**             bytes of the SSL2 record header, and will advance the state
**             to GS_HEADER.
**
** GS_HEADER - The machine is in this state while waiting for the completion
**             of the first 3 bytes of the SSL2 record.  When complete, the
**             machine will compute the remaining unread length of this record
**             and will initiate a read of that many bytes.  The machine will
**             advance to one of two states, depending on whether the record
**             is encrypted (GS_MAC), or unencrypted (GS_DATA).
**
** GS_MAC    - The machine is in this state while waiting for the remainder 
**             of the SSL2 record to be read in.  When the read is completed,
**             the machine checks the record for valid length, decrypts it,
**             and checks and discards the MAC, then advances to GS_INIT.
**
** GS_DATA   - The machine is in this state while waiting for the remainder
**             of the unencrypted SSL2 record to be read in.  Upon completion,
**             the machine advances to the GS_INIT state and returns the data.
*/
int 
ssl2_GatherData(sslSocket *ss, sslGather *gs, int flags)
{
    unsigned char *  bp;
    unsigned char *  pBuf;
    int              nb, err, rv;

    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss) );

    if (gs->state == GS_INIT) {
	/* Initialize gathering engine */
	gs->state         = GS_HEADER;
	gs->remainder     = 3;
	gs->count         = 3;
	gs->offset        = 0;
	gs->recordLen     = 0;
	gs->recordPadding = 0;
	gs->hdr[2]        = 0;

	gs->writeOffset   = 0;
	gs->readOffset    = 0;
    }
    if (gs->encrypted) {
	PORT_Assert(ss->sec.hash != 0);
    }

    pBuf = gs->buf.buf;
    for (;;) {
	SSL_TRC(30, ("%d: SSL[%d]: gather state %d (need %d more)",
		     SSL_GETPID(), ss->fd, gs->state, gs->remainder));
	bp = ((gs->state != GS_HEADER) ? pBuf : gs->hdr) + gs->offset;
	nb = ssl_DefRecv(ss, bp, gs->remainder, flags);
	if (nb > 0) {
	    PRINT_BUF(60, (ss, "raw gather data:", bp, nb));
	}
	if (nb == 0) {
	    /* EOF */
	    SSL_TRC(30, ("%d: SSL[%d]: EOF", SSL_GETPID(), ss->fd));
	    rv = 0;
	    break;
	}
	if (nb < 0) {
	    SSL_DBG(("%d: SSL[%d]: recv error %d", SSL_GETPID(), ss->fd,
		     PR_GetError()));
	    rv = SECFailure;
	    break;
	}

	gs->offset    += nb;
	gs->remainder -= nb;

	if (gs->remainder > 0) {
	    continue;
	}

	/* Probably finished this piece */
	switch (gs->state) {
	case GS_HEADER: 
	    if (!SSL3_ALL_VERSIONS_DISABLED(&ss->vrange) && !ss->firstHsDone) {

		PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

		/* If this looks like an SSL3 handshake record, 
		** and we're expecting an SSL2 Hello message from our peer, 
		** handle it here.
		*/
		if (gs->hdr[0] == content_handshake) {
		    if ((ss->nextHandshake == ssl2_HandleClientHelloMessage) ||
			(ss->nextHandshake == ssl2_HandleServerHelloMessage)) {
			rv = ssl2_HandleV3HandshakeRecord(ss);
			if (rv == SECFailure) {
			    return SECFailure;
			}
		    }
		    /* XXX_1	The call stack to here is:
		     * ssl_Do1stHandshake -> ssl_GatherRecord1stHandshake -> 
		     *			ssl2_GatherRecord -> here.
		     * We want to return all the way out to ssl_Do1stHandshake,
		     * and have it call ssl_GatherRecord1stHandshake again. 
		     * ssl_GatherRecord1stHandshake will call 
		     * ssl3_GatherCompleteHandshake when it is called again.
		     *
		     * Returning SECWouldBlock here causes 
		     * ssl_GatherRecord1stHandshake to return without clearing 
		     * ss->handshake, ensuring that ssl_Do1stHandshake will 
		     * call it again immediately.
		     * 
		     * If we return 1 here, ssl_GatherRecord1stHandshake will 
		     * clear ss->handshake before returning, and thus will not 
		     * be called again by ssl_Do1stHandshake.  
		     */
		    return SECWouldBlock;
		} else if (gs->hdr[0] == content_alert) {
		    if (ss->nextHandshake == ssl2_HandleServerHelloMessage) {
			/* XXX This is a hack.  We're assuming that any failure
			 * XXX on the client hello is a failure to match
			 * XXX ciphers.
			 */
			PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
			return SECFailure;
		    }
		}
	    }

	    /* we've got the first 3 bytes.  The header may be two or three. */
	    if (gs->hdr[0] & 0x80) {
		/* This record has a 2-byte header, and no padding */
		gs->count = ((gs->hdr[0] & 0x7f) << 8) | gs->hdr[1];
		gs->recordPadding = 0;
	    } else {
		/* This record has a 3-byte header that is all read in now. */
		gs->count = ((gs->hdr[0] & 0x3f) << 8) | gs->hdr[1];
	    /*  is_escape =  (gs->hdr[0] & 0x40) != 0; */
		gs->recordPadding = gs->hdr[2];
	    }
	    if (!gs->count) {
		PORT_SetError(SSL_ERROR_RX_RECORD_TOO_LONG);
		goto cleanup;
	    }

	    if (gs->count > gs->buf.space) {
		err = sslBuffer_Grow(&gs->buf, gs->count);
		if (err) {
		    return err;
		}
		pBuf = gs->buf.buf;
	    }


	    if (gs->hdr[0] & 0x80) {
	    	/* we've already read in the first byte of the body.
		** Put it into the buffer.
		*/
		pBuf[0]        = gs->hdr[2];
		gs->offset    = 1;
		gs->remainder = gs->count - 1;
	    } else {
		gs->offset    = 0;
		gs->remainder = gs->count;
	    }

	    if (gs->encrypted) {
		gs->state     = GS_MAC;
		gs->recordLen = gs->count - gs->recordPadding
		                          - ss->sec.hash->length;
	    } else {
		gs->state     = GS_DATA;
		gs->recordLen = gs->count;
	    }

	    break;


	case GS_MAC:
	    /* Have read in entire rest of the ciphertext.  
	    ** Check for valid length.
	    ** Decrypt it.
	    ** Check the MAC.
	    */
	    PORT_Assert(gs->encrypted);

	  {
	    unsigned int     macLen;
	    int              nout;
	    unsigned char    mac[SSL_MAX_MAC_BYTES];

	    ssl_GetSpecReadLock(ss); /**********************************/

	    /* If this is a stream cipher, blockSize will be 1,
	     * and this test will always be false.
	     * If this is a block cipher, this will detect records
	     * that are not a multiple of the blocksize in length.
	     */
	    if (gs->count & (ss->sec.blockSize - 1)) {
		/* This is an error. Sender is misbehaving */
		SSL_DBG(("%d: SSL[%d]: sender, count=%d blockSize=%d",
			 SSL_GETPID(), ss->fd, gs->count,
			 ss->sec.blockSize));
		PORT_SetError(SSL_ERROR_BAD_BLOCK_PADDING);
		rv = SECFailure;
		goto spec_locked_done;
	    }
	    PORT_Assert(gs->count == gs->offset);

	    if (gs->offset == 0) {
		rv = 0;			/* means EOF. */
		goto spec_locked_done;
	    }

	    /* Decrypt the portion of data that we just received.
	    ** Decrypt it in place.
	    */
	    rv = (*ss->sec.dec)(ss->sec.readcx, pBuf, &nout, gs->offset,
			     pBuf, gs->offset);
	    if (rv != SECSuccess) {
		goto spec_locked_done;
	    }


	    /* Have read in all the MAC portion of record 
	    **
	    ** Prepare MAC by resetting it and feeding it the shared secret
	    */
	    macLen = ss->sec.hash->length;
	    if (gs->offset >= macLen) {
		PRUint32           sequenceNumber = ss->sec.rcvSequence++;
		unsigned char    seq[4];

		seq[0] = (unsigned char) (sequenceNumber >> 24);
		seq[1] = (unsigned char) (sequenceNumber >> 16);
		seq[2] = (unsigned char) (sequenceNumber >> 8);
		seq[3] = (unsigned char) (sequenceNumber);

		(*ss->sec.hash->begin)(ss->sec.hashcx);
		(*ss->sec.hash->update)(ss->sec.hashcx, ss->sec.rcvSecret.data,
				        ss->sec.rcvSecret.len);
		(*ss->sec.hash->update)(ss->sec.hashcx, pBuf + macLen, 
				        gs->offset - macLen);
		(*ss->sec.hash->update)(ss->sec.hashcx, seq, 4);
		(*ss->sec.hash->end)(ss->sec.hashcx, mac, &macLen, macLen);

		PORT_Assert(macLen == ss->sec.hash->length);

		ssl_ReleaseSpecReadLock(ss);  /******************************/

		if (NSS_SecureMemcmp(mac, pBuf, macLen) != 0) {
		    /* MAC's didn't match... */
		    SSL_DBG(("%d: SSL[%d]: mac check failed, seq=%d",
			     SSL_GETPID(), ss->fd, ss->sec.rcvSequence));
		    PRINT_BUF(1, (ss, "computed mac:", mac, macLen));
		    PRINT_BUF(1, (ss, "received mac:", pBuf, macLen));
		    PORT_SetError(SSL_ERROR_BAD_MAC_READ);
		    rv = SECFailure;
		    goto cleanup;
		}
	    } else {
		ssl_ReleaseSpecReadLock(ss);  /******************************/
	    }

	    if (gs->recordPadding + macLen <= gs->offset) {
		gs->recordOffset  = macLen;
		gs->readOffset    = macLen;
		gs->writeOffset   = gs->offset - gs->recordPadding;
		rv = 1;
	    } else {
		PORT_SetError(SSL_ERROR_BAD_BLOCK_PADDING);
cleanup:
		/* nothing in the buffer any more. */
		gs->recordOffset  = 0;
		gs->readOffset    = 0;
	    	gs->writeOffset   = 0;
		rv = SECFailure;
	    }

	    gs->recordLen     = gs->writeOffset - gs->readOffset;
	    gs->recordPadding = 0;	/* forget we did any padding. */
	    gs->state = GS_INIT;


	    if (rv > 0) {
		PRINT_BUF(50, (ss, "recv clear record:", 
		               pBuf + gs->recordOffset, gs->recordLen));
	    }
	    return rv;

spec_locked_done:
	    ssl_ReleaseSpecReadLock(ss);
	    return rv;
	  }

	case GS_DATA:
	    /* Have read in all the DATA portion of record */

	    gs->recordOffset  = 0;
	    gs->readOffset    = 0;
	    gs->writeOffset   = gs->offset;
	    PORT_Assert(gs->recordLen == gs->writeOffset - gs->readOffset);
	    gs->recordLen     = gs->offset;
	    gs->recordPadding = 0;
	    gs->state         = GS_INIT;

	    ++ss->sec.rcvSequence;

	    PRINT_BUF(50, (ss, "recv clear record:", 
	                   pBuf + gs->recordOffset, gs->recordLen));
	    return 1;

	}	/* end switch gs->state */
    }		/* end gather loop. */
    return rv;
}

/*
** Gather a single record of data from the receiving stream. This code
** first gathers the header (2 or 3 bytes long depending on the value of
** the most significant bit in the first byte) then gathers up the data
** for the record into the readBuf. This code handles non-blocking I/O
** and is to be called multiple times until ss->sec.recordLen != 0.
 *
 * Returns +1 when it has gathered a complete SSLV2 record.
 * Returns  0 if it hits EOF.
 * Returns -1 (SECFailure)    on any error
 * Returns -2 (SECWouldBlock) 
 *
 * Called by ssl_GatherRecord1stHandshake in sslcon.c, 
 * and by DoRecv in sslsecur.c
 * Caller must hold RecvBufLock.
 */
int 
ssl2_GatherRecord(sslSocket *ss, int flags)
{
    return ssl2_GatherData(ss, &ss->gs, flags);
}

/* Caller should hold RecvBufLock. */
SECStatus
ssl_InitGather(sslGather *gs)
{
    SECStatus status;

    gs->state = GS_INIT;
    gs->writeOffset = 0;
    gs->readOffset  = 0;
    gs->dtlsPacketOffset = 0;
    gs->dtlsPacket.len = 0;
    status = sslBuffer_Grow(&gs->buf, 4096);
    return status;
}

/* Caller must hold RecvBufLock. */
void 
ssl_DestroyGather(sslGather *gs)
{
    if (gs) {	/* the PORT_*Free functions check for NULL pointers. */
	PORT_ZFree(gs->buf.buf, gs->buf.space);
	PORT_Free(gs->inbuf.buf);
	PORT_Free(gs->dtlsPacket.buf);
    }
}

/* Caller must hold RecvBufLock. */
static SECStatus
ssl2_HandleV3HandshakeRecord(sslSocket *ss)
{
    SECStatus           rv;

    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss) );
    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    /* We've read in 3 bytes, there are 2 more to go in an ssl3 header. */
    ss->gs.remainder         = 2;
    ss->gs.count             = 0;

    /* Clearing these handshake pointers ensures that 
     * ssl_Do1stHandshake won't call ssl2_HandleMessage when we return.
     */
    ss->nextHandshake     = 0;
    ss->securityHandshake = 0;

    /* Setting ss->version to an SSL 3.x value will cause 
    ** ssl_GatherRecord1stHandshake to invoke ssl3_GatherCompleteHandshake() 
    ** the next time it is called.
    **/
    rv = ssl3_NegotiateVersion(ss, SSL_LIBRARY_VERSION_MAX_SUPPORTED,
			       PR_TRUE);
    if (rv != SECSuccess) {
	return rv;
    }

    ss->sec.send         = ssl3_SendApplicationData;

    return SECSuccess;
}
