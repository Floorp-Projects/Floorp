/*
 * This file is PRIVATE to SSL and should be the first thing included by
 * any SSL implementation file.
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
 *   Dr Stephen Henson <stephen.henson@gemplus.com>
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
/* $Id: sslimpl.h,v 1.35 2004/06/19 03:21:39 jpierre%netscape.com Exp $ */

#ifndef __sslimpl_h_
#define __sslimpl_h_

#ifdef DEBUG
#undef NDEBUG
#else
#undef NDEBUG
#define NDEBUG
#endif
#include "secport.h"
#include "secerr.h"
#include "sslerr.h"
#include "ssl3prot.h"
#include "hasht.h"
#include "nssilock.h"
#include "pkcs11t.h"
#if defined(XP_UNIX) || defined(XP_BEOS)
#include "unistd.h"
#endif
#include "nssrwlk.h"
#include "prthread.h"

#include "sslt.h" /* for some formerly private types, now public */

/* to make some of these old enums public without namespace pollution,
** it was necessary to prepend ssl_ to the names.
** These #defines preserve compatibility with the old code here in libssl.
*/
typedef SSLKEAType      SSL3KEAType;
typedef SSLMACAlgorithm SSL3MACAlgorithm;
typedef SSLSignType     SSL3SignType;

#define sign_null	ssl_sign_null
#define sign_rsa	ssl_sign_rsa
#define sign_dsa	ssl_sign_dsa
#define sign_ecdsa	ssl_sign_ecdsa

#define calg_null	ssl_calg_null
#define calg_rc4	ssl_calg_rc4
#define calg_rc2	ssl_calg_rc2
#define calg_des	ssl_calg_des
#define calg_3des	ssl_calg_3des
#define calg_idea	ssl_calg_idea
#define calg_fortezza	ssl_calg_fortezza
#define calg_aes	ssl_calg_aes

#define mac_null	ssl_mac_null
#define mac_md5 	ssl_mac_md5
#define mac_sha 	ssl_mac_sha
#define hmac_md5	ssl_hmac_md5
#define hmac_sha	ssl_hmac_sha


#if defined(DEBUG) || defined(TRACE)
#ifdef __cplusplus
#define Debug 1
#else
extern int Debug;
#endif
#else
#undef Debug
#endif

#if defined(DEBUG) && !defined(TRACE) && !defined(NISCC_TEST)
#define TRACE
#endif

#ifdef TRACE
#define SSL_TRC(a,b) if (ssl_trace >= (a)) ssl_Trace b
#define PRINT_BUF(a,b) if (ssl_trace >= (a)) ssl_PrintBuf b
#define DUMP_MSG(a,b) if (ssl_trace >= (a)) ssl_DumpMsg b
#else
#define SSL_TRC(a,b)
#define PRINT_BUF(a,b)
#define DUMP_MSG(a,b)
#endif

#ifdef DEBUG
#define SSL_DBG(b) if (ssl_debug) ssl_Trace b
#else
#define SSL_DBG(b)
#endif

#if defined (DEBUG)
#ifdef macintosh
#include "pprthred.h"
#else
#include "private/pprthred.h"	/* for PR_InMonitor() */
#endif
#define ssl_InMonitor(m) PZ_InMonitor(m)
#endif

#define LSB(x) ((unsigned char) (x & 0xff))
#define MSB(x) ((unsigned char) (((unsigned)(x)) >> 8))

/************************************************************************/

typedef enum { SSLAppOpRead = 0,
	       SSLAppOpWrite,
	       SSLAppOpRDWR,
	       SSLAppOpPost,
	       SSLAppOpHeader
} SSLAppOperation;

#define SSL_MIN_MASTER_KEY_BYTES	5
#define SSL_MAX_MASTER_KEY_BYTES	64

#define SSL2_SESSIONID_BYTES		16
#define SSL3_SESSIONID_BYTES		32

#define SSL_MIN_CHALLENGE_BYTES		16
#define SSL_MAX_CHALLENGE_BYTES		32
#define SSL_CHALLENGE_BYTES		16

#define SSL_CONNECTIONID_BYTES		16

#define SSL_MIN_CYPHER_ARG_BYTES	0
#define SSL_MAX_CYPHER_ARG_BYTES	32

#define SSL_MAX_MAC_BYTES		16

/* number of wrap mechanisms potentially used to wrap master secrets. */
#define SSL_NUM_WRAP_MECHS              13

/* This makes the cert cache entry exactly 4k. */
#define SSL_MAX_CACHED_CERT_LEN		4060

#ifndef BPB
#define BPB 8 /* Bits Per Byte */
#endif

typedef struct sslBufferStr             sslBuffer;
typedef struct sslConnectInfoStr        sslConnectInfo;
typedef struct sslGatherStr             sslGather;
typedef struct sslSecurityInfoStr       sslSecurityInfo;
typedef struct sslSessionIDStr          sslSessionID;
typedef struct sslSocketStr             sslSocket;
typedef struct sslSocketOpsStr          sslSocketOps;

typedef struct ssl3StateStr             ssl3State;
typedef struct ssl3CertNodeStr          ssl3CertNode;
typedef struct ssl3BulkCipherDefStr     ssl3BulkCipherDef;
typedef struct ssl3MACDefStr            ssl3MACDef;
typedef struct ssl3KeyPairStr		ssl3KeyPair;

struct ssl3CertNodeStr {
    struct ssl3CertNodeStr *next;
    CERTCertificate *       cert;
};

typedef SECStatus (*sslHandshakeFunc)(sslSocket *ss);

/* This type points to the low layer send func, 
** e.g. ssl2_SendStream or ssl3_SendPlainText.
** These functions return the same values as PR_Send, 
** i.e.  >= 0 means number of bytes sent, < 0 means error.
*/
typedef PRInt32       (*sslSendFunc)(sslSocket *ss, const unsigned char *buf,
			             PRInt32 n, PRInt32 flags);

typedef void          (*sslSessionIDCacheFunc)  (sslSessionID *sid);
typedef void          (*sslSessionIDUncacheFunc)(sslSessionID *sid);
typedef sslSessionID *(*sslSessionIDLookupFunc)(const PRIPv6Addr    *addr,
						unsigned char* sid,
						unsigned int   sidLen,
                                                CERTCertDBHandle * dbHandle);


/* Socket ops */
struct sslSocketOpsStr {
    int         (*connect) (sslSocket *, const PRNetAddr *);
    PRFileDesc *(*accept)  (sslSocket *, PRNetAddr *);
    int         (*bind)    (sslSocket *, const PRNetAddr *);
    int         (*listen)  (sslSocket *, int);
    int         (*shutdown)(sslSocket *, int);
    int         (*close)   (sslSocket *);

    int         (*recv)    (sslSocket *, unsigned char *, int, int);

    /* points to the higher-layer send func, e.g. ssl_SecureSend. */
    int         (*send)    (sslSocket *, const unsigned char *, int, int);
    int         (*read)    (sslSocket *, unsigned char *, int);
    int         (*write)   (sslSocket *, const unsigned char *, int);

    int         (*getpeername)(sslSocket *, PRNetAddr *);
    int         (*getsockname)(sslSocket *, PRNetAddr *);
};

/* Flags interpreted by ssl send functions. */
#define ssl_SEND_FLAG_FORCE_INTO_BUFFER	0x40000000
#define ssl_SEND_FLAG_NO_BUFFER		0x20000000
#define ssl_SEND_FLAG_MASK		0x7f000000

/*
** A buffer object.
*/
struct sslBufferStr {
    unsigned char *	buf;
    unsigned int 	len;
    unsigned int 	space;
};

/*
** SSL3 cipher suite policy and preference struct.
*/
typedef struct {
#if !defined(_WIN32)
    unsigned int    cipher_suite : 16;
    unsigned int    policy       :  8;
    unsigned int    enabled      :  1;
    unsigned int    isPresent    :  1;
#else
    ssl3CipherSuite cipher_suite;
    PRUint8         policy;
    unsigned char   enabled   : 1;
    unsigned char   isPresent : 1;
#endif
} ssl3CipherSuiteCfg;

#ifdef NSS_ENABLE_ECC
#define ssl_V3_SUITES_IMPLEMENTED 40
#else
#define ssl_V3_SUITES_IMPLEMENTED 26
#endif /* NSS_ENABLE_ECC */

typedef struct sslOptionsStr {
    unsigned int useSecurity		: 1;  /*  1 */
    unsigned int useSocks		: 1;  /*  2 */
    unsigned int requestCertificate	: 1;  /*  3 */
    unsigned int requireCertificate	: 2;  /*  4-5 */
    unsigned int handshakeAsClient	: 1;  /*  6 */
    unsigned int handshakeAsServer	: 1;  /*  7 */
    unsigned int enableSSL2		: 1;  /*  8 */
    unsigned int enableSSL3		: 1;  /*  9 */
    unsigned int enableTLS		: 1;  /* 10 */
    unsigned int noCache		: 1;  /* 11 */
    unsigned int fdx			: 1;  /* 12 */
    unsigned int v2CompatibleHello	: 1;  /* 13 */
    unsigned int detectRollBack  	: 1;  /* 14 */
} sslOptions;

typedef enum { sslHandshakingUndetermined = 0,
	       sslHandshakingAsClient,
	       sslHandshakingAsServer 
} sslHandshakingType;

typedef struct sslServerCertsStr {
    /* Configuration state for server sockets */
    CERTCertificate *     serverCert;
    CERTCertificateList * serverCertChain;
    SECKEYPrivateKey *    serverKey;
    unsigned int          serverKeyBits;
} sslServerCerts;


#define SSL_LOCK_RANK_SPEC 	255
#define SSL_LOCK_RANK_GLOBAL 	NSS_RWLOCK_RANK_NONE

/* These are the valid values for shutdownHow. 
** These values are each 1 greater than the NSPR values, and the code
** depends on that relation to efficiently convert PR_SHUTDOWN values 
** into ssl_SHUTDOWN values.  These values use one bit for read, and 
** another bit for write, and can be used as bitmasks.
*/
#define ssl_SHUTDOWN_NONE	0	/* NOT shutdown at all */
#define ssl_SHUTDOWN_RCV	1	/* PR_SHUTDOWN_RCV  +1 */
#define ssl_SHUTDOWN_SEND	2	/* PR_SHUTDOWN_SEND +1 */
#define ssl_SHUTDOWN_BOTH	3	/* PR_SHUTDOWN_BOTH +1 */

/*
** A gather object. Used to read some data until a count has been
** satisfied. Primarily for support of async sockets.
** Everything in here is protected by the recvBufLock.
*/
struct sslGatherStr {
    int           state;	/* see GS_ values below. */     /* ssl 2 & 3 */

    /* "buf" holds received plaintext SSL records, after decrypt and MAC check.
     * SSL2: recv'd ciphertext records are put here, then decrypted in place.
     * SSL3: recv'd ciphertext records are put in inbuf (see below), then 
     *       decrypted into buf.
     */
    sslBuffer     buf;				/*recvBufLock*/	/* ssl 2 & 3 */

    /* number of bytes previously read into hdr or buf(ssl2) or inbuf (ssl3). 
    ** (offset - writeOffset) is the number of ciphertext bytes read in but 
    **     not yet deciphered.
    */
    unsigned int  offset;                                       /* ssl 2 & 3 */

    /* number of bytes to read in next call to ssl_DefRecv (recv) */
    unsigned int  remainder;                                    /* ssl 2 & 3 */

    /* Number of ciphertext bytes to read in after 2-byte SSL record header. */
    unsigned int  count;					/* ssl2 only */

    /* size of the final plaintext record. 
    ** == count - (recordPadding + MAC size)
    */
    unsigned int  recordLen;					/* ssl2 only */

    /* number of bytes of padding to be removed after decrypting. */
    /* This value is taken from the record's hdr[2], which means a too large
     * value could crash us.
     */
    unsigned int  recordPadding;				/* ssl2 only */

    /* plaintext DATA begins this many bytes into "buf".  */
    unsigned int  recordOffset;					/* ssl2 only */

    int           encrypted;    /* SSL2 session is now encrypted.  ssl2 only */

    /* These next two values are used by SSL2 and SSL3.  
    ** DoRecv uses them to extract application data.
    ** The difference between writeOffset and readOffset is the amount of 
    ** data available to the application.   Note that the actual offset of 
    ** the data in "buf" is recordOffset (above), not readOffset.
    ** In the current implementation, this is made available before the 
    ** MAC is checked!!
    */
    unsigned int  readOffset;  /* Spot where DATA reader (e.g. application
                               ** or handshake code) will read next.
                               ** Always zero for SSl3 application data.
			       */
    /* offset in buf/inbuf/hdr into which new data will be read from socket. */
    unsigned int  writeOffset; 

    /* Buffer for ssl3 to read (encrypted) data from the socket */
    sslBuffer     inbuf;			/*recvBufLock*/	/* ssl3 only */

    /* The ssl[23]_GatherData functions read data into this buffer, rather
    ** than into buf or inbuf, while in the GS_HEADER state.  
    ** The portion of the SSL record header put here always comes off the wire 
    ** as plaintext, never ciphertext.
    ** For SSL2, the plaintext portion is two bytes long.  For SSl3 it is 5.
    */
    unsigned char hdr[5];					/* ssl 2 & 3 */
};

/* sslGather.state */
#define GS_INIT		0
#define GS_HEADER	1
#define GS_MAC		2
#define GS_DATA		3
#define GS_PAD		4

typedef SECStatus (*SSLCipher)(void *               context, 
                               unsigned char *      out,
			       int *                outlen, 
			       int                  maxout, 
			       const unsigned char *in,
			       int                  inlen);
typedef SECStatus (*SSLDestroy)(void *context, PRBool freeit);



/*
** ssl3State and CipherSpec structs
*/

/* The SSL bulk cipher definition */
typedef enum {
    cipher_null,
    cipher_rc4, 
    cipher_rc4_40,
    cipher_rc4_56,
    cipher_rc2, 
    cipher_rc2_40,
    cipher_des, 
    cipher_3des, 
    cipher_des40,
    cipher_idea, 
    cipher_fortezza,
    cipher_aes_128,
    cipher_aes_256,
    cipher_missing              /* reserved for no such supported cipher */
    /* This enum must match ssl3_cipherName[] in ssl3con.c.  */
} SSL3BulkCipher;

typedef enum { type_stream, type_block } CipherType;

#define MAX_IV_LENGTH 64

/*
 * Do not depend upon 64 bit arithmetic in the underlying machine. 
 */
typedef struct {
    uint32         high;
    uint32         low;
} SSL3SequenceNumber;

typedef struct {
    SSL3Opaque write_iv[MAX_IV_LENGTH];
    PK11SymKey *write_key;
    PK11SymKey *write_mac_key;
    PK11Context *write_mac_context;
} ssl3KeyMaterial;

typedef struct {
    SSL3Opaque        wrapped_client_write_key[12]; /* wrapped with Ks */
    SSL3Opaque        wrapped_server_write_key[12]; /* wrapped with Ks */
    SSL3Opaque        client_write_iv         [24];
    SSL3Opaque        server_write_iv         [24];
    SSL3Opaque        wrapped_master_secret   [48];
    PRUint16          wrapped_master_secret_len;
} ssl3SidKeys;

/*
** These are the "specs" in the "ssl3" struct.
** Access to the pointers to these specs, and all the specs' contents
** (direct and indirect) is protected by the reader/writer lock ss->specLock.
*/
typedef struct {
    const ssl3BulkCipherDef *cipher_def;
    const ssl3MACDef * mac_def;
    int                mac_size;
    SSLCipher          encode;
    void *             encodeContext;
    SSLCipher          decode;
    void *             decodeContext;
    SSLDestroy         destroy;
    PK11SymKey *       master_secret;
    ssl3KeyMaterial    client;
    ssl3KeyMaterial    server;
    SSL3SequenceNumber write_seq_num;
    SSL3SequenceNumber read_seq_num;
    SSL3ProtocolVersion version;
} ssl3CipherSpec;

typedef enum {	never_cached, 
		in_client_cache, 
		in_server_cache, 
		invalid_cache		/* no longer in any cache. */
} Cached;

struct sslSessionIDStr {
    sslSessionID *        next;   /* chain used for client sockets, only */

    CERTCertificate *     peerCert;
    const char *          peerID;     /* client only */
    const char *          urlSvrName; /* client only */
    CERTCertificate *     localCert;

    PRIPv6Addr            addr;
    PRUint16              port;

    SSL3ProtocolVersion   version;

    PRUint32              creationTime;		/* seconds since Jan 1, 1970 */
    PRUint32              lastAccessTime;	/* seconds since Jan 1, 1970 */
    PRUint32              expirationTime;	/* seconds since Jan 1, 1970 */
    Cached                cached;
    int                   references;

    SSLSignType           authAlgorithm;
    PRUint32              authKeyBits;
    SSLKEAType            keaType;
    PRUint32              keaKeyBits;

    union {
	struct {
	    /* the V2 code depends upon the size of sessionID.  */
	    unsigned char         sessionID[SSL2_SESSIONID_BYTES];

	    /* Stuff used to recreate key and read/write cipher objects */
	    SECItem               masterKey;
	    int                   cipherType;
	    SECItem               cipherArg;
	    int                   keyBits;
	    int                   secretKeyBits;
	} ssl2;
	struct {
	    /* values that are copied into the server's on-disk SID cache. */
	    uint8                 sessionIDLength;
	    SSL3Opaque            sessionID[SSL3_SESSIONID_BYTES];

	    ssl3CipherSuite       cipherSuite;
	    SSL3CompressionMethod compression;
	    PRBool                resumable;
	    int                   policy;
	    PRBool                hasFortezza;
	    ssl3SidKeys           keys;
	    CK_MECHANISM_TYPE     masterWrapMech;
				  /* mechanism used to wrap master secret */
            SSL3KEAType           exchKeyType;
				  /* key type used in exchange algorithm,
				   * and to wrap the sym wrapping key. */

	    /* The following values are NOT restored from the server's on-disk
	     * session cache, but are restored from the client's cache.
	     */
 	    PK11SymKey *      clientWriteKey;
	    PK11SymKey *      serverWriteKey;
	    PK11SymKey *      tek;

	    /* The following values pertain to the slot that wrapped the 
	    ** master secret. (used only in client)
	    */
	    SECMODModuleID    masterModuleID;
				    /* what module wrapped the master secret */
	    CK_SLOT_ID        masterSlotID;
	    PRUint16	      masterWrapIndex;
				/* what's the key index for the wrapping key */
	    PRUint16          masterWrapSeries;
	                        /* keep track of the slot series, so we don't 
				 * accidently try to use new keys after the 
				 * card gets removed and replaced.*/

	    /* The following values pertain to the slot that did the signature
	    ** for client auth.   (used only in client)
	    */
	    SECMODModuleID    clAuthModuleID;
	    CK_SLOT_ID        clAuthSlotID;
	    PRUint16          clAuthSeries;

            char              masterValid;
	    char              clAuthValid;

	    /* the following values are used only in the client, and only 
	     * with fortezza.
	     */
	    SSL3Opaque       clientWriteSave[80]; 
	    int              clientWriteSaveLen;  
	} ssl3;
    } u;
};


typedef struct ssl3CipherSuiteDefStr {
    ssl3CipherSuite          cipher_suite;
    SSL3BulkCipher           bulk_cipher_alg;
    SSL3MACAlgorithm         mac_alg;
    SSL3KeyExchangeAlgorithm key_exchange_alg;
} ssl3CipherSuiteDef;

/*
** There are tables of these, all const.
*/
typedef struct {
    SSL3KeyExchangeAlgorithm kea;
    SSL3KEAType              exchKeyType;
    SSL3SignType             signKeyType;
    PRBool                   is_limited;
    int                      key_size_limit;
    PRBool                   tls_keygen;
} ssl3KEADef;

typedef enum { kg_null, kg_strong, kg_export } SSL3KeyGenMode;

/*
** There are tables of these, all const.
*/
struct ssl3BulkCipherDefStr {
    SSL3BulkCipher  cipher;
    SSLCipherAlgorithm calg;
    int             key_size;
    int             secret_key_size;
    CipherType      type;
    int             iv_size;
    int             block_size;
    SSL3KeyGenMode  keygen_mode;
};

/*
** There are tables of these, all const.
*/
struct ssl3MACDefStr {
    SSL3MACAlgorithm mac;
    CK_MECHANISM_TYPE mmech;
    int              pad_size;
    int              mac_size;
};

typedef enum {
    wait_client_hello, 
    wait_client_cert, 
    wait_client_key,
    wait_cert_verify, 
    wait_change_cipher, 
    wait_finished,
    wait_server_hello, 
    wait_server_cert, 
    wait_server_key,
    wait_cert_request, 
    wait_hello_done,
    idle_handshake
} SSL3WaitState;

/*
** This is the "hs" member of the "ssl3" struct.
** This entire struct is protected by ssl3HandshakeLock
*/
typedef struct SSL3HandshakeStateStr {
    SSL3Random            server_random;
    SSL3Random            client_random;
    SSL3WaitState         ws;
    PK11Context *         md5;            /* handshake running hashes */
    PK11Context *         sha;
const ssl3KEADef *        kea_def;
    ssl3CipherSuite       cipher_suite;
const ssl3CipherSuiteDef *suite_def;
    SSL3CompressionMethod compression;
    sslBuffer             msg_body;    /* protected by recvBufLock */
                               /* partial handshake message from record layer */
    unsigned int          header_bytes; 
                               /* number of bytes consumed from handshake */
                               /* message for message type and header length */
    SSL3HandshakeType     msg_type;
    unsigned long         msg_len;
    SECItem               ca_list;     /* used only by client */
    PRBool                isResuming;  /* are we resuming a session */
    PRBool                rehandshake; /* immediately start another handshake 
                                        * when this one finishes */
    PRBool                usedStepDownKey;  /* we did a server key exchange. */
    sslBuffer             msgState;    /* current state for handshake messages*/
                                       /* protected by recvBufLock */
} SSL3HandshakeState;

struct SSL3FortezzaKEAParamsStr {
    unsigned char R_s[128];		/* server's "random" public key	*/
    PK11SymKey *  tek;
};

typedef struct SSL3FortezzaKEAParamsStr SSL3FortezzaKEAParams;

/*
** This is the "ssl3" struct, as in "ss->ssl3".
** note:
** usually,   crSpec == cwSpec and prSpec == pwSpec.  
** Sometimes, crSpec == pwSpec and prSpec == cwSpec.
** But there are never more than 2 actual specs.  
** No spec must ever be modified if either "current" pointer points to it.
*/
struct ssl3StateStr {

    /*
    ** The following Specs and Spec pointers must be protected using the 
    ** Spec Lock.
    */
    ssl3CipherSpec *     crSpec; 	/* current read spec. */
    ssl3CipherSpec *     prSpec; 	/* pending read spec. */
    ssl3CipherSpec *     cwSpec; 	/* current write spec. */
    ssl3CipherSpec *     pwSpec; 	/* pending write spec. */
    ssl3CipherSpec       specs[2];	/* one is current, one is pending. */

    SSL3HandshakeState   hs;

    CERTCertificate *    clientCertificate;  /* used by client */
    SECKEYPrivateKey *   clientPrivateKey;   /* used by client */
    CERTCertificateList *clientCertChain;    /* used by client */
    PRBool               sendEmptyCert;      /* used by client */

    int                  policy;
			/* This says what cipher suites we can do, and should 
			 * be either SSL_ALLOWED or SSL_RESTRICTED 
			 */
    PRArenaPool *        peerCertArena;  
			    /* These are used to keep track of the peer CA */
    void *               peerCertChain;     
			    /* chain while we are trying to validate it.   */
    CERTDistNames *      ca_list; 
			    /* used by server.  trusted CAs for this socket. */
    SSL3FortezzaKEAParams fortezza;
};

typedef struct {
    SSL3ContentType      type;
    SSL3ProtocolVersion  version;
    sslBuffer *          buf;
} SSL3Ciphertext;

struct ssl3KeyPairStr {
    SECKEYPrivateKey *    privKey;		/* RSA step down key */
    SECKEYPublicKey *     pubKey;		/* RSA step down key */
    PRInt32               refCount;	/* use PR_Atomic calls for this. */
};

typedef struct SSLWrappedSymWrappingKeyStr {
    SSL3Opaque        wrappedSymmetricWrappingkey[512];
    SSL3Opaque        wrapIV[24];
    CK_MECHANISM_TYPE symWrapMechanism;  
		    /* unwrapped symmetric wrapping key uses this mechanism */
    CK_MECHANISM_TYPE asymWrapMechanism; 
		    /* mechanism used to wrap the SymmetricWrappingKey using
		     * server's public and/or private keys. */
    SSL3KEAType       exchKeyType;   /* type of keys used to wrap SymWrapKey*/
    PRInt32           symWrapMechIndex;
    PRUint16          wrappedSymKeyLen;
    PRUint16          wrapIVLen;
} SSLWrappedSymWrappingKey;










/*
 * SSL2 buffers used in SSL3.
 *     writeBuf in the SecurityInfo maintained by sslsecur.c is used
 *              to hold the data just about to be passed to the kernel
 *     sendBuf in the ConnectInfo maintained by sslcon.c is used
 *              to hold handshake messages as they are accumulated
 */

/*
** This is "ci", as in "ss->sec->ci".
**
** Protection:  All the variables in here are protected by 
** firstHandshakeLock AND (in ssl3) ssl3HandshakeLock 
*/
struct sslConnectInfoStr {
    /* outgoing handshakes appended to this. */
    sslBuffer       sendBuf;	                /*xmitBufLock*/ /* ssl 2 & 3 */

    PRIPv6Addr      peer;                                       /* ssl 2 & 3 */
    unsigned short  port;                                       /* ssl 2 & 3 */

    sslSessionID   *sid;                                        /* ssl 2 & 3 */

    /* see CIS_HAVE defines below for the bit values in *elements. */
    char            elements;					/* ssl2 only */
    char            requiredElements;				/* ssl2 only */
    char            sentElements;                               /* ssl2 only */

    char            sentFinished;                               /* ssl2 only */

    /* Length of server challenge.  Used by client when saving challenge */
    int             serverChallengeLen;                         /* ssl2 only */
    /* type of authentication requested by server */
    unsigned char   authType;                                   /* ssl2 only */

    /* Challenge sent by client to server in client-hello message */
    /* SSL3 gets a copy of this.  See ssl3_StartHandshakeHash().  */
    unsigned char   clientChallenge[SSL_MAX_CHALLENGE_BYTES];   /* ssl 2 & 3 */

    /* Connection-id sent by server to client in server-hello message */
    unsigned char   connectionID[SSL_CONNECTIONID_BYTES];	/* ssl2 only */

    /* Challenge sent by server to client in request-certificate message */
    unsigned char   serverChallenge[SSL_MAX_CHALLENGE_BYTES];	/* ssl2 only */

    /* Information kept to handle a request-certificate message */
    unsigned char   readKey[SSL_MAX_MASTER_KEY_BYTES];		/* ssl2 only */
    unsigned char   writeKey[SSL_MAX_MASTER_KEY_BYTES];		/* ssl2 only */
    unsigned        keySize;					/* ssl2 only */
};

/* bit values for ci->elements, ci->requiredElements, sentElements. */
#define CIS_HAVE_MASTER_KEY		0x01
#define CIS_HAVE_CERTIFICATE		0x02
#define CIS_HAVE_FINISHED		0x04
#define CIS_HAVE_VERIFY			0x08

/* Note: The entire content of this struct and whatever it points to gets
 * blown away by SSL_ResetHandshake().  This is "sec" as in "ss->sec".
 *
 * Unless otherwise specified below, the contents of this struct are 
 * protected by firstHandshakeLock AND (in ssl3) ssl3HandshakeLock.
 */
struct sslSecurityInfoStr {
    sslSendFunc      send;			/*xmitBufLock*/	/* ssl 2 & 3 */
    int              isServer;			/* Spec Lock?*/	/* ssl 2 & 3 */
    sslBuffer        writeBuf;			/*xmitBufLock*/	/* ssl 2 & 3 */

    int              cipherType;				/* ssl 2 & 3 */
    int              keyBits;					/* ssl 2 & 3 */
    int              secretKeyBits;				/* ssl 2 & 3 */
    CERTCertificate *localCert;					/* ssl 2 & 3 */
    CERTCertificate *peerCert;					/* ssl 2 & 3 */
    SECKEYPublicKey *peerKey;					/* ssl3 only */

    SSLSignType      authAlgorithm;
    PRUint32         authKeyBits;
    SSLKEAType       keaType;
    PRUint32         keaKeyBits;

    /*
    ** Procs used for SID cache (nonce) management. 
    ** Different implementations exist for clients/servers 
    ** The lookup proc is only used for servers.  Baloney!
    */
    sslSessionIDCacheFunc     cache;				/* ssl 2 & 3 */
    sslSessionIDUncacheFunc   uncache;				/* ssl 2 & 3 */

    /*
    ** everything below here is for ssl2 only.
    ** This stuff is equivalent to SSL3's "spec", and is protected by the 
    ** same "Spec Lock" as used for SSL3's specs.
    */
    uint32           sendSequence;		/*xmitBufLock*/	/* ssl2 only */
    uint32           rcvSequence;		/*recvBufLock*/	/* ssl2 only */

    /* Hash information; used for one-way-hash functions (MD2, MD5, etc.) */
    const SECHashObject   *hash;		/* Spec Lock */ /* ssl2 only */
    void            *hashcx;			/* Spec Lock */	/* ssl2 only */

    SECItem          sendSecret;		/* Spec Lock */	/* ssl2 only */
    SECItem          rcvSecret;			/* Spec Lock */	/* ssl2 only */

    /* Session cypher contexts; one for each direction */
    void            *readcx;			/* Spec Lock */	/* ssl2 only */
    void            *writecx;			/* Spec Lock */	/* ssl2 only */
    SSLCipher        enc;			/* Spec Lock */	/* ssl2 only */
    SSLCipher        dec;			/* Spec Lock */	/* ssl2 only */
    void           (*destroy)(void *, PRBool);	/* Spec Lock */	/* ssl2 only */

    /* Blocking information for the session cypher */
    int              blockShift;		/* Spec Lock */	/* ssl2 only */
    int              blockSize;			/* Spec Lock */	/* ssl2 only */

    /* These are used during a connection handshake */
    sslConnectInfo   ci;					/* ssl 2 & 3 */

};


/*
** SSL Socket struct
**
** Protection:  XXX
*/
struct sslSocketStr {
    PRFileDesc *	fd;

    /* Pointer to operations vector for this socket */
    const sslSocketOps * ops;

    /* State flags */
    unsigned int     useSocks		: 1;
    unsigned int     useSecurity	: 1;
    unsigned int     requestCertificate	: 1;
    unsigned int     requireCertificate	: 2;
    unsigned int     handshakeAsClient	: 1;
    unsigned int     handshakeAsServer	: 1;
    unsigned int     enableSSL2		: 1;

    unsigned int     enableSSL3		: 1;
    unsigned int     enableTLS		: 1;
    unsigned int     clientAuthRequested: 1;
    unsigned int     noCache		: 1;
    unsigned int     fdx		: 1; /* simultaneous R/W threads */
    unsigned int     v2CompatibleHello	: 1; /* Send v3+ client hello in v2 format */
    unsigned int     detectRollBack   	: 1; /* Detect rollback to SSL v3 */
    unsigned int     firstHsDone	: 1; /* first handshake is complete. */

    unsigned int     recvdCloseNotify	: 1; /* received SSL EOF. */
    unsigned int     lastWriteBlocked   : 1;
    unsigned int     TCPconnected       : 1;
    unsigned int     handshakeBegun     : 1;
    unsigned int     delayDisabled      : 1; /* Nagle delay disabled */

    /* version of the protocol to use */
    SSL3ProtocolVersion version;
    SSL3ProtocolVersion clientHelloVersion; /* version sent in client hello. */

    sslSecurityInfo  sec;		/* not a pointer any more */

    /* protected by firstHandshakeLock AND (in ssl3) ssl3HandshakeLock. */
    const char      *url;				/* ssl 2 & 3 */

    /* Gather object used for gathering data */
    sslGather        gs;				/*recvBufLock*/

    sslHandshakeFunc handshake;				/*firstHandshakeLock*/
    sslHandshakeFunc nextHandshake;			/*firstHandshakeLock*/
    sslHandshakeFunc securityHandshake;			/*firstHandshakeLock*/

    sslBuffer        saveBuf;				/*xmitBufLock*/
    sslBuffer        pendingBuf;			/*xmitBufLock*/

    /* the following variable is only used with socks or other proxies. */
    char *           peerID;	/* String uniquely identifies target server. */

    ssl3State *      ssl3;
    unsigned char *  cipherSpecs;
    unsigned int     sizeCipherSpecs;
const unsigned char *  preferredCipher;

    /* Configuration state for server sockets */
    /* server cert and key for each KEA type */
    sslServerCerts        serverCerts[kt_kea_size];

    ssl3KeyPair *         stepDownKeyPair;	/* RSA step down keys */

    /* Callbacks */
    SSLAuthCertificate        authCertificate;
    void                     *authCertificateArg;
    SSLGetClientAuthData      getClientAuthData;
    void                     *getClientAuthDataArg;
    SSLBadCertHandler         handleBadCert;
    void                     *badCertArg;
    SSLHandshakeCallback      handshakeCallback;
    void                     *handshakeCallbackData;
    void                     *pkcs11PinArg;

    PRIntervalTime            rTimeout; /* timeout for NSPR I/O */
    PRIntervalTime            wTimeout; /* timeout for NSPR I/O */
    PRIntervalTime            cTimeout; /* timeout for NSPR I/O */

    PZLock *      recvLock;	/* lock against multiple reader threads. */
    PZLock *      sendLock;	/* lock against multiple sender threads. */

    PZMonitor *   recvBufLock;	/* locks low level recv buffers. */
    PZMonitor *   xmitBufLock;	/* locks low level xmit buffers. */

    /* Only one thread may operate on the socket until the initial handshake
    ** is complete.  This Monitor ensures that.  Since SSL2 handshake is
    ** only done once, this is also effectively the SSL2 handshake lock.
    */
    PZMonitor *   firstHandshakeLock; 

    /* This monitor protects the ssl3 handshake state machine data.
    ** Only one thread (reader or writer) may be in the ssl3 handshake state
    ** machine at any time.  */
    PZMonitor *   ssl3HandshakeLock;

    /* reader/writer lock, protects the secret data needed to encrypt and MAC
    ** outgoing records, and to decrypt and MAC check incoming ciphertext 
    ** records.  */
    NSSRWLock *   specLock;

    /* handle to perm cert db (and implicitly to the temp cert db) used 
    ** with this socket. 
    */
    CERTCertDBHandle * dbHandle;

    PRThread *  writerThread;   /* thread holds SSL_LOCK_WRITER lock */

    PRUint16	shutdownHow; 	/* See ssl_SHUTDOWN defines below. */

    PRUint16	allowedByPolicy;          /* copy of global policy bits. */
    PRUint16	maybeAllowedByPolicy;     /* copy of global policy bits. */
    PRUint16	chosenPreference;         /* SSL2 cipher preferences. */

    sslHandshakingType handshaking;

    ssl3CipherSuiteCfg cipherSuites[ssl_V3_SUITES_IMPLEMENTED];
    ssl3KeyPair *         ephemeralECDHKeyPair; /* for ECDHE-* handshake */
};



/* All the global data items declared here should be protected using the 
** ssl_global_data_lock, which is a reader/writer lock.
*/
extern NSSRWLock *             ssl_global_data_lock;
extern char                    ssl_debug;
extern char                    ssl_trace;
extern CERTDistNames *         ssl3_server_ca_list;
extern PRUint32                ssl_sid_timeout;
extern PRUint32                ssl3_sid_timeout;
extern PRBool                  ssl3_global_policy_some_restricted;

extern const char * const      ssl_cipherName[];
extern const char * const      ssl3_cipherName[];

extern sslSessionIDLookupFunc  ssl_sid_lookup;
extern sslSessionIDCacheFunc   ssl_sid_cache;
extern sslSessionIDUncacheFunc ssl_sid_uncache;

/************************************************************************/

SEC_BEGIN_PROTOS

/* Implementation of ops for default (non socks, non secure) case */
extern int ssl_DefConnect(sslSocket *ss, const PRNetAddr *addr);
extern PRFileDesc *ssl_DefAccept(sslSocket *ss, PRNetAddr *addr);
extern int ssl_DefBind(sslSocket *ss, const PRNetAddr *addr);
extern int ssl_DefListen(sslSocket *ss, int backlog);
extern int ssl_DefShutdown(sslSocket *ss, int how);
extern int ssl_DefClose(sslSocket *ss);
extern int ssl_DefRecv(sslSocket *ss, unsigned char *buf, int len, int flags);
extern int ssl_DefSend(sslSocket *ss, const unsigned char *buf,
		       int len, int flags);
extern int ssl_DefRead(sslSocket *ss, unsigned char *buf, int len);
extern int ssl_DefWrite(sslSocket *ss, const unsigned char *buf, int len);
extern int ssl_DefGetpeername(sslSocket *ss, PRNetAddr *name);
extern int ssl_DefGetsockname(sslSocket *ss, PRNetAddr *name);
extern int ssl_DefGetsockopt(sslSocket *ss, PRSockOption optname,
			     void *optval, PRInt32 *optlen);
extern int ssl_DefSetsockopt(sslSocket *ss, PRSockOption optname,
			     const void *optval, PRInt32 optlen);

/* Implementation of ops for socks only case */
extern int ssl_SocksConnect(sslSocket *ss, const PRNetAddr *addr);
extern PRFileDesc *ssl_SocksAccept(sslSocket *ss, PRNetAddr *addr);
extern int ssl_SocksBind(sslSocket *ss, const PRNetAddr *addr);
extern int ssl_SocksListen(sslSocket *ss, int backlog);
extern int ssl_SocksGetsockname(sslSocket *ss, PRNetAddr *name);
extern int ssl_SocksRecv(sslSocket *ss, unsigned char *buf, int len, int flags);
extern int ssl_SocksSend(sslSocket *ss, const unsigned char *buf,
			 int len, int flags);
extern int ssl_SocksRead(sslSocket *ss, unsigned char *buf, int len);
extern int ssl_SocksWrite(sslSocket *ss, const unsigned char *buf, int len);

/* Implementation of ops for secure only case */
extern int ssl_SecureConnect(sslSocket *ss, const PRNetAddr *addr);
extern PRFileDesc *ssl_SecureAccept(sslSocket *ss, PRNetAddr *addr);
extern int ssl_SecureRecv(sslSocket *ss, unsigned char *buf,
			  int len, int flags);
extern int ssl_SecureSend(sslSocket *ss, const unsigned char *buf,
			  int len, int flags);
extern int ssl_SecureRead(sslSocket *ss, unsigned char *buf, int len);
extern int ssl_SecureWrite(sslSocket *ss, const unsigned char *buf, int len);
extern int ssl_SecureShutdown(sslSocket *ss, int how);
extern int ssl_SecureClose(sslSocket *ss);

/* Implementation of ops for secure socks case */
extern int ssl_SecureSocksConnect(sslSocket *ss, const PRNetAddr *addr);
extern PRFileDesc *ssl_SecureSocksAccept(sslSocket *ss, PRNetAddr *addr);
extern PRFileDesc *ssl_FindTop(sslSocket *ss);

/* Gather funcs. */
extern sslGather * ssl_NewGather(void);
extern SECStatus   ssl_InitGather(sslGather *gs);
extern void        ssl_DestroyGather(sslGather *gs);
extern int         ssl2_GatherData(sslSocket *ss, sslGather *gs, int flags);
extern int         ssl2_GatherRecord(sslSocket *ss, int flags);
extern SECStatus   ssl_GatherRecord1stHandshake(sslSocket *ss);

extern SECStatus   ssl2_HandleClientHelloMessage(sslSocket *ss);
extern SECStatus   ssl2_HandleServerHelloMessage(sslSocket *ss);
extern int         ssl2_StartGatherBytes(sslSocket *ss, sslGather *gs, 
                                         unsigned int count);

extern SECStatus   ssl_CreateSecurityInfo(sslSocket *ss);
extern SECStatus   ssl_CopySecurityInfo(sslSocket *ss, sslSocket *os);
extern void        ssl_ResetSecurityInfo(sslSecurityInfo *sec);
extern void        ssl_DestroySecurityInfo(sslSecurityInfo *sec);

extern sslSocket * ssl_DupSocket(sslSocket *old);

extern void        ssl_PrintBuf(sslSocket *ss, const char *msg, const void *cp, int len);
extern void        ssl_DumpMsg(sslSocket *ss, unsigned char *bp, unsigned len);

extern int         ssl_SendSavedWriteData(sslSocket *ss, sslBuffer *buf,
				          sslSendFunc fp);
extern SECStatus ssl_SaveWriteData(sslSocket *ss, sslBuffer *buf, 
                                   const void* p, unsigned int l);
extern SECStatus ssl2_BeginClientHandshake(sslSocket *ss);
extern SECStatus ssl2_BeginServerHandshake(sslSocket *ss);
extern int       ssl_Do1stHandshake(sslSocket *ss);

extern SECStatus sslBuffer_Grow(sslBuffer *b, unsigned int newLen);

extern void      ssl2_UseClearSendFunc(sslSocket *ss);
extern void      ssl_ChooseSessionIDProcs(sslSecurityInfo *sec);

extern sslSessionID *ssl_LookupSID(const PRIPv6Addr *addr, PRUint16 port, 
                                   const char *peerID, const char *urlSvrName);
extern void      ssl_FreeSID(sslSessionID *sid);

extern int       ssl3_SendApplicationData(sslSocket *ss, const PRUint8 *in,
				          int len, int flags);

extern PRBool    ssl_FdIsBlocking(PRFileDesc *fd);

extern PRBool    ssl_SocketIsBlocking(sslSocket *ss);

extern void      ssl_SetAlwaysBlock(sslSocket *ss);

extern SECStatus ssl_EnableNagleDelay(sslSocket *ss, PRBool enabled);

#define SSL_LOCK_READER(ss)		if (ss->recvLock) PZ_Lock(ss->recvLock)
#define SSL_UNLOCK_READER(ss)	if (ss->recvLock) PZ_Unlock(ss->recvLock)
#define SSL_LOCK_WRITER(ss)		if (ss->sendLock) PZ_Lock(ss->sendLock)
#define SSL_UNLOCK_WRITER(ss)	if (ss->sendLock) PZ_Unlock(ss->sendLock)

#define ssl_Get1stHandshakeLock(ss)    PZ_EnterMonitor((ss)->firstHandshakeLock)
#define ssl_Release1stHandshakeLock(ss) PZ_ExitMonitor((ss)->firstHandshakeLock)
#define ssl_Have1stHandshakeLock(ss)	PZ_InMonitor(  (ss)->firstHandshakeLock)

#define ssl_GetSSL3HandshakeLock(ss)	PZ_EnterMonitor((ss)->ssl3HandshakeLock)
#define ssl_ReleaseSSL3HandshakeLock(ss) PZ_ExitMonitor((ss)->ssl3HandshakeLock)
#define ssl_HaveSSL3HandshakeLock(ss)	PZ_InMonitor(   (ss)->ssl3HandshakeLock)

#define ssl_GetSpecReadLock(ss)		NSSRWLock_LockRead(     (ss)->specLock)
#define ssl_ReleaseSpecReadLock(ss)	NSSRWLock_UnlockRead(   (ss)->specLock)

#define ssl_GetSpecWriteLock(ss)	NSSRWLock_LockWrite(  (ss)->specLock)
#define ssl_ReleaseSpecWriteLock(ss)	NSSRWLock_UnlockWrite((ss)->specLock)
#define ssl_HaveSpecWriteLock(ss)	NSSRWLock_HaveWriteLock((ss)->specLock)

#define ssl_GetRecvBufLock(ss)		PZ_EnterMonitor((ss)->recvBufLock)
#define ssl_ReleaseRecvBufLock(ss)	PZ_ExitMonitor( (ss)->recvBufLock)
#define ssl_HaveRecvBufLock(ss)		PZ_InMonitor(   (ss)->recvBufLock)

#define ssl_GetXmitBufLock(ss)		PZ_EnterMonitor((ss)->xmitBufLock)
#define ssl_ReleaseXmitBufLock(ss)	PZ_ExitMonitor( (ss)->xmitBufLock)
#define ssl_HaveXmitBufLock(ss)		PZ_InMonitor(   (ss)->xmitBufLock)


/* These functions are called from secnav, even though they're "private". */

extern int ssl2_SendErrorMessage(struct sslSocketStr *ss, int error);
extern int SSL_RestartHandshakeAfterServerCert(struct sslSocketStr *ss);
extern int SSL_RestartHandshakeAfterCertReq(struct sslSocketStr *ss,
					    CERTCertificate *cert,
					    SECKEYPrivateKey *key,
					    CERTCertificateList *certChain);
extern sslSocket *ssl_FindSocket(PRFileDesc *fd);
extern void ssl_FreeSocket(struct sslSocketStr *ssl);
extern SECStatus SSL3_SendAlert(sslSocket *ss, SSL3AlertLevel level,
				SSL3AlertDescription desc);

extern int ssl2_RestartHandshakeAfterCertReq(sslSocket *          ss,
					     CERTCertificate *    cert, 
					     SECKEYPrivateKey *   key);

extern SECStatus ssl3_RestartHandshakeAfterCertReq(sslSocket *    ss,
					     CERTCertificate *    cert, 
					     SECKEYPrivateKey *   key,
					     CERTCertificateList *certChain);

extern int ssl2_RestartHandshakeAfterServerCert(sslSocket *ss);
extern int ssl3_RestartHandshakeAfterServerCert(sslSocket *ss);

/*
 * for dealing with SSL 3.0 clients sending SSL 2.0 format hellos
 */
extern SECStatus ssl3_HandleV2ClientHello(
    sslSocket *ss, unsigned char *buffer, int length);
extern SECStatus ssl3_StartHandshakeHash(
    sslSocket *ss, unsigned char *buf, int length);

/*
 * SSL3 specific routines
 */
SECStatus ssl3_SendClientHello(sslSocket *ss);

/*
 * input into the SSL3 machinery from the actualy network reading code
 */
SECStatus ssl3_HandleRecord(
    sslSocket *ss, SSL3Ciphertext *cipher, sslBuffer *out);

int ssl3_GatherAppDataRecord(sslSocket *ss, int flags);
int ssl3_GatherCompleteHandshake(sslSocket *ss, int flags);
/*
 * When talking to export clients or using export cipher suites, servers 
 * with public RSA keys larger than 512 bits need to use a 512-bit public
 * key, signed by the larger key.  The smaller key is a "step down" key.
 * Generate that key pair and keep it around.
 */
extern SECStatus ssl3_CreateRSAStepDownKeys(sslSocket *ss);

#ifdef NSS_ENABLE_ECC
extern SECStatus ssl3_CreateECDHEphemeralKeys(sslSocket *ss);
#endif /* NSS_ENABLE_ECC */

extern SECStatus ssl3_CipherPrefSetDefault(ssl3CipherSuite which, PRBool on);
extern SECStatus ssl3_CipherPrefGetDefault(ssl3CipherSuite which, PRBool *on);
extern SECStatus ssl2_CipherPrefSetDefault(PRInt32 which, PRBool enabled);
extern SECStatus ssl2_CipherPrefGetDefault(PRInt32 which, PRBool *enabled);

extern SECStatus ssl3_CipherPrefSet(sslSocket *ss, ssl3CipherSuite which, PRBool on);
extern SECStatus ssl3_CipherPrefGet(sslSocket *ss, ssl3CipherSuite which, PRBool *on);
extern SECStatus ssl2_CipherPrefSet(sslSocket *ss, PRInt32 which, PRBool enabled);
extern SECStatus ssl2_CipherPrefGet(sslSocket *ss, PRInt32 which, PRBool *enabled);

extern SECStatus ssl3_SetPolicy(ssl3CipherSuite which, PRInt32 policy);
extern SECStatus ssl3_GetPolicy(ssl3CipherSuite which, PRInt32 *policy);
extern SECStatus ssl2_SetPolicy(PRInt32 which, PRInt32 policy);
extern SECStatus ssl2_GetPolicy(PRInt32 which, PRInt32 *policy);

extern void      ssl2_InitSocketPolicy(sslSocket *ss);
extern void      ssl3_InitSocketPolicy(sslSocket *ss);

extern SECStatus ssl3_ConstructV2CipherSpecsHack(sslSocket *ss,
						 unsigned char *cs, int *size);

extern SECStatus ssl3_RedoHandshake(sslSocket *ss, PRBool flushCache);

extern void ssl3_DestroySSL3Info(ssl3State *ssl3);

extern SECStatus ssl3_NegotiateVersion(sslSocket *ss, 
                                       SSL3ProtocolVersion peerVersion);

extern SECStatus ssl_GetPeerInfo(sslSocket *ss);

/* Construct a new NSPR socket for the app to use */
extern PRFileDesc *ssl_NewPRSocket(sslSocket *ss, PRFileDesc *fd);
extern void ssl_FreePRSocket(PRFileDesc *fd);

/* Internal config function so SSL2 can initialize the present state of 
 * various ciphers */
extern int ssl3_config_match_init(sslSocket *);


/* Create a new ref counted key pair object from two keys. */
extern ssl3KeyPair * ssl3_NewKeyPair( SECKEYPrivateKey * privKey, 
                                      SECKEYPublicKey * pubKey);

/* get a new reference (bump ref count) to an ssl3KeyPair. */
extern ssl3KeyPair * ssl3_GetKeyPairRef(ssl3KeyPair * keyPair);

/* Decrement keypair's ref count and free if zero. */
extern void ssl3_FreeKeyPair(ssl3KeyPair * keyPair);

/* calls for accessing wrapping keys across processes. */
extern PRBool
ssl_GetWrappingKey( PRInt32                   symWrapMechIndex,
                    SSL3KEAType               exchKeyType, 
		    SSLWrappedSymWrappingKey *wswk);

/* The caller passes in the new value it wants
 * to set.  This code tests the wrapped sym key entry in the file on disk.  
 * If it is uninitialized, this function writes the caller's value into 
 * the disk entry, and returns false.  
 * Otherwise, it overwrites the caller's wswk with the value obtained from 
 * the disk, and returns PR_TRUE.  
 * This is all done while holding the locks/semaphores necessary to make 
 * the operation atomic.
 */
extern PRBool
ssl_SetWrappingKey(SSLWrappedSymWrappingKey *wswk);

/* get rid of the symmetric wrapping key references. */
extern SECStatus SSL3_ShutdownServerCache(void);

extern void ssl_InitClientSessionCacheLock(void);

extern void ssl_InitSymWrapKeysLock(void);

/********************** misc calls *********************/

extern int ssl_MapLowLevelError(int hiLevelError);

extern PRUint32 ssl_Time(void);

/* emulation of NSPR routines. */
extern PRInt32 
ssl_EmulateAcceptRead(	PRFileDesc *   sd, 
			PRFileDesc **  nd,
			PRNetAddr **   raddr, 
			void *         buf, 
			PRInt32        amount, 
			PRIntervalTime timeout);
extern PRInt32 
ssl_EmulateTransmitFile(    PRFileDesc *        sd, 
			    PRFileDesc *        fd,
			    const void *        headers, 
			    PRInt32             hlen, 
			    PRTransmitFileFlags flags,
			    PRIntervalTime      timeout);
extern PRInt32 
ssl_EmulateSendFile( PRFileDesc *        sd, 
		     PRSendFileData *    sfd,
                     PRTransmitFileFlags flags, 
		     PRIntervalTime      timeout);

#ifdef TRACE
#define SSL_TRACE(msg) ssl_Trace msg
#else
#define SSL_TRACE(msg)
#endif

void ssl_Trace(const char *format, ...);

SEC_END_PROTOS

#ifdef XP_OS2_VACPP
#include <process.h>
#endif

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#define SSL_GETPID getpid
#elif defined(_WIN32_WCE)
#define SSL_GETPID GetCurrentProcessId
#elif defined(WIN32)
extern int __cdecl _getpid(void);
#define SSL_GETPID _getpid
#else
#define SSL_GETPID() 0
#endif

#endif /* __sslimpl_h_ */
