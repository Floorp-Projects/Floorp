/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * TLS 1.3 Protocol
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdarg.h"
#include "cert.h"
#include "ssl.h"
#include "keyhi.h"
#include "pk11func.h"
#include "secitem.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "sslerr.h"
#include "tls13hkdf.h"
#include "tls13con.h"

typedef enum {
    TrafficKeyEarlyData,
    TrafficKeyHandshake,
    TrafficKeyApplicationData
} TrafficKeyType;

typedef enum {
    InstallCipherSpecRead,
    InstallCipherSpecWrite,
    InstallCipherSpecBoth
} InstallCipherSpecDirection;

#define MAX_FINISHED_SIZE 64

static SECStatus tls13_InitializeHandshakeEncryption(sslSocket *ss);
static SECStatus tls13_InstallCipherSpec(
    sslSocket *ss, InstallCipherSpecDirection direction);
static SECStatus tls13_InitCipherSpec(
    sslSocket *ss, TrafficKeyType type, InstallCipherSpecDirection install);
static SECStatus tls13_AESGCM(
    ssl3KeyMaterial *keys,
    PRBool doDecrypt,
    unsigned char *out, int *outlen, int maxout,
    const unsigned char *in, int inlen,
    const unsigned char *additionalData, int additionalDataLen);
static SECStatus tls13_SendEncryptedExtensions(sslSocket *ss);
static SECStatus tls13_HandleEncryptedExtensions(sslSocket *ss, SSL3Opaque *b,
                                                 PRUint32 length);
static SECStatus tls13_HandleCertificate(
    sslSocket *ss, SSL3Opaque *b, PRUint32 length);
static SECStatus tls13_HandleCertificateRequest(sslSocket *ss, SSL3Opaque *b,
                                                PRUint32 length);
static SECStatus tls13_HandleCertificateStatus(sslSocket *ss, SSL3Opaque *b,
                                               PRUint32 length);
static SECStatus tls13_HandleCertificateVerify(
    sslSocket *ss, SSL3Opaque *b, PRUint32 length,
    SSL3Hashes *hashes);
static SECStatus tls13_HkdfExtractSharedKey(sslSocket *ss, PK11SymKey *key,
                                            SharedSecretType keyType);
static SECStatus tls13_SendFinished(sslSocket *ss);
static SECStatus tls13_HandleFinished(sslSocket *ss, SSL3Opaque *b, PRUint32 length,
                                      const SSL3Hashes *hashes);
static SECStatus tls13_HandleNewSessionTicket(sslSocket *ss, SSL3Opaque *b,
                                              PRUint32 length);
static SECStatus tls13_ComputeSecrets1(sslSocket *ss);
static SECStatus tls13_ComputeFinished(
    sslSocket *ss, const SSL3Hashes *hashes,
    PRBool sending,
    PRUint8 *output, unsigned int *outputLen,
    unsigned int maxOutputLen);
static SECStatus tls13_SendClientSecondRound(sslSocket *ss);
static SECStatus tls13_FinishHandshake(sslSocket *ss);

const char kHkdfLabelExpandedSs[] = "expanded static secret";
const char kHkdfLabelExpandedEs[] = "expanded ephemeral secret";
const char kHkdfLabelMasterSecret[] = "master secret";
const char kHkdfLabelTrafficSecret[] = "traffic secret";
const char kHkdfLabelClientFinishedSecret[] = "client finished";
const char kHkdfLabelServerFinishedSecret[] = "server finished";
const char kHkdfLabelResumptionMasterSecret[] = "resumption master secret";
const char kHkdfLabelExporterMasterSecret[] = "exporter master secret";
const char kHkdfPhaseEarlyHandshakeDataKeys[] = "early handshake key expansion";
const char kHkdfPhaseEarlyApplicationDataKeys[] = "early application data key expansion";
const char kHkdfPhaseHandshakeKeys[] = "handshake key expansion";
const char kHkdfPhaseApplicationDataKeys[] = "application data key expansion";
const char kHkdfPurposeClientWriteKey[] = "client write key";
const char kHkdfPurposeServerWriteKey[] = "server write key";
const char kHkdfPurposeClientWriteIv[] = "client write iv";
const char kHkdfPurposeServerWriteIv[] = "server write iv";
const char kClientFinishedLabel[] = "client finished";
const char kServerFinishedLabel[] = "server finished";

const SSL3ProtocolVersion kRecordVersion = 0x0301U;

#define FATAL_ERROR(ss, prError, desc)                                             \
    do {                                                                           \
        SSL_TRC(3, ("%d: TLS13[%d]: fatal error %d in %s (%s:%d)",                 \
                    SSL_GETPID(), ss->fd, prError, __func__, __FILE__, __LINE__)); \
        tls13_FatalError(ss, prError, desc);                                       \
    } while (0)

#define UNIMPLEMENTED()                                                   \
    do {                                                                  \
        SSL_TRC(3, ("%d: TLS13[%d]: unimplemented feature in %s (%s:%d)", \
                    SSL_GETPID(), ss->fd, __func__, __FILE__, __LINE__)); \
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);                         \
        PORT_Assert(0);                                                   \
        return SECFailure;                                                \
    } while (0)

void
tls13_FatalError(sslSocket *ss, PRErrorCode prError, SSL3AlertDescription desc)
{
    PORT_Assert(desc != internal_error); /* These should never happen */
    (void)SSL3_SendAlert(ss, alert_fatal, desc);
    PORT_SetError(prError);
}

#ifdef TRACE
#define STATE_CASE(a) \
    case a:           \
        return #a
static char *
tls13_HandshakeState(SSL3WaitState st)
{
    switch (st) {
        STATE_CASE(wait_client_hello);
        STATE_CASE(wait_client_cert);
        STATE_CASE(wait_cert_verify);
        STATE_CASE(wait_finished);
        STATE_CASE(wait_server_hello);
        STATE_CASE(wait_certificate_status);
        STATE_CASE(wait_server_cert);
        STATE_CASE(wait_cert_request);
        STATE_CASE(wait_encrypted_extensions);
        STATE_CASE(idle_handshake);
        default:
            break;
    }
    PORT_Assert(0);
    return "unknown";
}
#endif

#define TLS13_WAIT_STATE_MASK 0x80

#define TLS13_BASE_WAIT_STATE(ws) (ws & ~TLS13_WAIT_STATE_MASK)
/* We don't mask idle_handshake because other parts of the code use it*/
#define TLS13_WAIT_STATE(ws) (ws == idle_handshake ? ws : ws | TLS13_WAIT_STATE_MASK)
#define TLS13_CHECK_HS_STATE(ss, err, ...)                          \
    tls13_CheckHsState(ss, err, #err, __func__, __FILE__, __LINE__, \
                       __VA_ARGS__,                                 \
                       wait_invalid)
void
tls13_SetHsState(sslSocket *ss, SSL3WaitState ws,
                 const char *func, const char *file, int line)
{
#ifdef TRACE
    const char *new_state_name =
        tls13_HandshakeState(ws);

    SSL_TRC(3, ("%d: TLS13[%d]: state change from %s->%s in %s (%s:%d)",
                SSL_GETPID(), ss->fd,
                tls13_HandshakeState(TLS13_BASE_WAIT_STATE(ss->ssl3.hs.ws)),
                new_state_name,
                func, file, line));
#endif

    ss->ssl3.hs.ws = TLS13_WAIT_STATE(ws);
}

static PRBool
tls13_InHsStateV(sslSocket *ss, va_list ap)
{
    SSL3WaitState ws;

    while ((ws = va_arg(ap, SSL3WaitState)) != wait_invalid) {
        if (ws == TLS13_BASE_WAIT_STATE(ss->ssl3.hs.ws)) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

PRBool
tls13_InHsState(sslSocket *ss, ...)
{
    PRBool found;
    va_list ap;

    va_start(ap, ss);
    found = tls13_InHsStateV(ss, ap);
    va_end(ap);

    return found;
}

static SECStatus
tls13_CheckHsState(sslSocket *ss, int err, const char *error_name,
                   const char *func, const char *file, int line,
                   ...)
{
    va_list ap;
    va_start(ap, line);
    if (tls13_InHsStateV(ss, ap)) {
        va_end(ap);
        return SECSuccess;
    }
    va_end(ap);

    SSL_TRC(3, ("%d: TLS13[%d]: error %s state is (%s) at %s (%s:%d)",
                SSL_GETPID(), ss->fd,
                error_name,
                tls13_HandshakeState(TLS13_BASE_WAIT_STATE(ss->ssl3.hs.ws)),
                func, file, line));
    tls13_FatalError(ss, err, unexpected_message);
    return SECFailure;
}

SSLHashType
tls13_GetHash(sslSocket *ss)
{
    /* TODO(ekr@rtfm.com): This needs to actually be looked up. */
    return ssl_hash_sha256;
}

CK_MECHANISM_TYPE
tls13_GetHkdfMechanism(sslSocket *ss)
{
    /* TODO(ekr@rtfm.com): This needs to actually be looked up. */
    return CKM_NSS_HKDF_SHA256;
}

static CK_MECHANISM_TYPE
tls13_GetHmacMechanism(sslSocket *ss)
{
    /* TODO(ekr@rtfm.com): This needs to actually be looked up. */
    return CKM_SHA256_HMAC;
}

/*
 * Called from ssl3_SendClientHello
 */
SECStatus
tls13_SetupClientHello(sslSocket *ss)
{
    SECStatus rv;
    /* TODO(ekr@rtfm.com): Handle multiple curves here. */
    ECName curves_to_try[] = { ec_secp256r1 };

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    PORT_Assert(!ss->ephemeralECDHKeyPair);

    rv = ssl3_CreateECDHEphemeralKeyPair(curves_to_try[0],
                                         &ss->ephemeralECDHKeyPair);
    if (rv != SECSuccess)
        return rv;

    return SECSuccess;
}

static SECStatus
tls13_HandleECDHEKeyShare(sslSocket *ss,
                          TLS13KeyShareEntry *entry,
                          SECKEYPrivateKey *privKey,
                          SharedSecretType type)
{
    SECStatus rv;
    SECKEYPublicKey *peerKey;
    PK11SymKey *shared;

    peerKey = tls13_ImportECDHKeyShare(ss, entry->key_exchange.data,
                                       entry->key_exchange.len,
                                       entry->group);
    if (!peerKey)
        return SECFailure; /* Error code set already. */

    /* Compute shared key. */
    shared = tls13_ComputeECDHSharedKey(ss, privKey, peerKey);
    SECKEY_DestroyPublicKey(peerKey);
    if (!shared) {
        return SECFailure; /* Error code set already. */
    }

    /* Extract key. */
    rv = tls13_HkdfExtractSharedKey(ss, shared, type);
    PK11_FreeSymKey(shared);

    return rv;
}

SECStatus
tls13_HandlePostHelloHandshakeMessage(sslSocket *ss, SSL3Opaque *b,
                                      PRUint32 length, SSL3Hashes *hashesPtr)
{
    /* TODO(ekr@rtfm.com): Would it be better to check all the states here? */
    switch (ss->ssl3.hs.msg_type) {
        case certificate:
            return tls13_HandleCertificate(ss, b, length);

        case certificate_status:
            return tls13_HandleCertificateStatus(ss, b, length);

        case certificate_request:
            return tls13_HandleCertificateRequest(ss, b, length);

        case certificate_verify:
            return tls13_HandleCertificateVerify(ss, b, length, hashesPtr);

        case encrypted_extensions:
            return tls13_HandleEncryptedExtensions(ss, b, length);

        case new_session_ticket:
            return tls13_HandleNewSessionTicket(ss, b, length);

        case finished:
            return tls13_HandleFinished(ss, b, length, hashesPtr);

        default:
            FATAL_ERROR(ss, SSL_ERROR_RX_UNKNOWN_HANDSHAKE, unexpected_message);
            return SECFailure;
    }

    PORT_Assert(0); /* Unreached */
    return SECFailure;
}

/* Called from ssl3_HandleClientHello.
 *
 * Caller must hold Handshake and RecvBuf locks.
 */
SECStatus
tls13_HandleClientKeyShare(sslSocket *ss)
{
    ECName expectedGroup;
    SECStatus rv;
    TLS13KeyShareEntry *found = NULL;
    PRCList *cur_p;

    SSL_TRC(3, ("%d: TLS13[%d]: handle client_key_share handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    rv = ssl3_SetupPendingCipherSpec(ss);
    if (rv != SECSuccess)
        return SECFailure; /* Error code set below */

    /* Figure out what group we expect */
    switch (ss->ssl3.hs.kea_def->exchKeyType) {
#ifndef NSS_DISABLE_ECC
        case ssl_kea_ecdh:
            expectedGroup = ssl3_GetCurveNameForServerSocket(ss);
            if (!expectedGroup) {
                FATAL_ERROR(ss, SSL_ERROR_NO_CYPHER_OVERLAP,
                            handshake_failure);
                return SECFailure;
            }
            break;
#endif
        default:
            /* Got an unknown or unsupported Key Exchange Algorithm.
             * Can't happen. */
            FATAL_ERROR(ss, SEC_ERROR_UNSUPPORTED_KEYALG,
                        internal_error);
            return SECFailure;
    }

    /* Now walk through the keys until we find one for our group */
    cur_p = PR_NEXT_LINK(&ss->ssl3.hs.remoteKeyShares);
    while (cur_p != &ss->ssl3.hs.remoteKeyShares) {
        TLS13KeyShareEntry *offer = (TLS13KeyShareEntry *)cur_p;

        if (offer->group == expectedGroup) {
            found = offer;
            break;
        }
        cur_p = PR_NEXT_LINK(cur_p);
    }

    if (!found) {
        /* No acceptable group. In future, we will need to correct the client.
         * Currently just generate an error.
         * TODO(ekr@rtfm.com): Write code to correct client.
         */
        FATAL_ERROR(ss, SSL_ERROR_NO_CYPHER_OVERLAP, handshake_failure);
        return SECFailure;
    }

    /* Generate our key */
    rv = ssl3_CreateECDHEphemeralKeyPair(expectedGroup, &ss->ephemeralECDHKeyPair);
    if (rv != SECSuccess)
        return rv;

    ss->sec.keaType = ss->ssl3.hs.kea_def->exchKeyType;
    ss->sec.keaKeyBits = SECKEY_PublicKeyStrengthInBits(
        ss->ephemeralECDHKeyPair->pubKey);

    /* Register the sender */
    rv = ssl3_RegisterServerHelloExtensionSender(ss, ssl_tls13_key_share_xtn,
                                                 tls13_ServerSendKeyShareXtn);
    if (rv != SECSuccess)
        return SECFailure; /* Error code set below */

    rv = tls13_HandleECDHEKeyShare(ss, found,
                                   ss->ephemeralECDHKeyPair->privKey,
                                   EphemeralSharedSecret);
    if (rv != SECSuccess)
        return SECFailure; /* Error code set below */

    return SECSuccess;
}

/*
 *     [draft-ietf-tls-tls13-11] Section 6.3.3.2
 *
 *     opaque DistinguishedName<1..2^16-1>;
 *
 *     struct {
 *         opaque certificate_extension_oid<1..2^8-1>;
 *         opaque certificate_extension_values<0..2^16-1>;
 *     } CertificateExtension;
 *
 *     struct {
 *         opaque certificate_request_context<0..2^8-1>;
 *         SignatureAndHashAlgorithm
 *           supported_signature_algorithms<2..2^16-2>;
 *         DistinguishedName certificate_authorities<0..2^16-1>;
 *         CertificateExtension certificate_extensions<0..2^16-1>;
 *     } CertificateRequest;
 */
static SECStatus
tls13_SendCertificateRequest(sslSocket *ss)
{
    SECStatus rv;
    int calen;
    SECItem *names;
    int nnames;
    SECItem *name;
    int i;
    PRUint8 sigAlgs[MAX_SIGNATURE_ALGORITHMS * 2];
    unsigned int sigAlgsLength = 0;
    int length;

    SSL_TRC(3, ("%d: TLS13[%d]: begin send certificate_request",
                SSL_GETPID(), ss->fd));

    /* Fixed context value. */
    ss->ssl3.hs.certReqContext[0] = 0;
    ss->ssl3.hs.certReqContextLen = 1;

    rv = ssl3_EncodeCertificateRequestSigAlgs(ss, sigAlgs, sizeof(sigAlgs),
                                              &sigAlgsLength);
    if (rv != SECSuccess) {
        return rv;
    }

    ssl3_GetCertificateRequestCAs(ss, &calen, &names, &nnames);
    length = 1 + ss->ssl3.hs.certReqContextLen +
             2 + sigAlgsLength + 2 + calen + 2;

    rv = ssl3_AppendHandshakeHeader(ss, certificate_request, length);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    rv = ssl3_AppendHandshakeVariable(ss, ss->ssl3.hs.certReqContext,
                                      ss->ssl3.hs.certReqContextLen, 1);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    rv = ssl3_AppendHandshakeVariable(ss, sigAlgs, sigAlgsLength, 2);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    rv = ssl3_AppendHandshakeNumber(ss, calen, 2);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    for (i = 0, name = names; i < nnames; i++, name++) {
        rv = ssl3_AppendHandshakeVariable(ss, name->data, name->len, 2);
        if (rv != SECSuccess) {
            return rv; /* err set by AppendHandshake. */
        }
    }
    rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }

    return SECSuccess;
}

static SECStatus
tls13_HandleCertificateRequest(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;
    SECItem context = { siBuffer, NULL, 0 };
    SECItem algorithms = { siBuffer, NULL, 0 };
    PLArenaPool *arena;
    CERTDistNames ca_list;
    PRInt32 extensionsLength;

    SSL_TRC(3, ("%d: TLS13[%d]: handle certificate_request sequence",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    /* Client */
    rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST, wait_cert_request);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    PORT_Assert(ss->ssl3.clientCertChain == NULL);
    PORT_Assert(ss->ssl3.clientCertificate == NULL);
    PORT_Assert(ss->ssl3.clientPrivateKey == NULL);

    rv = ssl3_ConsumeHandshakeVariable(ss, &context, 1, &b, &length);
    if (rv != SECSuccess)
        return SECFailure;
    PORT_Assert(sizeof(ss->ssl3.hs.certReqContext) == 255);
    PORT_Memcpy(ss->ssl3.hs.certReqContext, context.data, context.len);
    ss->ssl3.hs.certReqContextLen = context.len;

    rv = ssl3_ConsumeHandshakeVariable(ss, &algorithms, 2, &b, &length);
    if (rv != SECSuccess)
        return SECFailure;

    if (algorithms.len == 0 || (algorithms.len & 1) != 0) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CERT_REQUEST,
                    illegal_parameter);
        return SECFailure;
    }

    arena = ca_list.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    rv = ssl3_ParseCertificateRequestCAs(ss, &b, &length, arena, &ca_list);
    if (rv != SECSuccess)
        goto loser; /* alert sent below */

    /* Verify that the extensions length is correct. */
    extensionsLength = ssl3_ConsumeHandshakeNumber(ss, 2, &b, &length);
    if (extensionsLength < 0) {
        goto loser; /* alert sent below */
    }
    if (extensionsLength != length) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CERT_REQUEST,
                    illegal_parameter);
        goto loser;
    }

    TLS13_SET_HS_STATE(ss, wait_server_cert);

    rv = ssl3_CompleteHandleCertificateRequest(ss, &algorithms, &ca_list);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        goto loser;
    }

    return SECSuccess;

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return SECFailure;
}

static SECStatus
tls13_InitializeHandshakeEncryption(sslSocket *ss)
{
    SECStatus rv;

    /* For all present cipher suites, SS = ES.
     * TODO(ekr@rtfm.com): Revisit for 0-RTT. */
    ss->ssl3.hs.xSS = PK11_ReferenceSymKey(ss->ssl3.hs.xES);
    if (!ss->ssl3.hs.xSS) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    rv = tls13_InitCipherSpec(ss, TrafficKeyHandshake,
                              InstallCipherSpecBoth);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_INIT_CIPHER_SUITE_FAILURE, internal_error);
        return SECFailure;
    }

    return rv;
}

/* Called from:  ssl3_HandleClientHello */
SECStatus
tls13_SendServerHelloSequence(sslSocket *ss)
{
    SECStatus rv;
    SSL3KEAType certIndex;

    SSL_TRC(3, ("%d: TLS13[%d]: begin send server_hello sequence",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    rv = ssl3_SendServerHello(ss);
    if (rv != SECSuccess) {
        return rv; /* err code is set. */
    }

    rv = tls13_InitializeHandshakeEncryption(ss);
    if (rv != SECSuccess) {
        return SECFailure; /* error code is set. */
    }

    rv = tls13_SendEncryptedExtensions(ss);
    if (rv != SECSuccess) {
        return SECFailure; /* error code is set. */
    }

    if (ss->opt.requestCertificate) {
        rv = tls13_SendCertificateRequest(ss);
        if (rv != SECSuccess) {
            return SECFailure; /* error code is set. */
        }
    }
    rv = ssl3_SendCertificate(ss);
    if (rv != SECSuccess) {
        return SECFailure; /* error code is set. */
    }
    rv = ssl3_SendCertificateStatus(ss);
    if (rv != SECSuccess) {
        return SECFailure; /* error code is set. */
    }

    /* This was copied from: ssl3_SendCertificate.
     * TODO(ekr@rtfm.com): Verify that this selection logic is correct.
     * Bug 1237514.
    */
    if ((ss->ssl3.hs.kea_def->kea == kea_ecdhe_rsa) ||
        (ss->ssl3.hs.kea_def->kea == kea_dhe_rsa)) {
        certIndex = kt_rsa;
    } else {
        certIndex = ss->ssl3.hs.kea_def->exchKeyType;
    }
    rv = ssl3_SendCertificateVerify(ss, ss->serverCerts[certIndex].SERVERKEY);
    if (rv != SECSuccess) {
        return rv; /* err code is set. */
    }

    /* Compute the rest of the secrets except for the resumption
     * and exporter secret. */
    rv = tls13_ComputeSecrets1(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    rv = tls13_SendFinished(ss);
    if (rv != SECSuccess) {
        return rv; /* error code is set. */
    }

    TLS13_SET_HS_STATE(ss, ss->opt.requestCertificate ? wait_client_cert
                                                      : wait_finished);

    return SECSuccess;
}

/*
 * Called from ssl3_HandleServerHello.
 *
 * Caller must hold Handshake and RecvBuf locks.
 */
SECStatus
tls13_HandleServerKeyShare(sslSocket *ss)
{
    SECStatus rv;
    ECName expectedGroup;
    PRCList *cur_p;
    TLS13KeyShareEntry *entry;

    SSL_TRC(3, ("%d: TLS13[%d]: handle server_key_share handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    switch (ss->ssl3.hs.kea_def->exchKeyType) {
#ifndef NSS_DISABLE_ECC
        case ssl_kea_ecdh:
            expectedGroup = ssl3_PubKey2ECName(ss->ephemeralECDHKeyPair->pubKey);
            break;
#endif /* NSS_DISABLE_ECC */
        default:
            FATAL_ERROR(ss, SEC_ERROR_UNSUPPORTED_KEYALG, handshake_failure);
            return SECFailure;
    }

    /* This list should have one entry. */
    cur_p = PR_NEXT_LINK(&ss->ssl3.hs.remoteKeyShares);
    if (!cur_p) {
        FATAL_ERROR(ss, SSL_ERROR_MISSING_KEY_SHARE, missing_extension);
        return SECFailure;
    }
    PORT_Assert(PR_NEXT_LINK(cur_p) == &ss->ssl3.hs.remoteKeyShares);

    entry = (TLS13KeyShareEntry *)cur_p;
    if (entry->group != expectedGroup) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_KEY_SHARE, illegal_parameter);
        return SECFailure;
    }

    rv = tls13_HandleECDHEKeyShare(ss, entry,
                                   ss->ephemeralECDHKeyPair->privKey,
                                   EphemeralSharedSecret);

    ss->sec.keaType = ss->ssl3.hs.kea_def->exchKeyType;
    ss->sec.keaKeyBits = SECKEY_PublicKeyStrengthInBits(
        ss->ephemeralECDHKeyPair->pubKey);

    if (rv != SECSuccess)
        return SECFailure; /* Error code set below */

    return tls13_InitializeHandshakeEncryption(ss);
}

/* Called from tls13_CompleteHandleHandshakeMessage() when it has deciphered a complete
 * tls13 Certificate message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
tls13_HandleCertificate(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;
    SECItem context = { siBuffer, NULL, 0 };

    SSL_TRC(3, ("%d: TLS13[%d]: handle certificate handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->sec.isServer) {
        rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_CERTIFICATE,
                                  wait_client_cert);
    } else {
        rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_CERTIFICATE,
                                  wait_cert_request, wait_server_cert);
    }
    if (rv != SECSuccess)
        return SECFailure;

    /* Process the context string */
    rv = ssl3_ConsumeHandshakeVariable(ss, &context, 1, &b, &length);
    if (rv != SECSuccess)
        return SECFailure;
    if (!ss->sec.isServer) {
        if (context.len) {
            /* The server's context string MUST be empty */
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CERTIFICATE,
                        illegal_parameter);
            return SECFailure;
        }
    } else {
        if (!context.len || context.len != ss->ssl3.hs.certReqContextLen ||
            (NSS_SecureMemcmp(ss->ssl3.hs.certReqContext,
                              context.data, context.len) != 0)) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CERTIFICATE,
                        illegal_parameter);
            return SECFailure;
        }
        context.len = 0; /* Belt and suspenders. Zero out the context. */
    }

    return ssl3_CompleteHandleCertificate(ss, b, length);
}

/* Called from tls13_CompleteHandleHandshakeMessage() when it has deciphered a complete
 * ssl3 CertificateStatus message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
tls13_HandleCertificateStatus(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;

    rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_CERT_STATUS,
                              wait_certificate_status);
    if (rv != SECSuccess)
        return rv;

    return ssl3_CompleteHandleCertificateStatus(ss, b, length);
}

/*
 * TODO(ekr@rtfm.com): This install logic needs renaming since it's
 * what happens at various stages of cipher spec setup. Legacy from ssl3con.c.
 */
int
tls13_InstallCipherSpec(sslSocket *ss, InstallCipherSpecDirection direction)
{
    SSL_TRC(3, ("%d: TLS13[%d]: Installing new cipher specs direction = %s",
                SSL_GETPID(), ss->fd,
                direction == InstallCipherSpecRead ? "read" : "write"));

    PORT_Assert(!IS_DTLS(ss)); /* TODO(ekr@rtfm.com): Update for DTLS */
    /* TODO(ekr@rtfm.com): Holddown timer for DTLS. */
    ssl_GetSpecWriteLock(ss); /**************************************/

    /* Flush out any old stuff in the handshake buffers */
    switch (direction) {
        case InstallCipherSpecWrite: {
            ssl3CipherSpec *pwSpec;
            pwSpec = ss->ssl3.pwSpec;

            ss->ssl3.pwSpec = ss->ssl3.cwSpec;
            ss->ssl3.cwSpec = pwSpec;
            break;
        } break;
        case InstallCipherSpecRead: {
            ssl3CipherSpec *prSpec;

            prSpec = ss->ssl3.prSpec;
            ss->ssl3.prSpec = ss->ssl3.crSpec;
            ss->ssl3.crSpec = prSpec;
        } break;
        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            ssl_ReleaseSpecWriteLock(ss); /**************************************/
            return SECFailure;
    }

    /* If we are really through with the old cipher prSpec
     * (Both the read and write sides have changed) destroy it.
     */
    if (ss->ssl3.prSpec == ss->ssl3.pwSpec) {
        ssl3_DestroyCipherSpec(ss->ssl3.prSpec, PR_FALSE /*freeSrvName*/);
    }
    ssl_ReleaseSpecWriteLock(ss); /**************************************/

    return SECSuccess;
}

/* Add context to the hash functions as described in
   [draft-ietf-tls-tls13; Section 4.9.1] */
SECStatus
tls13_AddContextToHashes(sslSocket *ss, SSL3Hashes *hashes /* IN/OUT */,
                         SSLHashType algorithm, PRBool sending)
{
    SECStatus rv = SECSuccess;
    PK11Context *ctx;
    const unsigned char context_padding[] = {
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    };
    const char *client_cert_verify_string = "TLS 1.3, client CertificateVerify";
    const char *server_cert_verify_string = "TLS 1.3, server CertificateVerify";
    const char *context_string = (sending ^ ss->sec.isServer) ? client_cert_verify_string
                                                              : server_cert_verify_string;
    unsigned int hashlength;

    /* Double check that we are doing SHA-256 for the handshake hash.*/
    PORT_Assert(hashes->hashAlg == ssl_hash_sha256);
    if (hashes->hashAlg != ssl_hash_sha256) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    PORT_Assert(hashes->len == 32);

    ctx = PK11_CreateDigestContext(ssl3_TLSHashAlgorithmToOID(algorithm));
    if (!ctx) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    PORT_Assert(SECFailure);
    PORT_Assert(!SECSuccess);

    rv |= PK11_DigestBegin(ctx);
    rv |= PK11_DigestOp(ctx, context_padding, sizeof(context_padding));
    rv |= PK11_DigestOp(ctx, (unsigned char *)context_string,
                        strlen(context_string) + 1); /* +1 includes the terminating 0 */
    rv |= PK11_DigestOp(ctx, hashes->u.raw, hashes->len);
    /* Update the hash in-place */
    rv |= PK11_DigestFinal(ctx, hashes->u.raw, &hashlength, sizeof(hashes->u.raw));
    PK11_DestroyContext(ctx, PR_TRUE);
    PRINT_BUF(90, (NULL, "TLS 1.3 hash with context", hashes->u.raw, hashlength));

    hashes->len = hashlength;
    hashes->hashAlg = algorithm;

    if (rv) {
        ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
        goto loser;
    }
    return SECSuccess;

loser:
    return SECFailure;
}

static SECStatus
tls13_HkdfExtractSharedKey(sslSocket *ss, PK11SymKey *key,
                           SharedSecretType keyType)
{
    PK11SymKey **destp;

    switch (keyType) {
        case EphemeralSharedSecret:
            destp = &ss->ssl3.hs.xES;
            break;
        case StaticSharedSecret:
            destp = &ss->ssl3.hs.xSS;
            break;
        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
    }

    PORT_Assert(!*destp);
    return tls13_HkdfExtract(NULL, key, tls13_GetHash(ss), destp);
}

static SECStatus
tls13_DeriveTrafficKeys(sslSocket *ss, ssl3CipherSpec *pwSpec,
                        TrafficKeyType type)
{
    size_t keySize = pwSpec->cipher_def->key_size;
    size_t ivSize = pwSpec->cipher_def->iv_size +
                    pwSpec->cipher_def->explicit_nonce_size; /* This isn't always going to
                                                              * work, but it does for
                                                              * AES-GCM */
    CK_MECHANISM_TYPE bulkAlgorithm = ssl3_Alg2Mech(pwSpec->cipher_def->calg);
    SSL3Hashes hashes;
    PK11SymKey *prk = NULL;
    const char *phase;
    char label[256]; /* Arbitrary buffer large enough to hold the label */
    SECStatus rv;

#define FORMAT_LABEL(phase_, purpose_)                                              \
    do {                                                                            \
        PRUint32 n = PR_snprintf(label, sizeof(label), "%s, %s", phase_, purpose_); \
        /* Check for getting close. */                                              \
        if ((n + 1) >= sizeof(label)) {                                             \
            PORT_Assert(0);                                                         \
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);                               \
            goto loser;                                                             \
        }                                                                           \
    } while (0)
#define EXPAND_TRAFFIC_KEY(purpose_, target_)                                 \
    do {                                                                      \
        FORMAT_LABEL(phase, purpose_);                                        \
        rv = tls13_HkdfExpandLabel(prk, tls13_GetHash(ss),                    \
                                   hashes.u.raw, hashes.len,                  \
                                   label, strlen(label),                      \
                                   bulkAlgorithm, keySize, &pwSpec->target_); \
        if (rv != SECSuccess) {                                               \
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);                         \
            PORT_Assert(0);                                                   \
            goto loser;                                                       \
        }                                                                     \
    } while (0)

#define EXPAND_TRAFFIC_IV(purpose_, target_)                    \
    do {                                                        \
        FORMAT_LABEL(phase, purpose_);                          \
        rv = tls13_HkdfExpandLabelRaw(prk, tls13_GetHash(ss),   \
                                      hashes.u.raw, hashes.len, \
                                      label, strlen(label),     \
                                      pwSpec->target_, ivSize); \
        if (rv != SECSuccess) {                                 \
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);           \
            PORT_Assert(0);                                     \
            goto loser;                                         \
        }                                                       \
    } while (0)

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSpecWriteLock(ss));
    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    rv = ssl3_ComputeHandshakeHashes(ss, pwSpec, &hashes, 0);
    if (rv != SECSuccess) {
        PORT_Assert(0); /* Should never fail */
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }
    PRINT_BUF(60, (ss, "Deriving traffic keys. Session hash=", hashes.u.raw,
                   hashes.len));

    switch (type) {
        case TrafficKeyHandshake:
            phase = kHkdfPhaseHandshakeKeys;
            prk = ss->ssl3.hs.xES;
            break;
        case TrafficKeyApplicationData:
            phase = kHkdfPhaseApplicationDataKeys;
            prk = ss->ssl3.hs.trafficSecret;
            break;
        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
    }
    PORT_Assert(prk != NULL);

    SSL_TRC(3, ("%d: TLS13[%d]: deriving traffic keys phase='%s'",
                SSL_GETPID(), ss->fd, phase));

    EXPAND_TRAFFIC_KEY(kHkdfPurposeClientWriteKey, client.write_key);
    EXPAND_TRAFFIC_KEY(kHkdfPurposeServerWriteKey, server.write_key);
    EXPAND_TRAFFIC_IV(kHkdfPurposeClientWriteIv, client.write_iv);
    EXPAND_TRAFFIC_IV(kHkdfPurposeServerWriteIv, server.write_iv);

    return SECSuccess;

loser:
    return SECFailure;
}

/* Set up a cipher spec with keys. If install is nonzero, then also install
 * it as the current cipher spec for each value in the mask. */
SECStatus
tls13_InitCipherSpec(sslSocket *ss, TrafficKeyType type, InstallCipherSpecDirection install)
{
    ssl3CipherSpec *pwSpec;
    ssl3CipherSpec *cwSpec;
    SECStatus rv;
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (install == InstallCipherSpecWrite ||
        install == InstallCipherSpecBoth) {
        ssl_GetXmitBufLock(ss);

        rv = ssl3_FlushHandshake(ss, ssl_SEND_FLAG_FORCE_INTO_BUFFER);
        ssl_ReleaseXmitBufLock(ss);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    ssl_GetSpecWriteLock(ss); /**************************************/

    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    pwSpec = ss->ssl3.pwSpec;
    cwSpec = ss->ssl3.cwSpec;

    switch (pwSpec->cipher_def->calg) {
        case calg_aes_gcm:
            pwSpec->aead = tls13_AESGCM;
            break;
        default:
            PORT_Assert(0);
            goto loser;
            break;
    }

    /* Generic behaviors -- common to all crypto methods */
    if (!IS_DTLS(ss)) {
        pwSpec->read_seq_num.high = pwSpec->write_seq_num.high = 0;
    } else {
        if (cwSpec->epoch == PR_UINT16_MAX) {
            /* The problem here is that we have rehandshaked too many
             * times (you are not allowed to wrap the epoch). The
             * spec says you should be discarding the connection
             * and start over, so not much we can do here. */
            rv = SECFailure;
            goto loser;
        }
        /* The sequence number has the high 16 bits as the epoch. */
        pwSpec->epoch = cwSpec->epoch + 1;
        pwSpec->read_seq_num.high = pwSpec->write_seq_num.high =
            pwSpec->epoch << 16;

        dtls_InitRecvdRecords(&pwSpec->recvdRecords);
    }
    pwSpec->read_seq_num.low = pwSpec->write_seq_num.low = 0;

    rv = tls13_DeriveTrafficKeys(ss, pwSpec, type);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (install == InstallCipherSpecWrite ||
        install == InstallCipherSpecBoth) {
        rv = tls13_InstallCipherSpec(ss, InstallCipherSpecWrite);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    if (install == InstallCipherSpecRead ||
        install == InstallCipherSpecBoth) {
        rv = tls13_InstallCipherSpec(ss, InstallCipherSpecRead);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    ssl_ReleaseSpecWriteLock(ss); /**************************************/

    return SECSuccess;

loser:
    ssl_ReleaseSpecWriteLock(ss); /**************************************/
    PORT_SetError(SSL_ERROR_INIT_CIPHER_SUITE_FAILURE);
    return SECFailure;
}

static SECStatus
tls13_ComputeSecrets1(sslSocket *ss)
{
    SECStatus rv;
    PK11SymKey *mSS = NULL;
    PK11SymKey *mES = NULL;
    PK11SymKey *masterSecret = NULL;
    SSL3Hashes hashes;

    rv = ssl3_SetupPendingCipherSpec(ss);
    if (rv != SECSuccess) {
        return rv; /* error code set below. */
    }

    rv = ssl3_ComputeHandshakeHashes(ss, ss->ssl3.pwSpec, &hashes, 0);
    if (rv != SECSuccess) {
        PORT_Assert(0); /* Should never fail */
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }

    PORT_Assert(ss->ssl3.hs.xSS);
    PORT_Assert(ss->ssl3.hs.xES);

    rv = tls13_HkdfExpandLabel(ss->ssl3.hs.xSS,
                               tls13_GetHash(ss),
                               hashes.u.raw, hashes.len,
                               kHkdfLabelExpandedSs,
                               strlen(kHkdfLabelExpandedSs),
                               tls13_GetHkdfMechanism(ss),
                               hashes.len, &mSS);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(ss->ssl3.hs.xES,
                               tls13_GetHash(ss),
                               hashes.u.raw, hashes.len,
                               kHkdfLabelExpandedEs,
                               strlen(kHkdfLabelExpandedEs),
                               tls13_GetHkdfMechanism(ss),
                               hashes.len, &mES);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExtract(mSS, mES,
                           tls13_GetHash(ss),
                           &masterSecret);

    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(masterSecret,
                               tls13_GetHash(ss),
                               hashes.u.raw, hashes.len,
                               kHkdfLabelTrafficSecret,
                               strlen(kHkdfLabelTrafficSecret),
                               tls13_GetHkdfMechanism(ss),
                               hashes.len, &ss->ssl3.hs.trafficSecret);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(masterSecret,
                               tls13_GetHash(ss),
                               NULL, 0,
                               kHkdfLabelClientFinishedSecret,
                               strlen(kHkdfLabelClientFinishedSecret),
                               tls13_GetHmacMechanism(ss),
                               hashes.len, &ss->ssl3.hs.clientFinishedSecret);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(masterSecret,
                               tls13_GetHash(ss),
                               NULL, 0,
                               kHkdfLabelServerFinishedSecret,
                               strlen(kHkdfLabelServerFinishedSecret),
                               tls13_GetHmacMechanism(ss),
                               hashes.len, &ss->ssl3.hs.serverFinishedSecret);
    if (rv != SECSuccess) {
        goto loser;
    }

loser:
    PK11_FreeSymKey(ss->ssl3.hs.xSS);
    ss->ssl3.hs.xSS = NULL;
    PK11_FreeSymKey(ss->ssl3.hs.xES);
    ss->ssl3.hs.xES = NULL;

    if (mSS) {
        PK11_FreeSymKey(mSS);
    }
    if (mES) {
        PK11_FreeSymKey(mES);
    }
    if (masterSecret) {
        PK11_FreeSymKey(masterSecret);
    }

    return rv;
}

void
tls13_DestroyKeyShareEntry(TLS13KeyShareEntry *offer)
{
    SECITEM_ZfreeItem(&offer->key_exchange, PR_FALSE);
    PORT_ZFree(offer, sizeof(*offer));
}

void
tls13_DestroyKeyShares(PRCList *list)
{
    PRCList *cur_p;

    while (!PR_CLIST_IS_EMPTY(list)) {
        cur_p = PR_LIST_TAIL(list);
        PR_REMOVE_LINK(cur_p);
        tls13_DestroyKeyShareEntry((TLS13KeyShareEntry *)cur_p);
    }
}

/* Implement the SSLAEADCipher interface defined in sslimpl.h.
 *
 * That interface mixes the AD and the sequence number, but in
 * TLS 1.3 there is no additional data so this value is just the
 * encoded sequence number and we call it |seqNumBuf|.
 */
static SECStatus
tls13_AESGCM(ssl3KeyMaterial *keys,
             PRBool doDecrypt,
             unsigned char *out,
             int *outlen,
             int maxout,
             const unsigned char *in,
             int inlen,
             const unsigned char *seqNumBuf,
             int seqNumLen)
{
    SECItem param;
    SECStatus rv = SECFailure;
    unsigned char nonce[12];
    size_t i;
    unsigned int uOutLen;
    CK_GCM_PARAMS gcmParams;
    static const int tagSize = 16;

    PORT_Assert(seqNumLen == 8);

    /* draft-ietf-tls-tls13 Section 5.2.2 specifies the following
     * nonce algorithm:
     *
     * The length of the per-record nonce (iv_length) is set to max(8 bytes,
     * N_MIN) for the AEAD algorithm (see [RFC5116] Section 4).  An AEAD
     * algorithm where N_MAX is less than 8 bytes MUST NOT be used with TLS.
     * The per-record nonce for the AEAD construction is formed as follows:
     *
     * 1.  The 64-bit record sequence number is padded to the left with
     *     zeroes to iv_length.
     *
     * 2.  The padded sequence number is XORed with the static
     *     client_write_iv or server_write_iv, depending on the role.
     *
     * The resulting quantity (of length iv_length) is used as the per-
     * record nonce.
     *
     * Per RFC 5288: N_MIN = N_MAX = 12 bytes.
     *
     */
    memcpy(nonce, keys->write_iv, sizeof(nonce));
    for (i = 0; i < 8; ++i) {
        nonce[4 + i] ^= seqNumBuf[i];
    }

    param.type = siBuffer;
    param.data = (unsigned char *)&gcmParams;
    param.len = sizeof(gcmParams);
    gcmParams.pIv = nonce;
    gcmParams.ulIvLen = sizeof(nonce);
    gcmParams.pAAD = NULL;
    gcmParams.ulAADLen = 0;
    gcmParams.ulTagBits = tagSize * 8;

    if (doDecrypt) {
        rv = PK11_Decrypt(keys->write_key, CKM_AES_GCM, &param, out, &uOutLen,
                          maxout, in, inlen);
    } else {
        rv = PK11_Encrypt(keys->write_key, CKM_AES_GCM, &param, out, &uOutLen,
                          maxout, in, inlen);
    }
    *outlen = (int)uOutLen;

    return rv;
}

static SECStatus
tls13_HandleEncryptedExtensions(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;
    PRInt32 innerLength;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    SSL_TRC(3, ("%d: TLS13[%d]: handle encrypted extensions",
                SSL_GETPID(), ss->fd));

    rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_ENCRYPTED_EXTENSIONS,
                              wait_encrypted_extensions);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    innerLength = ssl3_ConsumeHandshakeNumber(ss, 2, &b, &length);
    if (innerLength < 0) {
        return SECFailure; /* Alert already sent. */
    }
    if (innerLength != length) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ENCRYPTED_EXTENSIONS,
                    illegal_parameter);
        return SECFailure;
    }

    rv = ssl3_HandleHelloExtensions(ss, &b, &length, encrypted_extensions);
    if (rv != SECSuccess) {
        return SECFailure; /* Error code set below */
    }

    TLS13_SET_HS_STATE(ss, wait_cert_request);
    return SECSuccess;
}

static SECStatus
tls13_SendEncryptedExtensions(sslSocket *ss)
{
    SECStatus rv;
    PRInt32 extensions_len = 0;
    PRInt32 sent_len = 0;
    PRUint32 maxBytes = 65535;

    SSL_TRC(3, ("%d: TLS13[%d]: send encrypted extensions handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    extensions_len = ssl3_CallHelloExtensionSenders(
        ss, PR_FALSE, maxBytes, &ss->xtnData.encryptedExtensionsSenders[0]);

    rv = ssl3_AppendHandshakeHeader(ss, encrypted_extensions,
                                    extensions_len + 2);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }
    rv = ssl3_AppendHandshakeNumber(ss, extensions_len, 2);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }
    sent_len = ssl3_CallHelloExtensionSenders(
        ss, PR_TRUE, extensions_len,
        &ss->xtnData.encryptedExtensionsSenders[0]);
    PORT_Assert(sent_len == extensions_len);
    if (sent_len != extensions_len) {
        PORT_Assert(sent_len == 0);
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    return SECSuccess;
}

/* Called from tls13_CompleteHandleHandshakeMessage() when it has deciphered a complete
 * tls13 CertificateVerify message
 * Caller must hold Handshake and RecvBuf locks.
 */
SECStatus
tls13_HandleCertificateVerify(sslSocket *ss, SSL3Opaque *b, PRUint32 length,
                              SSL3Hashes *hashes)
{
    SECItem signed_hash = { siBuffer, NULL, 0 };
    SECStatus rv;
    SSLSignatureAndHashAlg sigAndHash;

    SSL_TRC(3, ("%d: TLS13[%d]: handle certificate_verify handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY,
                              wait_cert_verify);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (!hashes) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    /* We only support CertificateVerify messages that use the handshake
     * hash.
     * TODO(ekr@rtfm.com): This should be easy to relax in TLS 1.3 by
     * reading the client's hash algorithm first, but there may
     * be subtleties so retain the restriction for now.
     */
    rv = tls13_AddContextToHashes(ss, hashes, hashes->hashAlg, PR_FALSE);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_DIGEST_FAILURE, internal_error);
        return SECFailure;
    }

    rv = ssl3_ConsumeSignatureAndHashAlgorithm(ss, &b, &length,
                                               &sigAndHash);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CERT_VERIFY);
        return SECFailure;
    }

    rv = ssl3_CheckSignatureAndHashAlgorithmConsistency(
        ss, &sigAndHash, ss->sec.peerCert);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CERT_VERIFY, decrypt_error);
        return SECFailure;
    }

    /* We only support CertificateVerify messages that use the handshake
     * hash. */
    if (sigAndHash.hashAlg != hashes->hashAlg) {
        FATAL_ERROR(ss, SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM, decrypt_error);
        return SECFailure;
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &signed_hash, 2, &b, &length);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CERT_VERIFY);
        return SECFailure;
    }

    if (length != 0) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CERT_VERIFY, decode_error);
        return SECFailure;
    }

    rv = ssl3_VerifySignedHashes(hashes, ss->sec.peerCert, &signed_hash,
                                 PR_TRUE, ss->pkcs11PinArg);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, PORT_GetError(), decrypt_error);
        return SECFailure;
    }

    if (!ss->sec.isServer) {
        /* Compute the rest of the secrets except for the resumption
         * and exporter secret. */
        rv = tls13_ComputeSecrets1(ss);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            return SECFailure;
        }
    }
    TLS13_SET_HS_STATE(ss, wait_finished);

    return SECSuccess;
}

static SECStatus
tls13_ComputeFinished(sslSocket *ss, const SSL3Hashes *hashes, PRBool sending,
                      PRUint8 *output, unsigned int *outputLen, unsigned int maxOutputLen)
{
    SECStatus rv;
    PK11Context *hmacCtx = NULL;
    CK_MECHANISM_TYPE macAlg = tls13_GetHmacMechanism(ss);
    SECItem param = { siBuffer, NULL, 0 };
    unsigned int outputLenUint;
    PK11SymKey *secret = (ss->sec.isServer ^ sending) ? ss->ssl3.hs.clientFinishedSecret
                                                      : ss->ssl3.hs.serverFinishedSecret;

    PORT_Assert(secret);
    PRINT_BUF(90, (NULL, "Handshake hash", hashes->u.raw, hashes->len));

    hmacCtx = PK11_CreateContextBySymKey(macAlg, CKA_SIGN,
                                         secret, &param);
    if (!hmacCtx) {
        goto abort;
    }

    rv = PK11_DigestBegin(hmacCtx);
    if (rv != SECSuccess)
        goto abort;

    rv = PK11_DigestOp(hmacCtx, hashes->u.raw, hashes->len);
    if (rv != SECSuccess)
        goto abort;

    PORT_Assert(maxOutputLen >= hashes->len);
    rv = PK11_DigestFinal(hmacCtx, output, &outputLenUint, maxOutputLen);
    if (rv != SECSuccess)
        goto abort;
    *outputLen = outputLenUint;

    PK11_DestroyContext(hmacCtx, PR_TRUE);
    return SECSuccess;

abort:
    if (hmacCtx) {
        PK11_DestroyContext(hmacCtx, PR_TRUE);
    }

    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}

static SECStatus
tls13_SendFinished(sslSocket *ss)
{
    SECStatus rv;
    PRUint8 finishedBuf[MAX_FINISHED_SIZE];
    unsigned int finishedLen;
    SSL3Hashes hashes;
    int errCode;

    SSL_TRC(3, ("%d: TLS13[%d]: send finished handshake", SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    rv = ssl3_ComputeHandshakeHashes(ss, ss->ssl3.cwSpec, &hashes, 0);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    ssl_GetSpecReadLock(ss);
    rv = tls13_ComputeFinished(ss, &hashes, PR_TRUE,
                               finishedBuf, &finishedLen, sizeof(finishedBuf));
    ssl_ReleaseSpecReadLock(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    rv = ssl3_AppendHandshakeHeader(ss, finished, finishedLen);
    if (rv != SECSuccess) {
        errCode = PR_GetError();
        goto alert_loser;
    }

    rv = ssl3_AppendHandshake(ss, finishedBuf, finishedLen);
    if (rv != SECSuccess) {
        errCode = PR_GetError();
        goto alert_loser;
    }

    rv = ssl3_FlushHandshake(ss, 0);
    if (rv != SECSuccess) {
        errCode = PR_GetError();
        goto alert_loser;
    }

    if (ss->sec.isServer) {
        rv = tls13_InitCipherSpec(ss, TrafficKeyApplicationData,
                                  InstallCipherSpecWrite);
    } else {
        rv = tls13_InstallCipherSpec(ss, InstallCipherSpecWrite);
    }
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* TODO(ekr@rtfm.com): Record key log */
    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, internal_error);
    PORT_SetError(errCode); /* Restore error code */
    return rv;
}

static SECStatus
tls13_HandleFinished(sslSocket *ss, SSL3Opaque *b, PRUint32 length,
                     const SSL3Hashes *hashes)
{
    SECStatus rv;
    PRUint8 finishedBuf[MAX_FINISHED_SIZE];
    unsigned int finishedLen;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    SSL_TRC(3, ("%d: TLS13[%d]: handle finished handshake",
                SSL_GETPID(), ss->fd));

    rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_FINISHED, wait_finished);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    if (!hashes) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    ssl_GetSpecReadLock(ss);
    rv = tls13_ComputeFinished(ss, hashes, PR_FALSE,
                               finishedBuf, &finishedLen, sizeof(finishedBuf));
    ssl_ReleaseSpecReadLock(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    if (length != finishedLen) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_FINISHED, decode_error);
        return SECFailure;
    }

    if (NSS_SecureMemcmp(b, finishedBuf, finishedLen) != 0) {
        FATAL_ERROR(ss, SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE,
                    decrypt_error);
        return SECFailure;
    }

    /* Server is now finished.
     * Client sends second flight
     */
    /* TODO(ekr@rtfm.com): Send NewSession Ticket if server. */
    if (ss->sec.isServer) {
        rv = tls13_InstallCipherSpec(ss, InstallCipherSpecRead);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            return SECFailure;
        }

        rv = tls13_FinishHandshake(ss);
    } else {
        if (ss->ssl3.hs.authCertificatePending) {
            /* TODO(ekr@rtfm.com): Handle pending auth */
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            PORT_Assert(0);
            return SECFailure;
        }
        rv = tls13_InitCipherSpec(ss, TrafficKeyApplicationData,
                                  InstallCipherSpecRead);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            return SECFailure;
        }

        rv = tls13_SendClientSecondRound(ss);
        if (rv != SECSuccess)
            return SECFailure; /* Error code and alerts handled below */
    }

    return rv;
}

static SECStatus
tls13_FinishHandshake(sslSocket *ss)
{
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->ssl3.hs.restartTarget == NULL);

    /* The first handshake is now completed. */
    ss->handshake = NULL;

    TLS13_SET_HS_STATE(ss, idle_handshake);

    ssl_FinishHandshake(ss);

    return SECSuccess;
}

static SECStatus
tls13_SendClientSecondRound(sslSocket *ss)
{
    SECStatus rv;
    PRBool sendClientCert;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    sendClientCert = !ss->ssl3.sendEmptyCert &&
                     ss->ssl3.clientCertChain != NULL &&
                     ss->ssl3.clientPrivateKey != NULL;

    /* Defer client authentication sending if we are still
     * waiting for server authentication. See the long block
     * comment in ssl3_SendClientSecondRound for more detail.
     */
    if (ss->ssl3.hs.restartTarget) {
        PR_NOT_REACHED("unexpected ss->ssl3.hs.restartTarget");
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (ss->ssl3.hs.authCertificatePending && (sendClientCert ||
                                               ss->ssl3.sendEmptyCert)) {
        SSL_TRC(3, ("%d: TLS13[%p]: deferring ssl3_SendClientSecondRound because"
                    " certificate authentication is still pending.",
                    SSL_GETPID(), ss->fd));
        ss->ssl3.hs.restartTarget = tls13_SendClientSecondRound;
        return SECWouldBlock;
    }

    ssl_GetXmitBufLock(ss); /*******************************/
    if (ss->ssl3.sendEmptyCert) {
        ss->ssl3.sendEmptyCert = PR_FALSE;
        rv = ssl3_SendEmptyCertificate(ss);
        /* Don't send verify */
        if (rv != SECSuccess) {
            goto loser; /* error code is set. */
        }
    } else if (sendClientCert) {
        rv = ssl3_SendCertificate(ss);
        if (rv != SECSuccess) {
            goto loser; /* error code is set. */
        }
    }

    if (sendClientCert) {
        rv = ssl3_SendCertificateVerify(ss, ss->ssl3.clientPrivateKey);
        SECKEY_DestroyPrivateKey(ss->ssl3.clientPrivateKey);
        ss->ssl3.clientPrivateKey = NULL;
        if (rv != SECSuccess) {
            goto loser; /* err is set. */
        }
    }

    rv = tls13_SendFinished(ss);
    if (rv != SECSuccess) {
        goto loser; /* err code was set. */
    }
    ssl_ReleaseXmitBufLock(ss); /*******************************/

    /* The handshake is now finished */
    return tls13_FinishHandshake(ss);

loser:
    ssl_ReleaseXmitBufLock(ss); /*******************************/
    return SECFailure;
}

static SECStatus
tls13_HandleNewSessionTicket(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;

    rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET,
                              idle_handshake);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    UNIMPLEMENTED();

    /* Ignore */
    return SECSuccess;
}

typedef enum {
    ExtensionNotUsed,
    ExtensionClientOnly,
    ExtensionSendClear,
    ExtensionSendEncrypted,
} Tls13ExtensionStatus;

static const struct {
    SSLExtensionType ex_value;
    Tls13ExtensionStatus status;
} KnownExtensions[] = {
    { ssl_server_name_xtn,
      ExtensionSendEncrypted },
    {
        ssl_cert_status_xtn,
        ExtensionNotUsed /* TODO(ekr@rtfm.com): Disabled because broken
                            in TLS 1.3. */
        /* ExtensionSendEncrypted */
    },
    { ssl_elliptic_curves_xtn,
      ExtensionSendClear },
    { ssl_ec_point_formats_xtn,
      ExtensionNotUsed },
    { ssl_signature_algorithms_xtn,
      ExtensionClientOnly },
    { ssl_use_srtp_xtn,
      ExtensionSendEncrypted },
    { ssl_app_layer_protocol_xtn,
      ExtensionSendEncrypted },
    { ssl_padding_xtn,
      ExtensionNotUsed },
    { ssl_extended_master_secret_xtn,
      ExtensionNotUsed },
    { ssl_session_ticket_xtn,
      ExtensionClientOnly },
    { ssl_tls13_key_share_xtn,
      ExtensionSendClear },
    { ssl_next_proto_nego_xtn,
      ExtensionNotUsed },
    { ssl_renegotiation_info_xtn,
      ExtensionNotUsed },
    { ssl_tls13_draft_version_xtn,
      ExtensionClientOnly }
};

PRBool
tls13_ExtensionAllowed(PRUint16 extension, SSL3HandshakeType message)
{
    unsigned int i;

    PORT_Assert((message == client_hello) ||
                (message == server_hello) ||
                (message == encrypted_extensions));

    for (i = 0; i < PR_ARRAY_SIZE(KnownExtensions); i++) {
        if (KnownExtensions[i].ex_value == extension)
            break;
    }
    if (i == PR_ARRAY_SIZE(KnownExtensions)) {
        /* We have never heard of this extension which is OK on
         * the server but not the client. */
        return message == client_hello;
    }

    switch (KnownExtensions[i].status) {
        case ExtensionNotUsed:
            return PR_FALSE;
        case ExtensionClientOnly:
            return message == client_hello;
        case ExtensionSendClear:
            return message == client_hello ||
                   message == server_hello;
        case ExtensionSendEncrypted:
            return message == client_hello ||
                   message == encrypted_extensions;
    }

    PORT_Assert(0);

    /* Not reached */
    return PR_TRUE;
}

/* Helper function to encode a uint32 into a buffer */
unsigned char *
tls13_EncodeUintX(PRUint32 value, unsigned int bytes, unsigned char *to)
{
    PRUint32 encoded;

    PORT_Assert(bytes > 0 && bytes <= 4);

    encoded = PR_htonl(value);
    memcpy(to, ((unsigned char *)(&encoded)) + (4 - bytes), bytes);
    return to + bytes;
}

/* TLS 1.3 doesn't actually have additional data but the aead function
 * signature overloads additional data to carry the record sequence
 * number and that's what we put here. The TLS 1.3 AEAD functions
 * just use this input as the sequence number and not as additional
 * data. */
static void
tls13_FormatAdditionalData(unsigned char *aad, unsigned int length,
                           SSL3SequenceNumber seqNum)
{
    unsigned char *ptr = aad;

    PORT_Assert(length == 8);
    ptr = tls13_EncodeUintX(seqNum.high, 4, ptr);
    ptr = tls13_EncodeUintX(seqNum.low, 4, ptr);
    PORT_Assert((ptr - aad) == length);
}

SECStatus
tls13_ProtectRecord(sslSocket *ss,
                    SSL3ContentType type,
                    const SSL3Opaque *pIn,
                    PRUint32 contentLen,
                    sslBuffer *wrBuf)
{
    ssl3CipherSpec *cwSpec = ss->ssl3.cwSpec;
    const ssl3BulkCipherDef *cipher_def = cwSpec->cipher_def;
    SECStatus rv;
    PRUint16 headerLen;
    int cipherBytes = 0;
    const int tagLen = cipher_def->tag_size;

    SSL_TRC(3, ("%d: TLS13[%d]: protect record of length %u, seq=0x%0x%0x",
                SSL_GETPID(), ss->fd, contentLen,
                cwSpec->write_seq_num.high,
                cwSpec->write_seq_num.low));

    headerLen = IS_DTLS(ss) ? DTLS_RECORD_HEADER_LENGTH : SSL3_RECORD_HEADER_LENGTH;

    if (headerLen + contentLen + 1 + tagLen > wrBuf->space) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* Copy the data into the wrBuf. We're going to encrypt in-place
     * in the AEAD branch anyway */
    PORT_Memcpy(wrBuf->buf + headerLen, pIn, contentLen);

    if (cipher_def->calg == ssl_calg_null) {
        /* Shortcut for plaintext */
        cipherBytes = contentLen;
    } else {
        unsigned char aad[8];
        PORT_Assert(cipher_def->type == type_aead);

        /* Add the content type at the end. */
        wrBuf->buf[headerLen + contentLen] = type;

        /* Stomp the content type to be application_data */
        type = content_application_data;

        tls13_FormatAdditionalData(aad, sizeof(aad),
                                   cwSpec->write_seq_num);
        cipherBytes = contentLen + 1; /* Room for the content type on the end. */
        rv = cwSpec->aead(
            ss->sec.isServer ? &cwSpec->server : &cwSpec->client,
            PR_FALSE,                               /* do encrypt */
            wrBuf->buf + headerLen,                 /* output  */
            &cipherBytes,                           /* out len */
            wrBuf->space - headerLen,               /* max out */
            wrBuf->buf + headerLen, contentLen + 1, /* input   */
            aad, sizeof(aad));
        if (rv != SECSuccess) {
            PORT_SetError(SSL_ERROR_ENCRYPTION_FAILURE);
            return SECFailure;
        }
    }

    PORT_Assert(cipherBytes <= MAX_FRAGMENT_LENGTH + 256);

    wrBuf->len = cipherBytes + headerLen;
    wrBuf->buf[0] = type;

    if (IS_DTLS(ss)) {
        (void)tls13_EncodeUintX(2, dtls_TLSVersionToDTLSVersion(kRecordVersion),
                                &wrBuf->buf[1]);
        (void)tls13_EncodeUintX(cwSpec->write_seq_num.high, 4, &wrBuf->buf[3]);
        (void)tls13_EncodeUintX(cwSpec->write_seq_num.low, 4, &wrBuf->buf[7]);
        (void)tls13_EncodeUintX(cipherBytes, 2, &wrBuf->buf[11]);
    } else {
        (void)tls13_EncodeUintX(kRecordVersion, 2, &wrBuf->buf[1]);
        (void)tls13_EncodeUintX(cipherBytes, 2, &wrBuf->buf[3]);
    }
    ssl3_BumpSequenceNumber(&cwSpec->write_seq_num);

    return SECSuccess;
}

/* Unprotect a TLS 1.3 record and leave the result in plaintext.
 *
 * Called by ssl3_HandleRecord. Caller must hold the spec read lock.
 * Therefore, we MUST not call SSL3_SendAlert().
 *
 * If SECFailure is returned, we:
 * 1. Set |*alert| to the alert to be sent.
 * 2. Call PORT_SetError() witn an appropriate code.
 */
SECStatus
tls13_UnprotectRecord(sslSocket *ss, SSL3Ciphertext *cText, sslBuffer *plaintext,
                      SSL3AlertDescription *alert)
{
    ssl3CipherSpec *crSpec = ss->ssl3.crSpec;
    const ssl3BulkCipherDef *cipher_def = crSpec->cipher_def;
    unsigned char aad[8];
    SECStatus rv;

    *alert = bad_record_mac; /* Default alert for most issues. */

    SSL_TRC(3, ("%d: TLS13[%d]: unprotect record of length %u",
                SSL_GETPID(), ss->fd, cText->buf->len));

    /* We can perform this test in variable time because the record's total
     * length and the ciphersuite are both public knowledge. */
    if (cText->buf->len < cipher_def->tag_size) {
        PORT_SetError(SSL_ERROR_RX_RECORD_TOO_LONG);
        return SECFailure;
    }

    /* Verify that the content type is right, even though we overwrite it. */
    if (cText->type != content_application_data) {
        /* Do we need a better error here? */
        PORT_SetError(SSL_ERROR_BAD_MAC_READ);
        return SECFailure;
    }

    /* Check the version number in the record */
    if (cText->version != kRecordVersion) {
        /* Do we need a better error here? */
        PORT_SetError(SSL_ERROR_BAD_MAC_READ);
        return SECFailure;
    }

    /* Decrypt */
    PORT_Assert(cipher_def->type == type_aead);
    tls13_FormatAdditionalData(aad, sizeof(aad),
                               IS_DTLS(ss) ? cText->seq_num
                                           : crSpec->read_seq_num);
    rv = crSpec->aead(
        ss->sec.isServer ? &crSpec->client : &crSpec->server,
        PR_TRUE,                /* do decrypt */
        plaintext->buf,         /* out */
        (int *)&plaintext->len, /* outlen */
        plaintext->space,       /* maxout */
        cText->buf->buf,        /* in */
        cText->buf->len,        /* inlen */
        aad, sizeof(aad));
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_BAD_MAC_READ);
        return SECFailure;
    }

    /* The record is right-padded with 0s, followed by the true
     * content type, so read from the right until we receive a
     * nonzero byte. */
    while (plaintext->len > 0 && !(plaintext->buf[plaintext->len - 1])) {
        --plaintext->len;
    }

    /* Bogus padding. */
    if (plaintext->len < 1) {
        /* It's safe to report this specifically because it happened
         * after the MAC has been verified. */
        PORT_SetError(SSL_ERROR_BAD_BLOCK_PADDING);
        return SECFailure;
    }

    /* Record the type. */
    cText->type = plaintext->buf[plaintext->len - 1];
    --plaintext->len;

    return SECSuccess;
}
