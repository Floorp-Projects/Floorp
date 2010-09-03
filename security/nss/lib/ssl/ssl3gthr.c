/*
 * Gather (Read) entire SSL3 records from socket into buffer.  
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/* $Id: ssl3gthr.c,v 1.9.20.1 2010/07/31 04:33:52 wtc%google.com Exp $ */

#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"
#include "ssl3prot.h"

/* 
 * Attempt to read in an entire SSL3 record.
 * Blocks here for blocking sockets, otherwise returns -1 with 
 * 	PR_WOULD_BLOCK_ERROR when socket would block.
 *
 * returns  1 if received a complete SSL3 record.
 * returns  0 if recv returns EOF
 * returns -1 if recv returns <0  
 *	(The error value may have already been set to PR_WOULD_BLOCK_ERROR)
 *
 * Caller must hold the recv buf lock.
 *
 * The Gather state machine has 3 states:  GS_INIT, GS_HEADER, GS_DATA.
 * GS_HEADER: waiting for the 5-byte SSL3 record header to come in.
 * GS_DATA:   waiting for the body of the SSL3 record   to come in.
 *
 * This loop returns when either (a) an error or EOF occurs, 
 *	(b) PR_WOULD_BLOCK_ERROR,
 * 	(c) data (entire SSL3 record) has been received.
 */
static int
ssl3_GatherData(sslSocket *ss, sslGather *gs, int flags)
{
    unsigned char *bp;
    unsigned char *lbp;
    int            nb;
    int            err;
    int            rv		= 1;

    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss) );
    if (gs->state == GS_INIT) {
	gs->state       = GS_HEADER;
	gs->remainder   = 5;
	gs->offset      = 0;
	gs->writeOffset = 0;
	gs->readOffset  = 0;
	gs->inbuf.len   = 0;
    }
    
    lbp = gs->inbuf.buf;
    for(;;) {
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

	PORT_Assert( nb <= gs->remainder );
	if (nb > gs->remainder) {
	    /* ssl_DefRecv is misbehaving!  this error is fatal to SSL. */
	    gs->state = GS_INIT;         /* so we don't crash next time */
	    rv = SECFailure;
	    break;
	}

	gs->offset    += nb;
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
	    /*
	    ** Have received SSL3 record header in gs->hdr.
	    ** Now extract the length of the following encrypted data, 
	    ** and then read in the rest of the SSL3 record into gs->inbuf.
	    */
	    gs->remainder = (gs->hdr[3] << 8) | gs->hdr[4];

	    /* This is the max fragment length for an encrypted fragment
	    ** plus the size of the record header.
	    */
	    if(gs->remainder > (MAX_FRAGMENT_LENGTH + 2048 + 5)) {
		SSL3_SendAlert(ss, alert_fatal, unexpected_message);
		gs->state = GS_INIT;
		PORT_SetError(SSL_ERROR_RX_RECORD_TOO_LONG);
		return SECFailure;
	    }

	    gs->state     = GS_DATA;
	    gs->offset    = 0;
	    gs->inbuf.len = 0;

	    if (gs->remainder > gs->inbuf.space) {
		err = sslBuffer_Grow(&gs->inbuf, gs->remainder);
		if (err) {	/* realloc has set error code to no mem. */
		    return err;
		}
		lbp = gs->inbuf.buf;
	    }
	    break;	/* End this case.  Continue around the loop. */


	case GS_DATA:
	    /* 
	    ** SSL3 record has been completely received.
	    */
	    gs->state = GS_INIT;
	    return 1;
	}
    }

    return rv;
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
    SSL3Ciphertext cText;
    int            rv;
    PRBool         canFalseStart = PR_FALSE;

    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss) );
    do {
	/* bring in the next sslv3 record. */
	rv = ssl3_GatherData(ss, &ss->gs, flags);
	if (rv <= 0) {
	    return rv;
	}
	
	/* decipher it, and handle it if it's a handshake. 
	 * If it's application data, ss->gs.buf will not be empty upon return. 
	 * If it's a change cipher spec, alert, or handshake message,
	 * ss->gs.buf.len will be 0 when ssl3_HandleRecord returns SECSuccess.
	 */
	cText.type    = (SSL3ContentType)ss->gs.hdr[0];
	cText.version = (ss->gs.hdr[1] << 8) | ss->gs.hdr[2];
	cText.buf     = &ss->gs.inbuf;
	rv = ssl3_HandleRecord(ss, &cText, &ss->gs.buf);
	if (rv < 0) {
	    return ss->recvdCloseNotify ? 0 : rv;
	}

	/* If we kicked off a false start in ssl3_HandleServerHelloDone, break
	 * out of this loop early without finishing the handshake.
	 */
	if (ss->opt.enableFalseStart) {
	    ssl_GetSSL3HandshakeLock(ss);
	    canFalseStart = (ss->ssl3.hs.ws == wait_change_cipher ||
			     ss->ssl3.hs.ws == wait_new_session_ticket) &&
		            ssl3_CanFalseStart(ss);
	    ssl_ReleaseSSL3HandshakeLock(ss);
	}
    } while (ss->ssl3.hs.ws != idle_handshake &&
             !canFalseStart &&
             ss->gs.buf.len == 0);

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
    int            rv;

    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss) );
    do {
	rv = ssl3_GatherCompleteHandshake(ss, flags);
    } while (rv > 0 && ss->gs.buf.len == 0);

    return rv;
}
