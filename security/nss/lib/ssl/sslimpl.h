/*
 * This file is PRIVATE to SSL and should be the first thing included by
 * any SSL implementation file.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "prclist.h"

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
#define calg_fortezza	ssl_calg_fortezza /* deprecated, must preserve */
#define calg_aes	ssl_calg_aes
#define calg_camellia	ssl_calg_camellia
#define calg_seed	ssl_calg_seed
#define calg_aes_gcm    ssl_calg_aes_gcm

#define mac_null	ssl_mac_null
#define mac_md5 	ssl_mac_md5
#define mac_sha 	ssl_mac_sha
#define hmac_md5	ssl_hmac_md5
#define hmac_sha	ssl_hmac_sha
#define hmac_sha256	ssl_hmac_sha256
#define mac_aead	ssl_mac_aead

#define SET_ERROR_CODE		/* reminder */
#define SEND_ALERT		/* reminder */
#define TEST_FOR_FAILURE	/* reminder */
#define DEAL_WITH_FAILURE	/* reminder */

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

#include "private/pprthred.h"	/* for PR_InMonitor() */
#define ssl_InMonitor(m) PZ_InMonitor(m)

#define LSB(x) ((unsigned char) ((x) & 0xff))
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

#define SSL3_RSA_PMS_LENGTH 48
#define SSL3_MASTER_SECRET_LENGTH 48

/* number of wrap mechanisms potentially used to wrap master secrets. */
#define SSL_NUM_WRAP_MECHS              16

/* This makes the cert cache entry exactly 4k. */
#define SSL_MAX_CACHED_CERT_LEN		4060

#define NUM_MIXERS                      9

/* Mask of the 25 named curves we support. */
#define SSL3_ALL_SUPPORTED_CURVES_MASK 0x3fffffe
/* Mask of only 3 curves, suite B */
#define SSL3_SUITE_B_SUPPORTED_CURVES_MASK 0x3800000

#ifndef BPB
#define BPB 8 /* Bits Per Byte */
#endif

#define EXPORT_RSA_KEY_LENGTH 64	/* bytes */

#define INITIAL_DTLS_TIMEOUT_MS   1000  /* Default value from RFC 4347 = 1s*/
#define MAX_DTLS_TIMEOUT_MS      60000  /* 1 minute */
#define DTLS_FINISHED_TIMER_MS  120000  /* Time to wait in FINISHED state */

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

/* registerable callback function that either appends extension to buffer
 * or returns length of data that it would have appended.
 */
typedef PRInt32 (*ssl3HelloExtensionSenderFunc)(sslSocket *ss, PRBool append,
						PRUint32 maxBytes);

/* registerable callback function that handles a received extension, 
 * of the given type.
 */
typedef SECStatus (* ssl3HelloExtensionHandlerFunc)(sslSocket *ss,
						    PRUint16   ex_type,
                                                    SECItem *  data);

/* row in a table of hello extension senders */
typedef struct {
    PRInt32                      ex_type;
    ssl3HelloExtensionSenderFunc ex_sender;
} ssl3HelloExtensionSender;

/* row in a table of hello extension handlers */
typedef struct {
    PRInt32                       ex_type;
    ssl3HelloExtensionHandlerFunc ex_handler;
} ssl3HelloExtensionHandler;

extern SECStatus 
ssl3_RegisterServerHelloExtensionSender(sslSocket *ss, PRUint16 ex_type,
				        ssl3HelloExtensionSenderFunc cb);

extern PRInt32
ssl3_CallHelloExtensionSenders(sslSocket *ss, PRBool append, PRUint32 maxBytes,
                               const ssl3HelloExtensionSender *sender);

extern unsigned int
ssl3_CalculatePaddingExtensionLength(unsigned int clientHelloLength);

extern PRInt32
ssl3_AppendPaddingExtension(sslSocket *ss, unsigned int extensionLen,
			    PRUint32 maxBytes);

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
#define ssl_SEND_FLAG_USE_EPOCH		0x10000000 /* DTLS only */
#define ssl_SEND_FLAG_NO_RETRANSMIT	0x08000000 /* DTLS only */
#define ssl_SEND_FLAG_CAP_RECORD_VERSION \
					0x04000000 /* TLS only */
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

#ifndef NSS_DISABLE_ECC
#define ssl_V3_SUITES_IMPLEMENTED 61
#else
#define ssl_V3_SUITES_IMPLEMENTED 37
#endif /* NSS_DISABLE_ECC */

#define MAX_DTLS_SRTP_CIPHER_SUITES 4

typedef struct sslOptionsStr {
    /* If SSL_SetNextProtoNego has been called, then this contains the
     * list of supported protocols. */
    SECItem nextProtoNego;

    unsigned int useSecurity		: 1;  /*  1 */
    unsigned int useSocks		: 1;  /*  2 */
    unsigned int requestCertificate	: 1;  /*  3 */
    unsigned int requireCertificate	: 2;  /*  4-5 */
    unsigned int handshakeAsClient	: 1;  /*  6 */
    unsigned int handshakeAsServer	: 1;  /*  7 */
    unsigned int enableSSL2		: 1;  /*  8 */
    unsigned int unusedBit9		: 1;  /*  9 */
    unsigned int unusedBit10		: 1;  /* 10 */
    unsigned int noCache		: 1;  /* 11 */
    unsigned int fdx			: 1;  /* 12 */
    unsigned int v2CompatibleHello	: 1;  /* 13 */
    unsigned int detectRollBack  	: 1;  /* 14 */
    unsigned int noStepDown             : 1;  /* 15 */
    unsigned int bypassPKCS11           : 1;  /* 16 */
    unsigned int noLocks                : 1;  /* 17 */
    unsigned int enableSessionTickets   : 1;  /* 18 */
    unsigned int enableDeflate          : 1;  /* 19 */
    unsigned int enableRenegotiation    : 2;  /* 20-21 */
    unsigned int requireSafeNegotiation : 1;  /* 22 */
    unsigned int enableFalseStart       : 1;  /* 23 */
    unsigned int cbcRandomIV            : 1;  /* 24 */
    unsigned int enableOCSPStapling     : 1;  /* 25 */
    unsigned int enableNPN              : 1;  /* 26 */
    unsigned int enableALPN             : 1;  /* 27 */
} sslOptions;

typedef enum { sslHandshakingUndetermined = 0,
	       sslHandshakingAsClient,
	       sslHandshakingAsServer 
} sslHandshakingType;

typedef struct sslServerCertsStr {
    /* Configuration state for server sockets */
    CERTCertificate *     serverCert;
    CERTCertificateList * serverCertChain;
    ssl3KeyPair *         serverKeyPair;
    unsigned int          serverKeyBits;
} sslServerCerts;

#define SERVERKEY serverKeyPair->privKey

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
    ** For DTLS it is 13.
    */
    unsigned char hdr[13];				/* ssl 2 & 3 or dtls */

    /* Buffer for DTLS data read off the wire as a single datagram */
    sslBuffer     dtlsPacket;

    /* the start of the buffered DTLS record in dtlsPacket */
    unsigned int  dtlsPacketOffset;
};

/* sslGather.state */
#define GS_INIT		0
#define GS_HEADER	1
#define GS_MAC		2
#define GS_DATA		3
#define GS_PAD		4



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
    cipher_aes_128,
    cipher_aes_256,
    cipher_camellia_128,
    cipher_camellia_256,
    cipher_seed,
    cipher_aes_128_gcm,
    cipher_missing              /* reserved for no such supported cipher */
    /* This enum must match ssl3_cipherName[] in ssl3con.c.  */
} SSL3BulkCipher;

typedef enum { type_stream, type_block, type_aead } CipherType;

#define MAX_IV_LENGTH 24

/*
 * Do not depend upon 64 bit arithmetic in the underlying machine. 
 */
typedef struct {
    PRUint32         high;
    PRUint32         low;
} SSL3SequenceNumber;

typedef PRUint16 DTLSEpoch;

typedef void (*DTLSTimerCb)(sslSocket *);

#define MAX_MAC_CONTEXT_BYTES 400  /* 400 is large enough for MD5, SHA-1, and
                                    * SHA-256. For SHA-384 support, increase
                                    * it to 712. */
#define MAX_MAC_CONTEXT_LLONGS (MAX_MAC_CONTEXT_BYTES / 8)

#define MAX_CIPHER_CONTEXT_BYTES 2080
#define MAX_CIPHER_CONTEXT_LLONGS (MAX_CIPHER_CONTEXT_BYTES / 8)

typedef struct {
    SSL3Opaque        wrapped_master_secret[48];
    PRUint16          wrapped_master_secret_len;
    PRUint8           msIsWrapped;
    PRUint8           resumable;
} ssl3SidKeys; /* 52 bytes */

typedef struct {
    PK11SymKey  *write_key;
    PK11SymKey  *write_mac_key;
    PK11Context *write_mac_context;
    SECItem     write_key_item;
    SECItem     write_iv_item;
    SECItem     write_mac_key_item;
    SSL3Opaque  write_iv[MAX_IV_LENGTH];
    PRUint64    cipher_context[MAX_CIPHER_CONTEXT_LLONGS];
} ssl3KeyMaterial;

typedef SECStatus (*SSLCipher)(void *               context, 
                               unsigned char *      out,
			       int *                outlen, 
			       int                  maxout, 
			       const unsigned char *in,
			       int                  inlen);
typedef SECStatus (*SSLAEADCipher)(
			       ssl3KeyMaterial *    keys,
			       PRBool               doDecrypt,
			       unsigned char *      out,
			       int *                outlen,
			       int                  maxout,
			       const unsigned char *in,
			       int                  inlen,
			       const unsigned char *additionalData,
			       int                  additionalDataLen);
typedef SECStatus (*SSLCompressor)(void *               context,
                                   unsigned char *      out,
                                   int *                outlen,
                                   int                  maxout,
                                   const unsigned char *in,
                                   int                  inlen);
typedef SECStatus (*SSLDestroy)(void *context, PRBool freeit);

/* The DTLS anti-replay window. Defined here because we need it in
 * the cipher spec. Note that this is a ring buffer but left and
 * right represent the true window, with modular arithmetic used to
 * map them onto the buffer.
 */
#define DTLS_RECVD_RECORDS_WINDOW 1024 /* Packets; approximate
				        * Must be divisible by 8
				        */
typedef struct DTLSRecvdRecordsStr {
    unsigned char data[DTLS_RECVD_RECORDS_WINDOW/8];
    PRUint64 left;
    PRUint64 right;
} DTLSRecvdRecords;

/*
** These are the "specs" in the "ssl3" struct.
** Access to the pointers to these specs, and all the specs' contents
** (direct and indirect) is protected by the reader/writer lock ss->specLock.
*/
typedef struct {
    const ssl3BulkCipherDef *cipher_def;
    const ssl3MACDef * mac_def;
    SSLCompressionMethod compression_method;
    int                mac_size;
    SSLCipher          encode;
    SSLCipher          decode;
    SSLAEADCipher      aead;
    SSLDestroy         destroy;
    void *             encodeContext;
    void *             decodeContext;
    SSLCompressor      compressor;    /* Don't name these fields compress */
    SSLCompressor      decompressor;  /* and uncompress because zconf.h   */
                                      /* may define them as macros.       */ 
    SSLDestroy         destroyCompressContext;
    void *             compressContext;
    SSLDestroy         destroyDecompressContext;
    void *             decompressContext;
    PRBool             bypassCiphers;	/* did double bypass (at least) */
    PK11SymKey *       master_secret;
    SSL3SequenceNumber write_seq_num;
    SSL3SequenceNumber read_seq_num;
    SSL3ProtocolVersion version;
    ssl3KeyMaterial    client;
    ssl3KeyMaterial    server;
    SECItem            msItem;
    unsigned char      key_block[NUM_MIXERS * MD5_LENGTH];
    unsigned char      raw_master_secret[56];
    SECItem            srvVirtName;    /* for server: name that was negotiated
                                        * with a client. For client - is
                                        * always set to NULL.*/
    DTLSEpoch          epoch;
    DTLSRecvdRecords   recvdRecords;
} ssl3CipherSpec;

typedef enum {	never_cached, 
		in_client_cache, 
		in_server_cache, 
		invalid_cache		/* no longer in any cache. */
} Cached;

struct sslSessionIDStr {
    /* The global cache lock must be held when accessing these members when the
     * sid is in any cache.
     */
    sslSessionID *        next;   /* chain used for client sockets, only */
    Cached                cached;
    int                   references;
    PRUint32              lastAccessTime;	/* seconds since Jan 1, 1970 */

    /* The rest of the members, except for the members of u.ssl3.locked, may
     * be modified only when the sid is not in any cache.
     */

    CERTCertificate *     peerCert;
    SECItemArray          peerCertStatus; /* client only */
    const char *          peerID;     /* client only */
    const char *          urlSvrName; /* client only */
    CERTCertificate *     localCert;

    PRIPv6Addr            addr;
    PRUint16              port;

    SSL3ProtocolVersion   version;

    PRUint32              creationTime;		/* seconds since Jan 1, 1970 */
    PRUint32              expirationTime;	/* seconds since Jan 1, 1970 */

    SSLSignType           authAlgorithm;
    PRUint32              authKeyBits;
    SSLKEAType            keaType;
    PRUint32              keaKeyBits;

    union {
	struct {
	    /* the V2 code depends upon the size of sessionID.  */
	    unsigned char         sessionID[SSL2_SESSIONID_BYTES];

	    /* Stuff used to recreate key and read/write cipher objects */
	    SECItem               masterKey;        /* never wrapped */
	    int                   cipherType;
	    SECItem               cipherArg;
	    int                   keyBits;
	    int                   secretKeyBits;
	} ssl2;
	struct {
	    /* values that are copied into the server's on-disk SID cache. */
	    PRUint8               sessionIDLength;
	    SSL3Opaque            sessionID[SSL3_SESSIONID_BYTES];

	    ssl3CipherSuite       cipherSuite;
	    SSLCompressionMethod  compression;
	    int                   policy;
	    ssl3SidKeys           keys;
	    CK_MECHANISM_TYPE     masterWrapMech;
				  /* mechanism used to wrap master secret */
            SSL3KEAType           exchKeyType;
				  /* key type used in exchange algorithm,
				   * and to wrap the sym wrapping key. */
#ifndef NSS_DISABLE_ECC
	    PRUint32              negotiatedECCurves;
#endif /* NSS_DISABLE_ECC */

	    /* The following values are NOT restored from the server's on-disk
	     * session cache, but are restored from the client's cache.
	     */
 	    PK11SymKey *      clientWriteKey;
	    PK11SymKey *      serverWriteKey;

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

	    SECItem           srvName;

	    /* This lock is lazily initialized by CacheSID when a sid is first
	     * cached. Before then, there is no need to lock anything because
	     * the sid isn't being shared by anything.
	     */
	    PRRWLock *lock;

	    /* The lock must be held while reading or writing these members
	     * because they change while the sid is cached.
	     */
	    struct {
		/* The session ticket, if we have one, is sent as an extension
		 * in the ClientHello message. This field is used only by
		 * clients. It is protected by lock when lock is non-null
		 * (after the sid has been added to the client session cache).
		 */
		NewSessionTicket sessionTicket;
	    } locked;
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
    int             tag_size;  /* authentication tag size for AEAD ciphers. */
    int             explicit_nonce_size;               /* for AEAD ciphers. */
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
    wait_certificate_status,
    wait_server_cert, 
    wait_server_key,
    wait_cert_request, 
    wait_hello_done,
    wait_new_session_ticket,
    idle_handshake
} SSL3WaitState;

/*
 * TLS extension related constants and data structures.
 */
typedef struct TLSExtensionDataStr       TLSExtensionData;
typedef struct SessionTicketDataStr      SessionTicketData;

struct TLSExtensionDataStr {
    /* registered callbacks that send server hello extensions */
    ssl3HelloExtensionSender serverSenders[SSL_MAX_EXTENSIONS];
    /* Keep track of the extensions that are negotiated. */
    PRUint16 numAdvertised;
    PRUint16 numNegotiated;
    PRUint16 advertised[SSL_MAX_EXTENSIONS];
    PRUint16 negotiated[SSL_MAX_EXTENSIONS];

    /* SessionTicket Extension related data. */
    PRBool ticketTimestampVerified;
    PRBool emptySessionTicket;
    PRBool sentSessionTicketInClientHello;

    /* SNI Extension related data
     * Names data is not coppied from the input buffer. It can not be
     * used outside the scope where input buffer is defined and that
     * is beyond ssl3_HandleClientHello function. */
    SECItem *sniNameArr;
    PRUint32 sniNameArrSize;
};

typedef SECStatus (*sslRestartTarget)(sslSocket *);

/*
** A DTLS queued message (potentially to be retransmitted)
*/
typedef struct DTLSQueuedMessageStr {
    PRCList link;         /* The linked list link */
    DTLSEpoch epoch;      /* The epoch to use */
    SSL3ContentType type; /* The message type */
    unsigned char *data;  /* The data */
    PRUint16 len;         /* The data length */
} DTLSQueuedMessage;

typedef enum {
    handshake_hash_unknown = 0,
    handshake_hash_combo = 1,  /* The MD5/SHA-1 combination */
    handshake_hash_single = 2  /* A single hash */
} SSL3HandshakeHashType;

/*
** This is the "hs" member of the "ssl3" struct.
** This entire struct is protected by ssl3HandshakeLock
*/
typedef struct SSL3HandshakeStateStr {
    SSL3Random            server_random;
    SSL3Random            client_random;
    SSL3WaitState         ws;

    /* This group of members is used for handshake running hashes. */
    SSL3HandshakeHashType hashType;
    sslBuffer             messages;    /* Accumulated handshake messages */
#ifndef NO_PKCS11_BYPASS
    /* Bypass mode:
     * SSL 3.0 - TLS 1.1 use both |md5_cx| and |sha_cx|. |md5_cx| is used for
     * MD5 and |sha_cx| for SHA-1.
     * TLS 1.2 and later use only |sha_cx|, for SHA-256. NOTE: When we support
     * SHA-384, increase MAX_MAC_CONTEXT_BYTES to 712. */
    PRUint64              md5_cx[MAX_MAC_CONTEXT_LLONGS];
    PRUint64              sha_cx[MAX_MAC_CONTEXT_LLONGS];
    const SECHashObject * sha_obj;
    /* The function prototype of sha_obj->clone() does not match the prototype
     * of the freebl <HASH>_Clone functions, so we need a dedicated function
     * pointer for the <HASH>_Clone function. */
    void (*sha_clone)(void *dest, void *src);
#endif
    /* PKCS #11 mode:
     * SSL 3.0 - TLS 1.1 use both |md5| and |sha|. |md5| is used for MD5 and
     * |sha| for SHA-1.
     * TLS 1.2 and later use only |sha|, for SHA-256. */
    /* NOTE: On the client side, TLS 1.2 and later use |md5| as a backup
     * handshake hash for generating client auth signatures. Confusingly, the
     * backup hash function is SHA-1. */
#define backupHash md5
    PK11Context *         md5;
    PK11Context *         sha;

const ssl3KEADef *        kea_def;
    ssl3CipherSuite       cipher_suite;
const ssl3CipherSuiteDef *suite_def;
    SSLCompressionMethod  compression;
    sslBuffer             msg_body;    /* protected by recvBufLock */
                               /* partial handshake message from record layer */
    unsigned int          header_bytes; 
                               /* number of bytes consumed from handshake */
                               /* message for message type and header length */
    SSL3HandshakeType     msg_type;
    unsigned long         msg_len;
    SECItem               ca_list;     /* used only by client */
    PRBool                isResuming;  /* are we resuming a session */
    PRBool                usedStepDownKey;  /* we did a server key exchange. */
    PRBool                sendingSCSV; /* instead of empty RI */
    sslBuffer             msgState;    /* current state for handshake messages*/
                                       /* protected by recvBufLock */

    /* The session ticket received in a NewSessionTicket message is temporarily
     * stored in newSessionTicket until the handshake is finished; then it is
     * moved to the sid.
     */
    PRBool                receivedNewSessionTicket;
    NewSessionTicket      newSessionTicket;

    PRUint16              finishedBytes; /* size of single finished below */
    union {
	TLSFinished       tFinished[2]; /* client, then server */
	SSL3Finished      sFinished[2];
	SSL3Opaque        data[72];
    }                     finishedMsgs;
#ifndef NSS_DISABLE_ECC
    PRUint32              negotiatedECCurves; /* bit mask */
#endif /* NSS_DISABLE_ECC */

    PRBool                authCertificatePending;
    /* Which function should SSL_RestartHandshake* call if we're blocked?
     * One of NULL, ssl3_SendClientSecondRound, ssl3_FinishHandshake,
     * or ssl3_AlwaysFail */
    sslRestartTarget      restartTarget;
    /* Shared state between ssl3_HandleFinished and ssl3_FinishHandshake */
    PRBool                cacheSID;

    PRBool                canFalseStart;   /* Can/did we False Start */

    /* clientSigAndHash contains the contents of the signature_algorithms
     * extension (if any) from the client. This is only valid for TLS 1.2
     * or later. */
    SSL3SignatureAndHashAlgorithm *clientSigAndHash;
    unsigned int          numClientSigAndHash;

    /* This group of values is used for DTLS */
    PRUint16              sendMessageSeq;  /* The sending message sequence
					    * number */
    PRCList               lastMessageFlight; /* The last message flight we
					      * sent */
    PRUint16              maxMessageSent;    /* The largest message we sent */
    PRUint16              recvMessageSeq;  /* The receiving message sequence
					    * number */
    sslBuffer             recvdFragments;  /* The fragments we have received in
					    * a bitmask */
    PRInt32               recvdHighWater;  /* The high water mark for fragments
					    * received. -1 means no reassembly
					    * in progress. */
    unsigned char         cookie[32];      /* The cookie */
    unsigned char         cookieLen;       /* The length of the cookie */
    PRIntervalTime        rtTimerStarted;  /* When the timer was started */
    DTLSTimerCb           rtTimerCb;       /* The function to call on expiry */
    PRUint32              rtTimeoutMs;     /* The length of the current timeout
					    * used for backoff (in ms) */
    PRUint32              rtRetries;       /* The retry counter */
} SSL3HandshakeState;



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

    CERTCertificate *    clientCertificate;  /* used by client */
    SECKEYPrivateKey *   clientPrivateKey;   /* used by client */
    CERTCertificateList *clientCertChain;    /* used by client */
    PRBool               sendEmptyCert;      /* used by client */

    int                  policy;
			/* This says what cipher suites we can do, and should 
			 * be either SSL_ALLOWED or SSL_RESTRICTED 
			 */
    PLArenaPool *        peerCertArena;
			    /* These are used to keep track of the peer CA */
    void *               peerCertChain;     
			    /* chain while we are trying to validate it.   */
    CERTDistNames *      ca_list; 
			    /* used by server.  trusted CAs for this socket. */
    PRBool               initialized;
    SSL3HandshakeState   hs;
    ssl3CipherSpec       specs[2];	/* one is current, one is pending. */

    /* In a client: if the server supports Next Protocol Negotiation, then
     * this is the protocol that was negotiated.
     */
    SECItem		 nextProto;
    SSLNextProtoState    nextProtoState;

    PRUint16             mtu;   /* Our estimate of the MTU */

    /* DTLS-SRTP cipher suite preferences (if any) */
    PRUint16             dtlsSRTPCiphers[MAX_DTLS_SRTP_CIPHER_SUITES];
    PRUint16             dtlsSRTPCipherCount;
    PRUint16             dtlsSRTPCipherSuite;	/* 0 if not selected */
};

#define DTLS_MAX_MTU  1500      /* Ethernet MTU but without subtracting the
				 * headers, so slightly larger than expected */
#define IS_DTLS(ss) (ss->protocolVariant == ssl_variant_datagram)

typedef struct {
    SSL3ContentType      type;
    SSL3ProtocolVersion  version;
    SSL3SequenceNumber   seq_num;  /* DTLS only */
    sslBuffer *          buf;
} SSL3Ciphertext;

struct ssl3KeyPairStr {
    SECKEYPrivateKey *    privKey;
    SECKEYPublicKey *     pubKey;
    PRInt32               refCount;	/* use PR_Atomic calls for this. */
};

typedef struct SSLWrappedSymWrappingKeyStr {
    SSL3Opaque        wrappedSymmetricWrappingkey[512];
    CK_MECHANISM_TYPE symWrapMechanism;  
		    /* unwrapped symmetric wrapping key uses this mechanism */
    CK_MECHANISM_TYPE asymWrapMechanism; 
		    /* mechanism used to wrap the SymmetricWrappingKey using
		     * server's public and/or private keys. */
    SSL3KEAType       exchKeyType;   /* type of keys used to wrap SymWrapKey*/
    PRInt32           symWrapMechIndex;
    PRUint16          wrappedSymKeyLen;
} SSLWrappedSymWrappingKey;

typedef struct SessionTicketStr {
    PRUint16              ticket_version;
    SSL3ProtocolVersion   ssl_version;
    ssl3CipherSuite       cipher_suite;
    SSLCompressionMethod  compression_method;
    SSLSignType           authAlgorithm;
    PRUint32              authKeyBits;
    SSLKEAType            keaType;
    PRUint32              keaKeyBits;
    /*
     * exchKeyType and msWrapMech contain meaningful values only if
     * ms_is_wrapped is true.
     */
    PRUint8               ms_is_wrapped;
    SSLKEAType            exchKeyType; /* XXX(wtc): same as keaType above? */
    CK_MECHANISM_TYPE     msWrapMech;
    PRUint16              ms_length;
    SSL3Opaque            master_secret[48];
    ClientIdentity        client_identity;
    SECItem               peer_cert;
    PRUint32              timestamp;
    SECItem               srvName; /* negotiated server name */
}  SessionTicket;

/*
 * SSL2 buffers used in SSL3.
 *     writeBuf in the SecurityInfo maintained by sslsecur.c is used
 *              to hold the data just about to be passed to the kernel
 *     sendBuf in the ConnectInfo maintained by sslcon.c is used
 *              to hold handshake messages as they are accumulated
 */

/*
** This is "ci", as in "ss->sec.ci".
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
    PRUint32           sendSequence;		/*xmitBufLock*/	/* ssl2 only */
    PRUint32           rcvSequence;		/*recvBufLock*/	/* ssl2 only */

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

    /* SSL socket options */
    sslOptions       opt;
    /* Enabled version range */
    SSLVersionRange  vrange;

    /* State flags */
    unsigned long    clientAuthRequested;
    unsigned long    delayDisabled;       /* Nagle delay disabled */
    unsigned long    firstHsDone;         /* first handshake is complete. */
    unsigned long    enoughFirstHsDone;   /* enough of the first handshake is
					   * done for callbacks to be able to
					   * retrieve channel security
					   * parameters from the SSL socket. */
    unsigned long    handshakeBegun;     
    unsigned long    lastWriteBlocked;   
    unsigned long    recvdCloseNotify;    /* received SSL EOF. */
    unsigned long    TCPconnected;       
    unsigned long    appDataBuffered;
    unsigned long    peerRequestedProtection; /* from old renegotiation */

    /* version of the protocol to use */
    SSL3ProtocolVersion version;
    SSL3ProtocolVersion clientHelloVersion; /* version sent in client hello. */

    sslSecurityInfo  sec;		/* not a pointer any more */

    /* protected by firstHandshakeLock AND (in ssl3) ssl3HandshakeLock. */
    const char      *url;				/* ssl 2 & 3 */

    sslHandshakeFunc handshake;				/*firstHandshakeLock*/
    sslHandshakeFunc nextHandshake;			/*firstHandshakeLock*/
    sslHandshakeFunc securityHandshake;			/*firstHandshakeLock*/

    /* the following variable is only used with socks or other proxies. */
    char *           peerID;	/* String uniquely identifies target server. */

    unsigned char *  cipherSpecs;
    unsigned int     sizeCipherSpecs;
const unsigned char *  preferredCipher;

    ssl3KeyPair *         stepDownKeyPair;	/* RSA step down keys */

    /* Callbacks */
    SSLAuthCertificate        authCertificate;
    void                     *authCertificateArg;
    SSLGetClientAuthData      getClientAuthData;
    void                     *getClientAuthDataArg;
    SSLSNISocketConfig        sniSocketConfig;
    void                     *sniSocketConfigArg;
    SSLBadCertHandler         handleBadCert;
    void                     *badCertArg;
    SSLHandshakeCallback      handshakeCallback;
    void                     *handshakeCallbackData;
    SSLCanFalseStartCallback  canFalseStartCallback;
    void                     *canFalseStartCallbackData;
    void                     *pkcs11PinArg;
    SSLNextProtoCallback      nextProtoCallback;
    void                     *nextProtoArg;

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

    /* Gather object used for gathering data */
    sslGather        gs;				/*recvBufLock*/

    sslBuffer        saveBuf;				/*xmitBufLock*/
    sslBuffer        pendingBuf;			/*xmitBufLock*/

    /* Configuration state for server sockets */
    /* server cert and key for each KEA type */
    sslServerCerts        serverCerts[kt_kea_size];
    /* each cert needs its own status */
    SECItemArray *        certStatusArray[kt_kea_size];

    ssl3CipherSuiteCfg cipherSuites[ssl_V3_SUITES_IMPLEMENTED];
    ssl3KeyPair *         ephemeralECDHKeyPair; /* for ECDHE-* handshake */

    /* SSL3 state info.  Formerly was a pointer */
    ssl3State        ssl3;

    /*
     * TLS extension related data.
     */
    /* True when the current session is a stateless resume. */
    PRBool               statelessResume;
    TLSExtensionData     xtnData;

    /* Whether we are doing stream or datagram mode */
    SSLProtocolVariant   protocolVariant;
};



/* All the global data items declared here should be protected using the 
** ssl_global_data_lock, which is a reader/writer lock.
*/
extern NSSRWLock *             ssl_global_data_lock;
extern char                    ssl_debug;
extern char                    ssl_trace;
extern FILE *                  ssl_trace_iob;
extern FILE *                  ssl_keylog_iob;
extern CERTDistNames *         ssl3_server_ca_list;
extern PRUint32                ssl_sid_timeout;
extern PRUint32                ssl3_sid_timeout;

extern const char * const      ssl_cipherName[];
extern const char * const      ssl3_cipherName[];

extern sslSessionIDLookupFunc  ssl_sid_lookup;
extern sslSessionIDCacheFunc   ssl_sid_cache;
extern sslSessionIDUncacheFunc ssl_sid_uncache;

/************************************************************************/

SEC_BEGIN_PROTOS

/* Internal initialization and installation of the SSL error tables */
extern SECStatus ssl_Init(void);
extern SECStatus ssl_InitializePRErrorTable(void);

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
extern void        ssl_ResetSecurityInfo(sslSecurityInfo *sec, PRBool doMemset);
extern void        ssl_DestroySecurityInfo(sslSecurityInfo *sec);

extern void        ssl_PrintBuf(sslSocket *ss, const char *msg, const void *cp, int len);
extern void        ssl_DumpMsg(sslSocket *ss, unsigned char *bp, unsigned len);

extern int         ssl_SendSavedWriteData(sslSocket *ss);
extern SECStatus ssl_SaveWriteData(sslSocket *ss, 
                                   const void* p, unsigned int l);
extern SECStatus ssl2_BeginClientHandshake(sslSocket *ss);
extern SECStatus ssl2_BeginServerHandshake(sslSocket *ss);
extern int       ssl_Do1stHandshake(sslSocket *ss);

extern SECStatus sslBuffer_Grow(sslBuffer *b, unsigned int newLen);
extern SECStatus sslBuffer_Append(sslBuffer *b, const void * data, 
		                  unsigned int len);

extern void      ssl2_UseClearSendFunc(sslSocket *ss);
extern void      ssl_ChooseSessionIDProcs(sslSecurityInfo *sec);

extern sslSessionID *ssl3_NewSessionID(sslSocket *ss, PRBool is_server);
extern sslSessionID *ssl_LookupSID(const PRIPv6Addr *addr, PRUint16 port, 
                                   const char *peerID, const char *urlSvrName);
extern void      ssl_FreeSID(sslSessionID *sid);

extern int       ssl3_SendApplicationData(sslSocket *ss, const PRUint8 *in,
				          int len, int flags);

extern PRBool    ssl_FdIsBlocking(PRFileDesc *fd);

extern PRBool    ssl_SocketIsBlocking(sslSocket *ss);

extern void      ssl3_SetAlwaysBlock(sslSocket *ss);

extern SECStatus ssl_EnableNagleDelay(sslSocket *ss, PRBool enabled);

extern void      ssl_FinishHandshake(sslSocket *ss);

/* Returns PR_TRUE if we are still waiting for the server to respond to our
 * client second round. Once we've received any part of the server's second
 * round then we don't bother trying to false start since it is almost always
 * the case that the NewSessionTicket, ChangeCipherSoec, and Finished messages
 * were sent in the same packet and we want to process them all at the same
 * time. If we were to try to false start in the middle of the server's second
 * round, then we would increase the number of I/O operations
 * (SSL_ForceHandshake/PR_Recv/PR_Send/etc.) needed to finish the handshake.
 */
extern PRBool    ssl3_WaitingForStartOfServerSecondRound(sslSocket *ss);

extern SECStatus
ssl3_CompressMACEncryptRecord(ssl3CipherSpec *   cwSpec,
		              PRBool             isServer,
			      PRBool             isDTLS,
			      PRBool             capRecordVersion,
                              SSL3ContentType    type,
		              const SSL3Opaque * pIn,
		              PRUint32           contentLen,
		              sslBuffer *        wrBuf);
extern PRInt32   ssl3_SendRecord(sslSocket *ss, DTLSEpoch epoch,
				 SSL3ContentType type,
                                 const SSL3Opaque* pIn, PRInt32 nIn,
                                 PRInt32 flags);

#ifdef NSS_ENABLE_ZLIB
/*
 * The DEFLATE algorithm can result in an expansion of 0.1% + 12 bytes. For a
 * maximum TLS record payload of 2**14 bytes, that's 29 bytes.
 */
#define SSL3_COMPRESSION_MAX_EXPANSION 29
#else  /* !NSS_ENABLE_ZLIB */
#define SSL3_COMPRESSION_MAX_EXPANSION 0
#endif

/*
 * make sure there is room in the write buffer for padding and
 * other compression and cryptographic expansions.
 */
#define SSL3_BUFFER_FUDGE     100 + SSL3_COMPRESSION_MAX_EXPANSION

#define SSL_LOCK_READER(ss)		if (ss->recvLock) PZ_Lock(ss->recvLock)
#define SSL_UNLOCK_READER(ss)		if (ss->recvLock) PZ_Unlock(ss->recvLock)
#define SSL_LOCK_WRITER(ss)		if (ss->sendLock) PZ_Lock(ss->sendLock)
#define SSL_UNLOCK_WRITER(ss)		if (ss->sendLock) PZ_Unlock(ss->sendLock)

/* firstHandshakeLock -> recvBufLock */
#define ssl_Get1stHandshakeLock(ss)     \
    { if (!ss->opt.noLocks) { \
	  PORT_Assert(PZ_InMonitor((ss)->firstHandshakeLock) || \
		      !ssl_HaveRecvBufLock(ss)); \
	  PZ_EnterMonitor((ss)->firstHandshakeLock); \
      } }
#define ssl_Release1stHandshakeLock(ss) \
    { if (!ss->opt.noLocks) PZ_ExitMonitor((ss)->firstHandshakeLock); }
#define ssl_Have1stHandshakeLock(ss)    \
    (PZ_InMonitor((ss)->firstHandshakeLock))

/* ssl3HandshakeLock -> xmitBufLock */
#define ssl_GetSSL3HandshakeLock(ss)	\
    { if (!ss->opt.noLocks) { \
	  PORT_Assert(!ssl_HaveXmitBufLock(ss)); \
	  PZ_EnterMonitor((ss)->ssl3HandshakeLock); \
      } }
#define ssl_ReleaseSSL3HandshakeLock(ss) \
    { if (!ss->opt.noLocks) PZ_ExitMonitor((ss)->ssl3HandshakeLock); }
#define ssl_HaveSSL3HandshakeLock(ss)	\
    (PZ_InMonitor((ss)->ssl3HandshakeLock))

#define ssl_GetSpecReadLock(ss)		\
    { if (!ss->opt.noLocks) NSSRWLock_LockRead((ss)->specLock); }
#define ssl_ReleaseSpecReadLock(ss)	\
    { if (!ss->opt.noLocks) NSSRWLock_UnlockRead((ss)->specLock); }
/* NSSRWLock_HaveReadLock is not exported so there's no
 * ssl_HaveSpecReadLock macro. */

#define ssl_GetSpecWriteLock(ss)	\
    { if (!ss->opt.noLocks) NSSRWLock_LockWrite((ss)->specLock); }
#define ssl_ReleaseSpecWriteLock(ss)	\
    { if (!ss->opt.noLocks) NSSRWLock_UnlockWrite((ss)->specLock); }
#define ssl_HaveSpecWriteLock(ss)	\
    (NSSRWLock_HaveWriteLock((ss)->specLock))

/* recvBufLock -> ssl3HandshakeLock -> xmitBufLock */
#define ssl_GetRecvBufLock(ss)		\
    { if (!ss->opt.noLocks) { \
	  PORT_Assert(!ssl_HaveSSL3HandshakeLock(ss)); \
	  PORT_Assert(!ssl_HaveXmitBufLock(ss)); \
	  PZ_EnterMonitor((ss)->recvBufLock); \
      } }
#define ssl_ReleaseRecvBufLock(ss)	\
    { if (!ss->opt.noLocks) PZ_ExitMonitor( (ss)->recvBufLock); }
#define ssl_HaveRecvBufLock(ss)		\
    (PZ_InMonitor((ss)->recvBufLock))

/* xmitBufLock -> specLock */
#define ssl_GetXmitBufLock(ss)		\
    { if (!ss->opt.noLocks) PZ_EnterMonitor((ss)->xmitBufLock); }
#define ssl_ReleaseXmitBufLock(ss)	\
    { if (!ss->opt.noLocks) PZ_ExitMonitor( (ss)->xmitBufLock); }
#define ssl_HaveXmitBufLock(ss)		\
    (PZ_InMonitor((ss)->xmitBufLock))

/* Placeholder value used in version ranges when SSL 3.0 and all
 * versions of TLS are disabled.
 */
#define SSL_LIBRARY_VERSION_NONE 0

/* SSL_LIBRARY_VERSION_MAX_SUPPORTED is the maximum version that this version 
 * of libssl supports. Applications should use SSL_VersionRangeGetSupported at
 * runtime to determine which versions are supported by the version of libssl
 * in use.
 */
#define SSL_LIBRARY_VERSION_MAX_SUPPORTED SSL_LIBRARY_VERSION_TLS_1_2

/* Rename this macro SSL_ALL_VERSIONS_DISABLED when SSL 2.0 is removed. */
#define SSL3_ALL_VERSIONS_DISABLED(vrange) \
    ((vrange)->min == SSL_LIBRARY_VERSION_NONE)

extern PRBool ssl3_VersionIsSupported(SSLProtocolVariant protocolVariant,
				      SSL3ProtocolVersion version);

extern SECStatus ssl3_KeyAndMacDeriveBypass(ssl3CipherSpec * pwSpec,
		    const unsigned char * cr, const unsigned char * sr,
		    PRBool isTLS, PRBool isExport);
extern  SECStatus ssl3_MasterKeyDeriveBypass( ssl3CipherSpec * pwSpec,
		    const unsigned char * cr, const unsigned char * sr,
		    const SECItem * pms, PRBool isTLS, PRBool isRSA);

/* These functions are called from secnav, even though they're "private". */

extern int ssl2_SendErrorMessage(struct sslSocketStr *ss, int error);
extern int SSL_RestartHandshakeAfterCertReq(struct sslSocketStr *ss,
					    CERTCertificate *cert,
					    SECKEYPrivateKey *key,
					    CERTCertificateList *certChain);
extern sslSocket *ssl_FindSocket(PRFileDesc *fd);
extern void ssl_FreeSocket(struct sslSocketStr *ssl);
extern SECStatus SSL3_SendAlert(sslSocket *ss, SSL3AlertLevel level,
				SSL3AlertDescription desc);
extern SECStatus ssl3_DecodeError(sslSocket *ss);

extern SECStatus ssl3_AuthCertificateComplete(sslSocket *ss, PRErrorCode error);

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
SECStatus ssl3_SendClientHello(sslSocket *ss, PRBool resending);

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

#ifndef NSS_DISABLE_ECC
extern void      ssl3_FilterECCipherSuitesByServerCerts(sslSocket *ss);
extern PRBool    ssl3_IsECCEnabled(sslSocket *ss);
extern SECStatus ssl3_DisableECCSuites(sslSocket * ss, 
                                       const ssl3CipherSuite * suite);
extern PRUint32  ssl3_GetSupportedECCurveMask(sslSocket *ss);


/* Macro for finding a curve equivalent in strength to RSA key's */
#define SSL_RSASTRENGTH_TO_ECSTRENGTH(s) \
        ((s <= 1024) ? 160 \
	  : ((s <= 2048) ? 224 \
	    : ((s <= 3072) ? 256 \
	      : ((s <= 7168) ? 384 : 521 ) ) ) )

/* Types and names of elliptic curves used in TLS */
typedef enum { ec_type_explicitPrime      = 1,
	       ec_type_explicitChar2Curve = 2,
	       ec_type_named
} ECType;

typedef enum { ec_noName     = 0,
	       ec_sect163k1  = 1, 
	       ec_sect163r1  = 2, 
	       ec_sect163r2  = 3,
	       ec_sect193r1  = 4, 
	       ec_sect193r2  = 5, 
	       ec_sect233k1  = 6,
	       ec_sect233r1  = 7, 
	       ec_sect239k1  = 8, 
	       ec_sect283k1  = 9,
	       ec_sect283r1  = 10, 
	       ec_sect409k1  = 11, 
	       ec_sect409r1  = 12,
	       ec_sect571k1  = 13, 
	       ec_sect571r1  = 14, 
	       ec_secp160k1  = 15,
	       ec_secp160r1  = 16, 
	       ec_secp160r2  = 17, 
	       ec_secp192k1  = 18,
	       ec_secp192r1  = 19, 
	       ec_secp224k1  = 20, 
	       ec_secp224r1  = 21,
	       ec_secp256k1  = 22, 
	       ec_secp256r1  = 23, 
	       ec_secp384r1  = 24,
	       ec_secp521r1  = 25,
	       ec_pastLastName
} ECName;

extern SECStatus ssl3_ECName2Params(PLArenaPool *arena, ECName curve,
				   SECKEYECParams *params);
ECName	ssl3_GetCurveWithECKeyStrength(PRUint32 curvemsk, int requiredECCbits);


#endif /* NSS_DISABLE_ECC */

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
extern SECStatus ssl3_HandleHandshakeMessage(sslSocket *ss, SSL3Opaque *b, 
					     PRUint32 length);

extern void ssl3_DestroySSL3Info(sslSocket *ss);

extern SECStatus ssl3_NegotiateVersion(sslSocket *ss, 
				       SSL3ProtocolVersion peerVersion,
				       PRBool allowLargerPeerVersion);

extern SECStatus ssl_GetPeerInfo(sslSocket *ss);

#ifndef NSS_DISABLE_ECC
/* ECDH functions */
extern SECStatus ssl3_SendECDHClientKeyExchange(sslSocket * ss, 
			     SECKEYPublicKey * svrPubKey);
extern SECStatus ssl3_HandleECDHServerKeyExchange(sslSocket *ss, 
					SSL3Opaque *b, PRUint32 length);
extern SECStatus ssl3_HandleECDHClientKeyExchange(sslSocket *ss, 
				     SSL3Opaque *b, PRUint32 length,
                                     SECKEYPublicKey *srvrPubKey,
                                     SECKEYPrivateKey *srvrPrivKey);
extern SECStatus ssl3_SendECDHServerKeyExchange(sslSocket *ss,
			const SSL3SignatureAndHashAlgorithm *sigAndHash);
#endif

extern SECStatus ssl3_ComputeCommonKeyHash(SECOidTag hashAlg,
				PRUint8 * hashBuf,
				unsigned int bufLen, SSL3Hashes *hashes, 
				PRBool bypassPKCS11);
extern void ssl3_DestroyCipherSpec(ssl3CipherSpec *spec, PRBool freeSrvName);
extern SECStatus ssl3_InitPendingCipherSpec(sslSocket *ss, PK11SymKey *pms);
extern SECStatus ssl3_AppendHandshake(sslSocket *ss, const void *void_src, 
			PRInt32 bytes);
extern SECStatus ssl3_AppendHandshakeHeader(sslSocket *ss, 
			SSL3HandshakeType t, PRUint32 length);
extern SECStatus ssl3_AppendHandshakeNumber(sslSocket *ss, PRInt32 num, 
			PRInt32 lenSize);
extern SECStatus ssl3_AppendHandshakeVariable( sslSocket *ss, 
			const SSL3Opaque *src, PRInt32 bytes, PRInt32 lenSize);
extern SECStatus ssl3_AppendSignatureAndHashAlgorithm(sslSocket *ss,
			const SSL3SignatureAndHashAlgorithm* sigAndHash);
extern SECStatus ssl3_ConsumeHandshake(sslSocket *ss, void *v, PRInt32 bytes, 
			SSL3Opaque **b, PRUint32 *length);
extern PRInt32   ssl3_ConsumeHandshakeNumber(sslSocket *ss, PRInt32 bytes, 
			SSL3Opaque **b, PRUint32 *length);
extern SECStatus ssl3_ConsumeHandshakeVariable(sslSocket *ss, SECItem *i, 
			PRInt32 bytes, SSL3Opaque **b, PRUint32 *length);
extern SECOidTag ssl3_TLSHashAlgorithmToOID(int hashFunc);
extern SECStatus ssl3_CheckSignatureAndHashAlgorithmConsistency(
			const SSL3SignatureAndHashAlgorithm *sigAndHash,
			CERTCertificate* cert);
extern SECStatus ssl3_ConsumeSignatureAndHashAlgorithm(sslSocket *ss,
			SSL3Opaque **b, PRUint32 *length,
			SSL3SignatureAndHashAlgorithm *out);
extern SECStatus ssl3_SignHashes(SSL3Hashes *hash, SECKEYPrivateKey *key, 
			SECItem *buf, PRBool isTLS);
extern SECStatus ssl3_VerifySignedHashes(SSL3Hashes *hash, 
			CERTCertificate *cert, SECItem *buf, PRBool isTLS, 
			void *pwArg);
extern SECStatus ssl3_CacheWrappedMasterSecret(sslSocket *ss,
			sslSessionID *sid, ssl3CipherSpec *spec,
			SSL3KEAType effectiveExchKeyType);

/* Functions that handle ClientHello and ServerHello extensions. */
extern SECStatus ssl3_HandleServerNameXtn(sslSocket * ss,
			PRUint16 ex_type, SECItem *data);
extern SECStatus ssl3_HandleSupportedCurvesXtn(sslSocket * ss,
			PRUint16 ex_type, SECItem *data);
extern SECStatus ssl3_HandleSupportedPointFormatsXtn(sslSocket * ss,
			PRUint16 ex_type, SECItem *data);
extern SECStatus ssl3_ClientHandleSessionTicketXtn(sslSocket *ss,
			PRUint16 ex_type, SECItem *data);
extern SECStatus ssl3_ServerHandleSessionTicketXtn(sslSocket *ss,
			PRUint16 ex_type, SECItem *data);

/* ClientHello and ServerHello extension senders.
 * Note that not all extension senders are exposed here; only those that
 * that need exposure.
 */
extern PRInt32 ssl3_SendSessionTicketXtn(sslSocket *ss, PRBool append,
			PRUint32 maxBytes);

/* ClientHello and ServerHello extension senders.
 * The code is in ssl3ext.c.
 */
extern PRInt32 ssl3_SendServerNameXtn(sslSocket *ss, PRBool append,
                     PRUint32 maxBytes);

/* Assigns new cert, cert chain and keys to ss->serverCerts
 * struct. If certChain is NULL, tries to find one. Aborts if
 * fails to do so. If cert and keyPair are NULL - unconfigures
 * sslSocket of kea type.*/
extern SECStatus ssl_ConfigSecureServer(sslSocket *ss, CERTCertificate *cert,
                                        const CERTCertificateList *certChain,
                                        ssl3KeyPair *keyPair, SSLKEAType kea);

#ifndef NSS_DISABLE_ECC
extern PRInt32 ssl3_SendSupportedCurvesXtn(sslSocket *ss,
			PRBool append, PRUint32 maxBytes);
extern PRInt32 ssl3_SendSupportedPointFormatsXtn(sslSocket *ss,
			PRBool append, PRUint32 maxBytes);
#endif

/* call the registered extension handlers. */
extern SECStatus ssl3_HandleHelloExtensions(sslSocket *ss, 
			SSL3Opaque **b, PRUint32 *length);

/* Hello Extension related routines. */
extern PRBool ssl3_ExtensionNegotiated(sslSocket *ss, PRUint16 ex_type);
extern void ssl3_SetSIDSessionTicket(sslSessionID *sid,
			/*in/out*/ NewSessionTicket *session_ticket);
extern SECStatus ssl3_SendNewSessionTicket(sslSocket *ss);
extern PRBool ssl_GetSessionTicketKeys(unsigned char *keyName,
			unsigned char *encKey, unsigned char *macKey);
extern PRBool ssl_GetSessionTicketKeysPKCS11(SECKEYPrivateKey *svrPrivKey,
			SECKEYPublicKey *svrPubKey, void *pwArg,
			unsigned char *keyName, PK11SymKey **aesKey,
			PK11SymKey **macKey);

/* Tell clients to consider tickets valid for this long. */
#define TLS_EX_SESS_TICKET_LIFETIME_HINT    (2 * 24 * 60 * 60) /* 2 days */
#define TLS_EX_SESS_TICKET_VERSION          (0x0100)

extern SECStatus ssl3_ValidateNextProtoNego(const unsigned char* data,
					    unsigned int length);

/* Construct a new NSPR socket for the app to use */
extern PRFileDesc *ssl_NewPRSocket(sslSocket *ss, PRFileDesc *fd);
extern void ssl_FreePRSocket(PRFileDesc *fd);

/* Internal config function so SSL3 can initialize the present state of
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

extern SECStatus ssl_InitSymWrapKeysLock(void);

extern SECStatus ssl_FreeSymWrapKeysLock(void);

extern SECStatus ssl_InitSessionCacheLocks(PRBool lazyInit);

extern SECStatus ssl_FreeSessionCacheLocks(void);


/**************** DTLS-specific functions **************/
extern void dtls_FreeQueuedMessage(DTLSQueuedMessage *msg);
extern void dtls_FreeQueuedMessages(PRCList *lst);
extern void dtls_FreeHandshakeMessages(PRCList *lst);

extern SECStatus dtls_HandleHandshake(sslSocket *ss, sslBuffer *origBuf);
extern SECStatus dtls_HandleHelloVerifyRequest(sslSocket *ss,
					       SSL3Opaque *b, PRUint32 length);
extern SECStatus dtls_StageHandshakeMessage(sslSocket *ss);
extern SECStatus dtls_QueueMessage(sslSocket *ss, SSL3ContentType type,
				   const SSL3Opaque *pIn, PRInt32 nIn);
extern SECStatus dtls_FlushHandshakeMessages(sslSocket *ss, PRInt32 flags);
extern SECStatus dtls_CompressMACEncryptRecord(sslSocket *ss,
					       DTLSEpoch epoch,
					       PRBool use_epoch,
					       SSL3ContentType type,
					       const SSL3Opaque *pIn,
					       PRUint32 contentLen,
					       sslBuffer *wrBuf);
SECStatus ssl3_DisableNonDTLSSuites(sslSocket * ss);
extern SECStatus dtls_StartTimer(sslSocket *ss, DTLSTimerCb cb);
extern SECStatus dtls_RestartTimer(sslSocket *ss, PRBool backoff,
				   DTLSTimerCb cb);
extern void dtls_CheckTimer(sslSocket *ss);
extern void dtls_CancelTimer(sslSocket *ss);
extern void dtls_FinishedTimerCb(sslSocket *ss);
extern void dtls_SetMTU(sslSocket *ss, PRUint16 advertised);
extern void dtls_InitRecvdRecords(DTLSRecvdRecords *records);
extern int dtls_RecordGetRecvd(DTLSRecvdRecords *records, PRUint64 seq);
extern void dtls_RecordSetRecvd(DTLSRecvdRecords *records, PRUint64 seq);
extern void dtls_RehandshakeCleanup(sslSocket *ss);
extern SSL3ProtocolVersion
dtls_TLSVersionToDTLSVersion(SSL3ProtocolVersion tlsv);
extern SSL3ProtocolVersion
dtls_DTLSVersionToTLSVersion(SSL3ProtocolVersion dtlsv);

/********************** misc calls *********************/

#ifdef DEBUG
extern void ssl3_CheckCipherSuiteOrderConsistency();
#endif

extern int ssl_MapLowLevelError(int hiLevelError);

extern PRUint32 ssl_Time(void);

extern void SSL_AtomicIncrementLong(long * x);

SECStatus SSL_DisableDefaultExportCipherSuites(void);
SECStatus SSL_DisableExportCipherSuites(PRFileDesc * fd);
PRBool    SSL_IsExportCipherSuite(PRUint16 cipherSuite);

extern SECStatus
ssl3_TLSPRFWithMasterSecret(ssl3CipherSpec *spec,
                            const char *label, unsigned int labelLen,
                            const unsigned char *val, unsigned int valLen,
                            unsigned char *out, unsigned int outLen);

#ifdef TRACE
#define SSL_TRACE(msg) ssl_Trace msg
#else
#define SSL_TRACE(msg)
#endif

void ssl_Trace(const char *format, ...);

SEC_END_PROTOS

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#define SSL_GETPID getpid
#elif defined(WIN32)
extern int __cdecl _getpid(void);
#define SSL_GETPID _getpid
#else
#define SSL_GETPID() 0
#endif

#endif /* __sslimpl_h_ */
