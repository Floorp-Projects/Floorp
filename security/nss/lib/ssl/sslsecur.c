/*
 * Various SSL functions.
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: sslsecur.c,v 1.2 2000/09/07 19:01:48 nelsonb%netscape.com Exp $
 */
#include "cert.h"
#include "secitem.h"
#include "keyhi.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "secoid.h"	/* for SECOID_GetALgorithmTag */
#include "pk11func.h"	/* for PK11_GenerateRandom */

#if defined(_WINDOWS)
#include "winsock.h"	/* for MSG_PEEK */
#elif defined(XP_MAC)
#include "macsocket.h"
#else
#include <sys/socket.h> /* for MSG_PEEK */
#endif

#define MAX_BLOCK_CYPHER_SIZE	32

#define TEST_FOR_FAILURE	/* reminder */
#define SET_ERROR_CODE		/* reminder */

/* Returns a SECStatus: SECSuccess or SECFailure, NOT SECWouldBlock. 
 * 
 * Currently, the list of functions called through ss->handshake is:
 * 
 * In sslsocks.c:
 *  SocksGatherRecord
 *  SocksHandleReply	
 *  SocksStartGather
 *
 * In sslcon.c:
 *  ssl_GatherRecord1stHandshake
 *  ssl2_HandleClientSessionKeyMessage
 *  ssl2_HandleMessage
 *  ssl2_HandleVerifyMessage
 *  ssl2_BeginClientHandshake
 *  ssl2_BeginServerHandshake
 *  ssl2_HandleClientHelloMessage
 *  ssl2_HandleServerHelloMessage
 * 
 * The ss->handshake function returns SECWouldBlock under these conditions:
 * 1.	ssl_GatherRecord1stHandshake called ssl2_GatherData which read in 
 *	the beginning of an SSL v3 hello message and returned SECWouldBlock 
 *	to switch to SSL v3 handshake processing.
 *
 * 2.	ssl2_HandleClientHelloMessage discovered version 3.0 in the incoming
 *	v2 client hello msg, and called ssl3_HandleV2ClientHello which 
 *	returned SECWouldBlock.
 *
 * 3.   SECWouldBlock was returned by one of the callback functions, via
 *	one of these paths:
 * -	ssl2_HandleMessage() -> ssl2_HandleRequestCertificate() -> ss->getClientAuthData()
 *
 * -	ssl2_HandleServerHelloMessage() -> ss->handleBadCert()
 *
 * -	ssl_GatherRecord1stHandshake() -> ssl3_GatherCompleteHandshake() -> 
 *	ssl3_HandleRecord() -> ssl3_HandleHandshake() -> 
 *	ssl3_HandleHandshakeMessage() -> ssl3_HandleCertificate() -> 
 *	ss->handleBadCert()
 *
 * -	ssl_GatherRecord1stHandshake() -> ssl3_GatherCompleteHandshake() -> 
 *	ssl3_HandleRecord() -> ssl3_HandleHandshake() -> 
 *	ssl3_HandleHandshakeMessage() -> ssl3_HandleCertificateRequest() -> 
 *	ss->getClientAuthData()
 *
 * Called from: SSL_ForceHandshake	(below), 
 *              ssl_SecureRecv 		(below) and
 *              ssl_SecureSend		(below)
 *	  from: WaitForResponse 	in sslsocks.c
 *	        ssl_SocksRecv   	in sslsocks.c
 *              ssl_SocksSend   	in sslsocks.c
 *
 * Caller must hold the (write) handshakeLock.
 */
int 
ssl_Do1stHandshake(sslSocket *ss)
{
    int rv        = SECSuccess;
    int loopCount = 0;

    PORT_Assert(ss->gather != 0);

    do {
	PORT_Assert( ssl_Have1stHandshakeLock(ss) );
	PORT_Assert( !ssl_HaveRecvBufLock(ss)   );
	PORT_Assert( !ssl_HaveXmitBufLock(ss)   );

	if (ss->handshake == 0) {
	    /* Previous handshake finished. Switch to next one */
	    ss->handshake = ss->nextHandshake;
	    ss->nextHandshake = 0;
	}
	if (ss->handshake == 0) {
	    /* Previous handshake finished. Switch to security handshake */
	    ss->handshake = ss->securityHandshake;
	    ss->securityHandshake = 0;
	}
	if (ss->handshake == 0) {
	    ssl_GetRecvBufLock(ss);
	    ss->gather->recordLen = 0;
	    ssl_ReleaseRecvBufLock(ss);

	    SSL_TRC(3, ("%d: SSL[%d]: handshake is completed",
			SSL_GETPID(), ss->fd));
            /* call handshake callback for ssl v2 */
	    /* for v3 this is done in ssl3_HandleFinished() */
	    if ((ss->sec != NULL) &&               /* used SSL */
	        (ss->handshakeCallback != NULL) && /* has callback */
		(!ss->connected) &&                /* only first time */
		(ss->version < SSL_LIBRARY_VERSION_3_0)) {  /* not ssl3 */
		ss->connected       = PR_TRUE;
		(ss->handshakeCallback)(ss->fd, ss->handshakeCallbackData);
	    }
	    ss->connected           = PR_TRUE;
	    ss->gather->writeOffset = 0;
	    ss->gather->readOffset  = 0;
	    break;
	}
	rv = (*ss->handshake)(ss);
	++loopCount;
    /* This code must continue to loop on SECWouldBlock, 
     * or any positive value.	See XXX_1 comments.
     */
    } while (rv != SECFailure);  	/* was (rv >= 0); XXX_1 */

    PORT_Assert( !ssl_HaveRecvBufLock(ss)   );
    PORT_Assert( !ssl_HaveXmitBufLock(ss)   );

    if (rv == SECWouldBlock) {
	PORT_SetError(PR_WOULD_BLOCK_ERROR);
	rv = SECFailure;
    }
    return rv;
}

/*
 * Handshake function that blocks.  Used to force a
 * retry on a connection on the next read/write.
 */
#ifdef macintosh
static SECStatus
#else
static int
#endif
AlwaysBlock(sslSocket *ss)
{
    PORT_SetError(PR_WOULD_BLOCK_ERROR);	/* perhaps redundant. */
    return SECWouldBlock;
}

/*
 * set the initial handshake state machine to block
 */
void
ssl_SetAlwaysBlock(sslSocket *ss)
{
    if (!ss->connected) {
	ss->handshake = AlwaysBlock;
	ss->nextHandshake = 0;
    }
}

/* Acquires and releases HandshakeLock.
*/
SECStatus
SSL_ResetHandshake(PRFileDesc *s, PRBool asServer)
{
    sslSocket *ss;
    SECStatus rv;

    ss = ssl_FindSocket(s);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in ResetHandshake", SSL_GETPID(), s));
	return SECFailure;
    }

    /* Don't waste my time */
    if (!ss->useSecurity)
	return SECSuccess;

    SSL_LOCK_READER(ss);
    SSL_LOCK_WRITER(ss);

    /* Reset handshake state */
    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    ss->connected = PR_FALSE;
    ss->handshake = asServer ? ssl2_BeginServerHandshake
	                     : ssl2_BeginClientHandshake;
    ss->nextHandshake       = 0;
    ss->securityHandshake   = 0;

    ssl_GetRecvBufLock(ss);
    ss->gather->state       = GS_INIT;
    ss->gather->writeOffset = 0;
    ss->gather->readOffset  = 0;
    ssl_ReleaseRecvBufLock(ss);

    /*
    ** Blow away old security state and get a fresh setup. This way if
    ** ssl was used to connect to the first point in communication, ssl
    ** can be used for the next layer.
    */
    if (ss->sec) {
	ssl_DestroySecurityInfo(ss->sec);
	ss->sec = 0;
    }
    rv = ssl_CreateSecurityInfo(ss);

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    SSL_UNLOCK_WRITER(ss);
    SSL_UNLOCK_READER(ss);

    return rv;
}

/* For SSLv2, does nothing but return an error.
** For SSLv3, flushes SID cache entry (if requested),
** and then starts new client hello or hello request.
** Acquires and releases HandshakeLock.
*/
int
SSL_ReHandshake(PRFileDesc *fd, PRBool flushCache)
{
    sslSocket *ss;
    int        rv;
    
    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in RedoHandshake", SSL_GETPID(), fd));
	PORT_SetError(PR_BAD_DESCRIPTOR_ERROR);
	return SECFailure;
    }

    if (!ss->useSecurity)
	return SECSuccess;
    
    ssl_Get1stHandshakeLock(ss);

    /* SSL v2 protocol does not support subsequent handshakes. */
    if (ss->version < SSL_LIBRARY_VERSION_3_0) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	rv = SECFailure;
    } else {
	ssl_GetSSL3HandshakeLock(ss);
	rv = ssl3_RedoHandshake(ss, flushCache); /* force full handshake. */
	ssl_ReleaseSSL3HandshakeLock(ss);
    }

    ssl_Release1stHandshakeLock(ss);

    return rv;
}

int
SSL_RedoHandshake(PRFileDesc *fd)
{
    return SSL_ReHandshake(fd, PR_TRUE);
}

/* Register an application callback to be called when SSL handshake completes.
** Acquires and releases HandshakeLock.
*/
int
SSL_HandshakeCallback(PRFileDesc *fd, SSLHandshakeCallback cb,
		      void *client_data)
{
    sslSocket *ss;
    
    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in HandshakeCallback",
		 SSL_GETPID(), fd));
	PORT_SetError(PR_BAD_DESCRIPTOR_ERROR);
	return SECFailure;
    }

    if (!ss->useSecurity) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    PORT_Assert(ss->sec);
    ss->handshakeCallback     = cb;
    ss->handshakeCallbackData = client_data;

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return SECSuccess;
}

/* Try to make progress on an SSL handshake by attempting to read the 
** next handshake from the peer, and sending any responses.
** For non-blocking sockets, returns PR_ERROR_WOULD_BLOCK  if it cannot 
** read the next handshake from the underlying socket.
** For SSLv2, returns when handshake is complete or fatal error occurs.
** For SSLv3, returns when handshake is complete, or application data has
** arrived that must be taken by application before handshake can continue, 
** or a fatal error occurs.
** Application should use handshake completion callback to tell which. 
*/
int
SSL_ForceHandshake(PRFileDesc *fd)
{
    sslSocket *ss;
    int        rv;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in ForceHandshake",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    /* Don't waste my time */
    if (!ss->useSecurity) 
    	return 0;

    ssl_Get1stHandshakeLock(ss);

    if (ss->version >= SSL_LIBRARY_VERSION_3_0) {
    	ssl_GetRecvBufLock(ss);
	rv = ssl3_GatherCompleteHandshake(ss, 0);
	ssl_ReleaseRecvBufLock(ss);
	if (rv == 0) {
	    PORT_SetError(PR_END_OF_FILE_ERROR);
	    rv = SECFailure;
	} else if (rv == SECWouldBlock) {
	    PORT_SetError(PR_WOULD_BLOCK_ERROR);
	    rv = SECFailure;
	}
    } else if (!ss->connected) {
	rv = ssl_Do1stHandshake(ss);
    } else {
	/* tried to force handshake on a connected SSL 2 socket. */
    	rv = SECSuccess;	/* just pretend we did it. */
    }

    ssl_Release1stHandshakeLock(ss);

    if (rv > 0) 
    	rv = SECSuccess;
    return rv;
}

/************************************************************************/

/*
** Grow a buffer to hold newLen bytes of data.
** Called for both recv buffers and xmit buffers.
** Caller must hold xmitBufLock or recvBufLock, as appropriate.
*/
SECStatus
sslBuffer_Grow(sslBuffer *b, unsigned int newLen)
{
    if (newLen > b->space) {
	if (b->buf) {
	    b->buf = (unsigned char *) PORT_Realloc(b->buf, newLen);
	} else {
	    b->buf = (unsigned char *) PORT_Alloc(newLen);
	}
	if (!b->buf) {
	    return SECFailure;
	}
	SSL_TRC(10, ("%d: SSL: grow buffer from %d to %d",
		     SSL_GETPID(), b->space, newLen));
	b->space = newLen;
    }
    return SECSuccess;
}

/*
** Save away write data that is trying to be written before the security
** handshake has been completed. When the handshake is completed, we will
** flush this data out.
** Caller must hold xmitBufLock
*/
SECStatus 
ssl_SaveWriteData(sslSocket *ss, sslBuffer *buf, const void *data, 
                      unsigned int len)
{
    unsigned int newlen;
    SECStatus    rv;

    PORT_Assert( ssl_HaveXmitBufLock(ss) );
    newlen = buf->len + len;
    if (newlen > buf->space) {
	rv = sslBuffer_Grow(buf, newlen);
	if (rv) {
	    return rv;
	}
    }
    SSL_TRC(5, ("%d: SSL[%d]: saving %d bytes of data (%d total saved so far)",
		 SSL_GETPID(), ss->fd, len, newlen));
    PORT_Memcpy(buf->buf + buf->len, data, len);
    buf->len = newlen;
    return SECSuccess;
}

/*
** Send saved write data. This will flush out data sent prior to a
** complete security handshake. Hopefully there won't be too much of it.
** Returns count of the bytes sent, NOT a SECStatus.
** Caller must hold xmitBufLock
*/
int 
ssl_SendSavedWriteData(sslSocket *ss, sslBuffer *buf, sslSendFunc send)
{
    int rv	= 0;
    int len	= buf->len;

    PORT_Assert( ssl_HaveXmitBufLock(ss) );
    if (len != 0) {
	SSL_TRC(5, ("%d: SSL[%d]: sending %d bytes of saved data",
		     SSL_GETPID(), ss->fd, len));
	rv = (*send)(ss, buf->buf, len, 0);
	if (rv < 0) {
	    return rv;
	} 
	if (rv < len) {
	    /* UGH !! This shifts the whole buffer down by copying it, and 
	    ** it depends on PORT_Memmove doing overlapping moves correctly!
	    ** It should advance the pointer offset instead !!
	    */
	    PORT_Memmove(buf->buf, buf->buf + rv, len - rv);
	    buf->len = len - rv;
	} else {
	    buf->len = 0;
    	}
    }
    return rv;
}

/************************************************************************/

/*
** Receive some application data on a socket.  Reads SSL records from the input
** stream, decrypts them and then copies them to the output buffer.
** Called from ssl_SecureRecv() below.
**
** Caller does NOT hold 1stHandshakeLock because that handshake is over.
** Caller doesn't call this until initial handshake is complete.
** For SSLv2, there is no subsequent handshake.
** For SSLv3, the call to ssl3_GatherAppDataRecord may encounter handshake
** messages from a subsequent handshake.
**
** This code is similar to, and easily confused with, 
**   ssl_GatherRecord1stHandshake() in sslcon.c
*/
static int 
DoRecv(sslSocket *ss, unsigned char *out, int len, int flags)
{
    sslGather *      gs;
    int              rv;
    int              amount;
    int              available;

    ssl_GetRecvBufLock(ss);
    PORT_Assert((ss->sec != 0) && (ss->gather != 0));
    gs = ss->gather;

    available = gs->writeOffset - gs->readOffset;
    if (available == 0) {
	/* Get some more data */
	if (ss->version >= SSL_LIBRARY_VERSION_3_0) {
	    /* Wait for application data to arrive.  */
	    rv = ssl3_GatherAppDataRecord(ss, 0);
	} else {
	    /* See if we have a complete record */
	    rv = ssl2_GatherRecord(ss, 0);
	}
	if (rv <= 0) {
	    if (rv == 0) {
		/* EOF */
		SSL_TRC(10, ("%d: SSL[%d]: ssl_recv EOF",
			     SSL_GETPID(), ss->fd));
		goto done;
	    }
	    if ((rv != SECWouldBlock) && 
	        (PR_GetError() != PR_WOULD_BLOCK_ERROR)) {
		/* Some random error */
		goto done;
	    }

	    /*
	    ** Gather record is blocked waiting for more record data to
	    ** arrive. Try to process what we have already received
	    */
	} else {
	    /* Gather record has finished getting a complete record */
	}

	/* See if any clear data is now available */
	available = gs->writeOffset - gs->readOffset;
	if (available == 0) {
	    /*
	    ** No partial data is available. Force error code to
	    ** EWOULDBLOCK so that caller will try again later. Note
	    ** that the error code is probably EWOULDBLOCK already,
	    ** but if it isn't (for example, if we received a zero
	    ** length record) then this will force it to be correct.
	    */
	    PORT_SetError(PR_WOULD_BLOCK_ERROR);
	    rv = SECFailure;
	    goto done;
	}
	SSL_TRC(30, ("%d: SSL[%d]: partial data ready, available=%d",
		     SSL_GETPID(), ss->fd, available));
    }

    /* Dole out clear data to reader */
    amount = PR_MIN(len, available);
    PORT_Memcpy(out, gs->buf.buf + gs->readOffset, amount);
    if (!(flags & MSG_PEEK)) {
	gs->readOffset += amount;
    }
    rv = amount;

    SSL_TRC(30, ("%d: SSL[%d]: amount=%d available=%d",
		 SSL_GETPID(), ss->fd, amount, available));
    PRINT_BUF(4, (ss, "DoRecv receiving plaintext:", out, amount));

done:
    ssl_ReleaseRecvBufLock(ss);
    return rv;
}

/************************************************************************/

SSLKEAType
ssl_FindCertKEAType(CERTCertificate * cert)
{
  SSLKEAType keaType = kt_null; 
  int tag;
  
  if (!cert) goto loser;
  
  tag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
  
  switch (tag) {
  case SEC_OID_X500_RSA_ENCRYPTION:
  case SEC_OID_PKCS1_RSA_ENCRYPTION:
    keaType = kt_rsa;
    break;
  case SEC_OID_MISSI_KEA_DSS_OLD:
  case SEC_OID_MISSI_KEA_DSS:
  case SEC_OID_MISSI_DSS_OLD:
  case SEC_OID_MISSI_DSS:
    keaType = kt_fortezza;
    break;
  case SEC_OID_X942_DIFFIE_HELMAN_KEY:
    keaType = kt_dh;
    break;
  default:
    keaType = kt_null;
  }
  
 loser:
  
  return keaType;

}


/* XXX need to protect the data that gets changed here.!! */

SECStatus
SSL_ConfigSecureServer(PRFileDesc *fd, CERTCertificate *cert,
		       SECKEYPrivateKey *key, SSL3KEAType kea)
{
    int rv;
    sslSocket *ss;
    sslSecurityInfo *sec;

    ss = ssl_FindSocket(fd);

    if ((rv = ssl_CreateSecurityInfo(ss)) != 0) {
	return((SECStatus)rv);
    }

    sec = ss->sec;
    if (sec == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* Both key and cert must have a value or be NULL */
    /* Passing a value of NULL will turn off key exchange algorithms that were
     * previously turned on */
    if (!cert != !key) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* make sure the key exchange is recognized */
    if ((kea > kt_kea_size) || (kea < kt_null)) {
	PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
	return SECFailure;
    }

    if (kea != ssl_FindCertKEAType(cert)) {
    	PORT_SetError(SSL_ERROR_CERT_KEA_MISMATCH);
	return SECFailure;
    }

    /* load the server certificate */
    if (ss->serverCert[kea] != NULL)
	CERT_DestroyCertificate(ss->serverCert[kea]);
    if (cert) {
	ss->serverCert[kea] = CERT_DupCertificate(cert);
	if (ss->serverCert[kea] == NULL)
	    goto loser;
    } else ss->serverCert[kea] = NULL;


    /* load the server cert chain */
    if (ss->serverCertChain[kea] != NULL)
	CERT_DestroyCertificateList(ss->serverCertChain[kea]);
    if (cert) {
	ss->serverCertChain[kea] = CERT_CertChainFromCert(
			    ss->serverCert[kea], certUsageSSLServer, PR_TRUE);
	if (ss->serverCertChain[kea] == NULL)
	     goto loser;
    } else ss->serverCertChain[kea] = NULL;


    /* Only do this once because it's global. */
    if (ssl3_server_ca_list == NULL)
	ssl3_server_ca_list = CERT_GetSSLCACerts(ss->dbHandle);

    /* load the private key */
    if (ss->serverKey[kea] != NULL)
	SECKEY_DestroyPrivateKey(ss->serverKey[kea]);
    if (key) {
	ss->serverKey[kea] = SECKEY_CopyPrivateKey(key);
	if (ss->serverKey[kea] == NULL)
	    goto loser;
    } else ss->serverKey[kea] = NULL;

    if (kea == kt_rsa) {
        rv = ssl3_CreateRSAStepDownKeys(ss);
	if (rv != SECSuccess) {
	    return SECFailure;	/* err set by ssl3_CreateRSAStepDownKeys */
	}
    }

    return SECSuccess;

loser:
    if (ss->serverCert[kea] != NULL) {
	CERT_DestroyCertificate(ss->serverCert[kea]);
	ss->serverCert[kea] = NULL;
    }
    if (ss->serverCertChain != NULL) {
	CERT_DestroyCertificateList(ss->serverCertChain[kea]);
	ss->serverCertChain[kea] = NULL;
    }
    if (ss->serverKey[kea] != NULL) {
	SECKEY_DestroyPrivateKey(ss->serverKey[kea]);
	ss->serverKey[kea] = NULL;
    }
    return SECFailure;
}

/************************************************************************/

SECStatus
ssl_CreateSecurityInfo(sslSocket *ss)
{
    sslSecurityInfo * sec  = (sslSecurityInfo *)0;
    sslGather *       gs   = (sslGather *      )0;
    int               rv;

    unsigned char     padbuf[MAX_BLOCK_CYPHER_SIZE];

    if (ss->sec) {
	return SECSuccess;
    }

    /* Force the global RNG to generate some random data that we never use */
    PK11_GenerateRandom(padbuf, sizeof padbuf);

    ss->sec = sec = (sslSecurityInfo*) PORT_ZAlloc(sizeof(sslSecurityInfo));
    if (!sec) {
	goto loser;
    }

    /* initialize sslv2 socket to send data in the clear. */
    ssl2_UseClearSendFunc(ss);

    sec->blockSize  = 1;
    sec->blockShift = 0;

    ssl_GetRecvBufLock(ss);
    if ((gs = ss->gather) == 0) {
	ss->gather = gs = ssl_NewGather();
	if (!gs) {
	    goto loser;
	}
    }

    rv = sslBuffer_Grow(&gs->buf, 4096);
    if (rv) {
	goto loser;
    }
    ssl_ReleaseRecvBufLock(ss);

    ssl_GetXmitBufLock(ss);
    rv = sslBuffer_Grow(&sec->writeBuf, 4096);
    if (rv) {
	goto loser;
    }
    ssl_ReleaseXmitBufLock(ss);

    SSL_TRC(5, ("%d: SSL[%d]: security info created", SSL_GETPID(), ss->fd));
    return SECSuccess;

  loser:
    if (sec) {
	PORT_Free(sec);
	ss->sec = sec = (sslSecurityInfo *)0;
    }
    if (gs) {
    	ssl_DestroyGather(gs);
	ss->gather = gs = (sslGather *)0;
    }
    return SECFailure;
}

/* XXX We should handle errors better in this function. */
/* This function assumes that none of the pointers in ss need to be 
** freed.
*/
SECStatus
ssl_CopySecurityInfo(sslSocket *ss, sslSocket *os)
{
    sslSecurityInfo *sec, *osec;
    int rv;

    rv = ssl_CreateSecurityInfo(ss);
    if (rv < 0) {
	goto loser;
    }
    sec = ss->sec;
    osec = os->sec;

    sec->send 		= osec->send;
    sec->isServer 	= osec->isServer;
    sec->keyBits    	= osec->keyBits;
    sec->secretKeyBits 	= osec->secretKeyBits;

    sec->peerCert   	= CERT_DupCertificate(osec->peerCert);

    sec->cache      	= osec->cache;
    sec->uncache    	= osec->uncache;

    /* we don't dup the connection info. */

    sec->sendSequence 	= osec->sendSequence;
    sec->rcvSequence 	= osec->rcvSequence;

    if (osec->hash && osec->hashcx) {
	sec->hash 	= osec->hash;
	sec->hashcx 	= osec->hash->clone(osec->hashcx);
    } else {
	sec->hash 	= NULL;
	sec->hashcx 	= NULL;
    }

    SECITEM_CopyItem(0, &sec->sendSecret, &osec->sendSecret);
    SECITEM_CopyItem(0, &sec->rcvSecret,  &osec->rcvSecret);

    PORT_Assert(osec->readcx  == 0);
    sec->readcx     	= osec->readcx;	/* XXX wrong if readcx  != 0 */
    PORT_Assert(osec->writecx == 0);
    sec->writecx    	= osec->writecx; /* XXX wrong if writecx != 0 */
    sec->destroy    	= 0;		 /* XXX wrong if either cx != 0*/

    sec->enc        	= osec->enc;
    sec->dec        	= osec->dec;

    sec->blockShift 	= osec->blockShift;
    sec->blockSize  	= osec->blockSize;

    return SECSuccess;

loser:
    return SECFailure;
}

/*
** Called from SSL_ResetHandshake (above), and 
**        from ssl_FreeSocket     in sslsock.c
*/
void 
ssl_DestroySecurityInfo(sslSecurityInfo *sec)
{
    if (sec != 0) {
	/* Destroy MAC */
	if (sec->hash && sec->hashcx) {
	    (*sec->hash->destroy)(sec->hashcx, PR_TRUE);
	    sec->hashcx = 0;
	}
	SECITEM_ZfreeItem(&sec->sendSecret, PR_FALSE);
	SECITEM_ZfreeItem(&sec->rcvSecret, PR_FALSE);

	/* Destroy ciphers */
	if (sec->destroy) {
	    (*sec->destroy)(sec->readcx, PR_TRUE);
	    (*sec->destroy)(sec->writecx, PR_TRUE);
	} else {
	    PORT_Assert(sec->readcx == 0);
	    PORT_Assert(sec->writecx == 0);
	}
	sec->readcx = 0;
	sec->writecx = 0;

	/* etc. */
	PORT_ZFree(sec->writeBuf.buf, sec->writeBuf.space);
	sec->writeBuf.buf = 0;

	CERT_DestroyCertificate(sec->peerCert);
	sec->peerCert = NULL;

	PORT_ZFree(sec->ci.sendBuf.buf, sec->ci.sendBuf.space);
	if (sec->ci.sid != NULL) {
	    ssl_FreeSID(sec->ci.sid);
	}

	PORT_ZFree(sec, sizeof *sec);
    }
}

/************************************************************************/

int 
ssl_SecureConnect(sslSocket *ss, const PRNetAddr *sa)
{
    PRFileDesc *osfd = ss->fd->lower;
    int rv;

    PORT_Assert(ss->sec != 0);

    /* First connect to server */
    rv = osfd->methods->connect(osfd, sa, ss->cTimeout);
    if (rv < 0) {
	int olderrno = PR_GetError();
	SSL_DBG(("%d: SSL[%d]: connect failed, errno=%d",
		 SSL_GETPID(), ss->fd, olderrno));
	if ((olderrno == PR_IS_CONNECTED_ERROR) ||
	    (olderrno == PR_IN_PROGRESS_ERROR)) {
	    /*
	    ** Connected or trying to connect.  Caller is Using a non-blocking
	    ** connect. Go ahead and set things up.
	    */
	} else {
	    return rv;
	}
    }

    SSL_TRC(5, ("%d: SSL[%d]: secure connect completed, setting up handshake",
		SSL_GETPID(), ss->fd));

    if ( ss->handshakeAsServer ) {
	ss->securityHandshake = ssl2_BeginServerHandshake;
    } else {
	ss->securityHandshake = ssl2_BeginClientHandshake;
    }
    
    return rv;
}

int
ssl_SecureSocksConnect(sslSocket *ss, const PRNetAddr *sa)
{
    int rv;

    PORT_Assert((ss->socks != 0) && (ss->sec != 0));

    /* First connect to socks daemon */
    rv = ssl_SocksConnect(ss, sa);
    if (rv < 0) {
	return rv;
    }

    if ( ss->handshakeAsServer ) {
	ss->securityHandshake = ssl2_BeginServerHandshake;
    } else {
	ss->securityHandshake = ssl2_BeginClientHandshake;
    }
    
    return 0;
}

PRFileDesc *
ssl_SecureSocksAccept(sslSocket *ss, PRNetAddr *addr)
{
#if 0
    sslSocket *ns;
    int rv;
    PRFileDesc *newfd, *fd;

    newfd = ssl_SocksAccept(ss, addr);
    if (newfd == NULL) {
	return newfd;
    }

    /* Create new socket */
    ns = ssl_FindSocket(newfd);
    PORT_Assert(ns != NULL);

    /* Make an NSPR socket to give back to app */
    fd = ssl_NewPRSocket(ns, newfd);
    if (fd == NULL) {
	ssl_FreeSocket(ns);
	PR_Close(newfd);
	return NULL;
    }

    if ( ns->handshakeAsClient ) {
	ns->handshake = ssl2_BeginClientHandshake;
    } else {
	ns->handshake = ssl2_BeginServerHandshake;
    }

    return fd;
#else
    return NULL;
#endif
}

int
ssl_SecureClose(sslSocket *ss)
{
    int rv;

    if (ss->version >= SSL_LIBRARY_VERSION_3_0 	&&
    	ss->connected 				&& 
	!(ss->shutdownHow & ssl_SHUTDOWN_SEND)	&&
	(ss->ssl3 != NULL)) {

	(void) SSL3_SendAlert(ss, alert_warning, close_notify);
    }
    rv = ssl_DefClose(ss);
    return rv;
}

/* Caller handles all locking */
int
ssl_SecureShutdown(sslSocket *ss, int nsprHow)
{
    PRFileDesc *osfd = ss->fd->lower;
    int 	rv;
    PRIntn	sslHow	= nsprHow + 1;

    if ((unsigned)nsprHow > PR_SHUTDOWN_BOTH) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
    	return PR_FAILURE;
    }

    if ((sslHow & ssl_SHUTDOWN_SEND) != 0 		&&
	!(ss->shutdownHow & ssl_SHUTDOWN_SEND)		&&
    	(ss->version >= SSL_LIBRARY_VERSION_3_0)	&&
	ss->connected 					&& 
	(ss->ssl3 != NULL)) {

	(void) SSL3_SendAlert(ss, alert_warning, close_notify);
    }

    rv = osfd->methods->shutdown(osfd, nsprHow);

    ss->shutdownHow |= sslHow;

    return rv;
}

/************************************************************************/


int
ssl_SecureRecv(sslSocket *ss, unsigned char *buf, int len, int flags)
{
    sslSecurityInfo *sec;
    int              rv   = 0;

    PORT_Assert(ss->sec != 0);
    sec = ss->sec;

    if (ss->shutdownHow & ssl_SHUTDOWN_RCV) {
	PORT_SetError(PR_SOCKET_SHUTDOWN_ERROR);
    	return PR_FAILURE;
    }
    if (flags & ~MSG_PEEK) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
    	return PR_FAILURE;
    }

    if (!ssl_SocketIsBlocking(ss) && !ss->fdx) {
	ssl_GetXmitBufLock(ss);
	if (ss->pendingBuf.len != 0) {
	    rv = ssl_SendSavedWriteData(ss, &ss->pendingBuf, ssl_DefSend);
	    if ((rv < 0) && (PORT_GetError() != PR_WOULD_BLOCK_ERROR)) {
		ssl_ReleaseXmitBufLock(ss);
		return SECFailure;
	    }
	    /* XXX short write? */
	}
	ssl_ReleaseXmitBufLock(ss);
    }
    
    rv = 0;
    /* If any of these is non-zero, the initial handshake is not done. */
    if (!ss->connected) {
	ssl_Get1stHandshakeLock(ss);
	if (ss->handshake || ss->nextHandshake || ss->securityHandshake) {
	    rv = ssl_Do1stHandshake(ss);
	}
	ssl_Release1stHandshakeLock(ss);
    }
    if (rv < 0) {
	return rv;
    }

    if (len == 0) return 0;

    rv = DoRecv(ss, (unsigned char*) buf, len, flags);
    SSL_TRC(2, ("%d: SSL[%d]: recving %d bytes securely (errno=%d)",
		SSL_GETPID(), ss->fd, rv, PORT_GetError()));
    return rv;
}

int
ssl_SecureRead(sslSocket *ss, unsigned char *buf, int len)
{
    return ssl_SecureRecv(ss, buf, len, 0);
}

int
ssl_SecureSend(sslSocket *ss, const unsigned char *buf, int len, int flags)
{
    sslSecurityInfo *sec;
    int              rv		= 0;

    PORT_Assert(ss->sec != 0);
    sec = ss->sec;

    if (ss->shutdownHow & ssl_SHUTDOWN_SEND) {
	PORT_SetError(PR_SOCKET_SHUTDOWN_ERROR);
    	return PR_FAILURE;
    }
    if (flags) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
    	return PR_FAILURE;
    }

    ssl_GetXmitBufLock(ss);
    if (ss->pendingBuf.len != 0) {
	PORT_Assert(ss->pendingBuf.len > 0);
	rv = ssl_SendSavedWriteData(ss, &ss->pendingBuf, ssl_DefSend);
	if (ss->pendingBuf.len != 0) {
	    PORT_Assert(ss->pendingBuf.len > 0);
	    PORT_SetError(PR_WOULD_BLOCK_ERROR);
	    rv = SECFailure;
	}
    }
    ssl_ReleaseXmitBufLock(ss);
    if (rv < 0) {
	return rv;
    }

    /* If any of these is non-zero, the initial handshake is not done. */
    if (!ss->connected) {
	ssl_Get1stHandshakeLock(ss);
	if (ss->handshake || ss->nextHandshake || ss->securityHandshake) {
	    rv = ssl_Do1stHandshake(ss);
	}
	ssl_Release1stHandshakeLock(ss);
    }
    if (rv < 0) {
	return rv;
    }

    /* Check for zero length writes after we do housekeeping so we make forward
     * progress.
     */
    if (len == 0) return 0;
    PORT_Assert(buf != NULL);
    
    SSL_TRC(2, ("%d: SSL[%d]: SecureSend: sending %d bytes",
		SSL_GETPID(), ss->fd, len));

    /* Send out the data using one of these functions:
     *	ssl2_SendClear, ssl2_SendStream, ssl2_SendBlock, 
     *  ssl3_SendApplicationData
     */
    ssl_GetXmitBufLock(ss);
    rv = (*sec->send)(ss, buf, len, flags);
    ssl_ReleaseXmitBufLock(ss);
    return rv;
}

int
ssl_SecureWrite(sslSocket *ss, const unsigned char *buf, int len)
{
    return ssl_SecureSend(ss, buf, len, 0);
}

int
SSL_BadCertHook(PRFileDesc *fd, SSLBadCertHandler f, void *arg)
{
    sslSocket *ss;
    int rv;
    
    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSLBadCertHook",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if ((rv = ssl_CreateSecurityInfo(ss)) != 0) {
	return(rv);
    }
    ss->handleBadCert = f;
    ss->badCertArg = arg;

    return(0);
}

/*
 * Allow the application to pass the url or hostname into the SSL library
 * so that we can do some checking on it.
 */
int
SSL_SetURL(PRFileDesc *fd, const char *url)
{
    sslSocket *   ss = ssl_FindSocket(fd);
    int           rv = SECSuccess;

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSLSetURL",
		 SSL_GETPID(), fd));
	return SECFailure;
    }
    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    if ( ss->url ) {
	PORT_Free((void *)ss->url);	/* CONST */
    }

    ss->url = (const char *)PORT_Strdup(url);
    if ( ss->url == NULL ) {
	rv = SECFailure;
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return rv;
}

/*
** Returns Negative number on error, zero or greater on success.
** Returns the amount of data immediately available to be read.
*/
int
SSL_DataPending(PRFileDesc *fd)
{
    sslSocket *ss;
    int        rv  = 0;

    ss = ssl_FindSocket(fd);


    if (ss && ss->useSecurity) {

	ssl_Get1stHandshakeLock(ss);
	ssl_GetSSL3HandshakeLock(ss);

	/* Create ss->sec if it doesn't already exist. */
	rv = ssl_CreateSecurityInfo(ss);
	if (rv == SECSuccess) {
	    ssl_GetRecvBufLock(ss);
	    rv = ss->gather->writeOffset - ss->gather->readOffset;
	    ssl_ReleaseRecvBufLock(ss);
	}

	ssl_ReleaseSSL3HandshakeLock(ss);
	ssl_Release1stHandshakeLock(ss);
    }

    return rv;
}

int
SSL_InvalidateSession(PRFileDesc *fd)
{
    sslSocket *   ss = ssl_FindSocket(fd);
    int           rv = SECFailure;

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    if (ss && ss->sec && ss->sec->ci.sid) {
	ss->sec->uncache(ss->sec->ci.sid);
	rv = SECSuccess;
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return rv;
}

SECItem *
SSL_GetSessionID(PRFileDesc *fd)
{
    sslSocket *    ss;
    SECItem *      item = NULL;
    sslSessionID * sid;

    ss = ssl_FindSocket(fd);

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    if (ss && ss->useSecurity && ss->connected && ss->sec && ss->sec->ci.sid) {
        sid = ss->sec->ci.sid;
        item = (SECItem *)PORT_Alloc(sizeof(SECItem));
        if (sid->version < SSL_LIBRARY_VERSION_3_0) {
            item->len = SSL_SESSIONID_BYTES;
            item->data = (unsigned char*)PORT_Alloc(item->len);
            PORT_Memcpy(item->data, sid->u.ssl2.sessionID, item->len);
        } else {
            item->len = sid->u.ssl3.sessionIDLength;
            item->data = (unsigned char*)PORT_Alloc(item->len);
            PORT_Memcpy(item->data, sid->u.ssl3.sessionID, item->len);
        }
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return item;
}

SECStatus
SSL_CertDBHandleSet(PRFileDesc *fd, CERTCertDBHandle *dbHandle)
{
    sslSocket *    ss;

    ss = ssl_FindSocket(fd);
    if (!ss)
    	return SECFailure;
    if (!dbHandle) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    ss->dbHandle = dbHandle;
    return SECSuccess;
}

/*
 * attempt to restart the handshake after asynchronously handling
 * a request for the client's certificate.
 *
 * inputs:  
 *	cert	Client cert chosen by application.
 *		Note: ssl takes this reference, and does not bump the 
 *		reference count.  The caller should drop its reference
 *		without calling CERT_DestroyCert after calling this function.
 *
 *	key	Private key associated with cert.  This function makes a 
 *		copy of the private key, so the caller remains responsible 
 *		for destroying its copy after this function returns.
 *
 *	certChain  Chain of signers for cert.  
 *		Note: ssl takes this reference, and does not copy the chain.
 *		The caller should drop its reference without destroying the 
 *		chain.  SSL will free the chain when it is done with it.
 *
 * Return value: XXX
 *
 * XXX This code only works on the initial handshake on a connection, XXX
 *     It does not work on a subsequent handshake (redo).
 */
int
SSL_RestartHandshakeAfterCertReq(sslSocket *         ss,
				CERTCertificate *    cert, 
				SECKEYPrivateKey *   key,
				CERTCertificateList *certChain)
{
    int              ret;

    ssl_Get1stHandshakeLock(ss);   /************************************/

    if (ss->version >= SSL_LIBRARY_VERSION_3_0) {
	ret = ssl3_RestartHandshakeAfterCertReq(ss, cert, key, certChain);
    } else {
    	ret = ssl2_RestartHandshakeAfterCertReq(ss, cert, key);
    }

    ssl_Release1stHandshakeLock(ss);  /************************************/
    return ret;
}


/* restart an SSL connection that we stopped to run certificate dialogs 
** XXX	Need to document here how an application marks a cert to show that
**	the application has accepted it (overridden CERT_VerifyCert).
 *
 * XXX This code only works on the initial handshake on a connection, XXX
 *     It does not work on a subsequent handshake (redo).
 *
 * Return value: XXX
*/
int
SSL_RestartHandshakeAfterServerCert(sslSocket *ss)
{
    int rv	= SECSuccess;

    ssl_Get1stHandshakeLock(ss); 

    if (ss->version >= SSL_LIBRARY_VERSION_3_0) {
	rv = ssl3_RestartHandshakeAfterServerCert(ss);
    } else {
	rv = ssl2_RestartHandshakeAfterServerCert(ss);
    }

    ssl_Release1stHandshakeLock(ss);
    return rv;
}
