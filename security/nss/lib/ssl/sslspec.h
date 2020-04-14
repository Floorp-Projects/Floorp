/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __sslspec_h_
#define __sslspec_h_

#include "sslexp.h"
#include "prclist.h"

typedef enum {
    TrafficKeyClearText = 0,
    TrafficKeyEarlyApplicationData = 1,
    TrafficKeyHandshake = 2,
    TrafficKeyApplicationData = 3
} TrafficKeyType;

#define SPEC_DIR(spec) \
    ((spec->direction == ssl_secret_read) ? "read" : "write")

typedef struct ssl3CipherSpecStr ssl3CipherSpec;
typedef struct ssl3BulkCipherDefStr ssl3BulkCipherDef;
typedef struct ssl3MACDefStr ssl3MACDef;
typedef struct ssl3CipherSuiteDefStr ssl3CipherSuiteDef;
typedef PRUint64 sslSequenceNumber;
typedef PRUint16 DTLSEpoch;

/* The SSL bulk cipher definition */
typedef enum {
    cipher_null,
    cipher_rc4,
    cipher_des,
    cipher_3des,
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

typedef enum {
    type_stream,
    type_block,
    type_aead
} CipherType;

/*
** There are tables of these, all const.
*/
struct ssl3BulkCipherDefStr {
    SSL3BulkCipher cipher;
    SSLCipherAlgorithm calg;
    unsigned int key_size;
    unsigned int secret_key_size;
    CipherType type;
    unsigned int iv_size;
    unsigned int block_size;
    unsigned int tag_size;            /* for AEAD ciphers. */
    unsigned int explicit_nonce_size; /* for AEAD ciphers. */
    SECOidTag oid;
    const char *short_name;
    /* The maximum number of records that can be sent/received with the same
      * symmetric key before the connection will be terminated. */
    PRUint64 max_records;
};

/* to make some of these old enums public without namespace pollution,
** it was necessary to prepend ssl_ to the names.
** These #defines preserve compatibility with the old code here in libssl.
*/
typedef SSLMACAlgorithm SSL3MACAlgorithm;

/*
 * There are tables of these, all const.
 */
struct ssl3MACDefStr {
    SSL3MACAlgorithm mac;
    CK_MECHANISM_TYPE mmech;
    int pad_size;
    int mac_size;
    SECOidTag oid;
};

#define MAX_IV_LENGTH 24

typedef struct {
    PK11SymKey *key;
    PK11SymKey *macKey;
    PK11Context *macContext;
    PRUint8 iv[MAX_IV_LENGTH];
} ssl3KeyMaterial;

typedef SECStatus (*SSLCipher)(void *context,
                               unsigned char *out,
                               unsigned int *outlen,
                               unsigned int maxout,
                               const unsigned char *in,
                               unsigned int inlen);
typedef SECStatus (*SSLAEADCipher)(PK11Context *context,
                                   CK_GENERATOR_FUNCTION ivGen,
                                   unsigned int fixedbits,
                                   unsigned char *iv, unsigned int ivlen,
                                   const unsigned char *aad,
                                   unsigned int aadlen,
                                   unsigned char *out, unsigned int *outlen,
                                   unsigned int maxout, unsigned char *tag,
                                   unsigned int taglen,
                                   const unsigned char *in, unsigned int inlen);

/* The DTLS anti-replay window in number of packets. Defined here because we
 * need it in the cipher spec. Note that this is a ring buffer but left and
 * right represent the true window, with modular arithmetic used to map them
 * onto the buffer.
 */
#define DTLS_RECVD_RECORDS_WINDOW 1024
#define RECORD_SEQ_MASK ((1ULL << 48) - 1)
#define RECORD_SEQ_MAX RECORD_SEQ_MASK
PR_STATIC_ASSERT(DTLS_RECVD_RECORDS_WINDOW % 8 == 0);

typedef struct DTLSRecvdRecordsStr {
    unsigned char data[DTLS_RECVD_RECORDS_WINDOW / 8];
    sslSequenceNumber left;
    sslSequenceNumber right;
} DTLSRecvdRecords;

/*
 * These are the "specs" used for reading and writing records.  Access to the
 * pointers to these specs, and all the specs' contents (direct and indirect) is
 * protected by the reader/writer lock ss->specLock.
 */
struct ssl3CipherSpecStr {
    PRCList link;
    PRUint8 refCt;

    SSLSecretDirection direction;
    SSL3ProtocolVersion version;
    SSL3ProtocolVersion recordVersion;

    const ssl3BulkCipherDef *cipherDef;
    const ssl3MACDef *macDef;

    SSLCipher cipher;
    void *cipherContext;

    PK11SymKey *masterSecret;
    ssl3KeyMaterial keyMaterial;

    DTLSEpoch epoch;
    const char *phase;

    /* The next sequence number to be sent or received. */
    sslSequenceNumber nextSeqNum;
    DTLSRecvdRecords recvdRecords;

    /* The number of 0-RTT bytes that can be sent or received in TLS 1.3. This
     * will be zero for everything but 0-RTT. */
    PRUint32 earlyDataRemaining;
    /* The maximum plaintext length.  This differs from the configured or
     * negotiated value for TLS 1.3; it is reduced by one to account for the
     * content type octet. */
    PRUint16 recordSizeLimit;

    /* Masking context used for DTLS 1.3 */
    SSLMaskingContext *maskContext;
};

typedef void (*sslCipherSpecChangedFunc)(void *arg,
                                         PRBool sending,
                                         ssl3CipherSpec *newSpec);

const ssl3BulkCipherDef *ssl_GetBulkCipherDef(const ssl3CipherSuiteDef *cipher_def);
const ssl3MACDef *ssl_GetMacDefByAlg(SSL3MACAlgorithm mac);
const ssl3MACDef *ssl_GetMacDef(const sslSocket *ss, const ssl3CipherSuiteDef *suiteDef);

ssl3CipherSpec *ssl_CreateCipherSpec(sslSocket *ss, SSLSecretDirection direction);
void ssl_SaveCipherSpec(sslSocket *ss, ssl3CipherSpec *spec);
void ssl_CipherSpecAddRef(ssl3CipherSpec *spec);
void ssl_CipherSpecRelease(ssl3CipherSpec *spec);
void ssl_DestroyCipherSpecs(PRCList *list);
SECStatus ssl_SetupNullCipherSpec(sslSocket *ss, SSLSecretDirection dir);

ssl3CipherSpec *ssl_FindCipherSpecByEpoch(sslSocket *ss,
                                          SSLSecretDirection direction,
                                          DTLSEpoch epoch);
void ssl_CipherSpecReleaseByEpoch(sslSocket *ss, SSLSecretDirection direction,
                                  DTLSEpoch epoch);

#endif /* __sslspec_h_ */
