/* 
 * SSL v2 handshake functions, and functions common to SSL2 and SSL3.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nssrenam.h"
#include "cert.h"
#include "secitem.h"
#include "sechash.h"
#include "cryptohi.h"		/* for SGN_ funcs */
#include "keyhi.h" 		/* for SECKEY_ high level functions. */
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "pk11func.h"
#include "prinit.h"
#include "prtime.h" 	/* for PR_Now() */

static PRBool policyWasSet;

/* This ordered list is indexed by (SSL_CK_xx * 3)   */
/* Second and third bytes are MSB and LSB of master key length. */
static const PRUint8 allCipherSuites[] = {
    0,						0,    0,
    SSL_CK_RC4_128_WITH_MD5,			0x00, 0x80,
    SSL_CK_RC4_128_EXPORT40_WITH_MD5,		0x00, 0x80,
    SSL_CK_RC2_128_CBC_WITH_MD5,		0x00, 0x80,
    SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5,	0x00, 0x80,
    SSL_CK_IDEA_128_CBC_WITH_MD5,		0x00, 0x80,
    SSL_CK_DES_64_CBC_WITH_MD5,			0x00, 0x40,
    SSL_CK_DES_192_EDE3_CBC_WITH_MD5,		0x00, 0xC0,
    0,						0,    0
};

#define ssl2_NUM_SUITES_IMPLEMENTED 6

/* This list is sent back to the client when the client-hello message 
 * contains no overlapping ciphers, so the client can report what ciphers
 * are supported by the server.  Unlike allCipherSuites (above), this list
 * is sorted by descending preference, not by cipherSuite number. 
 */
static const PRUint8 implementedCipherSuites[ssl2_NUM_SUITES_IMPLEMENTED * 3] = {
    SSL_CK_RC4_128_WITH_MD5,			0x00, 0x80,
    SSL_CK_RC2_128_CBC_WITH_MD5,		0x00, 0x80,
    SSL_CK_DES_192_EDE3_CBC_WITH_MD5,		0x00, 0xC0,
    SSL_CK_DES_64_CBC_WITH_MD5,			0x00, 0x40,
    SSL_CK_RC4_128_EXPORT40_WITH_MD5,		0x00, 0x80,
    SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5,	0x00, 0x80
};

typedef struct ssl2SpecsStr {
    PRUint8           nkm; /* do this many hashes to generate key material. */
    PRUint8           nkd; /* size of readKey and writeKey in bytes. */
    PRUint8           blockSize;
    PRUint8           blockShift;
    CK_MECHANISM_TYPE mechanism;
    PRUint8           keyLen;	/* cipher symkey size in bytes. */
    PRUint8           pubLen;	/* publicly reveal this many bytes of key. */
    PRUint8           ivLen;	/* length of IV data at *ca.	*/
} ssl2Specs;

static const ssl2Specs ssl_Specs[] = {
/* NONE                                 */ 
				{  0,  0, 0, 0, },
/* SSL_CK_RC4_128_WITH_MD5		*/ 
				{  2, 16, 1, 0, CKM_RC4,       16,   0, 0, },
/* SSL_CK_RC4_128_EXPORT40_WITH_MD5	*/ 
				{  2, 16, 1, 0, CKM_RC4,       16,  11, 0, },
/* SSL_CK_RC2_128_CBC_WITH_MD5		*/ 
				{  2, 16, 8, 3, CKM_RC2_CBC,   16,   0, 8, },
/* SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5	*/ 
				{  2, 16, 8, 3, CKM_RC2_CBC,   16,  11, 8, },
/* SSL_CK_IDEA_128_CBC_WITH_MD5		*/ 
				{  0,  0, 0, 0, },
/* SSL_CK_DES_64_CBC_WITH_MD5		*/ 
				{  1,  8, 8, 3, CKM_DES_CBC,    8,   0, 8, },
/* SSL_CK_DES_192_EDE3_CBC_WITH_MD5	*/ 
				{  3, 24, 8, 3, CKM_DES3_CBC,  24,   0, 8, },
};

#define SET_ERROR_CODE	  /* reminder */
#define TEST_FOR_FAILURE  /* reminder */

/*
** Put a string tag in the library so that we can examine an executable
** and see what kind of security it supports.
*/
const char *ssl_version = "SECURITY_VERSION:"
			" +us"
			" +export"
#ifdef TRACE
			" +trace"
#endif
#ifdef DEBUG
			" +debug"
#endif
			;

const char * const ssl_cipherName[] = {
    "unknown",
    "RC4",
    "RC4-Export",
    "RC2-CBC",
    "RC2-CBC-Export",
    "IDEA-CBC",
    "DES-CBC",
    "DES-EDE3-CBC",
    "unknown",
    "unknown", /* was fortezza, NO LONGER USED */
};


/* bit-masks, showing which SSLv2 suites are allowed.
 * lsb corresponds to first cipher suite in allCipherSuites[].
 */
static PRUint16	allowedByPolicy;          /* all off by default */
static PRUint16	maybeAllowedByPolicy;     /* all off by default */
static PRUint16	chosenPreference = 0xff;  /* all on  by default */

/* bit values for the above two bit masks */
#define SSL_CB_RC4_128_WITH_MD5              (1 << SSL_CK_RC4_128_WITH_MD5)
#define SSL_CB_RC4_128_EXPORT40_WITH_MD5     (1 << SSL_CK_RC4_128_EXPORT40_WITH_MD5)
#define SSL_CB_RC2_128_CBC_WITH_MD5          (1 << SSL_CK_RC2_128_CBC_WITH_MD5)
#define SSL_CB_RC2_128_CBC_EXPORT40_WITH_MD5 (1 << SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5)
#define SSL_CB_IDEA_128_CBC_WITH_MD5         (1 << SSL_CK_IDEA_128_CBC_WITH_MD5)
#define SSL_CB_DES_64_CBC_WITH_MD5           (1 << SSL_CK_DES_64_CBC_WITH_MD5)
#define SSL_CB_DES_192_EDE3_CBC_WITH_MD5     (1 << SSL_CK_DES_192_EDE3_CBC_WITH_MD5)
#define SSL_CB_IMPLEMENTED \
   (SSL_CB_RC4_128_WITH_MD5              | \
    SSL_CB_RC4_128_EXPORT40_WITH_MD5     | \
    SSL_CB_RC2_128_CBC_WITH_MD5          | \
    SSL_CB_RC2_128_CBC_EXPORT40_WITH_MD5 | \
    SSL_CB_DES_64_CBC_WITH_MD5           | \
    SSL_CB_DES_192_EDE3_CBC_WITH_MD5)


/* Construct a socket's list of cipher specs from the global default values.
 */
static SECStatus
ssl2_ConstructCipherSpecs(sslSocket *ss) 
{
    PRUint8 *	        cs		= NULL;
    unsigned int	allowed;
    unsigned int	count;
    int 		ssl3_count	= 0;
    int 		final_count;
    int 		i;
    SECStatus 		rv;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    count = 0;
    PORT_Assert(ss != 0);
    allowed = !ss->opt.enableSSL2 ? 0 :
    	(ss->allowedByPolicy & ss->chosenPreference & SSL_CB_IMPLEMENTED);
    while (allowed) {
    	if (allowed & 1) 
	    ++count;
	allowed >>= 1;
    }

    /* Call ssl3_config_match_init() once here, 
     * instead of inside ssl3_ConstructV2CipherSpecsHack(),
     * because the latter gets called twice below, 
     * and then again in ssl2_BeginClientHandshake().
     */
    ssl3_config_match_init(ss);

    /* ask SSL3 how many cipher suites it has. */
    rv = ssl3_ConstructV2CipherSpecsHack(ss, NULL, &ssl3_count);
    if (rv < 0) 
	return rv;
    count += ssl3_count;

    /* Allocate memory to hold cipher specs */
    if (count > 0)
	cs = (PRUint8*) PORT_Alloc(count * 3);
    else
	PORT_SetError(SSL_ERROR_SSL_DISABLED);
    if (cs == NULL)
    	return SECFailure;

    if (ss->cipherSpecs != NULL) {
	PORT_Free(ss->cipherSpecs);
    }
    ss->cipherSpecs     = cs;
    ss->sizeCipherSpecs = count * 3;

    /* fill in cipher specs for SSL2 cipher suites */
    allowed = !ss->opt.enableSSL2 ? 0 :
    	(ss->allowedByPolicy & ss->chosenPreference & SSL_CB_IMPLEMENTED);
    for (i = 0; i < ssl2_NUM_SUITES_IMPLEMENTED * 3; i += 3) {
	const PRUint8 * hs = implementedCipherSuites + i;
	int             ok = allowed & (1U << hs[0]);
	if (ok) {
	    cs[0] = hs[0];
	    cs[1] = hs[1];
	    cs[2] = hs[2];
	    cs += 3;
	}
    }

    /* now have SSL3 add its suites onto the end */
    rv = ssl3_ConstructV2CipherSpecsHack(ss, cs, &final_count);
    
    /* adjust for any difference between first pass and second pass */
    ss->sizeCipherSpecs -= (ssl3_count - final_count) * 3;

    return rv;
}

/* This function is called immediately after ssl2_ConstructCipherSpecs()
** at the beginning of a handshake.  It detects cases where a protocol
** (e.g. SSL2 or SSL3) is logically enabled, but all its cipher suites
** for that protocol have been disabled.  If such cases, it clears the 
** enable bit for the protocol.  If no protocols remain enabled, or
** if no cipher suites are found, it sets the error code and returns
** SECFailure, otherwise it returns SECSuccess.
*/
static SECStatus
ssl2_CheckConfigSanity(sslSocket *ss)
{
    unsigned int      allowed;
    int               ssl3CipherCount = 0;
    SECStatus         rv;

    /* count the SSL2 and SSL3 enabled ciphers.
     * if either is zero, clear the socket's enable for that protocol.
     */
    if (!ss->cipherSpecs)
    	goto disabled;

    allowed = ss->allowedByPolicy & ss->chosenPreference;
    if (! allowed)
	ss->opt.enableSSL2 = PR_FALSE; /* not really enabled if no ciphers */

    /* ssl3_config_match_init was called in ssl2_ConstructCipherSpecs(). */
    /* Ask how many ssl3 CipherSuites were enabled. */
    rv = ssl3_ConstructV2CipherSpecsHack(ss, NULL, &ssl3CipherCount);
    if (rv != SECSuccess || ssl3CipherCount <= 0) {
	/* SSL3/TLS not really enabled if no ciphers */
	ss->vrange.min = SSL_LIBRARY_VERSION_NONE;
	ss->vrange.max = SSL_LIBRARY_VERSION_NONE;
    }

    if (!ss->opt.enableSSL2 && SSL3_ALL_VERSIONS_DISABLED(&ss->vrange)) {
	SSL_DBG(("%d: SSL[%d]: Can't handshake! all versions disabled.",
		 SSL_GETPID(), ss->fd));
disabled:
	PORT_SetError(SSL_ERROR_SSL_DISABLED);
	return SECFailure;
    }
    return SECSuccess;
}

/* 
 * Since this is a global (not per-socket) setting, we cannot use the
 * HandshakeLock to protect this.  Probably want a global lock.
 */
SECStatus
ssl2_SetPolicy(PRInt32 which, PRInt32 policy)
{
    PRUint32  bitMask;
    SECStatus rv       = SECSuccess;

    which &= 0x000f;
    bitMask = 1 << which;

    if (!(bitMask & SSL_CB_IMPLEMENTED)) {
    	PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
    	return SECFailure;
    }

    if (policy == SSL_ALLOWED) {
	allowedByPolicy 	|= bitMask;
	maybeAllowedByPolicy 	|= bitMask;
    } else if (policy == SSL_RESTRICTED) {
    	allowedByPolicy 	&= ~bitMask;
	maybeAllowedByPolicy 	|= bitMask;
    } else {
    	allowedByPolicy 	&= ~bitMask;
    	maybeAllowedByPolicy 	&= ~bitMask;
    }
    allowedByPolicy 		&= SSL_CB_IMPLEMENTED;
    maybeAllowedByPolicy 	&= SSL_CB_IMPLEMENTED;

    policyWasSet = PR_TRUE;
    return rv;
}

SECStatus
ssl2_GetPolicy(PRInt32 which, PRInt32 *oPolicy)
{
    PRUint32     bitMask;
    PRInt32      policy;

    which &= 0x000f;
    bitMask = 1 << which;

    /* Caller assures oPolicy is not null. */
    if (!(bitMask & SSL_CB_IMPLEMENTED)) {
    	PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
	*oPolicy = SSL_NOT_ALLOWED;
    	return SECFailure;
    }

    if (maybeAllowedByPolicy & bitMask) {
    	policy = (allowedByPolicy & bitMask) ? SSL_ALLOWED : SSL_RESTRICTED;
    } else {
	policy = SSL_NOT_ALLOWED;
    }

    *oPolicy = policy;
    return SECSuccess;
}

/* 
 * Since this is a global (not per-socket) setting, we cannot use the
 * HandshakeLock to protect this.  Probably want a global lock.
 * Called from SSL_CipherPrefSetDefault in sslsock.c
 * These changes have no effect on any sslSockets already created. 
 */
SECStatus
ssl2_CipherPrefSetDefault(PRInt32 which, PRBool enabled)
{
    PRUint32     bitMask;
    
    which &= 0x000f;
    bitMask = 1 << which;

    if (!(bitMask & SSL_CB_IMPLEMENTED)) {
    	PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
    	return SECFailure;
    }

    if (enabled)
	chosenPreference |= bitMask;
    else
    	chosenPreference &= ~bitMask;
    chosenPreference &= SSL_CB_IMPLEMENTED;

    return SECSuccess;
}

SECStatus 
ssl2_CipherPrefGetDefault(PRInt32 which, PRBool *enabled)
{
    PRBool       rv       = PR_FALSE;
    PRUint32     bitMask;

    which &= 0x000f;
    bitMask = 1 << which;

    if (!(bitMask & SSL_CB_IMPLEMENTED)) {
    	PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
	*enabled = PR_FALSE;
    	return SECFailure;
    }

    rv = (PRBool)((chosenPreference & bitMask) != 0);
    *enabled = rv;
    return SECSuccess;
}

SECStatus 
ssl2_CipherPrefSet(sslSocket *ss, PRInt32 which, PRBool enabled)
{
    PRUint32     bitMask;
    
    which &= 0x000f;
    bitMask = 1 << which;

    if (!(bitMask & SSL_CB_IMPLEMENTED)) {
    	PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
    	return SECFailure;
    }

    if (enabled)
	ss->chosenPreference |= bitMask;
    else
    	ss->chosenPreference &= ~bitMask;
    ss->chosenPreference &= SSL_CB_IMPLEMENTED;

    return SECSuccess;
}

SECStatus 
ssl2_CipherPrefGet(sslSocket *ss, PRInt32 which, PRBool *enabled)
{
    PRBool       rv       = PR_FALSE;
    PRUint32     bitMask;

    which &= 0x000f;
    bitMask = 1 << which;

    if (!(bitMask & SSL_CB_IMPLEMENTED)) {
    	PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
	*enabled = PR_FALSE;
    	return SECFailure;
    }

    rv = (PRBool)((ss->chosenPreference & bitMask) != 0);
    *enabled = rv;
    return SECSuccess;
}


/* copy global default policy into socket. */
void      
ssl2_InitSocketPolicy(sslSocket *ss)
{
    ss->allowedByPolicy		= allowedByPolicy;
    ss->maybeAllowedByPolicy	= maybeAllowedByPolicy;
    ss->chosenPreference 	= chosenPreference;
}


/************************************************************************/

/* Called from ssl2_CreateSessionCypher(), which already holds handshake lock.
 */
static SECStatus
ssl2_CreateMAC(sslSecurityInfo *sec, SECItem *readKey, SECItem *writeKey, 
          int cipherChoice)
{
    switch (cipherChoice) {

      case SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5:
      case SSL_CK_RC2_128_CBC_WITH_MD5:
      case SSL_CK_RC4_128_EXPORT40_WITH_MD5:
      case SSL_CK_RC4_128_WITH_MD5:
      case SSL_CK_DES_64_CBC_WITH_MD5:
      case SSL_CK_DES_192_EDE3_CBC_WITH_MD5:
	sec->hash = HASH_GetHashObject(HASH_AlgMD5);
	SECITEM_CopyItem(0, &sec->sendSecret, writeKey);
	SECITEM_CopyItem(0, &sec->rcvSecret, readKey);
	break;

      default:
	PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
	return SECFailure;
    }
    sec->hashcx = (*sec->hash->create)();
    if (sec->hashcx == NULL)
	return SECFailure;
    return SECSuccess;
}

/************************************************************************
 * All the Send functions below must acquire and release the socket's 
 * xmitBufLock.
 */

/* Called from all the Send* functions below. */
static SECStatus
ssl2_GetSendBuffer(sslSocket *ss, unsigned int len)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    if (len < 128) {
	len = 128;
    }
    if (len > ss->sec.ci.sendBuf.space) {
	rv = sslBuffer_Grow(&ss->sec.ci.sendBuf, len);
	if (rv != SECSuccess) {
	    SSL_DBG(("%d: SSL[%d]: ssl2_GetSendBuffer failed, tried to get %d bytes",
		     SSL_GETPID(), ss->fd, len));
	    rv = SECFailure;
	}
    }
    return rv;
}

/* Called from:
 * ssl2_ClientSetupSessionCypher() <- ssl2_HandleServerHelloMessage()
 * ssl2_HandleRequestCertificate()     <- ssl2_HandleMessage() <- 
 					ssl_Do1stHandshake()
 * ssl2_HandleMessage()                <- ssl_Do1stHandshake()
 * ssl2_HandleServerHelloMessage() <- ssl_Do1stHandshake()
                                     after ssl2_BeginClientHandshake()
 * ssl2_HandleClientHelloMessage() <- ssl_Do1stHandshake() 
                                     after ssl2_BeginServerHandshake()
 * 
 * Acquires and releases the socket's xmitBufLock.
 */	
int
ssl2_SendErrorMessage(sslSocket *ss, int error)
{
    int rv;
    PRUint8 msg[SSL_HL_ERROR_HBYTES];

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    msg[0] = SSL_MT_ERROR;
    msg[1] = MSB(error);
    msg[2] = LSB(error);

    ssl_GetXmitBufLock(ss);    /***************************************/

    SSL_TRC(3, ("%d: SSL[%d]: sending error %d", SSL_GETPID(), ss->fd, error));

    ss->handshakeBegun = 1;
    rv = (*ss->sec.send)(ss, msg, sizeof(msg), 0);
    if (rv >= 0) {
	rv = SECSuccess;
    }
    ssl_ReleaseXmitBufLock(ss);    /***************************************/
    return rv;
}

/* Called from ssl2_TryToFinish().  
 * Acquires and releases the socket's xmitBufLock.
 */
static SECStatus
ssl2_SendClientFinishedMessage(sslSocket *ss)
{
    SECStatus        rv    = SECSuccess;
    int              sent;
    PRUint8    msg[1 + SSL_CONNECTIONID_BYTES];

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetXmitBufLock(ss);    /***************************************/

    if (ss->sec.ci.sentFinished == 0) {
	ss->sec.ci.sentFinished = 1;

	SSL_TRC(3, ("%d: SSL[%d]: sending client-finished",
		    SSL_GETPID(), ss->fd));

	msg[0] = SSL_MT_CLIENT_FINISHED;
	PORT_Memcpy(msg+1, ss->sec.ci.connectionID, 
	            sizeof(ss->sec.ci.connectionID));

	DUMP_MSG(29, (ss, msg, 1 + sizeof(ss->sec.ci.connectionID)));
	sent = (*ss->sec.send)(ss, msg, 1 + sizeof(ss->sec.ci.connectionID), 0);
	rv = (sent >= 0) ? SECSuccess : (SECStatus)sent;
    }
    ssl_ReleaseXmitBufLock(ss);    /***************************************/
    return rv;
}

/* Called from 
 * ssl2_HandleClientSessionKeyMessage() <- ssl2_HandleClientHelloMessage()
 * ssl2_HandleClientHelloMessage()  <- ssl_Do1stHandshake() 
                                      after ssl2_BeginServerHandshake()
 * Acquires and releases the socket's xmitBufLock.
 */
static SECStatus
ssl2_SendServerVerifyMessage(sslSocket *ss)
{
    PRUint8 *        msg;
    int              sendLen;
    int              sent;
    SECStatus        rv;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetXmitBufLock(ss);    /***************************************/

    sendLen = 1 + SSL_CHALLENGE_BYTES;
    rv = ssl2_GetSendBuffer(ss, sendLen);
    if (rv != SECSuccess) {
	goto done;
    }

    msg = ss->sec.ci.sendBuf.buf;
    msg[0] = SSL_MT_SERVER_VERIFY;
    PORT_Memcpy(msg+1, ss->sec.ci.clientChallenge, SSL_CHALLENGE_BYTES);

    DUMP_MSG(29, (ss, msg, sendLen));
    sent = (*ss->sec.send)(ss, msg, sendLen, 0);

    rv = (sent >= 0) ? SECSuccess : (SECStatus)sent;

done:
    ssl_ReleaseXmitBufLock(ss);    /***************************************/
    return rv;
}

/* Called from ssl2_TryToFinish(). 
 * Acquires and releases the socket's xmitBufLock.
 */
static SECStatus
ssl2_SendServerFinishedMessage(sslSocket *ss)
{
    sslSessionID *   sid;
    PRUint8 *        msg;
    int              sendLen, sent;
    SECStatus        rv    = SECSuccess;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetXmitBufLock(ss);    /***************************************/

    if (ss->sec.ci.sentFinished == 0) {
	ss->sec.ci.sentFinished = 1;
	PORT_Assert(ss->sec.ci.sid != 0);
	sid = ss->sec.ci.sid;

	SSL_TRC(3, ("%d: SSL[%d]: sending server-finished",
		    SSL_GETPID(), ss->fd));

	sendLen = 1 + sizeof(sid->u.ssl2.sessionID);
	rv = ssl2_GetSendBuffer(ss, sendLen);
	if (rv != SECSuccess) {
	    goto done;
	}

	msg = ss->sec.ci.sendBuf.buf;
	msg[0] = SSL_MT_SERVER_FINISHED;
	PORT_Memcpy(msg+1, sid->u.ssl2.sessionID,
		    sizeof(sid->u.ssl2.sessionID));

	DUMP_MSG(29, (ss, msg, sendLen));
	sent = (*ss->sec.send)(ss, msg, sendLen, 0);

	if (sent < 0) {
	    /* If send failed, it is now a bogus  session-id */
	    if (ss->sec.uncache)
		(*ss->sec.uncache)(sid);
	    rv = (SECStatus)sent;
	} else if (!ss->opt.noCache) {
	    if (sid->cached == never_cached) {
		(*ss->sec.cache)(sid);
	    }
	    rv = SECSuccess;
	}
	ssl_FreeSID(sid);
	ss->sec.ci.sid = 0;
    }
done:
    ssl_ReleaseXmitBufLock(ss);    /***************************************/
    return rv;
}

/* Called from ssl2_ClientSetupSessionCypher() <- 
 *						ssl2_HandleServerHelloMessage() 
 *                                           after ssl2_BeginClientHandshake()
 * Acquires and releases the socket's xmitBufLock.
 */
static SECStatus
ssl2_SendSessionKeyMessage(sslSocket *ss, int cipher, int keySize,
		      PRUint8 *ca, int caLen,
		      PRUint8 *ck, int ckLen,
		      PRUint8 *ek, int ekLen)
{
    PRUint8 *        msg;
    int              sendLen;
    int              sent;
    SECStatus        rv;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetXmitBufLock(ss);    /***************************************/

    sendLen = SSL_HL_CLIENT_MASTER_KEY_HBYTES + ckLen + ekLen + caLen;
    rv = ssl2_GetSendBuffer(ss, sendLen);
    if (rv != SECSuccess) 
	goto done;

    SSL_TRC(3, ("%d: SSL[%d]: sending client-session-key",
		SSL_GETPID(), ss->fd));

    msg = ss->sec.ci.sendBuf.buf;
    msg[0] = SSL_MT_CLIENT_MASTER_KEY;
    msg[1] = cipher;
    msg[2] = MSB(keySize);
    msg[3] = LSB(keySize);
    msg[4] = MSB(ckLen);
    msg[5] = LSB(ckLen);
    msg[6] = MSB(ekLen);
    msg[7] = LSB(ekLen);
    msg[8] = MSB(caLen);
    msg[9] = LSB(caLen);
    PORT_Memcpy(msg+SSL_HL_CLIENT_MASTER_KEY_HBYTES, ck, ckLen);
    PORT_Memcpy(msg+SSL_HL_CLIENT_MASTER_KEY_HBYTES+ckLen, ek, ekLen);
    PORT_Memcpy(msg+SSL_HL_CLIENT_MASTER_KEY_HBYTES+ckLen+ekLen, ca, caLen);

    DUMP_MSG(29, (ss, msg, sendLen));
    sent = (*ss->sec.send)(ss, msg, sendLen, 0);
    rv = (sent >= 0) ? SECSuccess : (SECStatus)sent;
done:
    ssl_ReleaseXmitBufLock(ss);    /***************************************/
    return rv;
}

/* Called from ssl2_TriggerNextMessage() <- ssl2_HandleMessage() 
 * Acquires and releases the socket's xmitBufLock.
 */
static SECStatus
ssl2_SendCertificateRequestMessage(sslSocket *ss)
{
    PRUint8 *        msg;
    int              sent;
    int              sendLen;
    SECStatus        rv;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetXmitBufLock(ss);    /***************************************/

    sendLen = SSL_HL_REQUEST_CERTIFICATE_HBYTES + SSL_CHALLENGE_BYTES;
    rv = ssl2_GetSendBuffer(ss, sendLen);
    if (rv != SECSuccess) 
	goto done;

    SSL_TRC(3, ("%d: SSL[%d]: sending certificate request",
		SSL_GETPID(), ss->fd));

    /* Generate random challenge for client to encrypt */
    PK11_GenerateRandom(ss->sec.ci.serverChallenge, SSL_CHALLENGE_BYTES);

    msg = ss->sec.ci.sendBuf.buf;
    msg[0] = SSL_MT_REQUEST_CERTIFICATE;
    msg[1] = SSL_AT_MD5_WITH_RSA_ENCRYPTION;
    PORT_Memcpy(msg + SSL_HL_REQUEST_CERTIFICATE_HBYTES, 
                ss->sec.ci.serverChallenge, SSL_CHALLENGE_BYTES);

    DUMP_MSG(29, (ss, msg, sendLen));
    sent = (*ss->sec.send)(ss, msg, sendLen, 0);
    rv = (sent >= 0) ? SECSuccess : (SECStatus)sent;
done:
    ssl_ReleaseXmitBufLock(ss);    /***************************************/
    return rv;
}

/* Called from ssl2_HandleRequestCertificate() <- ssl2_HandleMessage()
 * Acquires and releases the socket's xmitBufLock.
 */
static int
ssl2_SendCertificateResponseMessage(sslSocket *ss, SECItem *cert, 
                                    SECItem *encCode)
{
    PRUint8 *msg;
    int rv, sendLen;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetXmitBufLock(ss);    /***************************************/

    sendLen = SSL_HL_CLIENT_CERTIFICATE_HBYTES + encCode->len + cert->len;
    rv = ssl2_GetSendBuffer(ss, sendLen);
    if (rv) 
    	goto done;

    SSL_TRC(3, ("%d: SSL[%d]: sending certificate response",
		SSL_GETPID(), ss->fd));

    msg = ss->sec.ci.sendBuf.buf;
    msg[0] = SSL_MT_CLIENT_CERTIFICATE;
    msg[1] = SSL_CT_X509_CERTIFICATE;
    msg[2] = MSB(cert->len);
    msg[3] = LSB(cert->len);
    msg[4] = MSB(encCode->len);
    msg[5] = LSB(encCode->len);
    PORT_Memcpy(msg + SSL_HL_CLIENT_CERTIFICATE_HBYTES, cert->data, cert->len);
    PORT_Memcpy(msg + SSL_HL_CLIENT_CERTIFICATE_HBYTES + cert->len,
	      encCode->data, encCode->len);

    DUMP_MSG(29, (ss, msg, sendLen));
    rv = (*ss->sec.send)(ss, msg, sendLen, 0);
    if (rv >= 0) {
	rv = SECSuccess;
    }
done:
    ssl_ReleaseXmitBufLock(ss);    /***************************************/
    return rv;
}

/********************************************************************
**  Send functions above this line must aquire & release the socket's   
**	xmitBufLock.  
** All the ssl2_Send functions below this line are called vis ss->sec.send
**	and require that the caller hold the xmitBufLock.
*/

/*
** Called from ssl2_SendStream, ssl2_SendBlock, but not from ssl2_SendClear.
*/
static SECStatus
ssl2_CalcMAC(PRUint8             * result, 
	     sslSecurityInfo     * sec,
	     const PRUint8       * data, 
	     unsigned int          dataLen,
	     unsigned int          paddingLen)
{
    const PRUint8 *      secret		= sec->sendSecret.data;
    unsigned int         secretLen	= sec->sendSecret.len;
    unsigned long        sequenceNumber = sec->sendSequence;
    unsigned int         nout;
    PRUint8              seq[4];
    PRUint8              padding[32];/* XXX max blocksize? */

    if (!sec->hash || !sec->hash->length)
    	return SECSuccess;
    if (!sec->hashcx)
    	return SECFailure;

    /* Reset hash function */
    (*sec->hash->begin)(sec->hashcx);

    /* Feed hash the data */
    (*sec->hash->update)(sec->hashcx, secret, secretLen);
    (*sec->hash->update)(sec->hashcx, data, dataLen);
    PORT_Memset(padding, paddingLen, paddingLen);
    (*sec->hash->update)(sec->hashcx, padding, paddingLen);

    seq[0] = (PRUint8) (sequenceNumber >> 24);
    seq[1] = (PRUint8) (sequenceNumber >> 16);
    seq[2] = (PRUint8) (sequenceNumber >> 8);
    seq[3] = (PRUint8) (sequenceNumber);

    PRINT_BUF(60, (0, "calc-mac secret:", secret, secretLen));
    PRINT_BUF(60, (0, "calc-mac data:", data, dataLen));
    PRINT_BUF(60, (0, "calc-mac padding:", padding, paddingLen));
    PRINT_BUF(60, (0, "calc-mac seq:", seq, 4));

    (*sec->hash->update)(sec->hashcx, seq, 4);

    /* Get result */
    (*sec->hash->end)(sec->hashcx, result, &nout, sec->hash->length);

    return SECSuccess;
}

/*
** Maximum transmission amounts. These are tiny bit smaller than they
** need to be (they account for the MAC length plus some padding),
** assuming the MAC is 16 bytes long and the padding is a max of 7 bytes
** long. This gives an additional 9 bytes of slop to work within.
*/
#define MAX_STREAM_CYPHER_LEN	0x7fe0
#define MAX_BLOCK_CYPHER_LEN	0x3fe0

/*
** Send some data in the clear. 
** Package up data with the length header and send it.
**
** Return count of bytes successfully written, or negative number (failure).
*/
static PRInt32 
ssl2_SendClear(sslSocket *ss, const PRUint8 *in, PRInt32 len, PRInt32 flags)
{
    PRUint8         * out;
    int               rv;
    int               amount;
    int               count	= 0;

    PORT_Assert( ss->opt.noLocks || ssl_HaveXmitBufLock(ss) );

    SSL_TRC(10, ("%d: SSL[%d]: sending %d bytes in the clear",
		 SSL_GETPID(), ss->fd, len));
    PRINT_BUF(50, (ss, "clear data:", (PRUint8*) in, len));

    while (len) {
	amount = PR_MIN( len, MAX_STREAM_CYPHER_LEN );
	if (amount + 2 > ss->sec.writeBuf.space) {
	    rv = sslBuffer_Grow(&ss->sec.writeBuf, amount + 2);
	    if (rv != SECSuccess) {
		count = rv;
		break;
	    }
	}
	out = ss->sec.writeBuf.buf;

	/*
	** Construct message.
	*/
	out[0] = 0x80 | MSB(amount);
	out[1] = LSB(amount);
	PORT_Memcpy(&out[2], in, amount);

	/* Now send the data */
	rv = ssl_DefSend(ss, out, amount + 2, flags & ~ssl_SEND_FLAG_MASK);
	if (rv < 0) {
	    if (PORT_GetError() == PR_WOULD_BLOCK_ERROR) {
		rv = 0;
	    } else {
		/* Return short write if some data already went out... */
		if (count == 0)
		    count = rv;
		break;
	    }
	}

	if ((unsigned)rv < (amount + 2)) {
	    /* Short write.  Save the data and return. */
	    if (ssl_SaveWriteData(ss, out + rv, amount + 2 - rv) 
	        == SECFailure) {
		count = SECFailure;
	    } else {
		count += amount;
		ss->sec.sendSequence++;
	    }
	    break;
	}

	ss->sec.sendSequence++;
	in    += amount;
	count += amount;
	len   -= amount;
    }

    return count;
}

/*
** Send some data, when using a stream cipher. Stream ciphers have a
** block size of 1. Package up the data with the length header
** and send it.
*/
static PRInt32 
ssl2_SendStream(sslSocket *ss, const PRUint8 *in, PRInt32 len, PRInt32 flags)
{
    PRUint8       *  out;
    int              rv;
    int              count	= 0;

    int              amount;
    PRUint8          macLen;
    int              nout;
    int              buflen;

    PORT_Assert( ss->opt.noLocks || ssl_HaveXmitBufLock(ss) );

    SSL_TRC(10, ("%d: SSL[%d]: sending %d bytes using stream cipher",
		 SSL_GETPID(), ss->fd, len));
    PRINT_BUF(50, (ss, "clear data:", (PRUint8*) in, len));

    while (len) {
	ssl_GetSpecReadLock(ss);  /*************************************/

	macLen = ss->sec.hash->length;
	amount = PR_MIN( len, MAX_STREAM_CYPHER_LEN );
	buflen = amount + 2 + macLen;
	if (buflen > ss->sec.writeBuf.space) {
	    rv = sslBuffer_Grow(&ss->sec.writeBuf, buflen);
	    if (rv != SECSuccess) {
		goto loser;
	    }
	}
	out    = ss->sec.writeBuf.buf;
	nout   = amount + macLen;
	out[0] = 0x80 | MSB(nout);
	out[1] = LSB(nout);

	/* Calculate MAC */
	rv = ssl2_CalcMAC(out+2, 		/* put MAC here */
	                  &ss->sec, 
		          in, amount, 		/* input addr & length */
			  0); 			/* no padding */
	if (rv != SECSuccess) 
	    goto loser;

	/* Encrypt MAC */
	rv = (*ss->sec.enc)(ss->sec.writecx, out+2, &nout, macLen, out+2, macLen);
	if (rv) goto loser;

	/* Encrypt data from caller */
	rv = (*ss->sec.enc)(ss->sec.writecx, out+2+macLen, &nout, amount, in, amount);
	if (rv) goto loser;

	ssl_ReleaseSpecReadLock(ss);  /*************************************/

	PRINT_BUF(50, (ss, "encrypted data:", out, buflen));

	rv = ssl_DefSend(ss, out, buflen, flags & ~ssl_SEND_FLAG_MASK);
	if (rv < 0) {
	    if (PORT_GetError() == PR_WOULD_BLOCK_ERROR) {
		SSL_TRC(50, ("%d: SSL[%d]: send stream would block, "
			     "saving data", SSL_GETPID(), ss->fd));
		rv = 0;
	    } else {
		SSL_TRC(10, ("%d: SSL[%d]: send stream error %d",
			     SSL_GETPID(), ss->fd, PORT_GetError()));
		/* Return short write if some data already went out... */
		if (count == 0)
		    count = rv;
		goto done;
	    }
	}

	if ((unsigned)rv < buflen) {
	    /* Short write.  Save the data and return. */
	    if (ssl_SaveWriteData(ss, out + rv, buflen - rv) == SECFailure) {
		count = SECFailure;
	    } else {
	    	count += amount;
		ss->sec.sendSequence++;
	    }
	    goto done;
	}

	ss->sec.sendSequence++;
	in    += amount;
	count += amount;
	len   -= amount;
    }

done:
    return count;

loser:
    ssl_ReleaseSpecReadLock(ss);
    return SECFailure;
}

/*
** Send some data, when using a block cipher. Package up the data with
** the length header and send it.
*/
/* XXX assumes blocksize is > 7 */
static PRInt32
ssl2_SendBlock(sslSocket *ss, const PRUint8 *in, PRInt32 len, PRInt32 flags)
{
    PRUint8       *  out;  		    /* begining of output buffer.    */
    PRUint8       *  op;		    /* next output byte goes here.   */
    int              rv;		    /* value from funcs we called.   */
    int              count	= 0;        /* this function's return value. */

    unsigned int     hlen;		    /* output record hdr len, 2 or 3 */
    unsigned int     macLen;		    /* MAC is this many bytes long.  */
    int              amount;		    /* of plaintext to go in record. */
    unsigned int     padding;		    /* add this many padding byte.   */
    int              nout;		    /* ciphertext size after header. */
    int              buflen;		    /* size of generated record.     */

    PORT_Assert( ss->opt.noLocks || ssl_HaveXmitBufLock(ss) );

    SSL_TRC(10, ("%d: SSL[%d]: sending %d bytes using block cipher",
		 SSL_GETPID(), ss->fd, len));
    PRINT_BUF(50, (ss, "clear data:", in, len));

    while (len) {
	ssl_GetSpecReadLock(ss);  /*************************************/

	macLen = ss->sec.hash->length;
	/* Figure out how much to send, including mac and padding */
	amount  = PR_MIN( len, MAX_BLOCK_CYPHER_LEN );
	nout    = amount + macLen;
	padding = nout & (ss->sec.blockSize - 1);
	if (padding) {
	    hlen    = 3;
	    padding = ss->sec.blockSize - padding;
	    nout   += padding;
	} else {
	    hlen = 2;
	}
	buflen = hlen + nout;
	if (buflen > ss->sec.writeBuf.space) {
	    rv = sslBuffer_Grow(&ss->sec.writeBuf, buflen);
	    if (rv != SECSuccess) {
		goto loser;
	    }
	}
	out = ss->sec.writeBuf.buf;

	/* Construct header */
	op = out;
	if (padding) {
	    *op++ = MSB(nout);
	    *op++ = LSB(nout);
	    *op++ = padding;
	} else {
	    *op++ = 0x80 | MSB(nout);
	    *op++ = LSB(nout);
	}

	/* Calculate MAC */
	rv = ssl2_CalcMAC(op, 		/* MAC goes here. */
	                  &ss->sec, 
		          in, amount, 	/* intput addr, len */
			  padding);
	if (rv != SECSuccess) 
	    goto loser;
	op += macLen;

	/* Copy in the input data */
	/* XXX could eliminate the copy by folding it into the encryption */
	PORT_Memcpy(op, in, amount);
	op += amount;
	if (padding) {
	    PORT_Memset(op, padding, padding);
	    op += padding;
	}

	/* Encrypt result */
	rv = (*ss->sec.enc)(ss->sec.writecx, out+hlen, &nout, buflen-hlen,
			 out+hlen, op - (out + hlen));
	if (rv) 
	    goto loser;

	ssl_ReleaseSpecReadLock(ss);  /*************************************/

	PRINT_BUF(50, (ss, "final xmit data:", out, op - out));

	rv = ssl_DefSend(ss, out, op - out, flags & ~ssl_SEND_FLAG_MASK);
	if (rv < 0) {
	    if (PORT_GetError() == PR_WOULD_BLOCK_ERROR) {
		rv = 0;
	    } else {
		SSL_TRC(10, ("%d: SSL[%d]: send block error %d",
			     SSL_GETPID(), ss->fd, PORT_GetError()));
		/* Return short write if some data already went out... */
		if (count == 0)
		    count = rv;
		goto done;
	    }
	}

	if (rv < (op - out)) {
	    /* Short write.  Save the data and return. */
	    if (ssl_SaveWriteData(ss, out + rv, op - out - rv) == SECFailure) {
		count = SECFailure;
	    } else {
		count += amount;
		ss->sec.sendSequence++;
	    }
	    goto done;
	}

	ss->sec.sendSequence++;
	in    += amount;
	count += amount;
	len   -= amount;
    }

done:
    return count;

loser:
    ssl_ReleaseSpecReadLock(ss);
    return SECFailure;
}

/*
** Called from: ssl2_HandleServerHelloMessage,
**              ssl2_HandleClientSessionKeyMessage,
**              ssl2_HandleClientHelloMessage,
**              
*/
static void
ssl2_UseEncryptedSendFunc(sslSocket *ss)
{
    ssl_GetXmitBufLock(ss);
    PORT_Assert(ss->sec.hashcx != 0);

    ss->gs.encrypted = 1;
    ss->sec.send = (ss->sec.blockSize > 1) ? ssl2_SendBlock : ssl2_SendStream;
    ssl_ReleaseXmitBufLock(ss);
}

/* Called while initializing socket in ssl_CreateSecurityInfo().
** This function allows us to keep the name of ssl2_SendClear static.
*/
void
ssl2_UseClearSendFunc(sslSocket *ss)
{
    ss->sec.send = ssl2_SendClear;
}

/************************************************************************
** 			END of Send functions.                          * 
*************************************************************************/

/***********************************************************************
 * For SSL3, this gathers in and handles records/messages until either 
 *	the handshake is complete or application data is available.
 *
 * For SSL2, this gathers in only the next SSLV2 record.
 *
 * Called from ssl_Do1stHandshake() via function pointer ss->handshake.
 * Caller must hold handshake lock.
 * This function acquires and releases the RecvBufLock.
 *
 * returns SECSuccess for success.
 * returns SECWouldBlock when that value is returned by ssl2_GatherRecord() or
 *	ssl3_GatherCompleteHandshake().
 * returns SECFailure on all other errors.
 *
 * The gather functions called by ssl_GatherRecord1stHandshake are expected 
 * 	to return values interpreted as follows:
 *  1 : the function completed without error.
 *  0 : the function read EOF.
 * -1 : read error, or PR_WOULD_BLOCK_ERROR, or handleRecord error.
 * -2 : the function wants ssl_GatherRecord1stHandshake to be called again 
 *	immediately, by ssl_Do1stHandshake.
 *
 * This code is similar to, and easily confused with, DoRecv() in sslsecur.c
 *
 * This function is called from ssl_Do1stHandshake().  
 * The following functions put ssl_GatherRecord1stHandshake into ss->handshake:
 *	ssl2_HandleMessage
 *	ssl2_HandleVerifyMessage
 *	ssl2_HandleServerHelloMessage
 *	ssl2_BeginClientHandshake	
 *	ssl2_HandleClientSessionKeyMessage
 *	ssl3_RestartHandshakeAfterCertReq 
 *	ssl3_RestartHandshakeAfterServerCert 
 *	ssl2_HandleClientHelloMessage
 *	ssl2_BeginServerHandshake
 */
SECStatus
ssl_GatherRecord1stHandshake(sslSocket *ss)
{
    int rv;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetRecvBufLock(ss);

    /* The special case DTLS logic is needed here because the SSL/TLS
     * version wants to auto-detect SSL2 vs. SSL3 on the initial handshake
     * (ss->version == 0) but with DTLS it gets confused, so we force the
     * SSL3 version.
     */
    if ((ss->version >= SSL_LIBRARY_VERSION_3_0) || IS_DTLS(ss)) {
	/* Wait for handshake to complete, or application data to arrive.  */
	rv = ssl3_GatherCompleteHandshake(ss, 0);
    } else {
	/* See if we have a complete record */
	rv = ssl2_GatherRecord(ss, 0);
    }
    SSL_TRC(10, ("%d: SSL[%d]: handshake gathering, rv=%d",
		 SSL_GETPID(), ss->fd, rv));

    ssl_ReleaseRecvBufLock(ss);

    if (rv <= 0) {
	if (rv == SECWouldBlock) {
	    /* Progress is blocked waiting for callback completion.  */
	    SSL_TRC(10, ("%d: SSL[%d]: handshake blocked (need %d)",
			 SSL_GETPID(), ss->fd, ss->gs.remainder));
	    return SECWouldBlock;
	}
	if (rv == 0) {
	    /* EOF. Loser  */
	    PORT_SetError(PR_END_OF_FILE_ERROR);
	}
	return SECFailure;	/* rv is < 0 here. */
    }

    SSL_TRC(10, ("%d: SSL[%d]: got handshake record of %d bytes",
		 SSL_GETPID(), ss->fd, ss->gs.recordLen));

    ss->handshake = 0;	/* makes ssl_Do1stHandshake call ss->nextHandshake.*/
    return SECSuccess;
}

/************************************************************************/

/* Called from ssl2_ServerSetupSessionCypher()
 *             ssl2_ClientSetupSessionCypher()
 */
static SECStatus
ssl2_FillInSID(sslSessionID * sid, 
          int            cipher,
	  PRUint8       *keyData, 
	  int            keyLen,
	  PRUint8       *ca, 
	  int            caLen,
	  int            keyBits, 
	  int            secretKeyBits,
	  SSLSignType    authAlgorithm,
	  PRUint32       authKeyBits,
	  SSLKEAType     keaType,
	  PRUint32       keaKeyBits)
{
    PORT_Assert(sid->references == 1);
    PORT_Assert(sid->cached == never_cached);
    PORT_Assert(sid->u.ssl2.masterKey.data == 0);
    PORT_Assert(sid->u.ssl2.cipherArg.data == 0);

    sid->version = SSL_LIBRARY_VERSION_2;

    sid->u.ssl2.cipherType = cipher;
    sid->u.ssl2.masterKey.data = (PRUint8*) PORT_Alloc(keyLen);
    if (!sid->u.ssl2.masterKey.data) {
	return SECFailure;
    }
    PORT_Memcpy(sid->u.ssl2.masterKey.data, keyData, keyLen);
    sid->u.ssl2.masterKey.len = keyLen;
    sid->u.ssl2.keyBits       = keyBits;
    sid->u.ssl2.secretKeyBits = secretKeyBits;
    sid->authAlgorithm        = authAlgorithm;
    sid->authKeyBits          = authKeyBits;
    sid->keaType              = keaType;
    sid->keaKeyBits           = keaKeyBits;
    sid->lastAccessTime = sid->creationTime = ssl_Time();
    sid->expirationTime = sid->creationTime + ssl_sid_timeout;

    if (caLen) {
	sid->u.ssl2.cipherArg.data = (PRUint8*) PORT_Alloc(caLen);
	if (!sid->u.ssl2.cipherArg.data) {
	    return SECFailure;
	}
	sid->u.ssl2.cipherArg.len = caLen;
	PORT_Memcpy(sid->u.ssl2.cipherArg.data, ca, caLen);
    }
    return SECSuccess;
}

/*
** Construct session keys given the masterKey (tied to the session-id),
** the client's challenge and the server's nonce.
**
** Called from ssl2_CreateSessionCypher() <-
*/
static SECStatus
ssl2_ProduceKeys(sslSocket *    ss, 
            SECItem *      readKey, 
	    SECItem *      writeKey,
	    SECItem *      masterKey, 
	    PRUint8 *      challenge,
	    PRUint8 *      nonce, 
	    int            cipherType)
{
    PK11Context * cx        = 0;
    unsigned      nkm       = 0; /* number of hashes to generate key mat. */
    unsigned      nkd       = 0; /* size of readKey and writeKey. */
    unsigned      part;
    unsigned      i;
    unsigned      off;
    SECStatus     rv;
    PRUint8       countChar;
    PRUint8       km[3*16];	/* buffer for key material. */

    readKey->data = 0;
    writeKey->data = 0;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    rv = SECSuccess;
    cx = PK11_CreateDigestContext(SEC_OID_MD5);
    if (cx == NULL) {
	ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
	return SECFailure;
    }

    nkm = ssl_Specs[cipherType].nkm;
    nkd = ssl_Specs[cipherType].nkd;

    readKey->data = (PRUint8*) PORT_Alloc(nkd);
    if (!readKey->data) 
    	goto loser;
    readKey->len = nkd;

    writeKey->data = (PRUint8*) PORT_Alloc(nkd);
    if (!writeKey->data) 
    	goto loser;
    writeKey->len = nkd;

    /* Produce key material */
    countChar = '0';
    for (i = 0, off = 0; i < nkm; i++, off += 16) {
	rv  = PK11_DigestBegin(cx);
	rv |= PK11_DigestOp(cx, masterKey->data, masterKey->len);
	rv |= PK11_DigestOp(cx, &countChar,      1);
	rv |= PK11_DigestOp(cx, challenge,       SSL_CHALLENGE_BYTES);
	rv |= PK11_DigestOp(cx, nonce,           SSL_CONNECTIONID_BYTES);
	rv |= PK11_DigestFinal(cx, km+off, &part, MD5_LENGTH);
	if (rv != SECSuccess) {
	    ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
	    rv = SECFailure;
	    goto loser;
	}
	countChar++;
    }

    /* Produce keys */
    PORT_Memcpy(readKey->data,  km,       nkd);
    PORT_Memcpy(writeKey->data, km + nkd, nkd);

loser:
    PK11_DestroyContext(cx, PR_TRUE);
    return rv;
}

/* Called from ssl2_ServerSetupSessionCypher() 
**                  <- ssl2_HandleClientSessionKeyMessage()
**                          <- ssl2_HandleClientHelloMessage()
** and from    ssl2_ClientSetupSessionCypher() 
**                  <- ssl2_HandleServerHelloMessage()
*/
static SECStatus
ssl2_CreateSessionCypher(sslSocket *ss, sslSessionID *sid, PRBool isClient)
{
    SECItem         * rk = NULL;
    SECItem         * wk = NULL;
    SECItem *         param;
    SECStatus         rv;
    int               cipherType  = sid->u.ssl2.cipherType;
    PK11SlotInfo *    slot        = NULL;
    CK_MECHANISM_TYPE mechanism;
    SECItem           readKey;
    SECItem           writeKey;

    void *readcx = 0;
    void *writecx = 0;
    readKey.data = 0;
    writeKey.data = 0;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );
    if (ss->sec.ci.sid == 0)
    	goto sec_loser;	/* don't crash if asserts are off */

    /* Trying to cut down on all these switch statements that should be tables.
     * So, test cipherType once, here, and then use tables below. 
     */
    switch (cipherType) {
    case SSL_CK_RC4_128_EXPORT40_WITH_MD5:
    case SSL_CK_RC4_128_WITH_MD5:
    case SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5:
    case SSL_CK_RC2_128_CBC_WITH_MD5:
    case SSL_CK_DES_64_CBC_WITH_MD5:
    case SSL_CK_DES_192_EDE3_CBC_WITH_MD5:
	break;

    default:
	SSL_DBG(("%d: SSL[%d]: ssl2_CreateSessionCypher: unknown cipher=%d",
		 SSL_GETPID(), ss->fd, cipherType));
	PORT_SetError(isClient ? SSL_ERROR_BAD_SERVER : SSL_ERROR_BAD_CLIENT);
	goto sec_loser;
    }
 
    rk = isClient ? &readKey  : &writeKey;
    wk = isClient ? &writeKey : &readKey;

    /* Produce the keys for this session */
    rv = ssl2_ProduceKeys(ss, &readKey, &writeKey, &sid->u.ssl2.masterKey,
		     ss->sec.ci.clientChallenge, ss->sec.ci.connectionID,
		     cipherType);
    if (rv != SECSuccess) 
	goto loser;
    PRINT_BUF(7, (ss, "Session read-key: ", rk->data, rk->len));
    PRINT_BUF(7, (ss, "Session write-key: ", wk->data, wk->len));

    PORT_Memcpy(ss->sec.ci.readKey, readKey.data, readKey.len);
    PORT_Memcpy(ss->sec.ci.writeKey, writeKey.data, writeKey.len);
    ss->sec.ci.keySize = readKey.len;

    /* Setup the MAC */
    rv = ssl2_CreateMAC(&ss->sec, rk, wk, cipherType);
    if (rv != SECSuccess) 
    	goto loser;

    /* First create the session key object */
    SSL_TRC(3, ("%d: SSL[%d]: using %s", SSL_GETPID(), ss->fd,
	    ssl_cipherName[cipherType]));


    mechanism  = ssl_Specs[cipherType].mechanism;

    /* set destructer before we call loser... */
    ss->sec.destroy = (void (*)(void*, PRBool)) PK11_DestroyContext;
    slot = PK11_GetBestSlot(mechanism, ss->pkcs11PinArg);
    if (slot == NULL)
	goto loser;

    param = PK11_ParamFromIV(mechanism, &sid->u.ssl2.cipherArg);
    if (param == NULL)
	goto loser;
    readcx = PK11_CreateContextByRawKey(slot, mechanism, PK11_OriginUnwrap,
					CKA_DECRYPT, rk, param,
					ss->pkcs11PinArg);
    SECITEM_FreeItem(param, PR_TRUE);
    if (readcx == NULL)
	goto loser;

    /* build the client context */
    param = PK11_ParamFromIV(mechanism, &sid->u.ssl2.cipherArg);
    if (param == NULL)
	goto loser;
    writecx = PK11_CreateContextByRawKey(slot, mechanism, PK11_OriginUnwrap,
					 CKA_ENCRYPT, wk, param,
					 ss->pkcs11PinArg);
    SECITEM_FreeItem(param,PR_TRUE);
    if (writecx == NULL)
	goto loser;
    PK11_FreeSlot(slot);

    rv = SECSuccess;
    ss->sec.enc           = (SSLCipher) PK11_CipherOp;
    ss->sec.dec           = (SSLCipher) PK11_CipherOp;
    ss->sec.readcx        = (void *) readcx;
    ss->sec.writecx       = (void *) writecx;
    ss->sec.blockSize     = ssl_Specs[cipherType].blockSize;
    ss->sec.blockShift    = ssl_Specs[cipherType].blockShift;
    ss->sec.cipherType    = sid->u.ssl2.cipherType;
    ss->sec.keyBits       = sid->u.ssl2.keyBits;
    ss->sec.secretKeyBits = sid->u.ssl2.secretKeyBits;
    goto done;

  loser:
    if (ss->sec.destroy) {
	if (readcx)  (*ss->sec.destroy)(readcx, PR_TRUE);
	if (writecx) (*ss->sec.destroy)(writecx, PR_TRUE);
    }
    ss->sec.destroy = NULL;
    if (slot) PK11_FreeSlot(slot);

  sec_loser:
    rv = SECFailure;

  done:
    if (rk) {
	SECITEM_ZfreeItem(rk, PR_FALSE);
    }
    if (wk) {
	SECITEM_ZfreeItem(wk, PR_FALSE);
    }
    return rv;
}

/*
** Setup the server ciphers given information from a CLIENT-MASTER-KEY
** message.
** 	"ss"      pointer to the ssl-socket object
** 	"cipher"  the cipher type to use
** 	"keyBits" the size of the final cipher key
** 	"ck"      the clear-key data
** 	"ckLen"   the number of bytes of clear-key data
** 	"ek"      the encrypted-key data
** 	"ekLen"   the number of bytes of encrypted-key data
** 	"ca"      the cipher-arg data
** 	"caLen"   the number of bytes of cipher-arg data
**
** The MASTER-KEY is constructed by first decrypting the encrypted-key
** data. This produces the SECRET-KEY-DATA. The MASTER-KEY is composed by
** concatenating the clear-key data with the SECRET-KEY-DATA. This code
** checks to make sure that the client didn't send us an improper amount
** of SECRET-KEY-DATA (it restricts the length of that data to match the
** spec).
**
** Called from ssl2_HandleClientSessionKeyMessage().
*/
static SECStatus
ssl2_ServerSetupSessionCypher(sslSocket *ss, int cipher, unsigned int keyBits,
			 PRUint8 *ck, unsigned int ckLen,
			 PRUint8 *ek, unsigned int ekLen,
			 PRUint8 *ca, unsigned int caLen)
{
    PRUint8      *    dk   = NULL; /* decrypted master key */
    sslSessionID *    sid;
    sslServerCerts *  sc   = ss->serverCerts + kt_rsa;
    PRUint8       *   kbuf = 0;	/* buffer for RSA decrypted data. */
    unsigned int      ddLen;	/* length of RSA decrypted data in kbuf */
    unsigned int      keySize;
    unsigned int      dkLen;    /* decrypted key length in bytes */
    int               modulusLen;
    SECStatus         rv;
    PRUint16          allowed;  /* cipher kinds enabled and allowed by policy */
    PRUint8           mkbuf[SSL_MAX_MASTER_KEY_BYTES];

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );
    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss)   );
    PORT_Assert((sc->SERVERKEY != 0));
    PORT_Assert((ss->sec.ci.sid != 0));
    sid = ss->sec.ci.sid;

    /* Trying to cut down on all these switch statements that should be tables.
     * So, test cipherType once, here, and then use tables below. 
     */
    switch (cipher) {
    case SSL_CK_RC4_128_EXPORT40_WITH_MD5:
    case SSL_CK_RC4_128_WITH_MD5:
    case SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5:
    case SSL_CK_RC2_128_CBC_WITH_MD5:
    case SSL_CK_DES_64_CBC_WITH_MD5:
    case SSL_CK_DES_192_EDE3_CBC_WITH_MD5:
	break;

    default:
	SSL_DBG(("%d: SSL[%d]: ssl2_ServerSetupSessionCypher: unknown cipher=%d",
		 SSL_GETPID(), ss->fd, cipher));
	PORT_SetError(SSL_ERROR_BAD_CLIENT);
	goto loser;
    }

    allowed = ss->allowedByPolicy & ss->chosenPreference & SSL_CB_IMPLEMENTED;
    if (!(allowed & (1 << cipher))) {
    	/* client chose a kind we don't allow! */
	SSL_DBG(("%d: SSL[%d]: disallowed cipher=%d",
		 SSL_GETPID(), ss->fd, cipher));
	PORT_SetError(SSL_ERROR_BAD_CLIENT);
	goto loser;
    }

    keySize = ssl_Specs[cipher].keyLen;
    if (keyBits != keySize * BPB) {
	SSL_DBG(("%d: SSL[%d]: invalid master secret key length=%d (bits)!",
		 SSL_GETPID(), ss->fd, keyBits));
	PORT_SetError(SSL_ERROR_BAD_CLIENT);
	goto loser;
    }

    if (ckLen != ssl_Specs[cipher].pubLen) {
	SSL_DBG(("%d: SSL[%d]: invalid clear key length, ckLen=%d (bytes)!",
		 SSL_GETPID(), ss->fd, ckLen));
	PORT_SetError(SSL_ERROR_BAD_CLIENT);
	goto loser;
    }

    if (caLen != ssl_Specs[cipher].ivLen) {
	SSL_DBG(("%d: SSL[%d]: invalid key args length, caLen=%d (bytes)!",
		 SSL_GETPID(), ss->fd, caLen));
	PORT_SetError(SSL_ERROR_BAD_CLIENT);
	goto loser;
    }

    modulusLen = PK11_GetPrivateModulusLen(sc->SERVERKEY);
    if (modulusLen == -1) {
	/* XXX If the key is bad, then PK11_PubDecryptRaw will fail below. */
	modulusLen = ekLen;
    }
    if (ekLen > modulusLen || ekLen + ckLen < keySize) {
	SSL_DBG(("%d: SSL[%d]: invalid encrypted key length, ekLen=%d (bytes)!",
		 SSL_GETPID(), ss->fd, ekLen));
	PORT_SetError(SSL_ERROR_BAD_CLIENT);
	goto loser;
    }

    /* allocate the buffer to hold the decrypted portion of the key. */
    kbuf = (PRUint8*)PORT_Alloc(modulusLen);
    if (!kbuf) {
	goto loser;
    }
    dkLen = keySize - ckLen;
    dk    = kbuf + modulusLen - dkLen;

    /* Decrypt encrypted half of the key. 
    ** NOTE: PK11_PubDecryptRaw will barf on a non-RSA key. This is
    ** desired behavior here.
    */
    rv = PK11_PubDecryptRaw(sc->SERVERKEY, kbuf, &ddLen, modulusLen, ek, ekLen);
    if (rv != SECSuccess) 
	goto hide_loser;

    /* Is the length of the decrypted data (ddLen) the expected value? */
    if (modulusLen != ddLen) 
	goto hide_loser;

    /* Cheaply verify that PKCS#1 was used to format the encryption block */
    if ((kbuf[0] != 0x00) || (kbuf[1] != 0x02) || (dk[-1] != 0x00)) {
	SSL_DBG(("%d: SSL[%d]: strange encryption block",
		 SSL_GETPID(), ss->fd));
	PORT_SetError(SSL_ERROR_BAD_CLIENT);
	goto hide_loser;
    }

    /* Make sure we're not subject to a version rollback attack. */
    if (!SSL3_ALL_VERSIONS_DISABLED(&ss->vrange)) {
	static const PRUint8 threes[8] = { 0x03, 0x03, 0x03, 0x03,
			                   0x03, 0x03, 0x03, 0x03 };
	
	if (PORT_Memcmp(dk - 8 - 1, threes, 8) == 0) {
	    PORT_SetError(SSL_ERROR_BAD_CLIENT);
	    goto hide_loser;
	}
    }
    if (0) {
hide_loser:
	/* Defense against the Bleichenbacher attack.
	 * Provide the client with NO CLUES that the decrypted master key
	 * was erroneous.  Don't send any error messages.
	 * Instead, Generate a completely bogus master key .
	 */
	PK11_GenerateRandom(dk, dkLen);
    }

    /*
    ** Construct master key out of the pieces.
    */
    if (ckLen) {
	PORT_Memcpy(mkbuf, ck, ckLen);
    }
    PORT_Memcpy(mkbuf + ckLen, dk, dkLen);

    /* Fill in session-id */
    rv = ssl2_FillInSID(sid, cipher, mkbuf, keySize, ca, caLen,
		   keyBits, keyBits - (ckLen<<3),
		   ss->sec.authAlgorithm, ss->sec.authKeyBits,
		   ss->sec.keaType,       ss->sec.keaKeyBits);
    if (rv != SECSuccess) {
	goto loser;
    }

    /* Create session ciphers */
    rv = ssl2_CreateSessionCypher(ss, sid, PR_FALSE);
    if (rv != SECSuccess) {
	goto loser;
    }

    SSL_TRC(1, ("%d: SSL[%d]: server, using %s cipher, clear=%d total=%d",
		SSL_GETPID(), ss->fd, ssl_cipherName[cipher],
		ckLen<<3, keySize<<3));
    rv = SECSuccess;
    goto done;

  loser:
    rv = SECFailure;

  done:
    PORT_Free(kbuf);
    return rv;
}

/************************************************************************/

/*
** Rewrite the incoming cipher specs, comparing to list of specs we support,
** (ss->cipherSpecs) and eliminating anything we don't support
**
*  Note: Our list may contain SSL v3 ciphers.  
*  We MUST NOT match on any of those.  
*  Fortunately, this is easy to detect because SSLv3 ciphers have zero
*  in the first byte, and none of the SSLv2 ciphers do.
*
*  Called from ssl2_HandleClientHelloMessage().
*  Returns the number of bytes of "qualified cipher specs", 
*  which is typically a multiple of 3, but will be zero if there are none.
*/
static int
ssl2_QualifyCypherSpecs(sslSocket *ss, 
                        PRUint8 *  cs, /* cipher specs in client hello msg. */
		        int        csLen)
{
    PRUint8 *    ms;
    PRUint8 *    hs;
    PRUint8 *    qs;
    int          mc;
    int          hc;
    PRUint8      qualifiedSpecs[ssl2_NUM_SUITES_IMPLEMENTED * 3];

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );
    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss)   );

    if (!ss->cipherSpecs) {
	SECStatus rv = ssl2_ConstructCipherSpecs(ss);
	if (rv != SECSuccess || !ss->cipherSpecs) 
	    return 0;
    }

    PRINT_BUF(10, (ss, "specs from client:", cs, csLen));
    qs = qualifiedSpecs;
    ms = ss->cipherSpecs;
    for (mc = ss->sizeCipherSpecs; mc > 0; mc -= 3, ms += 3) {
	if (ms[0] == 0)
	    continue;
	for (hs = cs, hc = csLen; hc > 0; hs += 3, hc -= 3) {
	    if ((hs[0] == ms[0]) &&
		(hs[1] == ms[1]) &&
		(hs[2] == ms[2])) {
		/* Copy this cipher spec into the "keep" section */
		qs[0] = hs[0];
		qs[1] = hs[1];
		qs[2] = hs[2];
		qs   += 3;
		break;
	    }
	}
    }
    hc = qs - qualifiedSpecs;
    PRINT_BUF(10, (ss, "qualified specs from client:", qualifiedSpecs, hc));
    PORT_Memcpy(cs, qualifiedSpecs, hc);
    return hc;
}

/*
** Pick the best cipher we can find, given the array of server cipher
** specs.  Returns cipher number (e.g. SSL_CK_*), or -1 for no overlap.
** If successful, stores the master key size (bytes) in *pKeyLen.
**
** This is correct only for the client side, but presently
** this function is only called from 
**	ssl2_ClientSetupSessionCypher() <- ssl2_HandleServerHelloMessage()
**
** Note that most servers only return a single cipher suite in their 
** ServerHello messages.  So, the code below for finding the "best" cipher
** suite usually has only one choice.  The client and server should send 
** their cipher suite lists sorted in descending order by preference.
*/
static int
ssl2_ChooseSessionCypher(sslSocket *ss, 
                         int        hc,    /* number of cs's in hs. */
		         PRUint8 *  hs,    /* server hello's cipher suites. */
		         int *      pKeyLen) /* out: sym key size in bytes. */
{
    PRUint8 *       ms;
    unsigned int    i;
    int             bestKeySize;
    int             bestRealKeySize;
    int             bestCypher;
    int             keySize;
    int             realKeySize;
    PRUint8 *       ohs               = hs;
    const PRUint8 * preferred;
    static const PRUint8 noneSuch[3] = { 0, 0, 0 };

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );
    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss)   );

    if (!ss->cipherSpecs) {
	SECStatus rv = ssl2_ConstructCipherSpecs(ss);
	if (rv != SECSuccess || !ss->cipherSpecs) 
	    goto loser;
    }

    if (!ss->preferredCipher) {
    	unsigned int allowed = ss->allowedByPolicy & ss->chosenPreference &
	                       SSL_CB_IMPLEMENTED;
	if (allowed) {
	    preferred = implementedCipherSuites;
	    for (i = ssl2_NUM_SUITES_IMPLEMENTED; i > 0; --i) {
		if (0 != (allowed & (1U << preferred[0]))) {
		    ss->preferredCipher = preferred;
		    break;
		}
		preferred += 3;
	    }
	}
    }
    preferred = ss->preferredCipher ? ss->preferredCipher : noneSuch;
    /*
    ** Scan list of ciphers received from peer and look for a match in
    ** our list.  
    *  Note: Our list may contain SSL v3 ciphers.  
    *  We MUST NOT match on any of those.  
    *  Fortunately, this is easy to detect because SSLv3 ciphers have zero
    *  in the first byte, and none of the SSLv2 ciphers do.
    */
    bestKeySize = bestRealKeySize = 0;
    bestCypher = -1;
    while (--hc >= 0) {
	for (i = 0, ms = ss->cipherSpecs; i < ss->sizeCipherSpecs; i += 3, ms += 3) {
	    if ((hs[0] == preferred[0]) &&
		(hs[1] == preferred[1]) &&
		(hs[2] == preferred[2]) &&
		 hs[0] != 0) {
		/* Pick this cipher immediately! */
		*pKeyLen = (((hs[1] << 8) | hs[2]) + 7) >> 3;
		return hs[0];
	    }
	    if ((hs[0] == ms[0]) && (hs[1] == ms[1]) && (hs[2] == ms[2]) &&
	         hs[0] != 0) {
		/* Found a match */

		/* Use secret keySize to determine which cipher is best */
		realKeySize = (hs[1] << 8) | hs[2];
		switch (hs[0]) {
		  case SSL_CK_RC4_128_EXPORT40_WITH_MD5:
		  case SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5:
		    keySize = 40;
		    break;
		  default:
		    keySize = realKeySize;
		    break;
		}
		if (keySize > bestKeySize) {
		    bestCypher = hs[0];
		    bestKeySize = keySize;
		    bestRealKeySize = realKeySize;
		}
	    }
	}
	hs += 3;
    }
    if (bestCypher < 0) {
	/*
	** No overlap between server and client. Re-examine server list
	** to see what kind of ciphers it does support so that we can set
	** the error code appropriately.
	*/
	if ((ohs[0] == SSL_CK_RC4_128_WITH_MD5) ||
	    (ohs[0] == SSL_CK_RC2_128_CBC_WITH_MD5)) {
	    PORT_SetError(SSL_ERROR_US_ONLY_SERVER);
	} else if ((ohs[0] == SSL_CK_RC4_128_EXPORT40_WITH_MD5) ||
		   (ohs[0] == SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5)) {
	    PORT_SetError(SSL_ERROR_EXPORT_ONLY_SERVER);
	} else {
	    PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
	}
	SSL_DBG(("%d: SSL[%d]: no cipher overlap", SSL_GETPID(), ss->fd));
	goto loser;
    }
    *pKeyLen = (bestRealKeySize + 7) >> 3;
    return bestCypher;

  loser:
    return -1;
}

static SECStatus
ssl2_ClientHandleServerCert(sslSocket *ss, PRUint8 *certData, int certLen)
{
    CERTCertificate *cert      = NULL;
    SECItem          certItem;

    certItem.data = certData;
    certItem.len  = certLen;

    /* decode the certificate */
    cert = CERT_NewTempCertificate(ss->dbHandle, &certItem, NULL,
				   PR_FALSE, PR_TRUE);
    
    if (cert == NULL) {
	SSL_DBG(("%d: SSL[%d]: decode of server certificate fails",
		 SSL_GETPID(), ss->fd));
	PORT_SetError(SSL_ERROR_BAD_CERTIFICATE);
	return SECFailure;
    }

#ifdef TRACE
    {
	if (ssl_trace >= 1) {
	    char *issuer;
	    char *subject;
	    issuer = CERT_NameToAscii(&cert->issuer);
	    subject = CERT_NameToAscii(&cert->subject);
	    SSL_TRC(1,("%d: server certificate issuer: '%s'",
		       SSL_GETPID(), issuer ? issuer : "OOPS"));
	    SSL_TRC(1,("%d: server name: '%s'",
		       SSL_GETPID(), subject ? subject : "OOPS"));
	    PORT_Free(issuer);
	    PORT_Free(subject);
	}
    }
#endif

    ss->sec.peerCert = cert;
    return SECSuccess;
}


/*
 * Format one block of data for public/private key encryption using
 * the rules defined in PKCS #1. SSL2 does this itself to handle the
 * rollback detection.
 */
#define RSA_BLOCK_MIN_PAD_LEN           8
#define RSA_BLOCK_FIRST_OCTET           0x00
#define RSA_BLOCK_AFTER_PAD_OCTET       0x00
#define RSA_BLOCK_PUBLIC_OCTET       	0x02
unsigned char *
ssl_FormatSSL2Block(unsigned modulusLen, SECItem *data)
{
    unsigned char *block;
    unsigned char *bp;
    int padLen;
    SECStatus rv;
    int i;

    if (modulusLen < data->len + (3 + RSA_BLOCK_MIN_PAD_LEN)) {
	PORT_SetError(SEC_ERROR_BAD_KEY);
    	return NULL;
    }
    block = (unsigned char *) PORT_Alloc(modulusLen);
    if (block == NULL)
	return NULL;

    bp = block;

    /*
     * All RSA blocks start with two octets:
     *	0x00 || BlockType
     */
    *bp++ = RSA_BLOCK_FIRST_OCTET;
    *bp++ = RSA_BLOCK_PUBLIC_OCTET;

    /*
     * 0x00 || BT || Pad || 0x00 || ActualData
     *   1      1   padLen    1      data->len
     * Pad is all non-zero random bytes.
     */
    padLen = modulusLen - data->len - 3;
    PORT_Assert (padLen >= RSA_BLOCK_MIN_PAD_LEN);
    rv = PK11_GenerateRandom(bp, padLen);
    if (rv == SECFailure) goto loser;
    /* replace all the 'zero' bytes */
    for (i = 0; i < padLen; i++) {
	while (bp[i] == RSA_BLOCK_AFTER_PAD_OCTET) {
    	    rv = PK11_GenerateRandom(bp+i, 1);
	    if (rv == SECFailure) goto loser;
	}
    }
    bp += padLen;
    *bp++ = RSA_BLOCK_AFTER_PAD_OCTET;
    PORT_Memcpy (bp, data->data, data->len);

    return block;
loser:
    if (block) PORT_Free(block);
    return NULL;
}

/*
** Given the server's public key and cipher specs, generate a session key
** that is ready to use for encrypting/decrypting the byte stream. At
** the same time, generate the SSL_MT_CLIENT_MASTER_KEY message and
** send it to the server.
**
** Called from ssl2_HandleServerHelloMessage()
*/
static SECStatus
ssl2_ClientSetupSessionCypher(sslSocket *ss, PRUint8 *cs, int csLen)
{
    sslSessionID *    sid;
    PRUint8 *         ca;	/* points to iv data, or NULL if none. */
    PRUint8 *         ekbuf 		= 0;
    CERTCertificate * cert 		= 0;
    SECKEYPublicKey * serverKey 	= 0;
    unsigned          modulusLen 	= 0;
    SECStatus         rv;
    int               cipher;
    int               keyLen;	/* cipher symkey size in bytes. */
    int               ckLen;	/* publicly reveal this many bytes of key. */
    int               caLen;	/* length of IV data at *ca.	*/
    int               nc;

    unsigned char *eblock;	/* holds unencrypted PKCS#1 formatted key. */
    SECItem           rek;	/* holds portion of symkey to be encrypted. */

    PRUint8           keyData[SSL_MAX_MASTER_KEY_BYTES];
    PRUint8           iv     [8];

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    eblock = NULL;

    sid = ss->sec.ci.sid;
    PORT_Assert(sid != 0);

    cert = ss->sec.peerCert;
    
    serverKey = CERT_ExtractPublicKey(cert);
    if (!serverKey) {
	SSL_DBG(("%d: SSL[%d]: extract public key failed: error=%d",
		 SSL_GETPID(), ss->fd, PORT_GetError()));
	PORT_SetError(SSL_ERROR_BAD_CERTIFICATE);
	rv = SECFailure;
	goto loser2;
    }

    ss->sec.authAlgorithm = ssl_sign_rsa;
    ss->sec.keaType       = ssl_kea_rsa;
    ss->sec.keaKeyBits    = \
    ss->sec.authKeyBits   = SECKEY_PublicKeyStrengthInBits(serverKey);

    /* Choose a compatible cipher with the server */
    nc = csLen / 3;
    cipher = ssl2_ChooseSessionCypher(ss, nc, cs, &keyLen);
    if (cipher < 0) {
	/* ssl2_ChooseSessionCypher has set error code. */
	ssl2_SendErrorMessage(ss, SSL_PE_NO_CYPHERS);
	goto loser;
    }

    /* Generate the random keys */
    PK11_GenerateRandom(keyData, sizeof(keyData));

    /*
    ** Next, carve up the keys into clear and encrypted portions. The
    ** clear data is taken from the start of keyData and the encrypted
    ** portion from the remainder. Note that each of these portions is
    ** carved in half, one half for the read-key and one for the
    ** write-key.
    */
    ca = 0;

    /* We know that cipher is a legit value here, because 
     * ssl2_ChooseSessionCypher doesn't return bogus values.
     */
    ckLen = ssl_Specs[cipher].pubLen;	/* cleartext key length. */
    caLen = ssl_Specs[cipher].ivLen;	/* IV length.		*/
    if (caLen) {
	PORT_Assert(sizeof iv >= caLen);
    	PK11_GenerateRandom(iv, caLen);
	ca = iv;
    }

    /* Fill in session-id */
    rv = ssl2_FillInSID(sid, cipher, keyData, keyLen,
		   ca, caLen, keyLen << 3, (keyLen - ckLen) << 3,
		   ss->sec.authAlgorithm, ss->sec.authKeyBits,
		   ss->sec.keaType,       ss->sec.keaKeyBits);
    if (rv != SECSuccess) {
	goto loser;
    }

    SSL_TRC(1, ("%d: SSL[%d]: client, using %s cipher, clear=%d total=%d",
		SSL_GETPID(), ss->fd, ssl_cipherName[cipher],
		ckLen<<3, keyLen<<3));

    /* Now setup read and write ciphers */
    rv = ssl2_CreateSessionCypher(ss, sid, PR_TRUE);
    if (rv != SECSuccess) {
	goto loser;
    }

    /*
    ** Fill in the encryption buffer with some random bytes. Then 
    ** copy in the portion of the session key we are encrypting.
    */
    modulusLen = SECKEY_PublicKeyStrength(serverKey);
    rek.data   = keyData + ckLen;
    rek.len    = keyLen  - ckLen;
    eblock = ssl_FormatSSL2Block(modulusLen, &rek);
    if (eblock == NULL) 
    	goto loser;

    /* Set up the padding for version 2 rollback detection. */
    /* XXX We should really use defines here */
    if (!SSL3_ALL_VERSIONS_DISABLED(&ss->vrange)) {
	PORT_Assert((modulusLen - rek.len) > 12);
	PORT_Memset(eblock + modulusLen - rek.len - 8 - 1, 0x03, 8);
    }
    ekbuf = (PRUint8*) PORT_Alloc(modulusLen);
    if (!ekbuf) 
	goto loser;
    PRINT_BUF(10, (ss, "master key encryption block:",
		   eblock, modulusLen));

    /* Encrypt ekitem */
    rv = PK11_PubEncryptRaw(serverKey, ekbuf, eblock, modulusLen,
						ss->pkcs11PinArg);
    if (rv) 
    	goto loser;

    /*  Now we have everything ready to send */
    rv = ssl2_SendSessionKeyMessage(ss, cipher, keyLen << 3, ca, caLen,
			       keyData, ckLen, ekbuf, modulusLen);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = SECSuccess;
    goto done;

  loser:
    rv = SECFailure;

  loser2:
  done:
    PORT_Memset(keyData, 0, sizeof(keyData));
    PORT_ZFree(ekbuf, modulusLen);
    PORT_ZFree(eblock, modulusLen);
    SECKEY_DestroyPublicKey(serverKey);
    return rv;
}

/************************************************************************/

/* 
 * Called from ssl2_HandleMessage in response to SSL_MT_SERVER_FINISHED message.
 * Caller holds recvBufLock and handshakeLock
 */
static void
ssl2_ClientRegSessionID(sslSocket *ss, PRUint8 *s)
{
    sslSessionID *sid = ss->sec.ci.sid;

    /* Record entry in nonce cache */
    if (sid->peerCert == NULL) {
	PORT_Memcpy(sid->u.ssl2.sessionID, s, sizeof(sid->u.ssl2.sessionID));
	sid->peerCert = CERT_DupCertificate(ss->sec.peerCert);

    }
    if (!ss->opt.noCache && sid->cached == never_cached)
	(*ss->sec.cache)(sid);
}

/* Called from ssl2_HandleMessage() */
static SECStatus
ssl2_TriggerNextMessage(sslSocket *ss)
{
    SECStatus        rv;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    if ((ss->sec.ci.requiredElements & CIS_HAVE_CERTIFICATE) &&
	!(ss->sec.ci.sentElements & CIS_HAVE_CERTIFICATE)) {
	ss->sec.ci.sentElements |= CIS_HAVE_CERTIFICATE;
	rv = ssl2_SendCertificateRequestMessage(ss);
	return rv;
    }
    return SECSuccess;
}

/* See if it's time to send our finished message, or if the handshakes are
** complete.  Send finished message if appropriate.
** Returns SECSuccess unless anything goes wrong.
**
** Called from ssl2_HandleMessage,
**             ssl2_HandleVerifyMessage 
**             ssl2_HandleServerHelloMessage
**             ssl2_HandleClientSessionKeyMessage
*/
static SECStatus
ssl2_TryToFinish(sslSocket *ss)
{
    SECStatus        rv;
    char             e, ef;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    e = ss->sec.ci.elements;
    ef = e | CIS_HAVE_FINISHED;
    if ((ef & ss->sec.ci.requiredElements) == ss->sec.ci.requiredElements) {
	if (ss->sec.isServer) {
	    /* Send server finished message if we already didn't */
	    rv = ssl2_SendServerFinishedMessage(ss);
	} else {
	    /* Send client finished message if we already didn't */
	    rv = ssl2_SendClientFinishedMessage(ss);
	}
	if (rv != SECSuccess) {
	    return rv;
	}
	if ((e & ss->sec.ci.requiredElements) == ss->sec.ci.requiredElements) {
	    /* Totally finished */
	    ss->handshake = 0;
	    return SECSuccess;
	}
    }
    return SECSuccess;
}

/*
** Called from ssl2_HandleRequestCertificate
*/
static SECStatus
ssl2_SignResponse(sslSocket *ss,
	     SECKEYPrivateKey *key,
	     SECItem *response)
{
    SGNContext *     sgn = NULL;
    PRUint8 *        challenge;
    unsigned int     len;
    SECStatus        rv		= SECFailure;
    
    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    challenge = ss->sec.ci.serverChallenge;
    len = ss->sec.ci.serverChallengeLen;
    
    /* Sign the expected data... */
    sgn = SGN_NewContext(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,key);
    if (!sgn) 
    	goto done;
    rv = SGN_Begin(sgn);
    if (rv != SECSuccess) 
    	goto done;
    rv = SGN_Update(sgn, ss->sec.ci.readKey, ss->sec.ci.keySize);
    if (rv != SECSuccess) 
    	goto done;
    rv = SGN_Update(sgn, ss->sec.ci.writeKey, ss->sec.ci.keySize);
    if (rv != SECSuccess) 
    	goto done;
    rv = SGN_Update(sgn, challenge, len);
    if (rv != SECSuccess) 
    	goto done;
    rv = SGN_Update(sgn, ss->sec.peerCert->derCert.data, 
                         ss->sec.peerCert->derCert.len);
    if (rv != SECSuccess) 
    	goto done;
    rv = SGN_End(sgn, response);
    if (rv != SECSuccess) 
    	goto done;

done:
    SGN_DestroyContext(sgn, PR_TRUE);
    return rv == SECSuccess ? SECSuccess : SECFailure;
}

/*
** Try to handle a request-certificate message. Get client's certificate
** and private key and sign a message for the server to see.
** Caller must hold handshakeLock 
**
** Called from ssl2_HandleMessage().
*/
static int
ssl2_HandleRequestCertificate(sslSocket *ss)
{
    CERTCertificate * cert	= NULL;	/* app-selected client cert. */
    SECKEYPrivateKey *key	= NULL;	/* priv key for cert. */
    SECStatus         rv;
    SECItem           response;
    int               ret	= 0;
    PRUint8           authType;


    /*
     * These things all need to be initialized before we can "goto loser".
     */
    response.data = NULL;

    /* get challenge info from connectionInfo */
    authType = ss->sec.ci.authType;

    if (authType != SSL_AT_MD5_WITH_RSA_ENCRYPTION) {
	SSL_TRC(7, ("%d: SSL[%d]: unsupported auth type 0x%x", SSL_GETPID(),
		    ss->fd, authType));
	goto no_cert_error;
    }

    /* Get certificate and private-key from client */
    if (!ss->getClientAuthData) {
	SSL_TRC(7, ("%d: SSL[%d]: client doesn't support client-auth",
		    SSL_GETPID(), ss->fd));
	goto no_cert_error;
    }
    ret = (*ss->getClientAuthData)(ss->getClientAuthDataArg, ss->fd,
				   NULL, &cert, &key);
    if ( ret == SECWouldBlock ) {
	PORT_SetError(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SSL2);
	ret = -1;
	goto loser;
    }

    if (ret) {
	goto no_cert_error;
    }

    /* check what the callback function returned */
    if ((!cert) || (!key)) {
        /* we are missing either the key or cert */
        if (cert) {
            /* got a cert, but no key - free it */
            CERT_DestroyCertificate(cert);
            cert = NULL;
        }
        if (key) {
            /* got a key, but no cert - free it */
            SECKEY_DestroyPrivateKey(key);
            key = NULL;
        }
        goto no_cert_error;
    }

    rv = ssl2_SignResponse(ss, key, &response);
    if ( rv != SECSuccess ) {
	ret = -1;
	goto loser;
    }

    /* Send response message */
    ret = ssl2_SendCertificateResponseMessage(ss, &cert->derCert, &response);

    /* Now, remember the cert we sent. But first, forget any previous one. */
    if (ss->sec.localCert) {
	CERT_DestroyCertificate(ss->sec.localCert);
    }
    ss->sec.localCert = CERT_DupCertificate(cert);
    PORT_Assert(!ss->sec.ci.sid->localCert);
    if (ss->sec.ci.sid->localCert) {
	CERT_DestroyCertificate(ss->sec.ci.sid->localCert);
    }
    ss->sec.ci.sid->localCert = cert;
    cert = NULL;

    goto done;

  no_cert_error:
    SSL_TRC(7, ("%d: SSL[%d]: no certificate (ret=%d)", SSL_GETPID(),
		ss->fd, ret));
    ret = ssl2_SendErrorMessage(ss, SSL_PE_NO_CERTIFICATE);

  loser:
  done:
    if ( cert ) {
	CERT_DestroyCertificate(cert);
    }
    if ( key ) {
	SECKEY_DestroyPrivateKey(key);
    }
    if ( response.data ) {
	PORT_Free(response.data);
    }
    
    return ret;
}

/*
** Called from ssl2_HandleMessage for SSL_MT_CLIENT_CERTIFICATE message.
** Caller must hold HandshakeLock and RecvBufLock, since cd and response
** are contained in the gathered input data.
*/
static SECStatus
ssl2_HandleClientCertificate(sslSocket *    ss, 
                             PRUint8        certType,	/* XXX unused */
			     PRUint8 *      cd, 
			     unsigned int   cdLen,
			     PRUint8 *      response,
			     unsigned int   responseLen)
{
    CERTCertificate *cert	= NULL;
    SECKEYPublicKey *pubKey	= NULL;
    VFYContext *     vfy	= NULL;
    SECItem *        derCert;
    SECStatus        rv		= SECFailure;
    SECItem          certItem;
    SECItem          rep;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );
    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss)   );

    /* Extract the certificate */
    certItem.data = cd;
    certItem.len  = cdLen;

    cert = CERT_NewTempCertificate(ss->dbHandle, &certItem, NULL,
			 	   PR_FALSE, PR_TRUE);
    if (cert == NULL) {
	goto loser;
    }

    /* save the certificate, since the auth routine will need it */
    ss->sec.peerCert = cert;

    /* Extract the public key */
    pubKey = CERT_ExtractPublicKey(cert);
    if (!pubKey) 
    	goto loser;
    
    /* Verify the response data... */
    rep.data = response;
    rep.len = responseLen;
    /* SSL 2.0 only supports RSA certs, so we don't have to worry about
     * DSA here. */
    vfy = VFY_CreateContext(pubKey, &rep, SEC_OID_PKCS1_RSA_ENCRYPTION,
			    ss->pkcs11PinArg);
    if (!vfy) 
    	goto loser;
    rv = VFY_Begin(vfy);
    if (rv) 
    	goto loser;

    rv = VFY_Update(vfy, ss->sec.ci.readKey, ss->sec.ci.keySize);
    if (rv) 
    	goto loser;
    rv = VFY_Update(vfy, ss->sec.ci.writeKey, ss->sec.ci.keySize);
    if (rv) 
    	goto loser;
    rv = VFY_Update(vfy, ss->sec.ci.serverChallenge, SSL_CHALLENGE_BYTES);
    if (rv) 
    	goto loser;

    derCert = &ss->serverCerts[kt_rsa].serverCert->derCert;
    rv = VFY_Update(vfy, derCert->data, derCert->len);
    if (rv) 
    	goto loser;
    rv = VFY_End(vfy);
    if (rv) 
    	goto loser;

    /* Now ask the server application if it likes the certificate... */
    rv = (SECStatus) (*ss->authCertificate)(ss->authCertificateArg,
					    ss->fd, PR_TRUE, PR_TRUE);
    /* Hey, it liked it. */
    if (SECSuccess == rv) 
	goto done;

loser:
    ss->sec.peerCert = NULL;
    CERT_DestroyCertificate(cert);

done:
    VFY_DestroyContext(vfy, PR_TRUE);
    SECKEY_DestroyPublicKey(pubKey);
    return rv;
}

/*
** Handle remaining messages between client/server. Process finished
** messages from either side and any authentication requests.
** This should only be called for SSLv2 handshake messages, 
** not for application data records.
** Caller must hold handshake lock.
**
** Called from ssl_Do1stHandshake().
** 
*/
static SECStatus
ssl2_HandleMessage(sslSocket *ss)
{
    PRUint8 *        data;
    PRUint8 *        cid;
    unsigned         len, certType, certLen, responseLen;
    int              rv;
    int              rv2;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ssl_GetRecvBufLock(ss);

    data = ss->gs.buf.buf + ss->gs.recordOffset;

    if (ss->gs.recordLen < 1) {
	goto bad_peer;
    }
    SSL_TRC(3, ("%d: SSL[%d]: received %d message",
		SSL_GETPID(), ss->fd, data[0]));
    DUMP_MSG(29, (ss, data, ss->gs.recordLen));

    switch (data[0]) {
    case SSL_MT_CLIENT_FINISHED:
	if (ss->sec.ci.elements & CIS_HAVE_FINISHED) {
	    SSL_DBG(("%d: SSL[%d]: dup client-finished message",
		     SSL_GETPID(), ss->fd));
	    goto bad_peer;
	}

	/* See if nonce matches */
	len = ss->gs.recordLen - 1;
	cid = data + 1;
	if ((len != sizeof(ss->sec.ci.connectionID)) ||
	    (PORT_Memcmp(ss->sec.ci.connectionID, cid, len) != 0)) {
	    SSL_DBG(("%d: SSL[%d]: bad connection-id", SSL_GETPID(), ss->fd));
	    PRINT_BUF(5, (ss, "sent connection-id",
			  ss->sec.ci.connectionID, 
			  sizeof(ss->sec.ci.connectionID)));
	    PRINT_BUF(5, (ss, "rcvd connection-id", cid, len));
	    goto bad_peer;
	}

	SSL_TRC(5, ("%d: SSL[%d]: got client finished, waiting for 0x%d",
		    SSL_GETPID(), ss->fd, 
		    ss->sec.ci.requiredElements ^ ss->sec.ci.elements));
	ss->sec.ci.elements |= CIS_HAVE_FINISHED;
	break;

    case SSL_MT_SERVER_FINISHED:
	if (ss->sec.ci.elements & CIS_HAVE_FINISHED) {
	    SSL_DBG(("%d: SSL[%d]: dup server-finished message",
		     SSL_GETPID(), ss->fd));
	    goto bad_peer;
	}

	if (ss->gs.recordLen - 1 != SSL2_SESSIONID_BYTES) {
	    SSL_DBG(("%d: SSL[%d]: bad server-finished message, len=%d",
		     SSL_GETPID(), ss->fd, ss->gs.recordLen));
	    goto bad_peer;
	}
	ssl2_ClientRegSessionID(ss, data+1);
	SSL_TRC(5, ("%d: SSL[%d]: got server finished, waiting for 0x%d",
		    SSL_GETPID(), ss->fd, 
		    ss->sec.ci.requiredElements ^ ss->sec.ci.elements));
	ss->sec.ci.elements |= CIS_HAVE_FINISHED;
	break;

    case SSL_MT_REQUEST_CERTIFICATE:
	len = ss->gs.recordLen - 2;
	if ((len < SSL_MIN_CHALLENGE_BYTES) ||
	    (len > SSL_MAX_CHALLENGE_BYTES)) {
	    /* Bad challenge */
	    SSL_DBG(("%d: SSL[%d]: bad cert request message: code len=%d",
		     SSL_GETPID(), ss->fd, len));
	    goto bad_peer;
	}
	
	/* save auth request info */
	ss->sec.ci.authType           = data[1];
	ss->sec.ci.serverChallengeLen = len;
	PORT_Memcpy(ss->sec.ci.serverChallenge, data + 2, len);
	
	rv = ssl2_HandleRequestCertificate(ss);
	if (rv == SECWouldBlock) {
	    SSL_TRC(3, ("%d: SSL[%d]: async cert request",
			SSL_GETPID(), ss->fd));
	    /* someone is handling this asynchronously */
	    ssl_ReleaseRecvBufLock(ss);
	    return SECWouldBlock;
	}
	if (rv) {
	    SET_ERROR_CODE
	    goto loser;
	}
	break;

    case SSL_MT_CLIENT_CERTIFICATE:
	if (!ss->authCertificate) {
	    /* Server asked for authentication and can't handle it */
	    PORT_SetError(SSL_ERROR_BAD_SERVER);
	    goto loser;
	}
	if (ss->gs.recordLen < SSL_HL_CLIENT_CERTIFICATE_HBYTES) {
	    SET_ERROR_CODE
	    goto loser;
	}
	certType    = data[1];
	certLen     = (data[2] << 8) | data[3];
	responseLen = (data[4] << 8) | data[5];
	if (certType != SSL_CT_X509_CERTIFICATE) {
	    PORT_SetError(SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE);
	    goto loser;
	}
	if (certLen + responseLen + SSL_HL_CLIENT_CERTIFICATE_HBYTES 
	    > ss->gs.recordLen) {
	    /* prevent overflow crash. */
	    rv = SECFailure;
	} else
	rv = ssl2_HandleClientCertificate(ss, data[1],
		data + SSL_HL_CLIENT_CERTIFICATE_HBYTES,
		certLen,
		data + SSL_HL_CLIENT_CERTIFICATE_HBYTES + certLen,
		responseLen);
	if (rv) {
	    rv2 = ssl2_SendErrorMessage(ss, SSL_PE_BAD_CERTIFICATE);
	    SET_ERROR_CODE
	    goto loser;
	}
	ss->sec.ci.elements |= CIS_HAVE_CERTIFICATE;
	break;

    case SSL_MT_ERROR:
	rv = (data[1] << 8) | data[2];
	SSL_TRC(2, ("%d: SSL[%d]: got error message, error=0x%x",
		    SSL_GETPID(), ss->fd, rv));

	/* Convert protocol error number into API error number */
	switch (rv) {
	  case SSL_PE_NO_CYPHERS:
	    rv = SSL_ERROR_NO_CYPHER_OVERLAP;
	    break;
	  case SSL_PE_NO_CERTIFICATE:
	    rv = SSL_ERROR_NO_CERTIFICATE;
	    break;
	  case SSL_PE_BAD_CERTIFICATE:
	    rv = SSL_ERROR_BAD_CERTIFICATE;
	    break;
	  case SSL_PE_UNSUPPORTED_CERTIFICATE_TYPE:
	    rv = SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE;
	    break;
	  default:
	    goto bad_peer;
	}
	/* XXX make certificate-request optionally fail... */
	PORT_SetError(rv);
	goto loser;

    default:
	SSL_DBG(("%d: SSL[%d]: unknown message %d",
		 SSL_GETPID(), ss->fd, data[0]));
	goto loser;
    }

    SSL_TRC(3, ("%d: SSL[%d]: handled %d message, required=0x%x got=0x%x",
		SSL_GETPID(), ss->fd, data[0],
		ss->sec.ci.requiredElements, ss->sec.ci.elements));

    rv = ssl2_TryToFinish(ss);
    if (rv != SECSuccess) 
	goto loser;

    ss->gs.recordLen = 0;
    ssl_ReleaseRecvBufLock(ss);

    if (ss->handshake == 0) {
	return SECSuccess;
    }

    ss->handshake     = ssl_GatherRecord1stHandshake;
    ss->nextHandshake = ssl2_HandleMessage;
    return ssl2_TriggerNextMessage(ss);

  bad_peer:
    PORT_SetError(ss->sec.isServer ? SSL_ERROR_BAD_CLIENT : SSL_ERROR_BAD_SERVER);
    /* FALL THROUGH */

  loser:
    ssl_ReleaseRecvBufLock(ss);
    return SECFailure;
}

/************************************************************************/

/* Called from ssl_Do1stHandshake, after ssl2_HandleServerHelloMessage.
*/
static SECStatus
ssl2_HandleVerifyMessage(sslSocket *ss)
{
    PRUint8 *        data;
    SECStatus        rv;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );
    ssl_GetRecvBufLock(ss);

    data = ss->gs.buf.buf + ss->gs.recordOffset;
    DUMP_MSG(29, (ss, data, ss->gs.recordLen));
    if ((ss->gs.recordLen != 1 + SSL_CHALLENGE_BYTES) ||
	(data[0] != SSL_MT_SERVER_VERIFY) ||
	NSS_SecureMemcmp(data+1, ss->sec.ci.clientChallenge,
	                 SSL_CHALLENGE_BYTES)) {
	/* Bad server */
	PORT_SetError(SSL_ERROR_BAD_SERVER);
	goto loser;
    }
    ss->sec.ci.elements |= CIS_HAVE_VERIFY;

    SSL_TRC(5, ("%d: SSL[%d]: got server-verify, required=0x%d got=0x%x",
		SSL_GETPID(), ss->fd, ss->sec.ci.requiredElements,
		ss->sec.ci.elements));

    rv = ssl2_TryToFinish(ss);
    if (rv) 
	goto loser;

    ss->gs.recordLen = 0;
    ssl_ReleaseRecvBufLock(ss);

    if (ss->handshake == 0) {
	return SECSuccess;
    }
    ss->handshake         = ssl_GatherRecord1stHandshake;
    ss->nextHandshake     = ssl2_HandleMessage;
    return SECSuccess;


  loser:
    ssl_ReleaseRecvBufLock(ss);
    return SECFailure;
}

/* Not static because ssl2_GatherData() tests ss->nextHandshake for this value.
 * ICK! 
 * Called from ssl_Do1stHandshake after ssl2_BeginClientHandshake()
 */
SECStatus
ssl2_HandleServerHelloMessage(sslSocket *ss)
{
    sslSessionID *   sid;
    PRUint8 *        cert;
    PRUint8 *        cs;
    PRUint8 *        data;
    SECStatus        rv; 
    int              needed, sidHit, certLen, csLen, cidLen, certType, err;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    if (!ss->opt.enableSSL2) {
	PORT_SetError(SSL_ERROR_SSL2_DISABLED);
	return SECFailure;
    }

    ssl_GetRecvBufLock(ss);

    PORT_Assert(ss->sec.ci.sid != 0);
    sid = ss->sec.ci.sid;

    data = ss->gs.buf.buf + ss->gs.recordOffset;
    DUMP_MSG(29, (ss, data, ss->gs.recordLen));

    /* Make sure first message has some data and is the server hello message */
    if ((ss->gs.recordLen < SSL_HL_SERVER_HELLO_HBYTES)
	|| (data[0] != SSL_MT_SERVER_HELLO)) {
	if ((data[0] == SSL_MT_ERROR) && (ss->gs.recordLen == 3)) {
	    err = (data[1] << 8) | data[2];
	    if (err == SSL_PE_NO_CYPHERS) {
		PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
		goto loser;
	    }
	}
	goto bad_server;
    }

    sidHit      = data[1];
    certType    = data[2];
    ss->version = (data[3] << 8) | data[4];
    certLen     = (data[5] << 8) | data[6];
    csLen       = (data[7] << 8) | data[8];
    cidLen      = (data[9] << 8) | data[10];
    cert        = data + SSL_HL_SERVER_HELLO_HBYTES;
    cs          = cert + certLen;

    SSL_TRC(5,
	    ("%d: SSL[%d]: server-hello, hit=%d vers=%x certLen=%d csLen=%d cidLen=%d",
	     SSL_GETPID(), ss->fd, sidHit, ss->version, certLen,
	     csLen, cidLen));
    if (ss->version != SSL_LIBRARY_VERSION_2) {
        if (ss->version < SSL_LIBRARY_VERSION_2) {
	  SSL_TRC(3, ("%d: SSL[%d]: demoting self (%x) to server version (%x)",
		      SSL_GETPID(), ss->fd, SSL_LIBRARY_VERSION_2,
		      ss->version));
	} else {
	  SSL_TRC(1, ("%d: SSL[%d]: server version is %x (we are %x)",
		    SSL_GETPID(), ss->fd, ss->version, SSL_LIBRARY_VERSION_2));
	  /* server claims to be newer but does not follow protocol */
	  PORT_SetError(SSL_ERROR_UNSUPPORTED_VERSION);
	  goto loser;
	}
    }

    if ((SSL_HL_SERVER_HELLO_HBYTES + certLen + csLen + cidLen 
                                                  > ss->gs.recordLen)
	|| (csLen % 3) != 0   
	/* || cidLen < SSL_CONNECTIONID_BYTES || cidLen > 32  */
	) {
	goto bad_server;
    }

    /* Save connection-id.
    ** This code only saves the first 16 byte of the connectionID.
    ** If the connectionID is shorter than 16 bytes, it is zero-padded.
    */
    if (cidLen < sizeof ss->sec.ci.connectionID)
	memset(ss->sec.ci.connectionID, 0, sizeof ss->sec.ci.connectionID);
    cidLen = PR_MIN(cidLen, sizeof ss->sec.ci.connectionID);
    PORT_Memcpy(ss->sec.ci.connectionID, cs + csLen, cidLen);

    /* See if session-id hit */
    needed = CIS_HAVE_MASTER_KEY | CIS_HAVE_FINISHED | CIS_HAVE_VERIFY;
    if (sidHit) {
	if (certLen || csLen) {
	    /* Uh oh - bogus server */
	    SSL_DBG(("%d: SSL[%d]: client, huh? hit=%d certLen=%d csLen=%d",
		     SSL_GETPID(), ss->fd, sidHit, certLen, csLen));
	    goto bad_server;
	}

	/* Total winner. */
	SSL_TRC(1, ("%d: SSL[%d]: client, using nonce for peer=0x%08x "
		    "port=0x%04x",
		    SSL_GETPID(), ss->fd, ss->sec.ci.peer, ss->sec.ci.port));
	ss->sec.peerCert = CERT_DupCertificate(sid->peerCert);
        ss->sec.authAlgorithm = sid->authAlgorithm;
	ss->sec.authKeyBits   = sid->authKeyBits;
	ss->sec.keaType       = sid->keaType;
	ss->sec.keaKeyBits    = sid->keaKeyBits;
	rv = ssl2_CreateSessionCypher(ss, sid, PR_TRUE);
	if (rv != SECSuccess) {
	    goto loser;
	}
    } else {
	if (certType != SSL_CT_X509_CERTIFICATE) {
	    PORT_SetError(SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE);
	    goto loser;
	}
	if (csLen == 0) {
	    PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
	    SSL_DBG(("%d: SSL[%d]: no cipher overlap",
		     SSL_GETPID(), ss->fd));
	    goto loser;
	}
	if (certLen == 0) {
	    SSL_DBG(("%d: SSL[%d]: client, huh? certLen=%d csLen=%d",
		     SSL_GETPID(), ss->fd, certLen, csLen));
	    goto bad_server;
	}

	if (sid->cached != never_cached) {
	    /* Forget our session-id - server didn't like it */
	    SSL_TRC(7, ("%d: SSL[%d]: server forgot me, uncaching session-id",
			SSL_GETPID(), ss->fd));
	    if (ss->sec.uncache)
		(*ss->sec.uncache)(sid);
	    ssl_FreeSID(sid);
	    ss->sec.ci.sid = sid = PORT_ZNew(sslSessionID);
	    if (!sid) {
		goto loser;
	    }
	    sid->references = 1;
	    sid->addr = ss->sec.ci.peer;
	    sid->port = ss->sec.ci.port;
	}

	/* decode the server's certificate */
	rv = ssl2_ClientHandleServerCert(ss, cert, certLen);
	if (rv != SECSuccess) {
	    if (PORT_GetError() == SSL_ERROR_BAD_CERTIFICATE) {
		(void) ssl2_SendErrorMessage(ss, SSL_PE_BAD_CERTIFICATE);
	    }
	    goto loser;
	}

	/* Setup new session cipher */
	rv = ssl2_ClientSetupSessionCypher(ss, cs, csLen);
	if (rv != SECSuccess) {
	    if (PORT_GetError() == SSL_ERROR_BAD_CERTIFICATE) {
		(void) ssl2_SendErrorMessage(ss, SSL_PE_BAD_CERTIFICATE);
	    }
	    goto loser;
	}
    }

    /* Build up final list of required elements */
    ss->sec.ci.elements         = CIS_HAVE_MASTER_KEY;
    ss->sec.ci.requiredElements = needed;

  if (!sidHit) {
    /* verify the server's certificate. if sidHit, don't check signatures */
    rv = (* ss->authCertificate)(ss->authCertificateArg, ss->fd, 
				 (PRBool)(!sidHit), PR_FALSE);
    if (rv) {
	if (ss->handleBadCert) {
	    rv = (*ss->handleBadCert)(ss->badCertArg, ss->fd);
	    if ( rv ) {
		if ( rv == SECWouldBlock ) {
		    SSL_DBG(("%d: SSL[%d]: SSL2 bad cert handler returned "
			     "SECWouldBlock", SSL_GETPID(), ss->fd));
		    PORT_SetError(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SSL2);
		    rv = SECFailure;
		} else {
		    /* cert is bad */
		    SSL_DBG(("%d: SSL[%d]: server certificate is no good: error=%d",
			     SSL_GETPID(), ss->fd, PORT_GetError()));
		}
		goto loser;
	    }
	    /* cert is good */
	} else {
	    SSL_DBG(("%d: SSL[%d]: server certificate is no good: error=%d",
		     SSL_GETPID(), ss->fd, PORT_GetError()));
	    goto loser;
	}
    }
  }
    /*
    ** At this point we have a completed session key and our session
    ** cipher is setup and ready to go. Switch to encrypted write routine
    ** as all future message data is to be encrypted.
    */
    ssl2_UseEncryptedSendFunc(ss);

    rv = ssl2_TryToFinish(ss);
    if (rv != SECSuccess) 
	goto loser;

    ss->gs.recordLen = 0;

    ssl_ReleaseRecvBufLock(ss);

    if (ss->handshake == 0) {
	return SECSuccess;
    }

    SSL_TRC(5, ("%d: SSL[%d]: got server-hello, required=0x%d got=0x%x",
		SSL_GETPID(), ss->fd, ss->sec.ci.requiredElements, 
		ss->sec.ci.elements));
    ss->handshake     = ssl_GatherRecord1stHandshake;
    ss->nextHandshake = ssl2_HandleVerifyMessage;
    return SECSuccess;

  bad_server:
    PORT_SetError(SSL_ERROR_BAD_SERVER);
    /* FALL THROUGH */

  loser:
    ssl_ReleaseRecvBufLock(ss);
    return SECFailure;
}

/* Sends out the initial client Hello message on the connection.
 * Acquires and releases the socket's xmitBufLock.
 */
SECStatus
ssl2_BeginClientHandshake(sslSocket *ss)
{
    sslSessionID      *sid;
    PRUint8           *msg;
    PRUint8           *cp;
    PRUint8           *localCipherSpecs = NULL;
    unsigned int      localCipherSize;
    unsigned int      i;
    int               sendLen, sidLen = 0;
    SECStatus         rv;
    TLSExtensionData  *xtnData;

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    ss->sec.isServer     = 0;
    ss->sec.sendSequence = 0;
    ss->sec.rcvSequence  = 0;
    ssl_ChooseSessionIDProcs(&ss->sec);

    if (!ss->cipherSpecs) {
	rv = ssl2_ConstructCipherSpecs(ss);
	if (rv != SECSuccess)
	    goto loser;
    }

    /* count the SSL2 and SSL3 enabled ciphers.
     * if either is zero, clear the socket's enable for that protocol.
     */
    rv = ssl2_CheckConfigSanity(ss);
    if (rv != SECSuccess)
	goto loser;

    /* Get peer name of server */
    rv = ssl_GetPeerInfo(ss);
    if (rv < 0) {
#ifdef HPUX11
        /*
         * On some HP-UX B.11.00 systems, getpeername() occasionally
         * fails with ENOTCONN after a successful completion of
         * non-blocking connect.  I found that if we do a write()
         * and then retry getpeername(), it will work.
         */
        if (PR_GetError() == PR_NOT_CONNECTED_ERROR) {
            char dummy;
            (void) PR_Write(ss->fd->lower, &dummy, 0);
            rv = ssl_GetPeerInfo(ss);
            if (rv < 0) {
                goto loser;
            }
        }
#else
	goto loser;
#endif
    }

    SSL_TRC(3, ("%d: SSL[%d]: sending client-hello", SSL_GETPID(), ss->fd));

    /* Try to find server in our session-id cache */
    if (ss->opt.noCache) {
	sid = NULL;
    } else {
	sid = ssl_LookupSID(&ss->sec.ci.peer, ss->sec.ci.port, ss->peerID, 
	                    ss->url);
    }
    while (sid) {  /* this isn't really a loop */
	PRBool sidVersionEnabled =
	    (!SSL3_ALL_VERSIONS_DISABLED(&ss->vrange) &&
	     sid->version >= ss->vrange.min &&
	     sid->version <= ss->vrange.max) ||
	    (sid->version < SSL_LIBRARY_VERSION_3_0 && ss->opt.enableSSL2);

	/* if we're not doing this SID's protocol any more, drop it. */
	if (!sidVersionEnabled) {
	    if (ss->sec.uncache)
		ss->sec.uncache(sid);
	    ssl_FreeSID(sid);
	    sid = NULL;
	    break;
	}
	if (sid->version < SSL_LIBRARY_VERSION_3_0) {
	    /* If the cipher in this sid is not enabled, drop it. */
	    for (i = 0; i < ss->sizeCipherSpecs; i += 3) {
		if (ss->cipherSpecs[i] == sid->u.ssl2.cipherType)
		    break;
	    }
	    if (i >= ss->sizeCipherSpecs) {
		if (ss->sec.uncache)
		    ss->sec.uncache(sid);
		ssl_FreeSID(sid);
		sid = NULL;
		break;
	    }
	}
	sidLen = sizeof(sid->u.ssl2.sessionID);
	PRINT_BUF(4, (ss, "client, found session-id:", sid->u.ssl2.sessionID,
		      sidLen));
	ss->version = sid->version;
	PORT_Assert(!ss->sec.localCert);
	if (ss->sec.localCert) {
	    CERT_DestroyCertificate(ss->sec.localCert);
	}
	ss->sec.localCert     = CERT_DupCertificate(sid->localCert);
	break;  /* this isn't really a loop */
    } 
    if (!sid) {
	sidLen = 0;
	sid = PORT_ZNew(sslSessionID);
	if (!sid) {
	    goto loser;
	}
	sid->references = 1;
	sid->cached     = never_cached;
	sid->addr       = ss->sec.ci.peer;
	sid->port       = ss->sec.ci.port;
	if (ss->peerID != NULL) {
	    sid->peerID = PORT_Strdup(ss->peerID);
	}
	if (ss->url != NULL) {
	    sid->urlSvrName = PORT_Strdup(ss->url);
	}
    }
    ss->sec.ci.sid = sid;

    PORT_Assert(sid != NULL);

    if ((sid->version >= SSL_LIBRARY_VERSION_3_0 || !ss->opt.v2CompatibleHello) &&
	!SSL3_ALL_VERSIONS_DISABLED(&ss->vrange)) {
	ss->gs.state      = GS_INIT;
	ss->handshake     = ssl_GatherRecord1stHandshake;

	/* ssl3_SendClientHello will override this if it succeeds. */
	ss->version       = SSL_LIBRARY_VERSION_3_0;

	ssl_GetSSL3HandshakeLock(ss);
	ssl_GetXmitBufLock(ss);
	rv =  ssl3_SendClientHello(ss, PR_FALSE);
	ssl_ReleaseXmitBufLock(ss);
	ssl_ReleaseSSL3HandshakeLock(ss);

	return rv;
    }
#ifndef NSS_DISABLE_ECC
    /* ensure we don't neogtiate ECC cipher suites with SSL2 hello */
    ssl3_DisableECCSuites(ss, NULL); /* disable all ECC suites */
    if (ss->cipherSpecs != NULL) {
	PORT_Free(ss->cipherSpecs);
	ss->cipherSpecs     = NULL;
	ss->sizeCipherSpecs = 0;
    }
#endif /* NSS_DISABLE_ECC */

    if (!ss->cipherSpecs) {
        rv = ssl2_ConstructCipherSpecs(ss);
	if (rv < 0) {
	    return rv;
    	}
    }
    localCipherSpecs = ss->cipherSpecs;
    localCipherSize  = ss->sizeCipherSpecs;

    /* Add 3 for SCSV */
    sendLen = SSL_HL_CLIENT_HELLO_HBYTES + localCipherSize + 3 + sidLen +
	SSL_CHALLENGE_BYTES;

    /* Generate challenge bytes for server */
    PK11_GenerateRandom(ss->sec.ci.clientChallenge, SSL_CHALLENGE_BYTES);

    ssl_GetXmitBufLock(ss);    /***************************************/

    rv = ssl2_GetSendBuffer(ss, sendLen);
    if (rv) 
    	goto unlock_loser;

    /* Construct client-hello message */
    cp = msg = ss->sec.ci.sendBuf.buf;
    msg[0] = SSL_MT_CLIENT_HELLO;
    ss->clientHelloVersion = SSL3_ALL_VERSIONS_DISABLED(&ss->vrange) ?
	SSL_LIBRARY_VERSION_2 : ss->vrange.max;

    msg[1] = MSB(ss->clientHelloVersion);
    msg[2] = LSB(ss->clientHelloVersion);
    /* Add 3 for SCSV */
    msg[3] = MSB(localCipherSize + 3);
    msg[4] = LSB(localCipherSize + 3);
    msg[5] = MSB(sidLen);
    msg[6] = LSB(sidLen);
    msg[7] = MSB(SSL_CHALLENGE_BYTES);
    msg[8] = LSB(SSL_CHALLENGE_BYTES);
    cp += SSL_HL_CLIENT_HELLO_HBYTES;
    PORT_Memcpy(cp, localCipherSpecs, localCipherSize);
    cp += localCipherSize;
    /*
     * Add SCSV.  SSL 2.0 cipher suites are listed before SSL 3.0 cipher
     * suites in localCipherSpecs for compatibility with SSL 2.0 servers.
     * Since SCSV looks like an SSL 3.0 cipher suite, we can't add it at
     * the beginning.
     */
    cp[0] = 0x00;
    cp[1] = 0x00;
    cp[2] = 0xff;
    cp += 3;
    if (sidLen) {
	PORT_Memcpy(cp, sid->u.ssl2.sessionID, sidLen);
	cp += sidLen;
    }
    PORT_Memcpy(cp, ss->sec.ci.clientChallenge, SSL_CHALLENGE_BYTES);

    /* Send it to the server */
    DUMP_MSG(29, (ss, msg, sendLen));
    ss->handshakeBegun = 1;
    rv = (*ss->sec.send)(ss, msg, sendLen, 0);

    ssl_ReleaseXmitBufLock(ss);    /***************************************/

    if (rv < 0) {
	goto loser;
    }

    rv = ssl3_StartHandshakeHash(ss, msg, sendLen);
    if (rv < 0) {
	goto loser;
    }

    /*
     * Since we sent the SCSV, pretend we sent empty RI extension.  We need
     * to record the extension has been advertised after ssl3_InitState has
     * been called, which ssl3_StartHandshakeHash took care for us above.
     */
    xtnData = &ss->xtnData;
    xtnData->advertised[xtnData->numAdvertised++] = ssl_renegotiation_info_xtn;

    /* Setup to receive servers hello message */
    ssl_GetRecvBufLock(ss);
    ss->gs.recordLen = 0;
    ssl_ReleaseRecvBufLock(ss);

    ss->handshake     = ssl_GatherRecord1stHandshake;
    ss->nextHandshake = ssl2_HandleServerHelloMessage;
    return SECSuccess;

unlock_loser:
    ssl_ReleaseXmitBufLock(ss);
loser:
    return SECFailure;
}

/************************************************************************/

/* Handle the CLIENT-MASTER-KEY message. 
** Acquires and releases RecvBufLock.
** Called from ssl2_HandleClientHelloMessage(). 
*/
static SECStatus
ssl2_HandleClientSessionKeyMessage(sslSocket *ss)
{
    PRUint8 *        data;
    unsigned int     caLen;
    unsigned int     ckLen;
    unsigned int     ekLen;
    unsigned int     keyBits;
    int              cipher;
    SECStatus        rv;


    ssl_GetRecvBufLock(ss);

    data = ss->gs.buf.buf + ss->gs.recordOffset;
    DUMP_MSG(29, (ss, data, ss->gs.recordLen));

    if ((ss->gs.recordLen < SSL_HL_CLIENT_MASTER_KEY_HBYTES)
	|| (data[0] != SSL_MT_CLIENT_MASTER_KEY)) {
	goto bad_client;
    }
    cipher  = data[1];
    keyBits = (data[2] << 8) | data[3];
    ckLen   = (data[4] << 8) | data[5];
    ekLen   = (data[6] << 8) | data[7];
    caLen   = (data[8] << 8) | data[9];

    SSL_TRC(5, ("%d: SSL[%d]: session-key, cipher=%d keyBits=%d ckLen=%d ekLen=%d caLen=%d",
		SSL_GETPID(), ss->fd, cipher, keyBits, ckLen, ekLen, caLen));

    if (ss->gs.recordLen < 
    	    SSL_HL_CLIENT_MASTER_KEY_HBYTES + ckLen + ekLen + caLen) {
	SSL_DBG(("%d: SSL[%d]: protocol size mismatch dataLen=%d",
		 SSL_GETPID(), ss->fd, ss->gs.recordLen));
	goto bad_client;
    }

    /* Use info from client to setup session key */
    rv = ssl2_ServerSetupSessionCypher(ss, cipher, keyBits,
		data + SSL_HL_CLIENT_MASTER_KEY_HBYTES,                 ckLen,
		data + SSL_HL_CLIENT_MASTER_KEY_HBYTES + ckLen,         ekLen,
		data + SSL_HL_CLIENT_MASTER_KEY_HBYTES + ckLen + ekLen, caLen);
    ss->gs.recordLen = 0;	/* we're done with this record. */

    ssl_ReleaseRecvBufLock(ss);

    if (rv != SECSuccess) {
	goto loser;
    }
    ss->sec.ci.elements |= CIS_HAVE_MASTER_KEY;
    ssl2_UseEncryptedSendFunc(ss);

    /* Send server verify message now that keys are established */
    rv = ssl2_SendServerVerifyMessage(ss);
    if (rv != SECSuccess) 
	goto loser;

    rv = ssl2_TryToFinish(ss);
    if (rv != SECSuccess) 
	goto loser;
    if (ss->handshake == 0) {
	return SECSuccess;
    }

    SSL_TRC(5, ("%d: SSL[%d]: server: waiting for elements=0x%d",
		SSL_GETPID(), ss->fd, 
		ss->sec.ci.requiredElements ^ ss->sec.ci.elements));
    ss->handshake         = ssl_GatherRecord1stHandshake;
    ss->nextHandshake     = ssl2_HandleMessage;

    return ssl2_TriggerNextMessage(ss);

bad_client:
    ssl_ReleaseRecvBufLock(ss);
    PORT_SetError(SSL_ERROR_BAD_CLIENT);
    /* FALLTHROUGH */

loser:
    return SECFailure;
}

/*
** Handle the initial hello message from the client
**
** not static because ssl2_GatherData() tests ss->nextHandshake for this value.
*/
SECStatus
ssl2_HandleClientHelloMessage(sslSocket *ss)
{
    sslSessionID    *sid;
    sslServerCerts * sc;
    CERTCertificate *serverCert;
    PRUint8         *msg;
    PRUint8         *data;
    PRUint8         *cs;
    PRUint8         *sd;
    PRUint8         *cert = NULL;
    PRUint8         *challenge;
    unsigned int    challengeLen;
    SECStatus       rv; 
    int             csLen;
    int             sendLen;
    int             sdLen;
    int             certLen;
    int             pid;
    int             sent;
    int             gotXmitBufLock = 0;
#if defined(SOLARIS) && defined(i386)
    volatile PRUint8 hit;
#else
    int             hit;
#endif
    PRUint8         csImpl[sizeof implementedCipherSuites];

    PORT_Assert( ss->opt.noLocks || ssl_Have1stHandshakeLock(ss) );

    sc = ss->serverCerts + kt_rsa;
    serverCert = sc->serverCert;

    ssl_GetRecvBufLock(ss);


    data = ss->gs.buf.buf + ss->gs.recordOffset;
    DUMP_MSG(29, (ss, data, ss->gs.recordLen));

    /* Make sure first message has some data and is the client hello message */
    if ((ss->gs.recordLen < SSL_HL_CLIENT_HELLO_HBYTES)
	|| (data[0] != SSL_MT_CLIENT_HELLO)) {
	goto bad_client;
    }

    /* Get peer name of client */
    rv = ssl_GetPeerInfo(ss);
    if (rv != SECSuccess) {
	goto loser;
    }

    /* Examine version information */
    /*
     * See if this might be a V2 client hello asking to use the V3 protocol
     */
    if ((data[0] == SSL_MT_CLIENT_HELLO) && 
        (data[1] >= MSB(SSL_LIBRARY_VERSION_3_0)) && 
	!SSL3_ALL_VERSIONS_DISABLED(&ss->vrange)) {
	rv = ssl3_HandleV2ClientHello(ss, data, ss->gs.recordLen);
	if (rv != SECFailure) { /* Success */
	    ss->handshake             = NULL;
	    ss->nextHandshake         = ssl_GatherRecord1stHandshake;
	    ss->securityHandshake     = NULL;
	    ss->gs.state              = GS_INIT;

	    /* ssl3_HandleV3ClientHello has set ss->version,
	    ** and has gotten us a brand new sid.  
	    */
	    ss->sec.ci.sid->version  = ss->version;
	}
	ssl_ReleaseRecvBufLock(ss);
	return rv;
    }
    /* Previously, there was a test here to see if SSL2 was enabled.
    ** If not, an error code was set, and SECFailure was returned,
    ** without sending any error code to the other end of the connection.
    ** That test has been removed.  If SSL2 has been disabled, there
    ** should be no SSL2 ciphers enabled, and consequently, the code
    ** below should send the ssl2 error message SSL_PE_NO_CYPHERS.
    ** We now believe this is the correct thing to do, even when SSL2
    ** has been explicitly disabled by the application.
    */

    /* Extract info from message */
    ss->version = (data[1] << 8) | data[2];

    /* If some client thinks ssl v2 is 2.0 instead of 0.2, we'll allow it.  */
    if (ss->version >= SSL_LIBRARY_VERSION_3_0) {
	ss->version = SSL_LIBRARY_VERSION_2;
    }
    
    csLen        = (data[3] << 8) | data[4];
    sdLen        = (data[5] << 8) | data[6];
    challengeLen = (data[7] << 8) | data[8];
    cs           = data + SSL_HL_CLIENT_HELLO_HBYTES;
    sd           = cs + csLen;
    challenge    = sd + sdLen;
    PRINT_BUF(7, (ss, "server, client session-id value:", sd, sdLen));

    if (!csLen || (csLen % 3) != 0 || 
        (sdLen != 0 && sdLen != SSL2_SESSIONID_BYTES) ||
	challengeLen < SSL_MIN_CHALLENGE_BYTES || 
	challengeLen > SSL_MAX_CHALLENGE_BYTES ||
        (unsigned)ss->gs.recordLen != 
            SSL_HL_CLIENT_HELLO_HBYTES + csLen + sdLen + challengeLen) {
	SSL_DBG(("%d: SSL[%d]: bad client hello message, len=%d should=%d",
		 SSL_GETPID(), ss->fd, ss->gs.recordLen,
		 SSL_HL_CLIENT_HELLO_HBYTES+csLen+sdLen+challengeLen));
	goto bad_client;
    }

    SSL_TRC(3, ("%d: SSL[%d]: client version is %x",
		SSL_GETPID(), ss->fd, ss->version));
    if (ss->version != SSL_LIBRARY_VERSION_2) {
	if (ss->version > SSL_LIBRARY_VERSION_2) {
	    /*
	    ** Newer client than us. Things are ok because new clients
	    ** are required to be backwards compatible with old servers.
	    ** Change version number to our version number so that client
	    ** knows whats up.
	    */
	    ss->version = SSL_LIBRARY_VERSION_2;
	} else {
	    SSL_TRC(1, ("%d: SSL[%d]: client version is %x (we are %x)",
		SSL_GETPID(), ss->fd, ss->version, SSL_LIBRARY_VERSION_2));
	    PORT_SetError(SSL_ERROR_UNSUPPORTED_VERSION);
	    goto loser;
	}
    }

    /* Qualify cipher specs before returning them to client */
    csLen = ssl2_QualifyCypherSpecs(ss, cs, csLen);
    if (csLen == 0) {
	/* no overlap, send client our list of supported SSL v2 ciphers. */
        cs    = csImpl;
	csLen = sizeof implementedCipherSuites;
    	PORT_Memcpy(cs, implementedCipherSuites, csLen);
	csLen = ssl2_QualifyCypherSpecs(ss, cs, csLen);
	if (csLen == 0) {
	  /* We don't support any SSL v2 ciphers! */
	  ssl2_SendErrorMessage(ss, SSL_PE_NO_CYPHERS);
	  PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
	  goto loser;
	}
	/* Since this handhsake is going to fail, don't cache it. */
	ss->opt.noCache = 1; 
    }

    /* Squirrel away the challenge for later */
    PORT_Memcpy(ss->sec.ci.clientChallenge, challenge, challengeLen);

    /* Examine message and see if session-id is good */
    ss->sec.ci.elements = 0;
    if (sdLen > 0 && !ss->opt.noCache) {
	SSL_TRC(7, ("%d: SSL[%d]: server, lookup client session-id for 0x%08x%08x%08x%08x",
		    SSL_GETPID(), ss->fd, ss->sec.ci.peer.pr_s6_addr32[0],
		    ss->sec.ci.peer.pr_s6_addr32[1], 
		    ss->sec.ci.peer.pr_s6_addr32[2],
		    ss->sec.ci.peer.pr_s6_addr32[3]));
	sid = (*ssl_sid_lookup)(&ss->sec.ci.peer, sd, sdLen, ss->dbHandle);
    } else {
	sid = NULL;
    }
    if (sid) {
	/* Got a good session-id. Short cut! */
	SSL_TRC(1, ("%d: SSL[%d]: server, using session-id for 0x%08x (age=%d)",
		    SSL_GETPID(), ss->fd, ss->sec.ci.peer, 
		    ssl_Time() - sid->creationTime));
	PRINT_BUF(1, (ss, "session-id value:", sd, sdLen));
	ss->sec.ci.sid = sid;
	ss->sec.ci.elements = CIS_HAVE_MASTER_KEY;
	hit = 1;
	certLen = 0;
	csLen = 0;

        ss->sec.authAlgorithm = sid->authAlgorithm;
	ss->sec.authKeyBits   = sid->authKeyBits;
	ss->sec.keaType       = sid->keaType;
	ss->sec.keaKeyBits    = sid->keaKeyBits;

	rv = ssl2_CreateSessionCypher(ss, sid, PR_FALSE);
	if (rv != SECSuccess) {
	    goto loser;
	}
    } else {
	SECItem * derCert   = &serverCert->derCert;

	SSL_TRC(7, ("%d: SSL[%d]: server, lookup nonce missed",
		    SSL_GETPID(), ss->fd));
	if (!serverCert) {
	    SET_ERROR_CODE
	    goto loser;
	}
	hit = 0;
	sid = PORT_ZNew(sslSessionID);
	if (!sid) {
	    goto loser;
	}
	sid->references = 1;
	sid->addr = ss->sec.ci.peer;
	sid->port = ss->sec.ci.port;

	/* Invent a session-id */
	ss->sec.ci.sid = sid;
	PK11_GenerateRandom(sid->u.ssl2.sessionID+2, SSL2_SESSIONID_BYTES-2);

	pid = SSL_GETPID();
	sid->u.ssl2.sessionID[0] = MSB(pid);
	sid->u.ssl2.sessionID[1] = LSB(pid);
	cert    = derCert->data;
	certLen = derCert->len;

	/* pretend that server sids remember the local cert. */
	PORT_Assert(!sid->localCert);
	if (sid->localCert) {
	    CERT_DestroyCertificate(sid->localCert);
	}
	sid->localCert     = CERT_DupCertificate(serverCert);

	ss->sec.authAlgorithm = ssl_sign_rsa;
	ss->sec.keaType       = ssl_kea_rsa;
	ss->sec.keaKeyBits    = \
	ss->sec.authKeyBits   = ss->serverCerts[kt_rsa].serverKeyBits;
    }

    /* server sids don't remember the local cert, so whether we found
    ** a sid or not, just "remember" we used the rsa server cert.
    */
    if (ss->sec.localCert) {
	CERT_DestroyCertificate(ss->sec.localCert);
    }
    ss->sec.localCert     = CERT_DupCertificate(serverCert);

    /* Build up final list of required elements */
    ss->sec.ci.requiredElements = CIS_HAVE_MASTER_KEY | CIS_HAVE_FINISHED;
    if (ss->opt.requestCertificate) {
	ss->sec.ci.requiredElements |= CIS_HAVE_CERTIFICATE;
    }
    ss->sec.ci.sentElements = 0;

    /* Send hello message back to client */
    sendLen = SSL_HL_SERVER_HELLO_HBYTES + certLen + csLen
	    + SSL_CONNECTIONID_BYTES;

    ssl_GetXmitBufLock(ss); gotXmitBufLock = 1;
    rv = ssl2_GetSendBuffer(ss, sendLen);
    if (rv != SECSuccess) {
	goto loser;
    }

    SSL_TRC(3, ("%d: SSL[%d]: sending server-hello (%d)",
		SSL_GETPID(), ss->fd, sendLen));

    msg = ss->sec.ci.sendBuf.buf;
    msg[0] = SSL_MT_SERVER_HELLO;
    msg[1] = hit;
    msg[2] = SSL_CT_X509_CERTIFICATE;
    msg[3] = MSB(ss->version);
    msg[4] = LSB(ss->version);
    msg[5] = MSB(certLen);
    msg[6] = LSB(certLen);
    msg[7] = MSB(csLen);
    msg[8] = LSB(csLen);
    msg[9] = MSB(SSL_CONNECTIONID_BYTES);
    msg[10] = LSB(SSL_CONNECTIONID_BYTES);
    if (certLen) {
	PORT_Memcpy(msg+SSL_HL_SERVER_HELLO_HBYTES, cert, certLen);
    }
    if (csLen) {
	PORT_Memcpy(msg+SSL_HL_SERVER_HELLO_HBYTES+certLen, cs, csLen);
    }
    PORT_Memcpy(msg+SSL_HL_SERVER_HELLO_HBYTES+certLen+csLen, 
                ss->sec.ci.connectionID, SSL_CONNECTIONID_BYTES);

    DUMP_MSG(29, (ss, msg, sendLen));

    ss->handshakeBegun = 1;
    sent = (*ss->sec.send)(ss, msg, sendLen, 0);
    if (sent < 0) {
	goto loser;
    }
    ssl_ReleaseXmitBufLock(ss); gotXmitBufLock = 0;

    ss->gs.recordLen = 0;
    ss->handshake = ssl_GatherRecord1stHandshake;
    if (hit) {
	/* Old SID Session key is good. Go encrypted */
	ssl2_UseEncryptedSendFunc(ss);

	/* Send server verify message now that keys are established */
	rv = ssl2_SendServerVerifyMessage(ss);
	if (rv != SECSuccess) 
	    goto loser;

	ss->nextHandshake = ssl2_HandleMessage;
	ssl_ReleaseRecvBufLock(ss);
	rv = ssl2_TriggerNextMessage(ss);
	return rv;
    }
    ss->nextHandshake = ssl2_HandleClientSessionKeyMessage;
    ssl_ReleaseRecvBufLock(ss);
    return SECSuccess;

  bad_client:
    PORT_SetError(SSL_ERROR_BAD_CLIENT);
    /* FALLTHROUGH */

  loser:
    if (gotXmitBufLock) {
    	ssl_ReleaseXmitBufLock(ss); gotXmitBufLock = 0;
    }
    SSL_TRC(10, ("%d: SSL[%d]: server, wait for client-hello lossage",
		 SSL_GETPID(), ss->fd));
    ssl_ReleaseRecvBufLock(ss);
    return SECFailure;
}

SECStatus
ssl2_BeginServerHandshake(sslSocket *ss)
{
    SECStatus        rv;
    sslServerCerts * rsaAuth = ss->serverCerts + kt_rsa;

    ss->sec.isServer = 1;
    ssl_ChooseSessionIDProcs(&ss->sec);
    ss->sec.sendSequence = 0;
    ss->sec.rcvSequence = 0;

    /* don't turn on SSL2 if we don't have an RSA key and cert */
    if (!rsaAuth->serverKeyPair || !rsaAuth->SERVERKEY || 
        !rsaAuth->serverCert) {
	ss->opt.enableSSL2 = PR_FALSE;
    }

    if (!ss->cipherSpecs) {
	rv = ssl2_ConstructCipherSpecs(ss);
	if (rv != SECSuccess)
	    goto loser;
    }

    /* count the SSL2 and SSL3 enabled ciphers.
     * if either is zero, clear the socket's enable for that protocol.
     */
    rv = ssl2_CheckConfigSanity(ss);
    if (rv != SECSuccess)
	goto loser;

    /*
    ** Generate connection-id. Always do this, even if things fail
    ** immediately. This way the random number generator is always
    ** rolling around, every time we get a connection.
    */
    PK11_GenerateRandom(ss->sec.ci.connectionID, 
                        sizeof(ss->sec.ci.connectionID));

    ss->gs.recordLen = 0;
    ss->handshake     = ssl_GatherRecord1stHandshake;
    ss->nextHandshake = ssl2_HandleClientHelloMessage;
    return SECSuccess;

loser:
    return SECFailure;
}

/* This function doesn't really belong in this file.
** It's here to keep AIX compilers from optimizing it away, 
** and not including it in the DSO.
*/

#include "nss.h"
extern const char __nss_ssl_rcsid[];
extern const char __nss_ssl_sccsid[];

PRBool
NSSSSL_VersionCheck(const char *importedVersion)
{
    /*
     * This is the secret handshake algorithm.
     *
     * This release has a simple version compatibility
     * check algorithm.  This release is not backward
     * compatible with previous major releases.  It is
     * not compatible with future major, minor, or
     * patch releases.
     */
    volatile char c; /* force a reference that won't get optimized away */

    c = __nss_ssl_rcsid[0] + __nss_ssl_sccsid[0]; 
    return NSS_VersionCheck(importedVersion);
}

const char *
NSSSSL_GetVersion(void)
{
    return NSS_VERSION;
}
