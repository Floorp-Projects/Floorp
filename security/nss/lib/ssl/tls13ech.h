/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13ech_h_
#define __tls13ech_h_

#include "pk11hpke.h"

/* draft-09, supporting shared-mode and split-mode as a backend server only.
 * Notes on the implementation status:
 * - Padding (https://tools.ietf.org/html/draft-ietf-tls-esni-08#section-6.2),
 *   is not implemented (see bug 1677181).
 * - When multiple ECHConfigs are provided by the server, the first compatible
 *   config is selected by the client. Ciphersuite choices are limited and only
 *   the AEAD may vary (AES-128-GCM or ChaCha20Poly1305).
 * - Some of the buffering (construction/compression/decompression) could likely
 *   be optimized, but the spec is still evolving so that work is deferred.
 */
#define TLS13_ECH_VERSION 0xfe0d
#define TLS13_ECH_SIGNAL_LEN 8
#define TLS13_ECH_AEAD_TAG_LEN 16

static const char kHpkeInfoEch[] = "tls ech";
static const char hHkdfInfoEchConfigID[] = "tls ech config id";
static const char kHkdfInfoEchConfirm[] = "ech accept confirmation";
static const char kHkdfInfoEchHrrConfirm[] = "hrr ech accept confirmation";

typedef enum {
    ech_xtn_type_outer = 0,
    ech_xtn_type_inner = 1,
} EchXtnType;

struct sslEchConfigContentsStr {
    PRUint8 configId;
    HpkeKemId kemId;
    SECItem publicKey; /* NULL on server. Use the keypair in sslEchConfig instead. */
    HpkeKdfId kdfId;
    HpkeAeadId aeadId;
    SECItem suites; /* One or more HpkeCipherSuites. The selected s
                     * suite is placed in kdfId and aeadId. */
    PRUint8 maxNameLen;
    char *publicName;
    /* No supported extensions. */
};

/* ECH Information needed by a server to process a second CH after a
 * HelloRetryRequest is sent. This data is stored in the cookie. 
 */
struct sslEchCookieDataStr {
    PRBool previouslyOffered;
    PRUint8 configId;
    HpkeKdfId kdfId;
    HpkeAeadId aeadId;
    HpkeContext *hpkeCtx;
    sslBuffer signal;
};

struct sslEchConfigStr {
    PRCList link;
    SECItem raw;
    PRUint16 version;
    sslEchConfigContents contents;
};

struct sslEchXtnStateStr {
    SECItem innerCh;          /* Server: ClientECH.payload */
    SECItem senderPubKey;     /* Server: ClientECH.enc */
    PRUint8 configId;         /* Server: ClientECH.config_id  */
    HpkeKdfId kdfId;          /* Server: ClientECH.cipher_suite.kdf */
    HpkeAeadId aeadId;        /* Server: ClientECH.cipher_suite.aead */
    SECItem retryConfigs;     /* Client: ServerECH.retry_configs*/
    PRBool retryConfigsValid; /* Client: Extraction of retry_configss is allowed.
                               *  This is set once the handshake completes (having
                               *  verified to the ECHConfig public name). */
    PRUint8 *hrrConfirmation; /* Client/Server: HRR Confirmation Location */
    PRBool receivedInnerXtn;  /* Server: Handled ECH Xtn with Inner Enum */
    PRUint8 *payloadStart;    /* Server: Start of ECH Payload*/
};

SECStatus SSLExp_EncodeEchConfigId(PRUint8 configId, const char *publicName, unsigned int maxNameLen,
                                   HpkeKemId kemId, const SECKEYPublicKey *pubKey,
                                   const HpkeSymmetricSuite *hpkeSuites, unsigned int hpkeSuiteCount,
                                   PRUint8 *out, unsigned int *outlen, unsigned int maxlen);
SECStatus SSLExp_GetEchRetryConfigs(PRFileDesc *fd, SECItem *retryConfigs);
SECStatus SSLExp_SetClientEchConfigs(PRFileDesc *fd, const PRUint8 *echConfigs,
                                     unsigned int echConfigsLen);
SECStatus SSLExp_SetServerEchConfigs(PRFileDesc *fd,
                                     const SECKEYPublicKey *pubKey, const SECKEYPrivateKey *privKey,
                                     const PRUint8 *echConfigs, unsigned int numEchConfigs);
SECStatus SSLExp_RemoveEchConfigs(PRFileDesc *fd);

SECStatus tls13_ClientSetupEch(sslSocket *ss, sslClientHelloType type);
SECStatus tls13_ConstructClientHelloWithEch(sslSocket *ss, const sslSessionID *sid,
                                            PRBool freshSid, sslBuffer *chOuterBuf,
                                            sslBuffer *chInnerXtnsBuf);
SECStatus tls13_CopyEchConfigs(PRCList *oconfigs, PRCList *configs);
SECStatus tls13_DecodeEchConfigs(const SECItem *data, PRCList *configs);
void tls13_DestroyEchConfigs(PRCList *list);
void tls13_DestroyEchXtnState(sslEchXtnState *state);
SECStatus tls13_GetMatchingEchConfig(const sslSocket *ss, HpkeKdfId kdf, HpkeAeadId aead,
                                     const SECItem *configId, sslEchConfig **cfg);
SECStatus tls13_MaybeHandleEch(sslSocket *ss, const PRUint8 *msg, PRUint32 msgLen, SECItem *sidBytes,
                               SECItem *comps, SECItem *cookieBytes, SECItem *suites, SECItem **echInner);
SECStatus tls13_MaybeHandleEchSignal(sslSocket *ss, const PRUint8 *savedMsg, PRUint32 savedLength, PRBool isHrr);
SECStatus tls13_MaybeAcceptEch(sslSocket *ss, const SECItem *sidBytes, const PRUint8 *chOuter,
                               unsigned int chOuterLen, SECItem **chInner);
SECStatus tls13_MaybeGreaseEch(sslSocket *ss, const sslBuffer *preamble, sslBuffer *buf);
SECStatus tls13_WriteServerEchSignal(sslSocket *ss, PRUint8 *sh, unsigned int shLen);
SECStatus tls13_WriteServerEchHrrSignal(sslSocket *ss, PRUint8 *sh, unsigned int shLen);
SECStatus tls13_DeriveEchSecret(const sslSocket *ss, PK11SymKey **output);
SECStatus tls13_ComputeEchSignal(sslSocket *ss, PRBool isHrr, const PRUint8 *sh, unsigned int shLen, PRUint8 *out);

PRBool tls13_IsIp(const PRUint8 *str, unsigned int len);
PRBool tls13_IsLDH(const PRUint8 *str, unsigned int len);

#endif
