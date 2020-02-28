/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "pk11pub.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "tls13hkdf.h"
#include "tls13subcerts.h"

SECStatus
SSL_GetChannelInfo(PRFileDesc *fd, SSLChannelInfo *info, PRUintn len)
{
    sslSocket *ss;
    SSLChannelInfo inf;
    sslSessionID *sid;

    /* Check if we can properly return the length of data written and that
     * we're not asked to return more information than we know how to provide.
     */
    if (!info || len < sizeof inf.length || len > sizeof inf) {
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
        SSLCipherSuiteInfo cinfo;
        SECStatus rv;

        sid = ss->sec.ci.sid;
        inf.protocolVersion = ss->version;
        inf.authKeyBits = ss->sec.authKeyBits;
        inf.keaKeyBits = ss->sec.keaKeyBits;

        ssl_GetSpecReadLock(ss);
        /* XXX  The cipher suite should be in the specs and this
         * function should get it from cwSpec rather than from the "hs".
         * See bug 275744 comment 69 and bug 766137.
         */
        inf.cipherSuite = ss->ssl3.hs.cipher_suite;
        ssl_ReleaseSpecReadLock(ss);
        inf.compressionMethod = ssl_compression_null;
        inf.compressionMethodName = "NULL";

        /* Fill in the cipher details from the cipher suite. */
        rv = SSL_GetCipherSuiteInfo(inf.cipherSuite,
                                    &cinfo, sizeof(cinfo));
        if (rv != SECSuccess) {
            return SECFailure; /* Error code already set. */
        }
        inf.symCipher = cinfo.symCipher;
        inf.macAlgorithm = cinfo.macAlgorithm;
        /* Get these fromm |ss->sec| because that is accurate
         * even with TLS 1.3 disaggregated cipher suites. */
        inf.keaType = ss->sec.keaType;
        inf.originalKeaGroup = ss->sec.originalKeaGroup
                                   ? ss->sec.originalKeaGroup->name
                                   : ssl_grp_none;
        inf.keaGroup = ss->sec.keaGroup
                           ? ss->sec.keaGroup->name
                           : ssl_grp_none;
        inf.keaKeyBits = ss->sec.keaKeyBits;
        inf.authType = ss->sec.authType;
        inf.authKeyBits = ss->sec.authKeyBits;
        inf.signatureScheme = ss->sec.signatureScheme;
        /* If this is a resumed session, signatureScheme isn't set in ss->sec.
         * Use the signature scheme from the previous handshake. */
        if (inf.signatureScheme == ssl_sig_none && sid->sigScheme) {
            inf.signatureScheme = sid->sigScheme;
        }
        inf.resumed = ss->statelessResume || ss->ssl3.hs.isResuming;
        inf.peerDelegCred = tls13_IsVerifyingWithDelegatedCredential(ss);

        if (sid) {
            unsigned int sidLen;

            inf.creationTime = sid->creationTime / PR_USEC_PER_SEC;
            inf.lastAccessTime = sid->lastAccessTime / PR_USEC_PER_SEC;
            inf.expirationTime = sid->expirationTime / PR_USEC_PER_SEC;
            inf.extendedMasterSecretUsed =
                (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 ||
                 sid->u.ssl3.keys.extendedMasterSecretUsed)
                    ? PR_TRUE
                    : PR_FALSE;

            inf.earlyDataAccepted =
                (ss->ssl3.hs.zeroRttState == ssl_0rtt_accepted ||
                 ss->ssl3.hs.zeroRttState == ssl_0rtt_done);
            sidLen = sid->u.ssl3.sessionIDLength;
            sidLen = PR_MIN(sidLen, sizeof inf.sessionID);
            inf.sessionIDLength = sidLen;
            memcpy(inf.sessionID, sid->u.ssl3.sessionID, sidLen);
        }
    }

    memcpy(info, &inf, inf.length);

    return SECSuccess;
}

SECStatus
SSL_GetPreliminaryChannelInfo(PRFileDesc *fd,
                              SSLPreliminaryChannelInfo *info,
                              PRUintn len)
{
    sslSocket *ss;
    SSLPreliminaryChannelInfo inf;

    /* Check if we can properly return the length of data written and that
     * we're not asked to return more information than we know how to provide.
     */
    if (!info || len < sizeof inf.length || len > sizeof inf) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetPreliminaryChannelInfo",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    memset(&inf, 0, sizeof(inf));
    inf.length = PR_MIN(sizeof(inf), len);

    inf.valuesSet = ss->ssl3.hs.preliminaryInfo;
    inf.protocolVersion = ss->version;
    inf.cipherSuite = ss->ssl3.hs.cipher_suite;
    inf.canSendEarlyData = !ss->sec.isServer &&
                           (ss->ssl3.hs.zeroRttState == ssl_0rtt_sent ||
                            ss->ssl3.hs.zeroRttState == ssl_0rtt_accepted);
    /* We shouldn't be able to send early data if the handshake is done. */
    PORT_Assert(!ss->firstHsDone || !inf.canSendEarlyData);

    if (ss->sec.ci.sid &&
        (ss->ssl3.hs.zeroRttState == ssl_0rtt_sent ||
         ss->ssl3.hs.zeroRttState == ssl_0rtt_accepted)) {
        inf.maxEarlyDataSize =
            ss->sec.ci.sid->u.ssl3.locked.sessionTicket.max_early_data_size;
    } else {
        inf.maxEarlyDataSize = 0;
    }
    inf.zeroRttCipherSuite = ss->ssl3.hs.zeroRttSuite;

    inf.peerDelegCred = tls13_IsVerifyingWithDelegatedCredential(ss);
    inf.authKeyBits = ss->sec.authKeyBits;
    inf.signatureScheme = ss->sec.signatureScheme;

    memcpy(info, &inf, inf.length);
    return SECSuccess;
}

/* name */
#define CS_(x) x, #x
#define CS(x) CS_(TLS_##x)

/* legacy values for authAlgorithm */
#define S_DSA "DSA", ssl_auth_dsa
/* S_RSA is incorrect for signature-based suites */
/* ECDH suites incorrectly report S_RSA or S_ECDSA */
#define S_RSA "RSA", ssl_auth_rsa_decrypt
#define S_ECDSA "ECDSA", ssl_auth_ecdsa
#define S_PSK "PSK", ssl_auth_psk
#define S_ANY "TLS 1.3", ssl_auth_tls13_any

/* real authentication algorithm */
#define A_DSA ssl_auth_dsa
#define A_RSAD ssl_auth_rsa_decrypt
#define A_RSAS ssl_auth_rsa_sign
#define A_ECDSA ssl_auth_ecdsa
#define A_ECDH_R ssl_auth_ecdh_rsa
#define A_ECDH_E ssl_auth_ecdh_ecdsa
#define A_PSK ssl_auth_psk
/* Report ssl_auth_null for export suites that can't decide between
 * ssl_auth_rsa_sign and ssl_auth_rsa_decrypt. */
#define A_EXP ssl_auth_null
#define A_ANY ssl_auth_tls13_any

/* key exchange */
#define K_DHE "DHE", ssl_kea_dh
#define K_RSA "RSA", ssl_kea_rsa
#define K_KEA "KEA", ssl_kea_kea
#define K_ECDH "ECDH", ssl_kea_ecdh
#define K_ECDHE "ECDHE", ssl_kea_ecdh
#define K_ECDHE_PSK "ECDHE-PSK", ssl_kea_ecdh_psk
#define K_DHE_PSK "DHE-PSK", ssl_kea_dh_psk
#define K_ANY "TLS 1.3", ssl_kea_tls13_any

/* record protection cipher */
#define C_SEED "SEED", ssl_calg_seed
#define C_CAMELLIA "CAMELLIA", ssl_calg_camellia
#define C_AES "AES", ssl_calg_aes
#define C_RC4 "RC4", ssl_calg_rc4
#define C_RC2 "RC2", ssl_calg_rc2
#define C_DES "DES", ssl_calg_des
#define C_3DES "3DES", ssl_calg_3des
#define C_NULL "NULL", ssl_calg_null
#define C_SJ "SKIPJACK", ssl_calg_sj
#define C_AESGCM "AES-GCM", ssl_calg_aes_gcm
#define C_CHACHA20 "CHACHA20POLY1305", ssl_calg_chacha20

/* "block cipher" sizes */
#define B_256 256, 256, 256
#define B_128 128, 128, 128
#define B_3DES 192, 156, 112
#define B_SJ 96, 80, 80
#define B_DES 64, 56, 56
#define B_56 128, 56, 56
#define B_40 128, 40, 40
#define B_0 0, 0, 0

/* "mac algorithm" and size */
#define M_AEAD_128 "AEAD", ssl_mac_aead, 128
#define M_SHA384 "SHA384", ssl_hmac_sha384, 384
#define M_SHA256 "SHA256", ssl_hmac_sha256, 256
#define M_SHA "SHA1", ssl_mac_sha, 160
#define M_MD5 "MD5", ssl_mac_md5, 128
#define M_NULL "NULL", ssl_mac_null, 0

/* flags: FIPS, exportable, nonstandard, reserved */
#define F_FIPS_STD 1, 0, 0, 0
#define F_FIPS_NSTD 1, 0, 1, 0
#define F_NFIPS_STD 0, 0, 0, 0
#define F_NFIPS_NSTD 0, 0, 1, 0 /* i.e., trash */
#define F_EXPORT 0, 1, 0, 0     /* i.e., trash */

// RFC 5705
#define MAX_CONTEXT_LEN PR_UINT16_MAX - 1

static const SSLCipherSuiteInfo suiteInfo[] = {
    /* <------ Cipher suite --------------------> <auth> <KEA>  <bulk cipher> <MAC> <FIPS> */
    { 0, CS_(TLS_AES_128_GCM_SHA256), S_ANY, K_ANY, C_AESGCM, B_128, M_AEAD_128, F_FIPS_STD, A_ANY, ssl_hash_sha256 },
    { 0, CS_(TLS_CHACHA20_POLY1305_SHA256), S_ANY, K_ANY, C_CHACHA20, B_256, M_AEAD_128, F_NFIPS_STD, A_ANY, ssl_hash_sha256 },
    { 0, CS_(TLS_AES_256_GCM_SHA384), S_ANY, K_ANY, C_AESGCM, B_256, M_AEAD_128, F_FIPS_STD, A_ANY, ssl_hash_sha384 },

    { 0, CS(RSA_WITH_AES_128_GCM_SHA256), S_RSA, K_RSA, C_AESGCM, B_128, M_AEAD_128, F_FIPS_STD, A_RSAD, ssl_hash_sha256 },
    { 0, CS(DHE_RSA_WITH_CHACHA20_POLY1305_SHA256), S_RSA, K_DHE, C_CHACHA20, B_256, M_AEAD_128, F_NFIPS_STD, A_RSAS, ssl_hash_sha256 },

    { 0, CS(DHE_RSA_WITH_CAMELLIA_256_CBC_SHA), S_RSA, K_DHE, C_CAMELLIA, B_256, M_SHA, F_NFIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_CAMELLIA_256_CBC_SHA), S_DSA, K_DHE, C_CAMELLIA, B_256, M_SHA, F_NFIPS_STD, A_DSA, ssl_hash_none },
    { 0, CS(DHE_RSA_WITH_AES_256_CBC_SHA256), S_RSA, K_DHE, C_AES, B_256, M_SHA256, F_FIPS_STD, A_RSAS, ssl_hash_sha256 },
    { 0, CS(DHE_RSA_WITH_AES_256_CBC_SHA), S_RSA, K_DHE, C_AES, B_256, M_SHA, F_FIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_AES_256_CBC_SHA), S_DSA, K_DHE, C_AES, B_256, M_SHA, F_FIPS_STD, A_DSA, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_AES_256_CBC_SHA256), S_DSA, K_DHE, C_AES, B_256, M_SHA256, F_FIPS_STD, A_DSA, ssl_hash_sha256 },
    { 0, CS(RSA_WITH_CAMELLIA_256_CBC_SHA), S_RSA, K_RSA, C_CAMELLIA, B_256, M_SHA, F_NFIPS_STD, A_RSAD, ssl_hash_none },
    { 0, CS(RSA_WITH_AES_256_CBC_SHA256), S_RSA, K_RSA, C_AES, B_256, M_SHA256, F_FIPS_STD, A_RSAD, ssl_hash_sha256 },
    { 0, CS(RSA_WITH_AES_256_CBC_SHA), S_RSA, K_RSA, C_AES, B_256, M_SHA, F_FIPS_STD, A_RSAD, ssl_hash_none },

    { 0, CS(DHE_RSA_WITH_CAMELLIA_128_CBC_SHA), S_RSA, K_DHE, C_CAMELLIA, B_128, M_SHA, F_NFIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_CAMELLIA_128_CBC_SHA), S_DSA, K_DHE, C_CAMELLIA, B_128, M_SHA, F_NFIPS_STD, A_DSA, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_RC4_128_SHA), S_DSA, K_DHE, C_RC4, B_128, M_SHA, F_NFIPS_STD, A_DSA, ssl_hash_none },
    { 0, CS(DHE_RSA_WITH_AES_128_CBC_SHA256), S_RSA, K_DHE, C_AES, B_128, M_SHA256, F_FIPS_STD, A_RSAS, ssl_hash_sha256 },
    { 0, CS(DHE_RSA_WITH_AES_128_GCM_SHA256), S_RSA, K_DHE, C_AESGCM, B_128, M_AEAD_128, F_FIPS_STD, A_RSAS, ssl_hash_sha256 },
    { 0, CS(DHE_RSA_WITH_AES_128_CBC_SHA), S_RSA, K_DHE, C_AES, B_128, M_SHA, F_FIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_AES_128_GCM_SHA256), S_DSA, K_DHE, C_AESGCM, B_128, M_AEAD_128, F_FIPS_STD, A_DSA, ssl_hash_sha256 },
    { 0, CS(DHE_DSS_WITH_AES_128_CBC_SHA), S_DSA, K_DHE, C_AES, B_128, M_SHA, F_FIPS_STD, A_DSA, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_AES_128_CBC_SHA256), S_DSA, K_DHE, C_AES, B_128, M_SHA256, F_FIPS_STD, A_DSA, ssl_hash_sha256 },
    { 0, CS(RSA_WITH_SEED_CBC_SHA), S_RSA, K_RSA, C_SEED, B_128, M_SHA, F_FIPS_STD, A_RSAD, ssl_hash_none },
    { 0, CS(RSA_WITH_CAMELLIA_128_CBC_SHA), S_RSA, K_RSA, C_CAMELLIA, B_128, M_SHA, F_NFIPS_STD, A_RSAD, ssl_hash_none },
    { 0, CS(RSA_WITH_RC4_128_SHA), S_RSA, K_RSA, C_RC4, B_128, M_SHA, F_NFIPS_STD, A_RSAD, ssl_hash_none },
    { 0, CS(RSA_WITH_RC4_128_MD5), S_RSA, K_RSA, C_RC4, B_128, M_MD5, F_NFIPS_STD, A_RSAD, ssl_hash_none },
    { 0, CS(RSA_WITH_AES_128_CBC_SHA256), S_RSA, K_RSA, C_AES, B_128, M_SHA256, F_FIPS_STD, A_RSAD, ssl_hash_sha256 },
    { 0, CS(RSA_WITH_AES_128_CBC_SHA), S_RSA, K_RSA, C_AES, B_128, M_SHA, F_FIPS_STD, A_RSAD, ssl_hash_none },

    { 0, CS(DHE_RSA_WITH_3DES_EDE_CBC_SHA), S_RSA, K_DHE, C_3DES, B_3DES, M_SHA, F_FIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_3DES_EDE_CBC_SHA), S_DSA, K_DHE, C_3DES, B_3DES, M_SHA, F_FIPS_STD, A_DSA, ssl_hash_none },
    { 0, CS(RSA_WITH_3DES_EDE_CBC_SHA), S_RSA, K_RSA, C_3DES, B_3DES, M_SHA, F_FIPS_STD, A_RSAD, ssl_hash_none },

    { 0, CS(DHE_RSA_WITH_DES_CBC_SHA), S_RSA, K_DHE, C_DES, B_DES, M_SHA, F_NFIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(DHE_DSS_WITH_DES_CBC_SHA), S_DSA, K_DHE, C_DES, B_DES, M_SHA, F_NFIPS_STD, A_DSA, ssl_hash_none },
    { 0, CS(RSA_WITH_DES_CBC_SHA), S_RSA, K_RSA, C_DES, B_DES, M_SHA, F_NFIPS_STD, A_RSAD, ssl_hash_none },

    { 0, CS(RSA_WITH_NULL_SHA256), S_RSA, K_RSA, C_NULL, B_0, M_SHA256, F_EXPORT, A_RSAD, ssl_hash_sha256 },
    { 0, CS(RSA_WITH_NULL_SHA), S_RSA, K_RSA, C_NULL, B_0, M_SHA, F_EXPORT, A_RSAD, ssl_hash_none },
    { 0, CS(RSA_WITH_NULL_MD5), S_RSA, K_RSA, C_NULL, B_0, M_MD5, F_EXPORT, A_RSAD, ssl_hash_none },

    /* ECC cipher suites */
    { 0, CS(ECDHE_RSA_WITH_AES_128_GCM_SHA256), S_RSA, K_ECDHE, C_AESGCM, B_128, M_AEAD_128, F_FIPS_STD, A_RSAS, ssl_hash_sha256 },
    { 0, CS(ECDHE_ECDSA_WITH_AES_128_GCM_SHA256), S_ECDSA, K_ECDHE, C_AESGCM, B_128, M_AEAD_128, F_FIPS_STD, A_ECDSA, ssl_hash_sha256 },
    { 0, CS(ECDH_ECDSA_WITH_NULL_SHA), S_ECDSA, K_ECDH, C_NULL, B_0, M_SHA, F_NFIPS_STD, A_ECDH_E, ssl_hash_none },
    { 0, CS(ECDH_ECDSA_WITH_RC4_128_SHA), S_ECDSA, K_ECDH, C_RC4, B_128, M_SHA, F_NFIPS_STD, A_ECDH_E, ssl_hash_none },
    { 0, CS(ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA), S_ECDSA, K_ECDH, C_3DES, B_3DES, M_SHA, F_FIPS_STD, A_ECDH_E, ssl_hash_none },
    { 0, CS(ECDH_ECDSA_WITH_AES_128_CBC_SHA), S_ECDSA, K_ECDH, C_AES, B_128, M_SHA, F_FIPS_STD, A_ECDH_E, ssl_hash_none },
    { 0, CS(ECDH_ECDSA_WITH_AES_256_CBC_SHA), S_ECDSA, K_ECDH, C_AES, B_256, M_SHA, F_FIPS_STD, A_ECDH_E, ssl_hash_none },

    { 0, CS(ECDHE_ECDSA_WITH_NULL_SHA), S_ECDSA, K_ECDHE, C_NULL, B_0, M_SHA, F_NFIPS_STD, A_ECDSA, ssl_hash_none },
    { 0, CS(ECDHE_ECDSA_WITH_RC4_128_SHA), S_ECDSA, K_ECDHE, C_RC4, B_128, M_SHA, F_NFIPS_STD, A_ECDSA, ssl_hash_none },
    { 0, CS(ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA), S_ECDSA, K_ECDHE, C_3DES, B_3DES, M_SHA, F_FIPS_STD, A_ECDSA, ssl_hash_none },
    { 0, CS(ECDHE_ECDSA_WITH_AES_128_CBC_SHA), S_ECDSA, K_ECDHE, C_AES, B_128, M_SHA, F_FIPS_STD, A_ECDSA, ssl_hash_none },
    { 0, CS(ECDHE_ECDSA_WITH_AES_128_CBC_SHA256), S_ECDSA, K_ECDHE, C_AES, B_128, M_SHA256, F_FIPS_STD, A_ECDSA, ssl_hash_sha256 },
    { 0, CS(ECDHE_ECDSA_WITH_AES_256_CBC_SHA), S_ECDSA, K_ECDHE, C_AES, B_256, M_SHA, F_FIPS_STD, A_ECDSA, ssl_hash_none },
    { 0, CS(ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256), S_ECDSA, K_ECDHE, C_CHACHA20, B_256, M_AEAD_128, F_NFIPS_STD, A_ECDSA, ssl_hash_sha256 },

    { 0, CS(ECDH_RSA_WITH_NULL_SHA), S_RSA, K_ECDH, C_NULL, B_0, M_SHA, F_NFIPS_STD, A_ECDH_R, ssl_hash_none },
    { 0, CS(ECDH_RSA_WITH_RC4_128_SHA), S_RSA, K_ECDH, C_RC4, B_128, M_SHA, F_NFIPS_STD, A_ECDH_R, ssl_hash_none },
    { 0, CS(ECDH_RSA_WITH_3DES_EDE_CBC_SHA), S_RSA, K_ECDH, C_3DES, B_3DES, M_SHA, F_FIPS_STD, A_ECDH_R, ssl_hash_none },
    { 0, CS(ECDH_RSA_WITH_AES_128_CBC_SHA), S_RSA, K_ECDH, C_AES, B_128, M_SHA, F_FIPS_STD, A_ECDH_R, ssl_hash_none },
    { 0, CS(ECDH_RSA_WITH_AES_256_CBC_SHA), S_RSA, K_ECDH, C_AES, B_256, M_SHA, F_FIPS_STD, A_ECDH_R, ssl_hash_none },

    { 0, CS(ECDHE_RSA_WITH_NULL_SHA), S_RSA, K_ECDHE, C_NULL, B_0, M_SHA, F_NFIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(ECDHE_RSA_WITH_RC4_128_SHA), S_RSA, K_ECDHE, C_RC4, B_128, M_SHA, F_NFIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(ECDHE_RSA_WITH_3DES_EDE_CBC_SHA), S_RSA, K_ECDHE, C_3DES, B_3DES, M_SHA, F_FIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(ECDHE_RSA_WITH_AES_128_CBC_SHA), S_RSA, K_ECDHE, C_AES, B_128, M_SHA, F_FIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(ECDHE_RSA_WITH_AES_128_CBC_SHA256), S_RSA, K_ECDHE, C_AES, B_128, M_SHA256, F_FIPS_STD, A_RSAS, ssl_hash_sha256 },
    { 0, CS(ECDHE_RSA_WITH_AES_256_CBC_SHA), S_RSA, K_ECDHE, C_AES, B_256, M_SHA, F_FIPS_STD, A_RSAS, ssl_hash_none },
    { 0, CS(ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256), S_RSA, K_ECDHE, C_CHACHA20, B_256, M_AEAD_128, F_NFIPS_STD, A_RSAS, ssl_hash_sha256 },
    { 0, CS(ECDHE_RSA_WITH_AES_256_CBC_SHA384), S_RSA, K_ECDHE, C_AES, B_256, M_SHA384, F_FIPS_STD, A_RSAS, ssl_hash_sha384 },
    { 0, CS(ECDHE_ECDSA_WITH_AES_256_CBC_SHA384), S_ECDSA, K_ECDHE, C_AES, B_256, M_SHA384, F_FIPS_STD, A_ECDSA, ssl_hash_sha384 },
    { 0, CS(ECDHE_ECDSA_WITH_AES_256_GCM_SHA384), S_ECDSA, K_ECDHE, C_AESGCM, B_256, M_AEAD_128, F_FIPS_STD, A_ECDSA, ssl_hash_sha384 },
    { 0, CS(ECDHE_RSA_WITH_AES_256_GCM_SHA384), S_RSA, K_ECDHE, C_AESGCM, B_256, M_AEAD_128, F_FIPS_STD, A_RSAS, ssl_hash_sha384 },

    { 0, CS(DHE_DSS_WITH_AES_256_GCM_SHA384), S_DSA, K_DHE, C_AESGCM, B_256, M_AEAD_128, F_FIPS_STD, A_DSA, ssl_hash_sha384 },
    { 0, CS(DHE_RSA_WITH_AES_256_GCM_SHA384), S_RSA, K_DHE, C_AESGCM, B_256, M_AEAD_128, F_FIPS_STD, A_RSAS, ssl_hash_sha384 },
    { 0, CS(RSA_WITH_AES_256_GCM_SHA384), S_RSA, K_RSA, C_AESGCM, B_256, M_AEAD_128, F_FIPS_STD, A_RSAD, ssl_hash_sha384 },
};

#define NUM_SUITEINFOS ((sizeof suiteInfo) / (sizeof suiteInfo[0]))

SECStatus
SSL_GetCipherSuiteInfo(PRUint16 cipherSuite,
                       SSLCipherSuiteInfo *info, PRUintn len)
{
    unsigned int i;

    /* Check if we can properly return the length of data written and that
     * we're not asked to return more information than we know how to provide.
     */
    if (!info || len < sizeof suiteInfo[0].length ||
        len > sizeof suiteInfo[0]) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    len = PR_MIN(len, sizeof suiteInfo[0]);
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

SECItem *
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
        if (ss->version > SSL_LIBRARY_VERSION_3_0) { /* TLS */
            SECItem *crsName;
            ssl_GetSpecReadLock(ss); /*********************************/
            crsName = &ss->ssl3.hs.srvVirtName;
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
        sniName->data = (void *)name;
        sniName->len = PORT_Strlen(name);
    }
    return sniName;
}

/*
 *     HKDF-Expand-Label(Derive-Secret(Secret, label, ""),
 *                       "exporter", Hash(context_value), key_length)
 */
static SECStatus
tls13_Exporter(sslSocket *ss, PK11SymKey *secret,
               const char *label, unsigned int labelLen,
               const unsigned char *context, unsigned int contextLen,
               unsigned char *out, unsigned int outLen)
{
    SSL3Hashes contextHash;
    PK11SymKey *innerSecret = NULL;
    SECStatus rv;

    static const char *kExporterInnerLabel = "exporter";

    if (!secret) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Pre-hash the context. */
    rv = tls13_ComputeHash(ss, &contextHash, context, contextLen);
    if (rv != SECSuccess) {
        return rv;
    }

    rv = tls13_DeriveSecretNullHash(ss, secret, label, labelLen,
                                    &innerSecret);
    if (rv != SECSuccess) {
        return rv;
    }

    rv = tls13_HkdfExpandLabelRaw(innerSecret,
                                  tls13_GetHash(ss),
                                  contextHash.u.raw, contextHash.len,
                                  kExporterInnerLabel,
                                  strlen(kExporterInnerLabel),
                                  ss->protocolVariant, out, outLen);
    PK11_FreeSymKey(innerSecret);
    return rv;
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

    if (!label || !labelLen || !out || !outLen ||
        (hasContext && (!context || !contextLen))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        return tls13_Exporter(ss, ss->ssl3.hs.exporterSecret,
                              label, labelLen,
                              context, hasContext ? contextLen : 0,
                              out, outLen);
    }

    if (hasContext && contextLen > MAX_CONTEXT_LEN) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
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
    PORT_Memcpy(val + i, ss->ssl3.hs.client_random, SSL3_RANDOM_LENGTH);
    i += SSL3_RANDOM_LENGTH;
    PORT_Memcpy(val + i, ss->ssl3.hs.server_random, SSL3_RANDOM_LENGTH);
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
    if (!ss->ssl3.cwSpec->masterSecret) {
        PORT_SetError(SSL_ERROR_HANDSHAKE_NOT_COMPLETED);
        rv = SECFailure;
    } else {
        rv = ssl3_TLSPRFWithMasterSecret(ss, ss->ssl3.cwSpec, label, labelLen,
                                         val, valLen, out, outLen);
    }
    ssl_ReleaseSpecReadLock(ss);

    PORT_ZFree(val, valLen);
    return rv;
}

SECStatus
SSL_ExportEarlyKeyingMaterial(PRFileDesc *fd,
                              const char *label, unsigned int labelLen,
                              const unsigned char *context,
                              unsigned int contextLen,
                              unsigned char *out, unsigned int outLen)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_ExportEarlyKeyingMaterial",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    if (!label || !labelLen || !out || !outLen ||
        (!context && contextLen)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    return tls13_Exporter(ss, ss->ssl3.hs.earlyExporterSecret,
                          label, labelLen, context, contextLen,
                          out, outLen);
}
