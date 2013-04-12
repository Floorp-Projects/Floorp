/*
 * vtables (and methods that call through them) for the 4 types of 
 * SSLSockets supported.  Only one type is still supported.
 * Various other functions.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */
#include "seccomon.h"
#include "cert.h"
#include "keyhi.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "nspr.h"
#include "private/pprio.h"
#ifndef NO_PKCS11_BYPASS
#include "blapi.h"
#endif
#include "nss.h"

#define SET_ERROR_CODE   /* reminder */

struct cipherPolicyStr {
	int		cipher;
	unsigned char 	export;	/* policy value for export policy */
	unsigned char 	france;	/* policy value for france policy */
};

typedef struct cipherPolicyStr cipherPolicy;

/* This table contains two preconfigured policies: Export and France.
** It is used only by the functions SSL_SetDomesticPolicy, 
** SSL_SetExportPolicy, and SSL_SetFrancyPolicy.
** Order of entries is not important.
*/
static cipherPolicy ssl_ciphers[] = {	   /*   Export           France   */
 {  SSL_EN_RC4_128_WITH_MD5,		    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_EN_RC4_128_EXPORT40_WITH_MD5,	    SSL_ALLOWED,     SSL_ALLOWED },
 {  SSL_EN_RC2_128_CBC_WITH_MD5,	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5,   SSL_ALLOWED,     SSL_ALLOWED },
 {  SSL_EN_DES_64_CBC_WITH_MD5,		    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_EN_DES_192_EDE3_CBC_WITH_MD5,	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_RSA_WITH_RC4_128_MD5,		    SSL_RESTRICTED,  SSL_NOT_ALLOWED },
 {  SSL_RSA_WITH_RC4_128_SHA,		    SSL_RESTRICTED,  SSL_NOT_ALLOWED },
 {  SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_RSA_WITH_3DES_EDE_CBC_SHA,	    SSL_RESTRICTED,  SSL_NOT_ALLOWED },
 {  SSL_RSA_FIPS_WITH_DES_CBC_SHA,	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_RSA_WITH_DES_CBC_SHA,		    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_RSA_EXPORT_WITH_RC4_40_MD5,	    SSL_ALLOWED,     SSL_ALLOWED },
 {  SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,	    SSL_ALLOWED,     SSL_ALLOWED },
 {  SSL_DHE_RSA_WITH_DES_CBC_SHA,           SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_DHE_DSS_WITH_DES_CBC_SHA,           SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,      SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,      SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_DSS_WITH_RC4_128_SHA,           SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  SSL_RSA_WITH_NULL_SHA,		    SSL_ALLOWED,     SSL_ALLOWED },
 {  SSL_RSA_WITH_NULL_MD5,		    SSL_ALLOWED,     SSL_ALLOWED },
 {  TLS_DHE_DSS_WITH_AES_128_CBC_SHA, 	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_RSA_WITH_AES_128_CBC_SHA,       SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_RSA_WITH_AES_128_CBC_SHA,     	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_DSS_WITH_AES_256_CBC_SHA, 	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_RSA_WITH_AES_256_CBC_SHA,       SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_RSA_WITH_AES_256_CBC_SHA,     	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,  SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,  SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_RSA_WITH_CAMELLIA_128_CBC_SHA, 	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,  SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,  SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_RSA_WITH_CAMELLIA_256_CBC_SHA, 	    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_RSA_WITH_SEED_CBC_SHA,		    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA,    SSL_ALLOWED,     SSL_NOT_ALLOWED },
 {  TLS_RSA_EXPORT1024_WITH_RC4_56_SHA,     SSL_ALLOWED,     SSL_NOT_ALLOWED },
#ifdef NSS_ENABLE_ECC
 {  TLS_ECDH_ECDSA_WITH_NULL_SHA,           SSL_ALLOWED,     SSL_ALLOWED },
 {  TLS_ECDH_ECDSA_WITH_RC4_128_SHA,        SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,   SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_ECDSA_WITH_NULL_SHA,          SSL_ALLOWED,     SSL_ALLOWED },
 {  TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,       SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,  SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,   SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,   SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDH_RSA_WITH_NULL_SHA,             SSL_ALLOWED,     SSL_ALLOWED },
 {  TLS_ECDH_RSA_WITH_RC4_128_SHA,          SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,     SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,      SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,      SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_RSA_WITH_NULL_SHA,            SSL_ALLOWED,     SSL_ALLOWED },
 {  TLS_ECDHE_RSA_WITH_RC4_128_SHA,         SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,     SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
 {  TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,     SSL_NOT_ALLOWED, SSL_NOT_ALLOWED },
#endif /* NSS_ENABLE_ECC */
 {  0,					    SSL_NOT_ALLOWED, SSL_NOT_ALLOWED }
};

static const sslSocketOps ssl_default_ops = {	/* No SSL. */
    ssl_DefConnect,
    NULL,
    ssl_DefBind,
    ssl_DefListen,
    ssl_DefShutdown,
    ssl_DefClose,
    ssl_DefRecv,
    ssl_DefSend,
    ssl_DefRead,
    ssl_DefWrite,
    ssl_DefGetpeername,
    ssl_DefGetsockname
};

static const sslSocketOps ssl_secure_ops = {	/* SSL. */
    ssl_SecureConnect,
    NULL,
    ssl_DefBind,
    ssl_DefListen,
    ssl_SecureShutdown,
    ssl_SecureClose,
    ssl_SecureRecv,
    ssl_SecureSend,
    ssl_SecureRead,
    ssl_SecureWrite,
    ssl_DefGetpeername,
    ssl_DefGetsockname
};

/*
** default settings for socket enables
*/
static sslOptions ssl_defaults = {
    { siBuffer, NULL, 0 }, /* nextProtoNego */
    PR_TRUE, 	/* useSecurity        */
    PR_FALSE,	/* useSocks           */
    PR_FALSE,	/* requestCertificate */
    2,	        /* requireCertificate */
    PR_FALSE,	/* handshakeAsClient  */
    PR_FALSE,	/* handshakeAsServer  */
    PR_FALSE,	/* enableSSL2         */ /* now defaults to off in NSS 3.13 */
    PR_FALSE,	/* unusedBit9         */
    PR_FALSE, 	/* unusedBit10        */
    PR_FALSE,	/* noCache            */
    PR_FALSE,	/* fdx                */
    PR_FALSE,	/* v2CompatibleHello  */ /* now defaults to off in NSS 3.13 */
    PR_TRUE,	/* detectRollBack     */
    PR_FALSE,   /* noStepDown         */
    PR_FALSE,   /* bypassPKCS11       */
    PR_FALSE,   /* noLocks            */
    PR_FALSE,   /* enableSessionTickets */
    PR_FALSE,   /* enableDeflate      */
    2,          /* enableRenegotiation (default: requires extension) */
    PR_FALSE,   /* requireSafeNegotiation */
    PR_FALSE,   /* enableFalseStart   */
    PR_TRUE,    /* cbcRandomIV        */
    PR_FALSE    /* enableOCSPStapling */
};

/*
 * default range of enabled SSL/TLS protocols
 */
static SSLVersionRange versions_defaults_stream = {
    SSL_LIBRARY_VERSION_3_0,
    SSL_LIBRARY_VERSION_TLS_1_0
};

static SSLVersionRange versions_defaults_datagram = {
    SSL_LIBRARY_VERSION_TLS_1_1,
    SSL_LIBRARY_VERSION_TLS_1_1
};

#define VERSIONS_DEFAULTS(variant) \
    (variant == ssl_variant_stream ? &versions_defaults_stream : \
                                     &versions_defaults_datagram)

sslSessionIDLookupFunc  ssl_sid_lookup;
sslSessionIDCacheFunc   ssl_sid_cache;
sslSessionIDUncacheFunc ssl_sid_uncache;

static PRBool ssl_inited = PR_FALSE;
static PRDescIdentity ssl_layer_id;

PRBool                  locksEverDisabled; 	/* implicitly PR_FALSE */
PRBool			ssl_force_locks;  	/* implicitly PR_FALSE */
int                     ssl_lock_readers	= 1;	/* default true. */
char                    ssl_debug;
char                    ssl_trace;
FILE *                  ssl_trace_iob;
FILE *                  ssl_keylog_iob;
char lockStatus[] = "Locks are ENABLED.  ";
#define LOCKSTATUS_OFFSET 10 /* offset of ENABLED */

/* SRTP_NULL_HMAC_SHA1_80 and SRTP_NULL_HMAC_SHA1_32 are not implemented. */
static const PRUint16 srtpCiphers[] = {
    SRTP_AES128_CM_HMAC_SHA1_80,
    SRTP_AES128_CM_HMAC_SHA1_32,
    0
};

/* forward declarations. */
static sslSocket *ssl_NewSocket(PRBool makeLocks, SSLProtocolVariant variant);
static SECStatus  ssl_MakeLocks(sslSocket *ss);
static void       ssl_SetDefaultsFromEnvironment(void);
static PRStatus   ssl_PushIOLayer(sslSocket *ns, PRFileDesc *stack, 
                                  PRDescIdentity id);

/************************************************************************/

/*
** Lookup a socket structure from a file descriptor.
** Only functions called through the PRIOMethods table should use this.
** Other app-callable functions should use ssl_FindSocket.
*/
static sslSocket *
ssl_GetPrivate(PRFileDesc *fd)
{
    sslSocket *ss;

    PORT_Assert(fd != NULL);
    PORT_Assert(fd->methods->file_type == PR_DESC_LAYERED);
    PORT_Assert(fd->identity == ssl_layer_id);

    if (fd->methods->file_type != PR_DESC_LAYERED ||
        fd->identity != ssl_layer_id) {
	PORT_SetError(PR_BAD_DESCRIPTOR_ERROR);
	return NULL;
    }

    ss = (sslSocket *)fd->secret;
    ss->fd = fd;
    return ss;
}

/* This function tries to find the SSL layer in the stack. 
 * It searches for the first SSL layer at or below the argument fd,
 * and failing that, it searches for the nearest SSL layer above the 
 * argument fd.  It returns the private sslSocket from the found layer.
 */
sslSocket *
ssl_FindSocket(PRFileDesc *fd)
{
    PRFileDesc *layer;
    sslSocket *ss;

    PORT_Assert(fd != NULL);
    PORT_Assert(ssl_layer_id != 0);

    layer = PR_GetIdentitiesLayer(fd, ssl_layer_id);
    if (layer == NULL) {
	PORT_SetError(PR_BAD_DESCRIPTOR_ERROR);
	return NULL;
    }

    ss = (sslSocket *)layer->secret;
    ss->fd = layer;
    return ss;
}

static sslSocket *
ssl_DupSocket(sslSocket *os)
{
    sslSocket *ss;
    SECStatus rv;

    ss = ssl_NewSocket((PRBool)(!os->opt.noLocks), os->protocolVariant);
    if (ss) {
	ss->opt                = os->opt;
	ss->opt.useSocks       = PR_FALSE;
	ss->vrange             = os->vrange;

	ss->peerID             = !os->peerID ? NULL : PORT_Strdup(os->peerID);
	ss->url                = !os->url    ? NULL : PORT_Strdup(os->url);

	ss->ops      = os->ops;
	ss->rTimeout = os->rTimeout;
	ss->wTimeout = os->wTimeout;
	ss->cTimeout = os->cTimeout;
	ss->dbHandle = os->dbHandle;

	/* copy ssl2&3 policy & prefs, even if it's not selected (yet) */
	ss->allowedByPolicy	= os->allowedByPolicy;
	ss->maybeAllowedByPolicy= os->maybeAllowedByPolicy;
	ss->chosenPreference 	= os->chosenPreference;
	PORT_Memcpy(ss->cipherSuites, os->cipherSuites, sizeof os->cipherSuites);
	PORT_Memcpy(ss->ssl3.dtlsSRTPCiphers, os->ssl3.dtlsSRTPCiphers,
		    sizeof(PRUint16) * os->ssl3.dtlsSRTPCipherCount);
	ss->ssl3.dtlsSRTPCipherCount = os->ssl3.dtlsSRTPCipherCount;

	if (os->cipherSpecs) {
	    ss->cipherSpecs  = (unsigned char*)PORT_Alloc(os->sizeCipherSpecs);
	    if (ss->cipherSpecs) 
	    	PORT_Memcpy(ss->cipherSpecs, os->cipherSpecs, 
		            os->sizeCipherSpecs);
	    ss->sizeCipherSpecs    = os->sizeCipherSpecs;
	    ss->preferredCipher    = os->preferredCipher;
	} else {
	    ss->cipherSpecs        = NULL;  /* produced lazily */
	    ss->sizeCipherSpecs    = 0;
	    ss->preferredCipher    = NULL;
	}
	if (ss->opt.useSecurity) {
	    /* This int should be SSLKEAType, but CC on Irix complains,
	     * during the for loop.
	     */
	    int i;
	    sslServerCerts * oc = os->serverCerts;
	    sslServerCerts * sc = ss->serverCerts;
            
	    for (i=kt_null; i < kt_kea_size; i++, oc++, sc++) {
		if (oc->serverCert && oc->serverCertChain) {
		    sc->serverCert      = CERT_DupCertificate(oc->serverCert);
		    sc->serverCertChain = CERT_DupCertList(oc->serverCertChain);
		    if (!sc->serverCertChain) 
		    	goto loser;
		} else {
		    sc->serverCert      = NULL;
		    sc->serverCertChain = NULL;
		}
		sc->serverKeyPair = oc->serverKeyPair ?
				ssl3_GetKeyPairRef(oc->serverKeyPair) : NULL;
		if (oc->serverKeyPair && !sc->serverKeyPair)
		    goto loser;
	        sc->serverKeyBits = oc->serverKeyBits;
	    }
	    ss->stepDownKeyPair = !os->stepDownKeyPair ? NULL :
		                  ssl3_GetKeyPairRef(os->stepDownKeyPair);
	    ss->ephemeralECDHKeyPair = !os->ephemeralECDHKeyPair ? NULL :
		                  ssl3_GetKeyPairRef(os->ephemeralECDHKeyPair);
	    ss->certStatusArray = !os->certStatusArray ? NULL :
				  SECITEM_DupArray(NULL, os->certStatusArray);
/*
 * XXX the preceding CERT_ and SECKEY_ functions can fail and return NULL.
 * XXX We should detect this, and not just march on with NULL pointers.
 */
	    ss->authCertificate       = os->authCertificate;
	    ss->authCertificateArg    = os->authCertificateArg;
	    ss->getClientAuthData     = os->getClientAuthData;
	    ss->getClientAuthDataArg  = os->getClientAuthDataArg;
            ss->sniSocketConfig       = os->sniSocketConfig;
            ss->sniSocketConfigArg    = os->sniSocketConfigArg;
	    ss->handleBadCert         = os->handleBadCert;
	    ss->badCertArg            = os->badCertArg;
	    ss->handshakeCallback     = os->handshakeCallback;
	    ss->handshakeCallbackData = os->handshakeCallbackData;
	    ss->pkcs11PinArg          = os->pkcs11PinArg;
    
	    /* Create security data */
	    rv = ssl_CopySecurityInfo(ss, os);
	    if (rv != SECSuccess) {
		goto loser;
	    }
	}
    }
    return ss;

loser:
    ssl_FreeSocket(ss);
    return NULL;
}

static void
ssl_DestroyLocks(sslSocket *ss)
{
    /* Destroy locks. */
    if (ss->firstHandshakeLock) {
    	PZ_DestroyMonitor(ss->firstHandshakeLock);
	ss->firstHandshakeLock = NULL;
    }
    if (ss->ssl3HandshakeLock) {
    	PZ_DestroyMonitor(ss->ssl3HandshakeLock);
	ss->ssl3HandshakeLock = NULL;
    }
    if (ss->specLock) {
    	NSSRWLock_Destroy(ss->specLock);
	ss->specLock = NULL;
    }

    if (ss->recvLock) {
    	PZ_DestroyLock(ss->recvLock);
	ss->recvLock = NULL;
    }
    if (ss->sendLock) {
    	PZ_DestroyLock(ss->sendLock);
	ss->sendLock = NULL;
    }
    if (ss->xmitBufLock) {
    	PZ_DestroyMonitor(ss->xmitBufLock);
	ss->xmitBufLock = NULL;
    }
    if (ss->recvBufLock) {
    	PZ_DestroyMonitor(ss->recvBufLock);
	ss->recvBufLock = NULL;
    }
}

/* Caller holds any relevant locks */
static void
ssl_DestroySocketContents(sslSocket *ss)
{
    /* "i" should be of type SSLKEAType, but CC on IRIX complains during
     * the for loop.
     */
    int        i;

    /* Free up socket */
    ssl_DestroySecurityInfo(&ss->sec);

    ssl3_DestroySSL3Info(ss);

    PORT_Free(ss->saveBuf.buf);
    PORT_Free(ss->pendingBuf.buf);
    ssl_DestroyGather(&ss->gs);

    if (ss->peerID != NULL)
	PORT_Free(ss->peerID);
    if (ss->url != NULL)
	PORT_Free((void *)ss->url);	/* CONST */
    if (ss->cipherSpecs) {
	PORT_Free(ss->cipherSpecs);
	ss->cipherSpecs     = NULL;
	ss->sizeCipherSpecs = 0;
    }

    /* Clean up server configuration */
    for (i=kt_null; i < kt_kea_size; i++) {
	sslServerCerts * sc = ss->serverCerts + i;
	if (sc->serverCert != NULL)
	    CERT_DestroyCertificate(sc->serverCert);
	if (sc->serverCertChain != NULL)
	    CERT_DestroyCertificateList(sc->serverCertChain);
	if (sc->serverKeyPair != NULL)
	    ssl3_FreeKeyPair(sc->serverKeyPair);
    }
    if (ss->stepDownKeyPair) {
	ssl3_FreeKeyPair(ss->stepDownKeyPair);
	ss->stepDownKeyPair = NULL;
    }
    if (ss->ephemeralECDHKeyPair) {
	ssl3_FreeKeyPair(ss->ephemeralECDHKeyPair);
	ss->ephemeralECDHKeyPair = NULL;
    }
    if (ss->certStatusArray) {
	SECITEM_FreeArray(ss->certStatusArray, PR_TRUE);
	ss->certStatusArray = NULL;
    }
    SECITEM_FreeItem(&ss->opt.nextProtoNego, PR_FALSE);
    PORT_Assert(!ss->xtnData.sniNameArr);
    if (ss->xtnData.sniNameArr) {
        PORT_Free(ss->xtnData.sniNameArr);
        ss->xtnData.sniNameArr = NULL;
    }
}

/*
 * free an sslSocket struct, and all the stuff that hangs off of it
 */
void
ssl_FreeSocket(sslSocket *ss)
{
/* Get every lock you can imagine!
** Caller already holds these:
**  SSL_LOCK_READER(ss);
**  SSL_LOCK_WRITER(ss);
*/
    ssl_Get1stHandshakeLock(ss);
    ssl_GetRecvBufLock(ss);
    ssl_GetSSL3HandshakeLock(ss);
    ssl_GetXmitBufLock(ss);
    ssl_GetSpecWriteLock(ss);

    ssl_DestroySocketContents(ss);

    /* Release all the locks acquired above.  */
    SSL_UNLOCK_READER(ss);
    SSL_UNLOCK_WRITER(ss);
    ssl_Release1stHandshakeLock(ss);
    ssl_ReleaseRecvBufLock(ss);
    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_ReleaseXmitBufLock(ss);
    ssl_ReleaseSpecWriteLock(ss);

    ssl_DestroyLocks(ss);

#ifdef DEBUG
    PORT_Memset(ss, 0x1f, sizeof *ss);
#endif
    PORT_Free(ss);
    return;
}

/************************************************************************/
SECStatus 
ssl_EnableNagleDelay(sslSocket *ss, PRBool enabled)
{
    PRFileDesc *       osfd = ss->fd->lower;
    SECStatus         rv = SECFailure;
    PRSocketOptionData opt;

    opt.option         = PR_SockOpt_NoDelay;
    opt.value.no_delay = (PRBool)!enabled;

    if (osfd->methods->setsocketoption) {
        rv = (SECStatus) osfd->methods->setsocketoption(osfd, &opt);
    } else {
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    }

    return rv;
}

static void
ssl_ChooseOps(sslSocket *ss)
{
    ss->ops = ss->opt.useSecurity ? &ssl_secure_ops : &ssl_default_ops;
}

/* Called from SSL_Enable (immediately below) */
static SECStatus
PrepareSocket(sslSocket *ss)
{
    SECStatus     rv = SECSuccess;

    ssl_ChooseOps(ss);
    return rv;
}

SECStatus
SSL_Enable(PRFileDesc *fd, int which, PRBool on)
{
    return SSL_OptionSet(fd, which, on);
}

#ifndef NO_PKCS11_BYPASS
static const PRCallOnceType pristineCallOnce;
static PRCallOnceType setupBypassOnce;

static SECStatus SSL_BypassShutdown(void* appData, void* nssData)
{
    /* unload freeBL shared library from memory */
    BL_Unload();
    setupBypassOnce = pristineCallOnce;
    return SECSuccess;
}

static PRStatus SSL_BypassRegisterShutdown(void)
{
    SECStatus rv = NSS_RegisterShutdown(SSL_BypassShutdown, NULL);
    PORT_Assert(SECSuccess == rv);
    return SECSuccess == rv ? PR_SUCCESS : PR_FAILURE;
}
#endif

static PRStatus SSL_BypassSetup(void)
{
#ifdef NO_PKCS11_BYPASS
    /* Guarantee binary compatibility */
    return PR_SUCCESS;
#else
    return PR_CallOnce(&setupBypassOnce, &SSL_BypassRegisterShutdown);
#endif
}

/* Implements the semantics for SSL_OptionSet(SSL_ENABLE_TLS, on) described in
 * ssl.h in the section "SSL version range setting API".
 */
static void
ssl_EnableTLS(SSLVersionRange *vrange, PRBool on)
{
    if (SSL3_ALL_VERSIONS_DISABLED(vrange)) {
	if (on) {
	    vrange->min = SSL_LIBRARY_VERSION_TLS_1_0;
	    vrange->max = SSL_LIBRARY_VERSION_TLS_1_0;
	} /* else don't change anything */
	return;
    }

    if (on) {
	/* Expand the range of enabled version to include TLS 1.0 */
	vrange->min = PR_MIN(vrange->min, SSL_LIBRARY_VERSION_TLS_1_0);
	vrange->max = PR_MAX(vrange->max, SSL_LIBRARY_VERSION_TLS_1_0);
    } else {
	/* Disable all TLS versions, leaving only SSL 3.0 if it was enabled */
	if (vrange->min == SSL_LIBRARY_VERSION_3_0) {
	    vrange->max = SSL_LIBRARY_VERSION_3_0;
	} else {
	    /* Only TLS was enabled, so now no versions are. */
	    vrange->min = SSL_LIBRARY_VERSION_NONE;
	    vrange->max = SSL_LIBRARY_VERSION_NONE;
	}
    }
}

/* Implements the semantics for SSL_OptionSet(SSL_ENABLE_SSL3, on) described in
 * ssl.h in the section "SSL version range setting API".
 */
static void
ssl_EnableSSL3(SSLVersionRange *vrange, PRBool on)
{
   if (SSL3_ALL_VERSIONS_DISABLED(vrange)) {
	if (on) {
	    vrange->min = SSL_LIBRARY_VERSION_3_0;
	    vrange->max = SSL_LIBRARY_VERSION_3_0;
	} /* else don't change anything */
	return;
    }

   if (on) {
	/* Expand the range of enabled versions to include SSL 3.0. We know
	 * SSL 3.0 or some version of TLS is already enabled at this point, so
	 * we don't need to change vrange->max.
	 */
	vrange->min = SSL_LIBRARY_VERSION_3_0;
   } else {
	/* Disable SSL 3.0, leaving TLS unaffected. */
	if (vrange->max > SSL_LIBRARY_VERSION_3_0) {
	    vrange->min = PR_MAX(vrange->min, SSL_LIBRARY_VERSION_TLS_1_0);
	} else {
	    /* Only SSL 3.0 was enabled, so now no versions are. */
	    vrange->min = SSL_LIBRARY_VERSION_NONE;
	    vrange->max = SSL_LIBRARY_VERSION_NONE;
	}
    }
}

SECStatus
SSL_OptionSet(PRFileDesc *fd, PRInt32 which, PRBool on)
{
    sslSocket *ss = ssl_FindSocket(fd);
    SECStatus  rv = SECSuccess;
    PRBool     holdingLocks;

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in Enable", SSL_GETPID(), fd));
	return SECFailure;
    }

    holdingLocks = (!ss->opt.noLocks);
    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    switch (which) {
      case SSL_SOCKS:
	ss->opt.useSocks = PR_FALSE;
	rv = PrepareSocket(ss);
	if (on) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    rv = SECFailure;
	}
	break;

      case SSL_SECURITY:
	ss->opt.useSecurity = on;
	rv = PrepareSocket(ss);
	break;

      case SSL_REQUEST_CERTIFICATE:
	ss->opt.requestCertificate = on;
	break;

      case SSL_REQUIRE_CERTIFICATE:
	ss->opt.requireCertificate = on;
	break;

      case SSL_HANDSHAKE_AS_CLIENT:
	if ( ss->opt.handshakeAsServer && on ) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    rv = SECFailure;
	    break;
	}
	ss->opt.handshakeAsClient = on;
	break;

      case SSL_HANDSHAKE_AS_SERVER:
	if ( ss->opt.handshakeAsClient && on ) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    rv = SECFailure;
	    break;
	}
	ss->opt.handshakeAsServer = on;
	break;

      case SSL_ENABLE_TLS:
        if (IS_DTLS(ss)) {
	    if (on) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		rv = SECFailure; /* not allowed */
	    }
	    break;
	}
	ssl_EnableTLS(&ss->vrange, on);
	ss->preferredCipher     = NULL;
	if (ss->cipherSpecs) {
	    PORT_Free(ss->cipherSpecs);
	    ss->cipherSpecs     = NULL;
	    ss->sizeCipherSpecs = 0;
	}
	break;

      case SSL_ENABLE_SSL3:
        if (IS_DTLS(ss)) {
	    if (on) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		rv = SECFailure; /* not allowed */
	    }
	    break;
	}
	ssl_EnableSSL3(&ss->vrange, on);
	ss->preferredCipher     = NULL;
	if (ss->cipherSpecs) {
	    PORT_Free(ss->cipherSpecs);
	    ss->cipherSpecs     = NULL;
	    ss->sizeCipherSpecs = 0;
	}
	break;

      case SSL_ENABLE_SSL2:
        if (IS_DTLS(ss)) {
	    if (on) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		rv = SECFailure; /* not allowed */
	    }
	    break;
	}
	ss->opt.enableSSL2       = on;
	if (on) {
	    ss->opt.v2CompatibleHello = on;
	}
	ss->preferredCipher     = NULL;
	if (ss->cipherSpecs) {
	    PORT_Free(ss->cipherSpecs);
	    ss->cipherSpecs     = NULL;
	    ss->sizeCipherSpecs = 0;
	}
	break;

      case SSL_NO_CACHE:
	ss->opt.noCache = on;
	break;

      case SSL_ENABLE_FDX:
	if (on && ss->opt.noLocks) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    rv = SECFailure;
	}
      	ss->opt.fdx = on;
	break;

      case SSL_V2_COMPATIBLE_HELLO:
        if (IS_DTLS(ss)) {
	    if (on) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		rv = SECFailure; /* not allowed */
	    }
	    break;
	}
      	ss->opt.v2CompatibleHello = on;
	if (!on) {
	    ss->opt.enableSSL2    = on;
	}
	break;

      case SSL_ROLLBACK_DETECTION:  
	ss->opt.detectRollBack = on;
        break;

      case SSL_NO_STEP_DOWN:        
	ss->opt.noStepDown     = on;         
	if (on) 
	    SSL_DisableExportCipherSuites(fd);
	break;

      case SSL_BYPASS_PKCS11:
	if (ss->handshakeBegun) {
	    PORT_SetError(PR_INVALID_STATE_ERROR);
	    rv = SECFailure;
	} else {
            if (PR_FALSE != on) {
                if (PR_SUCCESS == SSL_BypassSetup() ) {
#ifdef NO_PKCS11_BYPASS
                    ss->opt.bypassPKCS11   = PR_FALSE;
#else
                    ss->opt.bypassPKCS11   = on;
#endif
                } else {
                    rv = SECFailure;
                }
            } else {
                ss->opt.bypassPKCS11   = PR_FALSE;
            }
	}
	break;

      case SSL_NO_LOCKS:
	if (on && ss->opt.fdx) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    rv = SECFailure;
	}
	if (on && ssl_force_locks) 
	    on = PR_FALSE;	/* silent override */
	ss->opt.noLocks   = on;
	if (on) {
	    locksEverDisabled = PR_TRUE;
	    strcpy(lockStatus + LOCKSTATUS_OFFSET, "DISABLED.");
	} else if (!holdingLocks) {
	    rv = ssl_MakeLocks(ss);
	    if (rv != SECSuccess) {
		ss->opt.noLocks   = PR_TRUE;
	    }
	}
	break;

      case SSL_ENABLE_SESSION_TICKETS:
	ss->opt.enableSessionTickets = on;
	break;

      case SSL_ENABLE_DEFLATE:
	ss->opt.enableDeflate = on;
	break;

      case SSL_ENABLE_RENEGOTIATION:
	ss->opt.enableRenegotiation = on;
	break;

      case SSL_REQUIRE_SAFE_NEGOTIATION:
	ss->opt.requireSafeNegotiation = on;
	break;

      case SSL_ENABLE_FALSE_START:
	ss->opt.enableFalseStart = on;
	break;

      case SSL_CBC_RANDOM_IV:
	ss->opt.cbcRandomIV = on;
	break;

      case SSL_ENABLE_OCSP_STAPLING:
       ss->opt.enableOCSPStapling = on;
       break;

      default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	rv = SECFailure;
    }

    /* We can't use the macros for releasing the locks here,
     * because ss->opt.noLocks might have changed just above.
     * We must release these locks (monitors) here, if we aquired them above,
     * regardless of the current value of ss->opt.noLocks.
     */
    if (holdingLocks) {
	PZ_ExitMonitor((ss)->ssl3HandshakeLock);
	PZ_ExitMonitor((ss)->firstHandshakeLock);
    }

    return rv;
}

SECStatus
SSL_OptionGet(PRFileDesc *fd, PRInt32 which, PRBool *pOn)
{
    sslSocket *ss = ssl_FindSocket(fd);
    SECStatus  rv = SECSuccess;
    PRBool     on = PR_FALSE;

    if (!pOn) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in Enable", SSL_GETPID(), fd));
	*pOn = PR_FALSE;
	return SECFailure;
    }

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    switch (which) {
    case SSL_SOCKS:               on = PR_FALSE;               break;
    case SSL_SECURITY:            on = ss->opt.useSecurity;        break;
    case SSL_REQUEST_CERTIFICATE: on = ss->opt.requestCertificate; break;
    case SSL_REQUIRE_CERTIFICATE: on = ss->opt.requireCertificate; break;
    case SSL_HANDSHAKE_AS_CLIENT: on = ss->opt.handshakeAsClient;  break;
    case SSL_HANDSHAKE_AS_SERVER: on = ss->opt.handshakeAsServer;  break;
    case SSL_ENABLE_TLS:
	on = ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_0;
	break;
    case SSL_ENABLE_SSL3:
	on = ss->vrange.min == SSL_LIBRARY_VERSION_3_0;
	break;
    case SSL_ENABLE_SSL2:         on = ss->opt.enableSSL2;         break;
    case SSL_NO_CACHE:            on = ss->opt.noCache;            break;
    case SSL_ENABLE_FDX:          on = ss->opt.fdx;                break;
    case SSL_V2_COMPATIBLE_HELLO: on = ss->opt.v2CompatibleHello;  break;
    case SSL_ROLLBACK_DETECTION:  on = ss->opt.detectRollBack;     break;
    case SSL_NO_STEP_DOWN:        on = ss->opt.noStepDown;         break;
    case SSL_BYPASS_PKCS11:       on = ss->opt.bypassPKCS11;       break;
    case SSL_NO_LOCKS:            on = ss->opt.noLocks;            break;
    case SSL_ENABLE_SESSION_TICKETS:
	on = ss->opt.enableSessionTickets;
	break;
    case SSL_ENABLE_DEFLATE:      on = ss->opt.enableDeflate;      break;
    case SSL_ENABLE_RENEGOTIATION:     
                                  on = ss->opt.enableRenegotiation; break;
    case SSL_REQUIRE_SAFE_NEGOTIATION: 
                                  on = ss->opt.requireSafeNegotiation; break;
    case SSL_ENABLE_FALSE_START:  on = ss->opt.enableFalseStart;   break;
    case SSL_CBC_RANDOM_IV:       on = ss->opt.cbcRandomIV;        break;
    case SSL_ENABLE_OCSP_STAPLING: on = ss->opt.enableOCSPStapling; break;

    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	rv = SECFailure;
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    *pOn = on;
    return rv;
}

SECStatus
SSL_OptionGetDefault(PRInt32 which, PRBool *pOn)
{
    SECStatus  rv = SECSuccess;
    PRBool     on = PR_FALSE;

    if (!pOn) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    ssl_SetDefaultsFromEnvironment();

    switch (which) {
    case SSL_SOCKS:               on = PR_FALSE;                        break;
    case SSL_SECURITY:            on = ssl_defaults.useSecurity;        break;
    case SSL_REQUEST_CERTIFICATE: on = ssl_defaults.requestCertificate; break;
    case SSL_REQUIRE_CERTIFICATE: on = ssl_defaults.requireCertificate; break;
    case SSL_HANDSHAKE_AS_CLIENT: on = ssl_defaults.handshakeAsClient;  break;
    case SSL_HANDSHAKE_AS_SERVER: on = ssl_defaults.handshakeAsServer;  break;
    case SSL_ENABLE_TLS:
	on = versions_defaults_stream.max >= SSL_LIBRARY_VERSION_TLS_1_0;
	break;
    case SSL_ENABLE_SSL3:
	on = versions_defaults_stream.min == SSL_LIBRARY_VERSION_3_0;
	break;
    case SSL_ENABLE_SSL2:         on = ssl_defaults.enableSSL2;         break;
    case SSL_NO_CACHE:            on = ssl_defaults.noCache;		break;
    case SSL_ENABLE_FDX:          on = ssl_defaults.fdx;                break;
    case SSL_V2_COMPATIBLE_HELLO: on = ssl_defaults.v2CompatibleHello;  break;
    case SSL_ROLLBACK_DETECTION:  on = ssl_defaults.detectRollBack;     break;
    case SSL_NO_STEP_DOWN:        on = ssl_defaults.noStepDown;         break;
    case SSL_BYPASS_PKCS11:       on = ssl_defaults.bypassPKCS11;       break;
    case SSL_NO_LOCKS:            on = ssl_defaults.noLocks;            break;
    case SSL_ENABLE_SESSION_TICKETS:
	on = ssl_defaults.enableSessionTickets;
	break;
    case SSL_ENABLE_DEFLATE:      on = ssl_defaults.enableDeflate;      break;
    case SSL_ENABLE_RENEGOTIATION:     
                                  on = ssl_defaults.enableRenegotiation; break;
    case SSL_REQUIRE_SAFE_NEGOTIATION: 
                                  on = ssl_defaults.requireSafeNegotiation; 
				  break;
    case SSL_ENABLE_FALSE_START:  on = ssl_defaults.enableFalseStart;   break;
    case SSL_CBC_RANDOM_IV:       on = ssl_defaults.cbcRandomIV;        break;
    case SSL_ENABLE_OCSP_STAPLING:
       on = ssl_defaults.enableOCSPStapling;
       break;

    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	rv = SECFailure;
    }

    *pOn = on;
    return rv;
}

/* XXX Use Global Lock to protect this stuff. */
SECStatus
SSL_EnableDefault(int which, PRBool on)
{
    return SSL_OptionSetDefault(which, on);
}

SECStatus
SSL_OptionSetDefault(PRInt32 which, PRBool on)
{
    SECStatus status = ssl_Init();

    if (status != SECSuccess) {
	return status;
    }

    ssl_SetDefaultsFromEnvironment();

    switch (which) {
      case SSL_SOCKS:
	ssl_defaults.useSocks = PR_FALSE;
	if (on) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
	break;

      case SSL_SECURITY:
	ssl_defaults.useSecurity = on;
	break;

      case SSL_REQUEST_CERTIFICATE:
	ssl_defaults.requestCertificate = on;
	break;

      case SSL_REQUIRE_CERTIFICATE:
	ssl_defaults.requireCertificate = on;
	break;

      case SSL_HANDSHAKE_AS_CLIENT:
	if ( ssl_defaults.handshakeAsServer && on ) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
	ssl_defaults.handshakeAsClient = on;
	break;

      case SSL_HANDSHAKE_AS_SERVER:
	if ( ssl_defaults.handshakeAsClient && on ) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
	ssl_defaults.handshakeAsServer = on;
	break;

      case SSL_ENABLE_TLS:
	ssl_EnableTLS(&versions_defaults_stream, on);
	break;

      case SSL_ENABLE_SSL3:
	ssl_EnableSSL3(&versions_defaults_stream, on);
	break;

      case SSL_ENABLE_SSL2:
	ssl_defaults.enableSSL2 = on;
	if (on) {
	    ssl_defaults.v2CompatibleHello = on;
	}
	break;

      case SSL_NO_CACHE:
	ssl_defaults.noCache = on;
	break;

      case SSL_ENABLE_FDX:
	if (on && ssl_defaults.noLocks) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
      	ssl_defaults.fdx = on;
	break;

      case SSL_V2_COMPATIBLE_HELLO:
      	ssl_defaults.v2CompatibleHello = on;
	if (!on) {
	    ssl_defaults.enableSSL2    = on;
	}
	break;

      case SSL_ROLLBACK_DETECTION:  
	ssl_defaults.detectRollBack = on;
	break;

      case SSL_NO_STEP_DOWN:        
	ssl_defaults.noStepDown     = on;         
	if (on)
	    SSL_DisableDefaultExportCipherSuites();
	break;

      case SSL_BYPASS_PKCS11:
        if (PR_FALSE != on) {
            if (PR_SUCCESS == SSL_BypassSetup()) {
#ifdef NO_PKCS11_BYPASS
                ssl_defaults.bypassPKCS11   = PR_FALSE;
#else
                ssl_defaults.bypassPKCS11   = on;
#endif
            } else {
                return SECFailure;
            }
        } else {
            ssl_defaults.bypassPKCS11   = PR_FALSE;
        }
	break;

      case SSL_NO_LOCKS:
	if (on && ssl_defaults.fdx) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
	if (on && ssl_force_locks) 
	    on = PR_FALSE;		/* silent override */
	ssl_defaults.noLocks        = on;
	if (on) {
	    locksEverDisabled = PR_TRUE;
	    strcpy(lockStatus + LOCKSTATUS_OFFSET, "DISABLED.");
	}
	break;

      case SSL_ENABLE_SESSION_TICKETS:
	ssl_defaults.enableSessionTickets = on;
	break;

      case SSL_ENABLE_DEFLATE:
	ssl_defaults.enableDeflate = on;
	break;

      case SSL_ENABLE_RENEGOTIATION:
	ssl_defaults.enableRenegotiation = on;
	break;

      case SSL_REQUIRE_SAFE_NEGOTIATION:
	ssl_defaults.requireSafeNegotiation = on;
	break;

      case SSL_ENABLE_FALSE_START:
	ssl_defaults.enableFalseStart = on;
	break;

      case SSL_CBC_RANDOM_IV:
	ssl_defaults.cbcRandomIV = on;
	break;

      case SSL_ENABLE_OCSP_STAPLING:
       ssl_defaults.enableOCSPStapling = on;
       break;

      default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    return SECSuccess;
}

/* function tells us if the cipher suite is one that we no longer support. */
static PRBool 
ssl_IsRemovedCipherSuite(PRInt32 suite)
{
    switch (suite) {
    case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
    case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
    case SSL_FORTEZZA_DMS_WITH_RC4_128_SHA:
    	return PR_TRUE;
    default:
    	return PR_FALSE;
    }
}

/* Part of the public NSS API.
 * Since this is a global (not per-socket) setting, we cannot use the
 * HandshakeLock to protect this.  Probably want a global lock.
 */
SECStatus
SSL_SetPolicy(long which, int policy)
{
    if ((which & 0xfffe) == SSL_RSA_OLDFIPS_WITH_3DES_EDE_CBC_SHA) {
    	/* one of the two old FIPS ciphers */
	if (which == SSL_RSA_OLDFIPS_WITH_3DES_EDE_CBC_SHA) 
	    which = SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA;
	else if (which == SSL_RSA_OLDFIPS_WITH_DES_CBC_SHA)
	    which = SSL_RSA_FIPS_WITH_DES_CBC_SHA;
    }
    if (ssl_IsRemovedCipherSuite(which))
    	return SECSuccess;
    return SSL_CipherPolicySet(which, policy);
}

SECStatus
SSL_CipherPolicySet(PRInt32 which, PRInt32 policy)
{
    SECStatus rv = ssl_Init();

    if (rv != SECSuccess) {
	return rv;
    }

    if (ssl_IsRemovedCipherSuite(which)) {
    	rv = SECSuccess;
    } else if (SSL_IS_SSL2_CIPHER(which)) {
	rv = ssl2_SetPolicy(which, policy);
    } else {
	rv = ssl3_SetPolicy((ssl3CipherSuite)which, policy);
    }
    return rv;
}

SECStatus
SSL_CipherPolicyGet(PRInt32 which, PRInt32 *oPolicy)
{
    SECStatus rv;

    if (!oPolicy) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    if (ssl_IsRemovedCipherSuite(which)) {
	*oPolicy = SSL_NOT_ALLOWED;
    	rv = SECSuccess;
    } else if (SSL_IS_SSL2_CIPHER(which)) {
	rv = ssl2_GetPolicy(which, oPolicy);
    } else {
	rv = ssl3_GetPolicy((ssl3CipherSuite)which, oPolicy);
    }
    return rv;
}

/* Part of the public NSS API.
 * Since this is a global (not per-socket) setting, we cannot use the
 * HandshakeLock to protect this.  Probably want a global lock.
 * These changes have no effect on any sslSockets already created. 
 */
SECStatus
SSL_EnableCipher(long which, PRBool enabled)
{
    if ((which & 0xfffe) == SSL_RSA_OLDFIPS_WITH_3DES_EDE_CBC_SHA) {
    	/* one of the two old FIPS ciphers */
	if (which == SSL_RSA_OLDFIPS_WITH_3DES_EDE_CBC_SHA) 
	    which = SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA;
	else if (which == SSL_RSA_OLDFIPS_WITH_DES_CBC_SHA)
	    which = SSL_RSA_FIPS_WITH_DES_CBC_SHA;
    }
    if (ssl_IsRemovedCipherSuite(which))
    	return SECSuccess;
    return SSL_CipherPrefSetDefault(which, enabled);
}

SECStatus
SSL_CipherPrefSetDefault(PRInt32 which, PRBool enabled)
{
    SECStatus rv = ssl_Init();

    if (rv != SECSuccess) {
	return rv;
    }

    if (ssl_IsRemovedCipherSuite(which))
    	return SECSuccess;
    if (enabled && ssl_defaults.noStepDown && SSL_IsExportCipherSuite(which)) {
    	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return SECFailure;
    }
    if (SSL_IS_SSL2_CIPHER(which)) {
	rv = ssl2_CipherPrefSetDefault(which, enabled);
    } else {
	rv = ssl3_CipherPrefSetDefault((ssl3CipherSuite)which, enabled);
    }
    return rv;
}

SECStatus 
SSL_CipherPrefGetDefault(PRInt32 which, PRBool *enabled)
{
    SECStatus  rv;
    
    if (!enabled) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    if (ssl_IsRemovedCipherSuite(which)) {
	*enabled = PR_FALSE;
    	rv = SECSuccess;
    } else if (SSL_IS_SSL2_CIPHER(which)) {
	rv = ssl2_CipherPrefGetDefault(which, enabled);
    } else {
	rv = ssl3_CipherPrefGetDefault((ssl3CipherSuite)which, enabled);
    }
    return rv;
}

SECStatus
SSL_CipherPrefSet(PRFileDesc *fd, PRInt32 which, PRBool enabled)
{
    SECStatus rv;
    sslSocket *ss = ssl_FindSocket(fd);
    
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in CipherPrefSet", SSL_GETPID(), fd));
	return SECFailure;
    }
    if (ssl_IsRemovedCipherSuite(which))
    	return SECSuccess;
    if (enabled && ss->opt.noStepDown && SSL_IsExportCipherSuite(which)) {
    	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return SECFailure;
    }
    if (SSL_IS_SSL2_CIPHER(which)) {
	rv = ssl2_CipherPrefSet(ss, which, enabled);
    } else {
	rv = ssl3_CipherPrefSet(ss, (ssl3CipherSuite)which, enabled);
    }
    return rv;
}

SECStatus 
SSL_CipherPrefGet(PRFileDesc *fd, PRInt32 which, PRBool *enabled)
{
    SECStatus  rv;
    sslSocket *ss = ssl_FindSocket(fd);
    
    if (!enabled) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in CipherPrefGet", SSL_GETPID(), fd));
	*enabled = PR_FALSE;
	return SECFailure;
    }
    if (ssl_IsRemovedCipherSuite(which)) {
	*enabled = PR_FALSE;
    	rv = SECSuccess;
    } else if (SSL_IS_SSL2_CIPHER(which)) {
	rv = ssl2_CipherPrefGet(ss, which, enabled);
    } else {
	rv = ssl3_CipherPrefGet(ss, (ssl3CipherSuite)which, enabled);
    }
    return rv;
}

SECStatus
NSS_SetDomesticPolicy(void)
{
    SECStatus      status = SECSuccess;
    cipherPolicy * policy;

    for (policy = ssl_ciphers; policy->cipher != 0; ++policy) {
	status = SSL_SetPolicy(policy->cipher, SSL_ALLOWED);
	if (status != SECSuccess)
	    break;
    }
    return status;
}

SECStatus
NSS_SetExportPolicy(void)
{
    return NSS_SetDomesticPolicy();
}

SECStatus
NSS_SetFrancePolicy(void)
{
    return NSS_SetDomesticPolicy();
}



/* LOCKS ??? XXX */
static PRFileDesc *
ssl_ImportFD(PRFileDesc *model, PRFileDesc *fd, SSLProtocolVariant variant)
{
    sslSocket * ns = NULL;
    PRStatus    rv;
    PRNetAddr   addr;
    SECStatus	status = ssl_Init();

    if (status != SECSuccess) {
	return NULL;
    }

    if (model == NULL) {
	/* Just create a default socket if we're given NULL for the model */
	ns = ssl_NewSocket((PRBool)(!ssl_defaults.noLocks), variant);
    } else {
	sslSocket * ss = ssl_FindSocket(model);
	if (ss == NULL || ss->protocolVariant != variant) {
	    SSL_DBG(("%d: SSL[%d]: bad model socket in ssl_ImportFD", 
	    	      SSL_GETPID(), model));
	    return NULL;
	}
	ns = ssl_DupSocket(ss);
    }
    if (ns == NULL)
    	return NULL;

    rv = ssl_PushIOLayer(ns, fd, PR_TOP_IO_LAYER);
    if (rv != PR_SUCCESS) {
	ssl_FreeSocket(ns);
	SET_ERROR_CODE
	return NULL;
    }
#ifdef _WIN32
    PR_Sleep(PR_INTERVAL_NO_WAIT);     /* workaround NT winsock connect bug. */
#endif
    ns = ssl_FindSocket(fd);
    PORT_Assert(ns);
    if (ns)
	ns->TCPconnected = (PR_SUCCESS == ssl_DefGetpeername(ns, &addr));
    return fd;
}

PRFileDesc *
SSL_ImportFD(PRFileDesc *model, PRFileDesc *fd)
{
    return ssl_ImportFD(model, fd, ssl_variant_stream);
}

PRFileDesc *
DTLS_ImportFD(PRFileDesc *model, PRFileDesc *fd)
{
    return ssl_ImportFD(model, fd, ssl_variant_datagram);
}

SECStatus
SSL_SetNextProtoCallback(PRFileDesc *fd, SSLNextProtoCallback callback,
			 void *arg)
{
    sslSocket *ss = ssl_FindSocket(fd);

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetNextProtoCallback", SSL_GETPID(),
		 fd));
	return SECFailure;
    }

    ssl_GetSSL3HandshakeLock(ss);
    ss->nextProtoCallback = callback;
    ss->nextProtoArg = arg;
    ssl_ReleaseSSL3HandshakeLock(ss);

    return SECSuccess;
}

/* ssl_NextProtoNegoCallback is set as an NPN callback for the case when
 * SSL_SetNextProtoNego is used.
 */
static SECStatus
ssl_NextProtoNegoCallback(void *arg, PRFileDesc *fd,
			  const unsigned char *protos, unsigned int protos_len,
			  unsigned char *protoOut, unsigned int *protoOutLen,
			  unsigned int protoMaxLen)
{
    unsigned int i, j;
    const unsigned char *result;
    sslSocket *ss = ssl_FindSocket(fd);

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in ssl_NextProtoNegoCallback",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if (protos_len == 0) {
	/* The server supports the extension, but doesn't have any protocols
	 * configured. In this case we request our favoured protocol. */
	goto pick_first;
    }

    /* For each protocol in server preference, see if we support it. */
    for (i = 0; i < protos_len; ) {
	for (j = 0; j < ss->opt.nextProtoNego.len; ) {
	    if (protos[i] == ss->opt.nextProtoNego.data[j] &&
		PORT_Memcmp(&protos[i+1], &ss->opt.nextProtoNego.data[j+1],
			     protos[i]) == 0) {
		/* We found a match. */
		ss->ssl3.nextProtoState = SSL_NEXT_PROTO_NEGOTIATED;
		result = &protos[i];
		goto found;
	    }
	    j += 1 + (unsigned int)ss->opt.nextProtoNego.data[j];
	}
	i += 1 + (unsigned int)protos[i];
    }

pick_first:
    ss->ssl3.nextProtoState = SSL_NEXT_PROTO_NO_OVERLAP;
    result = ss->opt.nextProtoNego.data;

found:
    if (protoMaxLen < result[0]) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    memcpy(protoOut, result + 1, result[0]);
    *protoOutLen = result[0];
    return SECSuccess;
}

SECStatus
SSL_SetNextProtoNego(PRFileDesc *fd, const unsigned char *data,
		     unsigned int length)
{
    sslSocket *ss;
    SECStatus rv;
    SECItem dataItem = { siBuffer, (unsigned char *) data, length };

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetNextProtoNego",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if (ssl3_ValidateNextProtoNego(data, length) != SECSuccess)
	return SECFailure;

    ssl_GetSSL3HandshakeLock(ss);
    SECITEM_FreeItem(&ss->opt.nextProtoNego, PR_FALSE);
    rv = SECITEM_CopyItem(NULL, &ss->opt.nextProtoNego, &dataItem);
    ssl_ReleaseSSL3HandshakeLock(ss);

    if (rv != SECSuccess)
	return rv;

    return SSL_SetNextProtoCallback(fd, ssl_NextProtoNegoCallback, NULL);
}

SECStatus
SSL_GetNextProto(PRFileDesc *fd, SSLNextProtoState *state, unsigned char *buf,
		 unsigned int *bufLen, unsigned int bufLenMax)
{
    sslSocket *ss = ssl_FindSocket(fd);

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetNextProto", SSL_GETPID(),
		 fd));
	return SECFailure;
    }

    if (!state || !buf || !bufLen) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    *state = ss->ssl3.nextProtoState;

    if (ss->ssl3.nextProtoState != SSL_NEXT_PROTO_NO_SUPPORT &&
	ss->ssl3.nextProto.data) {
	if (ss->ssl3.nextProto.len > bufLenMax) {
	    PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	    return SECFailure;
	}
	PORT_Memcpy(buf, ss->ssl3.nextProto.data, ss->ssl3.nextProto.len);
	*bufLen = ss->ssl3.nextProto.len;
    } else {
	*bufLen = 0;
    }

    return SECSuccess;
}

SECStatus SSL_SetSRTPCiphers(PRFileDesc *fd,
			     const PRUint16 *ciphers,
			     unsigned int numCiphers)
{
    sslSocket *ss;
    unsigned int i;

    ss = ssl_FindSocket(fd);
    if (!ss || !IS_DTLS(ss)) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetSRTPCiphers",
		 SSL_GETPID(), fd));
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    if (numCiphers > MAX_DTLS_SRTP_CIPHER_SUITES) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    ss->ssl3.dtlsSRTPCipherCount = 0;
    for (i = 0; i < numCiphers; i++) {
	const PRUint16 *srtpCipher = srtpCiphers;

	while (*srtpCipher) {
	    if (ciphers[i] == *srtpCipher)
		break;
	    srtpCipher++;
	}
	if (*srtpCipher) {
	    ss->ssl3.dtlsSRTPCiphers[ss->ssl3.dtlsSRTPCipherCount++] =
		ciphers[i];
	} else {
	    SSL_DBG(("%d: SSL[%d]: invalid or unimplemented SRTP cipher "
		    "suite specified: 0x%04hx", SSL_GETPID(), fd,
		    ciphers[i]));
	}
    }

    if (ss->ssl3.dtlsSRTPCipherCount == 0) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    return SECSuccess;
}

SECStatus
SSL_GetSRTPCipher(PRFileDesc *fd, PRUint16 *cipher)
{
    sslSocket * ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetSRTPCipher",
		 SSL_GETPID(), fd));
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    if (!ss->ssl3.dtlsSRTPCipherSuite) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    *cipher = ss->ssl3.dtlsSRTPCipherSuite;
    return SECSuccess;
}

PRFileDesc *
SSL_ReconfigFD(PRFileDesc *model, PRFileDesc *fd)
{
    sslSocket * sm = NULL, *ss = NULL;
    int i;
    sslServerCerts * mc = NULL;
    sslServerCerts * sc = NULL;

    if (model == NULL) {
        PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
        return NULL;
    }
    sm = ssl_FindSocket(model);
    if (sm == NULL) {
        SSL_DBG(("%d: SSL[%d]: bad model socket in ssl_ReconfigFD", 
                 SSL_GETPID(), model));
        return NULL;
    }
    ss = ssl_FindSocket(fd);
    PORT_Assert(ss);
    if (ss == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    
    ss->opt  = sm->opt;
    ss->vrange = sm->vrange;
    PORT_Memcpy(ss->cipherSuites, sm->cipherSuites, sizeof sm->cipherSuites);
    PORT_Memcpy(ss->ssl3.dtlsSRTPCiphers, sm->ssl3.dtlsSRTPCiphers,
                sizeof(PRUint16) * sm->ssl3.dtlsSRTPCipherCount);
    ss->ssl3.dtlsSRTPCipherCount = sm->ssl3.dtlsSRTPCipherCount;

    if (!ss->opt.useSecurity) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    /* This int should be SSLKEAType, but CC on Irix complains,
     * during the for loop.
     */
    for (i=kt_null; i < kt_kea_size; i++) {
        mc = &(sm->serverCerts[i]);
        sc = &(ss->serverCerts[i]);
        if (mc->serverCert && mc->serverCertChain) {
            if (sc->serverCert) {
                CERT_DestroyCertificate(sc->serverCert);
            }
            sc->serverCert      = CERT_DupCertificate(mc->serverCert);
            if (sc->serverCertChain) {
                CERT_DestroyCertificateList(sc->serverCertChain);
            }
            sc->serverCertChain = CERT_DupCertList(mc->serverCertChain);
            if (!sc->serverCertChain)
                goto loser;
        }
        if (mc->serverKeyPair) {
            if (sc->serverKeyPair) {
                ssl3_FreeKeyPair(sc->serverKeyPair);
            }
            sc->serverKeyPair = ssl3_GetKeyPairRef(mc->serverKeyPair);
            sc->serverKeyBits = mc->serverKeyBits;
        }
    }
    if (sm->stepDownKeyPair) {
        if (ss->stepDownKeyPair) {
            ssl3_FreeKeyPair(ss->stepDownKeyPair);
        }
        ss->stepDownKeyPair = ssl3_GetKeyPairRef(sm->stepDownKeyPair);
    }
    if (sm->ephemeralECDHKeyPair) {
        if (ss->ephemeralECDHKeyPair) {
            ssl3_FreeKeyPair(ss->ephemeralECDHKeyPair);
        }
        ss->ephemeralECDHKeyPair =
            ssl3_GetKeyPairRef(sm->ephemeralECDHKeyPair);
    }
    if (sm->certStatusArray) {
	if (ss->certStatusArray) {
	    SECITEM_FreeArray(ss->certStatusArray, PR_TRUE);
	    ss->certStatusArray = NULL;
	}
	ss->certStatusArray = SECITEM_DupArray(NULL, sm->certStatusArray);
    }
    /* copy trust anchor names */
    if (sm->ssl3.ca_list) {
        if (ss->ssl3.ca_list) {
            CERT_FreeDistNames(ss->ssl3.ca_list);
        }
        ss->ssl3.ca_list = CERT_DupDistNames(sm->ssl3.ca_list);
        if (!ss->ssl3.ca_list) {
            goto loser;
        }
    }
    
    if (sm->authCertificate)
        ss->authCertificate       = sm->authCertificate;
    if (sm->authCertificateArg)
        ss->authCertificateArg    = sm->authCertificateArg;
    if (sm->getClientAuthData)
        ss->getClientAuthData     = sm->getClientAuthData;
    if (sm->getClientAuthDataArg)
        ss->getClientAuthDataArg  = sm->getClientAuthDataArg;
    if (sm->sniSocketConfig)
        ss->sniSocketConfig       = sm->sniSocketConfig;
    if (sm->sniSocketConfigArg)
        ss->sniSocketConfigArg    = sm->sniSocketConfigArg;
    if (sm->handleBadCert)
        ss->handleBadCert         = sm->handleBadCert;
    if (sm->badCertArg)
        ss->badCertArg            = sm->badCertArg;
    if (sm->handshakeCallback)
        ss->handshakeCallback     = sm->handshakeCallback;
    if (sm->handshakeCallbackData)
        ss->handshakeCallbackData = sm->handshakeCallbackData;
    if (sm->pkcs11PinArg)
        ss->pkcs11PinArg          = sm->pkcs11PinArg;
    return fd;
loser:
    return NULL;
}

PRBool
ssl3_VersionIsSupported(SSLProtocolVariant protocolVariant,
			SSL3ProtocolVersion version)
{
    switch (protocolVariant) {
    case ssl_variant_stream:
	return (version >= SSL_LIBRARY_VERSION_3_0 &&
		version <= SSL_LIBRARY_VERSION_MAX_SUPPORTED);
    case ssl_variant_datagram:
	return (version >= SSL_LIBRARY_VERSION_TLS_1_1 &&
		version <= SSL_LIBRARY_VERSION_MAX_SUPPORTED);
    default:
	/* Can't get here */
	PORT_Assert(PR_FALSE);
	return PR_FALSE;
    }
}

/* Returns PR_TRUE if the given version range is valid and
** fully supported; otherwise, returns PR_FALSE.
*/
static PRBool
ssl3_VersionRangeIsValid(SSLProtocolVariant protocolVariant,
			 const SSLVersionRange *vrange)
{
    return vrange &&
	   vrange->min <= vrange->max &&
	   ssl3_VersionIsSupported(protocolVariant, vrange->min) &&
	   ssl3_VersionIsSupported(protocolVariant, vrange->max);
}

SECStatus
SSL_VersionRangeGetSupported(SSLProtocolVariant protocolVariant,
			     SSLVersionRange *vrange)
{
    if (!vrange) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    switch (protocolVariant) {
    case ssl_variant_stream:
	vrange->min = SSL_LIBRARY_VERSION_3_0;
	vrange->max = SSL_LIBRARY_VERSION_MAX_SUPPORTED;
	break;
    case ssl_variant_datagram:
	vrange->min = SSL_LIBRARY_VERSION_TLS_1_1;
	vrange->max = SSL_LIBRARY_VERSION_MAX_SUPPORTED;
	break;
    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    return SECSuccess;
}

SECStatus
SSL_VersionRangeGetDefault(SSLProtocolVariant protocolVariant,
			   SSLVersionRange *vrange)
{
    if ((protocolVariant != ssl_variant_stream &&
	 protocolVariant != ssl_variant_datagram) || !vrange) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    *vrange = *VERSIONS_DEFAULTS(protocolVariant);

    return SECSuccess;
}

SECStatus
SSL_VersionRangeSetDefault(SSLProtocolVariant protocolVariant,
			   const SSLVersionRange *vrange)
{
    if (!ssl3_VersionRangeIsValid(protocolVariant, vrange)) {
	PORT_SetError(SSL_ERROR_INVALID_VERSION_RANGE);
	return SECFailure;
    }

    *VERSIONS_DEFAULTS(protocolVariant) = *vrange;

    return SECSuccess;
}

SECStatus
SSL_VersionRangeGet(PRFileDesc *fd, SSLVersionRange *vrange)
{
    sslSocket *ss = ssl_FindSocket(fd);

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL3_VersionRangeGet",
		SSL_GETPID(), fd));
	return SECFailure;
    }

    if (!vrange) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    *vrange = ss->vrange;

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return SECSuccess;
}

SECStatus
SSL_VersionRangeSet(PRFileDesc *fd, const SSLVersionRange *vrange)
{
    sslSocket *ss = ssl_FindSocket(fd);

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL3_VersionRangeSet",
		SSL_GETPID(), fd));
	return SECFailure;
    }

    if (!ssl3_VersionRangeIsValid(ss->protocolVariant, vrange)) {
	PORT_SetError(SSL_ERROR_INVALID_VERSION_RANGE);
	return SECFailure;
    }

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    ss->vrange = *vrange;

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return SECSuccess;
}

const SECItemArray *
SSL_PeerStapledOCSPResponses(PRFileDesc *fd)
{
    sslSocket *ss = ssl_FindSocket(fd);

    if (!ss) {
       SSL_DBG(("%d: SSL[%d]: bad socket in SSL_PeerStapledOCSPResponses",
                SSL_GETPID(), fd));
       return NULL;
    }

    if (!ss->sec.ci.sid) {
       PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
       return NULL;
    }
    
    return &ss->sec.ci.sid->peerCertStatus;
}

/************************************************************************/
/* The following functions are the TOP LEVEL SSL functions.
** They all get called through the NSPRIOMethods table below.
*/

static PRFileDesc * PR_CALLBACK
ssl_Accept(PRFileDesc *fd, PRNetAddr *sockaddr, PRIntervalTime timeout)
{
    sslSocket  *ss;
    sslSocket  *ns 	= NULL;
    PRFileDesc *newfd 	= NULL;
    PRFileDesc *osfd;
    PRStatus    status;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in accept", SSL_GETPID(), fd));
	return NULL;
    }

    /* IF this is a listen socket, there shouldn't be any I/O going on */
    SSL_LOCK_READER(ss);
    SSL_LOCK_WRITER(ss);
    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    ss->cTimeout = timeout;

    osfd = ss->fd->lower;

    /* First accept connection */
    newfd = osfd->methods->accept(osfd, sockaddr, timeout);
    if (newfd == NULL) {
	SSL_DBG(("%d: SSL[%d]: accept failed, errno=%d",
		 SSL_GETPID(), ss->fd, PORT_GetError()));
    } else {
	/* Create ssl module */
	ns = ssl_DupSocket(ss);
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);
    SSL_UNLOCK_WRITER(ss);
    SSL_UNLOCK_READER(ss);			/* ss isn't used below here. */

    if (ns == NULL)
	goto loser;

    /* push ssl module onto the new socket */
    status = ssl_PushIOLayer(ns, newfd, PR_TOP_IO_LAYER);
    if (status != PR_SUCCESS)
	goto loser;

    /* Now start server connection handshake with client.
    ** Don't need locks here because nobody else has a reference to ns yet.
    */
    if ( ns->opt.useSecurity ) {
	if ( ns->opt.handshakeAsClient ) {
	    ns->handshake = ssl2_BeginClientHandshake;
	    ss->handshaking = sslHandshakingAsClient;
	} else {
	    ns->handshake = ssl2_BeginServerHandshake;
	    ss->handshaking = sslHandshakingAsServer;
	}
    }
    ns->TCPconnected = 1;
    return newfd;

loser:
    if (ns != NULL)
	ssl_FreeSocket(ns);
    if (newfd != NULL)
	PR_Close(newfd);
    return NULL;
}

static PRStatus PR_CALLBACK
ssl_Connect(PRFileDesc *fd, const PRNetAddr *sockaddr, PRIntervalTime timeout)
{
    sslSocket *ss;
    PRStatus   rv;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in connect", SSL_GETPID(), fd));
	return PR_FAILURE;
    }

    /* IF this is a listen socket, there shouldn't be any I/O going on */
    SSL_LOCK_READER(ss);
    SSL_LOCK_WRITER(ss);

    ss->cTimeout = timeout;
    rv = (PRStatus)(*ss->ops->connect)(ss, sockaddr);
#ifdef _WIN32
    PR_Sleep(PR_INTERVAL_NO_WAIT);     /* workaround NT winsock connect bug. */
#endif

    SSL_UNLOCK_WRITER(ss);
    SSL_UNLOCK_READER(ss);

    return rv;
}

static PRStatus PR_CALLBACK
ssl_Bind(PRFileDesc *fd, const PRNetAddr *addr)
{
    sslSocket * ss = ssl_GetPrivate(fd);
    PRStatus    rv;

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in bind", SSL_GETPID(), fd));
	return PR_FAILURE;
    }
    SSL_LOCK_READER(ss);
    SSL_LOCK_WRITER(ss);

    rv = (PRStatus)(*ss->ops->bind)(ss, addr);

    SSL_UNLOCK_WRITER(ss);
    SSL_UNLOCK_READER(ss);
    return rv;
}

static PRStatus PR_CALLBACK
ssl_Listen(PRFileDesc *fd, PRIntn backlog)
{
    sslSocket * ss = ssl_GetPrivate(fd);
    PRStatus    rv;

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in listen", SSL_GETPID(), fd));
	return PR_FAILURE;
    }
    SSL_LOCK_READER(ss);
    SSL_LOCK_WRITER(ss);

    rv = (PRStatus)(*ss->ops->listen)(ss, backlog);

    SSL_UNLOCK_WRITER(ss);
    SSL_UNLOCK_READER(ss);
    return rv;
}

static PRStatus PR_CALLBACK
ssl_Shutdown(PRFileDesc *fd, PRIntn how)
{
    sslSocket * ss = ssl_GetPrivate(fd);
    PRStatus    rv;

    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in shutdown", SSL_GETPID(), fd));
	return PR_FAILURE;
    }
    if (how == PR_SHUTDOWN_RCV || how == PR_SHUTDOWN_BOTH) {
    	SSL_LOCK_READER(ss);
    }
    if (how == PR_SHUTDOWN_SEND || how == PR_SHUTDOWN_BOTH) {
    	SSL_LOCK_WRITER(ss);
    }

    rv = (PRStatus)(*ss->ops->shutdown)(ss, how);

    if (how == PR_SHUTDOWN_SEND || how == PR_SHUTDOWN_BOTH) {
    	SSL_UNLOCK_WRITER(ss);
    }
    if (how == PR_SHUTDOWN_RCV || how == PR_SHUTDOWN_BOTH) {
    	SSL_UNLOCK_READER(ss);
    }
    return rv;
}

static PRStatus PR_CALLBACK
ssl_Close(PRFileDesc *fd)
{
    sslSocket *ss;
    PRStatus   rv;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in close", SSL_GETPID(), fd));
	return PR_FAILURE;
    }

    /* There must not be any I/O going on */
    SSL_LOCK_READER(ss);
    SSL_LOCK_WRITER(ss);

    /* By the time this function returns, 
    ** ss is an invalid pointer, and the locks to which it points have 
    ** been unlocked and freed.  So, this is the ONE PLACE in all of SSL
    ** where the LOCK calls and the corresponding UNLOCK calls are not in
    ** the same function scope.  The unlock calls are in ssl_FreeSocket().
    */
    rv = (PRStatus)(*ss->ops->close)(ss);

    return rv;
}

static int PR_CALLBACK
ssl_Recv(PRFileDesc *fd, void *buf, PRInt32 len, PRIntn flags,
	 PRIntervalTime timeout)
{
    sslSocket *ss;
    int        rv;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in recv", SSL_GETPID(), fd));
	return SECFailure;
    }
    SSL_LOCK_READER(ss);
    ss->rTimeout = timeout;
    if (!ss->opt.fdx)
	ss->wTimeout = timeout;
    rv = (*ss->ops->recv)(ss, (unsigned char*)buf, len, flags);
    SSL_UNLOCK_READER(ss);
    return rv;
}

static int PR_CALLBACK
ssl_Send(PRFileDesc *fd, const void *buf, PRInt32 len, PRIntn flags,
	 PRIntervalTime timeout)
{
    sslSocket *ss;
    int        rv;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in send", SSL_GETPID(), fd));
	return SECFailure;
    }
    SSL_LOCK_WRITER(ss);
    ss->wTimeout = timeout;
    if (!ss->opt.fdx)
	ss->rTimeout = timeout;
    rv = (*ss->ops->send)(ss, (const unsigned char*)buf, len, flags);
    SSL_UNLOCK_WRITER(ss);
    return rv;
}

static int PR_CALLBACK
ssl_Read(PRFileDesc *fd, void *buf, PRInt32 len)
{
    sslSocket *ss;
    int        rv;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in read", SSL_GETPID(), fd));
	return SECFailure;
    }
    SSL_LOCK_READER(ss);
    ss->rTimeout = PR_INTERVAL_NO_TIMEOUT;
    if (!ss->opt.fdx)
	ss->wTimeout = PR_INTERVAL_NO_TIMEOUT;
    rv = (*ss->ops->read)(ss, (unsigned char*)buf, len);
    SSL_UNLOCK_READER(ss);
    return rv;
}

static int PR_CALLBACK
ssl_Write(PRFileDesc *fd, const void *buf, PRInt32 len)
{
    sslSocket *ss;
    int        rv;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in write", SSL_GETPID(), fd));
	return SECFailure;
    }
    SSL_LOCK_WRITER(ss);
    ss->wTimeout = PR_INTERVAL_NO_TIMEOUT;
    if (!ss->opt.fdx)
	ss->rTimeout = PR_INTERVAL_NO_TIMEOUT;
    rv = (*ss->ops->write)(ss, (const unsigned char*)buf, len);
    SSL_UNLOCK_WRITER(ss);
    return rv;
}

static PRStatus PR_CALLBACK
ssl_GetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    sslSocket *ss;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in getpeername", SSL_GETPID(), fd));
	return PR_FAILURE;
    }
    return (PRStatus)(*ss->ops->getpeername)(ss, addr);
}

/*
*/
SECStatus
ssl_GetPeerInfo(sslSocket *ss)
{
    PRFileDesc *      osfd;
    int               rv;
    PRNetAddr         sin;

    osfd = ss->fd->lower;

    PORT_Memset(&sin, 0, sizeof(sin));
    rv = osfd->methods->getpeername(osfd, &sin);
    if (rv < 0) {
	return SECFailure;
    }
    ss->TCPconnected = 1;
    if (sin.inet.family == PR_AF_INET) {
        PR_ConvertIPv4AddrToIPv6(sin.inet.ip, &ss->sec.ci.peer);
	ss->sec.ci.port = sin.inet.port;
    } else if (sin.ipv6.family == PR_AF_INET6) {
	ss->sec.ci.peer = sin.ipv6.ip;
	ss->sec.ci.port = sin.ipv6.port;
    } else {
	PORT_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR);
    	return SECFailure;
    }
    return SECSuccess;
}

static PRStatus PR_CALLBACK
ssl_GetSockName(PRFileDesc *fd, PRNetAddr *name)
{
    sslSocket *ss;

    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in getsockname", SSL_GETPID(), fd));
	return PR_FAILURE;
    }
    return (PRStatus)(*ss->ops->getsockname)(ss, name);
}

SECStatus
SSL_SetStapledOCSPResponses(PRFileDesc *fd, SECItemArray *responses,
			    PRBool takeOwnership)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetStapledOCSPResponses",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if (ss->certStatusArray) {
        SECITEM_FreeArray(ss->certStatusArray, PR_TRUE);
        ss->certStatusArray = NULL;
    }
    if (responses) {
	if (takeOwnership) {
	    ss->certStatusArray = responses;
	}
	else {
	    ss->certStatusArray = SECITEM_DupArray(NULL, responses);
	}
    }
    return (ss->certStatusArray || !responses) ? SECSuccess : SECFailure;
}

SECStatus
SSL_SetSockPeerID(PRFileDesc *fd, const char *peerID)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetSockPeerID",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if (ss->peerID) {
    	PORT_Free(ss->peerID);
	ss->peerID = NULL;
    }
    if (peerID)
	ss->peerID = PORT_Strdup(peerID);
    return (ss->peerID || !peerID) ? SECSuccess : SECFailure;
}

#define PR_POLL_RW (PR_POLL_WRITE | PR_POLL_READ)

static PRInt16 PR_CALLBACK
ssl_Poll(PRFileDesc *fd, PRInt16 how_flags, PRInt16 *p_out_flags)
{
    sslSocket *ss;
    PRInt16    new_flags = how_flags;	/* should select on these flags. */
    PRNetAddr  addr;

    *p_out_flags = 0;
    ss = ssl_GetPrivate(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_Poll",
		 SSL_GETPID(), fd));
	return 0;	/* don't poll on this socket */
    }

    if (ss->opt.useSecurity && 
	ss->handshaking != sslHandshakingUndetermined &&
        !ss->firstHsDone &&
	(how_flags & PR_POLL_RW)) {
	if (!ss->TCPconnected) {
	    ss->TCPconnected = (PR_SUCCESS == ssl_DefGetpeername(ss, &addr));
	}
	/* If it's not connected, then presumably the application is polling
	** on read or write appropriately, so don't change it. 
	*/
	if (ss->TCPconnected) {
	    if (!ss->handshakeBegun) {
		/* If the handshake has not begun, poll on read or write 
		** based on the local application's role in the handshake,
		** not based on what the application requested.
		*/
		new_flags &= ~PR_POLL_RW;
		if (ss->handshaking == sslHandshakingAsClient) {
		    new_flags |= PR_POLL_WRITE;
		} else { /* handshaking as server */
		    new_flags |= PR_POLL_READ;
		}
	    } else 
	    /* First handshake is in progress */
	    if (ss->lastWriteBlocked) {
		if (new_flags & PR_POLL_READ) {
		    /* The caller is waiting for data to be received, 
		    ** but the initial handshake is blocked on write, or the 
		    ** client's first handshake record has not been written.
		    ** The code should select on write, not read.
		    */
		    new_flags ^=  PR_POLL_READ;	   /* don't select on read. */
		    new_flags |=  PR_POLL_WRITE;   /* do    select on write. */
		}
	    } else if (new_flags & PR_POLL_WRITE) {
		    /* The caller is trying to write, but the handshake is 
		    ** blocked waiting for data to read, and the first 
		    ** handshake has been sent.  so do NOT to poll on write.
		    */
		    new_flags ^=  PR_POLL_WRITE;   /* don't select on write. */
		    new_flags |=  PR_POLL_READ;	   /* do    select on read. */
	    }
	}
    } else if ((new_flags & PR_POLL_READ) && (SSL_DataPending(fd) > 0)) {
	*p_out_flags = PR_POLL_READ;	/* it's ready already. */
	return new_flags;
    } else if ((ss->lastWriteBlocked) && (how_flags & PR_POLL_READ) &&
	       (ss->pendingBuf.len != 0)) { /* write data waiting to be sent */
	new_flags |=  PR_POLL_WRITE;   /* also select on write. */
    }

    if (ss->version >= SSL_LIBRARY_VERSION_3_0 &&
	ss->ssl3.hs.restartTarget != NULL) {
	/* Read and write will block until the asynchronous callback completes
	 * (e.g. until SSL_AuthCertificateComplete is called), so don't tell
	 * the caller to poll the socket unless there is pending write data.
	 */
	if (ss->lastWriteBlocked && ss->pendingBuf.len != 0) {
	    /* Ignore any newly-received data on the socket, but do wait for
	     * the socket to become writable again. Here, it is OK for an error
	     * to be detected, because our logic for sending pending write data
	     * will allow us to report the error to the caller without the risk
	     * of the application spinning.
	     */
	    new_flags &= (PR_POLL_WRITE | PR_POLL_EXCEPT);
	} else {
	    /* Unfortunately, clearing new_flags will make it impossible for
	     * the application to detect errors that it would otherwise be
	     * able to detect with PR_POLL_EXCEPT, until the asynchronous
	     * callback completes. However, we must clear all the flags to
	     * prevent the application from spinning (alternating between
	     * calling PR_Poll that would return PR_POLL_EXCEPT, and send/recv
	     * which won't actually report the I/O error while we are waiting
	     * for the asynchronous callback to complete).
	     */
	    new_flags = 0;
	}
    }

    if (new_flags && (fd->lower->methods->poll != NULL)) {
	PRInt16    lower_out_flags = 0;
	PRInt16    lower_new_flags;
        lower_new_flags = fd->lower->methods->poll(fd->lower, new_flags, 
					           &lower_out_flags);
	if ((lower_new_flags & lower_out_flags) && (how_flags != new_flags)) {
	    PRInt16 out_flags = lower_out_flags & ~PR_POLL_RW;
	    if (lower_out_flags & PR_POLL_READ) 
		out_flags |= PR_POLL_WRITE;
	    if (lower_out_flags & PR_POLL_WRITE) 
		out_flags |= PR_POLL_READ;
	    *p_out_flags = out_flags;
	    new_flags = how_flags;
	} else {
	    *p_out_flags = lower_out_flags;
	    new_flags    = lower_new_flags;
	}
    }

    return new_flags;
}

static PRInt32 PR_CALLBACK
ssl_TransmitFile(PRFileDesc *sd, PRFileDesc *fd,
		 const void *headers, PRInt32 hlen,
		 PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    PRSendFileData sfd;

    sfd.fd = fd;
    sfd.file_offset = 0;
    sfd.file_nbytes = 0;
    sfd.header = headers;
    sfd.hlen = hlen;
    sfd.trailer = NULL;
    sfd.tlen = 0;

    return sd->methods->sendfile(sd, &sfd, flags, timeout);
}


PRBool
ssl_FdIsBlocking(PRFileDesc *fd)
{
    PRSocketOptionData opt;
    PRStatus           status;

    opt.option             = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    status = PR_GetSocketOption(fd, &opt);
    if (status != PR_SUCCESS)
	return PR_FALSE;
    return (PRBool)!opt.value.non_blocking;
}

PRBool
ssl_SocketIsBlocking(sslSocket *ss)
{
    return ssl_FdIsBlocking(ss->fd);
}

PRInt32  sslFirstBufSize = 8 * 1024;
PRInt32  sslCopyLimit    = 1024;

static PRInt32 PR_CALLBACK
ssl_WriteV(PRFileDesc *fd, const PRIOVec *iov, PRInt32 vectors, 
           PRIntervalTime timeout)
{
    PRInt32            bufLen;
    PRInt32            left;
    PRInt32            rv;
    PRInt32            sent      =  0;
    const PRInt32      first_len = sslFirstBufSize;
    const PRInt32      limit     = sslCopyLimit;
    PRBool             blocking;
    PRIOVec            myIov	 = { 0, 0 };
    char               buf[MAX_FRAGMENT_LENGTH];

    if (vectors > PR_MAX_IOVECTOR_SIZE) {
    	PORT_SetError(PR_BUFFER_OVERFLOW_ERROR);
	return -1;
    }
    blocking = ssl_FdIsBlocking(fd);

#define K16 sizeof(buf)
#define KILL_VECTORS while (vectors && !iov->iov_len) { ++iov; --vectors; }
#define GET_VECTOR   do { myIov = *iov++; --vectors; KILL_VECTORS } while (0)
#define HANDLE_ERR(rv, len) \
    if (rv != len) { \
	if (rv < 0) { \
	    if (!blocking \
		&& (PR_GetError() == PR_WOULD_BLOCK_ERROR) \
		&& (sent > 0)) { \
		return sent; \
	    } else { \
		return -1; \
	    } \
	} \
	/* Only a nonblocking socket can have partial sends */ \
	PR_ASSERT(!blocking); \
	return sent + rv; \
    } 
#define SEND(bfr, len) \
    do { \
	rv = ssl_Send(fd, bfr, len, 0, timeout); \
	HANDLE_ERR(rv, len) \
	sent += len; \
    } while (0)

    /* Make sure the first write is at least 8 KB, if possible. */
    KILL_VECTORS
    if (!vectors)
	return ssl_Send(fd, 0, 0, 0, timeout);
    GET_VECTOR;
    if (!vectors) {
	return ssl_Send(fd, myIov.iov_base, myIov.iov_len, 0, timeout);
    }
    if (myIov.iov_len < first_len) {
	PORT_Memcpy(buf, myIov.iov_base, myIov.iov_len);
	bufLen = myIov.iov_len;
	left = first_len - bufLen;
	while (vectors && left) {
	    int toCopy;
	    GET_VECTOR;
	    toCopy = PR_MIN(left, myIov.iov_len);
	    PORT_Memcpy(buf + bufLen, myIov.iov_base, toCopy);
	    bufLen         += toCopy;
	    left           -= toCopy;
	    myIov.iov_base += toCopy;
	    myIov.iov_len  -= toCopy;
	}
	SEND( buf, bufLen );
    }

    while (vectors || myIov.iov_len) {
	PRInt32   addLen;
	if (!myIov.iov_len) {
	    GET_VECTOR;
	}
	while (myIov.iov_len >= K16) {
	    SEND(myIov.iov_base, K16);
	    myIov.iov_base += K16;
	    myIov.iov_len  -= K16;
	}
	if (!myIov.iov_len)
	    continue;

	if (!vectors || myIov.iov_len > limit) {
	    addLen = 0;
	} else if ((addLen = iov->iov_len % K16) + myIov.iov_len <= limit) {
	    /* Addlen is already computed. */;
	} else if (vectors > 1 && 
	     iov[1].iov_len % K16 + addLen + myIov.iov_len <= 2 * limit) {
	     addLen = limit - myIov.iov_len;
	} else 
	    addLen = 0;

	if (!addLen) {
	    SEND( myIov.iov_base, myIov.iov_len );
	    myIov.iov_len = 0;
	    continue;
	}
	PORT_Memcpy(buf, myIov.iov_base, myIov.iov_len);
	bufLen = myIov.iov_len;
	do {
	    GET_VECTOR;
	    PORT_Memcpy(buf + bufLen, myIov.iov_base, addLen);
	    myIov.iov_base += addLen;
	    myIov.iov_len  -= addLen;
	    bufLen         += addLen;

	    left = PR_MIN( limit, K16 - bufLen);
	    if (!vectors 		/* no more left */
	    ||  myIov.iov_len > 0	/* we didn't use that one all up */
	    ||  bufLen >= K16		/* it's full. */
	    ) {
		addLen = 0;
	    } else if ((addLen = iov->iov_len % K16) <= left) {
		/* Addlen is already computed. */;
	    } else if (vectors > 1 && 
		 iov[1].iov_len % K16 + addLen <= left + limit) {
		 addLen = left;
	    } else 
		addLen = 0;

	} while (addLen);
	SEND( buf, bufLen );
    } 
    return sent;
}

/*
 * These functions aren't implemented.
 */

static PRInt32 PR_CALLBACK
ssl_Available(PRFileDesc *fd)
{
    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return SECFailure;
}

static PRInt64 PR_CALLBACK
ssl_Available64(PRFileDesc *fd)
{
    PRInt64 res;

    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    LL_I2L(res, -1L);
    return res;
}

static PRStatus PR_CALLBACK
ssl_FSync(PRFileDesc *fd)
{
    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

static PRInt32 PR_CALLBACK
ssl_Seek(PRFileDesc *fd, PRInt32 offset, PRSeekWhence how) {
    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return SECFailure;
}

static PRInt64 PR_CALLBACK
ssl_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence how) {
    PRInt64 res;

    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    LL_I2L(res, -1L);
    return res;
}

static PRStatus PR_CALLBACK
ssl_FileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus PR_CALLBACK
ssl_FileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

static PRInt32 PR_CALLBACK
ssl_RecvFrom(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
	     PRNetAddr *addr, PRIntervalTime timeout)
{
    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return SECFailure;
}

static PRInt32 PR_CALLBACK
ssl_SendTo(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
	   const PRNetAddr *addr, PRIntervalTime timeout)
{
    PORT_Assert(0);
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return SECFailure;
}

static const PRIOMethods ssl_methods = {
    PR_DESC_LAYERED,
    ssl_Close,           	/* close        */
    ssl_Read,            	/* read         */
    ssl_Write,           	/* write        */
    ssl_Available,       	/* available    */
    ssl_Available64,     	/* available64  */
    ssl_FSync,           	/* fsync        */
    ssl_Seek,            	/* seek         */
    ssl_Seek64,          	/* seek64       */
    ssl_FileInfo,        	/* fileInfo     */
    ssl_FileInfo64,      	/* fileInfo64   */
    ssl_WriteV,          	/* writev       */
    ssl_Connect,         	/* connect      */
    ssl_Accept,          	/* accept       */
    ssl_Bind,            	/* bind         */
    ssl_Listen,          	/* listen       */
    ssl_Shutdown,        	/* shutdown     */
    ssl_Recv,            	/* recv         */
    ssl_Send,            	/* send         */
    ssl_RecvFrom,        	/* recvfrom     */
    ssl_SendTo,          	/* sendto       */
    ssl_Poll,            	/* poll         */
    PR_EmulateAcceptRead,       /* acceptread   */
    ssl_TransmitFile,           /* transmitfile */
    ssl_GetSockName,     	/* getsockname  */
    ssl_GetPeerName,     	/* getpeername  */
    NULL,                	/* getsockopt   OBSOLETE */
    NULL,                	/* setsockopt   OBSOLETE */
    NULL,                	/* getsocketoption   */
    NULL,                	/* setsocketoption   */
    PR_EmulateSendFile, 	/* Send a (partial) file with header/trailer*/
    NULL,                	/* reserved for future use */
    NULL,                	/* reserved for future use */
    NULL,                	/* reserved for future use */
    NULL,                	/* reserved for future use */
    NULL                 	/* reserved for future use */
};


static PRIOMethods combined_methods;

static void
ssl_SetupIOMethods(void)
{
          PRIOMethods *new_methods  = &combined_methods;
    const PRIOMethods *nspr_methods = PR_GetDefaultIOMethods();
    const PRIOMethods *my_methods   = &ssl_methods;

    *new_methods = *nspr_methods;

    new_methods->file_type         = my_methods->file_type;
    new_methods->close             = my_methods->close;
    new_methods->read              = my_methods->read;
    new_methods->write             = my_methods->write;
    new_methods->available         = my_methods->available;
    new_methods->available64       = my_methods->available64;
    new_methods->fsync             = my_methods->fsync;
    new_methods->seek              = my_methods->seek;
    new_methods->seek64            = my_methods->seek64;
    new_methods->fileInfo          = my_methods->fileInfo;
    new_methods->fileInfo64        = my_methods->fileInfo64;
    new_methods->writev            = my_methods->writev;
    new_methods->connect           = my_methods->connect;
    new_methods->accept            = my_methods->accept;
    new_methods->bind              = my_methods->bind;
    new_methods->listen            = my_methods->listen;
    new_methods->shutdown          = my_methods->shutdown;
    new_methods->recv              = my_methods->recv;
    new_methods->send              = my_methods->send;
    new_methods->recvfrom          = my_methods->recvfrom;
    new_methods->sendto            = my_methods->sendto;
    new_methods->poll              = my_methods->poll;
    new_methods->acceptread        = my_methods->acceptread;
    new_methods->transmitfile      = my_methods->transmitfile;
    new_methods->getsockname       = my_methods->getsockname;
    new_methods->getpeername       = my_methods->getpeername;
/*  new_methods->getsocketoption   = my_methods->getsocketoption;	*/
/*  new_methods->setsocketoption   = my_methods->setsocketoption;	*/
    new_methods->sendfile          = my_methods->sendfile;

}

static PRCallOnceType initIoLayerOnce;

static PRStatus  
ssl_InitIOLayer(void)
{
    ssl_layer_id = PR_GetUniqueIdentity("SSL");
    ssl_SetupIOMethods();
    ssl_inited = PR_TRUE;
    return PR_SUCCESS;
}

static PRStatus
ssl_PushIOLayer(sslSocket *ns, PRFileDesc *stack, PRDescIdentity id)
{
    PRFileDesc *layer	= NULL;
    PRStatus    status;

    if (!ssl_inited) {
	status = PR_CallOnce(&initIoLayerOnce, &ssl_InitIOLayer);
	if (status != PR_SUCCESS)
	    goto loser;
    }

    if (ns == NULL)
	goto loser;

    layer = PR_CreateIOLayerStub(ssl_layer_id, &combined_methods);
    if (layer == NULL)
	goto loser;
    layer->secret = (PRFilePrivate *)ns;

    /* Here, "stack" points to the PRFileDesc on the top of the stack.
    ** "layer" points to a new FD that is to be inserted into the stack.
    ** If layer is being pushed onto the top of the stack, then 
    ** PR_PushIOLayer switches the contents of stack and layer, and then
    ** puts stack on top of layer, so that after it is done, the top of 
    ** stack is the same "stack" as it was before, and layer is now the 
    ** FD for the former top of stack.
    ** After this call, stack always points to the top PRFD on the stack.
    ** If this function fails, the contents of stack and layer are as 
    ** they were before the call.
    */
    status = PR_PushIOLayer(stack, id, layer);
    if (status != PR_SUCCESS)
	goto loser;

    ns->fd = (id == PR_TOP_IO_LAYER) ? stack : layer;
    return PR_SUCCESS;

loser:
    if (layer) {
	layer->dtor(layer); /* free layer */
    }
    return PR_FAILURE;
}

/* if this fails, caller must destroy socket. */
static SECStatus
ssl_MakeLocks(sslSocket *ss)
{
    ss->firstHandshakeLock = PZ_NewMonitor(nssILockSSL);
    if (!ss->firstHandshakeLock) 
	goto loser;
    ss->ssl3HandshakeLock  = PZ_NewMonitor(nssILockSSL);
    if (!ss->ssl3HandshakeLock) 
	goto loser;
    ss->specLock           = NSSRWLock_New(SSL_LOCK_RANK_SPEC, NULL);
    if (!ss->specLock) 
	goto loser;
    ss->recvBufLock        = PZ_NewMonitor(nssILockSSL);
    if (!ss->recvBufLock) 
	goto loser;
    ss->xmitBufLock        = PZ_NewMonitor(nssILockSSL);
    if (!ss->xmitBufLock) 
	goto loser;
    ss->writerThread       = NULL;
    if (ssl_lock_readers) {
	ss->recvLock       = PZ_NewLock(nssILockSSL);
	if (!ss->recvLock) 
	    goto loser;
	ss->sendLock       = PZ_NewLock(nssILockSSL);
	if (!ss->sendLock) 
	    goto loser;
    }
    return SECSuccess;
loser:
    ssl_DestroyLocks(ss);
    return SECFailure;
}

#if defined(XP_UNIX) || defined(XP_WIN32) || defined(XP_BEOS)
#define NSS_HAVE_GETENV 1
#endif

#define LOWER(x) (x | 0x20)  /* cheap ToLower function ignores LOCALE */

static void
ssl_SetDefaultsFromEnvironment(void)
{
#if defined( NSS_HAVE_GETENV )
    static int firsttime = 1;

    if (firsttime) {
	char * ev;
	firsttime = 0;
#ifdef DEBUG
	ev = getenv("SSLDEBUGFILE");
	if (ev && ev[0]) {
	    ssl_trace_iob = fopen(ev, "w");
	}
	if (!ssl_trace_iob) {
	    ssl_trace_iob = stderr;
	}
#ifdef TRACE
	ev = getenv("SSLTRACE");
	if (ev && ev[0]) {
	    ssl_trace = atoi(ev);
	    SSL_TRACE(("SSL: tracing set to %d", ssl_trace));
	}
#endif /* TRACE */
	ev = getenv("SSLDEBUG");
	if (ev && ev[0]) {
	    ssl_debug = atoi(ev);
	    SSL_TRACE(("SSL: debugging set to %d", ssl_debug));
	}
#endif /* DEBUG */
	ev = getenv("SSLKEYLOGFILE");
	if (ev && ev[0]) {
	    ssl_keylog_iob = fopen(ev, "a");
	    if (!ssl_keylog_iob) {
		SSL_TRACE(("SSL: failed to open key log file"));
	    } else {
		if (ftell(ssl_keylog_iob) == 0) {
		    fputs("# SSL/TLS secrets log file, generated by NSS\n",
			  ssl_keylog_iob);
		}
		SSL_TRACE(("SSL: logging SSL/TLS secrets to %s", ev));
	    }
	}
#ifndef NO_PKCS11_BYPASS
	ev = getenv("SSLBYPASS");
	if (ev && ev[0]) {
	    ssl_defaults.bypassPKCS11 = (ev[0] == '1');
	    SSL_TRACE(("SSL: bypass default set to %d", \
		      ssl_defaults.bypassPKCS11));
	}
#endif /* NO_PKCS11_BYPASS */
	ev = getenv("SSLFORCELOCKS");
	if (ev && ev[0] == '1') {
	    ssl_force_locks = PR_TRUE;
	    ssl_defaults.noLocks = 0;
	    strcpy(lockStatus + LOCKSTATUS_OFFSET, "FORCED.  ");
	    SSL_TRACE(("SSL: force_locks set to %d", ssl_force_locks));
	}
	ev = getenv("NSS_SSL_ENABLE_RENEGOTIATION");
	if (ev) {
	    if (ev[0] == '1' || LOWER(ev[0]) == 'u')
	    	ssl_defaults.enableRenegotiation = SSL_RENEGOTIATE_UNRESTRICTED;
	    else if (ev[0] == '0' || LOWER(ev[0]) == 'n')
	    	ssl_defaults.enableRenegotiation = SSL_RENEGOTIATE_NEVER;
	    else if (ev[0] == '2' || LOWER(ev[0]) == 'r')
		ssl_defaults.enableRenegotiation = SSL_RENEGOTIATE_REQUIRES_XTN;
	    else if (ev[0] == '3' || LOWER(ev[0]) == 't')
	    	ssl_defaults.enableRenegotiation = SSL_RENEGOTIATE_TRANSITIONAL;
	    SSL_TRACE(("SSL: enableRenegotiation set to %d", 
	               ssl_defaults.enableRenegotiation));
	}
	ev = getenv("NSS_SSL_REQUIRE_SAFE_NEGOTIATION");
	if (ev && ev[0] == '1') {
	    ssl_defaults.requireSafeNegotiation = PR_TRUE;
	    SSL_TRACE(("SSL: requireSafeNegotiation set to %d", 
	                PR_TRUE));
	}
	ev = getenv("NSS_SSL_CBC_RANDOM_IV");
	if (ev && ev[0] == '0') {
	    ssl_defaults.cbcRandomIV = PR_FALSE;
	    SSL_TRACE(("SSL: cbcRandomIV set to 0"));
	}
    }
#endif /* NSS_HAVE_GETENV */
}

/*
** Create a newsocket structure for a file descriptor.
*/
static sslSocket *
ssl_NewSocket(PRBool makeLocks, SSLProtocolVariant protocolVariant)
{
    sslSocket *ss;

    ssl_SetDefaultsFromEnvironment();

    if (ssl_force_locks)
	makeLocks = PR_TRUE;

    /* Make a new socket and get it ready */
    ss = (sslSocket*) PORT_ZAlloc(sizeof(sslSocket));
    if (ss) {
        /* This should be of type SSLKEAType, but CC on IRIX
	 * complains during the for loop.
	 */
	int i;
	SECStatus status;
 
	ss->opt                = ssl_defaults;
	ss->opt.useSocks       = PR_FALSE;
	ss->opt.noLocks        = !makeLocks;
	ss->vrange             = *VERSIONS_DEFAULTS(protocolVariant);

	ss->peerID             = NULL;
	ss->rTimeout	       = PR_INTERVAL_NO_TIMEOUT;
	ss->wTimeout	       = PR_INTERVAL_NO_TIMEOUT;
	ss->cTimeout	       = PR_INTERVAL_NO_TIMEOUT;
	ss->cipherSpecs        = NULL;
        ss->sizeCipherSpecs    = 0;  /* produced lazily */
        ss->preferredCipher    = NULL;
        ss->url                = NULL;

	for (i=kt_null; i < kt_kea_size; i++) {
	    sslServerCerts * sc = ss->serverCerts + i;
	    sc->serverCert      = NULL;
	    sc->serverCertChain = NULL;
	    sc->serverKeyPair   = NULL;
	    sc->serverKeyBits   = 0;
	}
	ss->stepDownKeyPair    = NULL;
	ss->dbHandle           = CERT_GetDefaultCertDB();
	ss->certStatusArray    = NULL;

	/* Provide default implementation of hooks */
	ss->authCertificate    = SSL_AuthCertificate;
	ss->authCertificateArg = (void *)ss->dbHandle;
        ss->sniSocketConfig    = NULL;
        ss->sniSocketConfigArg = NULL;
	ss->getClientAuthData  = NULL;
	ss->handleBadCert      = NULL;
	ss->badCertArg         = NULL;
	ss->pkcs11PinArg       = NULL;
	ss->ephemeralECDHKeyPair = NULL;

	ssl_ChooseOps(ss);
	ssl2_InitSocketPolicy(ss);
	ssl3_InitSocketPolicy(ss);
	PR_INIT_CLIST(&ss->ssl3.hs.lastMessageFlight);

	if (makeLocks) {
	    status = ssl_MakeLocks(ss);
	    if (status != SECSuccess)
		goto loser;
	}
	status = ssl_CreateSecurityInfo(ss);
	if (status != SECSuccess) 
	    goto loser;
	status = ssl_InitGather(&ss->gs);
	if (status != SECSuccess) {
loser:
	    ssl_DestroySocketContents(ss);
	    ssl_DestroyLocks(ss);
	    PORT_Free(ss);
	    ss = NULL;
	}
	ss->protocolVariant = protocolVariant;
    }
    return ss;
}

