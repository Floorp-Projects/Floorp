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
 * Copyright (C) 2001 Netscape Communications Corporation.  All 
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
 * $Id: sslinfo.c,v 1.1 2001/09/18 01:59:20 nelsonb%netscape.com Exp $
 */
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"

typedef struct BulkCipherInfoStr {
    SSLCipherAlgorithm  symCipher;
    PRUint16		symKeyBits;
    PRUint16		symKeySpace;
    PRUint16		effectiveKeyBits;
} BulkCipherInfo;

static const BulkCipherInfo ssl2CipherInfo[] = {
/* NONE                                 */ { ssl_calg_null,   0,   0,   0 },
/* SSL_CK_RC4_128_WITH_MD5		*/ { ssl_calg_rc4,  128, 128, 128 },
/* SSL_CK_RC4_128_EXPORT40_WITH_MD5	*/ { ssl_calg_rc4,  128,  40,  40 },
/* SSL_CK_RC2_128_CBC_WITH_MD5		*/ { ssl_calg_rc2,  128, 128, 128 },
/* SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5	*/ { ssl_calg_rc2,  128,  40,  40 },
/* SSL_CK_IDEA_128_CBC_WITH_MD5		*/ { ssl_calg_idea,   0,   0,   0 },
/* SSL_CK_DES_64_CBC_WITH_MD5		*/ { ssl_calg_des,   64,  56,  56 },
/* SSL_CK_DES_192_EDE3_CBC_WITH_MD5	*/ { ssl_calg_3des, 192, 168, 112 }
};

static const char * const authName[] = {
  { "NULL" },
  { "RSA"  },
  { "DSA"  }
};

static const char * const keaName[] = {
  { "NULL" },
  { "RSA"  },
  { "DH"   },
  { "KEA"  },
  { "BOGUS" }
};

static const char * const cipherName[] = {
  { "NULL" },
  { "RC4"  },
  { "RC2"  },
  { "DES"  },
  { "3DES" },
  { "IDEA" },
  { "SKIPJACK" },
  { "AES"  }
};

static const char * const macName[] = {
  { "NULL" },
  { "MD5"  },
  { "SHA"  },
  { "MD5"  },
  { "SHA"  }
};

#define SSL_OFFSETOF(str, memb) ((PRPtrdiff)(&(((str *)0)->memb)))

SECStatus SSL_GetChannelInfo(PRFileDesc *fd, SSLChannelInfo *info, PRUintn len)
{
    sslSocket *      ss;
    sslSecurityInfo *sec;
    SSLChannelInfo   inf;

    if (!info) {  /* He doesn't want it?  OK. */
    	return SECSuccess;
    }

    ss = ssl_FindSocket(fd);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetChannelInfo",
		 SSL_GETPID(), fd));
	return SECFailure;
    }

    memset(&inf, 0, sizeof inf);
    inf.length = SSL_OFFSETOF(SSLChannelInfo, reserved);
    inf.length = PR_MIN(inf.length, len);

    sec = ss->sec;
    if (ss->useSecurity && ss->firstHsDone && sec) {
	if (ss->version < SSL_LIBRARY_VERSION_3_0) {
	    /* SSL2 */
	    const BulkCipherInfo * bulk = ssl2CipherInfo + ss->sec->cipherType;

	    inf.protocolVersion  = ss->version;
	    inf.cipherSuite      = ss->sec->cipherType | 0xff00;

	    /* server auth */
	    inf.authAlgorithm    = ss->sec->authAlgorithm;
	    inf.authKeyBits      = ss->sec->authKeyBits;

	    /* key exchange */
	    inf.keaType          = ss->sec->keaType;
	    inf.keaKeyBits       = ss->sec->keaKeyBits;

	    /* symmetric cipher */
	    inf.symCipher        = bulk->symCipher;
	    inf.symKeyBits       = bulk->symKeyBits;
	    inf.symKeySpace      = bulk->symKeySpace;
	    inf.effectiveKeyBits = bulk->effectiveKeyBits;

	    /* MAC info */
	    inf.macAlgorithm     = ssl_mac_md5;
	    inf.macBits          = MD5_LENGTH * BPB;

	    /* misc */
	    inf.isFIPS           = 0;

	} else if (ss->ssl3 && ss->ssl3->crSpec && 
	           ss->ssl3->crSpec->cipher_def) {
	    /* SSL3 and TLS */
	    ssl3CipherSpec *          crSpec     = ss->ssl3->crSpec;
	    const ssl3BulkCipherDef * cipher_def = crSpec->cipher_def;

	    /* XXX NBB These should come from crSpec */
	    inf.protocolVersion      = ss->version;
	    inf.cipherSuite          = ss->ssl3->hs.cipher_suite;

	    /* server auth */
	    inf.authAlgorithm        = ss->sec->authAlgorithm;
	    inf.authKeyBits          = ss->sec->authKeyBits;

	    /* key exchange */
	    inf.keaType              = ss->sec->keaType;
	    inf.keaKeyBits           = ss->sec->keaKeyBits;

	    /* symmetric cipher */
	    inf.symCipher            = cipher_def->calg;
	    switch (inf.symCipher) {
	    case ssl_calg_des:
		inf.symKeyBits       = cipher_def->key_size        * 8 ;
		inf.symKeySpace      = \
		inf.effectiveKeyBits = cipher_def->secret_key_size * 7 ;
		break;
	    case ssl_calg_3des:
		inf.symKeyBits       = cipher_def->key_size        * 8 ;
		inf.symKeySpace      = cipher_def->secret_key_size * 7 ;
		inf.effectiveKeyBits = (inf.symKeySpace / 3 ) * 2;
		break;
	    default:
		inf.symKeyBits       = cipher_def->key_size * BPB ;
		inf.symKeySpace      = \
		inf.effectiveKeyBits = cipher_def->secret_key_size * BPB ;
		break;
            }

	    /* MAC info */
	    inf.macAlgorithm         = crSpec->mac_def->mac;
	    inf.macBits              = crSpec->mac_def->mac_size * BPB;

	    /* misc */
	    inf.isFIPS =  (inf.symCipher == ssl_calg_des ||
	                   inf.symCipher == ssl_calg_3des ||
	                   inf.symCipher == ssl_calg_aes)
		       && (inf.macAlgorithm == ssl_mac_sha ||
		           inf.macAlgorithm == ssl_hmac_sha)
		       && (inf.protocolVersion > SSL_LIBRARY_VERSION_3_0 ||
		           inf.cipherSuite >= 0xfef0);
	}

    }
    inf.authAlgorithmName = authName[  inf.authAlgorithm];
    inf.keaTypeName       = keaName[   inf.keaType      ];
    inf.symCipherName     = cipherName[inf.symCipher    ];
    inf.macAlgorithmName  = macName[   inf.macAlgorithm ];

    memcpy(info, &inf, inf.length);

    return SECSuccess;
}

