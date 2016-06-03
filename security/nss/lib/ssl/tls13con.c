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
#include "secmod.h"
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
static SECStatus tls13_SetupNullCipherSpec(sslSocket *ss);
static SECStatus tls13_SetupPendingCipherSpec(sslSocket *ss);
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
static SECStatus tls13_ChaCha20Poly1305(
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
static SECStatus
tls13_ComputeHandshakeHashes(sslSocket *ss, SSL3Hashes *hashes);
static SECStatus tls13_ComputeSecrets1(sslSocket *ss);
static SECStatus tls13_ComputeSecrets2(sslSocket *ss);
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

const SSL3ProtocolVersion kTlsRecordVersion = SSL_LIBRARY_VERSION_TLS_1_0;
const SSL3ProtocolVersion kDtlsRecordVersion = SSL_LIBRARY_VERSION_TLS_1_1;

/* Belt and suspenders in case we ever add a TLS 1.4. */
PR_STATIC_ASSERT(SSL_LIBRARY_VERSION_MAX_SUPPORTED <=
                 SSL_LIBRARY_VERSION_TLS_1_3);

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
    /* All TLS 1.3 cipher suites must have an explict PRF hash. */
    PORT_Assert(ss->ssl3.hs.suite_def->prf_hash != ssl_hash_none);
    return ss->ssl3.hs.suite_def->prf_hash;
}

unsigned int
tls13_GetHashSize(sslSocket *ss)
{
    switch (tls13_GetHash(ss)) {
        case ssl_hash_sha256:
            return 32;
        case ssl_hash_sha384:
            return 48;
        default:
            PORT_Assert(0);
    }
    return 32;
}

CK_MECHANISM_TYPE
tls13_GetHkdfMechanism(sslSocket *ss)
{
    switch (tls13_GetHash(ss)) {
        case ssl_hash_sha256:
            return CKM_NSS_HKDF_SHA256;
        case ssl_hash_sha384:
            return CKM_NSS_HKDF_SHA384;
        default:
            /*PORT_Assert(0);*/
            return CKM_NSS_HKDF_SHA256;
    }
    return CKM_NSS_HKDF_SHA256;
}

static CK_MECHANISM_TYPE
tls13_GetHmacMechanism(sslSocket *ss)
{
    switch (tls13_GetHash(ss)) {
        case ssl_hash_sha256:
            return CKM_SHA256_HMAC;
        case ssl_hash_sha384:
            return CKM_SHA384_HMAC;
        default:
            PORT_Assert(0);
    }
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

static SECStatus
tls13_RecoverWrappedSharedSecret(sslSocket *ss, sslSessionID *sid)
{
    PK11SymKey *wrapKey; /* wrapping key */
    PK11SymKey *SS = NULL;
    SECItem wrappedMS = { siBuffer, NULL, 0 };
    SSLHashType hashType;
    SECStatus rv;

    SSL_TRC(3, ("%d: TLS13[%d]: recovering static secret (%s)",
                SSL_GETPID(), ss->fd,
                ss->sec.isServer ? "server" : "client"));
    if (!sid->u.ssl3.keys.msIsWrapped) {
        PORT_Assert(0); /* I think this can't happen. */
        return SECFailure;
    }

    hashType = tls13_GetHash(ss);
    if (hashType != ssl_hash_sha256 && hashType != ssl_hash_sha384) {
        PORT_Assert(0);
        return SECFailure;
    }

    /* If we are the server, we compute the wrapping key, but if we
     * are the client, it's coordinates are stored with the ticket. */
    if (ss->sec.isServer) {
        const sslServerCert *serverCert;

        serverCert = ssl_FindServerCert(ss, &sid->certType);
        PORT_Assert(serverCert);
        wrapKey = ssl3_GetWrappingKey(ss, NULL, serverCert,
                                      sid->u.ssl3.masterWrapMech,
                                      ss->pkcs11PinArg);
    } else {
        PK11SlotInfo *slot = SECMOD_LookupSlot(sid->u.ssl3.masterModuleID,
                                               sid->u.ssl3.masterSlotID);
        if (!slot)
            return SECFailure;

        wrapKey = PK11_GetWrapKey(slot,
                                  sid->u.ssl3.masterWrapIndex,
                                  sid->u.ssl3.masterWrapMech,
                                  sid->u.ssl3.masterWrapSeries,
                                  ss->pkcs11PinArg);
        PK11_FreeSlot(slot);
    }
    if (!wrapKey) {
        return SECFailure;
    }

    wrappedMS.data = sid->u.ssl3.keys.wrapped_master_secret;
    wrappedMS.len = sid->u.ssl3.keys.wrapped_master_secret_len;

    /* unwrap the "master secret" which becomes SS. */
    SS = PK11_UnwrapSymKeyWithFlags(wrapKey, sid->u.ssl3.masterWrapMech,
                                    NULL, &wrappedMS,
                                    CKM_SSL3_MASTER_KEY_DERIVE,
                                    CKA_DERIVE, tls13_GetHashSize(ss),
                                    CKF_SIGN | CKF_VERIFY);
    PK11_FreeSymKey(wrapKey);
    if (!SS) {
        return SECFailure;
    }
    PRINT_KEY(50, (ss, "Recovered static secret", SS));
    rv = tls13_HkdfExtractSharedKey(ss, SS, StaticSharedSecret);
    PK11_FreeSymKey(SS);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

static void
tls13_RestoreCipherInfo(sslSocket *ss, sslSessionID *sid)
{
    /* Set these to match the cached value.
     * TODO(ekr@rtfm.com): Make a version with the "true" values.
     * Bug 1256137.
     */
    ss->sec.authType = sid->authType;
    ss->sec.authKeyBits = sid->authKeyBits;
    ss->sec.keaType = sid->keaType;
    ss->sec.keaKeyBits = sid->keaKeyBits;
    ss->ssl3.hs.origCipherSuite = sid->u.ssl3.cipherSuite;
}

PRBool
tls13_AllowPskCipher(const sslSocket *ss, const ssl3CipherSuiteDef *cipher_def)
{
    if (ss->sec.isServer) {
        if (!ss->statelessResume)
            return PR_FALSE;
    } else {
        sslSessionID *sid = ss->sec.ci.sid;
        const ssl3CipherSuiteDef *cached_cipher_def;

        /* Verify that this was cached. */
        PORT_Assert(sid);
        if (sid->cached == never_cached)
            return PR_FALSE;

        /* Don't offer this if the session version < TLS 1.3 */
        if (sid->version < SSL_LIBRARY_VERSION_TLS_1_3)
            return PR_FALSE;
        cached_cipher_def = ssl_LookupCipherSuiteDef(
            sid->u.ssl3.cipherSuite);
        PORT_Assert(cached_cipher_def);

        /* Only offer a PSK cipher with the same symmetric parameters
         * as we negotiated before. */
        if (cached_cipher_def->bulk_cipher_alg !=
            cipher_def->bulk_cipher_alg)
            return PR_FALSE;

        /* PSK cipher must have the same PSK hash as was negotiated before. */
        if (cipher_def->prf_hash != cached_cipher_def->prf_hash) {
            return PR_FALSE;
        }
    }
    SSL_TRC(3, ("%d: TLS 1.3[%d]: Enabling cipher suite suite 0x%04x",
                SSL_GETPID(), ss->fd,
                cipher_def->cipher_suite));

    return PR_TRUE;
}

/* Check whether resumption-PSK is allowed. */
static PRBool
tls13_CanResume(sslSocket *ss, const sslSessionID *sid)
{
    const sslServerCert *sc;

    if (sid->version != ss->version) {
        return PR_FALSE;
    }

    /* Server sids don't remember the server cert we previously sent, but they
     * do remember the type of certificate we originally used, so we can locate
     * it again, provided that the current ssl socket has had its server certs
     * configured the same as the previous one. */
    sc = ssl_FindServerCert(ss, &sid->certType);
    if (!sc || !sc->serverCert) {
        return PR_FALSE;
    }

    return PR_TRUE;
}

/* Called from ssl3_HandleClientHello after we have parsed the
 * ClientHello and are sure that we are going to do TLS 1.3
 * or fail. */
SECStatus
tls13_HandleClientHelloPart2(sslSocket *ss,
                             const SECItem *suites,
                             sslSessionID *sid)
{
    SECStatus rv;
    SSL3Statistics *ssl3stats = SSL_GetStatistics();
    int j;

    rv = tls13_SetupNullCipherSpec(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }
    if (sid != NULL && !tls13_CanResume(ss, sid)) {
        /* Destroy SID if it is present an unusable. */
        SSL_AtomicIncrementLong(&ssl3stats->hch_sid_cache_not_ok);
        if (ss->sec.uncache)
            ss->sec.uncache(sid);
        ssl_FreeSID(sid);
        sid = NULL;
        ss->statelessResume = PR_FALSE;
    }

#ifndef PARANOID
    /* Look for a matching cipher suite. */
    j = ssl3_config_match_init(ss);
    if (j <= 0) { /* no ciphers are working/supported by PK11 */
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        goto loser;
    }
#endif

    rv = ssl3_NegotiateCipherSuite(ss, suites);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_NO_CYPHER_OVERLAP, handshake_failure);
        goto loser;
    }

    if (ss->ssl3.hs.kea_def->authKeyType != ssl_auth_psk) {
        /* TODO(ekr@rtfm.com): Free resumeSID. */
        ss->statelessResume = PR_FALSE;
    }

    if (ss->statelessResume) {
        PORT_Assert(sid);

        rv = tls13_RecoverWrappedSharedSecret(ss, sid);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            goto loser;
        }

        SSL_AtomicIncrementLong(&ssl3stats->hch_sid_cache_hits);
        SSL_AtomicIncrementLong(&ssl3stats->hch_sid_stateless_resumes);

        tls13_RestoreCipherInfo(ss, sid);

        ss->sec.serverCert = ssl_FindServerCert(ss, &sid->certType);
        PORT_Assert(ss->sec.serverCert);
        ss->sec.localCert = CERT_DupCertificate(ss->sec.serverCert->serverCert);
        if (sid->peerCert != NULL) {
            ss->sec.peerCert = CERT_DupCertificate(sid->peerCert);
        }
        ssl3_RegisterServerHelloExtensionSender(
            ss, ssl_tls13_pre_shared_key_xtn, tls13_ServerSendPreSharedKeyXtn);
        ss->sec.ci.sid = sid;
    } else {
        if (sid) { /* we had a sid, but it's no longer valid, free it */
            SSL_AtomicIncrementLong(&ssl3stats->hch_sid_cache_not_ok);
            if (ss->sec.uncache)
                ss->sec.uncache(sid);
            ssl_FreeSID(sid);
            sid = NULL;
        }
        ss->ssl3.hs.origCipherSuite = ss->ssl3.hs.cipher_suite;
        SSL_AtomicIncrementLong(&ssl3stats->hch_sid_cache_misses);
    }

    /* This call needs to happen before the SNI callback because
     * it sets up |pwSpec| to point to the right place. */
    rv = tls13_SetupPendingCipherSpec(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        goto loser;
    }

    rv = ssl3_ServerCallSNICallback(ss);
    if (rv != SECSuccess) {
        goto loser; /* An alert has already been sent. */
    }

    if (sid) {
        /* Check that the negotiated SID and the cached SID match. */
        if (SECITEM_CompareItem(&sid->u.ssl3.srvName,
                                &ss->ssl3.pwSpec->srvVirtName) !=
            SECEqual) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO,
                        handshake_failure);
            goto loser;
        }
    }

    if (!ss->statelessResume) {
        rv = ssl3_SelectServerCert(ss);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    /* If this is TLS 1.3 we are expecting a ClientKeyShare
     * extension. Missing/absent extension cause failure
     * below. */
    rv = tls13_HandleClientKeyShare(ss);
    if (rv != SECSuccess) {
        goto loser; /* An alert was sent already. */
    }

    if (!sid) {
        sid = ssl3_NewSessionID(ss, PR_TRUE);
        if (sid == NULL) {
            FATAL_ERROR(ss, PORT_GetError(), internal_error);
            goto loser;
        }
        ss->sec.ci.sid = sid;
    }

    ssl_GetXmitBufLock(ss);
    rv = tls13_SendServerHelloSequence(ss);
    ssl_ReleaseXmitBufLock(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, PORT_GetError(), handshake_failure);
        goto loser;
    }

    return SECSuccess;

loser:
    return SECFailure;
}

/* Called from tls13_HandleClientHello.
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

    /* Figure out what group we expect */
    switch (ss->ssl3.hs.kea_def->exchKeyType) {
        case ssl_kea_ecdh:
        case ssl_kea_ecdh_psk:
            expectedGroup = ssl3_GetCurveNameForServerSocket(ss);
            if (!expectedGroup) {
                FATAL_ERROR(ss, SSL_ERROR_NO_CYPHER_OVERLAP,
                            handshake_failure);
                return SECFailure;
            }
            break;
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

    PORT_Assert(!!ss->ssl3.hs.xSS ==
                (ss->ssl3.hs.kea_def->authKeyType == ssl_auth_psk));
    if (!ss->ssl3.hs.xSS) {
        ss->ssl3.hs.xSS = PK11_ReferenceSymKey(ss->ssl3.hs.xES);
        if (!ss->ssl3.hs.xSS) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            return SECFailure;
        }
    }

    /* Here we destroy the old cipher spec immediately; in DTLS, we have to
     * avoid running the holddown timer at this point. Retransmission of old
     * packets will use the static nullCipherSpec spec. */
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
    SECKEYPrivateKey *svrPrivKey;

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
    if (ss->ssl3.hs.kea_def->authKeyType != ssl_auth_psk) {
        rv = ssl3_SendCertificate(ss);
        if (rv != SECSuccess) {
            return SECFailure; /* error code is set. */
        }
        rv = ssl3_SendCertificateStatus(ss);
        if (rv != SECSuccess) {
            return SECFailure; /* error code is set. */
        }

        svrPrivKey = ss->sec.serverCert->serverKeyPair->privKey;
        rv = ssl3_SendCertificateVerify(ss, svrPrivKey);
        if (rv != SECSuccess) {
            return rv; /* err code is set. */
        }
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

SECStatus
tls13_HandleServerHelloPart2(sslSocket *ss)
{
    SECStatus rv;
    PRBool isPSK = ssl3_ExtensionNegotiated(ss, ssl_tls13_pre_shared_key_xtn);
    sslSessionID *sid = ss->sec.ci.sid;
    SSL3Statistics *ssl3stats = SSL_GetStatistics();

    rv = tls13_SetupNullCipherSpec(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    /* we need to call ssl3_SetupPendingCipherSpec here so we can check the
     * key exchange algorithm. */
    rv = tls13_SetupPendingCipherSpec(ss);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    if (isPSK) {
        PRBool cacheOK = PR_FALSE;
        do {
            if (ss->ssl3.hs.kea_def->authKeyType != ssl_auth_psk) {
                FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_SERVER_HELLO,
                            illegal_parameter);
                break;
            }
            rv = tls13_RecoverWrappedSharedSecret(ss, sid);
            if (rv != SECSuccess) {
                FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
                break;
            }
            cacheOK = PR_TRUE;
        } while (0);

        if (!cacheOK) {
            SSL_AtomicIncrementLong(&ssl3stats->hsh_sid_cache_not_ok);
            if (ss->sec.uncache)
                ss->sec.uncache(sid);
            return SECFailure;
        }

        tls13_RestoreCipherInfo(ss, sid);
        if (sid->peerCert) {
            ss->sec.peerCert = CERT_DupCertificate(sid->peerCert);
        }

        SSL_AtomicIncrementLong(&ssl3stats->hsh_sid_cache_hits);
        SSL_AtomicIncrementLong(&ssl3stats->hsh_sid_stateless_resumes);
    } else {
        /* No PSK negotiated.*/
        if (ss->ssl3.hs.kea_def->authKeyType == ssl_auth_psk) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_SERVER_HELLO,
                        illegal_parameter);
            return SECFailure;
        }
        if (ssl3_ClientExtensionAdvertised(ss, ssl_tls13_pre_shared_key_xtn)) {
            SSL_AtomicIncrementLong(&ssl3stats->hsh_sid_cache_misses);
        }

        /* Copy Signed Certificate Timestamps, if any. */
        if (ss->xtnData.signedCertTimestamps.data) {
            rv = SECITEM_CopyItem(NULL, &sid->u.ssl3.signedCertTimestamps,
                                  &ss->xtnData.signedCertTimestamps);
            if (rv != SECSuccess) {
                FATAL_ERROR(ss, SEC_ERROR_NO_MEMORY, internal_error);
                return SECFailure;
            }
            /* Clean up the temporary pointer to the handshake buffer. */
            ss->xtnData.signedCertTimestamps.data = NULL;
            ss->xtnData.signedCertTimestamps.len = 0;
        }
        ss->ssl3.hs.origCipherSuite = ss->ssl3.hs.cipher_suite;

        if (sid->cached == in_client_cache && (ss->sec.uncache)) {
            /* If we tried to resume and failed, let's not try again. */
            ss->sec.uncache(sid);
        }
    }

    /* Discard current SID and make a new one, though it may eventually
     * end up looking a lot like the old one.
     */
    ssl_FreeSID(sid);
    ss->sec.ci.sid = sid = ssl3_NewSessionID(ss, PR_FALSE);
    if (sid == NULL) {
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        return SECFailure;
    }
    if (isPSK && ss->sec.peerCert) {
        sid->peerCert = CERT_DupCertificate(ss->sec.peerCert);
    }
    sid->version = ss->version;
    sid->u.ssl3.cipherSuite = ss->ssl3.hs.origCipherSuite;

    rv = tls13_HandleServerKeyShare(ss);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    TLS13_SET_HS_STATE(ss, wait_encrypted_extensions);

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
    TLS13KeyShareEntry *entry;

    SSL_TRC(3, ("%d: TLS13[%d]: handle server_key_share handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    switch (ss->ssl3.hs.kea_def->exchKeyType) {
        case ssl_kea_ecdh:
        case ssl_kea_ecdh_psk:
            expectedGroup = ssl3_PubKey2ECName(ss->ephemeralECDHKeyPair->pubKey);
            break;
        default:
            FATAL_ERROR(ss, SEC_ERROR_UNSUPPORTED_KEYALG, handshake_failure);
            return SECFailure;
    }

    /* This list should have one entry. */
    if (PR_CLIST_IS_EMPTY(&ss->ssl3.hs.remoteKeyShares)) {
        FATAL_ERROR(ss, SSL_ERROR_MISSING_KEY_SHARE, missing_extension);
        return SECFailure;
    }
    entry = (TLS13KeyShareEntry *)PR_NEXT_LINK(&ss->ssl3.hs.remoteKeyShares);
    PORT_Assert(PR_NEXT_LINK(&entry->link) == &ss->ssl3.hs.remoteKeyShares);

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

/* Add a dummy initial cipher spec to the list. */
static SECStatus
tls13_SetupNullCipherSpec(sslSocket *ss)
{
    ssl3CipherSpec *spec = PORT_ZNew(ssl3CipherSpec);
    if (!spec) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    PR_APPEND_LINK(&spec->link, &ss->ssl3.hs.cipherSpecs);

    ssl3_InitCipherSpec(spec);
    spec->refCt = 2; /* One for read, one for write. */
    SSL_TRC(50, ("%d: TLS 1.3: Setting up cipher spec %d. ref ct = %d",
                 SSL_GETPID(), spec, spec->refCt));
    ssl_GetSpecWriteLock(ss);
    ss->ssl3.crSpec = ss->ssl3.cwSpec = spec;
    ssl_ReleaseSpecWriteLock(ss);

    return SECSuccess;
}

void
tls13_CipherSpecAddRef(ssl3CipherSpec *spec)
{
    ++spec->refCt;
    SSL_TRC(50, ("%d: TLS 1.3: Increment ref ct for spec %d. new ct = %d",
                 SSL_GETPID(), spec, spec->refCt));
}

/* This function is never called on a spec which is on the
 * cipherSpecs list. */
void
tls13_CipherSpecRelease(ssl3CipherSpec *spec)
{
    PORT_Assert(spec->refCt > 0);
    --spec->refCt;
    SSL_TRC(50, ("%d: TLS 1.3: Decrement ref ct for spec %d. new ct = %d",
                 SSL_GETPID(), spec, spec->refCt));
    if (!spec->refCt) {
        SSL_TRC(50, ("%d: TLS 1.3: Freeing spec %d",
                     SSL_GETPID(), spec));
        PR_REMOVE_LINK((PRCList *)spec);
        ssl3_DestroyCipherSpec(spec, PR_TRUE);
        PORT_Free(spec);
    }
}

/* Create a new cipher spec, throw it on the list, and
 * set it up. Sets |ss->ssl3.hs.pwSpec| as a convenient
 * side effect. */
static SECStatus
tls13_SetupPendingCipherSpec(sslSocket *ss)
{
    /* Note: we do not need a lock here because we never
     * manipulate this array unsafely from multiple threads. */
    ssl3CipherSpec *spec = PORT_ZNew(ssl3CipherSpec);
    if (!spec) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    spec->refCt = 2; /* One for read and one for write. */
    SSL_TRC(50, ("%d: TLS 1.3: Setting up cipher spec %d. ref ct = %d",
                 SSL_GETPID(), spec, spec->refCt));

    PR_APPEND_LINK(&spec->link, &ss->ssl3.hs.cipherSpecs);
    ss->ssl3.pwSpec = ss->ssl3.prSpec = spec;

    /* This is really overkill, because we need about 10% of
     * what ssl3_SetupPendingCipherSpec does. */
    return ssl3_SetupPendingCipherSpec(ss);
}

/*
 * Move the pointer forward for the current cipher spec. Optionally
 * free cipher suites if they have zero ref counts.
 */
static SECStatus
tls13_InstallCipherSpec(sslSocket *ss, InstallCipherSpecDirection direction)
{
    ssl3CipherSpec *spec;
    ssl3CipherSpec **specp;

    SSL_TRC(3, ("%d: TLS13[%d]: Installing new cipher specs direction = %s",
                SSL_GETPID(), ss->fd,
                direction == InstallCipherSpecRead ? "read" : "write"));

    ssl_GetSpecWriteLock(ss); /**************************************/

    /* Flush out any old stuff in the handshake buffers */
    switch (direction) {
        case InstallCipherSpecWrite:
            specp = &ss->ssl3.cwSpec;
            break;
        case InstallCipherSpecRead: {
            specp = &ss->ssl3.crSpec;
        } break;
        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            ssl_ReleaseSpecWriteLock(ss); /**************************************/
            return SECFailure;
    }

    /* Decrement the ref count on the cipher */
    spec = *specp;
    (*specp) = (ssl3CipherSpec *)PR_NEXT_LINK(&spec->link);
    tls13_CipherSpecRelease(spec);
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

    /* Double check that we are doing the same hash.*/
    PORT_Assert(hashes->len == tls13_GetHashSize(ss));

    ctx = PK11_CreateDigestContext(ssl3_TLSHashAlgorithmToOID(algorithm));
    if (!ctx) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }

    PORT_Assert(SECFailure);
    PORT_Assert(!SECSuccess);

    PRINT_BUF(50, (ss, "TLS 1.3 hash without context", hashes->u.raw, hashes->len));
    PRINT_BUF(50, (ss, "Context string", context_string, strlen(context_string)));
    rv |= PK11_DigestBegin(ctx);
    rv |= PK11_DigestOp(ctx, context_padding, sizeof(context_padding));
    rv |= PK11_DigestOp(ctx, (unsigned char *)context_string,
                        strlen(context_string) + 1); /* +1 includes the terminating 0 */
    rv |= PK11_DigestOp(ctx, hashes->u.raw, hashes->len);
    /* Update the hash in-place */
    rv |= PK11_DigestFinal(ctx, hashes->u.raw, &hashlength, sizeof(hashes->u.raw));
    PK11_DestroyContext(ctx, PR_TRUE);
    PRINT_BUF(50, (ss, "TLS 1.3 hash with context", hashes->u.raw, hashlength));

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

/* Derive traffic keys for the next cipher spec in the queue. */
static SECStatus
tls13_DeriveTrafficKeys(sslSocket *ss, ssl3CipherSpec *spec,
                        TrafficKeyType type)
{
    size_t keySize = spec->cipher_def->key_size;
    size_t ivSize = spec->cipher_def->iv_size +
                    spec->cipher_def->explicit_nonce_size; /* This isn't always going to
                                                              * work, but it does for
                                                              * AES-GCM */
    CK_MECHANISM_TYPE bulkAlgorithm = ssl3_Alg2Mech(spec->cipher_def->calg);
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
#define EXPAND_TRAFFIC_KEY(purpose_, target_)                               \
    do {                                                                    \
        FORMAT_LABEL(phase, purpose_);                                      \
        rv = tls13_HkdfExpandLabel(prk, tls13_GetHash(ss),                  \
                                   hashes.u.raw, hashes.len,                \
                                   label, strlen(label),                    \
                                   bulkAlgorithm, keySize, &spec->target_); \
        if (rv != SECSuccess) {                                             \
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);                       \
            PORT_Assert(0);                                                 \
            goto loser;                                                     \
        }                                                                   \
    } while (0)

#define EXPAND_TRAFFIC_IV(purpose_, target_)                    \
    do {                                                        \
        FORMAT_LABEL(phase, purpose_);                          \
        rv = tls13_HkdfExpandLabelRaw(prk, tls13_GetHash(ss),   \
                                      hashes.u.raw, hashes.len, \
                                      label, strlen(label),     \
                                      spec->target_, ivSize);   \
        if (rv != SECSuccess) {                                 \
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);           \
            PORT_Assert(0);                                     \
            goto loser;                                         \
        }                                                       \
    } while (0)

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSpecWriteLock(ss));

    rv = tls13_ComputeHandshakeHashes(ss, &hashes);
    if (rv != SECSuccess) {
        PORT_Assert(0); /* Should never fail */
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }
    PRINT_BUF(50, (ss, "Deriving traffic keys. Session hash=", hashes.u.raw,
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
tls13_InitCipherSpec(sslSocket *ss, TrafficKeyType type,
                     InstallCipherSpecDirection install)
{
    ssl3CipherSpec *spec =
        (ssl3CipherSpec *)PR_LIST_TAIL(&ss->ssl3.hs.cipherSpecs);

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

    cwSpec = ss->ssl3.cwSpec;

    switch (spec->cipher_def->calg) {
        case calg_aes_gcm:
            spec->aead = tls13_AESGCM;
            break;
        case calg_chacha20:
            spec->aead = tls13_ChaCha20Poly1305;
            break;
        default:
            PORT_Assert(0);
            goto loser;
            break;
    }

    /* We use the epoch for cipher suite identification, so increment
     * it in both TLS and DTLS. */
    if (cwSpec->epoch == PR_UINT16_MAX) {
        goto loser;
    }
    spec->epoch = cwSpec->epoch + 1;

    if (!IS_DTLS(ss)) {
        spec->read_seq_num.high = spec->write_seq_num.high = 0;
    } else {
        /* The sequence number has the high 16 bits as the epoch. */
        spec->read_seq_num.high = spec->write_seq_num.high =
            spec->epoch << 16;

        dtls_InitRecvdRecords(&spec->recvdRecords);
    }
    spec->read_seq_num.low = spec->write_seq_num.low = 0;

    rv = tls13_DeriveTrafficKeys(ss, spec, type);
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
tls13_ComputeHandshakeHashes(sslSocket *ss,
                             SSL3Hashes *hashes)
{
    SECStatus rv;
    PK11Context *ctx = NULL;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    /* TODO(ekr@rtfm.com): This first clause is futureproofing for
     * 0-RTT. */
    PORT_Assert(ss->ssl3.hs.hashType != handshake_hash_unknown);
    ctx = PK11_CloneContext(ss->ssl3.hs.sha);
    if (!ctx) {
        ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
        return SECFailure;
    }

    rv = PK11_DigestFinal(ctx, hashes->u.raw, &hashes->len,
                          sizeof(hashes->u.raw));
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
        goto loser;
    }

    PRINT_BUF(10, (NULL, "Handshake hash computed ",
                   hashes->u.raw, hashes->len));

    /* If we ever support ciphersuites where the PRF hash isn't SHA-256
     * then this will need to be updated. */
    PORT_Assert(hashes->len == tls13_GetHashSize(ss));
    hashes->hashAlg = tls13_GetHash(ss);

    PK11_DestroyContext(ctx, PR_TRUE);
    return SECSuccess;

loser:
    PK11_DestroyContext(ctx, PR_TRUE);
    return SECFailure;
}

static SECStatus
tls13_ComputeSecrets1(sslSocket *ss)
{
    SECStatus rv;
    PK11SymKey *mSS = NULL;
    PK11SymKey *mES = NULL;
    SSL3Hashes hashes;
    ssl3CipherSpec *pwSpec;

    rv = tls13_SetupPendingCipherSpec(ss);
    if (rv != SECSuccess) {
        return rv; /* error code set below. */
    }
    pwSpec = ss->ssl3.pwSpec;

    rv = tls13_ComputeHandshakeHashes(ss, &hashes);
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
                           &pwSpec->master_secret);

    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(pwSpec->master_secret,
                               tls13_GetHash(ss),
                               hashes.u.raw, hashes.len,
                               kHkdfLabelTrafficSecret,
                               strlen(kHkdfLabelTrafficSecret),
                               tls13_GetHkdfMechanism(ss),
                               hashes.len, &ss->ssl3.hs.trafficSecret);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(pwSpec->master_secret,
                               tls13_GetHash(ss),
                               NULL, 0,
                               kHkdfLabelClientFinishedSecret,
                               strlen(kHkdfLabelClientFinishedSecret),
                               tls13_GetHmacMechanism(ss),
                               hashes.len, &ss->ssl3.hs.clientFinishedSecret);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(pwSpec->master_secret,
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

    return rv;
}

static SECStatus
tls13_ComputeSecrets2(sslSocket *ss)
{
    SECStatus rv;
    SSL3Hashes hashes;
    ssl3CipherSpec *cwSpec = ss->ssl3.cwSpec;
    PK11SymKey *resumptionMasterSecret = NULL;

    rv = tls13_ComputeHandshakeHashes(ss, &hashes);
    if (rv != SECSuccess) {
        PORT_Assert(0); /* Should never fail */
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }

    rv = tls13_HkdfExpandLabel(cwSpec->master_secret,
                               tls13_GetHash(ss),
                               hashes.u.raw, hashes.len,
                               kHkdfLabelResumptionMasterSecret,
                               strlen(kHkdfLabelResumptionMasterSecret),
                               tls13_GetHkdfMechanism(ss),
                               hashes.len, &resumptionMasterSecret);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* This is pretty gross. TLS 1.3 uses a number of master secrets.
     * the master secret to generate the keys and then the resumption
     * master secret for future connections. To make this work without
     * refactoring too much of the SSLv3 code, we replace
     * |cwSpec->master_secret| with the resumption master secret.
     */
    PK11_FreeSymKey(cwSpec->master_secret);
    cwSpec->master_secret = resumptionMasterSecret;

loser:
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

void
tls13_DestroyCipherSpecs(PRCList *list)
{
    PRCList *cur_p;

    while (!PR_CLIST_IS_EMPTY(list)) {
        cur_p = PR_LIST_TAIL(list);
        PR_REMOVE_LINK(cur_p);
        ssl3_DestroyCipherSpec((ssl3CipherSpec *)cur_p, PR_FALSE);
        PORT_Free(cur_p);
    }
}

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
 * Existing suites have the same nonce size: N_MIN = N_MAX = 12 bytes
 *
 * See RFC 5288 and https://tools.ietf.org/html/draft-ietf-tls-chacha20-poly1305-04#section-2
 */
static void
tls13_WriteNonce(ssl3KeyMaterial *keys,
                 const unsigned char *seqNumBuf, unsigned int seqNumLen,
                 unsigned char *nonce, unsigned int nonceLen)
{
    size_t i;

    PORT_Assert(nonceLen == 12);
    memcpy(nonce, keys->write_iv, 12);

    /* XOR the last 8 bytes of the IV with the sequence number. */
    PORT_Assert(seqNumLen == 8);
    for (i = 0; i < 8; ++i) {
        nonce[4 + i] ^= seqNumBuf[i];
    }
}

/* Implement the SSLAEADCipher interface defined in sslimpl.h.
 *
 * That interface takes the additional data (see below) and reinterprets that as
 * a sequence number. In TLS 1.3 there is no additional data so this value is
 * just the encoded sequence number.
 */
static SECStatus
tls13_AEAD(ssl3KeyMaterial *keys, PRBool doDecrypt,
           unsigned char *out, int *outlen, int maxout,
           const unsigned char *in, int inlen,
           CK_MECHANISM_TYPE mechanism,
           unsigned char *aeadParams, unsigned int aeadParamLength)
{
    SECStatus rv;
    unsigned int uOutLen = 0;
    SECItem param = {
        siBuffer, aeadParams, aeadParamLength
    };

    if (doDecrypt) {
        rv = PK11_Decrypt(keys->write_key, mechanism, &param,
                          out, &uOutLen, maxout, in, inlen);
    } else {
        rv = PK11_Encrypt(keys->write_key, mechanism, &param,
                          out, &uOutLen, maxout, in, inlen);
    }
    *outlen = (int)uOutLen;

    return rv;
}

static SECStatus
tls13_AESGCM(ssl3KeyMaterial *keys,
             PRBool doDecrypt,
             unsigned char *out,
             int *outlen,
             int maxout,
             const unsigned char *in,
             int inlen,
             const unsigned char *additionalData,
             int additionalDataLen)
{
    CK_GCM_PARAMS gcmParams;
    unsigned char nonce[12];

    memset(&gcmParams, 0, sizeof(gcmParams));
    gcmParams.pIv = nonce;
    gcmParams.ulIvLen = sizeof(nonce);
    gcmParams.pAAD = NULL;
    gcmParams.ulAADLen = 0;
    gcmParams.ulTagBits = 128; /* GCM measures tag length in bits. */

    tls13_WriteNonce(keys, additionalData, additionalDataLen,
                     nonce, sizeof(nonce));
    return tls13_AEAD(keys, doDecrypt, out, outlen, maxout, in, inlen,
                      CKM_AES_GCM,
                      (unsigned char *)&gcmParams, sizeof(gcmParams));
}

static SECStatus
tls13_ChaCha20Poly1305(ssl3KeyMaterial *keys, PRBool doDecrypt,
                       unsigned char *out, int *outlen, int maxout,
                       const unsigned char *in, int inlen,
                       const unsigned char *additionalData,
                       int additionalDataLen)
{
    CK_NSS_AEAD_PARAMS aeadParams;
    unsigned char nonce[12];

    memset(&aeadParams, 0, sizeof(aeadParams));
    aeadParams.pNonce = nonce;
    aeadParams.ulNonceLen = sizeof(nonce);
    aeadParams.pAAD = NULL; /* No AAD in TLS 1.3. */
    aeadParams.ulAADLen = 0;
    aeadParams.ulTagLen = 16; /* The Poly1305 tag is 16 octets. */

    tls13_WriteNonce(keys, additionalData, additionalDataLen,
                     nonce, sizeof(nonce));
    return tls13_AEAD(keys, doDecrypt, out, outlen, maxout, in, inlen,
                      CKM_NSS_CHACHA20_POLY1305,
                      (unsigned char *)&aeadParams, sizeof(aeadParams));
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

    PORT_Assert(!ss->sec.isServer);

    if (ss->ssl3.hs.kea_def->authKeyType == ssl_auth_psk) {
        /* Compute the rest of the secrets except for the resumption
         * and exporter secret. */
        rv = tls13_ComputeSecrets1(ss);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            return SECFailure;
        }
        TLS13_SET_HS_STATE(ss, wait_finished);
    } else {
        TLS13_SET_HS_STATE(ss, wait_cert_request);
    }

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
    PRINT_BUF(50, (NULL, "Handshake hash", hashes->u.raw, hashes->len));

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

    rv = tls13_ComputeHandshakeHashes(ss, &hashes);
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

    rv = ssl3_FlushHandshake(ss,
                             (IS_DTLS(ss) && !ss->sec.isServer) ? ssl_SEND_FLAG_NO_RETRANSMIT : 0);
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
    if (ss->sec.isServer) {
        rv = tls13_InstallCipherSpec(ss, InstallCipherSpecRead);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            return SECFailure;
        }

        rv = tls13_FinishHandshake(ss);
        if (rv != SECSuccess) {
            return SECFailure; /* Error code and alerts handled below */
        }
        ssl_GetXmitBufLock(ss);
        if (ss->opt.enableSessionTickets &&
            ss->ssl3.hs.kea_def->authKeyType != ssl_auth_psk) {
            /* TODO(ekr@rtfm.com): Add support for new tickets in PSK. */
            rv = ssl3_SendNewSessionTicket(ss);
            if (rv != SECSuccess) {
                ssl_ReleaseXmitBufLock(ss);
                return SECFailure; /* Error code and alerts handled below */
            }
            rv = ssl3_FlushHandshake(ss, 0);
        }
        ssl_ReleaseXmitBufLock(ss);

    } else {
        rv = tls13_InitCipherSpec(ss, TrafficKeyApplicationData,
                                  InstallCipherSpecRead);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            return SECFailure;
        }

        rv = tls13_SendClientSecondRound(ss);
    }

    return rv;
}

static SECStatus
tls13_FinishHandshake(sslSocket *ss)
{
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->ssl3.hs.restartTarget == NULL);

    rv = tls13_ComputeSecrets2(ss);
    if (rv != SECSuccess)
        return SECFailure;

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
    if (ss->ssl3.hs.authCertificatePending) {
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

    rv = dtls_StartHolddownTimer(ss);
    if (rv != SECSuccess) {
        goto loser; /* err code was set. */
    }

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
    PRInt32 tmp;
    NewSessionTicket ticket;
    SECItem data;

    SSL_TRC(3, ("%d: TLS13[%d]: handle new session ticket message",
                SSL_GETPID(), ss->fd));

    rv = TLS13_CHECK_HS_STATE(ss, SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET,
                              idle_handshake);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    if (!ss->firstHsDone || ss->sec.isServer) {
        FATAL_ERROR(ss, SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET,
                    unexpected_message);
        return SECFailure;
    }

    ticket.received_timestamp = ssl_Time();
    tmp = ssl3_ConsumeHandshakeNumber(ss, 4, &b, &length);
    if (tmp < 0) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET,
                    decode_error);
        return SECFailure;
    }
    ticket.ticket_lifetime_hint = (PRUint32)tmp;
    ticket.ticket.type = siBuffer;
    rv = ssl3_ConsumeHandshakeVariable(ss, &data, 2, &b, &length);
    if (rv != SECSuccess || length != 0 || !data.len) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET,
                    decode_error);
        return SECFailure;
    }

    /* TODO(ekr@rtfm.com): Re-enable new tickets when PSK mode is
     * in use. I believe this works, but I can't test it until the
     * server side supports it. Bug 1257047.
     */
    if (!ss->opt.noCache && ss->sec.cache &&
        ss->ssl3.hs.kea_def->authKeyType != ssl_auth_psk) {

        /* Uncache so that we replace. */
        (*ss->sec.uncache)(ss->sec.ci.sid);

        rv = SECITEM_CopyItem(NULL, &ticket.ticket, &data);
        if (rv != SECSuccess) {
            FATAL_ERROR(ss, SEC_ERROR_NO_MEMORY, internal_error);
            return SECFailure;
        }
        PRINT_BUF(50, (ss, "Caching session ticket",
                       ticket.ticket.data,
                       ticket.ticket.len));

        ssl3_SetSIDSessionTicket(ss->sec.ci.sid, &ticket);
        PORT_Assert(!ticket.ticket.data);

        rv = ssl3_FillInCachedSID(ss, ss->sec.ci.sid);
        if (rv != SECSuccess)
            return SECFailure;

        /* Cache the session. */
        ss->sec.ci.sid->cached = never_cached;
        (*ss->sec.cache)(ss->sec.ci.sid);
    }

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
    { ssl_tls13_pre_shared_key_xtn,
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
                    ssl3CipherSpec *cwSpec,
                    SSL3ContentType type,
                    const SSL3Opaque *pIn,
                    PRUint32 contentLen,
                    sslBuffer *wrBuf)
{
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
        (void)tls13_EncodeUintX(
            dtls_TLSVersionToDTLSVersion(kDtlsRecordVersion), 2,
            &wrBuf->buf[1]);
        (void)tls13_EncodeUintX(cwSpec->write_seq_num.high, 4, &wrBuf->buf[3]);
        (void)tls13_EncodeUintX(cwSpec->write_seq_num.low, 4, &wrBuf->buf[7]);
        (void)tls13_EncodeUintX(cipherBytes, 2, &wrBuf->buf[11]);
    } else {
        (void)tls13_EncodeUintX(kTlsRecordVersion, 2, &wrBuf->buf[1]);
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
        SSL_TRC(3,
                ("%d: TLS13[%d]: record too short to contain valid AEAD data",
                 SSL_GETPID(), ss->fd));
        PORT_SetError(SSL_ERROR_BAD_MAC_READ);
        return SECFailure;
    }

    /* Verify that the content type is right, even though we overwrite it. */
    if (cText->type != content_application_data) {
        SSL_TRC(3,
                ("%d: TLS13[%d]: record has invalid exterior content type=%d",
                 SSL_GETPID(), ss->fd, cText->type));
        /* Do we need a better error here? */
        PORT_SetError(SSL_ERROR_BAD_MAC_READ);
        return SECFailure;
    }

    /* Check the version number in the record */
    if ((IS_DTLS(ss) && cText->version != kDtlsRecordVersion) ||
        (!IS_DTLS(ss) && cText->version != kTlsRecordVersion)) {
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
