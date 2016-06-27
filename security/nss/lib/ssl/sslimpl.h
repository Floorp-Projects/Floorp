/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
typedef SSLMACAlgorithm SSL3MACAlgorithm;

#define calg_null ssl_calg_null
#define calg_rc4 ssl_calg_rc4
#define calg_rc2 ssl_calg_rc2
#define calg_des ssl_calg_des
#define calg_3des ssl_calg_3des
#define calg_idea ssl_calg_idea
#define calg_fortezza ssl_calg_fortezza /* deprecated, must preserve */
#define calg_aes ssl_calg_aes
#define calg_camellia ssl_calg_camellia
#define calg_seed ssl_calg_seed
#define calg_aes_gcm ssl_calg_aes_gcm
#define calg_chacha20 ssl_calg_chacha20

#define mac_null ssl_mac_null
#define mac_md5 ssl_mac_md5
#define mac_sha ssl_mac_sha
#define hmac_md5 ssl_hmac_md5
#define hmac_sha ssl_hmac_sha
#define hmac_sha256 ssl_hmac_sha256
#define hmac_sha384 ssl_hmac_sha384
#define mac_aead ssl_mac_aead

#define SET_ERROR_CODE    /* reminder */
#define SEND_ALERT        /* reminder */
#define TEST_FOR_FAILURE  /* reminder */
#define DEAL_WITH_FAILURE /* reminder */

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
#define SSL_TRC(a, b)     \
    if (ssl_trace >= (a)) \
    ssl_Trace b
#define PRINT_BUF(a, b)   \
    if (ssl_trace >= (a)) \
    ssl_PrintBuf b
#define PRINT_KEY(a, b)   \
    if (ssl_trace >= (a)) \
    ssl_PrintKey b
#else
#define SSL_TRC(a, b)
#define PRINT_BUF(a, b)
#define PRINT_KEY(a, b)
#endif

#ifdef DEBUG
#define SSL_DBG(b) \
    if (ssl_debug) \
    ssl_Trace b
#else
#define SSL_DBG(b)
#endif

#include "private/pprthred.h" /* for PR_InMonitor() */
#define ssl_InMonitor(m) PZ_InMonitor(m)

#define LSB(x) ((unsigned char)((x)&0xff))
#define MSB(x) ((unsigned char)(((unsigned)(x)) >> 8))

/************************************************************************/

typedef enum { SSLAppOpRead = 0,
               SSLAppOpWrite,
               SSLAppOpRDWR,
               SSLAppOpPost,
               SSLAppOpHeader
} SSLAppOperation;

#define SSL3_SESSIONID_BYTES 32

#define SSL_MIN_CHALLENGE_BYTES 16
#define SSL_MAX_CHALLENGE_BYTES 32

#define SSL3_RSA_PMS_LENGTH 48
#define SSL3_MASTER_SECRET_LENGTH 48

/* number of wrap mechanisms potentially used to wrap master secrets. */
#define SSL_NUM_WRAP_MECHS 16

/* This makes the cert cache entry exactly 4k. */
#define SSL_MAX_CACHED_CERT_LEN 4060

#define NUM_MIXERS 9

#ifndef BPB
#define BPB 8 /* Bits Per Byte */
#endif

#define EXPORT_RSA_KEY_LENGTH 64 /* bytes */

/* The default value from RFC 4347 is 1s, which is too slow. */
#define DTLS_RETRANSMIT_INITIAL_MS 50
/* The maximum time to wait between retransmissions. */
#define DTLS_RETRANSMIT_MAX_MS 10000
/* Time to wait in FINISHED state for retransmissions. */
#define DTLS_RETRANSMIT_FINISHED_MS 30000

/* Types and names of elliptic curves used in TLS */
typedef enum {
    ec_type_explicitPrime = 1,      /* not supported */
    ec_type_explicitChar2Curve = 2, /* not supported */
    ec_type_named = 3
} ECType;

/* Previously known as NamedCurve. */
typedef enum {
    ec_sect163k1 = 1,
    ec_sect163r1 = 2,
    ec_sect163r2 = 3,
    ec_sect193r1 = 4,
    ec_sect193r2 = 5,
    ec_sect233k1 = 6,
    ec_sect233r1 = 7,
    ec_sect239k1 = 8,
    ec_sect283k1 = 9,
    ec_sect283r1 = 10,
    ec_sect409k1 = 11,
    ec_sect409r1 = 12,
    ec_sect571k1 = 13,
    ec_sect571r1 = 14,
    ec_secp160k1 = 15,
    ec_secp160r1 = 16,
    ec_secp160r2 = 17,
    ec_secp192k1 = 18,
    ec_secp192r1 = 19,
    ec_secp224k1 = 20,
    ec_secp224r1 = 21,
    ec_secp256k1 = 22,
    ec_secp256r1 = 23,
    ec_secp384r1 = 24,
    ec_secp521r1 = 25,
    ffdhe_2048 = 256, /* draft-ietf-tls-negotiated-ff-dhe-10 */
    ffdhe_3072 = 257,
    ffdhe_4096 = 258,
    ffdhe_6144 = 259,
    ffdhe_8192 = 260,
    ffdhe_custom = 65537 /* special value */
} NamedGroup;

/* TODO: decide if SSLKEAType might be better here. */
typedef enum {
    group_type_ec,
    group_type_ff
} NamedGroupType;

typedef struct {
    /* This is the index of the curve into the bit mask that we use to track
     * what has been negotiated on the socket. */
    PRUint8 index;
    /* The name is the value that is encoded on the wire in TLS. */
    NamedGroup name;
    /* The number of bits in the group. */
    unsigned int bits;
    /* Whether the group is Elliptic or Finite-Field. */
    NamedGroupType type;
    /* The OID that identifies the group to PKCS11.  This also determines
     * whether the group is enabled in policy. */
    SECOidTag oidTag;
    /* Non-suite-B groups are enabled by patching NSS. Yuck. */
    PRBool suiteb;
} namedGroupDef;

typedef struct sslBufferStr sslBuffer;
typedef struct sslConnectInfoStr sslConnectInfo;
typedef struct sslGatherStr sslGather;
typedef struct sslSecurityInfoStr sslSecurityInfo;
typedef struct sslSessionIDStr sslSessionID;
typedef struct sslSocketStr sslSocket;
typedef struct sslSocketOpsStr sslSocketOps;

typedef struct ssl3StateStr ssl3State;
typedef struct ssl3CertNodeStr ssl3CertNode;
typedef struct ssl3BulkCipherDefStr ssl3BulkCipherDef;
typedef struct ssl3MACDefStr ssl3MACDef;
typedef struct sslKeyPairStr sslKeyPair;
typedef struct ssl3DHParamsStr ssl3DHParams;

struct ssl3CertNodeStr {
    struct ssl3CertNodeStr *next;
    CERTCertificate *cert;
};

typedef SECStatus (*sslHandshakeFunc)(sslSocket *ss);

typedef void (*sslSessionIDCacheFunc)(sslSessionID *sid);
typedef void (*sslSessionIDUncacheFunc)(sslSessionID *sid);
typedef sslSessionID *(*sslSessionIDLookupFunc)(const PRIPv6Addr *addr,
                                                unsigned char *sid,
                                                unsigned int sidLen,
                                                CERTCertDBHandle *dbHandle);

/* registerable callback function that either appends extension to buffer
 * or returns length of data that it would have appended.
 */
typedef PRInt32 (*ssl3HelloExtensionSenderFunc)(sslSocket *ss, PRBool append,
                                                PRUint32 maxBytes);

/* registerable callback function that handles a received extension,
 * of the given type.
 */
typedef SECStatus (*ssl3HelloExtensionHandlerFunc)(sslSocket *ss,
                                                   PRUint16 ex_type,
                                                   SECItem *data);

/* row in a table of hello extension senders */
typedef struct {
    PRInt32 ex_type;
    ssl3HelloExtensionSenderFunc ex_sender;
} ssl3HelloExtensionSender;

/* row in a table of hello extension handlers */
typedef struct {
    PRInt32 ex_type;
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
    int (*connect)(sslSocket *, const PRNetAddr *);
    PRFileDesc *(*accept)(sslSocket *, PRNetAddr *);
    int (*bind)(sslSocket *, const PRNetAddr *);
    int (*listen)(sslSocket *, int);
    int (*shutdown)(sslSocket *, int);
    int (*close)(sslSocket *);

    int (*recv)(sslSocket *, unsigned char *, int, int);

    /* points to the higher-layer send func, e.g. ssl_SecureSend. */
    int (*send)(sslSocket *, const unsigned char *, int, int);
    int (*read)(sslSocket *, unsigned char *, int);
    int (*write)(sslSocket *, const unsigned char *, int);

    int (*getpeername)(sslSocket *, PRNetAddr *);
    int (*getsockname)(sslSocket *, PRNetAddr *);
};

/* Flags interpreted by ssl send functions. */
#define ssl_SEND_FLAG_FORCE_INTO_BUFFER 0x40000000
#define ssl_SEND_FLAG_NO_BUFFER 0x20000000
#define ssl_SEND_FLAG_NO_RETRANSMIT 0x08000000 /* DTLS only */
#define ssl_SEND_FLAG_CAP_RECORD_VERSION \
    0x04000000 /* TLS only */
#define ssl_SEND_FLAG_MASK 0x7f000000

/*
** A buffer object.
*/
struct sslBufferStr {
    unsigned char *buf;
    unsigned int len;
    unsigned int space;
};

/*
** SSL3 cipher suite policy and preference struct.
*/
typedef struct {
#if !defined(_WIN32)
    unsigned int cipher_suite : 16;
    unsigned int policy : 8;
    unsigned int enabled : 1;
    unsigned int isPresent : 1;
#else
    ssl3CipherSuite cipher_suite;
    PRUint8 policy;
    unsigned char enabled : 1;
    unsigned char isPresent : 1;
#endif
} ssl3CipherSuiteCfg;

#define ssl_V3_SUITES_IMPLEMENTED 80

#define MAX_DTLS_SRTP_CIPHER_SUITES 4

/* MAX_SIGNATURE_ALGORITHMS allows for a large number of combinations of
 * SSLSignType and SSLHashType, but not all combinations (specifically, this
 * doesn't allow space for combinations with MD5). */
#define MAX_SIGNATURE_ALGORITHMS 15

typedef struct sslOptionsStr {
    /* If SSL_SetNextProtoNego has been called, then this contains the
     * list of supported protocols. */
    SECItem nextProtoNego;

    unsigned int useSecurity : 1;
    unsigned int useSocks : 1;
    unsigned int requestCertificate : 1;
    unsigned int requireCertificate : 2;
    unsigned int handshakeAsClient : 1;
    unsigned int handshakeAsServer : 1;
    unsigned int noCache : 1;
    unsigned int fdx : 1;
    unsigned int detectRollBack : 1;
    unsigned int noStepDown : 1;
    unsigned int bypassPKCS11 : 1;
    unsigned int noLocks : 1;
    unsigned int enableSessionTickets : 1;
    unsigned int enableDeflate : 1;
    unsigned int enableRenegotiation : 2;
    unsigned int requireSafeNegotiation : 1;
    unsigned int enableFalseStart : 1;
    unsigned int cbcRandomIV : 1;
    unsigned int enableOCSPStapling : 1;
    unsigned int enableNPN : 1;
    unsigned int enableALPN : 1;
    unsigned int reuseServerECDHEKey : 1;
    unsigned int enableFallbackSCSV : 1;
    unsigned int enableServerDhe : 1;
    unsigned int enableExtendedMS : 1;
    unsigned int enableSignedCertTimestamps : 1;
    unsigned int requireDHENamedGroups : 1;
} sslOptions;

typedef enum { sslHandshakingUndetermined = 0,
               sslHandshakingAsClient,
               sslHandshakingAsServer
} sslHandshakingType;

#define SSL_LOCK_RANK_SPEC 255
#define SSL_LOCK_RANK_GLOBAL NSS_RWLOCK_RANK_NONE

/* These are the valid values for shutdownHow.
** These values are each 1 greater than the NSPR values, and the code
** depends on that relation to efficiently convert PR_SHUTDOWN values
** into ssl_SHUTDOWN values.  These values use one bit for read, and
** another bit for write, and can be used as bitmasks.
*/
#define ssl_SHUTDOWN_NONE 0 /* NOT shutdown at all */
#define ssl_SHUTDOWN_RCV 1  /* PR_SHUTDOWN_RCV  +1 */
#define ssl_SHUTDOWN_SEND 2 /* PR_SHUTDOWN_SEND +1 */
#define ssl_SHUTDOWN_BOTH 3 /* PR_SHUTDOWN_BOTH +1 */

/*
** A gather object. Used to read some data until a count has been
** satisfied. Primarily for support of async sockets.
** Everything in here is protected by the recvBufLock.
*/
struct sslGatherStr {
    int state; /* see GS_ values below. */

    /* "buf" holds received plaintext SSL records, after decrypt and MAC check.
     * recv'd ciphertext records are put in inbuf (see below), then decrypted
     * into buf.
     */
    sslBuffer buf; /*recvBufLock*/

    /* number of bytes previously read into hdr or inbuf.
    ** (offset - writeOffset) is the number of ciphertext bytes read in but
    **     not yet deciphered.
    */
    unsigned int offset;

    /* number of bytes to read in next call to ssl_DefRecv (recv) */
    unsigned int remainder;

    /* DoRecv uses the next two values to extract application data.
    ** The difference between writeOffset and readOffset is the amount of
    ** data available to the application.   Note that the actual offset of
    ** the data in "buf" is recordOffset (above), not readOffset.
    ** In the current implementation, this is made available before the
    ** MAC is checked!!
    */
    unsigned int readOffset; /* Spot where DATA reader (e.g. application
                               ** or handshake code) will read next.
                               ** Always zero for SSl3 application data.
                               */
    /* offset in buf/inbuf/hdr into which new data will be read from socket. */
    unsigned int writeOffset;

    /* Buffer for ssl3 to read (encrypted) data from the socket */
    sslBuffer inbuf; /*recvBufLock*/

    /* The ssl[23]_GatherData functions read data into this buffer, rather
    ** than into buf or inbuf, while in the GS_HEADER state.
    ** The portion of the SSL record header put here always comes off the wire
    ** as plaintext, never ciphertext.
    ** For SSL3/TLS, the plaintext portion is 5 bytes long. For DTLS it is 13.
    */
    unsigned char hdr[13];

    /* Buffer for DTLS data read off the wire as a single datagram */
    sslBuffer dtlsPacket;

    /* the start of the buffered DTLS record in dtlsPacket */
    unsigned int dtlsPacketOffset;
};

/* sslGather.state */
#define GS_INIT 0
#define GS_HEADER 1
#define GS_MAC 2
#define GS_DATA 3
#define GS_PAD 4

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
    cipher_aes_256_gcm,
    cipher_chacha20,
    cipher_missing /* reserved for no such supported cipher */
    /* This enum must match ssl3_cipherName[] in ssl3con.c.  */
} SSL3BulkCipher;

typedef enum { type_stream,
               type_block,
               type_aead } CipherType;

#define MAX_IV_LENGTH 24

/*
 * Do not depend upon 64 bit arithmetic in the underlying machine.
 */
typedef struct {
    PRUint32 high;
    PRUint32 low;
} SSL3SequenceNumber;

typedef PRUint16 DTLSEpoch;

typedef void (*DTLSTimerCb)(sslSocket *);

/* 400 is large enough for MD5, SHA-1, and SHA-256.
 * For SHA-384 support, increase it to 712. */
#define MAX_MAC_CONTEXT_BYTES 712
#define MAX_MAC_CONTEXT_LLONGS (MAX_MAC_CONTEXT_BYTES / 8)

#define MAX_CIPHER_CONTEXT_BYTES 2080
#define MAX_CIPHER_CONTEXT_LLONGS (MAX_CIPHER_CONTEXT_BYTES / 8)

typedef struct {
    SSL3Opaque wrapped_master_secret[48];
    PRUint16 wrapped_master_secret_len;
    PRUint8 msIsWrapped;
    PRUint8 resumable;
    PRUint8 extendedMasterSecretUsed;
} ssl3SidKeys; /* 52 bytes */

typedef struct {
    PK11SymKey *write_key;
    PK11SymKey *write_mac_key;
    PK11Context *write_mac_context;
    SECItem write_key_item;
    SECItem write_iv_item;
    SECItem write_mac_key_item;
    SSL3Opaque write_iv[MAX_IV_LENGTH];
    PRUint64 cipher_context[MAX_CIPHER_CONTEXT_LLONGS];
} ssl3KeyMaterial;

typedef SECStatus (*SSLCipher)(void *context,
                               unsigned char *out,
                               int *outlen,
                               int maxout,
                               const unsigned char *in,
                               int inlen);
typedef SECStatus (*SSLAEADCipher)(
    ssl3KeyMaterial *keys,
    PRBool doDecrypt,
    unsigned char *out,
    int *outlen,
    int maxout,
    const unsigned char *in,
    int inlen,
    const unsigned char *additionalData,
    int additionalDataLen);
typedef SECStatus (*SSLCompressor)(void *context,
                                   unsigned char *out,
                                   int *outlen,
                                   int maxout,
                                   const unsigned char *in,
                                   int inlen);
typedef SECStatus (*SSLDestroy)(void *context, PRBool freeit);

/* The DTLS anti-replay window. Defined here because we need it in
 * the cipher spec. Note that this is a ring buffer but left and
 * right represent the true window, with modular arithmetic used to
 * map them onto the buffer.
 */
#define DTLS_RECVD_RECORDS_WINDOW 1024 /* Packets; approximate   \
                                        * Must be divisible by 8 \
                                        */
typedef struct DTLSRecvdRecordsStr {
    unsigned char data[DTLS_RECVD_RECORDS_WINDOW / 8];
    PRUint64 left;
    PRUint64 right;
} DTLSRecvdRecords;

/*
** These are the "specs" in the "ssl3" struct.
** Access to the pointers to these specs, and all the specs' contents
** (direct and indirect) is protected by the reader/writer lock ss->specLock.
*/
typedef struct {
    PRCList link;
    const ssl3BulkCipherDef *cipher_def;
    const ssl3MACDef *mac_def;
    SSLCompressionMethod compression_method;
    int mac_size;
    SSLCipher encode;
    SSLCipher decode;
    SSLAEADCipher aead;
    SSLDestroy destroy;
    void *encodeContext;
    void *decodeContext;
    SSLCompressor compressor;   /* Don't name these fields compress */
    SSLCompressor decompressor; /* and uncompress because zconf.h   */
                                /* may define them as macros.       */
    SSLDestroy destroyCompressContext;
    void *compressContext;
    SSLDestroy destroyDecompressContext;
    void *decompressContext;
    PRBool bypassCiphers; /* did double bypass (at least) */
    PK11SymKey *master_secret;
    SSL3SequenceNumber write_seq_num;
    SSL3SequenceNumber read_seq_num;
    SSL3ProtocolVersion version;
    ssl3KeyMaterial client;
    ssl3KeyMaterial server;
    SECItem msItem;
    unsigned char key_block[NUM_MIXERS * HASH_LENGTH_MAX];
    unsigned char raw_master_secret[56];
    SECItem srvVirtName; /* for server: name that was negotiated
                          * with a client. For client - is
                          * always set to NULL.*/
    DTLSEpoch epoch;
    DTLSRecvdRecords recvdRecords;

    PRUint8 refCt;
} ssl3CipherSpec;

typedef enum { never_cached,
               in_client_cache,
               in_server_cache,
               invalid_cache /* no longer in any cache. */
} Cached;

#include "sslcert.h"

struct sslSessionIDStr {
    /* The global cache lock must be held when accessing these members when the
     * sid is in any cache.
     */
    sslSessionID *next; /* chain used for client sockets, only */
    Cached cached;
    int references;
    PRUint32 lastAccessTime; /* seconds since Jan 1, 1970 */

    /* The rest of the members, except for the members of u.ssl3.locked, may
     * be modified only when the sid is not in any cache.
     */

    CERTCertificate *peerCert;
    SECItemArray peerCertStatus; /* client only */
    const char *peerID;          /* client only */
    const char *urlSvrName;      /* client only */
    sslServerCertType certType;
    CERTCertificate *localCert;

    PRIPv6Addr addr;
    PRUint16 port;

    SSL3ProtocolVersion version;

    PRUint32 creationTime;   /* seconds since Jan 1, 1970 */
    PRUint32 expirationTime; /* seconds since Jan 1, 1970 */

    SSLAuthType authType;
    PRUint32 authKeyBits;
    SSLKEAType keaType;
    PRUint32 keaKeyBits;
    PRUint32 namedGroups;

    union {
        struct {
            /* values that are copied into the server's on-disk SID cache. */
            PRUint8 sessionIDLength;
            SSL3Opaque sessionID[SSL3_SESSIONID_BYTES];

            ssl3CipherSuite cipherSuite;
            SSLCompressionMethod compression;
            int policy;
            ssl3SidKeys keys;
            /* mechanism used to wrap master secret */
            CK_MECHANISM_TYPE masterWrapMech;

            /* The following values are NOT restored from the server's on-disk
             * session cache, but are restored from the client's cache.
             */
            PK11SymKey *clientWriteKey;
            PK11SymKey *serverWriteKey;

            /* The following values pertain to the slot that wrapped the
            ** master secret. (used only in client)
            */
            SECMODModuleID masterModuleID;
            /* what module wrapped the master secret */
            CK_SLOT_ID masterSlotID;
            PRUint16 masterWrapIndex;
            /* what's the key index for the wrapping key */
            PRUint16 masterWrapSeries;
            /* keep track of the slot series, so we don't
             * accidently try to use new keys after the
             * card gets removed and replaced.*/

            /* The following values pertain to the slot that did the signature
            ** for client auth.   (used only in client)
            */
            SECMODModuleID clAuthModuleID;
            CK_SLOT_ID clAuthSlotID;
            PRUint16 clAuthSeries;

            char masterValid;
            char clAuthValid;

            SECItem srvName;

            /* Signed certificate timestamps received in a TLS extension.
            ** (used only in client).
            */
            SECItem signedCertTimestamps;

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
    ssl3CipherSuite cipher_suite;
    SSL3BulkCipher bulk_cipher_alg;
    SSL3MACAlgorithm mac_alg;
    SSL3KeyExchangeAlgorithm key_exchange_alg;
    SSLHashType prf_hash;
} ssl3CipherSuiteDef;

/*
** There are tables of these, all const.
*/
typedef struct {
    /* An identifier for this struct. */
    SSL3KeyExchangeAlgorithm kea;
    /* The type of key exchange used by the cipher suite. */
    SSLKEAType exchKeyType;
    /* If the cipher suite uses a signature, the type of signature. */
    SSLSignType signKeyType;
    /* In most cases, cipher suites depend on their signature type for
     * authentication, ECDH certificates being the exception. */
    SSLAuthType authKeyType;
    /* For export cipher suites:
     * is_limited identifies a suite as having a limit on the key size.
     * key_size_limit provides the corresponding limit. */
    PRBool is_limited;
    unsigned int key_size_limit;
    PRBool tls_keygen;
    /* True if the key exchange for the suite is ephemeral.  Or to be more
     * precise: true if the ServerKeyExchange message is always required. */
    PRBool ephemeral;
    /* An OID describing the key exchange */
    SECOidTag oid;
} ssl3KEADef;

/*
** There are tables of these, all const.
*/
struct ssl3BulkCipherDefStr {
    SSL3BulkCipher cipher;
    SSLCipherAlgorithm calg;
    int key_size;
    int secret_key_size;
    CipherType type;
    int iv_size;
    int block_size;
    int tag_size;            /* authentication tag size for AEAD ciphers. */
    int explicit_nonce_size; /* for AEAD ciphers. */
    SECOidTag oid;
};

/*
** There are tables of these, all const.
*/
struct ssl3MACDefStr {
    SSL3MACAlgorithm mac;
    CK_MECHANISM_TYPE mmech;
    int pad_size;
    int mac_size;
    SECOidTag oid;
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
    wait_encrypted_extensions,
    idle_handshake,
    wait_invalid /* Invalid value. There is no handshake message "invalid". */
} SSL3WaitState;

/*
 * TLS extension related constants and data structures.
 */
typedef struct TLSExtensionDataStr TLSExtensionData;
typedef struct SessionTicketDataStr SessionTicketData;

struct TLSExtensionDataStr {
    /* registered callbacks that send server hello extensions */
    ssl3HelloExtensionSender serverHelloSenders[SSL_MAX_EXTENSIONS];
    ssl3HelloExtensionSender encryptedExtensionsSenders[SSL_MAX_EXTENSIONS];

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

    /* Signed Certificate Timestamps extracted from the TLS extension.
     * (client only).
     * This container holds a temporary pointer to the extension data,
     * until a session structure (the sec.ci.sid of an sslSocket) is setup
     * that can hold a permanent copy of the data
     * (in sec.ci.sid.u.ssl3.signedCertTimestamps).
     * The data pointed to by this structure is neither explicitly allocated
     * nor copied: the pointer points to the handshake message buffer and is
     * only valid in the scope of ssl3_HandleServerHello.
     */
    SECItem signedCertTimestamps;
};

typedef enum {
    sni_nametype_hostname
} SNINameType;

typedef SECStatus (*sslRestartTarget)(sslSocket *);

/*
** A DTLS queued message (potentially to be retransmitted)
*/
typedef struct DTLSQueuedMessageStr {
    PRCList link;           /* The linked list link */
    ssl3CipherSpec *cwSpec; /* The cipher spec to use, null for none */
    SSL3ContentType type;   /* The message type */
    unsigned char *data;    /* The data */
    PRUint16 len;           /* The data length */
} DTLSQueuedMessage;

typedef struct TLS13KeyShareEntryStr {
    PRCList link;               /* The linked list link */
    const namedGroupDef *group; /* The group for the entry */
    SECItem key_exchange;       /* The share itself */
} TLS13KeyShareEntry;

typedef enum {
    handshake_hash_unknown = 0,
    handshake_hash_combo = 1,  /* The MD5/SHA-1 combination */
    handshake_hash_single = 2, /* A single hash */
    handshake_hash_record
} SSL3HandshakeHashType;

/*
** This is the "hs" member of the "ssl3" struct.
** This entire struct is protected by ssl3HandshakeLock
*/
typedef struct SSL3HandshakeStateStr {
    SSL3Random server_random;
    SSL3Random client_random;
    SSL3WaitState ws; /* May also contain SSL3WaitState | 0x80 for TLS 1.3 */

    /* This group of members is used for handshake running hashes. */
    SSL3HandshakeHashType hashType;
    sslBuffer messages; /* Accumulated handshake messages */
#ifndef NO_PKCS11_BYPASS
    /* Bypass mode:
     * SSL 3.0 - TLS 1.1 use both |md5_cx| and |sha_cx|. |md5_cx| is used for
     * MD5 and |sha_cx| for SHA-1.
     * TLS 1.2 and later use only |sha_cx|, for SHA-256. NOTE: When we support
     * SHA-384, increase MAX_MAC_CONTEXT_BYTES to 712. */
    PRUint64 md5_cx[MAX_MAC_CONTEXT_LLONGS];
    PRUint64 sha_cx[MAX_MAC_CONTEXT_LLONGS];
    const SECHashObject *sha_obj;
    /* The function prototype of sha_obj->clone() does not match the prototype
     * of the freebl <HASH>_Clone functions, so we need a dedicated function
     * pointer for the <HASH>_Clone function. */
    void (*sha_clone)(void *dest, void *src);
#endif
    /* PKCS #11 mode:
     * SSL 3.0 - TLS 1.1 use both |md5| and |sha|. |md5| is used for MD5 and
     * |sha| for SHA-1.
     * TLS 1.2 and later use only |sha|, for SHA-256. */
    PK11Context *md5;
    PK11Context *sha;
    SSLHashType tls12CertVerifyHash;

    const ssl3KEADef *kea_def;
    ssl3CipherSuite cipher_suite;
    const ssl3CipherSuiteDef *suite_def;
    SSLCompressionMethod compression;
    sslBuffer msg_body; /* protected by recvBufLock */
                        /* partial handshake message from record layer */
    unsigned int header_bytes;
    /* number of bytes consumed from handshake */
    /* message for message type and header length */
    SSL3HandshakeType msg_type;
    unsigned long msg_len;
    PRBool isResuming;      /* we are resuming (not used in TLS 1.3) */
    PRBool usedStepDownKey; /* we did a server key exchange. */
    PRBool sendingSCSV;     /* instead of empty RI */
    sslBuffer msgState;     /* current state for handshake messages*/
                            /* protected by recvBufLock */

    /* The session ticket received in a NewSessionTicket message is temporarily
     * stored in newSessionTicket until the handshake is finished; then it is
     * moved to the sid.
     */
    PRBool receivedNewSessionTicket;
    NewSessionTicket newSessionTicket;

    PRUint16 finishedBytes; /* size of single finished below */
    union {
        TLSFinished tFinished[2]; /* client, then server */
        SSL3Finished sFinished[2];
        SSL3Opaque data[72];
    } finishedMsgs;

    PRBool authCertificatePending;
    /* Which function should SSL_RestartHandshake* call if we're blocked?
     * One of NULL, ssl3_SendClientSecondRound, ssl3_FinishHandshake,
     * or ssl3_AlwaysFail */
    sslRestartTarget restartTarget;
    /* Shared state between ssl3_HandleFinished and ssl3_FinishHandshake */
    PRBool cacheSID;

    PRBool canFalseStart; /* Can/did we False Start */
    /* Which preliminaryinfo values have been set. */
    PRUint32 preliminaryInfo;

    PRBool peerSupportsFfdheGroups; /* if the peer supports named ffdhe groups */

    /* clientSigAndHash contains the contents of the signature_algorithms
     * extension (if any) from the client. This is only valid for TLS 1.2
     * or later. */
    SSLSignatureAndHashAlg *clientSigAndHash;
    unsigned int numClientSigAndHash;

    /* This group of values is used for DTLS */
    PRUint16 sendMessageSeq;       /* The sending message sequence
                                    * number */
    PRCList lastMessageFlight;     /* The last message flight we
                                    * sent */
    PRUint16 maxMessageSent;       /* The largest message we sent */
    PRUint16 recvMessageSeq;       /* The receiving message sequence
                                    * number */
    sslBuffer recvdFragments;      /* The fragments we have received in
                                    * a bitmask */
    PRInt32 recvdHighWater;        /* The high water mark for fragments
                                    * received. -1 means no reassembly
                                    * in progress. */
    unsigned char cookie[32];      /* The cookie */
    unsigned char cookieLen;       /* The length of the cookie */
    PRIntervalTime rtTimerStarted; /* When the timer was started */
    DTLSTimerCb rtTimerCb;         /* The function to call on expiry */
    PRUint32 rtTimeoutMs;          /* The length of the current timeout
                                    * used for backoff (in ms) */
    PRUint32 rtRetries;            /* The retry counter */

    /* This group of values is used for TLS 1.3 and above */
    PRCList remoteKeyShares;           /* The other side's public keys */
    PK11SymKey *xSS;                   /* Extracted static secret */
    PK11SymKey *xES;                   /* Extracted ephemeral secret */
    PK11SymKey *trafficSecret;         /* The source key to use to generate
                                        * traffic keys */
    PK11SymKey *clientFinishedSecret;  /* Used for client Finished */
    PK11SymKey *serverFinishedSecret;  /* Used for server Finished */
    unsigned char certReqContext[255]; /* Ties CertificateRequest
                                        * to Certificate */
    PRUint8 certReqContextLen;         /* Length of the context
                                        * cannot be greater than 255. */
    ssl3CipherSuite origCipherSuite;   /* The cipher suite from the original
                                        * connection if we are resuming. */
    PRCList cipherSpecs;               /* The cipher specs in the sequence they
                                        * will be applied. */
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
    ssl3CipherSpec *crSpec; /* current read spec. */
    ssl3CipherSpec *prSpec; /* pending read spec. */
    ssl3CipherSpec *cwSpec; /* current write spec. */
    ssl3CipherSpec *pwSpec; /* pending write spec. */

    CERTCertificate *clientCertificate;   /* used by client */
    SECKEYPrivateKey *clientPrivateKey;   /* used by client */
    CERTCertificateList *clientCertChain; /* used by client */
    PRBool sendEmptyCert;                 /* used by client */

    int policy;
    /* This says what cipher suites we can do, and should
     * be either SSL_ALLOWED or SSL_RESTRICTED
     */
    PLArenaPool *peerCertArena;
    /* These are used to keep track of the peer CA */
    void *peerCertChain;
    /* chain while we are trying to validate it.   */
    CERTDistNames *ca_list;
    /* used by server.  trusted CAs for this socket. */
    PRBool initialized;
    SSL3HandshakeState hs;
    ssl3CipherSpec specs[2]; /* one is current, one is pending. */

    /* In a client: if the server supports Next Protocol Negotiation, then
     * this is the protocol that was negotiated.
     */
    SECItem nextProto;
    SSLNextProtoState nextProtoState;

    PRUint16 mtu; /* Our estimate of the MTU */

    /* DTLS-SRTP cipher suite preferences (if any) */
    PRUint16 dtlsSRTPCiphers[MAX_DTLS_SRTP_CIPHER_SUITES];
    PRUint16 dtlsSRTPCipherCount;
    PRUint16 dtlsSRTPCipherSuite; /* 0 if not selected */
    PRBool fatalAlertSent;
    PRBool dheWeakGroupEnabled; /* used by server */

    /* TLS 1.2 introduces separate signature algorithm negotiation.
     * This is our preference order. */
    SSLSignatureAndHashAlg signatureAlgorithms[MAX_SIGNATURE_ALGORITHMS];
    unsigned int signatureAlgorithmCount;

    /* The version to check if we fell back from our highest version
     * of TLS. Default is 0 in which case we check against the maximum
     * configured version for this socket. Used only on the client. */
    SSL3ProtocolVersion downgradeCheckVersion;
};

/* Ethernet MTU but without subtracting the headers,
 * so slightly larger than expected */
#define DTLS_MAX_MTU 1500U
#define IS_DTLS(ss) (ss->protocolVariant == ssl_variant_datagram)

typedef struct {
    SSL3ContentType type;
    SSL3ProtocolVersion version;
    SSL3SequenceNumber seq_num; /* DTLS only */
    sslBuffer *buf;
} SSL3Ciphertext;

struct sslKeyPairStr {
    SECKEYPrivateKey *privKey;
    SECKEYPublicKey *pubKey;
    PRInt32 refCount; /* use PR_Atomic calls for this. */
};

typedef struct {
    PRCList link;
    const namedGroupDef *group;
    sslKeyPair *keys;
} sslEphemeralKeyPair;

struct ssl3DHParamsStr {
    NamedGroup name;
    SECItem prime; /* p */
    SECItem base;  /* g */
};

typedef struct SSLWrappedSymWrappingKeyStr {
    SSL3Opaque wrappedSymmetricWrappingkey[512];
    CK_MECHANISM_TYPE symWrapMechanism;
    /* unwrapped symmetric wrapping key uses this mechanism */
    CK_MECHANISM_TYPE asymWrapMechanism;
    /* mechanism used to wrap the SymmetricWrappingKey using
     * server's public and/or private keys. */
    SSLAuthType authType; /* type of keys used to wrap SymWrapKey*/
    PRInt32 symWrapMechIndex;
    PRUint16 wrappedSymKeyLen;
} SSLWrappedSymWrappingKey;

typedef struct SessionTicketStr {
    PRUint16 ticket_version;
    SSL3ProtocolVersion ssl_version;
    ssl3CipherSuite cipher_suite;
    SSLCompressionMethod compression_method;
    SSLAuthType authType;
    PRUint32 authKeyBits;
    SSLKEAType keaType;
    PRUint32 keaKeyBits;
    sslServerCertType certType;
    /*
     * msWrapMech contains a meaningful value only if ms_is_wrapped is true.
     */
    PRUint8 ms_is_wrapped;
    CK_MECHANISM_TYPE msWrapMech;
    PRUint16 ms_length;
    SSL3Opaque master_secret[48];
    PRBool extendedMasterSecretUsed;
    ClientIdentity client_identity;
    SECItem peer_cert;
    PRUint32 timestamp;
    SECItem srvName; /* negotiated server name */
} SessionTicket;

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
** firstHandshakeLock AND ssl3HandshakeLock
*/
struct sslConnectInfoStr {
    /* outgoing handshakes appended to this. */
    sslBuffer sendBuf; /*xmitBufLock*/

    PRIPv6Addr peer;
    unsigned short port;

    sslSessionID *sid;
};

/* Note: The entire content of this struct and whatever it points to gets
 * blown away by SSL_ResetHandshake().  This is "sec" as in "ss->sec".
 *
 * Unless otherwise specified below, the contents of this struct are
 * protected by firstHandshakeLock AND ssl3HandshakeLock.
 */
struct sslSecurityInfoStr {
    int isServer;       /* Spec Lock?*/
    sslBuffer writeBuf; /*xmitBufLock*/

    int cipherType;
    int keyBits;
    int secretKeyBits;
    CERTCertificate *localCert;
    CERTCertificate *peerCert;
    SECKEYPublicKey *peerKey;

    SSLAuthType authType;
    PRUint32 authKeyBits;
    SSLKEAType keaType;
    PRUint32 keaKeyBits;
    /* The selected certificate (for servers only). */
    const sslServerCert *serverCert;

    /*
    ** Procs used for SID cache (nonce) management.
    ** Different implementations exist for clients/servers
    ** The lookup proc is only used for servers.  Baloney!
    */
    sslSessionIDCacheFunc cache;
    sslSessionIDUncacheFunc uncache;

    /* These are used during a connection handshake */
    sslConnectInfo ci;
};

/*
** SSL Socket struct
**
** Protection:  XXX
*/
struct sslSocketStr {
    PRFileDesc *fd;

    /* Pointer to operations vector for this socket */
    const sslSocketOps *ops;

    /* SSL socket options */
    sslOptions opt;
    /* Enabled version range */
    SSLVersionRange vrange;

    /* State flags */
    unsigned long clientAuthRequested;
    unsigned long delayDisabled;     /* Nagle delay disabled */
    unsigned long firstHsDone;       /* first handshake is complete. */
    unsigned long enoughFirstHsDone; /* enough of the first handshake is
                                      * done for callbacks to be able to
                                      * retrieve channel security
                                      * parameters from the SSL socket. */
    unsigned long handshakeBegun;
    unsigned long lastWriteBlocked;
    unsigned long recvdCloseNotify; /* received SSL EOF. */
    unsigned long TCPconnected;
    unsigned long appDataBuffered;
    unsigned long peerRequestedProtection; /* from old renegotiation */

    /* version of the protocol to use */
    SSL3ProtocolVersion version;
    SSL3ProtocolVersion clientHelloVersion; /* version sent in client hello. */

    sslSecurityInfo sec; /* not a pointer any more */

    /* protected by firstHandshakeLock AND ssl3HandshakeLock. */
    const char *url;

    sslHandshakeFunc handshake; /*firstHandshakeLock*/

    /* the following variable is only used with socks or other proxies. */
    char *peerID; /* String uniquely identifies target server. */

    sslKeyPair *stepDownKeyPair; /* RSA step down keys */

    /* ECDHE and DHE keys: In TLS 1.3, we might have to maintain multiple of
     * these on the client side.  The server inserts a single value into this
     * list for all versions. */
    PRCList /*<sslEphemeralKeyPair>*/ ephemeralKeyPairs;

    /* Callbacks */
    SSLAuthCertificate authCertificate;
    void *authCertificateArg;
    SSLGetClientAuthData getClientAuthData;
    void *getClientAuthDataArg;
    SSLSNISocketConfig sniSocketConfig;
    void *sniSocketConfigArg;
    SSLBadCertHandler handleBadCert;
    void *badCertArg;
    SSLHandshakeCallback handshakeCallback;
    void *handshakeCallbackData;
    SSLCanFalseStartCallback canFalseStartCallback;
    void *canFalseStartCallbackData;
    void *pkcs11PinArg;
    SSLNextProtoCallback nextProtoCallback;
    void *nextProtoArg;

    PRIntervalTime rTimeout; /* timeout for NSPR I/O */
    PRIntervalTime wTimeout; /* timeout for NSPR I/O */
    PRIntervalTime cTimeout; /* timeout for NSPR I/O */

    PZLock *recvLock; /* lock against multiple reader threads. */
    PZLock *sendLock; /* lock against multiple sender threads. */

    PZMonitor *recvBufLock; /* locks low level recv buffers. */
    PZMonitor *xmitBufLock; /* locks low level xmit buffers. */

    /* Only one thread may operate on the socket until the initial handshake
    ** is complete.  This Monitor ensures that.  Since SSL2 handshake is
    ** only done once, this is also effectively the SSL2 handshake lock.
    */
    PZMonitor *firstHandshakeLock;

    /* This monitor protects the ssl3 handshake state machine data.
    ** Only one thread (reader or writer) may be in the ssl3 handshake state
    ** machine at any time.  */
    PZMonitor *ssl3HandshakeLock;

    /* reader/writer lock, protects the secret data needed to encrypt and MAC
    ** outgoing records, and to decrypt and MAC check incoming ciphertext
    ** records.  */
    NSSRWLock *specLock;

    /* handle to perm cert db (and implicitly to the temp cert db) used
    ** with this socket.
    */
    CERTCertDBHandle *dbHandle;

    PRThread *writerThread; /* thread holds SSL_LOCK_WRITER lock */

    PRUint16 shutdownHow; /* See ssl_SHUTDOWN defines below. */

    sslHandshakingType handshaking;

    /* Gather object used for gathering data */
    sslGather gs; /*recvBufLock*/

    sslBuffer saveBuf;    /*xmitBufLock*/
    sslBuffer pendingBuf; /*xmitBufLock*/

    /* Configuration state for server sockets */
    /* One server cert and key for each authentication type. */
    PRCList /* <sslServerCert> */ serverCerts;

    ssl3CipherSuiteCfg cipherSuites[ssl_V3_SUITES_IMPLEMENTED];
    /* This bit mask determines what EC and FFDHE groups are enabled.  This
     * starts with all being enabled and can be modified either by negotiation
     * (in which case groups not supported by a peer are masked off), or by
     * calling SSL_DHEGroupPrefSet(), which will alter the mask for FFDHE. */
    PRUint32 namedGroups;

    /* SSL3 state info.  Formerly was a pointer */
    ssl3State ssl3;

    /*
     * TLS extension related data.
     */
    /* True when the current session is a stateless resume. */
    PRBool statelessResume;
    TLSExtensionData xtnData;

    /* Whether we are doing stream or datagram mode */
    SSLProtocolVariant protocolVariant;
};

/* All the global data items declared here should be protected using the
** ssl_global_data_lock, which is a reader/writer lock.
*/
extern NSSRWLock *ssl_global_data_lock;
extern char ssl_debug;
extern char ssl_trace;
extern FILE *ssl_trace_iob;
extern FILE *ssl_keylog_iob;
extern CERTDistNames *ssl3_server_ca_list;
extern PRUint32 ssl_sid_timeout;
extern PRUint32 ssl3_sid_timeout;

extern const char *const ssl3_cipherName[];

extern sslSessionIDLookupFunc ssl_sid_lookup;
extern sslSessionIDCacheFunc ssl_sid_cache;
extern sslSessionIDUncacheFunc ssl_sid_uncache;

extern const namedGroupDef ssl_named_groups[];
extern const unsigned int ssl_named_group_count;

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
extern sslGather *ssl_NewGather(void);
extern SECStatus ssl3_InitGather(sslGather *gs);
extern void ssl3_DestroyGather(sslGather *gs);
extern SECStatus ssl_GatherRecord1stHandshake(sslSocket *ss);

extern SECStatus ssl_CreateSecurityInfo(sslSocket *ss);
extern SECStatus ssl_CopySecurityInfo(sslSocket *ss, sslSocket *os);
extern void ssl_ResetSecurityInfo(sslSecurityInfo *sec, PRBool doMemset);
extern void ssl_DestroySecurityInfo(sslSecurityInfo *sec);

extern void ssl_PrintBuf(sslSocket *ss, const char *msg, const void *cp, int len);
extern void ssl_PrintKey(sslSocket *ss, const char *msg, PK11SymKey *key);

extern int ssl_SendSavedWriteData(sslSocket *ss);
extern SECStatus ssl_SaveWriteData(sslSocket *ss,
                                   const void *p, unsigned int l);
extern SECStatus ssl_BeginClientHandshake(sslSocket *ss);
extern SECStatus ssl_BeginServerHandshake(sslSocket *ss);
extern int ssl_Do1stHandshake(sslSocket *ss);

extern SECStatus sslBuffer_Grow(sslBuffer *b, unsigned int newLen);
extern SECStatus sslBuffer_Append(sslBuffer *b, const void *data,
                                  unsigned int len);
extern void sslBuffer_Clear(sslBuffer *b);

extern void ssl_ChooseSessionIDProcs(sslSecurityInfo *sec);

extern void ssl3_InitCipherSpec(ssl3CipherSpec *spec);
extern sslSessionID *ssl3_NewSessionID(sslSocket *ss, PRBool is_server);
extern sslSessionID *ssl_LookupSID(const PRIPv6Addr *addr, PRUint16 port,
                                   const char *peerID, const char *urlSvrName);
extern void ssl_FreeSID(sslSessionID *sid);

extern int ssl3_SendApplicationData(sslSocket *ss, const PRUint8 *in,
                                    int len, int flags);

extern PRBool ssl_FdIsBlocking(PRFileDesc *fd);

extern PRBool ssl_SocketIsBlocking(sslSocket *ss);

extern void ssl3_SetAlwaysBlock(sslSocket *ss);

extern SECStatus ssl_EnableNagleDelay(sslSocket *ss, PRBool enabled);

extern void ssl_FinishHandshake(sslSocket *ss);

extern SECStatus ssl_CipherPolicySet(PRInt32 which, PRInt32 policy);

extern SECStatus ssl_CipherPrefSetDefault(PRInt32 which, PRBool enabled);

extern SECStatus ssl3_ConstrainRangeByPolicy(void);

extern SECStatus ssl3_InitState(sslSocket *ss);
extern SECStatus ssl3_RestartHandshakeHashes(sslSocket *ss);
extern SECStatus ssl3_UpdateHandshakeHashes(sslSocket *ss,
                                            const unsigned char *b,
                                            unsigned int l);

/* Returns PR_TRUE if we are still waiting for the server to complete its
 * response to our client second round. Once we've received the Finished from
 * the server then there is no need to check false start.
 */
extern PRBool ssl3_WaitingForServerSecondRound(sslSocket *ss);

extern SECStatus
ssl3_CompressMACEncryptRecord(ssl3CipherSpec *cwSpec,
                              PRBool isServer,
                              PRBool isDTLS,
                              PRBool capRecordVersion,
                              SSL3ContentType type,
                              const SSL3Opaque *pIn,
                              PRUint32 contentLen,
                              sslBuffer *wrBuf);

extern PRInt32 ssl3_SendRecord(sslSocket *ss, ssl3CipherSpec *cwSpec,
                               SSL3ContentType type,
                               const SSL3Opaque *pIn, PRInt32 nIn,
                               PRInt32 flags);

#ifdef NSS_SSL_ENABLE_ZLIB
/*
 * The DEFLATE algorithm can result in an expansion of 0.1% + 12 bytes. For a
 * maximum TLS record payload of 2**14 bytes, that's 29 bytes.
 */
#define SSL3_COMPRESSION_MAX_EXPANSION 29
#else /* !NSS_SSL_ENABLE_ZLIB */
#define SSL3_COMPRESSION_MAX_EXPANSION 0
#endif

/*
 * make sure there is room in the write buffer for padding and
 * other compression and cryptographic expansions.
 */
#define SSL3_BUFFER_FUDGE 100 + SSL3_COMPRESSION_MAX_EXPANSION

#define SSL_LOCK_READER(ss) \
    if (ss->recvLock)       \
    PZ_Lock(ss->recvLock)
#define SSL_UNLOCK_READER(ss) \
    if (ss->recvLock)         \
    PZ_Unlock(ss->recvLock)
#define SSL_LOCK_WRITER(ss) \
    if (ss->sendLock)       \
    PZ_Lock(ss->sendLock)
#define SSL_UNLOCK_WRITER(ss) \
    if (ss->sendLock)         \
    PZ_Unlock(ss->sendLock)

/* firstHandshakeLock -> recvBufLock */
#define ssl_Get1stHandshakeLock(ss)                               \
    {                                                             \
        if (!ss->opt.noLocks) {                                   \
            PORT_Assert(PZ_InMonitor((ss)->firstHandshakeLock) || \
                        !ssl_HaveRecvBufLock(ss));                \
            PZ_EnterMonitor((ss)->firstHandshakeLock);            \
        }                                                         \
    }
#define ssl_Release1stHandshakeLock(ss)               \
    {                                                 \
        if (!ss->opt.noLocks)                         \
            PZ_ExitMonitor((ss)->firstHandshakeLock); \
    }
#define ssl_Have1stHandshakeLock(ss) \
    (PZ_InMonitor((ss)->firstHandshakeLock))

/* ssl3HandshakeLock -> xmitBufLock */
#define ssl_GetSSL3HandshakeLock(ss)                  \
    {                                                 \
        if (!ss->opt.noLocks) {                       \
            PORT_Assert(!ssl_HaveXmitBufLock(ss));    \
            PZ_EnterMonitor((ss)->ssl3HandshakeLock); \
        }                                             \
    }
#define ssl_ReleaseSSL3HandshakeLock(ss)             \
    {                                                \
        if (!ss->opt.noLocks)                        \
            PZ_ExitMonitor((ss)->ssl3HandshakeLock); \
    }
#define ssl_HaveSSL3HandshakeLock(ss) \
    (PZ_InMonitor((ss)->ssl3HandshakeLock))

#define ssl_GetSpecReadLock(ss)                 \
    {                                           \
        if (!ss->opt.noLocks)                   \
            NSSRWLock_LockRead((ss)->specLock); \
    }
#define ssl_ReleaseSpecReadLock(ss)               \
    {                                             \
        if (!ss->opt.noLocks)                     \
            NSSRWLock_UnlockRead((ss)->specLock); \
    }
/* NSSRWLock_HaveReadLock is not exported so there's no
 * ssl_HaveSpecReadLock macro. */

#define ssl_GetSpecWriteLock(ss)                 \
    {                                            \
        if (!ss->opt.noLocks)                    \
            NSSRWLock_LockWrite((ss)->specLock); \
    }
#define ssl_ReleaseSpecWriteLock(ss)               \
    {                                              \
        if (!ss->opt.noLocks)                      \
            NSSRWLock_UnlockWrite((ss)->specLock); \
    }
#define ssl_HaveSpecWriteLock(ss) \
    (NSSRWLock_HaveWriteLock((ss)->specLock))

/* recvBufLock -> ssl3HandshakeLock -> xmitBufLock */
#define ssl_GetRecvBufLock(ss)                           \
    {                                                    \
        if (!ss->opt.noLocks) {                          \
            PORT_Assert(!ssl_HaveSSL3HandshakeLock(ss)); \
            PORT_Assert(!ssl_HaveXmitBufLock(ss));       \
            PZ_EnterMonitor((ss)->recvBufLock);          \
        }                                                \
    }
#define ssl_ReleaseRecvBufLock(ss)             \
    {                                          \
        if (!ss->opt.noLocks)                  \
            PZ_ExitMonitor((ss)->recvBufLock); \
    }
#define ssl_HaveRecvBufLock(ss) \
    (PZ_InMonitor((ss)->recvBufLock))

/* xmitBufLock -> specLock */
#define ssl_GetXmitBufLock(ss)                  \
    {                                           \
        if (!ss->opt.noLocks)                   \
            PZ_EnterMonitor((ss)->xmitBufLock); \
    }
#define ssl_ReleaseXmitBufLock(ss)             \
    {                                          \
        if (!ss->opt.noLocks)                  \
            PZ_ExitMonitor((ss)->xmitBufLock); \
    }
#define ssl_HaveXmitBufLock(ss) \
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
#ifdef NSS_ENABLE_TLS_1_3
#define SSL_LIBRARY_VERSION_MAX_SUPPORTED SSL_LIBRARY_VERSION_TLS_1_3
#else
#define SSL_LIBRARY_VERSION_MAX_SUPPORTED SSL_LIBRARY_VERSION_TLS_1_2
#endif

#define SSL_ALL_VERSIONS_DISABLED(vrange) \
    ((vrange)->min == SSL_LIBRARY_VERSION_NONE)

extern PRBool ssl3_VersionIsSupported(SSLProtocolVariant protocolVariant,
                                      SSL3ProtocolVersion version);

extern SECStatus ssl3_KeyAndMacDeriveBypass(ssl3CipherSpec *pwSpec,
                                            const unsigned char *cr, const unsigned char *sr,
                                            PRBool isTLS, HASH_HashType tls12HashType, PRBool isExport);
extern SECStatus ssl3_MasterSecretDeriveBypass(ssl3CipherSpec *pwSpec,
                                               const unsigned char *cr, const unsigned char *sr,
                                               const SECItem *pms, PRBool isTLS,
                                               HASH_HashType tls12HashType, PRBool isRSA);

/* These functions are called from secnav, even though they're "private". */

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
    sslSocket *ss, unsigned char *buffer, int length, PRUint8 padding);

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

/* Create a new ref counted key pair object from two keys. */
extern sslKeyPair *ssl_NewKeyPair(SECKEYPrivateKey *privKey,
                                  SECKEYPublicKey *pubKey);

/* get a new reference (bump ref count) to an ssl3KeyPair. */
extern sslKeyPair *ssl_GetKeyPairRef(sslKeyPair *keyPair);

/* Decrement keypair's ref count and free if zero. */
extern void ssl_FreeKeyPair(sslKeyPair *keyPair);

extern sslEphemeralKeyPair *ssl_NewEphemeralKeyPair(
    const namedGroupDef *group,
    SECKEYPrivateKey *privKey, SECKEYPublicKey *pubKey);
extern sslEphemeralKeyPair *ssl_CopyEphemeralKeyPair(
    sslEphemeralKeyPair *keyPair);
extern void ssl_FreeEphemeralKeyPair(sslEphemeralKeyPair *keyPair);
extern sslEphemeralKeyPair *ssl_LookupEphemeralKeyPair(
    sslSocket *ss, const namedGroupDef *groupDef);
extern void ssl_FreeEphemeralKeyPairs(sslSocket *ss);

extern SECStatus ssl_AppendPaddedDHKeyShare(sslSocket *ss,
                                            SECKEYPublicKey *pubKey);
extern const ssl3DHParams *ssl_GetDHEParams(const namedGroupDef *groupDef);
extern SECStatus ssl_SelectDHEParams(sslSocket *ss,
                                     const namedGroupDef **groupDef,
                                     const ssl3DHParams **params);
extern SECStatus ssl_CreateDHEKeyPair(const namedGroupDef *groupDef,
                                      const ssl3DHParams *params,
                                      sslEphemeralKeyPair **keyPair);
extern PRBool ssl_IsValidDHEShare(const SECItem *dh_p, const SECItem *dh_Ys);
extern SECStatus ssl_ValidateDHENamedGroup(sslSocket *ss,
                                           const SECItem *dh_p,
                                           const SECItem *dh_g,
                                           const namedGroupDef **groupDef,
                                           const ssl3DHParams **dhParams);

extern PRBool ssl3_IsECCEnabled(sslSocket *ss);

/* Macro for finding a curve equivalent in strength to RSA key's */
/* clang-format off */
#define SSL_RSASTRENGTH_TO_ECSTRENGTH(s)                                       \
        ((s <= 1024) ? 160                                                     \
                     : ((s <= 2048) ? 224                                      \
                                    : ((s <= 3072) ? 256                       \
                                                   : ((s <= 7168) ? 384        \
                                                                  : 521 ) ) ) )
/* clang-format on */

extern const namedGroupDef *ssl_LookupNamedGroup(NamedGroup group);
extern PRBool ssl_NamedGroupEnabled(const sslSocket *ss, const namedGroupDef *group);
extern SECStatus ssl_NamedGroup2ECParams(PLArenaPool *arena,
                                         const namedGroupDef *curve,
                                         SECKEYECParams *params);
extern const namedGroupDef *ssl_ECPubKey2NamedGroup(
    const SECKEYPublicKey *pubKey);

extern const namedGroupDef *ssl_GetECGroupWithStrength(PRUint32 curvemsk,
                                                       int requiredECCbits);
extern const namedGroupDef *ssl_GetECGroupForServerSocket(sslSocket *ss);
extern PRBool ssl_SuiteBOnly(sslSocket *ss);

extern SECStatus ssl3_CipherPrefSetDefault(ssl3CipherSuite which, PRBool on);
extern SECStatus ssl3_CipherPrefGetDefault(ssl3CipherSuite which, PRBool *on);

extern SECStatus ssl3_CipherPrefSet(sslSocket *ss, ssl3CipherSuite which, PRBool on);
extern SECStatus ssl3_CipherPrefGet(sslSocket *ss, ssl3CipherSuite which, PRBool *on);

extern SECStatus ssl3_SetPolicy(ssl3CipherSuite which, PRInt32 policy);
extern SECStatus ssl3_GetPolicy(ssl3CipherSuite which, PRInt32 *policy);

extern void ssl3_InitSocketPolicy(sslSocket *ss);
extern void ssl3_InitCipherSpec(ssl3CipherSpec *spec);

extern SECStatus ssl3_RedoHandshake(sslSocket *ss, PRBool flushCache);
extern SECStatus ssl3_HandleHandshakeMessage(sslSocket *ss, SSL3Opaque *b,
                                             PRUint32 length,
                                             PRBool endOfRecord);

extern void ssl3_DestroySSL3Info(sslSocket *ss);

extern SECStatus ssl3_NegotiateVersion(sslSocket *ss,
                                       SSL3ProtocolVersion peerVersion,
                                       PRBool allowLargerPeerVersion);

extern SECStatus ssl_GetPeerInfo(sslSocket *ss);

/* ECDH functions */
extern SECStatus ssl3_SendECDHClientKeyExchange(sslSocket *ss,
                                                SECKEYPublicKey *svrPubKey);
extern SECStatus ssl3_HandleECDHServerKeyExchange(sslSocket *ss,
                                                  SSL3Opaque *b, PRUint32 length);
extern SECStatus ssl3_HandleECDHClientKeyExchange(sslSocket *ss,
                                                  SSL3Opaque *b, PRUint32 length,
                                                  sslKeyPair *serverKeys);
extern SECStatus ssl3_SendECDHServerKeyExchange(
    sslSocket *ss, const SSLSignatureAndHashAlg *sigAndHash);
extern SECStatus tls13_ImportECDHKeyShare(
    sslSocket *ss, SECKEYPublicKey *peerKey,
    SSL3Opaque *b, PRUint32 length, const namedGroupDef *curve);
unsigned int tls13_SizeOfECDHEKeyShareKEX(const SECKEYPublicKey *pubKey);
SECStatus tls13_EncodeECDHEKeyShareKEX(sslSocket *ss,
                                       const SECKEYPublicKey *pubKey);

extern SECStatus ssl3_ComputeCommonKeyHash(SSLHashType hashAlg,
                                           PRUint8 *hashBuf,
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
extern SECStatus ssl3_AppendHandshakeVariable(sslSocket *ss,
                                              const SSL3Opaque *src, PRInt32 bytes, PRInt32 lenSize);
extern SECStatus ssl3_AppendSignatureAndHashAlgorithm(
    sslSocket *ss, const SSLSignatureAndHashAlg *sigAndHash);
extern SECStatus ssl3_ConsumeHandshake(sslSocket *ss, void *v, PRInt32 bytes,
                                       SSL3Opaque **b, PRUint32 *length);
extern PRInt32 ssl3_ConsumeHandshakeNumber(sslSocket *ss, PRInt32 bytes,
                                           SSL3Opaque **b, PRUint32 *length);
extern SECStatus ssl3_ConsumeHandshakeVariable(sslSocket *ss, SECItem *i,
                                               PRInt32 bytes, SSL3Opaque **b, PRUint32 *length);
extern PRBool ssl3_IsSupportedSignatureAlgorithm(
    const SSLSignatureAndHashAlg *alg);
extern SECStatus ssl3_CheckSignatureAndHashAlgorithmConsistency(
    sslSocket *ss, const SSLSignatureAndHashAlg *sigAndHash,
    CERTCertificate *cert);
extern SECStatus ssl3_ConsumeSignatureAndHashAlgorithm(
    sslSocket *ss, SSL3Opaque **b, PRUint32 *length,
    SSLSignatureAndHashAlg *out);
extern SECStatus ssl3_SignHashes(SSL3Hashes *hash, SECKEYPrivateKey *key,
                                 SECItem *buf, PRBool isTLS);
extern SECStatus ssl3_VerifySignedHashes(SSL3Hashes *hash,
                                         CERTCertificate *cert, SECItem *buf, PRBool isTLS,
                                         void *pwArg);
extern SECStatus ssl3_CacheWrappedMasterSecret(
    sslSocket *ss, sslSessionID *sid,
    ssl3CipherSpec *spec, SSLAuthType authType);
extern void ssl3_FreeSniNameArray(TLSExtensionData *xtnData);

/* Functions that handle ClientHello and ServerHello extensions. */
extern SECStatus ssl3_HandleServerNameXtn(sslSocket *ss,
                                          PRUint16 ex_type, SECItem *data);
extern SECStatus ssl_HandleSupportedGroupsXtn(sslSocket *ss,
                                              PRUint16 ex_type, SECItem *data);
extern SECStatus ssl3_HandleSupportedPointFormatsXtn(sslSocket *ss,
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

extern PRInt32 ssl_SendSupportedGroupsXtn(sslSocket *ss,
                                          PRBool append, PRUint32 maxBytes);
extern PRInt32 ssl3_SendSupportedPointFormatsXtn(sslSocket *ss,
                                                 PRBool append, PRUint32 maxBytes);

/* call the registered extension handlers. */
extern SECStatus ssl3_HandleHelloExtensions(sslSocket *ss,
                                            SSL3Opaque **b, PRUint32 *length,
                                            SSL3HandshakeType handshakeMessage);

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
extern SECStatus ssl3_SessionTicketShutdown(void *appData, void *nssData);

/* Tell clients to consider tickets valid for this long. */
#define TLS_EX_SESS_TICKET_LIFETIME_HINT (2 * 24 * 60 * 60) /* 2 days */
#define TLS_EX_SESS_TICKET_VERSION (0x0102)

extern SECStatus ssl3_ValidateNextProtoNego(const unsigned char *data,
                                            unsigned int length);

/* Construct a new NSPR socket for the app to use */
extern PRFileDesc *ssl_NewPRSocket(sslSocket *ss, PRFileDesc *fd);
extern void ssl_FreePRSocket(PRFileDesc *fd);

/* Internal config function so SSL3 can initialize the present state of
 * various ciphers */
extern int ssl3_config_match_init(sslSocket *);

/* calls for accessing wrapping keys across processes. */
extern PRBool
ssl_GetWrappingKey(PRInt32 symWrapMechIndex, SSLAuthType authType,
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
extern void dtls_FreeHandshakeMessage(DTLSQueuedMessage *msg);
extern void dtls_FreeHandshakeMessages(PRCList *lst);

extern SECStatus dtls_HandleHandshake(sslSocket *ss, sslBuffer *origBuf);
extern SECStatus dtls_HandleHelloVerifyRequest(sslSocket *ss,
                                               SSL3Opaque *b, PRUint32 length);
extern SECStatus dtls_StageHandshakeMessage(sslSocket *ss);
extern SECStatus dtls_QueueMessage(sslSocket *ss, SSL3ContentType type,
                                   const SSL3Opaque *pIn, PRInt32 nIn);
extern SECStatus dtls_FlushHandshakeMessages(sslSocket *ss, PRInt32 flags);
extern SECStatus dtls_CompressMACEncryptRecord(sslSocket *ss,
                                               ssl3CipherSpec *cwSpec,
                                               SSL3ContentType type,
                                               const SSL3Opaque *pIn,
                                               PRUint32 contentLen,
                                               sslBuffer *wrBuf);
SECStatus ssl3_DisableNonDTLSSuites(sslSocket *ss);
extern SECStatus dtls_StartHolddownTimer(sslSocket *ss);
extern void dtls_CheckTimer(sslSocket *ss);
extern void dtls_CancelTimer(sslSocket *ss);
extern void dtls_SetMTU(sslSocket *ss, PRUint16 advertised);
extern void dtls_InitRecvdRecords(DTLSRecvdRecords *records);
extern int dtls_RecordGetRecvd(const DTLSRecvdRecords *records, PRUint64 seq);
extern void dtls_RecordSetRecvd(DTLSRecvdRecords *records, PRUint64 seq);
extern void dtls_RehandshakeCleanup(sslSocket *ss);
extern SSL3ProtocolVersion
dtls_TLSVersionToDTLSVersion(SSL3ProtocolVersion tlsv);
extern SSL3ProtocolVersion
dtls_DTLSVersionToTLSVersion(SSL3ProtocolVersion dtlsv);
extern PRBool dtls_IsRelevant(sslSocket *ss, const ssl3CipherSpec *crSpec,
                              const SSL3Ciphertext *cText, PRUint64 *seqNum);
extern SECStatus dtls_MaybeRetransmitHandshake(sslSocket *ss,
                                               const SSL3Ciphertext *cText);

CK_MECHANISM_TYPE ssl3_Alg2Mech(SSLCipherAlgorithm calg);
SECStatus ssl3_NegotiateCipherSuite(sslSocket *ss, const SECItem *suites);
SECStatus ssl3_ServerCallSNICallback(sslSocket *ss);
SECStatus ssl3_SetupPendingCipherSpec(sslSocket *ss);
SECStatus ssl3_FlushHandshake(sslSocket *ss, PRInt32 flags);
SECStatus ssl3_SendCertificate(sslSocket *ss);
SECStatus ssl3_CompleteHandleCertificate(sslSocket *ss,
                                         SSL3Opaque *b, PRUint32 length);
SECStatus ssl3_SendEmptyCertificate(sslSocket *ss);
SECStatus ssl3_SendCertificateStatus(sslSocket *ss);
SECStatus ssl3_CompleteHandleCertificateStatus(sslSocket *ss, SSL3Opaque *b,
                                               PRUint32 length);
SECStatus ssl3_EncodeCertificateRequestSigAlgs(sslSocket *ss, PRUint8 *buf,
                                               unsigned maxLen, PRUint32 *len);
void ssl3_GetCertificateRequestCAs(sslSocket *ss, int *calenp, SECItem **namesp,
                                   int *nnamesp);
SECStatus ssl3_ParseCertificateRequestCAs(sslSocket *ss, SSL3Opaque **b,
                                          PRUint32 *length, PLArenaPool *arena,
                                          CERTDistNames *ca_list);
SECStatus ssl3_CompleteHandleCertificateRequest(sslSocket *ss,
                                                SECItem *algorithms,
                                                CERTDistNames *ca_list);
SECStatus ssl3_SendCertificateVerify(sslSocket *ss,
                                     SECKEYPrivateKey *privKey);
SECStatus ssl3_SendServerHello(sslSocket *ss);
SECOidTag ssl3_TLSHashAlgorithmToOID(SSLHashType hashFunc);
SECStatus ssl3_ComputeHandshakeHashes(sslSocket *ss,
                                      ssl3CipherSpec *spec,
                                      SSL3Hashes *hashes,
                                      PRUint32 sender);
void ssl3_BumpSequenceNumber(SSL3SequenceNumber *num);
PRInt32 tls13_ServerSendKeyShareXtn(sslSocket *ss, PRBool append,
                                    PRUint32 maxBytes);
SECStatus ssl_CreateECDHEphemeralKeyPair(const namedGroupDef *ecGroup,
                                         sslEphemeralKeyPair **keyPair);
SECStatus ssl3_FlushHandshake(sslSocket *ss, PRInt32 flags);
PK11SymKey *ssl3_GetWrappingKey(sslSocket *ss,
                                PK11SlotInfo *masterSecretSlot,
                                const sslServerCert *serverCert,
                                CK_MECHANISM_TYPE masterWrapMech,
                                void *pwArg);
PRInt32 tls13_ServerSendPreSharedKeyXtn(sslSocket *ss,
                                        PRBool append,
                                        PRUint32 maxBytes);
PRBool ssl3_ClientExtensionAdvertised(sslSocket *ss, PRUint16 ex_type);
SECStatus ssl3_FillInCachedSID(sslSocket *ss, sslSessionID *sid);
const ssl3CipherSuiteDef *ssl_LookupCipherSuiteDef(ssl3CipherSuite suite);
SECStatus ssl3_SelectServerCert(sslSocket *ss);

/* Pull in TLS 1.3 functions */
#include "tls13con.h"

/********************** misc calls *********************/

#ifdef DEBUG
extern void ssl3_CheckCipherSuiteOrderConsistency();
#endif

extern int ssl_MapLowLevelError(int hiLevelError);

extern PRUint32 ssl_Time(void);

extern void SSL_AtomicIncrementLong(long *x);

SECStatus SSL_DisableDefaultExportCipherSuites(void);
SECStatus SSL_DisableExportCipherSuites(PRFileDesc *fd);
PRBool SSL_IsExportCipherSuite(PRUint16 cipherSuite);

SECStatus ssl3_ApplyNSSPolicy(void);

extern HASH_HashType
ssl3_GetTls12HashType(sslSocket *ss);

extern SECStatus
ssl3_TLSPRFWithMasterSecret(ssl3CipherSpec *spec,
                            const char *label, unsigned int labelLen,
                            const unsigned char *val, unsigned int valLen,
                            unsigned char *out, unsigned int outLen,
                            HASH_HashType tls12HashType);
extern SECOidTag
ssl3_TLSHashAlgorithmToOID(SSLHashType hashFunc);

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
