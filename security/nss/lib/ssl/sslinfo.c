/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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

    if (ss->opt.useSecurity && ss->enoughFirstHsDone) {
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
	     * function should get it from cwSpec rather than from the "hs".
	     * See bug 275744 comment 69 and bug 766137.
	     */
	    inf.cipherSuite           = ss->ssl3.hs.cipher_suite;
	    inf.compressionMethod     = ss->ssl3.cwSpec->compression_method;
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
#define C_CAMELLIA "CAMELLIA", calg_camellia
#define C_AES	"AES", calg_aes
#define C_RC4	"RC4", calg_rc4
#define C_RC2	"RC2", calg_rc2
#define C_DES	"DES", calg_des
#define C_3DES	"3DES", calg_3des
#define C_NULL  "NULL", calg_null
#define C_SJ 	"SKIPJACK", calg_sj
#define C_AESGCM "AES-GCM", calg_aes_gcm

#define B_256	256, 256, 256
#define B_128	128, 128, 128
#define B_3DES  192, 156, 112
#define B_SJ     96,  80,  80
#define B_DES    64,  56,  56
#define B_56    128,  56,  56
#define B_40    128,  40,  40
#define B_0  	  0,   0,   0

#define M_AEAD_128 "AEAD", ssl_mac_aead, 128
#define M_SHA256 "SHA256", ssl_hmac_sha256, 256
#define M_SHA	"SHA1", ssl_mac_sha, 160
#define M_MD5	"MD5",  ssl_mac_md5, 128
#define M_NULL	"NULL", ssl_mac_null,  0

static const SSLCipherSuiteInfo suiteInfo[] = {
/* <------ Cipher suite --------------------> <auth> <KEA>  <bulk cipher> <MAC> <FIPS> */
{0,CS(TLS_RSA_WITH_AES_128_GCM_SHA256),       S_RSA, K_RSA, C_AESGCM, B_128, M_AEAD_128, 1, 0, 0, },

{0,CS(TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA), S_RSA, K_DHE, C_CAMELLIA, B_256, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA), S_DSA, K_DHE, C_CAMELLIA, B_256, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256),   S_RSA, K_DHE, C_AES, B_256, M_SHA256, 1, 0, 0, },
{0,CS(TLS_DHE_RSA_WITH_AES_256_CBC_SHA),      S_RSA, K_DHE, C_AES, B_256, M_SHA, 1, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_AES_256_CBC_SHA),      S_DSA, K_DHE, C_AES, B_256, M_SHA, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_CAMELLIA_256_CBC_SHA),     S_RSA, K_RSA, C_CAMELLIA, B_256, M_SHA, 0, 0, 0, },
{0,CS(TLS_RSA_WITH_AES_256_CBC_SHA256),       S_RSA, K_RSA, C_AES, B_256, M_SHA256, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_AES_256_CBC_SHA),          S_RSA, K_RSA, C_AES, B_256, M_SHA, 1, 0, 0, },

{0,CS(TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA), S_RSA, K_DHE, C_CAMELLIA, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA), S_DSA, K_DHE, C_CAMELLIA, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_RC4_128_SHA),          S_DSA, K_DHE, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256),   S_RSA, K_DHE, C_AES, B_128, M_SHA256, 1, 0, 0, },
{0,CS(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256),   S_RSA, K_DHE, C_AESGCM, B_128, M_AEAD_128, 1, 0, 0, },
{0,CS(TLS_DHE_RSA_WITH_AES_128_CBC_SHA),      S_RSA, K_DHE, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_AES_128_CBC_SHA),      S_DSA, K_DHE, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_SEED_CBC_SHA),             S_RSA, K_RSA, C_SEED,B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_CAMELLIA_128_CBC_SHA),     S_RSA, K_RSA, C_CAMELLIA, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_RSA_WITH_RC4_128_SHA),              S_RSA, K_RSA, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_RSA_WITH_RC4_128_MD5),              S_RSA, K_RSA, C_RC4, B_128, M_MD5, 0, 0, 0, },
{0,CS(TLS_RSA_WITH_AES_128_CBC_SHA256),       S_RSA, K_RSA, C_AES, B_128, M_SHA256, 1, 0, 0, },
{0,CS(TLS_RSA_WITH_AES_128_CBC_SHA),          S_RSA, K_RSA, C_AES, B_128, M_SHA, 1, 0, 0, },

{0,CS(TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA),     S_RSA, K_DHE, C_3DES,B_3DES,M_SHA, 1, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA),     S_DSA, K_DHE, C_3DES,B_3DES,M_SHA, 1, 0, 0, },
{0,CS(SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA),    S_RSA, K_RSA, C_3DES,B_3DES,M_SHA, 1, 0, 1, },
{0,CS(TLS_RSA_WITH_3DES_EDE_CBC_SHA),         S_RSA, K_RSA, C_3DES,B_3DES,M_SHA, 1, 0, 0, },

{0,CS(TLS_DHE_RSA_WITH_DES_CBC_SHA),          S_RSA, K_DHE, C_DES, B_DES, M_SHA, 0, 0, 0, },
{0,CS(TLS_DHE_DSS_WITH_DES_CBC_SHA),          S_DSA, K_DHE, C_DES, B_DES, M_SHA, 0, 0, 0, },
{0,CS(SSL_RSA_FIPS_WITH_DES_CBC_SHA),         S_RSA, K_RSA, C_DES, B_DES, M_SHA, 0, 0, 1, },
{0,CS(TLS_RSA_WITH_DES_CBC_SHA),              S_RSA, K_RSA, C_DES, B_DES, M_SHA, 0, 0, 0, },

{0,CS(TLS_RSA_EXPORT1024_WITH_RC4_56_SHA),    S_RSA, K_RSA, C_RC4, B_56,  M_SHA, 0, 1, 0, },
{0,CS(TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA),   S_RSA, K_RSA, C_DES, B_DES, M_SHA, 0, 1, 0, },
{0,CS(TLS_RSA_EXPORT_WITH_RC4_40_MD5),        S_RSA, K_RSA, C_RC4, B_40,  M_MD5, 0, 1, 0, },
{0,CS(TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5),    S_RSA, K_RSA, C_RC2, B_40,  M_MD5, 0, 1, 0, },
{0,CS(TLS_RSA_WITH_NULL_SHA256),              S_RSA, K_RSA, C_NULL,B_0,   M_SHA256, 0, 1, 0, },
{0,CS(TLS_RSA_WITH_NULL_SHA),                 S_RSA, K_RSA, C_NULL,B_0,   M_SHA, 0, 1, 0, },
{0,CS(TLS_RSA_WITH_NULL_MD5),                 S_RSA, K_RSA, C_NULL,B_0,   M_MD5, 0, 1, 0, },

#ifndef NSS_DISABLE_ECC
/* ECC cipher suites */
{0,CS(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256), S_RSA, K_ECDHE, C_AESGCM, B_128, M_AEAD_128, 1, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256), S_ECDSA, K_ECDHE, C_AESGCM, B_128, M_AEAD_128, 1, 0, 0, },

{0,CS(TLS_ECDH_ECDSA_WITH_NULL_SHA),          S_ECDSA, K_ECDH, C_NULL, B_0, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_RC4_128_SHA),       S_ECDSA, K_ECDH, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA),  S_ECDSA, K_ECDH, C_3DES, B_3DES, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA),   S_ECDSA, K_ECDH, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA),   S_ECDSA, K_ECDH, C_AES, B_256, M_SHA, 1, 0, 0, },

{0,CS(TLS_ECDHE_ECDSA_WITH_NULL_SHA),         S_ECDSA, K_ECDHE, C_NULL, B_0, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA),      S_ECDSA, K_ECDHE, C_RC4, B_128, M_SHA, 0, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA), S_ECDSA, K_ECDHE, C_3DES, B_3DES, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA),  S_ECDSA, K_ECDHE, C_AES, B_128, M_SHA, 1, 0, 0, },
{0,CS(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256), S_ECDSA, K_ECDHE, C_AES, B_128, M_SHA256, 1, 0, 0, },
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
{0,CS(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256), S_RSA, K_ECDHE, C_AES, B_128, M_SHA256, 1, 0, 0, },
{0,CS(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA),    S_RSA, K_ECDHE, C_AES, B_256, M_SHA, 1, 0, 0, },
#endif /* NSS_DISABLE_ECC */

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
            crsName = &ss->ssl3.cwSpec->srvVirtName;
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

SECStatus
SSL_ExportKeyingMaterial(PRFileDesc *fd,
                         const char *label, unsigned int labelLen,
                         PRBool hasContext,
                         const unsigned char *context, unsigned int contextLen,
                         unsigned char *out, unsigned int outLen)
{
    sslSocket *ss;
    unsigned char *val = NULL;
    unsigned int valLen, i;
    SECStatus rv = SECFailure;

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in ExportKeyingMaterial",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    if (ss->version < SSL_LIBRARY_VERSION_3_1_TLS) {
	PORT_SetError(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_VERSION);
	return SECFailure;
    }

    /* construct PRF arguments */
    valLen = SSL3_RANDOM_LENGTH * 2;
    if (hasContext) {
	valLen += 2 /* PRUint16 length */ + contextLen;
    }
    val = PORT_Alloc(valLen);
    if (!val) {
	return SECFailure;
    }
    i = 0;
    PORT_Memcpy(val + i, &ss->ssl3.hs.client_random.rand, SSL3_RANDOM_LENGTH);
    i += SSL3_RANDOM_LENGTH;
    PORT_Memcpy(val + i, &ss->ssl3.hs.server_random.rand, SSL3_RANDOM_LENGTH);
    i += SSL3_RANDOM_LENGTH;
    if (hasContext) {
	val[i++] = contextLen >> 8;
	val[i++] = contextLen;
	PORT_Memcpy(val + i, context, contextLen);
	i += contextLen;
    }
    PORT_Assert(i == valLen);

    /* Allow TLS keying material to be exported sooner, when the master
     * secret is available and we have sent ChangeCipherSpec.
     */
    ssl_GetSpecReadLock(ss);
    if (!ss->ssl3.cwSpec->master_secret && !ss->ssl3.cwSpec->msItem.len) {
	PORT_SetError(SSL_ERROR_HANDSHAKE_NOT_COMPLETED);
	rv = SECFailure;
    } else {
	rv = ssl3_TLSPRFWithMasterSecret(ss->ssl3.cwSpec, label, labelLen, val,
					 valLen, out, outLen);
    }
    ssl_ReleaseSpecReadLock(ss);

    PORT_ZFree(val, valLen);
    return rv;
}
