/*
 * Various SSL functions.
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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
/* $Id: sslsecur.c,v 1.31 2005/08/16 03:42:26 nelsonb%netscape.com Exp $ */
#include "cert.h"
#include "secitem.h"
#include "keyhi.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "secoid.h"	/* for SECOID_GetALgorithmTag */
#include "pk11func.h"	/* for PK11_GenerateRandom */

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
	    ss->gs.recordLen = 0;
	    ssl_ReleaseRecvBufLock(ss);

	    SSL_TRC(3, ("%d: SSL[%d]: handshake is completed",
			SSL_GETPID(), ss->fd));
            /* call handshake callback for ssl v2 */
	    /* for v3 this is done in ssl3_HandleFinished() */
	    if ((ss->handshakeCallback != NULL) && /* has callback */
		(!ss->firstHsDone) &&              /* only first time */
		(ss->version < SSL_LIBRARY_VERSION_3_0)) {  /* not ssl3 */
		ss->firstHsDone     = PR_TRUE;
		(ss->handshakeCallback)(ss->fd, ss->handshakeCallbackData);
	    }
	    ss->firstHsDone         = PR_TRUE;
	    ss->gs.writeOffset = 0;
	    ss->gs.readOffset  = 0;
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
static SECStatus
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
    if (!ss->firstHsDone) {
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
    SECStatus status;
    PRNetAddr addr;

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

    ss->firstHsDone = PR_FALSE;
    if ( asServer ) {
	ss->handshake = ssl2_BeginServerHandshake;
	ss->handshaking = sslHandshakingAsServer;
    } else {
	ss->handshake = ssl2_BeginClientHandshake;
	ss->handshaking = sslHandshakingAsClient;
    }
    ss->nextHandshake       = 0;
    ss->securityHandshake   = 0;

    ssl_GetRecvBufLock(ss);
    status = ssl_InitGather(&ss->gs);
    ssl_ReleaseRecvBufLock(ss);

    /*
    ** Blow away old security state and get a fresh setup.
    */
    ssl_GetXmitBufLock(ss); 
    ssl_ResetSecurityInfo(&ss->sec);
    status = ssl_CreateSecurityInfo(ss);
    ssl_ReleaseXmitBufLock(ss); 

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    if (!ss->TCPconnected)
	ss->TCPconnected = (PR_SUCCESS == ssl_DefGetpeername(ss, &addr));

    SSL_UNLOCK_WRITER(ss);
    SSL_UNLOCK_READER(ss);

    return status;
}

/* For SSLv2, does nothing but return an error.
** For SSLv3, flushes SID cache entry (if requested),
** and then starts new client hello or hello request.
** Acquires and releases HandshakeLock.
*/
SECStatus
SSL_ReHandshake(PRFileDesc *fd, PRBool flushCache)
{
    sslSocket *ss;
    SECStatus  rv;
    
    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in RedoHandshake", SSL_GETPID(), fd));
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

SECStatus
SSL_RedoHandshake(PRFileDesc *fd)
{
    return SSL_ReHandshake(fd, PR_TRUE);
}

/* Register an application callback to be called when SSL handshake completes.
** Acquires and releases HandshakeLock.
*/
SECStatus
SSL_HandshakeCallback(PRFileDesc *fd, SSLHandshakeCallback cb,
		      void *client_data)
{
    sslSocket *ss;
    
    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in HandshakeCallback",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if (!ss->useSecurity) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

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
SECStatus
SSL_ForceHandshake(PRFileDesc *fd)
{
    sslSocket *ss;
    SECStatus  rv = SECFailure;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in ForceHandshake",
		 SSL_GETPID(), fd));
	return rv;
    }

    /* Don't waste my time */
    if (!ss->useSecurity) 
    	return SECSuccess;

    ssl_Get1stHandshakeLock(ss);

    if (ss->version >= SSL_LIBRARY_VERSION_3_0) {
	int gatherResult;

    	ssl_GetRecvBufLock(ss);
	gatherResult = ssl3_GatherCompleteHandshake(ss, 0);
	ssl_ReleaseRecvBufLock(ss);
	if (gatherResult > 0) {
	    rv = SECSuccess;
	} else if (gatherResult == 0) {
	    PORT_SetError(PR_END_OF_FILE_ERROR);
	} else if (gatherResult == SECWouldBlock) {
	    PORT_SetError(PR_WOULD_BLOCK_ERROR);
	}
    } else if (!ss->firstHsDone) {
	rv = ssl_Do1stHandshake(ss);
    } else {
	/* tried to force handshake on an SSL 2 socket that has 
	** already completed the handshake. */
    	rv = SECSuccess;	/* just pretend we did it. */
    }

    ssl_Release1stHandshakeLock(ss);

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
	unsigned char *newBuf;
	if (b->buf) {
	    newBuf = (unsigned char *) PORT_Realloc(b->buf, newLen);
	} else {
	    newBuf = (unsigned char *) PORT_Alloc(newLen);
	}
	if (!newBuf) {
	    return SECFailure;
	}
	SSL_TRC(10, ("%d: SSL: grow buffer from %d to %d",
		     SSL_GETPID(), b->space, newLen));
	b->buf = newBuf;
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
    int              rv;
    int              amount;
    int              available;

    ssl_GetRecvBufLock(ss);

    available = ss->gs.writeOffset - ss->gs.readOffset;
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
	available = ss->gs.writeOffset - ss->gs.readOffset;
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
    PORT_Memcpy(out, ss->gs.buf.buf + ss->gs.readOffset, amount);
    if (!(flags & PR_MSG_PEEK)) {
	ss->gs.readOffset += amount;
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

  case SEC_OID_X942_DIFFIE_HELMAN_KEY:
    keaType = kt_dh;
    break;
#ifdef NSS_ENABLE_ECC
  case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
    keaType = kt_ecdh;
    break;
#endif /* NSS_ENABLE_ECC */
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
    SECStatus rv;
    sslSocket *ss;
    sslServerCerts  *sc;

    ss = ssl_FindSocket(fd);
    if (!ss) {
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
    if ((kea >= kt_kea_size) || (kea < kt_null)) {
	PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
	return SECFailure;
    }

    if (kea != ssl_FindCertKEAType(cert)) {
    	PORT_SetError(SSL_ERROR_CERT_KEA_MISMATCH);
	return SECFailure;
    }

    sc = ss->serverCerts + kea;
    /* load the server certificate */
    if (sc->serverCert != NULL) {
	CERT_DestroyCertificate(sc->serverCert);
    	sc->serverCert = NULL;
    }
    if (cert) {
	SECKEYPublicKey * pubKey;
	sc->serverCert = CERT_DupCertificate(cert);
	if (!sc->serverCert)
	    goto loser;
    	/* get the size of the cert's public key, and remember it */
	pubKey = CERT_ExtractPublicKey(cert);
	if (!pubKey) 
	    goto loser;
	sc->serverKeyBits = SECKEY_PublicKeyStrengthInBits(pubKey);
	SECKEY_DestroyPublicKey(pubKey); 
	pubKey = NULL;
    }


    /* load the server cert chain */
    if (sc->serverCertChain != NULL) {
	CERT_DestroyCertificateList(sc->serverCertChain);
    	sc->serverCertChain = NULL;
    }
    if (cert) {
	sc->serverCertChain = CERT_CertChainFromCert(
			    sc->serverCert, certUsageSSLServer, PR_TRUE);
	if (sc->serverCertChain == NULL)
	     goto loser;
    }

    /* load the private key */
    if (sc->serverKey != NULL) {
	SECKEY_DestroyPrivateKey(sc->serverKey);
    	sc->serverKey = NULL;
    }
    if (key) {
	sc->serverKey = SECKEY_CopyPrivateKey(key);
	if (sc->serverKey == NULL)
	    goto loser;
	SECKEY_CacheStaticFlags(sc->serverKey);
    }

    if (kea == kt_rsa && cert && sc->serverKeyBits > 512) {
	if (ss->noStepDown) {
	    /* disable all export ciphersuites */
	} else {
	    rv = ssl3_CreateRSAStepDownKeys(ss);
	    if (rv != SECSuccess) {
		return SECFailure; /* err set by ssl3_CreateRSAStepDownKeys */
	    }
	}
    }

    /* Only do this once because it's global. */
    if (ssl3_server_ca_list == NULL)
	ssl3_server_ca_list = CERT_GetSSLCACerts(ss->dbHandle);

    return SECSuccess;

loser:
    if (sc->serverCert != NULL) {
	CERT_DestroyCertificate(sc->serverCert);
	sc->serverCert = NULL;
    }
    if (sc->serverCertChain != NULL) {
	CERT_DestroyCertificateList(sc->serverCertChain);
	sc->serverCertChain = NULL;
    }
    if (sc->serverKey != NULL) {
	SECKEY_DestroyPrivateKey(sc->serverKey);
	sc->serverKey = NULL;
    }
    return SECFailure;
}

/************************************************************************/

SECStatus
ssl_CreateSecurityInfo(sslSocket *ss)
{
    SECStatus status;

    /* initialize sslv2 socket to send data in the clear. */
    ssl2_UseClearSendFunc(ss);

    ss->sec.blockSize  = 1;
    ss->sec.blockShift = 0;

    ssl_GetXmitBufLock(ss); 
    status = sslBuffer_Grow(&ss->sec.writeBuf, 4096);
    ssl_ReleaseXmitBufLock(ss); 

    return status;
}

SECStatus
ssl_CopySecurityInfo(sslSocket *ss, sslSocket *os)
{
    ss->sec.send 		= os->sec.send;
    ss->sec.isServer 		= os->sec.isServer;
    ss->sec.keyBits    		= os->sec.keyBits;
    ss->sec.secretKeyBits 	= os->sec.secretKeyBits;

    ss->sec.peerCert   		= CERT_DupCertificate(os->sec.peerCert);
    if (os->sec.peerCert && !ss->sec.peerCert)
    	goto loser;

    ss->sec.cache      		= os->sec.cache;
    ss->sec.uncache    		= os->sec.uncache;

    /* we don't dup the connection info. */

    ss->sec.sendSequence 	= os->sec.sendSequence;
    ss->sec.rcvSequence 	= os->sec.rcvSequence;

    if (os->sec.hash && os->sec.hashcx) {
	ss->sec.hash 		= os->sec.hash;
	ss->sec.hashcx 		= os->sec.hash->clone(os->sec.hashcx);
	if (os->sec.hashcx && !ss->sec.hashcx)
	    goto loser;
    } else {
	ss->sec.hash 		= NULL;
	ss->sec.hashcx 		= NULL;
    }

    SECITEM_CopyItem(0, &ss->sec.sendSecret, &os->sec.sendSecret);
    if (os->sec.sendSecret.data && !ss->sec.sendSecret.data)
    	goto loser;
    SECITEM_CopyItem(0, &ss->sec.rcvSecret,  &os->sec.rcvSecret);
    if (os->sec.rcvSecret.data && !ss->sec.rcvSecret.data)
    	goto loser;

    /* XXX following code is wrong if either cx != 0 */
    PORT_Assert(os->sec.readcx  == 0);
    PORT_Assert(os->sec.writecx == 0);
    ss->sec.readcx     		= os->sec.readcx;
    ss->sec.writecx    		= os->sec.writecx;
    ss->sec.destroy    		= 0;	

    ss->sec.enc        		= os->sec.enc;
    ss->sec.dec        		= os->sec.dec;

    ss->sec.blockShift 		= os->sec.blockShift;
    ss->sec.blockSize  		= os->sec.blockSize;

    return SECSuccess;

loser:
    return SECFailure;
}

/* Reset sec back to its initial state.
** Caller holds any relevant locks.
*/
void 
ssl_ResetSecurityInfo(sslSecurityInfo *sec)
{
    /* Destroy MAC */
    if (sec->hash && sec->hashcx) {
	(*sec->hash->destroy)(sec->hashcx, PR_TRUE);
	sec->hashcx = NULL;
	sec->hash = NULL;
    }
    SECITEM_ZfreeItem(&sec->sendSecret, PR_FALSE);
    SECITEM_ZfreeItem(&sec->rcvSecret, PR_FALSE);

    /* Destroy ciphers */
    if (sec->destroy) {
	(*sec->destroy)(sec->readcx, PR_TRUE);
	(*sec->destroy)(sec->writecx, PR_TRUE);
	sec->readcx = NULL;
	sec->writecx = NULL;
    } else {
	PORT_Assert(sec->readcx == 0);
	PORT_Assert(sec->writecx == 0);
    }
    sec->readcx = 0;
    sec->writecx = 0;

    if (sec->localCert) {
	CERT_DestroyCertificate(sec->localCert);
	sec->localCert = NULL;
    }
    if (sec->peerCert) {
	CERT_DestroyCertificate(sec->peerCert);
	sec->peerCert = NULL;
    }
    if (sec->peerKey) {
	SECKEY_DestroyPublicKey(sec->peerKey);
	sec->peerKey = NULL;
    }

    /* cleanup the ci */
    if (sec->ci.sid != NULL) {
	ssl_FreeSID(sec->ci.sid);
    }
    PORT_ZFree(sec->ci.sendBuf.buf, sec->ci.sendBuf.space);
    memset(&sec->ci, 0, sizeof sec->ci);
}

/*
** Called from SSL_ResetHandshake (above), and 
**        from ssl_FreeSocket     in sslsock.c
** Caller should hold relevant locks (e.g. XmitBufLock)
*/
void 
ssl_DestroySecurityInfo(sslSecurityInfo *sec)
{
    ssl_ResetSecurityInfo(sec);

    PORT_ZFree(sec->writeBuf.buf, sec->writeBuf.space);
    sec->writeBuf.buf = 0;

    memset(sec, 0, sizeof *sec);
}

/************************************************************************/

int 
ssl_SecureConnect(sslSocket *ss, const PRNetAddr *sa)
{
    PRFileDesc *osfd = ss->fd->lower;
    int rv;

    if ( ss->handshakeAsServer ) {
	ss->securityHandshake = ssl2_BeginServerHandshake;
	ss->handshaking = sslHandshakingAsServer;
    } else {
	ss->securityHandshake = ssl2_BeginClientHandshake;
	ss->handshaking = sslHandshakingAsClient;
    }

    /* connect to server */
    rv = osfd->methods->connect(osfd, sa, ss->cTimeout);
    if (rv == PR_SUCCESS) {
	ss->TCPconnected = 1;
    } else {
	int err = PR_GetError();
	SSL_DBG(("%d: SSL[%d]: connect failed, errno=%d",
		 SSL_GETPID(), ss->fd, err));
	if (err == PR_IS_CONNECTED_ERROR) {
	    ss->TCPconnected = 1;
	} 
    }

    SSL_TRC(5, ("%d: SSL[%d]: secure connect completed, rv == %d",
		SSL_GETPID(), ss->fd, rv));
    return rv;
}

int
ssl_SecureClose(sslSocket *ss)
{
    int rv;

    if (ss->version >= SSL_LIBRARY_VERSION_3_0 	&&
    	ss->firstHsDone 			&& 
	!(ss->shutdownHow & ssl_SHUTDOWN_SEND)	&&
	!ss->recvdCloseNotify                   &&
	(ss->ssl3 != NULL)) {

	/* We don't want the final alert to be Nagle delayed. */
	if (!ss->delayDisabled) {
	    ssl_EnableNagleDelay(ss, PR_FALSE);
	    ss->delayDisabled = 1;
	}

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
	ss->firstHsDone 				&& 
	!ss->recvdCloseNotify                   	&&
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

    sec = &ss->sec;

    if (ss->shutdownHow & ssl_SHUTDOWN_RCV) {
	PORT_SetError(PR_SOCKET_SHUTDOWN_ERROR);
    	return PR_FAILURE;
    }
    if (flags & ~PR_MSG_PEEK) {
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
    if (!ss->firstHsDone) {
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

/* Caller holds the SSL Socket's write lock. SSL_LOCK_WRITER(ss) */
int
ssl_SecureSend(sslSocket *ss, const unsigned char *buf, int len, int flags)
{
    int              rv		= 0;

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
	if (rv >= 0 && ss->pendingBuf.len != 0) {
	    PORT_Assert(ss->pendingBuf.len > 0);
	    PORT_SetError(PR_WOULD_BLOCK_ERROR);
	    rv = SECFailure;
	}
    }
    ssl_ReleaseXmitBufLock(ss);
    if (rv < 0) {
	return rv;
    }

    if (len > 0) 
    	ss->writerThread = PR_GetCurrentThread();
    /* If any of these is non-zero, the initial handshake is not done. */
    if (!ss->firstHsDone) {
	ssl_Get1stHandshakeLock(ss);
	if (ss->handshake || ss->nextHandshake || ss->securityHandshake) {
	    rv = ssl_Do1stHandshake(ss);
	}
	ssl_Release1stHandshakeLock(ss);
    }
    if (rv < 0) {
    	ss->writerThread = NULL;
	return rv;
    }

    /* Check for zero length writes after we do housekeeping so we make forward
     * progress.
     */
    if (len == 0) {
    	return 0;
    }
    PORT_Assert(buf != NULL);
    
    SSL_TRC(2, ("%d: SSL[%d]: SecureSend: sending %d bytes",
		SSL_GETPID(), ss->fd, len));

    /* Send out the data using one of these functions:
     *	ssl2_SendClear, ssl2_SendStream, ssl2_SendBlock, 
     *  ssl3_SendApplicationData
     */
    ssl_GetXmitBufLock(ss);
    rv = (*ss->sec.send)(ss, buf, len, flags);
    ssl_ReleaseXmitBufLock(ss);
    ss->writerThread = NULL;
    return rv;
}

int
ssl_SecureWrite(sslSocket *ss, const unsigned char *buf, int len)
{
    return ssl_SecureSend(ss, buf, len, 0);
}

SECStatus
SSL_BadCertHook(PRFileDesc *fd, SSLBadCertHandler f, void *arg)
{
    sslSocket *ss;
    
    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSLBadCertHook",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    ss->handleBadCert = f;
    ss->badCertArg = arg;

    return SECSuccess;
}

/*
 * Allow the application to pass the url or hostname into the SSL library
 * so that we can do some checking on it.
 */
SECStatus
SSL_SetURL(PRFileDesc *fd, const char *url)
{
    sslSocket *   ss = ssl_FindSocket(fd);
    SECStatus     rv = SECSuccess;

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

	ssl_GetRecvBufLock(ss);
	rv = ss->gs.writeOffset - ss->gs.readOffset;
	ssl_ReleaseRecvBufLock(ss);

	ssl_ReleaseSSL3HandshakeLock(ss);
	ssl_Release1stHandshakeLock(ss);
    }

    return rv;
}

SECStatus
SSL_InvalidateSession(PRFileDesc *fd)
{
    sslSocket *   ss = ssl_FindSocket(fd);
    SECStatus     rv = SECFailure;

    if (ss) {
	ssl_Get1stHandshakeLock(ss);
	ssl_GetSSL3HandshakeLock(ss);

	if (ss->sec.ci.sid) {
	    ss->sec.uncache(ss->sec.ci.sid);
	    rv = SECSuccess;
	}

	ssl_ReleaseSSL3HandshakeLock(ss);
	ssl_Release1stHandshakeLock(ss);
    }
    return rv;
}

SECItem *
SSL_GetSessionID(PRFileDesc *fd)
{
    sslSocket *    ss;
    SECItem *      item = NULL;

    ss = ssl_FindSocket(fd);
    if (ss) {
	ssl_Get1stHandshakeLock(ss);
	ssl_GetSSL3HandshakeLock(ss);

	if (ss->useSecurity && ss->firstHsDone && ss->sec.ci.sid) {
	    item = (SECItem *)PORT_Alloc(sizeof(SECItem));
	    if (item) {
		sslSessionID * sid = ss->sec.ci.sid;
		if (sid->version < SSL_LIBRARY_VERSION_3_0) {
		    item->len = SSL2_SESSIONID_BYTES;
		    item->data = (unsigned char*)PORT_Alloc(item->len);
		    PORT_Memcpy(item->data, sid->u.ssl2.sessionID, item->len);
		} else {
		    item->len = sid->u.ssl3.sessionIDLength;
		    item->data = (unsigned char*)PORT_Alloc(item->len);
		    PORT_Memcpy(item->data, sid->u.ssl3.sessionID, item->len);
		}
	    }
	}

	ssl_ReleaseSSL3HandshakeLock(ss);
	ssl_Release1stHandshakeLock(ss);
    }
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
