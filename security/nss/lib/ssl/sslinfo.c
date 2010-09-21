/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
/* $Id: sslinfo.c,v 1.23.2.1 2010/09/02 01:13:46 wtc%google.com Exp $ */
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"

static const char *
ssl_GetCompressionMethodName(SSLCompressionMethod compression)
{
    switch (compression) {
    case ssl_compression_null:
	return "NULL";
#ifdef NSS_ENABLE_ZLIB
    case ssl_compression_deflate:
	return "DEFLATE";
#endif
    default:
	return "???";
    }
}

SECStatus 
SSL_GetChannelInfo(PRFileDesc *fd, SSLChannelInfo *info, PRUintn len)
{
    sslSocket *      ss;
    SSLChannelInfo   inf;
    sslSessionID *   sid;
    PRBool           enoughFirstHsDone = PR_FALSE;

    if (!info || len < sizeof inf.length) { 
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetChannelInfo",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    memset(&inf, 0, sizeof inf);
    inf.length = PR_MIN(sizeof inf, len);

    if (ss->firstHsDone) {
	enoughFirstHsDone = PR_TRUE;
    } else if (ss->version >= SSL_LIBRARY_VERSION_3_0 &&
	       ssl3_CanFalseStart(ss)) {
	enoughFirstHsDone = PR_TRUE;
    }

    if (ss->opt.useSecurity && enoughFirstHsDone) {
        sid = ss->sec.ci.sid;
	inf.protocolVersion  = ss->version;
	inf.authKeyBits      = ss->sec.authKeyBits;
	inf.keaKeyBits       = ss->sec.keaKeyBits;
	if (ss->version < SSL_LIBRARY_VERSION_3_0) { /* SSL2 */
	    inf.cipherSuite           = ss->sec.cipherType | 0xff00;
	    inf.compressionMethod     = ssl_compression_null;
	    inf.compressionMethodName = "N/A";
	} else if (ss->ssl3.initialized) { 	/* SSL3 and TLS */
	    ssl_GetSpecReadLock(ss);
	    /* XXX  The cipher suite should be in the specs and this
	     * function should get it from crSpec rather than from the "hs".
	     * See bug 275744 comment 69.
	     */
	    inf.cipherSuite           = ss->ssl3.hs.cipher_suite;
	    inf.compressionMethod     = ss->ssl3.crSpec->compression_method;
	    ssl_ReleaseSpecReadLock(ss);
	    inf.compressionMethodName =
		ssl_GetCompressionMethodName(inf.compressionMethod);
	}
	if (sid) {
	    inf.creationTime   = sid->creationTime;
	    inf.lastAccessTime = sid->lastAccessTime;
	    inf.expirationTime = sid->expirationTime;
	    if (ss->version < SSL_LIBRARY_VERSION_3_0) { /* SSL2 */
	        inf.sessionIDLength = SSL2_SESSIONID_BYTES;
		memcpy(inf.sessionID, sid->u.ssl2.sessionID, 
		       SSL2_SESSIONID_BYTES);
	    } else {
		unsigned int sidLen = sid->u.ssl3.sessionIDLength;
	        sidLen = PR_MIN(sidLen, sizeof inf.sessionID);
	        inf.sessionIDLength = sidLen;
		memcpy(inf.sessionID, sid->u.ssl3.sessionID, sidLen);
	    }
	}
    }

    memcpy(info, &inf, inf.length);

    return SECSuccess;
}


#define CS(x) x, #x
#define CK(x) x | 0xff00, #x

#define S_DSA   "DSA", ssl_auth_dsa
#define S_RSA	"RSA", ssl_auth_rsa
#define S_KEA   "KEA", ssl_auth_kea
#define S_ECDSA "ECDSA", ssl_auth_ecdsa

#define K_DHE	"DHE", kt_dh
#define K_RSA	"RSA", kt_rsa
#define K_KEA	"KEA", kt_kea
#define K_ECDH	"ECDH", kt_ecdh
#define K_ECDHE	"ECDHE", kt_ecdh

#define C_SEED 	"SEED", calg_seed
#define C_CAMELLIA	"CAMELLIA", calg_camellia
#define C_AES	"AES", calg_aes
#define C_RC4	"RC4", calg_rc4
#define C_RC2	"RC2", calg_rc2
#define C_DES	"DES", calg_des
#define C_3DES	"3DES", calg_3des
#define C_NULL  "NULL", calg_null
#define C_SJ 	"SKIPJACK", calg_sj

#define B_256	256, 256, 256
#define B_128	128, 128, 128
#define B_3DES  192, 156, 112
#define B_SJ     96,  80,  80
#define B_DES    64,  56,  56
#define B_56    128,  56,  56
#define B_40    128,  40,  40
#define B_0  	  0,   0,   0

#define M_SHA	"SHA1", ssl_mac_sha, 160
#define M_MD5	"MD5",  ssl_mac_md5, 128

static const SSLCipherSuiteInfo suiteInfo[] = {
/* <------ Cipher suite --------------------> <auth> <KEA>  <bulk cipher> <MAC> <FIPS> */
{0,CS(TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA), S_RSA, K_DHE, C_CAMELLIA, B_256, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA), S_DSA, K_DHE, C_CAMELLIA, B_256, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_RSA_WITH_AES_256_CBC_SHA),      S_RSA, K_DHE, C_AES, B_256, M_SHA, 1, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_AES_256_CBC_SHA),      S_DSA, K_DHE, C_AES, B_256, M_SHA, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_CAMELLIA_256_CBC_SHA),     S_RSA, K_RSA, C_CAMELLIA, B_256, M_SHA, 0, 0, 0, },
{0,CS(TLS_RSA_WITH_AES_256_CBC_SHA),          S_RSA, K_RSA, C_AES, B_256, M_SHA, 1, 0, 0, },

{0,CS(TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA), S_RSA, K_DHE, C_CAMELLIA, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA), S_DSA, K_DHE, C_CAMELLIA, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_RC4_128_SHA),          S_DSA, K_DHE, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_RSA_WITH_AES_128_CBC_SHA),      S_RSA, K_DHE, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_AES_128_CBC_SHA),      S_DSA, K_DHE, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_SEED_CBC_SHA),             S_RSA, K_RSA, C_SEED,B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_CAMELLIA_128_CBC_SHA),     S_RSA, K_RSA, C_CAMELLIA, B_128, M_SHA, 0, 0, 0, },
{0,CS(SSL_RSA_WITH_RC4_128_MD5),              S_RSA, K_RSA, C_RC4, B_128, M_MD5, 0, 0, 0, },
{0,CS(SSL_RSA_WITH_RC4_128_SHA),              S_RSA, K_RSA, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_RSA_WITH_AES_128_CBC_SHA),          S_RSA, K_RSA, C_AES, B_128, M_SHA, 1, 0, 0, },

{0,CS(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA),     S_RSA, K_DHE, C_3DES,B_3DES,M_SHA, 1, 0, 0, },
{0,CS(SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA),     S_DSA, K_DHE, C_3DES,B_3DES,M_SHA, 1, 0, 0, },
{0,CS(SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA),    S_RSA, K_RSA, C_3DES,B_3DES,M_SHA, 1, 0, 1, },
{0,CS(SSL_RSA_WITH_3DES_EDE_CBC_SHA),         S_RSA, K_RSA, C_3DES,B_3DES,M_SHA, 1, 0, 0, },

{0,CS(SSL_DHE_RSA_WITH_DES_CBC_SHA),          S_RSA, K_DHE, C_DES, B_DES, M_SHA, 0, 0, 0, },
{0,CS(SSL_DHE_DSS_WITH_DES_CBC_SHA),          S_DSA, K_DHE, C_DES, B_DES, M_SHA, 0, 0, 0, },
{0,CS(SSL_RSA_FIPS_WITH_DES_CBC_SHA),         S_RSA, K_RSA, C_DES, B_DES, M_SHA, 0, 0, 1, },
{0,CS(SSL_RSA_WITH_DES_CBC_SHA),              S_RSA, K_RSA, C_DES, B_DES, M_SHA, 0, 0, 0, },

{0,CS(TLS_RSA_EXPORT1024_WITH_RC4_56_SHA),    S_RSA, K_RSA, C_RC4, B_56,  M_SHA, 0, 1, 0, },
{0,CS(TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA),   S_RSA, K_RSA, C_DES, B_DES, M_SHA, 0, 1, 0, },
{0,CS(SSL_RSA_EXPORT_WITH_RC4_40_MD5),        S_RSA, K_RSA, C_RC4, B_40,  M_MD5, 0, 1, 0, },
{0,CS(SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5),    S_RSA, K_RSA, C_RC2, B_40,  M_MD5, 0, 1, 0, },
{0,CS(SSL_RSA_WITH_NULL_SHA),                 S_RSA, K_RSA, C_NULL,B_0,   M_SHA, 0, 1, 0, },
{0,CS(SSL_RSA_WITH_NULL_MD5),                 S_RSA, K_RSA, C_NULL,B_0,   M_MD5, 0, 1, 0, },

#ifdef NSS_ENABLE_ECC
/* ECC cipher suites */
{0,CS(TLS_ECDH_ECDSA_WITH_NULL_SHA),          S_ECDSA, K_ECDH, C_NULL, B_0, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_RC4_128_SHA),       S_ECDSA, K_ECDH, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA),  S_ECDSA, K_ECDH, C_3DES, B_3DES, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA),   S_ECDSA, K_ECDH, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA),   S_ECDSA, K_ECDH, C_AES, B_256, M_SHA, 1, 0, 0, },

{0,CS(TLS_ECDHE_ECDSA_WITH_NULL_SHA),         S_ECDSA, K_ECDHE, C_NULL, B_0, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA),      S_ECDSA, K_ECDHE, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA), S_ECDSA, K_ECDHE, C_3DES, B_3DES, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA),  S_ECDSA, K_ECDHE, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA),  S_ECDSA, K_ECDHE, C_AES, B_256, M_SHA, 1, 0, 0, },

{0,CS(TLS_ECDH_RSA_WITH_NULL_SHA),            S_RSA, K_ECDH, C_NULL, B_0, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDH_RSA_WITH_RC4_128_SHA),         S_RSA, K_ECDH, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA),    S_RSA, K_ECDH, C_3DES, B_3DES, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA),     S_RSA, K_ECDH, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA),     S_RSA, K_ECDH, C_AES, B_256, M_SHA, 1, 0, 0, },

{0,CS(TLS_ECDHE_RSA_WITH_NULL_SHA),           S_RSA, K_ECDHE, C_NULL, B_0, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDHE_RSA_WITH_RC4_128_SHA),        S_RSA, K_ECDHE, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA),   S_RSA, K_ECDHE, C_3DES, B_3DES, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA),    S_RSA, K_ECDHE, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA),    S_RSA, K_ECDHE, C_AES, B_256, M_SHA, 1, 0, 0, },
#endif /* NSS_ENABLE_ECC */

/* SSL 2 table */
{0,CK(SSL_CK_RC4_128_WITH_MD5),               S_RSA, K_RSA, C_RC4, B_128, M_MD5, 0, 0, 0, },
{0,CK(SSL_CK_RC2_128_CBC_WITH_MD5),           S_RSA, K_RSA, C_RC2, B_128, M_MD5, 0, 0, 0, },
{0,CK(SSL_CK_DES_192_EDE3_CBC_WITH_MD5),      S_RSA, K_RSA, C_3DES,B_3DES,M_MD5, 0, 0, 0, },
{0,CK(SSL_CK_DES_64_CBC_WITH_MD5),            S_RSA, K_RSA, C_DES, B_DES, M_MD5, 0, 0, 0, },
{0,CK(SSL_CK_RC4_128_EXPORT40_WITH_MD5),      S_RSA, K_RSA, C_RC4, B_40,  M_MD5, 0, 1, 0, },
{0,CK(SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5),  S_RSA, K_RSA, C_RC2, B_40,  M_MD5, 0, 1, 0, }
};

#define NUM_SUITEINFOS ((sizeof suiteInfo) / (sizeof suiteInfo[0]))


SECStatus SSL_GetCipherSuiteInfo(PRUint16 cipherSuite, 
                                 SSLCipherSuiteInfo *info, PRUintn len)
{
    unsigned int i;

    len = PR_MIN(len, sizeof suiteInfo[0]);
    if (!info || len < sizeof suiteInfo[0].length) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
    	return SECFailure;
    }
    for (i = 0; i < NUM_SUITEINFOS; i++) {
    	if (suiteInfo[i].cipherSuite == cipherSuite) {
	    memcpy(info, &suiteInfo[i], len);
	    info->length = len;
	    return SECSuccess;
	}
    }
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
}

/* This function might be a candidate to be public. 
 * Disables all export ciphers in the default set of enabled ciphers.
 */
SECStatus 
SSL_DisableDefaultExportCipherSuites(void)
{
    const SSLCipherSuiteInfo * pInfo = suiteInfo;
    unsigned int i;
    SECStatus rv;

    for (i = 0; i < NUM_SUITEINFOS; ++i, ++pInfo) {
    	if (pInfo->isExportable) {
	    rv = SSL_CipherPrefSetDefault(pInfo->cipherSuite, PR_FALSE);
	    PORT_Assert(rv == SECSuccess);
	}
    }
    return SECSuccess;
}

/* This function might be a candidate to be public, 
 * except that it takes an sslSocket pointer as an argument.
 * A Public version would take a PRFileDesc pointer.
 * Disables all export ciphers in the default set of enabled ciphers.
 */
SECStatus 
SSL_DisableExportCipherSuites(PRFileDesc * fd)
{
    const SSLCipherSuiteInfo * pInfo = suiteInfo;
    unsigned int i;
    SECStatus rv;

    for (i = 0; i < NUM_SUITEINFOS; ++i, ++pInfo) {
    	if (pInfo->isExportable) {
	    rv = SSL_CipherPrefSet(fd, pInfo->cipherSuite, PR_FALSE);
	    PORT_Assert(rv == SECSuccess);
	}
    }
    return SECSuccess;
}

/* Tells us if the named suite is exportable 
 * returns false for unknown suites.
 */
PRBool
SSL_IsExportCipherSuite(PRUint16 cipherSuite)
{
    unsigned int i;
    for (i = 0; i < NUM_SUITEINFOS; i++) {
    	if (suiteInfo[i].cipherSuite == cipherSuite) {
	    return (PRBool)(suiteInfo[i].isExportable);
	}
    }
    return PR_FALSE;
}

SECItem*
SSL_GetNegotiatedHostInfo(PRFileDesc *fd)
{
    SECItem *sniName = NULL;
    sslSocket *ss;
    char *name = NULL;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetNegotiatedHostInfo",
		 SSL_GETPID(), fd));
	return NULL;
    }

    if (ss->sec.isServer) {
        if (ss->version > SSL_LIBRARY_VERSION_3_0 &&
            ss->ssl3.initialized) { /* TLS */
            SECItem *crsName;
            ssl_GetSpecReadLock(ss); /*********************************/
            crsName = &ss->ssl3.crSpec->srvVirtName;
            if (crsName->data) {
                sniName = SECITEM_DupItem(crsName);
            }
            ssl_ReleaseSpecReadLock(ss); /*----------------------------*/
        }
        return sniName;
    } 
    name = SSL_RevealURL(fd);
    if (name) {
        sniName = PORT_ZNew(SECItem);
        if (!sniName) {
            PORT_Free(name);
            return NULL;
        }
        sniName->data = (void*)name;
        sniName->len  = PORT_Strlen(name);
    }
    return sniName;
}
