/*
 * Various SSL functions.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */
#include "cert.h"
#include "secitem.h"
#include "keyhi.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "secoid.h"	/* for SECOID_GetALgorithmTag */
#include "pk11func.h"	/* for PK11_GenerateRandom */
#include "nss.h"        /* for NSS_RegisterShutdown */
#include "prinit.h"     /* for PR_CallOnceWithArg */

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
 * -	ssl2_HandleMessage() -> ssl2_HandleRequestCertificate() ->
 *	ss->getClientAuthData()
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
	PORT_Assert(ss->opt.noLocks ||  ssl_Have1stHandshakeLock(ss) );
	PORT_Assert(ss->opt.noLocks || !ssl_HaveRecvBufLock(ss));
	PORT_Assert(ss->opt.noLocks || !ssl_HaveXmitBufLock(ss));
	PORT_Assert(ss->opt.noLocks || !ssl_HaveSSL3HandshakeLock(ss));

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

    PORT_Assert(ss->opt.noLocks || !ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || !ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || !ssl_HaveSSL3HandshakeLock(ss));

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
ssl3_AlwaysBlock(sslSocket *ss)
{
    PORT_SetError(PR_WOULD_BLOCK_ERROR);	/* perhaps redundant. */
    return SECWouldBlock;
}

/*
 * set the initial handshake state machine to block
 */
void
ssl3_SetAlwaysBlock(sslSocket *ss)
{
    if (!ss->firstHsDone) {
	ss->handshake = ssl3_AlwaysBlock;
	ss->nextHandshake = 0;
    }
}

static SECStatus 
ssl_SetTimeout(PRFileDesc *fd, PRIntervalTime timeout)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SetTimeout", SSL_GETPID(), fd));
	return SECFailure;
    }
    SSL_LOCK_READER(ss);
    ss->rTimeout = timeout;
    if (ss->opt.fdx) {
        SSL_LOCK_WRITER(ss);
    }
    ss->wTimeout = timeout;
    if (ss->opt.fdx) {
        SSL_UNLOCK_WRITER(ss);
    }
    SSL_UNLOCK_READER(ss);
    return SECSuccess;
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
    if (!ss->opt.useSecurity)
	return SECSuccess;

    SSL_LOCK_READER(ss);
    SSL_LOCK_WRITER(ss);

    /* Reset handshake state */
    ssl_Get1stHandshakeLock(ss);

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

    ssl_GetSSL3HandshakeLock(ss);

    /*
    ** Blow away old security state and get a fresh setup.
    */
    ssl_GetXmitBufLock(ss); 
    ssl_ResetSecurityInfo(&ss->sec, PR_TRUE);
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

    if (!ss->opt.useSecurity)
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

/*
** Same as above, but with an I/O timeout.
 */
SSL_IMPORT SECStatus SSL_ReHandshakeWithTimeout(PRFileDesc *fd,
                                                PRBool flushCache,
                                                PRIntervalTime timeout)
{
    if (SECSuccess != ssl_SetTimeout(fd, timeout)) {
        return SECFailure;
    }
    return SSL_ReHandshake(fd, flushCache);
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

    if (!ss->opt.useSecurity) {
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
    if (!ss->opt.useSecurity) 
    	return SECSuccess;

    if (!ssl_SocketIsBlocking(ss)) {
	ssl_GetXmitBufLock(ss);
	if (ss->pendingBuf.len != 0) {
	    int sent = ssl_SendSavedWriteData(ss);
	    if ((sent < 0) && (PORT_GetError() != PR_WOULD_BLOCK_ERROR)) {
		ssl_ReleaseXmitBufLock(ss);
		return SECFailure;
	    }
	}
	ssl_ReleaseXmitBufLock(ss);
    }

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

/*
 ** Same as above, but with an I/O timeout.
 */
SSL_IMPORT SECStatus SSL_ForceHandshakeWithTimeout(PRFileDesc *fd,
                                                   PRIntervalTime timeout)
{
    if (SECSuccess != ssl_SetTimeout(fd, timeout)) {
        return SECFailure;
    }
    return SSL_ForceHandshake(fd);
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
    newLen = PR_MAX(newLen, MAX_FRAGMENT_LENGTH + 2048);
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

SECStatus 
sslBuffer_Append(sslBuffer *b, const void * data, unsigned int len)
{
    unsigned int newLen = b->len + len;
    SECStatus rv;

    rv = sslBuffer_Grow(b, newLen);
    if (rv != SECSuccess)
    	return rv;
    PORT_Memcpy(b->buf + b->len, data, len);
    b->len += len;
    return SECSuccess;
}

/*
** Save away write data that is trying to be written before the security
** handshake has been completed. When the handshake is completed, we will
** flush this data out.
** Caller must hold xmitBufLock
*/
SECStatus 
ssl_SaveWriteData(sslSocket *ss, const void *data, unsigned int len)
{
    SECStatus    rv;

    PORT_Assert( ss->opt.noLocks || ssl_HaveXmitBufLock(ss) );
    rv = sslBuffer_Append(&ss->pendingBuf, data, len);
    SSL_TRC(5, ("%d: SSL[%d]: saving %u bytes of data (%u total saved so far)",
		 SSL_GETPID(), ss->fd, len, ss->pendingBuf.len));
    return rv;
}

/*
** Send saved write data. This will flush out data sent prior to a
** complete security handshake. Hopefully there won't be too much of it.
** Returns count of the bytes sent, NOT a SECStatus.
** Caller must hold xmitBufLock
*/
int 
ssl_SendSavedWriteData(sslSocket *ss)
{
    int rv	= 0;

    PORT_Assert( ss->opt.noLocks || ssl_HaveXmitBufLock(ss) );
    if (ss->pendingBuf.len != 0) {
	SSL_TRC(5, ("%d: SSL[%d]: sending %d bytes of saved data",
		     SSL_GETPID(), ss->fd, ss->pendingBuf.len));
	rv = ssl_DefSend(ss, ss->pendingBuf.buf, ss->pendingBuf.len, 0);
	if (rv < 0) {
	    return rv;
	} 
	ss->pendingBuf.len -= rv;
	if (ss->pendingBuf.len > 0 && rv > 0) {
	    /* UGH !! This shifts the whole buffer down by copying it */
	    PORT_Memmove(ss->pendingBuf.buf, ss->pendingBuf.buf + rv, 
	                 ss->pendingBuf.len);
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
    PORT_Assert(ss->gs.readOffset <= ss->gs.writeOffset);
    rv = amount;

    SSL_TRC(30, ("%d: SSL[%d]: amount=%d available=%d",
		 SSL_GETPID(), ss->fd, amount, available));
    PRINT_BUF(4, (ss, "DoRecv receiving plaintext:", out, amount));

done:
    ssl_ReleaseRecvBufLock(ss);
    return rv;
}

/************************************************************************/

/*
** Return SSLKEAType derived from cert's Public Key algorithm info.
*/
SSLKEAType
NSS_FindCertKEAType(CERTCertificate * cert)
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

static const PRCallOnceType pristineCallOnce;
static       PRCallOnceType setupServerCAListOnce;

static SECStatus serverCAListShutdown(void* appData, void* nssData)
{
    PORT_Assert(ssl3_server_ca_list);
    if (ssl3_server_ca_list) {
	CERT_FreeDistNames(ssl3_server_ca_list);
	ssl3_server_ca_list = NULL;
    }
    setupServerCAListOnce = pristineCallOnce;
    return SECSuccess;
}

static PRStatus serverCAListSetup(void *arg)
{
    CERTCertDBHandle *dbHandle = (CERTCertDBHandle *)arg;
    SECStatus rv = NSS_RegisterShutdown(serverCAListShutdown, NULL);
    PORT_Assert(SECSuccess == rv);
    if (SECSuccess == rv) {
	ssl3_server_ca_list = CERT_GetSSLCACerts(dbHandle);
	return PR_SUCCESS;
    }
    return PR_FAILURE;
}

SECStatus
ssl_ConfigSecureServer(sslSocket *ss, CERTCertificate *cert,
                       const CERTCertificateList *certChain,
                       ssl3KeyPair *keyPair, SSLKEAType kea)
{
    CERTCertificateList *localCertChain = NULL;
    sslServerCerts  *sc = ss->serverCerts + kea;

    /* load the server certificate */
    if (sc->serverCert != NULL) {
	CERT_DestroyCertificate(sc->serverCert);
    	sc->serverCert = NULL;
        sc->serverKeyBits = 0;
    }
    /* load the server cert chain */
    if (sc->serverCertChain != NULL) {
	CERT_DestroyCertificateList(sc->serverCertChain);
    	sc->serverCertChain = NULL;
    }
    if (cert) {
        sc->serverCert = CERT_DupCertificate(cert);
        /* get the size of the cert's public key, and remember it */
        sc->serverKeyBits = SECKEY_PublicKeyStrengthInBits(keyPair->pubKey);
        if (!certChain) {
            localCertChain =
                CERT_CertChainFromCert(sc->serverCert, certUsageSSLServer,
                                       PR_TRUE);
            if (!localCertChain)
                goto loser;
        }
        sc->serverCertChain = (certChain) ? CERT_DupCertList(certChain) :
                                            localCertChain;
        if (!sc->serverCertChain) {
            goto loser;
        }
        localCertChain = NULL;      /* consumed */
    }

    /* get keyPair */
    if (sc->serverKeyPair != NULL) {
        ssl3_FreeKeyPair(sc->serverKeyPair);
        sc->serverKeyPair = NULL;
    }
    if (keyPair) {
        SECKEY_CacheStaticFlags(keyPair->privKey);
        sc->serverKeyPair = ssl3_GetKeyPairRef(keyPair);
    }
    if (kea == kt_rsa && cert && sc->serverKeyBits > 512 &&
        !ss->opt.noStepDown && !ss->stepDownKeyPair) { 
        if (ssl3_CreateRSAStepDownKeys(ss) != SECSuccess) {
            goto loser;
        }
    }
    return SECSuccess;

loser:
    if (localCertChain) {
        CERT_DestroyCertificateList(localCertChain);
    }
    if (sc->serverCert != NULL) {
	CERT_DestroyCertificate(sc->serverCert);
	sc->serverCert = NULL;
    }
    if (sc->serverCertChain != NULL) {
	CERT_DestroyCertificateList(sc->serverCertChain);
	sc->serverCertChain = NULL;
    }
    if (sc->serverKeyPair != NULL) {
	ssl3_FreeKeyPair(sc->serverKeyPair);
	sc->serverKeyPair = NULL;
    }
    return SECFailure;
}

/* XXX need to protect the data that gets changed here.!! */

SECStatus
SSL_ConfigSecureServer(PRFileDesc *fd, CERTCertificate *cert,
		       SECKEYPrivateKey *key, SSL3KEAType kea)
{

    return SSL_ConfigSecureServerWithCertChain(fd, cert, NULL, key, kea);
}

SECStatus
SSL_ConfigSecureServerWithCertChain(PRFileDesc *fd, CERTCertificate *cert,
                                    const CERTCertificateList *certChainOpt,
                                    SECKEYPrivateKey *key, SSL3KEAType kea)
{
    sslSocket *ss;
    SECKEYPublicKey *pubKey = NULL;
    ssl3KeyPair *keyPair = NULL;
    SECStatus rv = SECFailure;

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

    if (kea != NSS_FindCertKEAType(cert)) {
    	PORT_SetError(SSL_ERROR_CERT_KEA_MISMATCH);
	return SECFailure;
    }

    if (cert) {
    	/* get the size of the cert's public key, and remember it */
	pubKey = CERT_ExtractPublicKey(cert);
	if (!pubKey) 
            return SECFailure;
    }

    if (key) {
	SECKEYPrivateKey * keyCopy	= NULL;
	CK_MECHANISM_TYPE  keyMech	= CKM_INVALID_MECHANISM;

	if (key->pkcs11Slot) {
	    PK11SlotInfo * bestSlot;
	    bestSlot = PK11_ReferenceSlot(key->pkcs11Slot);
	    if (bestSlot) {
		keyCopy = PK11_CopyTokenPrivKeyToSessionPrivKey(bestSlot, key);
		PK11_FreeSlot(bestSlot);
	    }
	}
	if (keyCopy == NULL)
	    keyMech = PK11_MapSignKeyType(key->keyType);
	if (keyMech != CKM_INVALID_MECHANISM) {
	    PK11SlotInfo * bestSlot;
	    /* XXX Maybe should be bestSlotMultiple? */
	    bestSlot = PK11_GetBestSlot(keyMech, NULL /* wincx */);
	    if (bestSlot) {
		keyCopy = PK11_CopyTokenPrivKeyToSessionPrivKey(bestSlot, key);
		PK11_FreeSlot(bestSlot);
	    }
	}
	if (keyCopy == NULL)
	    keyCopy = SECKEY_CopyPrivateKey(key);
	if (keyCopy == NULL)
	    goto loser;
        keyPair = ssl3_NewKeyPair(keyCopy, pubKey);
        if (keyPair == NULL) {
            SECKEY_DestroyPrivateKey(keyCopy);
            goto loser;
        }
	pubKey = NULL; /* adopted by serverKeyPair */
    }
    if (ssl_ConfigSecureServer(ss, cert, certChainOpt,
                               keyPair, kea) == SECFailure) {
        goto loser;
    }

    /* Only do this once because it's global. */
    if (PR_SUCCESS == PR_CallOnceWithArg(&setupServerCAListOnce, 
                                         &serverCAListSetup,
                                         (void *)(ss->dbHandle))) {
        rv = SECSuccess;
    }

loser:
    if (keyPair) {
        ssl3_FreeKeyPair(keyPair);
    }
    if (pubKey) {
	SECKEY_DestroyPublicKey(pubKey); 
	pubKey = NULL;
    }
    return rv;
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
ssl_ResetSecurityInfo(sslSecurityInfo *sec, PRBool doMemset)
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
    if (doMemset) {
        memset(&sec->ci, 0, sizeof sec->ci);
    }
    
}

/*
** Called from SSL_ResetHandshake (above), and 
**        from ssl_FreeSocket     in sslsock.c
** Caller should hold relevant locks (e.g. XmitBufLock)
*/
void 
ssl_DestroySecurityInfo(sslSecurityInfo *sec)
{
    ssl_ResetSecurityInfo(sec, PR_FALSE);

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

    if ( ss->opt.handshakeAsServer ) {
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

/*
 * The TLS 1.2 RFC 5246, Section 7.2.1 says:
 *
 *     Unless some other fatal alert has been transmitted, each party is
 *     required to send a close_notify alert before closing the write side
 *     of the connection.  The other party MUST respond with a close_notify
 *     alert of its own and close down the connection immediately,
 *     discarding any pending writes.  It is not required for the initiator
 *     of the close to wait for the responding close_notify alert before
 *     closing the read side of the connection.
 *
 * The second sentence requires that we send a close_notify alert when we
 * have received a close_notify alert.  In practice, all SSL implementations
 * close the socket immediately after sending a close_notify alert (which is
 * allowed by the third sentence), so responding with a close_notify alert
 * would result in a write failure with the ECONNRESET error.  This is why
 * we don't respond with a close_notify alert.
 *
 * Also, in the unlikely event that the TCP pipe is full and the peer stops
 * reading, the SSL3_SendAlert call in ssl_SecureClose and ssl_SecureShutdown
 * may block indefinitely in blocking mode, and may fail (without retrying)
 * in non-blocking mode.
 */

int
ssl_SecureClose(sslSocket *ss)
{
    int rv;

    if (ss->version >= SSL_LIBRARY_VERSION_3_0 	&&
	!(ss->shutdownHow & ssl_SHUTDOWN_SEND)	&&
    	ss->firstHsDone 			&& 
	!ss->recvdCloseNotify                   &&
	ss->ssl3.initialized) {

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
    	ss->version >= SSL_LIBRARY_VERSION_3_0		&&
	!(ss->shutdownHow & ssl_SHUTDOWN_SEND)		&&
	ss->firstHsDone 				&& 
	!ss->recvdCloseNotify                   	&&
	ss->ssl3.initialized) {

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

    if (!ssl_SocketIsBlocking(ss) && !ss->opt.fdx) {
	ssl_GetXmitBufLock(ss);
	if (ss->pendingBuf.len != 0) {
	    rv = ssl_SendSavedWriteData(ss);
	    if ((rv < 0) && (PORT_GetError() != PR_WOULD_BLOCK_ERROR)) {
		ssl_ReleaseXmitBufLock(ss);
		return SECFailure;
	    }
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

    SSL_TRC(2, ("%d: SSL[%d]: SecureSend: sending %d bytes",
		SSL_GETPID(), ss->fd, len));

    if (ss->shutdownHow & ssl_SHUTDOWN_SEND) {
	PORT_SetError(PR_SOCKET_SHUTDOWN_ERROR);
    	rv = PR_FAILURE;
	goto done;
    }
    if (flags) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
    	rv = PR_FAILURE;
	goto done;
    }

    ssl_GetXmitBufLock(ss);
    if (ss->pendingBuf.len != 0) {
	PORT_Assert(ss->pendingBuf.len > 0);
	rv = ssl_SendSavedWriteData(ss);
	if (rv >= 0 && ss->pendingBuf.len != 0) {
	    PORT_Assert(ss->pendingBuf.len > 0);
	    PORT_SetError(PR_WOULD_BLOCK_ERROR);
	    rv = SECFailure;
	}
    }
    ssl_ReleaseXmitBufLock(ss);
    if (rv < 0) {
	goto done;
    }

    if (len > 0) 
    	ss->writerThread = PR_GetCurrentThread();
    /* If any of these is non-zero, the initial handshake is not done. */
    if (!ss->firstHsDone) {
	PRBool canFalseStart = PR_FALSE;
	ssl_Get1stHandshakeLock(ss);
	if (ss->version >= SSL_LIBRARY_VERSION_3_0) {
	    ssl_GetSSL3HandshakeLock(ss);
	    if ((ss->ssl3.hs.ws == wait_change_cipher ||
		ss->ssl3.hs.ws == wait_finished ||
		ss->ssl3.hs.ws == wait_new_session_ticket) &&
		ssl3_CanFalseStart(ss)) {
		canFalseStart = PR_TRUE;
	    }
	    ssl_ReleaseSSL3HandshakeLock(ss);
	}
	if (!canFalseStart &&
	    (ss->handshake || ss->nextHandshake || ss->securityHandshake)) {
	    rv = ssl_Do1stHandshake(ss);
	}
	ssl_Release1stHandshakeLock(ss);
    }
    if (rv < 0) {
    	ss->writerThread = NULL;
	goto done;
    }

    /* Check for zero length writes after we do housekeeping so we make forward
     * progress.
     */
    if (len == 0) {
    	rv = 0;
	goto done;
    }
    PORT_Assert(buf != NULL);
    if (!buf) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
    	rv = PR_FAILURE;
	goto done;
    }

    /* Send out the data using one of these functions:
     *	ssl2_SendClear, ssl2_SendStream, ssl2_SendBlock, 
     *  ssl3_SendApplicationData
     */
    ssl_GetXmitBufLock(ss);
    rv = (*ss->sec.send)(ss, buf, len, flags);
    ssl_ReleaseXmitBufLock(ss);
    ss->writerThread = NULL;
done:
    if (rv < 0) {
	SSL_TRC(2, ("%d: SSL[%d]: SecureSend: returning %d count, error %d",
		    SSL_GETPID(), ss->fd, rv, PORT_GetError()));
    } else {
	SSL_TRC(2, ("%d: SSL[%d]: SecureSend: returning %d count",
		    SSL_GETPID(), ss->fd, rv));
    }
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
 * so that we can do some checking on it. It will be used for the value in
 * SNI extension of client hello message.
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
 * Allow the application to pass the set of trust anchors
 */
SECStatus
SSL_SetTrustAnchors(PRFileDesc *fd, CERTCertList *certList)
{
    sslSocket *   ss = ssl_FindSocket(fd);
    CERTDistNames *names = NULL;

    if (!certList) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetTrustAnchors",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    names = CERT_DistNamesFromCertList(certList);
    if (names == NULL) {
        return SECFailure;
    }
    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);
    if (ss->ssl3.ca_list) {
        CERT_FreeDistNames(ss->ssl3.ca_list);
    }
    ss->ssl3.ca_list = names;
    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return SECSuccess;
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

    if (ss && ss->opt.useSecurity) {
	ssl_GetRecvBufLock(ss);
	rv = ss->gs.writeOffset - ss->gs.readOffset;
	ssl_ReleaseRecvBufLock(ss);
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

	if (ss->sec.ci.sid && ss->sec.uncache) {
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

	if (ss->opt.useSecurity && ss->firstHsDone && ss->sec.ci.sid) {
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

/* DO NOT USE. This function was exported in ssl.def with the wrong signature;
 * this implementation exists to maintain link-time compatibility.
 */
int
SSL_RestartHandshakeAfterCertReq(sslSocket *         ss,
				CERTCertificate *    cert, 
				SECKEYPrivateKey *   key,
				CERTCertificateList *certChain)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return -1;
}

/* DO NOT USE. This function was exported in ssl.def with the wrong signature;
 * this implementation exists to maintain link-time compatibility.
 */
int
SSL_RestartHandshakeAfterServerCert(sslSocket * ss)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return -1;
}

/* See documentation in ssl.h */
SECStatus
SSL_AuthCertificateComplete(PRFileDesc *fd, PRErrorCode error)
{
    SECStatus rv;
    sslSocket *ss = ssl_FindSocket(fd);

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_AuthCertificateComplete",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    ssl_Get1stHandshakeLock(ss);

    if (!ss->ssl3.initialized) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	rv = SECFailure;
    } else if (ss->version < SSL_LIBRARY_VERSION_3_0) {
	PORT_SetError(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SSL2);
	rv = SECFailure;
    } else {
	rv = ssl3_AuthCertificateComplete(ss, error);
    }

    ssl_Release1stHandshakeLock(ss);

    return rv;
}

/* For more info see ssl.h */
SECStatus 
SSL_SNISocketConfigHook(PRFileDesc *fd, SSLSNISocketConfig func,
                        void *arg)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SNISocketConfigHook",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    ss->sniSocketConfig = func;
    ss->sniSocketConfigArg = arg;
    return SECSuccess;
}
