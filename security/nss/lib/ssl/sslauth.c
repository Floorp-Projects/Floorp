/*
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
 * $Id: sslauth.c,v 1.2 2000/09/12 20:15:42 jgmyers%netscape.com Exp $
 */
#include "cert.h"
#include "secitem.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "pk11func.h"

/* NEED LOCKS IN HERE.  */
CERTCertificate *SSL_PeerCertificate(PRFileDesc *fd)
{
    sslSocket *ss;
    sslSecurityInfo *sec;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in PeerCertificate",
		 SSL_GETPID(), fd));
	return 0;
    }
    sec = ss->sec;
    if (ss->useSecurity && sec && sec->peerCert) {
	return CERT_DupCertificate(sec->peerCert);
    }
    return 0;
}

/* NEED LOCKS IN HERE.  */
int
SSL_SecurityStatus(PRFileDesc *fd, int *op, char **cp, int *kp0, int *kp1,
		   char **ip, char **sp)
{
    sslSocket *ss;
    sslSecurityInfo *sec;
    const char *cipherName;
    PRBool isDes = PR_FALSE;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SecurityStatus",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if (cp) *cp = 0;
    if (kp0) *kp0 = 0;
    if (kp1) *kp1 = 0;
    if (ip) *ip = 0;
    if (sp) *sp = 0;
    if (op) {
	*op = SSL_SECURITY_STATUS_OFF;
    }

    if (ss->useSecurity && ss->connected) {
	PORT_Assert(ss->sec != 0);
	sec = ss->sec;

	if (ss->version < SSL_LIBRARY_VERSION_3_0) {
	    cipherName = ssl_cipherName[sec->cipherType];
	} else {
	    cipherName = ssl3_cipherName[sec->cipherType];
	}
	if (cipherName && PORT_Strstr(cipherName, "DES")) isDes = PR_TRUE;
	/* do same key stuff for fortezza */
    
	if (cp) {
	    *cp = PORT_Strdup(cipherName);
	}

	if (kp0) {
	    *kp0 = sec->keyBits;
	    if (isDes) *kp0 = (*kp0 * 7) / 8;
	}
	if (kp1) {
	    *kp1 = sec->secretKeyBits;
	    if (isDes) *kp1 = (*kp1 * 7) / 8;
	}
	if (op) {
	    if (sec->keyBits == 0) {
		*op = SSL_SECURITY_STATUS_OFF;
	    } else if (sec->secretKeyBits < 90) {
		*op = SSL_SECURITY_STATUS_ON_LOW;

	    } else {
		*op = SSL_SECURITY_STATUS_ON_HIGH;
	    }
	}

	if (ip || sp) {
	    CERTCertificate *cert;

	    cert = sec->peerCert;
	    if (cert) {
		if (ip) {
		    *ip = CERT_NameToAscii(&cert->issuer);
		}
		if (sp) {
		    *sp = CERT_NameToAscii(&cert->subject);
		}
	    } else {
		if (ip) {
		    *ip = PORT_Strdup("no certificate");
		}
		if (sp) {
		    *sp = PORT_Strdup("no certificate");
		}
	    }
	}
    }

    return 0;
}

/************************************************************************/

/* NEED LOCKS IN HERE.  */
int
SSL_AuthCertificateHook(PRFileDesc *s, SSLAuthCertificate func, void *arg)
{
    sslSocket *ss;
    int rv;

    ss = ssl_FindSocket(s);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in AuthCertificateHook",
		 SSL_GETPID(), s));
	return SECFailure;
    }

    if ((rv = ssl_CreateSecurityInfo(ss)) != 0) {
	return(rv);
    }
    ss->authCertificate = func;
    ss->authCertificateArg = arg;

    return(0);
}

/* NEED LOCKS IN HERE.  */
int 
SSL_GetClientAuthDataHook(PRFileDesc *s, SSLGetClientAuthData func,
			      void *arg)
{
    sslSocket *ss;
    int rv;

    ss = ssl_FindSocket(s);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in GetClientAuthDataHook",
		 SSL_GETPID(), s));
	return SECFailure;
    }

    if ((rv = ssl_CreateSecurityInfo(ss)) != 0) {
	return rv;
    }
    ss->getClientAuthData = func;
    ss->getClientAuthDataArg = arg;
    return 0;
}

/* NEED LOCKS IN HERE.  */
int 
SSL_SetPKCS11PinArg(PRFileDesc *s, void *arg)
{
    sslSocket *ss;
    int rv;

    ss = ssl_FindSocket(s);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in GetClientAuthDataHook",
		 SSL_GETPID(), s));
	return SECFailure;
    }

    if ((rv = ssl_CreateSecurityInfo(ss)) != 0) {
	return rv;
    }
    ss->pkcs11PinArg = arg;
    return 0;
}


/* This is the "default" authCert callback function.  It is called when a 
 * certificate message is received from the peer and the local application
 * has not registered an authCert callback function.
 */
int
SSL_AuthCertificate(void *arg, PRFileDesc *fd, PRBool checkSig, PRBool isServer)
{
    SECStatus          rv;
    CERTCertDBHandle * handle;
    sslSocket *        ss;
    SECCertUsage       certUsage;
    const char *             hostname    = NULL;
    
    ss = ssl_FindSocket(fd);
    PORT_Assert(ss != NULL);

    handle = (CERTCertDBHandle *)arg;

    /* this may seem backwards, but isn't. */
    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;

    rv = CERT_VerifyCertNow(handle, ss->sec->peerCert, checkSig, certUsage,
			    ss->pkcs11PinArg);

    if ( rv != SECSuccess || isServer )
	return rv;
  
    /* cert is OK.  This is the client side of an SSL connection.
     * Now check the name field in the cert against the desired hostname.
     * NB: This is our only defense against Man-In-The-Middle (MITM) attacks!
     */
    hostname = ss->url;
    if (hostname && hostname[0])
	rv = CERT_VerifyCertName(ss->sec->peerCert, hostname);
    else 
	rv = SECFailure;
    if (rv != SECSuccess)
	PORT_SetError(SSL_ERROR_BAD_CERT_DOMAIN);

    return rv;
}
