/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SSL3 Protocol
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* TODO(ekr): Implement HelloVerifyRequest on server side. OK for now. */

#include "cert.h"
#include "ssl.h"
#include "cryptohi.h" /* for DSAU_ stuff */
#include "keyhi.h"
#include "secder.h"
#include "secitem.h"
#include "sechash.h"

#include "sslimpl.h"
#include "sslproto.h"
#include "sslerr.h"
#include "ssl3ext.h"
#include "ssl3exthandle.h"
#include "prtime.h"
#include "prinrval.h"
#include "prerror.h"
#include "pratom.h"
#include "prthread.h"
#include "nss.h"
#include "nssoptions.h"

#include "pk11func.h"
#include "secmod.h"
#include "blapi.h"

#include <stdio.h>
#ifdef NSS_SSL_ENABLE_ZLIB
#include "zlib.h"
#endif

#ifndef PK11_SETATTRS
#define PK11_SETATTRS(x, id, v, l) \
    (x)->type = (id);              \
    (x)->pValue = (v);             \
    (x)->ulValueLen = (l);
#endif

static PK11SymKey *ssl3_GenerateRSAPMS(sslSocket *ss, ssl3CipherSpec *spec,
                                       PK11SlotInfo *serverKeySlot);
static SECStatus ssl3_DeriveMasterSecret(sslSocket *ss, PK11SymKey *pms);
static SECStatus ssl3_DeriveConnectionKeys(sslSocket *ss);
static SECStatus ssl3_HandshakeFailure(sslSocket *ss);
static SECStatus ssl3_SendCertificate(sslSocket *ss);
static SECStatus ssl3_SendCertificateRequest(sslSocket *ss);
static SECStatus ssl3_SendNextProto(sslSocket *ss);
static SECStatus ssl3_SendFinished(sslSocket *ss, PRInt32 flags);
static SECStatus ssl3_SendServerHelloDone(sslSocket *ss);
static SECStatus ssl3_SendServerKeyExchange(sslSocket *ss);
static SECStatus ssl3_HandleClientHelloPart2(sslSocket *ss,
                                             SECItem *suites,
                                             SECItem *comps,
                                             sslSessionID *sid);
static SECStatus ssl3_HandleServerHelloPart2(sslSocket *ss,
                                             const SECItem *sidBytes,
                                             int *retErrCode);
static SECStatus ssl3_HandlePostHelloHandshakeMessage(sslSocket *ss,
                                                      SSL3Opaque *b,
                                                      PRUint32 length,
                                                      SSL3Hashes *hashesPtr);
static SECStatus ssl3_FlushHandshakeMessages(sslSocket *ss, PRInt32 flags);

static SECStatus Null_Cipher(void *ctx, unsigned char *output, int *outputLen,
                             int maxOutputLen, const unsigned char *input,
                             int inputLen);

static CK_MECHANISM_TYPE ssl3_GetHashMechanismByHashType(SSLHashType hashType);
static CK_MECHANISM_TYPE ssl3_GetMgfMechanismByHashType(SSLHashType hash);
PRBool ssl_IsRsaPssSignatureScheme(SSLSignatureScheme scheme);

#define MAX_SEND_BUF_LENGTH 32000 /* watch for 16-bit integer overflow */
#define MIN_SEND_BUF_LENGTH 4000

/* This list of SSL3 cipher suites is sorted in descending order of
 * precedence (desirability).  It only includes cipher suites we implement.
 * This table is modified by SSL3_SetPolicy(). The ordering of cipher suites
 * in this table must match the ordering in SSL_ImplementedCiphers (sslenum.c)
 *
 * Important: See bug 946147 before enabling, reordering, or adding any cipher
 * suites to this list.
 */
/* clang-format off */
static ssl3CipherSuiteCfg cipherSuites[ssl_V3_SUITES_IMPLEMENTED] = {
   /*      cipher_suite                     policy       enabled   isPresent */
 /* Special TLS 1.3 suites. */
 { TLS_AES_128_GCM_SHA256, SSL_ALLOWED, PR_TRUE, PR_FALSE },
 { TLS_CHACHA20_POLY1305_SHA256, SSL_ALLOWED, PR_TRUE, PR_FALSE },
 { TLS_AES_256_GCM_SHA384, SSL_ALLOWED, PR_TRUE, PR_FALSE },

 { TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,   SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,   SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,   SSL_ALLOWED, PR_FALSE, PR_FALSE},
   /* TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA is out of order to work around
    * bug 946147.
    */
 { TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,    SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,    SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,      SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,   SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,      SSL_ALLOWED, PR_TRUE, PR_FALSE},
 { TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,   SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,   SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,        SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_RC4_128_SHA,          SSL_ALLOWED, PR_FALSE, PR_FALSE},

 { TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,     SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,SSL_ALLOWED,PR_TRUE,  PR_FALSE},
 { TLS_DHE_DSS_WITH_AES_128_GCM_SHA256,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_DSS_WITH_AES_256_GCM_SHA384,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_RSA_WITH_AES_128_CBC_SHA,        SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_DSS_WITH_AES_128_CBC_SHA,        SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,     SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,   SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,   SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_RSA_WITH_AES_256_CBC_SHA,        SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_DSS_WITH_AES_256_CBC_SHA,        SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,     SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,   SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,   SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,       SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,       SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_DHE_DSS_WITH_RC4_128_SHA,            SSL_ALLOWED, PR_FALSE, PR_FALSE},

 { TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,       SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,     SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,       SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,    SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,      SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_ECDSA_WITH_RC4_128_SHA,         SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_RSA_WITH_RC4_128_SHA,           SSL_ALLOWED, PR_FALSE, PR_FALSE},

 /* RSA */
 { TLS_RSA_WITH_AES_128_GCM_SHA256,         SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_RSA_WITH_AES_256_GCM_SHA384,         SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_AES_128_CBC_SHA,            SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_RSA_WITH_AES_128_CBC_SHA256,         SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,       SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_AES_256_CBC_SHA,            SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_RSA_WITH_AES_256_CBC_SHA256,         SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,       SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_SEED_CBC_SHA,               SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_3DES_EDE_CBC_SHA,           SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_RSA_WITH_RC4_128_SHA,                SSL_ALLOWED, PR_TRUE,  PR_FALSE},
 { TLS_RSA_WITH_RC4_128_MD5,                SSL_ALLOWED, PR_TRUE,  PR_FALSE},

 /* 56-bit DES "domestic" cipher suites */
 { TLS_DHE_RSA_WITH_DES_CBC_SHA,            SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_DHE_DSS_WITH_DES_CBC_SHA,            SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_DES_CBC_SHA,                SSL_ALLOWED, PR_FALSE, PR_FALSE},

 /* ciphersuites with no encryption */
 { TLS_ECDHE_ECDSA_WITH_NULL_SHA,           SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDHE_RSA_WITH_NULL_SHA,             SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_RSA_WITH_NULL_SHA,              SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_ECDH_ECDSA_WITH_NULL_SHA,            SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_NULL_SHA,                   SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_NULL_SHA256,                SSL_ALLOWED, PR_FALSE, PR_FALSE},
 { TLS_RSA_WITH_NULL_MD5,                   SSL_ALLOWED, PR_FALSE, PR_FALSE},
};
/* clang-format on */

/* This is the default supported set of signature schemes.  The order of the
 * hashes here is all that is important, since that will (sometimes) determine
 * which hash we use.  The key pair (i.e., cert) is the primary thing that
 * determines what we use and this doesn't affect how we select key pairs.  The
 * order of signature types is based on the same rules for ordering we use for
 * cipher suites just for consistency.
 */
static const SSLSignatureScheme defaultSignatureSchemes[] = {
    ssl_sig_ecdsa_secp256r1_sha256,
    ssl_sig_ecdsa_secp384r1_sha384,
    ssl_sig_ecdsa_secp521r1_sha512,
    ssl_sig_ecdsa_sha1,
    ssl_sig_rsa_pss_sha256,
    ssl_sig_rsa_pss_sha384,
    ssl_sig_rsa_pss_sha512,
    ssl_sig_rsa_pkcs1_sha256,
    ssl_sig_rsa_pkcs1_sha384,
    ssl_sig_rsa_pkcs1_sha512,
    ssl_sig_rsa_pkcs1_sha1,
    ssl_sig_dsa_sha256,
    ssl_sig_dsa_sha384,
    ssl_sig_dsa_sha512,
    ssl_sig_dsa_sha1
};
PR_STATIC_ASSERT(PR_ARRAY_SIZE(defaultSignatureSchemes) <=
                 MAX_SIGNATURE_SCHEMES);

/* Verify that SSL_ImplementedCiphers and cipherSuites are in consistent order.
 */
#ifdef DEBUG
void
ssl3_CheckCipherSuiteOrderConsistency()
{
    unsigned int i;

    PORT_Assert(SSL_NumImplementedCiphers == PR_ARRAY_SIZE(cipherSuites));

    for (i = 0; i < PR_ARRAY_SIZE(cipherSuites); ++i) {
        PORT_Assert(SSL_ImplementedCiphers[i] == cipherSuites[i].cipher_suite);
    }
}
#endif

/* This list of SSL3 compression methods is sorted in descending order of
 * precedence (desirability).  It only includes compression methods we
 * implement.
 */
static const SSLCompressionMethod ssl_compression_methods[] = {
#ifdef NSS_SSL_ENABLE_ZLIB
    ssl_compression_deflate,
#endif
    ssl_compression_null
};

static const unsigned int ssl_compression_method_count =
    PR_ARRAY_SIZE(ssl_compression_methods);

/* compressionEnabled returns true iff the compression algorithm is enabled
 * for the given SSL socket. */
static PRBool
ssl_CompressionEnabled(sslSocket *ss, SSLCompressionMethod compression)
{
    SSL3ProtocolVersion version;

    if (compression == ssl_compression_null) {
        return PR_TRUE; /* Always enabled */
    }
    if (ss->sec.isServer) {
        /* We can't easily check that the client didn't attempt TLS 1.3,
         * so this will have to do. */
        PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);
        version = ss->version;
    } else {
        version = ss->vrange.max;
    }
    if (version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        return PR_FALSE;
    }
#ifdef NSS_SSL_ENABLE_ZLIB
    if (compression == ssl_compression_deflate) {
        if (IS_DTLS(ss)) {
            return PR_FALSE;
        }
        return ss->opt.enableDeflate;
    }
#endif
    return PR_FALSE;
}

static const /*SSL3ClientCertificateType */ PRUint8 certificate_types[] = {
    ct_RSA_sign,
    ct_ECDSA_sign,
    ct_DSS_sign,
};

/* This global item is used only in servers.  It is is initialized by
** SSL_ConfigSecureServer(), and is used in ssl3_SendCertificateRequest().
*/
CERTDistNames *ssl3_server_ca_list = NULL;
static SSL3Statistics ssl3stats;

/* Record protection algorithms, indexed by SSL3BulkCipher.
 *
 * The |max_records| field (|mr| below) is set to a number that is higher than
 * recommended in some literature (esp. TLS 1.3) because we currently abort the
 * connection when this limit is reached and we want to ensure that we only
 * rarely hit this limit.  See bug 1268745 for details.
 */
#define MR_MAX RECORD_SEQ_MAX  /* 2^48-1 */
#define MR_128 (0x5aULL << 28) /* For AES and similar. */
#define MR_LOW (1ULL << 20)    /* For weak ciphers. */
/* clang-format off */
static const ssl3BulkCipherDef bulk_cipher_defs[] = {
    /*                                    |--------- Lengths ---------| */
    /* cipher             calg            :  s                        : */
    /*                                    :  e                  b     n */
    /* oid                short_name mr   :                     l     o */
    /*                                    k  r                  o  t  n */
    /*                                    e  e               i  c  a  c */
    /*                                    y  t  type         v  k  g  e */
    {cipher_null,         calg_null,      0, 0, type_stream, 0, 0, 0, 0,
     SEC_OID_NULL_CIPHER, "NULL", MR_MAX},
    {cipher_rc4,          calg_rc4,      16,16, type_stream, 0, 0, 0, 0,
     SEC_OID_RC4,         "RC4", MR_LOW},
    {cipher_des,          calg_des,       8, 8, type_block,  8, 8, 0, 0,
     SEC_OID_DES_CBC,     "DES-CBC", MR_LOW},
    {cipher_3des,         calg_3des,     24,24, type_block,  8, 8, 0, 0,
     SEC_OID_DES_EDE3_CBC, "3DES-EDE-CBC", MR_LOW},
    {cipher_aes_128,      calg_aes,      16,16, type_block, 16,16, 0, 0,
     SEC_OID_AES_128_CBC, "AES-128", MR_128},
    {cipher_aes_256,      calg_aes,      32,32, type_block, 16,16, 0, 0,
     SEC_OID_AES_256_CBC, "AES-256", MR_128},
    {cipher_camellia_128, calg_camellia, 16,16, type_block, 16,16, 0, 0,
     SEC_OID_CAMELLIA_128_CBC, "Camellia-128", MR_128},
    {cipher_camellia_256, calg_camellia, 32,32, type_block, 16,16, 0, 0,
     SEC_OID_CAMELLIA_256_CBC, "Camellia-256", MR_128},
    {cipher_seed,         calg_seed,     16,16, type_block, 16,16, 0, 0,
     SEC_OID_SEED_CBC,    "SEED-CBC", MR_128},
    {cipher_aes_128_gcm,  calg_aes_gcm,  16,16, type_aead,   4, 0,16, 8,
     SEC_OID_AES_128_GCM, "AES-128-GCM", MR_128},
    {cipher_aes_256_gcm,  calg_aes_gcm,  32,32, type_aead,   4, 0,16, 8,
     SEC_OID_AES_256_GCM, "AES-256-GCM", MR_128},
    {cipher_chacha20,     calg_chacha20, 32,32, type_aead,  12, 0,16, 0,
     SEC_OID_CHACHA20_POLY1305, "ChaCha20-Poly1305", MR_MAX},
    {cipher_missing,      calg_null,      0, 0, type_stream, 0, 0, 0, 0,
     SEC_OID_UNKNOWN,     "missing", 0U},
};

static const ssl3KEADef kea_defs[] =
{ /* indexed by SSL3KeyExchangeAlgorithm */
    /* kea            exchKeyType signKeyType authKeyType ephemeral  oid */
    {kea_null,           ssl_kea_null, nullKey, ssl_auth_null, PR_FALSE, 0},
    {kea_rsa,            ssl_kea_rsa,  nullKey, ssl_auth_rsa_decrypt, PR_FALSE, SEC_OID_TLS_RSA},
    {kea_dh_dss,         ssl_kea_dh,   dsaKey, ssl_auth_dsa, PR_FALSE, SEC_OID_TLS_DH_DSS},
    {kea_dh_rsa,         ssl_kea_dh,   rsaKey, ssl_auth_rsa_sign, PR_FALSE, SEC_OID_TLS_DH_RSA},
    {kea_dhe_dss,        ssl_kea_dh,   dsaKey, ssl_auth_dsa, PR_TRUE,  SEC_OID_TLS_DHE_DSS},
    {kea_dhe_rsa,        ssl_kea_dh,   rsaKey, ssl_auth_rsa_sign, PR_TRUE,  SEC_OID_TLS_DHE_RSA},
    {kea_dh_anon,        ssl_kea_dh,   nullKey, ssl_auth_null, PR_TRUE,  SEC_OID_TLS_DH_ANON},
    {kea_ecdh_ecdsa,     ssl_kea_ecdh, nullKey, ssl_auth_ecdh_ecdsa, PR_FALSE, SEC_OID_TLS_ECDH_ECDSA},
    {kea_ecdhe_ecdsa,    ssl_kea_ecdh, ecKey, ssl_auth_ecdsa, PR_TRUE,  SEC_OID_TLS_ECDHE_ECDSA},
    {kea_ecdh_rsa,       ssl_kea_ecdh, nullKey, ssl_auth_ecdh_rsa, PR_FALSE, SEC_OID_TLS_ECDH_RSA},
    {kea_ecdhe_rsa,      ssl_kea_ecdh, rsaKey, ssl_auth_rsa_sign, PR_TRUE,  SEC_OID_TLS_ECDHE_RSA},
    {kea_ecdh_anon,      ssl_kea_ecdh, nullKey, ssl_auth_null, PR_TRUE,  SEC_OID_TLS_ECDH_ANON},
    {kea_ecdhe_psk,      ssl_kea_ecdh_psk, nullKey, ssl_auth_psk, PR_TRUE, SEC_OID_TLS_ECDHE_PSK},
    {kea_dhe_psk,      ssl_kea_dh_psk, nullKey, ssl_auth_psk, PR_TRUE, SEC_OID_TLS_DHE_PSK},
    {kea_tls13_any,      ssl_kea_tls13_any, nullKey, ssl_auth_tls13_any, PR_TRUE, SEC_OID_TLS13_KEA_ANY},
};

/* must use ssl_LookupCipherSuiteDef to access */
static const ssl3CipherSuiteDef cipher_suite_defs[] =
{
/*  cipher_suite                    bulk_cipher_alg mac_alg key_exchange_alg prf_hash */
/*  Note that the prf_hash_alg is the hash function used by the PRF, see sslimpl.h.  */

    {TLS_NULL_WITH_NULL_NULL,       cipher_null,   mac_null, kea_null, ssl_hash_none},
    {TLS_RSA_WITH_NULL_MD5,         cipher_null,   mac_md5, kea_rsa, ssl_hash_none},
    {TLS_RSA_WITH_NULL_SHA,         cipher_null,   mac_sha, kea_rsa, ssl_hash_none},
    {TLS_RSA_WITH_NULL_SHA256,      cipher_null,   hmac_sha256, kea_rsa, ssl_hash_sha256},
    {TLS_RSA_WITH_RC4_128_MD5,      cipher_rc4,    mac_md5, kea_rsa, ssl_hash_none},
    {TLS_RSA_WITH_RC4_128_SHA,      cipher_rc4,    mac_sha, kea_rsa, ssl_hash_none},
    {TLS_RSA_WITH_DES_CBC_SHA,      cipher_des,    mac_sha, kea_rsa, ssl_hash_none},
    {TLS_RSA_WITH_3DES_EDE_CBC_SHA, cipher_3des,   mac_sha, kea_rsa, ssl_hash_none},
    {TLS_DHE_DSS_WITH_DES_CBC_SHA,  cipher_des,    mac_sha, kea_dhe_dss, ssl_hash_none},
    {TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
                                    cipher_3des,   mac_sha, kea_dhe_dss, ssl_hash_none},
    {TLS_DHE_DSS_WITH_RC4_128_SHA,  cipher_rc4,    mac_sha, kea_dhe_dss, ssl_hash_none},
    {TLS_DHE_RSA_WITH_DES_CBC_SHA,  cipher_des,    mac_sha, kea_dhe_rsa, ssl_hash_none},
    {TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
                                    cipher_3des,   mac_sha, kea_dhe_rsa, ssl_hash_none},


/* New TLS cipher suites */
    {TLS_RSA_WITH_AES_128_CBC_SHA,      cipher_aes_128, mac_sha, kea_rsa, ssl_hash_none},
    {TLS_RSA_WITH_AES_128_CBC_SHA256,   cipher_aes_128, hmac_sha256, kea_rsa, ssl_hash_sha256},
    {TLS_DHE_DSS_WITH_AES_128_CBC_SHA,  cipher_aes_128, mac_sha, kea_dhe_dss, ssl_hash_none},
    {TLS_DHE_RSA_WITH_AES_128_CBC_SHA,  cipher_aes_128, mac_sha, kea_dhe_rsa, ssl_hash_none},
    {TLS_DHE_RSA_WITH_AES_128_CBC_SHA256, cipher_aes_128, hmac_sha256, kea_dhe_rsa, ssl_hash_sha256},
    {TLS_RSA_WITH_AES_256_CBC_SHA,      cipher_aes_256, mac_sha, kea_rsa, ssl_hash_none},
    {TLS_RSA_WITH_AES_256_CBC_SHA256,   cipher_aes_256, hmac_sha256, kea_rsa, ssl_hash_sha256},
    {TLS_DHE_DSS_WITH_AES_256_CBC_SHA,  cipher_aes_256, mac_sha, kea_dhe_dss, ssl_hash_none},
    {TLS_DHE_RSA_WITH_AES_256_CBC_SHA,  cipher_aes_256, mac_sha, kea_dhe_rsa, ssl_hash_none},
    {TLS_DHE_RSA_WITH_AES_256_CBC_SHA256, cipher_aes_256, hmac_sha256, kea_dhe_rsa, ssl_hash_sha256},
    {TLS_DHE_RSA_WITH_AES_256_GCM_SHA384, cipher_aes_256_gcm, mac_aead, kea_dhe_rsa, ssl_hash_sha384},

    {TLS_RSA_WITH_SEED_CBC_SHA,     cipher_seed,   mac_sha, kea_rsa, ssl_hash_none},

    {TLS_RSA_WITH_CAMELLIA_128_CBC_SHA, cipher_camellia_128, mac_sha, kea_rsa, ssl_hash_none},
    {TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
     cipher_camellia_128, mac_sha, kea_dhe_dss, ssl_hash_none},
    {TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
     cipher_camellia_128, mac_sha, kea_dhe_rsa, ssl_hash_none},
    {TLS_RSA_WITH_CAMELLIA_256_CBC_SHA, cipher_camellia_256, mac_sha, kea_rsa, ssl_hash_none},
    {TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
     cipher_camellia_256, mac_sha, kea_dhe_dss, ssl_hash_none},
    {TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
     cipher_camellia_256, mac_sha, kea_dhe_rsa, ssl_hash_none},

    {TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, cipher_aes_128_gcm, mac_aead, kea_dhe_rsa, ssl_hash_sha256},
    {TLS_RSA_WITH_AES_128_GCM_SHA256, cipher_aes_128_gcm, mac_aead, kea_rsa, ssl_hash_sha256},

    {TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, cipher_aes_128_gcm, mac_aead, kea_ecdhe_rsa, ssl_hash_sha256},
    {TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, cipher_aes_128_gcm, mac_aead, kea_ecdhe_ecdsa, ssl_hash_sha256},
    {TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, cipher_aes_256_gcm, mac_aead, kea_ecdhe_ecdsa, ssl_hash_sha384},
    {TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, cipher_aes_256_gcm, mac_aead, kea_ecdhe_rsa, ssl_hash_sha384},
    {TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, cipher_aes_256, hmac_sha384, kea_ecdhe_ecdsa, ssl_hash_sha384},
    {TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384, cipher_aes_256, hmac_sha384, kea_ecdhe_rsa, ssl_hash_sha384},
    {TLS_DHE_DSS_WITH_AES_128_GCM_SHA256, cipher_aes_128_gcm, mac_aead, kea_dhe_dss, ssl_hash_sha256},
    {TLS_DHE_DSS_WITH_AES_128_CBC_SHA256, cipher_aes_128, hmac_sha256, kea_dhe_dss, ssl_hash_sha256},
    {TLS_DHE_DSS_WITH_AES_256_CBC_SHA256, cipher_aes_256, hmac_sha256, kea_dhe_dss, ssl_hash_sha256},
    {TLS_DHE_DSS_WITH_AES_256_GCM_SHA384, cipher_aes_256_gcm, mac_aead, kea_dhe_dss, ssl_hash_sha384},
    {TLS_RSA_WITH_AES_256_GCM_SHA384, cipher_aes_256_gcm, mac_aead, kea_rsa, ssl_hash_sha384},

    {TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256, cipher_chacha20, mac_aead, kea_dhe_rsa, ssl_hash_sha256},

    {TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, cipher_chacha20, mac_aead, kea_ecdhe_rsa, ssl_hash_sha256},
    {TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, cipher_chacha20, mac_aead, kea_ecdhe_ecdsa, ssl_hash_sha256},

    {TLS_ECDH_ECDSA_WITH_NULL_SHA,        cipher_null, mac_sha, kea_ecdh_ecdsa, ssl_hash_none},
    {TLS_ECDH_ECDSA_WITH_RC4_128_SHA,      cipher_rc4, mac_sha, kea_ecdh_ecdsa, ssl_hash_none},
    {TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA, cipher_3des, mac_sha, kea_ecdh_ecdsa, ssl_hash_none},
    {TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, cipher_aes_128, mac_sha, kea_ecdh_ecdsa, ssl_hash_none},
    {TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, cipher_aes_256, mac_sha, kea_ecdh_ecdsa, ssl_hash_none},

    {TLS_ECDHE_ECDSA_WITH_NULL_SHA,        cipher_null, mac_sha, kea_ecdhe_ecdsa, ssl_hash_none},
    {TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,      cipher_rc4, mac_sha, kea_ecdhe_ecdsa, ssl_hash_none},
    {TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, cipher_3des, mac_sha, kea_ecdhe_ecdsa, ssl_hash_none},
    {TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, cipher_aes_128, mac_sha, kea_ecdhe_ecdsa, ssl_hash_none},
    {TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, cipher_aes_128, hmac_sha256, kea_ecdhe_ecdsa, ssl_hash_sha256},
    {TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, cipher_aes_256, mac_sha, kea_ecdhe_ecdsa, ssl_hash_none},

    {TLS_ECDH_RSA_WITH_NULL_SHA,         cipher_null,    mac_sha, kea_ecdh_rsa, ssl_hash_none},
    {TLS_ECDH_RSA_WITH_RC4_128_SHA,      cipher_rc4,     mac_sha, kea_ecdh_rsa, ssl_hash_none},
    {TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA, cipher_3des,    mac_sha, kea_ecdh_rsa, ssl_hash_none},
    {TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,  cipher_aes_128, mac_sha, kea_ecdh_rsa, ssl_hash_none},
    {TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,  cipher_aes_256, mac_sha, kea_ecdh_rsa, ssl_hash_none},

    {TLS_ECDHE_RSA_WITH_NULL_SHA,         cipher_null,    mac_sha, kea_ecdhe_rsa, ssl_hash_none},
    {TLS_ECDHE_RSA_WITH_RC4_128_SHA,      cipher_rc4,     mac_sha, kea_ecdhe_rsa, ssl_hash_none},
    {TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, cipher_3des,    mac_sha, kea_ecdhe_rsa, ssl_hash_none},
    {TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,  cipher_aes_128, mac_sha, kea_ecdhe_rsa, ssl_hash_none},
    {TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, cipher_aes_128, hmac_sha256, kea_ecdhe_rsa, ssl_hash_sha256},
    {TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,  cipher_aes_256, mac_sha, kea_ecdhe_rsa, ssl_hash_none},

    {TLS_AES_128_GCM_SHA256, cipher_aes_128_gcm, mac_aead, kea_tls13_any, ssl_hash_sha256},
    {TLS_CHACHA20_POLY1305_SHA256, cipher_chacha20, mac_aead, kea_tls13_any, ssl_hash_sha256},
    {TLS_AES_256_GCM_SHA384, cipher_aes_256_gcm, mac_aead, kea_tls13_any, ssl_hash_sha384},
};
/* clang-format on */

static const CK_MECHANISM_TYPE auth_alg_defs[] = {
    CKM_INVALID_MECHANISM, /* ssl_auth_null */
    CKM_RSA_PKCS,          /* ssl_auth_rsa_decrypt */
    CKM_DSA, /* ? _SHA1 */ /* ssl_auth_dsa */
    CKM_INVALID_MECHANISM, /* ssl_auth_kea (unused) */
    CKM_ECDSA,             /* ssl_auth_ecdsa */
    CKM_ECDH1_DERIVE,      /* ssl_auth_ecdh_rsa */
    CKM_ECDH1_DERIVE,      /* ssl_auth_ecdh_ecdsa */
    CKM_RSA_PKCS,          /* ssl_auth_rsa_sign */
    CKM_RSA_PKCS_PSS,      /* ssl_auth_rsa_pss */
    CKM_NSS_HKDF_SHA256,   /* ssl_auth_psk (just check for HKDF) */
    CKM_INVALID_MECHANISM  /* ssl_auth_tls13_any */
};
PR_STATIC_ASSERT(PR_ARRAY_SIZE(auth_alg_defs) == ssl_auth_size);

static const CK_MECHANISM_TYPE kea_alg_defs[] = {
    CKM_INVALID_MECHANISM, /* ssl_kea_null */
    CKM_RSA_PKCS,          /* ssl_kea_rsa */
    CKM_DH_PKCS_DERIVE,    /* ssl_kea_dh */
    CKM_INVALID_MECHANISM, /* ssl_kea_fortezza (unused) */
    CKM_ECDH1_DERIVE,      /* ssl_kea_ecdh */
    CKM_ECDH1_DERIVE,      /* ssl_kea_ecdh_psk */
    CKM_DH_PKCS_DERIVE,    /* ssl_kea_dh_psk */
    CKM_INVALID_MECHANISM, /* ssl_kea_tls13_any */
};
PR_STATIC_ASSERT(PR_ARRAY_SIZE(kea_alg_defs) == ssl_kea_size);

typedef struct SSLCipher2MechStr {
    SSLCipherAlgorithm calg;
    CK_MECHANISM_TYPE cmech;
} SSLCipher2Mech;

/* indexed by type SSLCipherAlgorithm */
static const SSLCipher2Mech alg2Mech[] = {
    /* calg,          cmech  */
    { calg_null, (CK_MECHANISM_TYPE)0x80000000L },
    { calg_rc4, CKM_RC4 },
    { calg_rc2, CKM_RC2_CBC },
    { calg_des, CKM_DES_CBC },
    { calg_3des, CKM_DES3_CBC },
    { calg_idea, CKM_IDEA_CBC },
    { calg_fortezza, CKM_SKIPJACK_CBC64 },
    { calg_aes, CKM_AES_CBC },
    { calg_camellia, CKM_CAMELLIA_CBC },
    { calg_seed, CKM_SEED_CBC },
    { calg_aes_gcm, CKM_AES_GCM },
    { calg_chacha20, CKM_NSS_CHACHA20_POLY1305 },
    /*  { calg_init     , (CK_MECHANISM_TYPE)0x7fffffffL    }  */
};

#define mmech_invalid (CK_MECHANISM_TYPE)0x80000000L
#define mmech_md5 CKM_SSL3_MD5_MAC
#define mmech_sha CKM_SSL3_SHA1_MAC
#define mmech_md5_hmac CKM_MD5_HMAC
#define mmech_sha_hmac CKM_SHA_1_HMAC
#define mmech_sha256_hmac CKM_SHA256_HMAC
#define mmech_sha384_hmac CKM_SHA384_HMAC

/* clang-format off */
static const ssl3MACDef mac_defs[] = { /* indexed by SSL3MACAlgorithm */
    /* pad_size is only used for SSL 3.0 MAC. See RFC 6101 Sec. 5.2.3.1. */
    /* mac      mmech       pad_size  mac_size                       */
    { mac_null, mmech_invalid,    0,  0         ,  0},
    { mac_md5,  mmech_md5,       48,  MD5_LENGTH,  SEC_OID_HMAC_MD5 },
    { mac_sha,  mmech_sha,       40,  SHA1_LENGTH, SEC_OID_HMAC_SHA1},
    {hmac_md5,  mmech_md5_hmac,   0,  MD5_LENGTH,  SEC_OID_HMAC_MD5},
    {hmac_sha,  mmech_sha_hmac,   0,  SHA1_LENGTH, SEC_OID_HMAC_SHA1},
    {hmac_sha256, mmech_sha256_hmac, 0, SHA256_LENGTH, SEC_OID_HMAC_SHA256},
    { mac_aead, mmech_invalid,    0,  0, 0 },
    {hmac_sha384, mmech_sha384_hmac, 0, SHA384_LENGTH, SEC_OID_HMAC_SHA384}
};
/* clang-format on */

const PRUint8 tls13_downgrade_random[] = { 0x44, 0x4F, 0x57, 0x4E,
                                           0x47, 0x52, 0x44, 0x01 };
const PRUint8 tls12_downgrade_random[] = { 0x44, 0x4F, 0x57, 0x4E,
                                           0x47, 0x52, 0x44, 0x00 };
PR_STATIC_ASSERT(sizeof(tls13_downgrade_random) ==
                 sizeof(tls13_downgrade_random));

/* The ECCWrappedKeyInfo structure defines how various pieces of
 * information are laid out within wrappedSymmetricWrappingkey
 * for ECDH key exchange. Since wrappedSymmetricWrappingkey is
 * a 512-byte buffer (see sslimpl.h), the variable length field
 * in ECCWrappedKeyInfo can be at most (512 - 8) = 504 bytes.
 *
 * XXX For now, NSS only supports named elliptic curves of size 571 bits
 * or smaller. The public value will fit within 145 bytes and EC params
 * will fit within 12 bytes. We'll need to revisit this when NSS
 * supports arbitrary curves.
 */
#define MAX_EC_WRAPPED_KEY_BUFLEN 504

typedef struct ECCWrappedKeyInfoStr {
    PRUint16 size;                          /* EC public key size in bits */
    PRUint16 encodedParamLen;               /* length (in bytes) of DER encoded EC params */
    PRUint16 pubValueLen;                   /* length (in bytes) of EC public value */
    PRUint16 wrappedKeyLen;                 /* length (in bytes) of the wrapped key */
    PRUint8 var[MAX_EC_WRAPPED_KEY_BUFLEN]; /* this buffer contains the */
    /* EC public-key params, the EC public value and the wrapped key  */
} ECCWrappedKeyInfo;

CK_MECHANISM_TYPE
ssl3_Alg2Mech(SSLCipherAlgorithm calg)
{
    PORT_Assert(alg2Mech[calg].calg == calg);
    return alg2Mech[calg].cmech;
}

#if defined(TRACE)

static char *
ssl3_DecodeHandshakeType(int msgType)
{
    char *rv;
    static char line[40];

    switch (msgType) {
        case hello_request:
            rv = "hello_request (0)";
            break;
        case client_hello:
            rv = "client_hello  (1)";
            break;
        case server_hello:
            rv = "server_hello  (2)";
            break;
        case hello_verify_request:
            rv = "hello_verify_request (3)";
            break;
        case new_session_ticket:
            rv = "session_ticket (4)";
            break;
        case hello_retry_request:
            rv = "hello_retry_request (6)";
            break;
        case encrypted_extensions:
            rv = "encrypted_extensions (8)";
            break;
        case certificate:
            rv = "certificate  (11)";
            break;
        case server_key_exchange:
            rv = "server_key_exchange (12)";
            break;
        case certificate_request:
            rv = "certificate_request (13)";
            break;
        case server_hello_done:
            rv = "server_hello_done   (14)";
            break;
        case certificate_verify:
            rv = "certificate_verify  (15)";
            break;
        case client_key_exchange:
            rv = "client_key_exchange (16)";
            break;
        case finished:
            rv = "finished     (20)";
            break;
        default:
            sprintf(line, "*UNKNOWN* handshake type! (%d)", msgType);
            rv = line;
    }
    return rv;
}

static char *
ssl3_DecodeContentType(int msgType)
{
    char *rv;
    static char line[40];

    switch (msgType) {
        case content_change_cipher_spec:
            rv = "change_cipher_spec (20)";
            break;
        case content_alert:
            rv = "alert      (21)";
            break;
        case content_handshake:
            rv = "handshake  (22)";
            break;
        case content_application_data:
            rv = "application_data (23)";
            break;
        default:
            sprintf(line, "*UNKNOWN* record type! (%d)", msgType);
            rv = line;
    }
    return rv;
}

#endif

SSL3Statistics *
SSL_GetStatistics(void)
{
    return &ssl3stats;
}

typedef struct tooLongStr {
#if defined(IS_LITTLE_ENDIAN)
    PRInt32 low;
    PRInt32 high;
#else
    PRInt32 high;
    PRInt32 low;
#endif
} tooLong;

void
SSL_AtomicIncrementLong(long *x)
{
    if ((sizeof *x) == sizeof(PRInt32)) {
        PR_ATOMIC_INCREMENT((PRInt32 *)x);
    } else {
        tooLong *tl = (tooLong *)x;
        if (PR_ATOMIC_INCREMENT(&tl->low) == 0)
            PR_ATOMIC_INCREMENT(&tl->high);
    }
}

static PRBool
ssl3_CipherSuiteAllowedForVersionRange(
    ssl3CipherSuite cipherSuite,
    const SSLVersionRange *vrange)
{
    switch (cipherSuite) {
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_NULL_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
            return vrange->max >= SSL_LIBRARY_VERSION_TLS_1_2 &&
                   vrange->min < SSL_LIBRARY_VERSION_TLS_1_3;

        /* RFC 4492: ECC cipher suites need TLS extensions to negotiate curves and
         * point formats.*/
        case TLS_ECDH_ECDSA_WITH_NULL_SHA:
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_RSA_WITH_NULL_SHA:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_NULL_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
            return vrange->max >= SSL_LIBRARY_VERSION_TLS_1_0 &&
                   vrange->min < SSL_LIBRARY_VERSION_TLS_1_3;

        case TLS_AES_128_GCM_SHA256:
        case TLS_AES_256_GCM_SHA384:
        case TLS_CHACHA20_POLY1305_SHA256:
            return vrange->max >= SSL_LIBRARY_VERSION_TLS_1_3;

        default:
            return vrange->min < SSL_LIBRARY_VERSION_TLS_1_3;
    }
}

/* return pointer to ssl3CipherSuiteDef for suite, or NULL */
/* XXX This does a linear search.  A binary search would be better. */
const ssl3CipherSuiteDef *
ssl_LookupCipherSuiteDef(ssl3CipherSuite suite)
{
    int cipher_suite_def_len =
        sizeof(cipher_suite_defs) / sizeof(cipher_suite_defs[0]);
    int i;

    for (i = 0; i < cipher_suite_def_len; i++) {
        if (cipher_suite_defs[i].cipher_suite == suite)
            return &cipher_suite_defs[i];
    }
    PORT_Assert(PR_FALSE); /* We should never get here. */
    PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
    return NULL;
}

/* Find the cipher configuration struct associate with suite */
/* XXX This does a linear search.  A binary search would be better. */
static ssl3CipherSuiteCfg *
ssl_LookupCipherSuiteCfgMutable(ssl3CipherSuite suite,
                                ssl3CipherSuiteCfg *suites)
{
    int i;

    for (i = 0; i < ssl_V3_SUITES_IMPLEMENTED; i++) {
        if (suites[i].cipher_suite == suite)
            return &suites[i];
    }
    /* return NULL and let the caller handle it.  */
    PORT_SetError(SSL_ERROR_UNKNOWN_CIPHER_SUITE);
    return NULL;
}

const static ssl3CipherSuiteCfg *
ssl_LookupCipherSuiteCfg(ssl3CipherSuite suite, const ssl3CipherSuiteCfg *suites)
{
    return ssl_LookupCipherSuiteCfgMutable(suite,
                                           CONST_CAST(ssl3CipherSuiteCfg, suites));
}

static PRBool
ssl_NamedGroupTypeEnabled(const sslSocket *ss, SSLKEAType keaType)
{
    unsigned int i;
    for (i = 0; i < SSL_NAMED_GROUP_COUNT; ++i) {
        if (ss->namedGroupPreferences[i] &&
            ss->namedGroupPreferences[i]->keaType == keaType) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

static PRBool
ssl_KEAEnabled(const sslSocket *ss, SSLKEAType keaType)
{
    switch (keaType) {
        case ssl_kea_rsa:
            return PR_TRUE;

        case ssl_kea_dh:
        case ssl_kea_dh_psk: {
            if (ss->sec.isServer && !ss->opt.enableServerDhe) {
                return PR_FALSE;
            }

            if (ss->sec.isServer) {
                /* If the server requires named FFDHE groups, then the client
                 * must have included an FFDHE group. peerSupportsFfdheGroups
                 * is set to true in ssl_HandleSupportedGroupsXtn(). */
                if (ss->opt.requireDHENamedGroups &&
                    !ss->xtnData.peerSupportsFfdheGroups) {
                    return PR_FALSE;
                }

                /* We can use the weak DH group if all of these are true:
                 * 1. We don't require named groups.
                 * 2. The peer doesn't support named groups.
                 * 3. This isn't TLS 1.3.
                 * 4. The weak group is enabled. */
                if (!ss->opt.requireDHENamedGroups &&
                    !ss->xtnData.peerSupportsFfdheGroups &&
                    ss->version < SSL_LIBRARY_VERSION_TLS_1_3 &&
                    ss->ssl3.dheWeakGroupEnabled) {
                    return PR_TRUE;
                }
            } else {
                if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3 &&
                    !ss->opt.requireDHENamedGroups) {
                    /* The client enables DHE cipher suites even if no DHE groups
                     * are enabled. Only if this isn't TLS 1.3 and named groups
                     * are not required. */
                    return PR_TRUE;
                }
            }
            return ssl_NamedGroupTypeEnabled(ss, ssl_kea_dh);
        }

        case ssl_kea_ecdh:
        case ssl_kea_ecdh_psk:
            return ssl_NamedGroupTypeEnabled(ss, ssl_kea_ecdh);

        case ssl_kea_tls13_any:
            return PR_TRUE;

        case ssl_kea_fortezza:
        default:
            PORT_Assert(0);
    }
    return PR_FALSE;
}

static PRBool
ssl_HasCert(const sslSocket *ss, SSLAuthType authType)
{
    PRCList *cursor;
    if (authType == ssl_auth_null || authType == ssl_auth_psk || authType == ssl_auth_tls13_any) {
        return PR_TRUE;
    }
    for (cursor = PR_NEXT_LINK(&ss->serverCerts);
         cursor != &ss->serverCerts;
         cursor = PR_NEXT_LINK(cursor)) {
        sslServerCert *cert = (sslServerCert *)cursor;
        if (cert->certType.authType != authType) {
            continue;
        }
        if (!cert->serverKeyPair ||
            !cert->serverKeyPair->privKey ||
            !cert->serverCertChain) {
            continue;
        }
        /* When called from ssl3_config_match_init(), all the EC curves will be
         * enabled, so this will essentially do nothing (unless we implement
         * curve configuration).  However, once we have seen the
         * supported_groups extension and this is called from config_match(),
         * this will filter out certificates with an unsupported curve. */
        if ((authType == ssl_auth_ecdsa ||
             authType == ssl_auth_ecdh_ecdsa ||
             authType == ssl_auth_ecdh_rsa) &&
            !ssl_NamedGroupEnabled(ss, cert->certType.namedCurve)) {
            continue;
        }
        return PR_TRUE;
    }
    return PR_FALSE;
}

const ssl3BulkCipherDef *
ssl_GetBulkCipherDef(const ssl3CipherSuiteDef *cipher_def)
{
    PORT_Assert(cipher_def->bulk_cipher_alg < PR_ARRAY_SIZE(bulk_cipher_defs));
    PORT_Assert(bulk_cipher_defs[cipher_def->bulk_cipher_alg].cipher == cipher_def->bulk_cipher_alg);
    return &bulk_cipher_defs[cipher_def->bulk_cipher_alg];
}

/* Initialize the suite->isPresent value for config_match
 * Returns count of enabled ciphers supported by extant tokens,
 * regardless of policy or user preference.
 * If this returns zero, the user cannot do SSL v3.
 */
int
ssl3_config_match_init(sslSocket *ss)
{
    ssl3CipherSuiteCfg *suite;
    const ssl3CipherSuiteDef *cipher_def;
    SSLCipherAlgorithm cipher_alg;
    CK_MECHANISM_TYPE cipher_mech;
    SSLAuthType authType;
    SSLKEAType keaType;
    int i;
    int numPresent = 0;
    int numEnabled = 0;

    PORT_Assert(ss);
    if (!ss) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return 0;
    }
    if (SSL_ALL_VERSIONS_DISABLED(&ss->vrange)) {
        return 0;
    }

    for (i = 0; i < ssl_V3_SUITES_IMPLEMENTED; i++) {
        suite = &ss->cipherSuites[i];
        if (suite->enabled) {
            ++numEnabled;
            /* We need the cipher defs to see if we have a token that can handle
             * this cipher.  It isn't part of the static definition.
             */
            cipher_def = ssl_LookupCipherSuiteDef(suite->cipher_suite);
            if (!cipher_def) {
                suite->isPresent = PR_FALSE;
                continue;
            }
            cipher_alg = ssl_GetBulkCipherDef(cipher_def)->calg;
            cipher_mech = ssl3_Alg2Mech(cipher_alg);

            /* Mark the suites that are backed by real tokens, certs and keys */
            suite->isPresent = PR_TRUE;

            authType = kea_defs[cipher_def->key_exchange_alg].authKeyType;
            if (authType != ssl_auth_null && authType != ssl_auth_tls13_any) {
                if (ss->sec.isServer && !ssl_HasCert(ss, authType)) {
                    suite->isPresent = PR_FALSE;
                }
                if (!PK11_TokenExists(auth_alg_defs[authType])) {
                    suite->isPresent = PR_FALSE;
                }
            }

            keaType = kea_defs[cipher_def->key_exchange_alg].exchKeyType;
            if (keaType != ssl_kea_null &&
                keaType != ssl_kea_tls13_any &&
                !PK11_TokenExists(kea_alg_defs[keaType])) {
                suite->isPresent = PR_FALSE;
            }

            if (cipher_alg != calg_null &&
                !PK11_TokenExists(cipher_mech)) {
                suite->isPresent = PR_FALSE;
            }

            if (suite->isPresent) {
                ++numPresent;
            }
        }
    }
    PORT_Assert(numPresent > 0 || numEnabled == 0);
    if (numPresent <= 0) {
        PORT_SetError(SSL_ERROR_NO_CIPHERS_SUPPORTED);
    }
    return numPresent;
}

/* Return PR_TRUE if suite is usable.  This if the suite is permitted by policy,
 * enabled, has a certificate (as needed), has a viable key agreement method, is
 * usable with the negotiated TLS version, and is otherwise usable. */
static PRBool
config_match(const ssl3CipherSuiteCfg *suite, int policy,
             const SSLVersionRange *vrange, const sslSocket *ss)
{
    const ssl3CipherSuiteDef *cipher_def;
    const ssl3KEADef *kea_def;

    PORT_Assert(policy != SSL_NOT_ALLOWED);
    if (policy == SSL_NOT_ALLOWED)
        return PR_FALSE;

    if (!suite->enabled || !suite->isPresent)
        return PR_FALSE;

    if ((suite->policy == SSL_NOT_ALLOWED) ||
        (suite->policy > policy))
        return PR_FALSE;

    PORT_Assert(ss != NULL);
    cipher_def = ssl_LookupCipherSuiteDef(suite->cipher_suite);
    PORT_Assert(cipher_def != NULL);
    kea_def = &kea_defs[cipher_def->key_exchange_alg];
    PORT_Assert(kea_def != NULL);
    if (!ssl_KEAEnabled(ss, kea_def->exchKeyType)) {
        return PR_FALSE;
    }

    if (ss->sec.isServer && !ssl_HasCert(ss, kea_def->authKeyType)) {
        return PR_FALSE;
    }

    return ssl3_CipherSuiteAllowedForVersionRange(suite->cipher_suite, vrange);
}

/* Return the number of cipher suites that are usable. */
/* called from ssl3_SendClientHello */
static int
count_cipher_suites(sslSocket *ss, int policy)
{
    int i, count = 0;

    if (SSL_ALL_VERSIONS_DISABLED(&ss->vrange)) {
        return 0;
    }
    for (i = 0; i < ssl_V3_SUITES_IMPLEMENTED; i++) {
        if (config_match(&ss->cipherSuites[i], policy, &ss->vrange, ss))
            count++;
    }
    if (count <= 0) {
        PORT_SetError(SSL_ERROR_SSL_DISABLED);
    }
    return count;
}

/*
 * Null compression, mac and encryption functions
 */
static SECStatus
Null_Cipher(void *ctx, unsigned char *output, int *outputLen, int maxOutputLen,
            const unsigned char *input, int inputLen)
{
    if (inputLen > maxOutputLen) {
        *outputLen = 0; /* Match PK11_CipherOp in setting outputLen */
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    *outputLen = inputLen;
    if (input != output)
        PORT_Memcpy(output, input, inputLen);
    return SECSuccess;
}

/*
 * SSL3 Utility functions
 */

/* allowLargerPeerVersion controls whether the function will select the
 * highest enabled SSL version or fail when peerVersion is greater than the
 * highest enabled version.
 *
 * If allowLargerPeerVersion is true, peerVersion is the peer's highest
 * enabled version rather than the peer's selected version.
 */
SECStatus
ssl3_NegotiateVersion(sslSocket *ss, SSL3ProtocolVersion peerVersion,
                      PRBool allowLargerPeerVersion)
{
    if (SSL_ALL_VERSIONS_DISABLED(&ss->vrange)) {
        PORT_SetError(SSL_ERROR_SSL_DISABLED);
        return SECFailure;
    }

    if (peerVersion < ss->vrange.min ||
        (peerVersion > ss->vrange.max && !allowLargerPeerVersion)) {
        PORT_SetError(SSL_ERROR_UNSUPPORTED_VERSION);
        return SECFailure;
    }

    ss->version = PR_MIN(peerVersion, ss->vrange.max);
    PORT_Assert(ssl3_VersionIsSupported(ss->protocolVariant, ss->version));

    return SECSuccess;
}

/* Used by the client when the server produces a version number.
 * This reads, validates, and normalizes the value. */
SECStatus
ssl_ClientReadVersion(sslSocket *ss, SSL3Opaque **b, unsigned int *len,
                      SSL3ProtocolVersion *version)
{
    SSL3ProtocolVersion v;
    PRInt32 temp;

    temp = ssl3_ConsumeHandshakeNumber(ss, 2, b, len);
    if (temp < 0) {
        return SECFailure; /* alert has been sent */
    }

#ifdef TLS_1_3_DRAFT_VERSION
    if (temp == SSL_LIBRARY_VERSION_TLS_1_3) {
        (void)SSL3_SendAlert(ss, alert_fatal, protocol_version);
        PORT_SetError(SSL_ERROR_UNSUPPORTED_VERSION);
        return SECFailure;
    }
    if (temp == tls13_EncodeDraftVersion(SSL_LIBRARY_VERSION_TLS_1_3)) {
        v = SSL_LIBRARY_VERSION_TLS_1_3;
    } else {
        v = (SSL3ProtocolVersion)temp;
    }
#else
    v = (SSL3ProtocolVersion)temp;
#endif

    if (IS_DTLS(ss)) {
        /* If this fails, we get 0 back and the next check to fails. */
        v = dtls_DTLSVersionToTLSVersion(v);
    }

    PORT_Assert(!SSL_ALL_VERSIONS_DISABLED(&ss->vrange));
    if (ss->vrange.min > v || ss->vrange.max < v) {
        (void)SSL3_SendAlert(ss, alert_fatal,
                             (v > SSL_LIBRARY_VERSION_3_0) ? protocol_version
                                                           : handshake_failure);
        PORT_SetError(SSL_ERROR_UNSUPPORTED_VERSION);
        return SECFailure;
    }
    *version = v;
    return SECSuccess;
}

static SECStatus
ssl3_GetNewRandom(SSL3Random *random)
{
    SECStatus rv;

    rv = PK11_GenerateRandom(random->rand, SSL3_RANDOM_LENGTH);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_GENERATE_RANDOM_FAILURE);
    }
    return rv;
}

/* Called by ssl3_SendServerKeyExchange and ssl3_SendCertificateVerify */
SECStatus
ssl3_SignHashes(sslSocket *ss, SSL3Hashes *hash, SECKEYPrivateKey *key,
                SECItem *buf)
{
    SECStatus rv = SECFailure;
    PRBool doDerEncode = PR_FALSE;
    PRBool isTLS = (PRBool)(ss->ssl3.pwSpec->version > SSL_LIBRARY_VERSION_3_0);
    PRBool useRsaPss = ssl_IsRsaPssSignatureScheme(ss->ssl3.hs.signatureScheme);
    SECItem hashItem;

    buf->data = NULL;

    switch (SECKEY_GetPrivateKeyType(key)) {
        case rsaKey:
            hashItem.data = hash->u.raw;
            hashItem.len = hash->len;
            break;
        case dsaKey:
            doDerEncode = isTLS;
            /* ssl_hash_none is used to specify the MD5/SHA1 concatenated hash.
             * In that case, we use just the SHA1 part. */
            if (hash->hashAlg == ssl_hash_none) {
                hashItem.data = hash->u.s.sha;
                hashItem.len = sizeof(hash->u.s.sha);
            } else {
                hashItem.data = hash->u.raw;
                hashItem.len = hash->len;
            }
            break;
        case ecKey:
            doDerEncode = PR_TRUE;
            /* ssl_hash_none is used to specify the MD5/SHA1 concatenated hash.
             * In that case, we use just the SHA1 part. */
            if (hash->hashAlg == ssl_hash_none) {
                hashItem.data = hash->u.s.sha;
                hashItem.len = sizeof(hash->u.s.sha);
            } else {
                hashItem.data = hash->u.raw;
                hashItem.len = hash->len;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            goto done;
    }
    PRINT_BUF(60, (NULL, "hash(es) to be signed", hashItem.data, hashItem.len));

    if (useRsaPss || hash->hashAlg == ssl_hash_none) {
        CK_MECHANISM_TYPE mech = PK11_MapSignKeyType(key->keyType);
        int signatureLen = PK11_SignatureLen(key);

        SECItem *params = NULL;
        CK_RSA_PKCS_PSS_PARAMS pssParams;
        SECItem pssParamsItem = { siBuffer,
                                  (unsigned char *)&pssParams,
                                  sizeof(pssParams) };

        if (signatureLen <= 0) {
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            goto done;
        }

        buf->len = (unsigned)signatureLen;
        buf->data = (unsigned char *)PORT_Alloc(signatureLen);
        if (!buf->data)
            goto done; /* error code was set. */

        if (useRsaPss) {
            pssParams.hashAlg = ssl3_GetHashMechanismByHashType(hash->hashAlg);
            pssParams.mgf = ssl3_GetMgfMechanismByHashType(hash->hashAlg);
            pssParams.sLen = hashItem.len;
            params = &pssParamsItem;
            mech = CKM_RSA_PKCS_PSS;
        }

        rv = PK11_SignWithMechanism(key, mech, params, buf, &hashItem);
    } else {
        SECOidTag hashOID = ssl3_HashTypeToOID(hash->hashAlg);
        rv = SGN_Digest(key, hashOID, buf, &hashItem);
    }
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_SIGN_HASHES_FAILURE);
    } else if (doDerEncode) {
        SECItem derSig = { siBuffer, NULL, 0 };

        /* This also works for an ECDSA signature */
        rv = DSAU_EncodeDerSigWithLen(&derSig, buf, buf->len);
        if (rv == SECSuccess) {
            PORT_Free(buf->data); /* discard unencoded signature. */
            *buf = derSig;        /* give caller encoded signature. */
        } else if (derSig.data) {
            PORT_Free(derSig.data);
        }
    }

    if (ss->sec.isServer) {
        ss->sec.signatureScheme = ss->ssl3.hs.signatureScheme;
    }
    PRINT_BUF(60, (NULL, "signed hashes", (unsigned char *)buf->data, buf->len));
done:
    if (rv != SECSuccess && buf->data) {
        PORT_Free(buf->data);
        buf->data = NULL;
    }
    return rv;
}

/* Called from ssl3_HandleServerKeyExchange, ssl3_HandleCertificateVerify */
SECStatus
ssl3_VerifySignedHashes(sslSocket *ss, SSLSignatureScheme scheme, SSL3Hashes *hash,
                        SECItem *buf)
{
    SECKEYPublicKey *key;
    SECItem *signature = NULL;
    SECStatus rv = SECFailure;
    SECItem hashItem;
    SECOidTag encAlg;
    SECOidTag hashAlg;
    void *pwArg = ss->pkcs11PinArg;
    PRBool isRsaPssScheme = ssl_IsRsaPssSignatureScheme(scheme);

    PRINT_BUF(60, (NULL, "check signed hashes",
                   buf->data, buf->len));

    key = CERT_ExtractPublicKey(ss->sec.peerCert);
    if (key == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE);
        return SECFailure;
    }

    hashAlg = ssl3_HashTypeToOID(hash->hashAlg);
    switch (SECKEY_GetPublicKeyType(key)) {
        case rsaKey:
            encAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;
            hashItem.data = hash->u.raw;
            hashItem.len = hash->len;
            if (scheme == ssl_sig_none) {
                scheme = ssl_sig_rsa_pkcs1_sha1md5;
            }
            break;
        case dsaKey:
            encAlg = SEC_OID_ANSIX9_DSA_SIGNATURE;
            /* ssl_hash_none is used to specify the MD5/SHA1 concatenated hash.
             * In that case, we use just the SHA1 part. */
            if (hash->hashAlg == ssl_hash_none) {
                hashItem.data = hash->u.s.sha;
                hashItem.len = sizeof(hash->u.s.sha);
            } else {
                hashItem.data = hash->u.raw;
                hashItem.len = hash->len;
            }
            /* Allow DER encoded DSA signatures in SSL 3.0 */
            if (ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0 ||
                buf->len != SECKEY_SignatureLen(key)) {
                signature = DSAU_DecodeDerSigToLen(buf, SECKEY_SignatureLen(key));
                if (!signature) {
                    PORT_SetError(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
                    goto loser;
                }
                buf = signature;
            }
            if (scheme == ssl_sig_none) {
                scheme = ssl_sig_dsa_sha1;
            }
            break;

        case ecKey:
            encAlg = SEC_OID_ANSIX962_EC_PUBLIC_KEY;
            /* ssl_hash_none is used to specify the MD5/SHA1 concatenated hash.
             * In that case, we use just the SHA1 part.
             * ECDSA signatures always encode the integers r and s using ASN.1
             * (unlike DSA where ASN.1 encoding is used with TLS but not with
             * SSL3). So we can use VFY_VerifyDigestDirect for ECDSA.
             */
            if (hash->hashAlg == ssl_hash_none) {
                hashAlg = SEC_OID_SHA1;
                hashItem.data = hash->u.s.sha;
                hashItem.len = sizeof(hash->u.s.sha);
            } else {
                hashItem.data = hash->u.raw;
                hashItem.len = hash->len;
            }
            if (scheme == ssl_sig_none) {
                scheme = ssl_sig_ecdsa_sha1;
            }
            break;

        default:
            PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
            goto loser;
    }

    PRINT_BUF(60, (NULL, "hash(es) to be verified",
                   hashItem.data, hashItem.len));

    if (isRsaPssScheme ||
        hashAlg == SEC_OID_UNKNOWN ||
        SECKEY_GetPublicKeyType(key) == dsaKey) {
        /* VFY_VerifyDigestDirect requires DSA signatures to be DER-encoded.
         * DSA signatures are DER-encoded in TLS but not in SSL3 and the code
         * above always removes the DER encoding of DSA signatures when
         * present. Thus DSA signatures are always verified with PK11_Verify.
         */
        CK_MECHANISM_TYPE mech = PK11_MapSignKeyType(key->keyType);

        SECItem *params = NULL;
        CK_RSA_PKCS_PSS_PARAMS pssParams;
        SECItem pssParamsItem = { siBuffer,
                                  (unsigned char *)&pssParams,
                                  sizeof(pssParams) };

        if (isRsaPssScheme) {
            pssParams.hashAlg = ssl3_GetHashMechanismByHashType(hash->hashAlg);
            pssParams.mgf = ssl3_GetMgfMechanismByHashType(hash->hashAlg);
            pssParams.sLen = hashItem.len;
            params = &pssParamsItem;
            mech = CKM_RSA_PKCS_PSS;
        }

        rv = PK11_VerifyWithMechanism(key, mech, params, buf, &hashItem, pwArg);
    } else {
        rv = VFY_VerifyDigestDirect(&hashItem, key, buf, encAlg, hashAlg,
                                    pwArg);
    }
    if (signature) {
        SECITEM_FreeItem(signature, PR_TRUE);
    }
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
    }
    if (!ss->sec.isServer) {
        ss->sec.signatureScheme = scheme;
    }

loser:
    SECKEY_DestroyPublicKey(key);
#ifdef UNSAFE_FUZZER_MODE
    rv = SECSuccess;
    PORT_SetError(0);
#endif
    return rv;
}

/* Caller must set hiLevel error code. */
/* Called from ssl3_ComputeDHKeyHash
 * which are called from ssl3_HandleServerKeyExchange.
 *
 * hashAlg: ssl_hash_none indicates the pre-1.2, MD5/SHA1 combination hash.
 */
SECStatus
ssl3_ComputeCommonKeyHash(SSLHashType hashAlg,
                          PRUint8 *hashBuf, unsigned int bufLen,
                          SSL3Hashes *hashes)
{
    SECStatus rv;
    SECOidTag hashOID;

    if (hashAlg == ssl_hash_none) {
        rv = PK11_HashBuf(SEC_OID_MD5, hashes->u.s.md5, hashBuf, bufLen);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
            return rv;
        }
        rv = PK11_HashBuf(SEC_OID_SHA1, hashes->u.s.sha, hashBuf, bufLen);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
            return rv;
        }
        hashes->len = MD5_LENGTH + SHA1_LENGTH;
    } else {
        hashOID = ssl3_HashTypeToOID(hashAlg);
        hashes->len = HASH_ResultLenByOidTag(hashOID);
        if (hashes->len == 0 || hashes->len > sizeof(hashes->u.raw)) {
            ssl_MapLowLevelError(SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM);
            return SECFailure;
        }
        rv = PK11_HashBuf(hashOID, hashes->u.raw, hashBuf, bufLen);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
            return rv;
        }
    }
    hashes->hashAlg = hashAlg;
    return SECSuccess;
}

/* Caller must set hiLevel error code. */
/* Called from ssl3_HandleServerKeyExchange. */
static SECStatus
ssl3_ComputeDHKeyHash(sslSocket *ss, SSLHashType hashAlg, SSL3Hashes *hashes,
                      SECItem dh_p, SECItem dh_g, SECItem dh_Ys, PRBool padY)
{
    PRUint8 *hashBuf;
    PRUint8 *pBuf;
    SECStatus rv = SECSuccess;
    unsigned int bufLen, yLen;
    PRUint8 buf[2 * SSL3_RANDOM_LENGTH + 2 + 4096 / 8 + 2 + 4096 / 8];

    PORT_Assert(dh_p.data);
    PORT_Assert(dh_g.data);
    PORT_Assert(dh_Ys.data);

    yLen = padY ? dh_p.len : dh_Ys.len;
    bufLen = 2 * SSL3_RANDOM_LENGTH +
             2 + dh_p.len +
             2 + dh_g.len +
             2 + yLen;
    if (bufLen <= sizeof buf) {
        hashBuf = buf;
    } else {
        hashBuf = PORT_Alloc(bufLen);
        if (!hashBuf) {
            return SECFailure;
        }
    }

    memcpy(hashBuf, &ss->ssl3.hs.client_random, SSL3_RANDOM_LENGTH);
    pBuf = hashBuf + SSL3_RANDOM_LENGTH;
    memcpy(pBuf, &ss->ssl3.hs.server_random, SSL3_RANDOM_LENGTH);
    pBuf += SSL3_RANDOM_LENGTH;
    pBuf = ssl_EncodeUintX(dh_p.len, 2, pBuf);
    memcpy(pBuf, dh_p.data, dh_p.len);
    pBuf += dh_p.len;
    pBuf = ssl_EncodeUintX(dh_g.len, 2, pBuf);
    memcpy(pBuf, dh_g.data, dh_g.len);
    pBuf += dh_g.len;
    pBuf = ssl_EncodeUintX(yLen, 2, pBuf);
    if (padY && dh_p.len > dh_Ys.len) {
        memset(pBuf, 0, dh_p.len - dh_Ys.len);
        pBuf += dh_p.len - dh_Ys.len;
    }
    /* If we're padding Y, dh_Ys can't be longer than dh_p. */
    PORT_Assert(!padY || dh_p.len >= dh_Ys.len);
    memcpy(pBuf, dh_Ys.data, dh_Ys.len);
    pBuf += dh_Ys.len;
    PORT_Assert((unsigned int)(pBuf - hashBuf) == bufLen);

    rv = ssl3_ComputeCommonKeyHash(hashAlg, hashBuf, bufLen, hashes);

    PRINT_BUF(95, (NULL, "DHkey hash: ", hashBuf, bufLen));
    if (rv == SECSuccess) {
        if (hashAlg == ssl_hash_none) {
            PRINT_BUF(95, (NULL, "DHkey hash: MD5 result",
                           hashes->u.s.md5, MD5_LENGTH));
            PRINT_BUF(95, (NULL, "DHkey hash: SHA1 result",
                           hashes->u.s.sha, SHA1_LENGTH));
        } else {
            PRINT_BUF(95, (NULL, "DHkey hash: result",
                           hashes->u.raw, hashes->len));
        }
    }

    if (hashBuf != buf && hashBuf != NULL)
        PORT_Free(hashBuf);
    return rv;
}

/* Called twice, only from ssl3_DestroyCipherSpec (immediately below). */
static void
ssl3_CleanupKeyMaterial(ssl3KeyMaterial *mat)
{
    if (mat->write_key != NULL) {
        PK11_FreeSymKey(mat->write_key);
        mat->write_key = NULL;
    }
    if (mat->write_mac_key != NULL) {
        PK11_FreeSymKey(mat->write_mac_key);
        mat->write_mac_key = NULL;
    }
    if (mat->write_mac_context != NULL) {
        PK11_DestroyContext(mat->write_mac_context, PR_TRUE);
        mat->write_mac_context = NULL;
    }
}

/* Called from ssl3_SendChangeCipherSpecs() and
**         ssl3_HandleChangeCipherSpecs()
**             ssl3_DestroySSL3Info
** Caller must hold SpecWriteLock.
*/
void
ssl3_DestroyCipherSpec(ssl3CipherSpec *spec, PRBool freeSrvName)
{
    /*  PORT_Assert( ss->opt.noLocks || ssl_HaveSpecWriteLock(ss)); Don't have ss! */
    if (spec->encodeContext) {
        PK11_DestroyContext(spec->encodeContext, PR_TRUE);
        spec->encodeContext = NULL;
    }
    if (spec->decodeContext) {
        PK11_DestroyContext(spec->decodeContext, PR_TRUE);
        spec->decodeContext = NULL;
    }
    if (spec->destroyCompressContext && spec->compressContext) {
        spec->destroyCompressContext(spec->compressContext, 1);
        spec->compressContext = NULL;
    }
    if (spec->destroyDecompressContext && spec->decompressContext) {
        spec->destroyDecompressContext(spec->decompressContext, 1);
        spec->decompressContext = NULL;
    }
    if (spec->master_secret != NULL) {
        PK11_FreeSymKey(spec->master_secret);
        spec->master_secret = NULL;
    }
    spec->msItem.data = NULL;
    spec->msItem.len = 0;
    ssl3_CleanupKeyMaterial(&spec->client);
    ssl3_CleanupKeyMaterial(&spec->server);
    spec->destroyCompressContext = NULL;
    spec->destroyDecompressContext = NULL;
}

/* Fill in the pending cipher spec with info from the selected ciphersuite.
** This is as much initialization as we can do without having key material.
** Called from ssl3_HandleServerHello(), ssl3_SendServerHello()
** Caller must hold the ssl3 handshake lock.
** Acquires & releases SpecWriteLock.
*/
SECStatus
ssl3_SetupPendingCipherSpec(sslSocket *ss)
{
    ssl3CipherSpec *pwSpec;
    ssl3CipherSpec *cwSpec;
    ssl3CipherSuite suite = ss->ssl3.hs.cipher_suite;
    SSL3MACAlgorithm mac;
    SSL3KeyExchangeAlgorithm kea;
    const ssl3CipherSuiteDef *suite_def;
    PRBool isTLS;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

    ssl_GetSpecWriteLock(ss); /*******************************/

    pwSpec = ss->ssl3.pwSpec;
    PORT_Assert(pwSpec == ss->ssl3.prSpec);

    /* This hack provides maximal interoperability with SSL 3 servers. */
    cwSpec = ss->ssl3.cwSpec;
    if (cwSpec->mac_def->mac == mac_null) {
        /* SSL records are not being MACed. */
        cwSpec->version = ss->version;
    }

    pwSpec->version = ss->version;
    isTLS = (PRBool)(pwSpec->version > SSL_LIBRARY_VERSION_3_0);

    SSL_TRC(3, ("%d: SSL3[%d]: Set XXX Pending Cipher Suite to 0x%04x",
                SSL_GETPID(), ss->fd, suite));

    suite_def = ssl_LookupCipherSuiteDef(suite);
    if (suite_def == NULL) {
        ssl_ReleaseSpecWriteLock(ss);
        return SECFailure; /* error code set by ssl_LookupCipherSuiteDef */
    }

    if (IS_DTLS(ss)) {
        /* Double-check that we did not pick an RC4 suite */
        PORT_Assert(suite_def->bulk_cipher_alg != cipher_rc4);
    }

    kea = suite_def->key_exchange_alg;
    mac = suite_def->mac_alg;
    if (mac <= ssl_mac_sha && mac != ssl_mac_null && isTLS)
        mac += 2;

    ss->ssl3.hs.suite_def = suite_def;
    ss->ssl3.hs.kea_def = &kea_defs[kea];
    PORT_Assert(ss->ssl3.hs.kea_def->kea == kea);

    pwSpec->cipher_def = ssl_GetBulkCipherDef(suite_def);

    pwSpec->mac_def = &mac_defs[mac];
    PORT_Assert(pwSpec->mac_def->mac == mac);

    pwSpec->encodeContext = NULL;
    pwSpec->decodeContext = NULL;

    pwSpec->mac_size = pwSpec->mac_def->mac_size;

    pwSpec->compression_method = ss->ssl3.hs.compression;
    pwSpec->compressContext = NULL;
    pwSpec->decompressContext = NULL;

    ssl_ReleaseSpecWriteLock(ss); /*******************************/
    return SECSuccess;
}

#ifdef NSS_SSL_ENABLE_ZLIB
#define SSL3_DEFLATE_CONTEXT_SIZE sizeof(z_stream)

static SECStatus
ssl3_MapZlibError(int zlib_error)
{
    switch (zlib_error) {
        case Z_OK:
            return SECSuccess;
        default:
            return SECFailure;
    }
}

static SECStatus
ssl3_DeflateInit(void *void_context)
{
    z_stream *context = void_context;
    context->zalloc = NULL;
    context->zfree = NULL;
    context->opaque = NULL;

    return ssl3_MapZlibError(deflateInit(context, Z_DEFAULT_COMPRESSION));
}

static SECStatus
ssl3_InflateInit(void *void_context)
{
    z_stream *context = void_context;
    context->zalloc = NULL;
    context->zfree = NULL;
    context->opaque = NULL;
    context->next_in = NULL;
    context->avail_in = 0;

    return ssl3_MapZlibError(inflateInit(context));
}

static SECStatus
ssl3_DeflateCompress(void *void_context, unsigned char *out, int *out_len,
                     int maxout, const unsigned char *in, int inlen)
{
    z_stream *context = void_context;

    if (!inlen) {
        *out_len = 0;
        return SECSuccess;
    }

    context->next_in = (unsigned char *)in;
    context->avail_in = inlen;
    context->next_out = out;
    context->avail_out = maxout;
    if (deflate(context, Z_SYNC_FLUSH) != Z_OK) {
        return SECFailure;
    }
    if (context->avail_out == 0) {
        /* We ran out of space! */
        SSL_TRC(3, ("%d: SSL3[%d] Ran out of buffer while compressing",
                    SSL_GETPID()));
        return SECFailure;
    }

    *out_len = maxout - context->avail_out;
    return SECSuccess;
}

static SECStatus
ssl3_DeflateDecompress(void *void_context, unsigned char *out, int *out_len,
                       int maxout, const unsigned char *in, int inlen)
{
    z_stream *context = void_context;

    if (!inlen) {
        *out_len = 0;
        return SECSuccess;
    }

    context->next_in = (unsigned char *)in;
    context->avail_in = inlen;
    context->next_out = out;
    context->avail_out = maxout;
    if (inflate(context, Z_SYNC_FLUSH) != Z_OK) {
        PORT_SetError(SSL_ERROR_DECOMPRESSION_FAILURE);
        return SECFailure;
    }

    *out_len = maxout - context->avail_out;
    return SECSuccess;
}

static SECStatus
ssl3_DestroyCompressContext(void *void_context, PRBool unused)
{
    deflateEnd(void_context);
    PORT_Free(void_context);
    return SECSuccess;
}

static SECStatus
ssl3_DestroyDecompressContext(void *void_context, PRBool unused)
{
    inflateEnd(void_context);
    PORT_Free(void_context);
    return SECSuccess;
}

#endif /* NSS_SSL_ENABLE_ZLIB */

/* Initialize the compression functions and contexts for the given
 * CipherSpec.  */
static SECStatus
ssl3_InitCompressionContext(ssl3CipherSpec *pwSpec)
{
    /* Setup the compression functions */
    switch (pwSpec->compression_method) {
        case ssl_compression_null:
            pwSpec->compressor = NULL;
            pwSpec->decompressor = NULL;
            pwSpec->compressContext = NULL;
            pwSpec->decompressContext = NULL;
            pwSpec->destroyCompressContext = NULL;
            pwSpec->destroyDecompressContext = NULL;
            break;
#ifdef NSS_SSL_ENABLE_ZLIB
        case ssl_compression_deflate:
            pwSpec->compressor = ssl3_DeflateCompress;
            pwSpec->decompressor = ssl3_DeflateDecompress;
            pwSpec->compressContext = PORT_Alloc(SSL3_DEFLATE_CONTEXT_SIZE);
            pwSpec->decompressContext = PORT_Alloc(SSL3_DEFLATE_CONTEXT_SIZE);
            pwSpec->destroyCompressContext = ssl3_DestroyCompressContext;
            pwSpec->destroyDecompressContext = ssl3_DestroyDecompressContext;
            ssl3_DeflateInit(pwSpec->compressContext);
            ssl3_InflateInit(pwSpec->decompressContext);
            break;
#endif /* NSS_SSL_ENABLE_ZLIB */
        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
    }

    return SECSuccess;
}

/* This function should probably be moved to pk11wrap and be named
 * PK11_ParamFromIVAndEffectiveKeyBits
 */
static SECItem *
ssl3_ParamFromIV(CK_MECHANISM_TYPE mtype, SECItem *iv, CK_ULONG ulEffectiveBits)
{
    SECItem *param = PK11_ParamFromIV(mtype, iv);
    if (param && param->data && param->len >= sizeof(CK_RC2_PARAMS)) {
        switch (mtype) {
            case CKM_RC2_KEY_GEN:
            case CKM_RC2_ECB:
            case CKM_RC2_CBC:
            case CKM_RC2_MAC:
            case CKM_RC2_MAC_GENERAL:
            case CKM_RC2_CBC_PAD:
                *(CK_RC2_PARAMS *)param->data = ulEffectiveBits;
            default:
                break;
        }
    }
    return param;
}

/* ssl3_BuildRecordPseudoHeader writes the SSL/TLS pseudo-header (the data
 * which is included in the MAC or AEAD additional data) to |out| and returns
 * its length. See https://tools.ietf.org/html/rfc5246#section-6.2.3.3 for the
 * definition of the AEAD additional data.
 *
 * TLS pseudo-header includes the record's version field, SSL's doesn't. Which
 * pseudo-header defintiion to use should be decided based on the version of
 * the protocol that was negotiated when the cipher spec became current, NOT
 * based on the version value in the record itself, and the decision is passed
 * to this function as the |includesVersion| argument. But, the |version|
 * argument should be the record's version value.
 */
static unsigned int
ssl3_BuildRecordPseudoHeader(unsigned char *out,
                             sslSequenceNumber seq_num,
                             SSL3ContentType type,
                             PRBool includesVersion,
                             SSL3ProtocolVersion version,
                             PRBool isDTLS,
                             int length)
{
    out[0] = (unsigned char)(seq_num >> 56);
    out[1] = (unsigned char)(seq_num >> 48);
    out[2] = (unsigned char)(seq_num >> 40);
    out[3] = (unsigned char)(seq_num >> 32);
    out[4] = (unsigned char)(seq_num >> 24);
    out[5] = (unsigned char)(seq_num >> 16);
    out[6] = (unsigned char)(seq_num >> 8);
    out[7] = (unsigned char)(seq_num >> 0);
    out[8] = type;

    /* SSL3 MAC doesn't include the record's version field. */
    if (!includesVersion) {
        out[9] = MSB(length);
        out[10] = LSB(length);
        return 11;
    }

    /* TLS MAC and AEAD additional data include version. */
    if (isDTLS) {
        SSL3ProtocolVersion dtls_version;

        dtls_version = dtls_TLSVersionToDTLSVersion(version);
        out[9] = MSB(dtls_version);
        out[10] = LSB(dtls_version);
    } else {
        out[9] = MSB(version);
        out[10] = LSB(version);
    }
    out[11] = MSB(length);
    out[12] = LSB(length);
    return 13;
}

static SECStatus
ssl3_AESGCM(ssl3KeyMaterial *keys,
            PRBool doDecrypt,
            unsigned char *out,
            int *outlen,
            int maxout,
            const unsigned char *in,
            int inlen,
            const unsigned char *additionalData,
            int additionalDataLen)
{
    SECItem param;
    SECStatus rv = SECFailure;
    unsigned char nonce[12];
    unsigned int uOutLen;
    CK_GCM_PARAMS gcmParams;

    const int tagSize = bulk_cipher_defs[cipher_aes_128_gcm].tag_size;
    const int explicitNonceLen =
        bulk_cipher_defs[cipher_aes_128_gcm].explicit_nonce_size;

    /* See https://tools.ietf.org/html/rfc5288#section-3 for details of how the
     * nonce is formed. */
    memcpy(nonce, keys->write_iv, 4);
    if (doDecrypt) {
        memcpy(nonce + 4, in, explicitNonceLen);
        in += explicitNonceLen;
        inlen -= explicitNonceLen;
        *outlen = 0;
    } else {
        if (maxout < explicitNonceLen) {
            PORT_SetError(SEC_ERROR_INPUT_LEN);
            return SECFailure;
        }
        /* Use the 64-bit sequence number as the explicit nonce. */
        memcpy(nonce + 4, additionalData, explicitNonceLen);
        memcpy(out, additionalData, explicitNonceLen);
        out += explicitNonceLen;
        maxout -= explicitNonceLen;
        *outlen = explicitNonceLen;
    }

    param.type = siBuffer;
    param.data = (unsigned char *)&gcmParams;
    param.len = sizeof(gcmParams);
    gcmParams.pIv = nonce;
    gcmParams.ulIvLen = sizeof(nonce);
    gcmParams.pAAD = (unsigned char *)additionalData; /* const cast */
    gcmParams.ulAADLen = additionalDataLen;
    gcmParams.ulTagBits = tagSize * 8;

    if (doDecrypt) {
        rv = PK11_Decrypt(keys->write_key, CKM_AES_GCM, &param, out, &uOutLen,
                          maxout, in, inlen);
    } else {
        rv = PK11_Encrypt(keys->write_key, CKM_AES_GCM, &param, out, &uOutLen,
                          maxout, in, inlen);
    }
    *outlen += (int)uOutLen;

    return rv;
}

static SECStatus
ssl3_ChaCha20Poly1305(ssl3KeyMaterial *keys, PRBool doDecrypt,
                      unsigned char *out, int *outlen, int maxout,
                      const unsigned char *in, int inlen,
                      const unsigned char *additionalData,
                      int additionalDataLen)
{
    size_t i;
    SECItem param;
    SECStatus rv = SECFailure;
    unsigned int uOutLen;
    unsigned char nonce[12];
    CK_NSS_AEAD_PARAMS aeadParams;

    const int tagSize = bulk_cipher_defs[cipher_chacha20].tag_size;

    /* See
     * https://tools.ietf.org/html/draft-ietf-tls-chacha20-poly1305-04#section-2
     * for details of how the nonce is formed. */
    PORT_Memcpy(nonce, keys->write_iv, 12);

    /* XOR the last 8 bytes of the IV with the sequence number. */
    PORT_Assert(additionalDataLen >= 8);
    for (i = 0; i < 8; ++i) {
        nonce[4 + i] ^= additionalData[i];
    }

    param.type = siBuffer;
    param.len = sizeof(aeadParams);
    param.data = (unsigned char *)&aeadParams;
    memset(&aeadParams, 0, sizeof(aeadParams));
    aeadParams.pNonce = nonce;
    aeadParams.ulNonceLen = sizeof(nonce);
    aeadParams.pAAD = (unsigned char *)additionalData;
    aeadParams.ulAADLen = additionalDataLen;
    aeadParams.ulTagLen = tagSize;

    if (doDecrypt) {
        rv = PK11_Decrypt(keys->write_key, CKM_NSS_CHACHA20_POLY1305, &param,
                          out, &uOutLen, maxout, in, inlen);
    } else {
        rv = PK11_Encrypt(keys->write_key, CKM_NSS_CHACHA20_POLY1305, &param,
                          out, &uOutLen, maxout, in, inlen);
    }
    *outlen = (int)uOutLen;

    return rv;
}

/* Initialize encryption and MAC contexts for pending spec.
 * Master Secret already is derived.
 * Caller holds Spec write lock.
 */
static SECStatus
ssl3_InitPendingContexts(sslSocket *ss)
{
    ssl3CipherSpec *pwSpec;
    const ssl3BulkCipherDef *cipher_def;
    PK11Context *serverContext = NULL;
    PK11Context *clientContext = NULL;
    SECItem *param;
    CK_MECHANISM_TYPE mechanism;
    CK_MECHANISM_TYPE mac_mech;
    CK_ULONG macLength;
    CK_ULONG effKeyBits;
    SECItem iv;
    SECItem mac_param;
    SSLCipherAlgorithm calg;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSpecWriteLock(ss));
    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    pwSpec = ss->ssl3.pwSpec;
    cipher_def = pwSpec->cipher_def;
    macLength = pwSpec->mac_size;
    calg = cipher_def->calg;
    PORT_Assert(alg2Mech[calg].calg == calg);

    pwSpec->client.write_mac_context = NULL;
    pwSpec->server.write_mac_context = NULL;

    if (cipher_def->type == type_aead) {
        pwSpec->encode = NULL;
        pwSpec->decode = NULL;
        pwSpec->encodeContext = NULL;
        pwSpec->decodeContext = NULL;
        switch (calg) {
            case calg_aes_gcm:
                pwSpec->aead = ssl3_AESGCM;
                break;
            case calg_chacha20:
                pwSpec->aead = ssl3_ChaCha20Poly1305;
                break;
            default:
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                return SECFailure;
        }
        return SECSuccess;
    }

    /*
    ** Now setup the MAC contexts,
    **   crypto contexts are setup below.
    */

    mac_mech = pwSpec->mac_def->mmech;
    mac_param.data = (unsigned char *)&macLength;
    mac_param.len = sizeof(macLength);
    mac_param.type = 0;

    pwSpec->client.write_mac_context = PK11_CreateContextBySymKey(
        mac_mech, CKA_SIGN, pwSpec->client.write_mac_key, &mac_param);
    if (pwSpec->client.write_mac_context == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE);
        goto fail;
    }
    pwSpec->server.write_mac_context = PK11_CreateContextBySymKey(
        mac_mech, CKA_SIGN, pwSpec->server.write_mac_key, &mac_param);
    if (pwSpec->server.write_mac_context == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE);
        goto fail;
    }

    /*
    ** Now setup the crypto contexts.
    */

    if (calg == calg_null) {
        pwSpec->encode = Null_Cipher;
        pwSpec->decode = Null_Cipher;
        return SECSuccess;
    }
    mechanism = ssl3_Alg2Mech(calg);
    effKeyBits = cipher_def->key_size * BPB;

    /*
     * build the server context
     */
    iv.data = pwSpec->server.write_iv;
    iv.len = cipher_def->iv_size;
    param = ssl3_ParamFromIV(mechanism, &iv, effKeyBits);
    if (param == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_IV_PARAM_FAILURE);
        goto fail;
    }
    serverContext = PK11_CreateContextBySymKey(mechanism,
                                               (ss->sec.isServer ? CKA_ENCRYPT
                                                                 : CKA_DECRYPT),
                                               pwSpec->server.write_key, param);
    iv.data = PK11_IVFromParam(mechanism, param, (int *)&iv.len);
    if (iv.data)
        PORT_Memcpy(pwSpec->server.write_iv, iv.data, iv.len);
    SECITEM_FreeItem(param, PR_TRUE);
    if (serverContext == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE);
        goto fail;
    }

    /*
     * build the client context
     */
    iv.data = pwSpec->client.write_iv;
    iv.len = cipher_def->iv_size;

    param = ssl3_ParamFromIV(mechanism, &iv, effKeyBits);
    if (param == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_IV_PARAM_FAILURE);
        goto fail;
    }
    clientContext = PK11_CreateContextBySymKey(mechanism,
                                               (ss->sec.isServer ? CKA_DECRYPT
                                                                 : CKA_ENCRYPT),
                                               pwSpec->client.write_key, param);
    iv.data = PK11_IVFromParam(mechanism, param, (int *)&iv.len);
    if (iv.data)
        PORT_Memcpy(pwSpec->client.write_iv, iv.data, iv.len);
    SECITEM_FreeItem(param, PR_TRUE);
    if (clientContext == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE);
        goto fail;
    }
    pwSpec->encode = (SSLCipher)PK11_CipherOp;
    pwSpec->decode = (SSLCipher)PK11_CipherOp;

    pwSpec->encodeContext = (ss->sec.isServer) ? serverContext : clientContext;
    pwSpec->decodeContext = (ss->sec.isServer) ? clientContext : serverContext;

    serverContext = NULL;
    clientContext = NULL;

    ssl3_InitCompressionContext(pwSpec);

    return SECSuccess;

fail:
    if (serverContext != NULL)
        PK11_DestroyContext(serverContext, PR_TRUE);
    if (pwSpec->client.write_mac_context != NULL) {
        PK11_DestroyContext(pwSpec->client.write_mac_context, PR_TRUE);
        pwSpec->client.write_mac_context = NULL;
    }
    if (pwSpec->server.write_mac_context != NULL) {
        PK11_DestroyContext(pwSpec->server.write_mac_context, PR_TRUE);
        pwSpec->server.write_mac_context = NULL;
    }

    return SECFailure;
}

HASH_HashType
ssl3_GetTls12HashType(sslSocket *ss)
{
    if (ss->ssl3.pwSpec->version < SSL_LIBRARY_VERSION_TLS_1_2) {
        return HASH_AlgNULL;
    }

    switch (ss->ssl3.hs.suite_def->prf_hash) {
        case ssl_hash_sha384:
            return HASH_AlgSHA384;
        case ssl_hash_sha256:
        case ssl_hash_none:
            /* ssl_hash_none is for pre-1.2 suites, which use SHA-256. */
            return HASH_AlgSHA256;
        default:
            PORT_Assert(0);
    }
    return HASH_AlgSHA256;
}

/* Complete the initialization of all keys, ciphers, MACs and their contexts
 * for the pending Cipher Spec.
 * Called from: ssl3_SendClientKeyExchange  (for Full handshake)
 *              ssl3_HandleRSAClientKeyExchange (for Full handshake)
 *              ssl3_HandleServerHello      (for session restart)
 *              ssl3_HandleClientHello      (for session restart)
 * Sets error code, but caller probably should override to disambiguate.
 * NULL pms means re-use old master_secret.
 *
 *  If the old master secret is reused, pms is NULL and the master secret is
 *  already in pwSpec->master_secret.
 */
SECStatus
ssl3_InitPendingCipherSpec(sslSocket *ss, PK11SymKey *pms)
{
    ssl3CipherSpec *pwSpec;
    ssl3CipherSpec *cwSpec;
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    ssl_GetSpecWriteLock(ss); /**************************************/

    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    pwSpec = ss->ssl3.pwSpec;
    cwSpec = ss->ssl3.cwSpec;

    if (pms || (!pwSpec->msItem.len && !pwSpec->master_secret)) {
        rv = ssl3_DeriveMasterSecret(ss, pms);
        if (rv != SECSuccess) {
            goto done; /* err code set by ssl3_DeriveMasterSecret */
        }
    }
    if (pwSpec->master_secret) {
        rv = ssl3_DeriveConnectionKeys(ss);
        if (rv == SECSuccess) {
            rv = ssl3_InitPendingContexts(ss);
        }
    } else {
        PORT_Assert(pwSpec->master_secret);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        rv = SECFailure;
    }
    if (rv != SECSuccess) {
        goto done;
    }

    /* Generic behaviors -- common to all crypto methods */
    if (!IS_DTLS(ss)) {
        pwSpec->read_seq_num = pwSpec->write_seq_num = 0;
    } else {
        if (cwSpec->epoch == PR_UINT16_MAX) {
            /* The problem here is that we have rehandshaked too many
             * times (you are not allowed to wrap the epoch). The
             * spec says you should be discarding the connection
             * and start over, so not much we can do here. */
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            rv = SECFailure;
            goto done;
        }
        /* The sequence number has the high 16 bits as the epoch. */
        pwSpec->epoch = cwSpec->epoch + 1;
        pwSpec->read_seq_num = pwSpec->write_seq_num =
            (sslSequenceNumber)pwSpec->epoch << 48;

        dtls_InitRecvdRecords(&pwSpec->recvdRecords);
    }

done:
    ssl_ReleaseSpecWriteLock(ss); /******************************/
    if (rv != SECSuccess)
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
    return rv;
}

/*
 * 60 bytes is 3 times the maximum length MAC size that is supported.
 */
static const unsigned char mac_pad_1[60] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36
};
static const unsigned char mac_pad_2[60] = {
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c
};

/* Called from: ssl3_SendRecord()
** Caller must already hold the SpecReadLock. (wish we could assert that!)
*/
static SECStatus
ssl3_ComputeRecordMAC(
    ssl3CipherSpec *spec,
    PRBool useServerMacKey,
    const unsigned char *header,
    unsigned int headerLen,
    const SSL3Opaque *input,
    int inputLength,
    unsigned char *outbuf,
    unsigned int *outLength)
{
    const ssl3MACDef *mac_def;
    SECStatus rv;

    PRINT_BUF(95, (NULL, "frag hash1: header", header, headerLen));
    PRINT_BUF(95, (NULL, "frag hash1: input", input, inputLength));

    mac_def = spec->mac_def;
    if (mac_def->mac == mac_null) {
        *outLength = 0;
        return SECSuccess;
    }

    PK11Context *mac_context =
        (useServerMacKey ? spec->server.write_mac_context
                         : spec->client.write_mac_context);
    rv = PK11_DigestBegin(mac_context);
    rv |= PK11_DigestOp(mac_context, header, headerLen);
    rv |= PK11_DigestOp(mac_context, input, inputLength);
    rv |= PK11_DigestFinal(mac_context, outbuf, outLength, spec->mac_size);
    PORT_Assert(rv != SECSuccess || *outLength == (unsigned)spec->mac_size);

    PRINT_BUF(95, (NULL, "frag hash2: result", outbuf, *outLength));

    if (rv != SECSuccess) {
        rv = SECFailure;
        ssl_MapLowLevelError(SSL_ERROR_MAC_COMPUTATION_FAILURE);
    }
    return rv;
}

/* Called from: ssl3_HandleRecord()
 * Caller must already hold the SpecReadLock. (wish we could assert that!)
 *
 * On entry:
 *   originalLen >= inputLen >= MAC size
*/
static SECStatus
ssl3_ComputeRecordMACConstantTime(
    ssl3CipherSpec *spec,
    PRBool useServerMacKey,
    const unsigned char *header,
    unsigned int headerLen,
    const SSL3Opaque *input,
    int inputLen,
    int originalLen,
    unsigned char *outbuf,
    unsigned int *outLen)
{
    CK_MECHANISM_TYPE macType;
    CK_NSS_MAC_CONSTANT_TIME_PARAMS params;
    SECItem param, inputItem, outputItem;
    SECStatus rv;
    PK11SymKey *key;

    PORT_Assert(inputLen >= spec->mac_size);
    PORT_Assert(originalLen >= inputLen);

    if (spec->mac_def->mac == mac_null) {
        *outLen = 0;
        return SECSuccess;
    }

    macType = CKM_NSS_HMAC_CONSTANT_TIME;
    if (spec->version == SSL_LIBRARY_VERSION_3_0) {
        macType = CKM_NSS_SSL3_MAC_CONSTANT_TIME;
    }

    params.macAlg = spec->mac_def->mmech;
    params.ulBodyTotalLen = originalLen;
    params.pHeader = (unsigned char *)header; /* const cast */
    params.ulHeaderLen = headerLen;

    param.data = (unsigned char *)&params;
    param.len = sizeof(params);
    param.type = 0;

    inputItem.data = (unsigned char *)input;
    inputItem.len = inputLen;
    inputItem.type = 0;

    outputItem.data = outbuf;
    outputItem.len = *outLen;
    outputItem.type = 0;

    key = spec->server.write_mac_key;
    if (!useServerMacKey) {
        key = spec->client.write_mac_key;
    }

    rv = PK11_SignWithSymKey(key, macType, &param, &outputItem, &inputItem);
    if (rv != SECSuccess) {
        if (PORT_GetError() == SEC_ERROR_INVALID_ALGORITHM) {
            /* ssl3_ComputeRecordMAC() expects the MAC to have been removed
             * from the input length already. */
            return ssl3_ComputeRecordMAC(spec, useServerMacKey,
                                         header, headerLen,
                                         input, inputLen - spec->mac_size,
                                         outbuf, outLen);
        }

        *outLen = 0;
        rv = SECFailure;
        ssl_MapLowLevelError(SSL_ERROR_MAC_COMPUTATION_FAILURE);
        return rv;
    }

    PORT_Assert(outputItem.len == (unsigned)spec->mac_size);
    *outLen = outputItem.len;

    return rv;
}

static PRBool
ssl3_ClientAuthTokenPresent(sslSessionID *sid)
{
    PK11SlotInfo *slot = NULL;
    PRBool isPresent = PR_TRUE;

    /* we only care if we are doing client auth */
    if (!sid || !sid->u.ssl3.clAuthValid) {
        return PR_TRUE;
    }

    /* get the slot */
    slot = SECMOD_LookupSlot(sid->u.ssl3.clAuthModuleID,
                             sid->u.ssl3.clAuthSlotID);
    if (slot == NULL ||
        !PK11_IsPresent(slot) ||
        sid->u.ssl3.clAuthSeries != PK11_GetSlotSeries(slot) ||
        sid->u.ssl3.clAuthSlotID != PK11_GetSlotID(slot) ||
        sid->u.ssl3.clAuthModuleID != PK11_GetModuleID(slot) ||
        (PK11_NeedLogin(slot) && !PK11_IsLoggedIn(slot, NULL))) {
        isPresent = PR_FALSE;
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return isPresent;
}

/* Caller must hold the spec read lock. */
SECStatus
ssl3_CompressMACEncryptRecord(ssl3CipherSpec *cwSpec,
                              PRBool isServer,
                              PRBool isDTLS,
                              PRBool capRecordVersion,
                              SSL3ContentType type,
                              const SSL3Opaque *pIn,
                              PRUint32 contentLen,
                              sslBuffer *wrBuf)
{
    const ssl3BulkCipherDef *cipher_def;
    SECStatus rv;
    PRUint32 macLen = 0;
    PRUint32 fragLen;
    PRUint32 p1Len, p2Len, oddLen = 0;
    unsigned int ivLen = 0;
    unsigned char pseudoHeader[13];
    unsigned int pseudoHeaderLen;

    cipher_def = cwSpec->cipher_def;

    if (cipher_def->type == type_block &&
        cwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_1) {
        /* Prepend the per-record explicit IV using technique 2b from
         * RFC 4346 section 6.2.3.2: The IV is a cryptographically
         * strong random number XORed with the CBC residue from the previous
         * record.
         */
        ivLen = cipher_def->iv_size;
        if (ivLen > wrBuf->space) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
        rv = PK11_GenerateRandom(wrBuf->buf, ivLen);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_GENERATE_RANDOM_FAILURE);
            return rv;
        }
        rv = cwSpec->encode(cwSpec->encodeContext,
                            wrBuf->buf,         /* output */
                            (int *)&wrBuf->len, /* outlen */
                            ivLen,              /* max outlen */
                            wrBuf->buf,         /* input */
                            ivLen);             /* input len */
        if (rv != SECSuccess || wrBuf->len != ivLen) {
            PORT_SetError(SSL_ERROR_ENCRYPTION_FAILURE);
            return SECFailure;
        }
    }

    if (cwSpec->compressor) {
        int outlen;
        rv = cwSpec->compressor(cwSpec->compressContext, wrBuf->buf + ivLen,
                                &outlen, wrBuf->space - ivLen, pIn, contentLen);
        if (rv != SECSuccess)
            return rv;
        pIn = wrBuf->buf + ivLen;
        contentLen = outlen;
    }

    pseudoHeaderLen = ssl3_BuildRecordPseudoHeader(
        pseudoHeader, cwSpec->write_seq_num, type,
        cwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_0, cwSpec->version,
        isDTLS, contentLen);
    PORT_Assert(pseudoHeaderLen <= sizeof(pseudoHeader));
    if (cipher_def->type == type_aead) {
        const int nonceLen = cipher_def->explicit_nonce_size;
        const int tagLen = cipher_def->tag_size;

        if (nonceLen + contentLen + tagLen > wrBuf->space) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }

        rv = cwSpec->aead(
            isServer ? &cwSpec->server : &cwSpec->client,
            PR_FALSE,           /* do encrypt */
            wrBuf->buf,         /* output  */
            (int *)&wrBuf->len, /* out len */
            wrBuf->space,       /* max out */
            pIn, contentLen,    /* input   */
            pseudoHeader, pseudoHeaderLen);
        if (rv != SECSuccess) {
            PORT_SetError(SSL_ERROR_ENCRYPTION_FAILURE);
            return SECFailure;
        }
    } else {
        /*
         * Add the MAC
         */
        rv = ssl3_ComputeRecordMAC(cwSpec, isServer, pseudoHeader,
                                   pseudoHeaderLen, pIn, contentLen,
                                   wrBuf->buf + ivLen + contentLen, &macLen);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_MAC_COMPUTATION_FAILURE);
            return SECFailure;
        }
        p1Len = contentLen;
        p2Len = macLen;
        fragLen = contentLen + macLen; /* needs to be encrypted */
        PORT_Assert(fragLen <= MAX_FRAGMENT_LENGTH + 1024);

        /*
         * Pad the text (if we're doing a block cipher)
         * then Encrypt it
         */
        if (cipher_def->type == type_block) {
            unsigned char *pBuf;
            int padding_length;
            int i;

            oddLen = contentLen % cipher_def->block_size;
            /* Assume blockSize is a power of two */
            padding_length = cipher_def->block_size - 1 - ((fragLen) & (cipher_def->block_size - 1));
            fragLen += padding_length + 1;
            PORT_Assert((fragLen % cipher_def->block_size) == 0);

            /* Pad according to TLS rules (also acceptable to SSL3). */
            pBuf = &wrBuf->buf[ivLen + fragLen - 1];
            for (i = padding_length + 1; i > 0; --i) {
                *pBuf-- = padding_length;
            }
            /* now, if contentLen is not a multiple of block size, fix it */
            p2Len = fragLen - p1Len;
        }
        if (p1Len < 256) {
            oddLen = p1Len;
            p1Len = 0;
        } else {
            p1Len -= oddLen;
        }
        if (oddLen) {
            p2Len += oddLen;
            PORT_Assert((cipher_def->block_size < 2) ||
                        (p2Len % cipher_def->block_size) == 0);
            memmove(wrBuf->buf + ivLen + p1Len, pIn + p1Len, oddLen);
        }
        if (p1Len > 0) {
            int cipherBytesPart1 = -1;
            rv = cwSpec->encode(cwSpec->encodeContext,
                                wrBuf->buf + ivLen, /* output */
                                &cipherBytesPart1,  /* actual outlen */
                                p1Len,              /* max outlen */
                                pIn,
                                p1Len); /* input, and inputlen */
            PORT_Assert(rv == SECSuccess && cipherBytesPart1 == (int)p1Len);
            if (rv != SECSuccess || cipherBytesPart1 != (int)p1Len) {
                PORT_SetError(SSL_ERROR_ENCRYPTION_FAILURE);
                return SECFailure;
            }
            wrBuf->len += cipherBytesPart1;
        }
        if (p2Len > 0) {
            int cipherBytesPart2 = -1;
            rv = cwSpec->encode(cwSpec->encodeContext,
                                wrBuf->buf + ivLen + p1Len,
                                &cipherBytesPart2, /* output and actual outLen */
                                p2Len,             /* max outlen */
                                wrBuf->buf + ivLen + p1Len,
                                p2Len); /* input and inputLen*/
            PORT_Assert(rv == SECSuccess && cipherBytesPart2 == (int)p2Len);
            if (rv != SECSuccess || cipherBytesPart2 != (int)p2Len) {
                PORT_SetError(SSL_ERROR_ENCRYPTION_FAILURE);
                return SECFailure;
            }
            wrBuf->len += cipherBytesPart2;
        }
    }

    return SECSuccess;
}

SECStatus
ssl_ProtectRecord(sslSocket *ss, ssl3CipherSpec *cwSpec,
                  PRBool capRecordVersion, SSL3ContentType type,
                  const SSL3Opaque *pIn, PRUint32 contentLen, sslBuffer *wrBuf)
{
    const ssl3BulkCipherDef *cipher_def = cwSpec->cipher_def;
    PRUint16 headerLen = IS_DTLS(ss) ? DTLS_RECORD_HEADER_LENGTH : SSL3_RECORD_HEADER_LENGTH;
    sslBuffer protBuf = { wrBuf->buf + headerLen, 0, wrBuf->space - headerLen };
    SSL3ProtocolVersion version = cwSpec->version;
    PRBool isTLS13;
    SECStatus rv;

    PORT_Assert(cipher_def->max_records <= RECORD_SEQ_MAX);
    if ((cwSpec->write_seq_num & RECORD_SEQ_MAX) >= cipher_def->max_records) {
        SSL_TRC(3, ("%d: SSL[-]: write sequence number at limit 0x%0llx",
                    SSL_GETPID(), cwSpec->write_seq_num));
        PORT_SetError(SSL_ERROR_TOO_MANY_RECORDS);
        return SECFailure;
    }

    isTLS13 = (PRBool)(cwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_3);

#ifdef UNSAFE_FUZZER_MODE
    rv = Null_Cipher(NULL, protBuf.buf, (int *)&protBuf.len, protBuf.space,
                     pIn, contentLen);
#else
    if (isTLS13) {
        rv = tls13_ProtectRecord(ss, cwSpec, type, pIn, contentLen, &protBuf);
    } else {
        rv = ssl3_CompressMACEncryptRecord(cwSpec, ss->sec.isServer,
                                           IS_DTLS(ss), capRecordVersion, type,
                                           pIn, contentLen, &protBuf);
    }
#endif
    if (rv != SECSuccess) {
        return SECFailure; /* error was set */
    }

    PORT_Assert(protBuf.len <= MAX_FRAGMENT_LENGTH + (isTLS13 ? 256 : 1024));
    wrBuf->len = protBuf.len + headerLen;

#ifndef UNSAFE_FUZZER_MODE
    if (isTLS13 && cipher_def->calg != ssl_calg_null) {
        wrBuf->buf[0] = content_application_data;
    } else
#endif
    {
        wrBuf->buf[0] = type;
    }

    if (IS_DTLS(ss)) {
        version = isTLS13 ? SSL_LIBRARY_VERSION_TLS_1_1 : version;
        version = dtls_TLSVersionToDTLSVersion(version);

        (void)ssl_EncodeUintX(version, 2, &wrBuf->buf[1]);
        (void)ssl_EncodeUintX(cwSpec->write_seq_num, 8, &wrBuf->buf[3]);
        (void)ssl_EncodeUintX(protBuf.len, 2, &wrBuf->buf[11]);
    } else {
        if (capRecordVersion || isTLS13) {
            version = PR_MIN(SSL_LIBRARY_VERSION_TLS_1_0, version);
        }

        (void)ssl_EncodeUintX(version, 2, &wrBuf->buf[1]);
        (void)ssl_EncodeUintX(protBuf.len, 2, &wrBuf->buf[3]);
    }
    ++cwSpec->write_seq_num;

    return SECSuccess;
}

/* Process the plain text before sending it.
 * Returns the number of bytes of plaintext that were successfully sent
 *  plus the number of bytes of plaintext that were copied into the
 *  output (write) buffer.
 * Returns SECFailure on a hard IO error, memory error, or crypto error.
 * Does NOT return SECWouldBlock.
 *
 * Notes on the use of the private ssl flags:
 * (no private SSL flags)
 *    Attempt to make and send SSL records for all plaintext
 *    If non-blocking and a send gets WOULD_BLOCK,
 *    or if the pending (ciphertext) buffer is not empty,
 *    then buffer remaining bytes of ciphertext into pending buf,
 *    and continue to do that for all succssive records until all
 *    bytes are used.
 * ssl_SEND_FLAG_FORCE_INTO_BUFFER
 *    As above, except this suppresses all write attempts, and forces
 *    all ciphertext into the pending ciphertext buffer.
 * ssl_SEND_FLAG_USE_EPOCH (for DTLS)
 *    Forces the use of the provided epoch
 * ssl_SEND_FLAG_CAP_RECORD_VERSION
 *    Caps the record layer version number of TLS ClientHello to { 3, 1 }
 *    (TLS 1.0). Some TLS 1.0 servers (which seem to use F5 BIG-IP) ignore
 *    ClientHello.client_version and use the record layer version number
 *    (TLSPlaintext.version) instead when negotiating protocol versions. In
 *    addition, if the record layer version number of ClientHello is { 3, 2 }
 *    (TLS 1.1) or higher, these servers reset the TCP connections. Lastly,
 *    some F5 BIG-IP servers hang if a record containing a ClientHello has a
 *    version greater than { 3, 1 } and a length greater than 255. Set this
 *    flag to work around such servers.
 */
PRInt32
ssl3_SendRecord(sslSocket *ss,
                ssl3CipherSpec *cwSpec, /* non-NULL for DTLS retransmits */
                SSL3ContentType type,
                const SSL3Opaque *pIn, /* input buffer */
                PRInt32 nIn,           /* bytes of input */
                PRInt32 flags)
{
    sslBuffer *wrBuf = &ss->sec.writeBuf;
    SECStatus rv;
    PRInt32 totalSent = 0;
    PRBool capRecordVersion;

    SSL_TRC(3, ("%d: SSL3[%d] SendRecord type: %s nIn=%d",
                SSL_GETPID(), ss->fd, ssl3_DecodeContentType(type),
                nIn));
    PRINT_BUF(50, (ss, "Send record (plain text)", pIn, nIn));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    if (ss->ssl3.fatalAlertSent) {
        SSL_TRC(3, ("%d: SSL3[%d] Suppress write, fatal alert already sent",
                    SSL_GETPID(), ss->fd));
        return SECFailure;
    }

    capRecordVersion = ((flags & ssl_SEND_FLAG_CAP_RECORD_VERSION) != 0);

    if (capRecordVersion) {
        /* ssl_SEND_FLAG_CAP_RECORD_VERSION can only be used with the
         * TLS initial ClientHello. */
        PORT_Assert(!IS_DTLS(ss));
        PORT_Assert(!ss->firstHsDone);
        PORT_Assert(type == content_handshake);
        PORT_Assert(ss->ssl3.hs.ws == wait_server_hello);
    }

    if (ss->ssl3.initialized == PR_FALSE) {
        /* This can happen on a server if the very first incoming record
        ** looks like a defective ssl3 record (e.g. too long), and we're
        ** trying to send an alert.
        */
        PR_ASSERT(type == content_alert);
        rv = ssl3_InitState(ss);
        if (rv != SECSuccess) {
            return SECFailure; /* ssl3_InitState has set the error code. */
        }
    }

    /* check for Token Presence */
    if (!ssl3_ClientAuthTokenPresent(ss->sec.ci.sid)) {
        PORT_SetError(SSL_ERROR_TOKEN_INSERTION_REMOVAL);
        return SECFailure;
    }

    while (nIn > 0) {
        PRUint32 contentLen = PR_MIN(nIn, MAX_FRAGMENT_LENGTH);
        unsigned int spaceNeeded;
        unsigned int numRecords;

        ssl_GetSpecReadLock(ss); /********************************/

        if (nIn > 1 && ss->opt.cbcRandomIV &&
            ss->ssl3.cwSpec->version < SSL_LIBRARY_VERSION_TLS_1_1 &&
            type == content_application_data &&
            ss->ssl3.cwSpec->cipher_def->type == type_block /* CBC mode */) {
            /* We will split the first byte of the record into its own record,
             * as explained in the documentation for SSL_CBC_RANDOM_IV in ssl.h
             */
            numRecords = 2;
        } else {
            numRecords = 1;
        }

        spaceNeeded = contentLen + (numRecords * SSL3_BUFFER_FUDGE);
        if (ss->ssl3.cwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_1 &&
            ss->ssl3.cwSpec->cipher_def->type == type_block) {
            spaceNeeded += ss->ssl3.cwSpec->cipher_def->iv_size;
        }
        if (spaceNeeded > wrBuf->space) {
            rv = sslBuffer_Grow(wrBuf, spaceNeeded);
            if (rv != SECSuccess) {
                SSL_DBG(("%d: SSL3[%d]: SendRecord, tried to get %d bytes",
                         SSL_GETPID(), ss->fd, spaceNeeded));
                goto spec_locked_loser; /* sslBuffer_Grow set error code. */
            }
        }

        if (numRecords == 2) {
            sslBuffer secondRecord;
            rv = ssl_ProtectRecord(ss, ss->ssl3.cwSpec, capRecordVersion, type,
                                   pIn, 1, wrBuf);
            if (rv != SECSuccess)
                goto spec_locked_loser;

            PRINT_BUF(50, (ss, "send (encrypted) record data [1/2]:",
                           wrBuf->buf, wrBuf->len));

            secondRecord.buf = wrBuf->buf + wrBuf->len;
            secondRecord.len = 0;
            secondRecord.space = wrBuf->space - wrBuf->len;

            rv = ssl_ProtectRecord(ss, ss->ssl3.cwSpec, capRecordVersion, type,
                                   pIn + 1, contentLen - 1, &secondRecord);
            if (rv == SECSuccess) {
                PRINT_BUF(50, (ss, "send (encrypted) record data [2/2]:",
                               secondRecord.buf, secondRecord.len));
                wrBuf->len += secondRecord.len;
            }
        } else {
            if (cwSpec) {
                /* cwSpec can only be set for retransmissions of DTLS handshake
                 * messages. */
                PORT_Assert(IS_DTLS(ss) &&
                            (type == content_handshake ||
                             type == content_change_cipher_spec));
            } else {
                cwSpec = ss->ssl3.cwSpec;
            }

            rv = ssl_ProtectRecord(ss, cwSpec, !IS_DTLS(ss) && capRecordVersion,
                                   type, pIn, contentLen, wrBuf);
            if (rv == SECSuccess) {
                PRINT_BUF(50, (ss, "send (encrypted) record data:",
                               wrBuf->buf, wrBuf->len));
            }
        }

    spec_locked_loser:
        ssl_ReleaseSpecReadLock(ss); /************************************/

        if (rv != SECSuccess)
            return SECFailure;

        pIn += contentLen;
        nIn -= contentLen;
        PORT_Assert(nIn >= 0);

        /* If there's still some previously saved ciphertext,
         * or the caller doesn't want us to send the data yet,
         * then add all our new ciphertext to the amount previously saved.
         */
        if ((ss->pendingBuf.len > 0) ||
            (flags & ssl_SEND_FLAG_FORCE_INTO_BUFFER)) {

            rv = ssl_SaveWriteData(ss, wrBuf->buf, wrBuf->len);
            if (rv != SECSuccess) {
                /* presumably a memory error, SEC_ERROR_NO_MEMORY */
                return SECFailure;
            }
            wrBuf->len = 0; /* All cipher text is saved away. */

            if (!(flags & ssl_SEND_FLAG_FORCE_INTO_BUFFER)) {
                PRInt32 sent;
                ss->handshakeBegun = 1;
                sent = ssl_SendSavedWriteData(ss);
                if (sent < 0 && PR_GetError() != PR_WOULD_BLOCK_ERROR) {
                    ssl_MapLowLevelError(SSL_ERROR_SOCKET_WRITE_FAILURE);
                    return SECFailure;
                }
                if (ss->pendingBuf.len) {
                    flags |= ssl_SEND_FLAG_FORCE_INTO_BUFFER;
                }
            }
        } else if (wrBuf->len > 0) {
            PRInt32 sent;
            ss->handshakeBegun = 1;
            sent = ssl_DefSend(ss, wrBuf->buf, wrBuf->len,
                               flags & ~ssl_SEND_FLAG_MASK);
            if (sent < 0) {
                if (PR_GetError() != PR_WOULD_BLOCK_ERROR) {
                    ssl_MapLowLevelError(SSL_ERROR_SOCKET_WRITE_FAILURE);
                    return SECFailure;
                }
                /* we got PR_WOULD_BLOCK_ERROR, which means none was sent. */
                sent = 0;
            }
            wrBuf->len -= sent;
            if (wrBuf->len) {
                if (IS_DTLS(ss)) {
                    /* DTLS just says no in this case. No buffering */
                    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
                    return SECFailure;
                }
                /* now take all the remaining unsent new ciphertext and
                 * append it to the buffer of previously unsent ciphertext.
                 */
                rv = ssl_SaveWriteData(ss, wrBuf->buf + sent, wrBuf->len);
                if (rv != SECSuccess) {
                    /* presumably a memory error, SEC_ERROR_NO_MEMORY */
                    return SECFailure;
                }
            }
        }
        totalSent += contentLen;
    }
    return totalSent;
}

#define SSL3_PENDING_HIGH_WATER 1024

/* Attempt to send the content of "in" in an SSL application_data record.
 * Returns "len" or SECFailure,   never SECWouldBlock, nor SECSuccess.
 */
int
ssl3_SendApplicationData(sslSocket *ss, const unsigned char *in,
                         PRInt32 len, PRInt32 flags)
{
    PRInt32 totalSent = 0;
    PRInt32 discarded = 0;

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    /* These flags for internal use only */
    PORT_Assert(!(flags & ssl_SEND_FLAG_NO_RETRANSMIT));
    if (len < 0 || !in) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    if (ss->pendingBuf.len > SSL3_PENDING_HIGH_WATER &&
        !ssl_SocketIsBlocking(ss)) {
        PORT_Assert(!ssl_SocketIsBlocking(ss));
        PORT_SetError(PR_WOULD_BLOCK_ERROR);
        return SECFailure;
    }

    if (ss->appDataBuffered && len) {
        PORT_Assert(in[0] == (unsigned char)(ss->appDataBuffered));
        if (in[0] != (unsigned char)(ss->appDataBuffered)) {
            PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
            return SECFailure;
        }
        in++;
        len--;
        discarded = 1;
    }
    while (len > totalSent) {
        PRInt32 sent, toSend;

        if (totalSent > 0) {
            /*
             * The thread yield is intended to give the reader thread a
             * chance to get some cycles while the writer thread is in
             * the middle of a large application data write.  (See
             * Bugzilla bug 127740, comment #1.)
             */
            ssl_ReleaseXmitBufLock(ss);
            PR_Sleep(PR_INTERVAL_NO_WAIT); /* PR_Yield(); */
            ssl_GetXmitBufLock(ss);
        }
        toSend = PR_MIN(len - totalSent, MAX_FRAGMENT_LENGTH);
        /*
         * Note that the 0 epoch is OK because flags will never require
         * its use, as guaranteed by the PORT_Assert above.
         */
        sent = ssl3_SendRecord(ss, NULL, content_application_data,
                               in + totalSent, toSend, flags);
        if (sent < 0) {
            if (totalSent > 0 && PR_GetError() == PR_WOULD_BLOCK_ERROR) {
                PORT_Assert(ss->lastWriteBlocked);
                break;
            }
            return SECFailure; /* error code set by ssl3_SendRecord */
        }
        totalSent += sent;
        if (ss->pendingBuf.len) {
            /* must be a non-blocking socket */
            PORT_Assert(!ssl_SocketIsBlocking(ss));
            PORT_Assert(ss->lastWriteBlocked);
            break;
        }
    }
    if (ss->pendingBuf.len) {
        /* Must be non-blocking. */
        PORT_Assert(!ssl_SocketIsBlocking(ss));
        if (totalSent > 0) {
            ss->appDataBuffered = 0x100 | in[totalSent - 1];
        }

        totalSent = totalSent + discarded - 1;
        if (totalSent <= 0) {
            PORT_SetError(PR_WOULD_BLOCK_ERROR);
            totalSent = SECFailure;
        }
        return totalSent;
    }
    ss->appDataBuffered = 0;
    return totalSent + discarded;
}

/* Attempt to send buffered handshake messages.
 * This function returns SECSuccess or SECFailure, never SECWouldBlock.
 * Always set sendBuf.len to 0, even when returning SECFailure.
 *
 * Depending on whether we are doing DTLS or not, this either calls
 *
 * - ssl3_FlushHandshakeMessages if non-DTLS
 * - dtls_FlushHandshakeMessages if DTLS
 *
 * Called from SSL3_SendAlert(), ssl3_SendChangeCipherSpecs(),
 *             ssl3_AppendHandshake(), ssl3_SendClientHello(),
 *             ssl3_SendHelloRequest(), ssl3_SendServerHelloDone(),
 *             ssl3_SendFinished(),
 */
SECStatus
ssl3_FlushHandshake(sslSocket *ss, PRInt32 flags)
{
    if (IS_DTLS(ss)) {
        return dtls_FlushHandshakeMessages(ss, flags);
    } else {
        return ssl3_FlushHandshakeMessages(ss, flags);
    }
}

/* Attempt to send the content of sendBuf buffer in an SSL handshake record.
 * This function returns SECSuccess or SECFailure, never SECWouldBlock.
 * Always set sendBuf.len to 0, even when returning SECFailure.
 *
 * Called from ssl3_FlushHandshake
 */
static SECStatus
ssl3_FlushHandshakeMessages(sslSocket *ss, PRInt32 flags)
{
    static const PRInt32 allowedFlags = ssl_SEND_FLAG_FORCE_INTO_BUFFER |
                                        ssl_SEND_FLAG_CAP_RECORD_VERSION;
    PRInt32 count = -1;
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    if (!ss->sec.ci.sendBuf.buf || !ss->sec.ci.sendBuf.len)
        return SECSuccess;

    /* only these flags are allowed */
    PORT_Assert(!(flags & ~allowedFlags));
    if ((flags & ~allowedFlags) != 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    count = ssl3_SendRecord(ss, NULL, content_handshake,
                            ss->sec.ci.sendBuf.buf,
                            ss->sec.ci.sendBuf.len, flags);
    if (count < 0) {
        int err = PORT_GetError();
        PORT_Assert(err != PR_WOULD_BLOCK_ERROR);
        if (err == PR_WOULD_BLOCK_ERROR) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        }
        rv = SECFailure;
    } else if ((unsigned int)count < ss->sec.ci.sendBuf.len) {
        /* short write should never happen */
        PORT_Assert((unsigned int)count >= ss->sec.ci.sendBuf.len);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        rv = SECFailure;
    } else {
        rv = SECSuccess;
    }

    /* Whether we succeeded or failed, toss the old handshake data. */
    ss->sec.ci.sendBuf.len = 0;
    return rv;
}

/*
 * Called from ssl3_HandleAlert and from ssl3_HandleCertificate when
 * the remote client sends a negative response to our certificate request.
 * Returns SECFailure if the application has required client auth.
 *         SECSuccess otherwise.
 */
SECStatus
ssl3_HandleNoCertificate(sslSocket *ss)
{
    ssl3_CleanupPeerCerts(ss);

    /* If the server has required client-auth blindly but doesn't
     * actually look at the certificate it won't know that no
     * certificate was presented so we shutdown the socket to ensure
     * an error.  We only do this if we haven't already completed the
     * first handshake because if we're redoing the handshake we
     * know the server is paying attention to the certificate.
     */
    if ((ss->opt.requireCertificate == SSL_REQUIRE_ALWAYS) ||
        (!ss->firstHsDone &&
         (ss->opt.requireCertificate == SSL_REQUIRE_FIRST_HANDSHAKE))) {
        PRFileDesc *lower;

        ss->sec.uncache(ss->sec.ci.sid);
        SSL3_SendAlert(ss, alert_fatal, bad_certificate);

        lower = ss->fd->lower;
#ifdef _WIN32
        lower->methods->shutdown(lower, PR_SHUTDOWN_SEND);
#else
        lower->methods->shutdown(lower, PR_SHUTDOWN_BOTH);
#endif
        PORT_SetError(SSL_ERROR_NO_CERTIFICATE);
        return SECFailure;
    }
    return SECSuccess;
}

/************************************************************************
 * Alerts
 */

/*
** Acquires both handshake and XmitBuf locks.
** Called from: ssl3_IllegalParameter   <-
**              ssl3_HandshakeFailure   <-
**              ssl3_HandleAlert    <- ssl3_HandleRecord.
**              ssl3_HandleChangeCipherSpecs <- ssl3_HandleRecord
**              ssl3_ConsumeHandshakeVariable <-
**              ssl3_HandleHelloRequest <-
**              ssl3_HandleServerHello  <-
**              ssl3_HandleServerKeyExchange <-
**              ssl3_HandleCertificateRequest <-
**              ssl3_HandleServerHelloDone <-
**              ssl3_HandleClientHello  <-
**              ssl3_HandleV2ClientHello <-
**              ssl3_HandleCertificateVerify <-
**              ssl3_HandleClientKeyExchange <-
**              ssl3_HandleCertificate  <-
**              ssl3_HandleFinished <-
**              ssl3_HandleHandshakeMessage <-
**              ssl3_HandlePostHelloHandshakeMessage <-
**              ssl3_HandleRecord   <-
**
*/
SECStatus
SSL3_SendAlert(sslSocket *ss, SSL3AlertLevel level, SSL3AlertDescription desc)
{
    PRUint8 bytes[2];
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: send alert record, level=%d desc=%d",
                SSL_GETPID(), ss->fd, level, desc));

    bytes[0] = level;
    bytes[1] = desc;

    ssl_GetSSL3HandshakeLock(ss);
    if (level == alert_fatal) {
        if (!ss->opt.noCache && ss->sec.ci.sid) {
            ss->sec.uncache(ss->sec.ci.sid);
        }
    }
    ssl_GetXmitBufLock(ss);
    rv = ssl3_FlushHandshake(ss, ssl_SEND_FLAG_FORCE_INTO_BUFFER);
    if (rv == SECSuccess) {
        PRInt32 sent;
        sent = ssl3_SendRecord(ss, NULL, content_alert, bytes, 2,
                               (desc == no_certificate) ? ssl_SEND_FLAG_FORCE_INTO_BUFFER : 0);
        rv = (sent >= 0) ? SECSuccess : (SECStatus)sent;
    }
    if (level == alert_fatal) {
        ss->ssl3.fatalAlertSent = PR_TRUE;
    }
    ssl_ReleaseXmitBufLock(ss);
    ssl_ReleaseSSL3HandshakeLock(ss);
    return rv; /* error set by ssl3_FlushHandshake or ssl3_SendRecord */
}

/*
 * Send illegal_parameter alert.  Set generic error number.
 */
static SECStatus
ssl3_IllegalParameter(sslSocket *ss)
{
    (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
    PORT_SetError(ss->sec.isServer ? SSL_ERROR_BAD_CLIENT
                                   : SSL_ERROR_BAD_SERVER);
    return SECFailure;
}

/*
 * Send handshake_Failure alert.  Set generic error number.
 */
static SECStatus
ssl3_HandshakeFailure(sslSocket *ss)
{
    (void)SSL3_SendAlert(ss, alert_fatal, handshake_failure);
    PORT_SetError(ss->sec.isServer ? SSL_ERROR_BAD_CLIENT
                                   : SSL_ERROR_BAD_SERVER);
    return SECFailure;
}

void
ssl3_SendAlertForCertError(sslSocket *ss, PRErrorCode errCode)
{
    SSL3AlertDescription desc = bad_certificate;
    PRBool isTLS = ss->version >= SSL_LIBRARY_VERSION_3_1_TLS;

    switch (errCode) {
        case SEC_ERROR_LIBRARY_FAILURE:
            desc = unsupported_certificate;
            break;
        case SEC_ERROR_EXPIRED_CERTIFICATE:
            desc = certificate_expired;
            break;
        case SEC_ERROR_REVOKED_CERTIFICATE:
            desc = certificate_revoked;
            break;
        case SEC_ERROR_INADEQUATE_KEY_USAGE:
        case SEC_ERROR_INADEQUATE_CERT_TYPE:
            desc = certificate_unknown;
            break;
        case SEC_ERROR_UNTRUSTED_CERT:
            desc = isTLS ? access_denied : certificate_unknown;
            break;
        case SEC_ERROR_UNKNOWN_ISSUER:
        case SEC_ERROR_UNTRUSTED_ISSUER:
            desc = isTLS ? unknown_ca : certificate_unknown;
            break;
        case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
            desc = isTLS ? unknown_ca : certificate_expired;
            break;

        case SEC_ERROR_CERT_NOT_IN_NAME_SPACE:
        case SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID:
        case SEC_ERROR_CA_CERT_INVALID:
        case SEC_ERROR_BAD_SIGNATURE:
        default:
            desc = bad_certificate;
            break;
    }
    SSL_DBG(("%d: SSL3[%d]: peer certificate is no good: error=%d",
             SSL_GETPID(), ss->fd, errCode));

    (void)SSL3_SendAlert(ss, alert_fatal, desc);
}

/*
 * Send decode_error alert.  Set generic error number.
 */
SECStatus
ssl3_DecodeError(sslSocket *ss)
{
    (void)SSL3_SendAlert(ss, alert_fatal,
                         ss->version > SSL_LIBRARY_VERSION_3_0 ? decode_error
                                                               : illegal_parameter);
    PORT_SetError(ss->sec.isServer ? SSL_ERROR_BAD_CLIENT
                                   : SSL_ERROR_BAD_SERVER);
    return SECFailure;
}

/* Called from ssl3_HandleRecord.
** Caller must hold both RecvBuf and Handshake locks.
*/
static SECStatus
ssl3_HandleAlert(sslSocket *ss, sslBuffer *buf)
{
    SSL3AlertLevel level;
    SSL3AlertDescription desc;
    int error;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    SSL_TRC(3, ("%d: SSL3[%d]: handle alert record", SSL_GETPID(), ss->fd));

    if (buf->len != 2) {
        (void)ssl3_DecodeError(ss);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ALERT);
        return SECFailure;
    }
    level = (SSL3AlertLevel)buf->buf[0];
    desc = (SSL3AlertDescription)buf->buf[1];
    buf->len = 0;
    SSL_TRC(5, ("%d: SSL3[%d] received alert, level = %d, description = %d",
                SSL_GETPID(), ss->fd, level, desc));

    switch (desc) {
        case close_notify:
            ss->recvdCloseNotify = 1;
            error = SSL_ERROR_CLOSE_NOTIFY_ALERT;
            break;
        case unexpected_message:
            error = SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT;
            break;
        case bad_record_mac:
            error = SSL_ERROR_BAD_MAC_ALERT;
            break;
        case decryption_failed_RESERVED:
            error = SSL_ERROR_DECRYPTION_FAILED_ALERT;
            break;
        case record_overflow:
            error = SSL_ERROR_RECORD_OVERFLOW_ALERT;
            break;
        case decompression_failure:
            error = SSL_ERROR_DECOMPRESSION_FAILURE_ALERT;
            break;
        case handshake_failure:
            error = SSL_ERROR_HANDSHAKE_FAILURE_ALERT;
            break;
        case no_certificate:
            error = SSL_ERROR_NO_CERTIFICATE;
            break;
        case bad_certificate:
            error = SSL_ERROR_BAD_CERT_ALERT;
            break;
        case unsupported_certificate:
            error = SSL_ERROR_UNSUPPORTED_CERT_ALERT;
            break;
        case certificate_revoked:
            error = SSL_ERROR_REVOKED_CERT_ALERT;
            break;
        case certificate_expired:
            error = SSL_ERROR_EXPIRED_CERT_ALERT;
            break;
        case certificate_unknown:
            error = SSL_ERROR_CERTIFICATE_UNKNOWN_ALERT;
            break;
        case illegal_parameter:
            error = SSL_ERROR_ILLEGAL_PARAMETER_ALERT;
            break;
        case inappropriate_fallback:
            error = SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT;
            break;

        /* All alerts below are TLS only. */
        case unknown_ca:
            error = SSL_ERROR_UNKNOWN_CA_ALERT;
            break;
        case access_denied:
            error = SSL_ERROR_ACCESS_DENIED_ALERT;
            break;
        case decode_error:
            error = SSL_ERROR_DECODE_ERROR_ALERT;
            break;
        case decrypt_error:
            error = SSL_ERROR_DECRYPT_ERROR_ALERT;
            break;
        case export_restriction:
            error = SSL_ERROR_EXPORT_RESTRICTION_ALERT;
            break;
        case protocol_version:
            error = SSL_ERROR_PROTOCOL_VERSION_ALERT;
            break;
        case insufficient_security:
            error = SSL_ERROR_INSUFFICIENT_SECURITY_ALERT;
            break;
        case internal_error:
            error = SSL_ERROR_INTERNAL_ERROR_ALERT;
            break;
        case user_canceled:
            error = SSL_ERROR_USER_CANCELED_ALERT;
            break;
        case no_renegotiation:
            error = SSL_ERROR_NO_RENEGOTIATION_ALERT;
            break;

        /* Alerts for TLS client hello extensions */
        case missing_extension:
            error = SSL_ERROR_MISSING_EXTENSION_ALERT;
            break;
        case unsupported_extension:
            error = SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT;
            break;
        case certificate_unobtainable:
            error = SSL_ERROR_CERTIFICATE_UNOBTAINABLE_ALERT;
            break;
        case unrecognized_name:
            error = SSL_ERROR_UNRECOGNIZED_NAME_ALERT;
            break;
        case bad_certificate_status_response:
            error = SSL_ERROR_BAD_CERT_STATUS_RESPONSE_ALERT;
            break;
        case bad_certificate_hash_value:
            error = SSL_ERROR_BAD_CERT_HASH_VALUE_ALERT;
            break;
        case end_of_early_data:
            error = SSL_ERROR_END_OF_EARLY_DATA_ALERT;
            break;
        default:
            error = SSL_ERROR_RX_UNKNOWN_ALERT;
            break;
    }
    if ((ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) &&
        (ss->ssl3.hs.ws != wait_server_hello)) {
        /* TLS 1.3 requires all but "end of data" alerts to be
         * treated as fatal. */
        switch (desc) {
            case close_notify:
            case user_canceled:
            case end_of_early_data:
                break;
            default:
                level = alert_fatal;
        }
    }
    if (level == alert_fatal) {
        if (!ss->opt.noCache) {
            ss->sec.uncache(ss->sec.ci.sid);
        }
        if ((ss->ssl3.hs.ws == wait_server_hello) &&
            (desc == handshake_failure)) {
            /* XXX This is a hack.  We're assuming that any handshake failure
             * XXX on the client hello is a failure to match ciphers.
             */
            error = SSL_ERROR_NO_CYPHER_OVERLAP;
        }
        PORT_SetError(error);
        return SECFailure;
    }
    if (desc == end_of_early_data) {
        return tls13_HandleEndOfEarlyData(ss);
    }
    if ((desc == no_certificate) && (ss->ssl3.hs.ws == wait_client_cert)) {
        /* I'm a server. I've requested a client cert. He hasn't got one. */
        SECStatus rv;

        PORT_Assert(ss->sec.isServer);
        ss->ssl3.hs.ws = wait_client_key;
        rv = ssl3_HandleNoCertificate(ss);
        return rv;
    }
    return SECSuccess;
}

/*
 * Change Cipher Specs
 * Called from ssl3_HandleServerHelloDone,
 *             ssl3_HandleClientHello,
 * and         ssl3_HandleFinished
 *
 * Acquires and releases spec write lock, to protect switching the current
 * and pending write spec pointers.
 */

static SECStatus
ssl3_SendChangeCipherSpecs(sslSocket *ss)
{
    PRUint8 change = change_cipher_spec_choice;
    ssl3CipherSpec *pwSpec;
    SECStatus rv;
    PRInt32 sent;

    SSL_TRC(3, ("%d: SSL3[%d]: send change_cipher_spec record",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    rv = ssl3_FlushHandshake(ss, ssl_SEND_FLAG_FORCE_INTO_BUFFER);
    if (rv != SECSuccess) {
        return rv; /* error code set by ssl3_FlushHandshake */
    }
    if (!IS_DTLS(ss)) {
        sent = ssl3_SendRecord(ss, NULL, content_change_cipher_spec, &change, 1,
                               ssl_SEND_FLAG_FORCE_INTO_BUFFER);
        if (sent < 0) {
            return (SECStatus)sent; /* error code set by ssl3_SendRecord */
        }
    } else {
        rv = dtls_QueueMessage(ss, content_change_cipher_spec, &change, 1);
        if (rv != SECSuccess) {
            return rv;
        }
    }

    /* swap the pending and current write specs. */
    ssl_GetSpecWriteLock(ss); /**************************************/
    pwSpec = ss->ssl3.pwSpec;

    ss->ssl3.pwSpec = ss->ssl3.cwSpec;
    ss->ssl3.cwSpec = pwSpec;

    SSL_TRC(3, ("%d: SSL3[%d] Set Current Write Cipher Suite to Pending",
                SSL_GETPID(), ss->fd));

    /* We need to free up the contexts, keys and certs ! */
    /* If we are really through with the old cipher spec
     * (Both the read and write sides have changed) destroy it.
     */
    if (ss->ssl3.prSpec == ss->ssl3.pwSpec) {
        if (!IS_DTLS(ss)) {
            ssl3_DestroyCipherSpec(ss->ssl3.pwSpec, PR_FALSE /*freeSrvName*/);
        } else {
            /* With DTLS, we need to set a holddown timer in case the final
             * message got lost */
            rv = dtls_StartHolddownTimer(ss);
        }
    }
    ssl_ReleaseSpecWriteLock(ss); /**************************************/

    return rv;
}

/* Called from ssl3_HandleRecord.
** Caller must hold both RecvBuf and Handshake locks.
 *
 * Acquires and releases spec write lock, to protect switching the current
 * and pending write spec pointers.
*/
static SECStatus
ssl3_HandleChangeCipherSpecs(sslSocket *ss, sslBuffer *buf)
{
    ssl3CipherSpec *prSpec;
    SSL3WaitState ws = ss->ssl3.hs.ws;
    SSL3ChangeCipherSpecChoice change;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    SSL_TRC(3, ("%d: SSL3[%d]: handle change_cipher_spec record",
                SSL_GETPID(), ss->fd));

    if (ws != wait_change_cipher) {
        if (IS_DTLS(ss)) {
            /* Ignore this because it's out of order. */
            SSL_TRC(3, ("%d: SSL3[%d]: discard out of order "
                        "DTLS change_cipher_spec",
                        SSL_GETPID(), ss->fd));
            buf->len = 0;
            return SECSuccess;
        }
        (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER);
        return SECFailure;
    }
    /* Handshake messages should not span ChangeCipherSpec. */
    if (ss->ssl3.hs.header_bytes) {
        (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER);
        return SECFailure;
    }
    if (buf->len != 1) {
        (void)ssl3_DecodeError(ss);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER);
        return SECFailure;
    }
    change = (SSL3ChangeCipherSpecChoice)buf->buf[0];
    if (change != change_cipher_spec_choice) {
        /* illegal_parameter is correct here for both SSL3 and TLS. */
        (void)ssl3_IllegalParameter(ss);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER);
        return SECFailure;
    }
    buf->len = 0;

    /* Swap the pending and current read specs. */
    ssl_GetSpecWriteLock(ss); /*************************************/
    prSpec = ss->ssl3.prSpec;

    ss->ssl3.prSpec = ss->ssl3.crSpec;
    ss->ssl3.crSpec = prSpec;
    ss->ssl3.hs.ws = wait_finished;

    SSL_TRC(3, ("%d: SSL3[%d] Set Current Read Cipher Suite to Pending",
                SSL_GETPID(), ss->fd));

    /* If we are really through with the old cipher prSpec
     * (Both the read and write sides have changed) destroy it.
     */
    if (ss->ssl3.prSpec == ss->ssl3.pwSpec) {
        ssl3_DestroyCipherSpec(ss->ssl3.prSpec, PR_FALSE /*freeSrvName*/);
    }
    ssl_ReleaseSpecWriteLock(ss); /*************************************/
    return SECSuccess;
}

static CK_MECHANISM_TYPE
ssl3_GetMgfMechanismByHashType(SSLHashType hash)
{
    switch (hash) {
        case ssl_hash_sha256:
            return CKG_MGF1_SHA256;
        case ssl_hash_sha384:
            return CKG_MGF1_SHA384;
        case ssl_hash_sha512:
            return CKG_MGF1_SHA512;
        default:
            PORT_Assert(0);
    }
    return CKG_MGF1_SHA256;
}

/* Function valid for >= TLS 1.2, only. */
static CK_MECHANISM_TYPE
ssl3_GetHashMechanismByHashType(SSLHashType hashType)
{
    switch (hashType) {
        case ssl_hash_sha512:
            return CKM_SHA512;
        case ssl_hash_sha384:
            return CKM_SHA384;
        case ssl_hash_sha256:
        case ssl_hash_none:
            /* ssl_hash_none is for pre-1.2 suites, which use SHA-256. */
            return CKM_SHA256;
        case ssl_hash_sha1:
            return CKM_SHA_1;
        default:
            PORT_Assert(0);
    }
    return CKM_SHA256;
}

/* Function valid for >= TLS 1.2, only. */
static CK_MECHANISM_TYPE
ssl3_GetPrfHashMechanism(sslSocket *ss)
{
    return ssl3_GetHashMechanismByHashType(ss->ssl3.hs.suite_def->prf_hash);
}

static SSLHashType
ssl3_GetSuitePrfHash(sslSocket *ss)
{
    /* ssl_hash_none is for pre-1.2 suites, which use SHA-256. */
    if (ss->ssl3.hs.suite_def->prf_hash == ssl_hash_none) {
        return ssl_hash_sha256;
    }
    return ss->ssl3.hs.suite_def->prf_hash;
}

/* This method completes the derivation of the MS from the PMS.
**
** 1. Derive the MS, if possible, else return an error.
**
** 2. Check the version if |pms_version| is non-zero and if wrong,
**    return an error.
**
** 3. If |msp| is nonzero, return MS in |*msp|.

** Called from:
**   ssl3_ComputeMasterSecretInt
**   tls_ComputeExtendedMasterSecretInt
*/
static SECStatus
ssl3_ComputeMasterSecretFinish(sslSocket *ss,
                               CK_MECHANISM_TYPE master_derive,
                               CK_MECHANISM_TYPE key_derive,
                               CK_VERSION *pms_version,
                               SECItem *params, CK_FLAGS keyFlags,
                               PK11SymKey *pms, PK11SymKey **msp)
{
    PK11SymKey *ms = NULL;

    ms = PK11_DeriveWithFlags(pms, master_derive,
                              params, key_derive,
                              CKA_DERIVE, 0, keyFlags);
    if (!ms) {
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }

    if (pms_version && ss->opt.detectRollBack) {
        SSL3ProtocolVersion client_version;
        client_version = pms_version->major << 8 | pms_version->minor;

        if (IS_DTLS(ss)) {
            client_version = dtls_DTLSVersionToTLSVersion(client_version);
        }

        if (client_version != ss->clientHelloVersion) {
            /* Destroy MS.  Version roll-back detected. */
            PK11_FreeSymKey(ms);
            ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
            return SECFailure;
        }
    }

    if (msp) {
        *msp = ms;
    } else {
        PK11_FreeSymKey(ms);
    }

    return SECSuccess;
}

/*  Compute the ordinary (pre draft-ietf-tls-session-hash) master
 ** secret and return it in |*msp|.
 **
 ** Called from: ssl3_ComputeMasterSecret
 */
static SECStatus
ssl3_ComputeMasterSecretInt(sslSocket *ss, PK11SymKey *pms,
                            PK11SymKey **msp)
{
    ssl3CipherSpec *pwSpec = ss->ssl3.pwSpec;
    unsigned char *cr = (unsigned char *)&ss->ssl3.hs.client_random;
    unsigned char *sr = (unsigned char *)&ss->ssl3.hs.server_random;
    PRBool isTLS = (PRBool)(pwSpec->version > SSL_LIBRARY_VERSION_3_0);
    PRBool isTLS12 =
        (PRBool)(isTLS && pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2);
    /*
     * Whenever isDH is true, we need to use CKM_TLS_MASTER_KEY_DERIVE_DH
     * which, unlike CKM_TLS_MASTER_KEY_DERIVE, converts arbitrary size
     * data into a 48-byte value, and does not expect to return the version.
     */
    PRBool isDH = (PRBool)((ss->ssl3.hs.kea_def->exchKeyType == ssl_kea_dh) ||
                           (ss->ssl3.hs.kea_def->exchKeyType == ssl_kea_ecdh));
    CK_MECHANISM_TYPE master_derive;
    CK_MECHANISM_TYPE key_derive;
    SECItem params;
    CK_FLAGS keyFlags;
    CK_VERSION pms_version;
    CK_VERSION *pms_version_ptr = NULL;
    /* master_params may be used as a CK_SSL3_MASTER_KEY_DERIVE_PARAMS */
    CK_TLS12_MASTER_KEY_DERIVE_PARAMS master_params;
    unsigned int master_params_len;

    if (isTLS12) {
        if (isDH)
            master_derive = CKM_TLS12_MASTER_KEY_DERIVE_DH;
        else
            master_derive = CKM_TLS12_MASTER_KEY_DERIVE;
        key_derive = CKM_TLS12_KEY_AND_MAC_DERIVE;
        keyFlags = CKF_SIGN | CKF_VERIFY;
    } else if (isTLS) {
        if (isDH)
            master_derive = CKM_TLS_MASTER_KEY_DERIVE_DH;
        else
            master_derive = CKM_TLS_MASTER_KEY_DERIVE;
        key_derive = CKM_TLS_KEY_AND_MAC_DERIVE;
        keyFlags = CKF_SIGN | CKF_VERIFY;
    } else {
        if (isDH)
            master_derive = CKM_SSL3_MASTER_KEY_DERIVE_DH;
        else
            master_derive = CKM_SSL3_MASTER_KEY_DERIVE;
        key_derive = CKM_SSL3_KEY_AND_MAC_DERIVE;
        keyFlags = 0;
    }

    if (!isDH) {
        pms_version_ptr = &pms_version;
    }

    master_params.pVersion = pms_version_ptr;
    master_params.RandomInfo.pClientRandom = cr;
    master_params.RandomInfo.ulClientRandomLen = SSL3_RANDOM_LENGTH;
    master_params.RandomInfo.pServerRandom = sr;
    master_params.RandomInfo.ulServerRandomLen = SSL3_RANDOM_LENGTH;
    if (isTLS12) {
        master_params.prfHashMechanism = ssl3_GetPrfHashMechanism(ss);
        master_params_len = sizeof(CK_TLS12_MASTER_KEY_DERIVE_PARAMS);
    } else {
        /* prfHashMechanism is not relevant with this PRF */
        master_params_len = sizeof(CK_SSL3_MASTER_KEY_DERIVE_PARAMS);
    }

    params.data = (unsigned char *)&master_params;
    params.len = master_params_len;

    return ssl3_ComputeMasterSecretFinish(ss, master_derive, key_derive,
                                          pms_version_ptr, &params,
                                          keyFlags, pms, msp);
}

/* Compute the draft-ietf-tls-session-hash master
** secret and return it in |*msp|.
**
** Called from: ssl3_ComputeMasterSecret
*/
static SECStatus
tls_ComputeExtendedMasterSecretInt(sslSocket *ss, PK11SymKey *pms,
                                   PK11SymKey **msp)
{
    ssl3CipherSpec *pwSpec = ss->ssl3.pwSpec;
    CK_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE_PARAMS extended_master_params;
    SSL3Hashes hashes;
    /*
     * Determine whether to use the DH/ECDH or RSA derivation modes.
     */
    /*
     * TODO(ekr@rtfm.com): Verify that the slot can handle this key expansion
     * mode. Bug 1198298 */
    PRBool isDH = (PRBool)((ss->ssl3.hs.kea_def->exchKeyType == ssl_kea_dh) ||
                           (ss->ssl3.hs.kea_def->exchKeyType == ssl_kea_ecdh));
    CK_MECHANISM_TYPE master_derive;
    CK_MECHANISM_TYPE key_derive;
    SECItem params;
    const CK_FLAGS keyFlags = CKF_SIGN | CKF_VERIFY;
    CK_VERSION pms_version;
    CK_VERSION *pms_version_ptr = NULL;
    SECStatus rv;

    rv = ssl3_ComputeHandshakeHashes(ss, pwSpec, &hashes, 0);
    if (rv != SECSuccess) {
        PORT_Assert(0); /* Should never fail */
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }

    if (isDH) {
        master_derive = CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE_DH;
    } else {
        master_derive = CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE;
        pms_version_ptr = &pms_version;
    }

    if (pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2) {
        /* TLS 1.2+ */
        extended_master_params.prfHashMechanism = ssl3_GetPrfHashMechanism(ss);
        key_derive = CKM_TLS12_KEY_AND_MAC_DERIVE;
    } else {
        /* TLS < 1.2 */
        extended_master_params.prfHashMechanism = CKM_TLS_PRF;
        key_derive = CKM_TLS_KEY_AND_MAC_DERIVE;
    }

    extended_master_params.pVersion = pms_version_ptr;
    extended_master_params.pSessionHash = hashes.u.raw;
    extended_master_params.ulSessionHashLen = hashes.len;

    params.data = (unsigned char *)&extended_master_params;
    params.len = sizeof extended_master_params;

    return ssl3_ComputeMasterSecretFinish(ss, master_derive, key_derive,
                                          pms_version_ptr, &params,
                                          keyFlags, pms, msp);
}

/* Wrapper method to compute the master secret and return it in |*msp|.
**
** Called from ssl3_ComputeMasterSecret
*/
static SECStatus
ssl3_ComputeMasterSecret(sslSocket *ss, PK11SymKey *pms,
                         PK11SymKey **msp)
{
    PORT_Assert(pms != NULL);
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    if (ssl3_ExtensionNegotiated(ss, ssl_extended_master_secret_xtn)) {
        return tls_ComputeExtendedMasterSecretInt(ss, pms, msp);
    } else {
        return ssl3_ComputeMasterSecretInt(ss, pms, msp);
    }
}

/* This method uses PKCS11 to derive the MS from the PMS, where PMS
** is a PKCS11 symkey. We call ssl3_ComputeMasterSecret to do the
** computations and then modify the pwSpec->state as a side effect.
**
** This is used in all cases except the "triple bypass" with RSA key
** exchange.
**
** Called from ssl3_InitPendingCipherSpec.   prSpec is pwSpec.
*/
static SECStatus
ssl3_DeriveMasterSecret(sslSocket *ss, PK11SymKey *pms)
{
    SECStatus rv;
    PK11SymKey *ms = NULL;
    ssl3CipherSpec *pwSpec = ss->ssl3.pwSpec;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSpecWriteLock(ss));
    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    if (pms) {
        rv = ssl3_ComputeMasterSecret(ss, pms, &ms);
        pwSpec->master_secret = ms;
        if (rv != SECSuccess)
            return rv;
    }

    return SECSuccess;
}

/*
 * Derive encryption and MAC Keys (and IVs) from master secret
 * Sets a useful error code when returning SECFailure.
 *
 * Called only from ssl3_InitPendingCipherSpec(),
 * which in turn is called from
 *              ssl3_SendRSAClientKeyExchange    (for Full handshake)
 *              ssl3_SendDHClientKeyExchange     (for Full handshake)
 *              ssl3_HandleClientKeyExchange    (for Full handshake)
 *              ssl3_HandleServerHello          (for session restart)
 *              ssl3_HandleClientHello          (for session restart)
 * Caller MUST hold the specWriteLock, and SSL3HandshakeLock.
 * ssl3_InitPendingCipherSpec does that.
 *
 */
static SECStatus
ssl3_DeriveConnectionKeys(sslSocket *ss)
{
    ssl3CipherSpec *pwSpec = ss->ssl3.pwSpec;
    unsigned char *cr = (unsigned char *)&ss->ssl3.hs.client_random;
    unsigned char *sr = (unsigned char *)&ss->ssl3.hs.server_random;
    PRBool isTLS = (PRBool)(pwSpec->version > SSL_LIBRARY_VERSION_3_0);
    PRBool isTLS12 =
        (PRBool)(isTLS && pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2);
    const ssl3BulkCipherDef *cipher_def = pwSpec->cipher_def;
    PK11SlotInfo *slot = NULL;
    PK11SymKey *symKey = NULL;
    void *pwArg = ss->pkcs11PinArg;
    int keySize;
    CK_TLS12_KEY_MAT_PARAMS key_material_params; /* may be used as a
                                                  * CK_SSL3_KEY_MAT_PARAMS */
    unsigned int key_material_params_len;
    CK_SSL3_KEY_MAT_OUT returnedKeys;
    CK_MECHANISM_TYPE key_derive;
    CK_MECHANISM_TYPE bulk_mechanism;
    SSLCipherAlgorithm calg;
    SECItem params;
    PRBool skipKeysAndIVs = (PRBool)(cipher_def->calg == calg_null);

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSpecWriteLock(ss));
    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    if (!pwSpec->master_secret) {
        PORT_SetError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }
    /*
     * generate the key material
     */
    key_material_params.ulMacSizeInBits = pwSpec->mac_size * BPB;
    key_material_params.ulKeySizeInBits = cipher_def->secret_key_size * BPB;
    key_material_params.ulIVSizeInBits = cipher_def->iv_size * BPB;
    if (cipher_def->type == type_block &&
        pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_1) {
        /* Block ciphers in >= TLS 1.1 use a per-record, explicit IV. */
        key_material_params.ulIVSizeInBits = 0;
        memset(pwSpec->client.write_iv, 0, cipher_def->iv_size);
        memset(pwSpec->server.write_iv, 0, cipher_def->iv_size);
    }

    key_material_params.bIsExport = PR_FALSE;
    key_material_params.RandomInfo.pClientRandom = cr;
    key_material_params.RandomInfo.ulClientRandomLen = SSL3_RANDOM_LENGTH;
    key_material_params.RandomInfo.pServerRandom = sr;
    key_material_params.RandomInfo.ulServerRandomLen = SSL3_RANDOM_LENGTH;
    key_material_params.pReturnedKeyMaterial = &returnedKeys;

    returnedKeys.pIVClient = pwSpec->client.write_iv;
    returnedKeys.pIVServer = pwSpec->server.write_iv;
    keySize = cipher_def->key_size;

    if (skipKeysAndIVs) {
        keySize = 0;
        key_material_params.ulKeySizeInBits = 0;
        key_material_params.ulIVSizeInBits = 0;
        returnedKeys.pIVClient = NULL;
        returnedKeys.pIVServer = NULL;
    }

    calg = cipher_def->calg;
    bulk_mechanism = ssl3_Alg2Mech(calg);

    if (isTLS12) {
        key_derive = CKM_TLS12_KEY_AND_MAC_DERIVE;
        key_material_params.prfHashMechanism = ssl3_GetPrfHashMechanism(ss);
        key_material_params_len = sizeof(CK_TLS12_KEY_MAT_PARAMS);
    } else if (isTLS) {
        key_derive = CKM_TLS_KEY_AND_MAC_DERIVE;
        key_material_params_len = sizeof(CK_SSL3_KEY_MAT_PARAMS);
    } else {
        key_derive = CKM_SSL3_KEY_AND_MAC_DERIVE;
        key_material_params_len = sizeof(CK_SSL3_KEY_MAT_PARAMS);
    }

    params.data = (unsigned char *)&key_material_params;
    params.len = key_material_params_len;

    /* CKM_SSL3_KEY_AND_MAC_DERIVE is defined to set ENCRYPT, DECRYPT, and
     * DERIVE by DEFAULT */
    symKey = PK11_Derive(pwSpec->master_secret, key_derive, &params,
                         bulk_mechanism, CKA_ENCRYPT, keySize);
    if (!symKey) {
        ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
        return SECFailure;
    }
    /* we really should use the actual mac'ing mechanism here, but we
     * don't because these types are used to map keytype anyway and both
     * mac's map to the same keytype.
     */
    slot = PK11_GetSlotFromKey(symKey);

    PK11_FreeSlot(slot); /* slot is held until the key is freed */
    pwSpec->client.write_mac_key =
        PK11_SymKeyFromHandle(slot, symKey, PK11_OriginDerive,
                              CKM_SSL3_SHA1_MAC, returnedKeys.hClientMacSecret, PR_TRUE, pwArg);
    if (pwSpec->client.write_mac_key == NULL) {
        goto loser; /* loser sets err */
    }
    pwSpec->server.write_mac_key =
        PK11_SymKeyFromHandle(slot, symKey, PK11_OriginDerive,
                              CKM_SSL3_SHA1_MAC, returnedKeys.hServerMacSecret, PR_TRUE, pwArg);
    if (pwSpec->server.write_mac_key == NULL) {
        goto loser; /* loser sets err */
    }
    if (!skipKeysAndIVs) {
        pwSpec->client.write_key =
            PK11_SymKeyFromHandle(slot, symKey, PK11_OriginDerive,
                                  bulk_mechanism, returnedKeys.hClientKey, PR_TRUE, pwArg);
        if (pwSpec->client.write_key == NULL) {
            goto loser; /* loser sets err */
        }
        pwSpec->server.write_key =
            PK11_SymKeyFromHandle(slot, symKey, PK11_OriginDerive,
                                  bulk_mechanism, returnedKeys.hServerKey, PR_TRUE, pwArg);
        if (pwSpec->server.write_key == NULL) {
            goto loser; /* loser sets err */
        }
    }
    PK11_FreeSymKey(symKey);
    return SECSuccess;

loser:
    if (symKey)
        PK11_FreeSymKey(symKey);
    ssl_MapLowLevelError(SSL_ERROR_SESSION_KEY_GEN_FAILURE);
    return SECFailure;
}

/* ssl3_InitHandshakeHashes creates handshake hash contexts and hashes in
 * buffered messages in ss->ssl3.hs.messages. Called from
 * ssl3_NegotiateCipherSuite(), tls13_HandleClientHelloPart2(),
 * and ssl3_HandleServerHello. */
SECStatus
ssl3_InitHandshakeHashes(sslSocket *ss)
{
    SSL_TRC(30, ("%d: SSL3[%d]: start handshake hashes", SSL_GETPID(), ss->fd));

    PORT_Assert(ss->ssl3.hs.hashType == handshake_hash_unknown);
    if (ss->version == SSL_LIBRARY_VERSION_TLS_1_2) {
        ss->ssl3.hs.hashType = handshake_hash_record;
    } else {
        PORT_Assert(!ss->ssl3.hs.md5 && !ss->ssl3.hs.sha);
        /*
         * note: We should probably lookup an SSL3 slot for these
         * handshake hashes in hopes that we wind up with the same slots
         * that the master secret will wind up in ...
         */
        if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
            /* determine the hash from the prf */
            const SECOidData *hash_oid =
                SECOID_FindOIDByMechanism(ssl3_GetPrfHashMechanism(ss));

            /* Get the PKCS #11 mechanism for the Hash from the cipher suite (prf_hash)
             * Convert that to the OidTag. We can then use that OidTag to create our
             * PK11Context */
            PORT_Assert(hash_oid != NULL);
            if (hash_oid == NULL) {
                ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
                return SECFailure;
            }

            ss->ssl3.hs.sha = PK11_CreateDigestContext(hash_oid->offset);
            if (ss->ssl3.hs.sha == NULL) {
                ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
                return SECFailure;
            }
            ss->ssl3.hs.hashType = handshake_hash_single;

            if (PK11_DigestBegin(ss->ssl3.hs.sha) != SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
                return SECFailure;
            }
        } else {
            /* Both ss->ssl3.hs.md5 and ss->ssl3.hs.sha should be NULL or
             * created successfully. */
            ss->ssl3.hs.md5 = PK11_CreateDigestContext(SEC_OID_MD5);
            if (ss->ssl3.hs.md5 == NULL) {
                ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
                return SECFailure;
            }
            ss->ssl3.hs.sha = PK11_CreateDigestContext(SEC_OID_SHA1);
            if (ss->ssl3.hs.sha == NULL) {
                PK11_DestroyContext(ss->ssl3.hs.md5, PR_TRUE);
                ss->ssl3.hs.md5 = NULL;
                ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
                return SECFailure;
            }
            ss->ssl3.hs.hashType = handshake_hash_combo;

            if (PK11_DigestBegin(ss->ssl3.hs.md5) != SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
                return SECFailure;
            }
            if (PK11_DigestBegin(ss->ssl3.hs.sha) != SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
                return SECFailure;
            }
        }
    }

    if (ss->ssl3.hs.hashType != handshake_hash_record &&
        ss->ssl3.hs.messages.len > 0) {
        if (ssl3_UpdateHandshakeHashes(ss, ss->ssl3.hs.messages.buf,
                                       ss->ssl3.hs.messages.len) != SECSuccess) {
            return SECFailure;
        }
        sslBuffer_Clear(&ss->ssl3.hs.messages);
    }

    return SECSuccess;
}

SECStatus
ssl3_RestartHandshakeHashes(sslSocket *ss)
{
    SECStatus rv = SECSuccess;

    SSL_TRC(30, ("%d: SSL3[%d]: reset handshake hashes",
                 SSL_GETPID(), ss->fd));
    ss->ssl3.hs.hashType = handshake_hash_unknown;
    ss->ssl3.hs.messages.len = 0;
    if (ss->ssl3.hs.md5) {
        PK11_DestroyContext(ss->ssl3.hs.md5, PR_TRUE);
        ss->ssl3.hs.md5 = NULL;
    }
    if (ss->ssl3.hs.sha) {
        PK11_DestroyContext(ss->ssl3.hs.sha, PR_TRUE);
        ss->ssl3.hs.sha = NULL;
    }
    return rv;
}

/*
 * Handshake messages
 */
/* Called from  ssl3_InitHandshakeHashes()
**      ssl3_AppendHandshake()
**      ssl3_HandleV2ClientHello()
**      ssl3_HandleHandshakeMessage()
** Caller must hold the ssl3Handshake lock.
*/
SECStatus
ssl3_UpdateHandshakeHashes(sslSocket *ss, const unsigned char *b, unsigned int l)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    /* With TLS 1.3, and versions TLS.1.1 and older, we keep the hash(es)
     * always up to date. However, we must initially buffer the handshake
     * messages, until we know what to do.
     * If ss->ssl3.hs.hashType != handshake_hash_unknown,
     * it means we know what to do. We calculate (hash our input),
     * and we stop appending to the buffer.
     *
     * With TLS 1.2, we always append all handshake messages,
     * and never update the hash, because the hash function we must use for
     * certificate_verify might be different from the hash function we use
     * when signing other handshake hashes. */

    if (ss->ssl3.hs.hashType == handshake_hash_unknown ||
        ss->ssl3.hs.hashType == handshake_hash_record) {
        return sslBuffer_Append(&ss->ssl3.hs.messages, b, l);
    }

    PRINT_BUF(90, (NULL, "handshake hash input:", b, l));

    if (ss->ssl3.hs.hashType == handshake_hash_single) {
        PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
        rv = PK11_DigestOp(ss->ssl3.hs.sha, b, l);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
            return rv;
        }
    } else if (ss->ssl3.hs.hashType == handshake_hash_combo) {
        rv = PK11_DigestOp(ss->ssl3.hs.md5, b, l);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
            return rv;
        }
        rv = PK11_DigestOp(ss->ssl3.hs.sha, b, l);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
            return rv;
        }
    }
    return rv;
}

/**************************************************************************
 * Append Handshake functions.
 * All these functions set appropriate error codes.
 * Most rely on ssl3_AppendHandshake to set the error code.
 **************************************************************************/
SECStatus
ssl3_AppendHandshake(sslSocket *ss, const void *void_src, PRInt32 bytes)
{
    unsigned char *src = (unsigned char *)void_src;
    int room = ss->sec.ci.sendBuf.space - ss->sec.ci.sendBuf.len;
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss)); /* protects sendBuf. */

    if (!bytes)
        return SECSuccess;
    if (ss->sec.ci.sendBuf.space < MAX_SEND_BUF_LENGTH && room < bytes) {
        rv = sslBuffer_Grow(&ss->sec.ci.sendBuf, PR_MAX(MIN_SEND_BUF_LENGTH,
                                                        PR_MIN(MAX_SEND_BUF_LENGTH, ss->sec.ci.sendBuf.len + bytes)));
        if (rv != SECSuccess)
            return rv; /* sslBuffer_Grow has set a memory error code. */
        room = ss->sec.ci.sendBuf.space - ss->sec.ci.sendBuf.len;
    }

    PRINT_BUF(60, (ss, "Append to Handshake", (unsigned char *)void_src, bytes));
    rv = ssl3_UpdateHandshakeHashes(ss, src, bytes);
    if (rv != SECSuccess)
        return rv; /* error code set by ssl3_UpdateHandshakeHashes */

    while (bytes > room) {
        if (room > 0)
            PORT_Memcpy(ss->sec.ci.sendBuf.buf + ss->sec.ci.sendBuf.len, src,
                        room);
        ss->sec.ci.sendBuf.len += room;
        rv = ssl3_FlushHandshake(ss, ssl_SEND_FLAG_FORCE_INTO_BUFFER);
        if (rv != SECSuccess) {
            return rv; /* error code set by ssl3_FlushHandshake */
        }
        bytes -= room;
        src += room;
        room = ss->sec.ci.sendBuf.space;
        PORT_Assert(ss->sec.ci.sendBuf.len == 0);
    }
    PORT_Memcpy(ss->sec.ci.sendBuf.buf + ss->sec.ci.sendBuf.len, src, bytes);
    ss->sec.ci.sendBuf.len += bytes;
    return SECSuccess;
}

SECStatus
ssl3_AppendHandshakeNumber(sslSocket *ss, PRInt32 num, PRInt32 lenSize)
{
    SECStatus rv;
    PRUint8 b[4];
    PRUint8 *p = b;

    PORT_Assert(lenSize <= 4 && lenSize > 0);
    if (lenSize < 4 && num >= (1L << (lenSize * 8))) {
        PORT_SetError(SSL_ERROR_TX_RECORD_TOO_LONG);
        return SECFailure;
    }

    switch (lenSize) {
        case 4:
            *p++ = (num >> 24) & 0xff;
        case 3:
            *p++ = (num >> 16) & 0xff;
        case 2:
            *p++ = (num >> 8) & 0xff;
        case 1:
            *p = num & 0xff;
    }
    SSL_TRC(60, ("%d: number:", SSL_GETPID()));
    rv = ssl3_AppendHandshake(ss, &b[0], lenSize);
    return rv; /* error code set by AppendHandshake, if applicable. */
}

SECStatus
ssl3_AppendHandshakeVariable(
    sslSocket *ss, const SSL3Opaque *src, PRInt32 bytes, PRInt32 lenSize)
{
    SECStatus rv;

    PORT_Assert((bytes < (1 << 8) && lenSize == 1) ||
                (bytes < (1L << 16) && lenSize == 2) ||
                (bytes < (1L << 24) && lenSize == 3));

    SSL_TRC(60, ("%d: append variable:", SSL_GETPID()));
    rv = ssl3_AppendHandshakeNumber(ss, bytes, lenSize);
    if (rv != SECSuccess) {
        return rv; /* error code set by AppendHandshake, if applicable. */
    }
    SSL_TRC(60, ("data:"));
    rv = ssl3_AppendHandshake(ss, src, bytes);
    return rv; /* error code set by AppendHandshake, if applicable. */
}

SECStatus
ssl3_AppendHandshakeHeader(sslSocket *ss, SSL3HandshakeType t, PRUint32 length)
{
    SECStatus rv;

    /* If we already have a message in place, we need to enqueue it.
     * This empties the buffer. This is a convenient place to call
     * dtls_StageHandshakeMessage to mark the message boundary.
     */
    if (IS_DTLS(ss)) {
        rv = dtls_StageHandshakeMessage(ss);
        if (rv != SECSuccess) {
            return rv;
        }
    }

    SSL_TRC(30, ("%d: SSL3[%d]: append handshake header: type %s",
                 SSL_GETPID(), ss->fd, ssl3_DecodeHandshakeType(t)));

    rv = ssl3_AppendHandshakeNumber(ss, t, 1);
    if (rv != SECSuccess) {
        return rv; /* error code set by AppendHandshake, if applicable. */
    }
    rv = ssl3_AppendHandshakeNumber(ss, length, 3);
    if (rv != SECSuccess) {
        return rv; /* error code set by AppendHandshake, if applicable. */
    }

    if (IS_DTLS(ss)) {
        /* Note that we make an unfragmented message here. We fragment in the
         * transmission code, if necessary */
        rv = ssl3_AppendHandshakeNumber(ss, ss->ssl3.hs.sendMessageSeq, 2);
        if (rv != SECSuccess) {
            return rv; /* error code set by AppendHandshake, if applicable. */
        }
        ss->ssl3.hs.sendMessageSeq++;

        /* 0 is the fragment offset, because it's not fragmented yet */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 3);
        if (rv != SECSuccess) {
            return rv; /* error code set by AppendHandshake, if applicable. */
        }

        /* Fragment length -- set to the packet length because not fragmented */
        rv = ssl3_AppendHandshakeNumber(ss, length, 3);
        if (rv != SECSuccess) {
            return rv; /* error code set by AppendHandshake, if applicable. */
        }
    }

    return rv; /* error code set by AppendHandshake, if applicable. */
}

/**************************************************************************
 * Consume Handshake functions.
 *
 * All data used in these functions is protected by two locks,
 * the RecvBufLock and the SSL3HandshakeLock
 **************************************************************************/

/* Read up the next "bytes" number of bytes from the (decrypted) input
 * stream "b" (which is *length bytes long). Copy them into buffer "v".
 * Reduces *length by bytes.  Advances *b by bytes.
 *
 * If this function returns SECFailure, it has already sent an alert,
 * and has set a generic error code.  The caller should probably
 * override the generic error code by setting another.
 */
SECStatus
ssl3_ConsumeHandshake(sslSocket *ss, void *v, PRInt32 bytes, SSL3Opaque **b,
                      PRUint32 *length)
{
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if ((PRUint32)bytes > *length) {
        return ssl3_DecodeError(ss);
    }
    PORT_Memcpy(v, *b, bytes);
    PRINT_BUF(60, (ss, "consume bytes:", *b, bytes));
    *b += bytes;
    *length -= bytes;
    return SECSuccess;
}

/* Read up the next "bytes" number of bytes from the (decrypted) input
 * stream "b" (which is *length bytes long), and interpret them as an
 * integer in network byte order.  Returns the received value.
 * Reduces *length by bytes.  Advances *b by bytes.
 *
 * Returns SECFailure (-1) on failure.
 * This value is indistinguishable from the equivalent received value.
 * Only positive numbers are to be received this way.
 * Thus, the largest value that may be sent this way is 0x7fffffff.
 * On error, an alert has been sent, and a generic error code has been set.
 */
PRInt32
ssl3_ConsumeHandshakeNumber(sslSocket *ss, PRInt32 bytes, SSL3Opaque **b,
                            PRUint32 *length)
{
    PRUint8 *buf = *b;
    int i;
    PRInt32 num = 0;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(bytes <= sizeof num);

    if ((PRUint32)bytes > *length) {
        return ssl3_DecodeError(ss);
    }
    PRINT_BUF(60, (ss, "consume bytes:", *b, bytes));

    for (i = 0; i < bytes; i++)
        num = (num << 8) + buf[i];
    *b += bytes;
    *length -= bytes;
    return num;
}

/* Read in two values from the incoming decrypted byte stream "b", which is
 * *length bytes long.  The first value is a number whose size is "bytes"
 * bytes long.  The second value is a byte-string whose size is the value
 * of the first number received.  The latter byte-string, and its length,
 * is returned in the SECItem i.
 *
 * Returns SECFailure (-1) on failure.
 * On error, an alert has been sent, and a generic error code has been set.
 *
 * RADICAL CHANGE for NSS 3.11.  All callers of this function make copies
 * of the data returned in the SECItem *i, so making a copy of it here
 * is simply wasteful.  So, This function now just sets SECItem *i to
 * point to the values in the buffer **b.
 */
SECStatus
ssl3_ConsumeHandshakeVariable(sslSocket *ss, SECItem *i, PRInt32 bytes,
                              SSL3Opaque **b, PRUint32 *length)
{
    PRInt32 count;

    PORT_Assert(bytes <= 3);
    i->len = 0;
    i->data = NULL;
    i->type = siBuffer;
    count = ssl3_ConsumeHandshakeNumber(ss, bytes, b, length);
    if (count < 0) { /* Can't test for SECSuccess here. */
        return SECFailure;
    }
    if (count > 0) {
        if ((PRUint32)count > *length) {
            return ssl3_DecodeError(ss);
        }
        i->data = *b;
        i->len = count;
        *b += count;
        *length -= count;
    }
    return SECSuccess;
}

/* Helper function to encode an unsigned integer into a buffer. */
PRUint8 *
ssl_EncodeUintX(PRUint64 value, unsigned int bytes, PRUint8 *to)
{
    PRUint64 encoded;

    PORT_Assert(bytes > 0 && bytes <= sizeof(encoded));

    encoded = PR_htonll(value);
    memcpy(to, ((unsigned char *)(&encoded)) + (sizeof(encoded) - bytes), bytes);
    return to + bytes;
}

/* ssl3_TLSHashAlgorithmToOID converts a TLS hash identifier into an OID value.
 * If the hash is not recognised, SEC_OID_UNKNOWN is returned.
 *
 * See https://tools.ietf.org/html/rfc5246#section-7.4.1.4.1 */
SECOidTag
ssl3_HashTypeToOID(SSLHashType hashType)
{
    switch (hashType) {
        case ssl_hash_sha1:
            return SEC_OID_SHA1;
        case ssl_hash_sha256:
            return SEC_OID_SHA256;
        case ssl_hash_sha384:
            return SEC_OID_SHA384;
        case ssl_hash_sha512:
            return SEC_OID_SHA512;
        default:
            break;
    }
    return SEC_OID_UNKNOWN;
}

SSLHashType
ssl_SignatureSchemeToHashType(SSLSignatureScheme scheme)
{
    switch (scheme) {
        case ssl_sig_rsa_pkcs1_sha1:
        case ssl_sig_dsa_sha1:
        case ssl_sig_ecdsa_sha1:
            return ssl_hash_sha1;
        case ssl_sig_rsa_pkcs1_sha256:
        case ssl_sig_ecdsa_secp256r1_sha256:
        case ssl_sig_rsa_pss_sha256:
        case ssl_sig_dsa_sha256:
            return ssl_hash_sha256;
        case ssl_sig_rsa_pkcs1_sha384:
        case ssl_sig_ecdsa_secp384r1_sha384:
        case ssl_sig_rsa_pss_sha384:
        case ssl_sig_dsa_sha384:
            return ssl_hash_sha384;
        case ssl_sig_rsa_pkcs1_sha512:
        case ssl_sig_ecdsa_secp521r1_sha512:
        case ssl_sig_rsa_pss_sha512:
        case ssl_sig_dsa_sha512:
            return ssl_hash_sha512;
        case ssl_sig_rsa_pkcs1_sha1md5:
            return ssl_hash_none; /* Special for TLS 1.0/1.1. */
        case ssl_sig_none:
        case ssl_sig_ed25519:
        case ssl_sig_ed448:
            break;
    }
    PORT_Assert(0);
    return ssl_hash_none;
}

KeyType
ssl_SignatureSchemeToKeyType(SSLSignatureScheme scheme)
{
    switch (scheme) {
        case ssl_sig_rsa_pkcs1_sha256:
        case ssl_sig_rsa_pkcs1_sha384:
        case ssl_sig_rsa_pkcs1_sha512:
        case ssl_sig_rsa_pkcs1_sha1:
        case ssl_sig_rsa_pss_sha256:
        case ssl_sig_rsa_pss_sha384:
        case ssl_sig_rsa_pss_sha512:
        case ssl_sig_rsa_pkcs1_sha1md5:
            return rsaKey;
        case ssl_sig_ecdsa_secp256r1_sha256:
        case ssl_sig_ecdsa_secp384r1_sha384:
        case ssl_sig_ecdsa_secp521r1_sha512:
        case ssl_sig_ecdsa_sha1:
            return ecKey;
        case ssl_sig_dsa_sha256:
        case ssl_sig_dsa_sha384:
        case ssl_sig_dsa_sha512:
        case ssl_sig_dsa_sha1:
            return dsaKey;
        case ssl_sig_none:
        case ssl_sig_ed25519:
        case ssl_sig_ed448:
            break;
    }
    PORT_Assert(0);
    return nullKey;
}

static SSLNamedGroup
ssl_NamedGroupForSignatureScheme(SSLSignatureScheme scheme)
{
    switch (scheme) {
        case ssl_sig_ecdsa_secp256r1_sha256:
            return ssl_grp_ec_secp256r1;
        case ssl_sig_ecdsa_secp384r1_sha384:
            return ssl_grp_ec_secp384r1;
        case ssl_sig_ecdsa_secp521r1_sha512:
            return ssl_grp_ec_secp521r1;
        default:
            break;
    }
    PORT_Assert(0);
    return 0;
}

/* Validate that the signature scheme works for the given key.
 * If |allowSha1| is set, we allow the use of SHA-1.
 * If |matchGroup| is set, we also check that the group and hash match. */
static PRBool
ssl_SignatureSchemeValidForKey(PRBool allowSha1, PRBool matchGroup,
                               KeyType keyType,
                               const sslNamedGroupDef *ecGroup,
                               SSLSignatureScheme scheme)
{
    if (!ssl_IsSupportedSignatureScheme(scheme)) {
        return PR_FALSE;
    }
    if (keyType != ssl_SignatureSchemeToKeyType(scheme)) {
        return PR_FALSE;
    }
    if (!allowSha1 && ssl_SignatureSchemeToHashType(scheme) == ssl_hash_sha1) {
        return PR_FALSE;
    }
    if (keyType != ecKey) {
        return PR_TRUE;
    }
    if (!ecGroup) {
        return PR_FALSE;
    }
    /* If |allowSha1| is present and the scheme is ssl_sig_ecdsa_sha1, it's OK.
     * This scheme isn't bound to a specific group. */
    if (allowSha1 && (scheme == ssl_sig_ecdsa_sha1)) {
        return PR_TRUE;
    }
    if (!matchGroup) {
        return PR_TRUE;
    }
    return ecGroup->name == ssl_NamedGroupForSignatureScheme(scheme);
}

/* ssl3_CheckSignatureSchemeConsistency checks that the signature
 * algorithm identifier in |sigAndHash| is consistent with the public key in
 * |cert|. It also checks the hash algorithm against the configured signature
 * algorithms.  If all the tests pass, SECSuccess is returned. Otherwise,
 * PORT_SetError is called and SECFailure is returned. */
SECStatus
ssl_CheckSignatureSchemeConsistency(
    sslSocket *ss, SSLSignatureScheme scheme, CERTCertificate *cert)
{
    unsigned int i;
    const sslNamedGroupDef *group = NULL;
    SECKEYPublicKey *key;
    KeyType keyType;
    PRBool isTLS13 = ss->version == SSL_LIBRARY_VERSION_TLS_1_3;

    key = CERT_ExtractPublicKey(cert);
    if (key == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE);
        return SECFailure;
    }

    keyType = SECKEY_GetPublicKeyType(key);
    if (keyType == ecKey) {
        group = ssl_ECPubKey2NamedGroup(key);
    }
    SECKEY_DestroyPublicKey(key);

    /* If we're a client, check that the signature algorithm matches the signing
     * key type of the cipher suite. */
    if (!isTLS13 &&
        !ss->sec.isServer &&
        ss->ssl3.hs.kea_def->signKeyType != keyType) {
        PORT_SetError(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM);
        return SECFailure;
    }

    /* Verify that the signature scheme matches the signing key. */
    if (!ssl_SignatureSchemeValidForKey(!isTLS13 /* allowSha1 */,
                                        isTLS13 /* matchGroup */,
                                        keyType, group, scheme)) {
        PORT_SetError(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM);
        return SECFailure;
    }

    for (i = 0; i < ss->ssl3.signatureSchemeCount; ++i) {
        if (scheme == ss->ssl3.signatureSchemes[i]) {
            return SECSuccess;
        }
    }
    PORT_SetError(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
    return SECFailure;
}

PRBool
ssl_IsSupportedSignatureScheme(SSLSignatureScheme scheme)
{
    switch (scheme) {
        case ssl_sig_rsa_pkcs1_sha1:
        case ssl_sig_rsa_pkcs1_sha256:
        case ssl_sig_rsa_pkcs1_sha384:
        case ssl_sig_rsa_pkcs1_sha512:
        case ssl_sig_rsa_pss_sha256:
        case ssl_sig_rsa_pss_sha384:
        case ssl_sig_rsa_pss_sha512:
        case ssl_sig_ecdsa_secp256r1_sha256:
        case ssl_sig_ecdsa_secp384r1_sha384:
        case ssl_sig_ecdsa_secp521r1_sha512:
        case ssl_sig_dsa_sha1:
        case ssl_sig_dsa_sha256:
        case ssl_sig_dsa_sha384:
        case ssl_sig_dsa_sha512:
        case ssl_sig_ecdsa_sha1:
            return PR_TRUE;

        case ssl_sig_rsa_pkcs1_sha1md5:
        case ssl_sig_none:
        case ssl_sig_ed25519:
        case ssl_sig_ed448:
            return PR_FALSE;
    }
    return PR_FALSE;
}

PRBool
ssl_IsRsaPssSignatureScheme(SSLSignatureScheme scheme)
{
    switch (scheme) {
        case ssl_sig_rsa_pss_sha256:
        case ssl_sig_rsa_pss_sha384:
        case ssl_sig_rsa_pss_sha512:
            return PR_TRUE;

        default:
            return PR_FALSE;
    }
    return PR_FALSE;
}

/* ssl_ConsumeSignatureScheme reads a SSLSignatureScheme (formerly
 * SignatureAndHashAlgorithm) structure from |b| and puts the resulting value
 * into |out|. |b| and |length| are updated accordingly.
 *
 * See https://tools.ietf.org/html/rfc5246#section-7.4.1.4.1 */
SECStatus
ssl_ConsumeSignatureScheme(sslSocket *ss, SSL3Opaque **b,
                           PRUint32 *length, SSLSignatureScheme *out)
{
    PRInt32 tmp;

    tmp = ssl3_ConsumeHandshakeNumber(ss, 2, b, length);
    if (tmp < 0) {
        return SECFailure; /* Error code set already. */
    }
    if (!ssl_IsSupportedSignatureScheme((SSLSignatureScheme)tmp)) {
        PORT_SetError(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
        return SECFailure;
    }
    *out = (SSLSignatureScheme)tmp;
    return SECSuccess;
}

/**************************************************************************
 * end of Consume Handshake functions.
 **************************************************************************/

static SECStatus
ssl3_ComputeHandshakeHash(unsigned char *buf, unsigned int len,
                          SSLHashType hashAlg, SSL3Hashes *hashes)
{
    SECStatus rv = SECFailure;
    PK11Context *hashContext = PK11_CreateDigestContext(
        ssl3_HashTypeToOID(hashAlg));

    if (!hashContext) {
        return rv;
    }
    rv = PK11_DigestBegin(hashContext);
    if (rv == SECSuccess) {
        rv = PK11_DigestOp(hashContext, buf, len);
    }
    if (rv == SECSuccess) {
        rv = PK11_DigestFinal(hashContext, hashes->u.raw, &hashes->len,
                              sizeof(hashes->u.raw));
    }
    if (rv == SECSuccess) {
        hashes->hashAlg = hashAlg;
    }
    PK11_DestroyContext(hashContext, PR_TRUE);
    return rv;
}

/* Extract the hashes of handshake messages to this point.
 * Called from ssl3_SendCertificateVerify
 *             ssl3_SendFinished
 *             ssl3_HandleHandshakeMessage
 *
 * Caller must hold the SSL3HandshakeLock.
 * Caller must hold a read or write lock on the Spec R/W lock.
 *  (There is presently no way to assert on a Read lock.)
 */
SECStatus
ssl3_ComputeHandshakeHashes(sslSocket *ss,
                            ssl3CipherSpec *spec, /* uses ->master_secret */
                            SSL3Hashes *hashes,   /* output goes here. */
                            PRUint32 sender)
{
    SECStatus rv = SECSuccess;
    PRBool isTLS = (PRBool)(spec->version > SSL_LIBRARY_VERSION_3_0);
    unsigned int outLength;
    SSL3Opaque md5_inner[MAX_MAC_LENGTH];
    SSL3Opaque sha_inner[MAX_MAC_LENGTH];

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    if (ss->ssl3.hs.hashType == handshake_hash_unknown) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    hashes->hashAlg = ssl_hash_none;

    if (ss->ssl3.hs.hashType == handshake_hash_single) {
        PK11Context *h;
        unsigned int stateLen;
        unsigned char stackBuf[1024];
        unsigned char *stateBuf = NULL;

        h = ss->ssl3.hs.sha;
        stateBuf = PK11_SaveContextAlloc(h, stackBuf,
                                         sizeof(stackBuf), &stateLen);
        if (stateBuf == NULL) {
            ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
            goto tls12_loser;
        }
        rv |= PK11_DigestFinal(h, hashes->u.raw, &hashes->len,
                               sizeof(hashes->u.raw));
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
            rv = SECFailure;
            goto tls12_loser;
        }

        hashes->hashAlg = ssl3_GetSuitePrfHash(ss);
        rv = SECSuccess;

    tls12_loser:
        if (stateBuf) {
            if (PK11_RestoreContext(h, stateBuf, stateLen) != SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
                rv = SECFailure;
            }
            if (stateBuf != stackBuf) {
                PORT_ZFree(stateBuf, stateLen);
            }
        }
    } else if (ss->ssl3.hs.hashType == handshake_hash_record) {
        rv = ssl3_ComputeHandshakeHash(ss->ssl3.hs.messages.buf,
                                       ss->ssl3.hs.messages.len,
                                       ssl3_GetSuitePrfHash(ss),
                                       hashes);
    } else {
        PK11Context *md5;
        PK11Context *sha = NULL;
        unsigned char *md5StateBuf = NULL;
        unsigned char *shaStateBuf = NULL;
        unsigned int md5StateLen, shaStateLen;
        unsigned char md5StackBuf[256];
        unsigned char shaStackBuf[512];

        md5StateBuf = PK11_SaveContextAlloc(ss->ssl3.hs.md5, md5StackBuf,
                                            sizeof md5StackBuf, &md5StateLen);
        if (md5StateBuf == NULL) {
            ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
            goto loser;
        }
        md5 = ss->ssl3.hs.md5;

        shaStateBuf = PK11_SaveContextAlloc(ss->ssl3.hs.sha, shaStackBuf,
                                            sizeof shaStackBuf, &shaStateLen);
        if (shaStateBuf == NULL) {
            ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
            goto loser;
        }
        sha = ss->ssl3.hs.sha;

        if (!isTLS) {
            /* compute hashes for SSL3. */
            unsigned char s[4];

            if (!spec->master_secret) {
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
                rv = SECFailure;
                goto loser;
            }

            s[0] = (unsigned char)(sender >> 24);
            s[1] = (unsigned char)(sender >> 16);
            s[2] = (unsigned char)(sender >> 8);
            s[3] = (unsigned char)sender;

            if (sender != 0) {
                rv |= PK11_DigestOp(md5, s, 4);
                PRINT_BUF(95, (NULL, "MD5 inner: sender", s, 4));
            }

            PRINT_BUF(95, (NULL, "MD5 inner: MAC Pad 1", mac_pad_1,
                           mac_defs[mac_md5].pad_size));

            rv |= PK11_DigestKey(md5, spec->master_secret);
            rv |= PK11_DigestOp(md5, mac_pad_1, mac_defs[mac_md5].pad_size);
            rv |= PK11_DigestFinal(md5, md5_inner, &outLength, MD5_LENGTH);
            PORT_Assert(rv != SECSuccess || outLength == MD5_LENGTH);
            if (rv != SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
                rv = SECFailure;
                goto loser;
            }

            PRINT_BUF(95, (NULL, "MD5 inner: result", md5_inner, outLength));

            if (sender != 0) {
                rv |= PK11_DigestOp(sha, s, 4);
                PRINT_BUF(95, (NULL, "SHA inner: sender", s, 4));
            }

            PRINT_BUF(95, (NULL, "SHA inner: MAC Pad 1", mac_pad_1,
                           mac_defs[mac_sha].pad_size));

            rv |= PK11_DigestKey(sha, spec->master_secret);
            rv |= PK11_DigestOp(sha, mac_pad_1, mac_defs[mac_sha].pad_size);
            rv |= PK11_DigestFinal(sha, sha_inner, &outLength, SHA1_LENGTH);
            PORT_Assert(rv != SECSuccess || outLength == SHA1_LENGTH);
            if (rv != SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
                rv = SECFailure;
                goto loser;
            }

            PRINT_BUF(95, (NULL, "SHA inner: result", sha_inner, outLength));

            PRINT_BUF(95, (NULL, "MD5 outer: MAC Pad 2", mac_pad_2,
                           mac_defs[mac_md5].pad_size));
            PRINT_BUF(95, (NULL, "MD5 outer: MD5 inner", md5_inner, MD5_LENGTH));

            rv |= PK11_DigestBegin(md5);
            rv |= PK11_DigestKey(md5, spec->master_secret);
            rv |= PK11_DigestOp(md5, mac_pad_2, mac_defs[mac_md5].pad_size);
            rv |= PK11_DigestOp(md5, md5_inner, MD5_LENGTH);
        }
        rv |= PK11_DigestFinal(md5, hashes->u.s.md5, &outLength, MD5_LENGTH);
        PORT_Assert(rv != SECSuccess || outLength == MD5_LENGTH);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
            rv = SECFailure;
            goto loser;
        }

        PRINT_BUF(60, (NULL, "MD5 outer: result", hashes->u.s.md5, MD5_LENGTH));

        if (!isTLS) {
            PRINT_BUF(95, (NULL, "SHA outer: MAC Pad 2", mac_pad_2,
                           mac_defs[mac_sha].pad_size));
            PRINT_BUF(95, (NULL, "SHA outer: SHA inner", sha_inner, SHA1_LENGTH));

            rv |= PK11_DigestBegin(sha);
            rv |= PK11_DigestKey(sha, spec->master_secret);
            rv |= PK11_DigestOp(sha, mac_pad_2, mac_defs[mac_sha].pad_size);
            rv |= PK11_DigestOp(sha, sha_inner, SHA1_LENGTH);
        }
        rv |= PK11_DigestFinal(sha, hashes->u.s.sha, &outLength, SHA1_LENGTH);
        PORT_Assert(rv != SECSuccess || outLength == SHA1_LENGTH);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
            rv = SECFailure;
            goto loser;
        }

        PRINT_BUF(60, (NULL, "SHA outer: result", hashes->u.s.sha, SHA1_LENGTH));

        hashes->len = MD5_LENGTH + SHA1_LENGTH;
        rv = SECSuccess;

    loser:
        if (md5StateBuf) {
            if (PK11_RestoreContext(ss->ssl3.hs.md5, md5StateBuf, md5StateLen) !=
                SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_MD5_DIGEST_FAILURE);
                rv = SECFailure;
            }
            if (md5StateBuf != md5StackBuf) {
                PORT_ZFree(md5StateBuf, md5StateLen);
            }
        }
        if (shaStateBuf) {
            if (PK11_RestoreContext(ss->ssl3.hs.sha, shaStateBuf, shaStateLen) !=
                SECSuccess) {
                ssl_MapLowLevelError(SSL_ERROR_SHA_DIGEST_FAILURE);
                rv = SECFailure;
            }
            if (shaStateBuf != shaStackBuf) {
                PORT_ZFree(shaStateBuf, shaStateLen);
            }
        }
    }
    return rv;
}

/**************************************************************************
 * end of Handshake Hash functions.
 * Begin Send and Handle functions for handshakes.
 **************************************************************************/

#ifdef TRACE
#define CHTYPE(t)          \
    case client_hello_##t: \
        return #t;

static const char *
ssl_ClientHelloTypeName(sslClientHelloType type)
{
    switch (type) {
        CHTYPE(initial);
        CHTYPE(retry);
        CHTYPE(retransmit);    /* DTLS only */
        CHTYPE(renegotiation); /* TLS <= 1.2 only */
    }
    PORT_Assert(0);
    return NULL;
}
#undef CHTYPE
#endif

/* Called from ssl3_HandleHelloRequest(),
 *             ssl3_RedoHandshake()
 *             ssl_BeginClientHandshake (when resuming ssl3 session)
 *             dtls_HandleHelloVerifyRequest(with resending=PR_TRUE)
 *
 * The |type| argument indicates what is going on here:
 * - client_hello_initial is set for the very first ClientHello
 * - client_hello_retry indicates that this is a second attempt after receiving
 *   a HelloRetryRequest (in TLS 1.3)
 * - client_hello_retransmit is used in DTLS when resending
 * - client_hello_renegotiation is used to renegotiate (in TLS <1.3)
 */
SECStatus
ssl3_SendClientHello(sslSocket *ss, sslClientHelloType type)
{
    sslSessionID *sid;
    ssl3CipherSpec *cwSpec;
    SECStatus rv;
    int i;
    int length;
    int num_suites;
    int actual_count = 0;
    PRBool isTLS = PR_FALSE;
    PRBool requestingResume = PR_FALSE, fallbackSCSV = PR_FALSE;
    PRInt32 total_exten_len = 0;
    unsigned paddingExtensionLen;
    unsigned numCompressionMethods;
    PRUint16 version;
    PRInt32 flags;

    SSL_TRC(3, ("%d: SSL3[%d]: send %s ClientHello handshake", SSL_GETPID(),
                ss->fd, ssl_ClientHelloTypeName(type)));

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    /* shouldn't get here if SSL3 is disabled, but ... */
    if (SSL_ALL_VERSIONS_DISABLED(&ss->vrange)) {
        PR_NOT_REACHED("No versions of SSL 3.0 or later are enabled");
        PORT_SetError(SSL_ERROR_SSL_DISABLED);
        return SECFailure;
    }

    /* If we are responding to a HelloRetryRequest, don't reinitialize. We need
     * to maintain the handshake hashes. */
    if (ss->ssl3.hs.helloRetry) {
        PORT_Assert(type == client_hello_retry);
    } else {
        rv = ssl3_InitState(ss);
        if (rv != SECSuccess) {
            return rv; /* ssl3_InitState has set the error code. */
        }

        rv = ssl3_RestartHandshakeHashes(ss);
        if (rv != SECSuccess) {
            return rv;
        }
    }

    /* These must be reset every handshake. */
    ss->ssl3.hs.sendingSCSV = PR_FALSE;
    ss->ssl3.hs.preliminaryInfo = 0;
    PORT_Assert(IS_DTLS(ss) || type != client_hello_retransmit);
    SECITEM_FreeItem(&ss->ssl3.hs.newSessionTicket.ticket, PR_FALSE);
    ss->ssl3.hs.receivedNewSessionTicket = PR_FALSE;
    ssl3_ResetExtensionData(&ss->xtnData);

    /* How many suites does our PKCS11 support (regardless of policy)? */
    num_suites = ssl3_config_match_init(ss);
    if (!num_suites) {
        return SECFailure; /* ssl3_config_match_init has set error code. */
    }

    /*
     * During a renegotiation, ss->clientHelloVersion will be used again to
     * work around a Windows SChannel bug. Ensure that it is still enabled.
     */
    if (ss->firstHsDone) {
        PORT_Assert(type != client_hello_initial);
        if (SSL_ALL_VERSIONS_DISABLED(&ss->vrange)) {
            PORT_SetError(SSL_ERROR_SSL_DISABLED);
            return SECFailure;
        }

        if (ss->clientHelloVersion < ss->vrange.min ||
            ss->clientHelloVersion > ss->vrange.max) {
            PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
            return SECFailure;
        }
    }

    /* We ignore ss->sec.ci.sid here, and use ssl_Lookup because Lookup
     * handles expired entries and other details.
     * XXX If we've been called from ssl_BeginClientHandshake, then
     * this lookup is duplicative and wasteful.
     */
    sid = (ss->opt.noCache) ? NULL
                            : ssl_LookupSID(&ss->sec.ci.peer, ss->sec.ci.port, ss->peerID, ss->url);

    /* We can't resume based on a different token. If the sid exists,
     * make sure the token that holds the master secret still exists ...
     * If we previously did client-auth, make sure that the token that holds
     * the private key still exists, is logged in, hasn't been removed, etc.
     */
    if (sid) {
        PRBool sidOK = PR_TRUE;
        const ssl3CipherSuiteCfg *suite;

        /* Check that the cipher suite we need is enabled. */
        suite = ssl_LookupCipherSuiteCfg(sid->u.ssl3.cipherSuite,
                                         ss->cipherSuites);
        PORT_Assert(suite);
        if (!suite || !config_match(suite, ss->ssl3.policy, &ss->vrange, ss)) {
            sidOK = PR_FALSE;
        }

        /* Check that we can recover the master secret. */
        if (sidOK && sid->u.ssl3.keys.msIsWrapped) {
            PK11SlotInfo *slot = NULL;
            if (sid->u.ssl3.masterValid) {
                slot = SECMOD_LookupSlot(sid->u.ssl3.masterModuleID,
                                         sid->u.ssl3.masterSlotID);
            }
            if (slot == NULL) {
                sidOK = PR_FALSE;
            } else {
                PK11SymKey *wrapKey = NULL;
                if (!PK11_IsPresent(slot) ||
                    ((wrapKey = PK11_GetWrapKey(slot,
                                                sid->u.ssl3.masterWrapIndex,
                                                sid->u.ssl3.masterWrapMech,
                                                sid->u.ssl3.masterWrapSeries,
                                                ss->pkcs11PinArg)) == NULL)) {
                    sidOK = PR_FALSE;
                }
                if (wrapKey)
                    PK11_FreeSymKey(wrapKey);
                PK11_FreeSlot(slot);
                slot = NULL;
            }
        }
        /* If we previously did client-auth, make sure that the token that
        ** holds the private key still exists, is logged in, hasn't been
        ** removed, etc.
        */
        if (sidOK && !ssl3_ClientAuthTokenPresent(sid)) {
            sidOK = PR_FALSE;
        }

        if (sidOK) {
            /* Set version based on the sid. */
            if (ss->firstHsDone) {
                /*
                 * Windows SChannel compares the client_version inside the RSA
                 * EncryptedPreMasterSecret of a renegotiation with the
                 * client_version of the initial ClientHello rather than the
                 * ClientHello in the renegotiation. To work around this bug, we
                 * continue to use the client_version used in the initial
                 * ClientHello when renegotiating.
                 *
                 * The client_version of the initial ClientHello is still
                 * available in ss->clientHelloVersion. Ensure that
                 * sid->version is bounded within
                 * [ss->vrange.min, ss->clientHelloVersion], otherwise we
                 * can't use sid.
                 */
                if (sid->version >= ss->vrange.min &&
                    sid->version <= ss->clientHelloVersion) {
                    version = ss->clientHelloVersion;
                } else {
                    sidOK = PR_FALSE;
                }
            } else {
                /*
                 * Check sid->version is OK first.
                 * Previously, we would cap the version based on sid->version,
                 * but that prevents negotiation of a higher version if the
                 * previous session was reduced (e.g., with version fallback)
                 */
                if (sid->version < ss->vrange.min ||
                    sid->version > ss->vrange.max) {
                    sidOK = PR_FALSE;
                } else {
                    version = ss->vrange.max;
                }
            }
        }

        if (!sidOK) {
            SSL_AtomicIncrementLong(&ssl3stats.sch_sid_cache_not_ok);
            ss->sec.uncache(sid);
            ssl_FreeSID(sid);
            sid = NULL;
        }
    }

    if (sid) {
        requestingResume = PR_TRUE;
        SSL_AtomicIncrementLong(&ssl3stats.sch_sid_cache_hits);

        PRINT_BUF(4, (ss, "client, found session-id:", sid->u.ssl3.sessionID,
                      sid->u.ssl3.sessionIDLength));

        ss->ssl3.policy = sid->u.ssl3.policy;
    } else {
        SSL_AtomicIncrementLong(&ssl3stats.sch_sid_cache_misses);

        /*
         * Windows SChannel compares the client_version inside the RSA
         * EncryptedPreMasterSecret of a renegotiation with the
         * client_version of the initial ClientHello rather than the
         * ClientHello in the renegotiation. To work around this bug, we
         * continue to use the client_version used in the initial
         * ClientHello when renegotiating.
         */
        if (ss->firstHsDone) {
            version = ss->clientHelloVersion;
        } else {
            version = ss->vrange.max;
        }

        sid = ssl3_NewSessionID(ss, PR_FALSE);
        if (!sid) {
            return SECFailure; /* memory error is set */
        }
        /* ss->version isn't set yet, but the sid needs a sane value. */
        sid->version = version;
    }

    isTLS = (version > SSL_LIBRARY_VERSION_3_0);
    ssl_GetSpecWriteLock(ss);
    cwSpec = ss->ssl3.cwSpec;
    if (cwSpec->mac_def->mac == mac_null) {
        /* SSL records are not being MACed. */
        cwSpec->version = version;
    }
    ssl_ReleaseSpecWriteLock(ss);

    if (ss->sec.ci.sid != NULL) {
        ssl_FreeSID(ss->sec.ci.sid); /* decrement ref count, free if zero */
    }
    ss->sec.ci.sid = sid;

    /* HACK for SCSV in SSL 3.0.  On initial handshake, prepend SCSV,
     * only if TLS is disabled.
     */
    if (!ss->firstHsDone && !isTLS) {
        /* Must set this before calling Hello Extension Senders,
         * to suppress sending of empty RI extension.
         */
        ss->ssl3.hs.sendingSCSV = PR_TRUE;
    }

    /* When we attempt session resumption (only), we must lock the sid to
     * prevent races with other resumption connections that receive a
     * NewSessionTicket that will cause the ticket in the sid to be replaced.
     * Once we've copied the session ticket into our ClientHello message, it
     * is OK for the ticket to change, so we just need to make sure we hold
     * the lock across the calls to ssl3_CallHelloExtensionSenders.
     */
    if (sid->u.ssl3.lock) {
        PR_RWLock_Rlock(sid->u.ssl3.lock);
    }

    if (ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3 &&
        type == client_hello_initial) {
        rv = tls13_SetupClientHello(ss);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    if (isTLS || (ss->firstHsDone && ss->peerRequestedProtection)) {
        PRUint32 maxBytes = 65535; /* 2^16 - 1 */
        PRInt32 extLen;

        extLen = ssl3_CallHelloExtensionSenders(ss, PR_FALSE, maxBytes, NULL);
        if (extLen < 0) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return SECFailure;
        }
        total_exten_len += extLen;

        if (total_exten_len > 0)
            total_exten_len += 2;
    }

    if (IS_DTLS(ss)) {
        ssl3_DisableNonDTLSSuites(ss);
    }

    /* how many suites are permitted by policy and user preference? */
    num_suites = count_cipher_suites(ss, ss->ssl3.policy);
    if (!num_suites) {
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        return SECFailure; /* count_cipher_suites has set error code. */
    }

    fallbackSCSV = ss->opt.enableFallbackSCSV && (!requestingResume ||
                                                  version < sid->version);
    /* make room for SCSV */
    if (ss->ssl3.hs.sendingSCSV) {
        ++num_suites;
    }
    if (fallbackSCSV) {
        ++num_suites;
    }

    /* count compression methods */
    numCompressionMethods = 0;
    for (i = 0; i < ssl_compression_method_count; i++) {
        if (ssl_CompressionEnabled(ss, ssl_compression_methods[i]))
            numCompressionMethods++;
    }

    length = sizeof(SSL3ProtocolVersion) + SSL3_RANDOM_LENGTH +
             1 + (sid->version >= SSL_LIBRARY_VERSION_TLS_1_3
                      ? 0
                      : sid->u.ssl3.sessionIDLength) +
             2 + num_suites * sizeof(ssl3CipherSuite) +
             1 + numCompressionMethods + total_exten_len;
    if (IS_DTLS(ss)) {
        length += 1 + ss->ssl3.hs.cookie.len;
    }

    /* A padding extension may be included to ensure that the record containing
     * the ClientHello doesn't have a length between 256 and 511 bytes
     * (inclusive). Initial, ClientHello records with such lengths trigger bugs
     * in F5 devices.
     *
     * This is not done for DTLS, for renegotiation, or when there are no
     * extensions. */
    if (!IS_DTLS(ss) && isTLS && !ss->firstHsDone && total_exten_len) {
        paddingExtensionLen = ssl3_CalculatePaddingExtensionLength(length);
        total_exten_len += paddingExtensionLen;
        length += paddingExtensionLen;
    } else {
        paddingExtensionLen = 0;
    }

    rv = ssl3_AppendHandshakeHeader(ss, client_hello, length);
    if (rv != SECSuccess) {
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        return rv; /* err set by ssl3_AppendHandshake* */
    }

    if (ss->firstHsDone) {
        /* The client hello version must stay unchanged to work around
         * the Windows SChannel bug described above. */
        PORT_Assert(version == ss->clientHelloVersion);
    }
    ss->clientHelloVersion = PR_MIN(version, SSL_LIBRARY_VERSION_TLS_1_2);
    if (IS_DTLS(ss)) {
        PRUint16 dtlsVersion;

        dtlsVersion = dtls_TLSVersionToDTLSVersion(ss->clientHelloVersion);
        rv = ssl3_AppendHandshakeNumber(ss, dtlsVersion, 2);
    } else {
        rv = ssl3_AppendHandshakeNumber(ss, ss->clientHelloVersion, 2);
    }
    if (rv != SECSuccess) {
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        return rv; /* err set by ssl3_AppendHandshake* */
    }

    /* Generate a new random if this is the first attempt. */
    if (type == client_hello_initial) {
        rv = ssl3_GetNewRandom(&ss->ssl3.hs.client_random);
        if (rv != SECSuccess) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return rv; /* err set by GetNewRandom. */
        }
    }
    rv = ssl3_AppendHandshake(ss, &ss->ssl3.hs.client_random,
                              SSL3_RANDOM_LENGTH);
    if (rv != SECSuccess) {
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        return rv; /* err set by ssl3_AppendHandshake* */
    }

    if (sid->version < SSL_LIBRARY_VERSION_TLS_1_3)
        rv = ssl3_AppendHandshakeVariable(
            ss, sid->u.ssl3.sessionID, sid->u.ssl3.sessionIDLength, 1);
    else
        rv = ssl3_AppendHandshakeNumber(ss, 0, 1);
    if (rv != SECSuccess) {
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        return rv; /* err set by ssl3_AppendHandshake* */
    }

    if (IS_DTLS(ss)) {
        rv = ssl3_AppendHandshakeVariable(
            ss, ss->ssl3.hs.cookie.data, ss->ssl3.hs.cookie.len, 1);
        if (rv != SECSuccess) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return rv; /* err set by ssl3_AppendHandshake* */
        }
    }

    rv = ssl3_AppendHandshakeNumber(ss, num_suites * sizeof(ssl3CipherSuite), 2);
    if (rv != SECSuccess) {
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        return rv; /* err set by ssl3_AppendHandshake* */
    }

    if (ss->ssl3.hs.sendingSCSV) {
        /* Add the actual SCSV */
        rv = ssl3_AppendHandshakeNumber(ss, TLS_EMPTY_RENEGOTIATION_INFO_SCSV,
                                        sizeof(ssl3CipherSuite));
        if (rv != SECSuccess) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return rv; /* err set by ssl3_AppendHandshake* */
        }
        actual_count++;
    }
    if (fallbackSCSV) {
        rv = ssl3_AppendHandshakeNumber(ss, TLS_FALLBACK_SCSV,
                                        sizeof(ssl3CipherSuite));
        if (rv != SECSuccess) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return rv; /* err set by ssl3_AppendHandshake* */
        }
        actual_count++;
    }
    for (i = 0; i < ssl_V3_SUITES_IMPLEMENTED; i++) {
        ssl3CipherSuiteCfg *suite = &ss->cipherSuites[i];
        if (config_match(suite, ss->ssl3.policy, &ss->vrange, ss)) {
            actual_count++;
            if (actual_count > num_suites) {
                if (sid->u.ssl3.lock) {
                    PR_RWLock_Unlock(sid->u.ssl3.lock);
                }
                /* set error card removal/insertion error */
                PORT_SetError(SSL_ERROR_TOKEN_INSERTION_REMOVAL);
                return SECFailure;
            }
            rv = ssl3_AppendHandshakeNumber(ss, suite->cipher_suite,
                                            sizeof(ssl3CipherSuite));
            if (rv != SECSuccess) {
                if (sid->u.ssl3.lock) {
                    PR_RWLock_Unlock(sid->u.ssl3.lock);
                }
                return rv; /* err set by ssl3_AppendHandshake* */
            }
        }
    }

    /* if cards were removed or inserted between count_cipher_suites and
     * generating our list, detect the error here rather than send it off to
     * the server.. */
    if (actual_count != num_suites) {
        /* Card removal/insertion error */
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        PORT_SetError(SSL_ERROR_TOKEN_INSERTION_REMOVAL);
        return SECFailure;
    }

    rv = ssl3_AppendHandshakeNumber(ss, numCompressionMethods, 1);
    if (rv != SECSuccess) {
        if (sid->u.ssl3.lock) {
            PR_RWLock_Unlock(sid->u.ssl3.lock);
        }
        return rv; /* err set by ssl3_AppendHandshake* */
    }
    for (i = 0; i < ssl_compression_method_count; i++) {
        if (!ssl_CompressionEnabled(ss, ssl_compression_methods[i]))
            continue;
        rv = ssl3_AppendHandshakeNumber(ss, ssl_compression_methods[i], 1);
        if (rv != SECSuccess) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return rv; /* err set by ssl3_AppendHandshake* */
        }
    }

    if (total_exten_len) {
        PRUint32 maxBytes = total_exten_len - 2;
        PRInt32 extLen;

        rv = ssl3_AppendHandshakeNumber(ss, maxBytes, 2);
        if (rv != SECSuccess) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return rv; /* err set by AppendHandshake. */
        }

        extLen = ssl3_AppendPaddingExtension(ss, paddingExtensionLen, maxBytes);
        if (extLen < 0) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return SECFailure;
        }
        maxBytes -= extLen;

        extLen = ssl3_CallHelloExtensionSenders(ss, PR_TRUE, maxBytes, NULL);
        if (extLen < 0) {
            if (sid->u.ssl3.lock) {
                PR_RWLock_Unlock(sid->u.ssl3.lock);
            }
            return SECFailure;
        }
        maxBytes -= extLen;

        PORT_Assert(!maxBytes);
    }

    if (sid->u.ssl3.lock) {
        PR_RWLock_Unlock(sid->u.ssl3.lock);
    }

    if (ss->xtnData.sentSessionTicketInClientHello) {
        SSL_AtomicIncrementLong(&ssl3stats.sch_sid_stateless_resumes);
    }

    if (ss->ssl3.hs.sendingSCSV) {
        /* Since we sent the SCSV, pretend we sent empty RI extension. */
        TLSExtensionData *xtnData = &ss->xtnData;
        xtnData->advertised[xtnData->numAdvertised++] =
            ssl_renegotiation_info_xtn;
    }

    flags = 0;
    if (!ss->firstHsDone && !IS_DTLS(ss)) {
        flags |= ssl_SEND_FLAG_CAP_RECORD_VERSION;
    }
    rv = ssl3_FlushHandshake(ss, flags);
    if (rv != SECSuccess) {
        return rv; /* error code set by ssl3_FlushHandshake */
    }

    if (version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = tls13_MaybeDo0RTTHandshake(ss);
        if (rv != SECSuccess) {
            return SECFailure; /* error code set already. */
        }
    }

    ss->ssl3.hs.ws = wait_server_hello;
    return SECSuccess;
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered a
 * complete ssl3 Hello Request.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleHelloRequest(sslSocket *ss)
{
    sslSessionID *sid = ss->sec.ci.sid;
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: handle hello_request handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

    if (ss->ssl3.hs.ws == wait_server_hello)
        return SECSuccess;
    if (ss->ssl3.hs.ws != idle_handshake || ss->sec.isServer) {
        (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_HELLO_REQUEST);
        return SECFailure;
    }
    if (ss->opt.enableRenegotiation == SSL_RENEGOTIATE_NEVER) {
        (void)SSL3_SendAlert(ss, alert_warning, no_renegotiation);
        PORT_SetError(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED);
        return SECFailure;
    }

    if (sid) {
        ss->sec.uncache(sid);
        ssl_FreeSID(sid);
        ss->sec.ci.sid = NULL;
    }

    if (IS_DTLS(ss)) {
        dtls_RehandshakeCleanup(ss);
    }

    ssl_GetXmitBufLock(ss);
    rv = ssl3_SendClientHello(ss, client_hello_renegotiation);
    ssl_ReleaseXmitBufLock(ss);

    return rv;
}

#define UNKNOWN_WRAP_MECHANISM 0x7fffffff

static const CK_MECHANISM_TYPE wrapMechanismList[SSL_NUM_WRAP_MECHS] = {
    CKM_DES3_ECB,
    CKM_CAST5_ECB,
    CKM_DES_ECB,
    CKM_KEY_WRAP_LYNKS,
    CKM_IDEA_ECB,
    CKM_CAST3_ECB,
    CKM_CAST_ECB,
    CKM_RC5_ECB,
    CKM_RC2_ECB,
    CKM_CDMF_ECB,
    CKM_SKIPJACK_WRAP,
    CKM_SKIPJACK_CBC64,
    CKM_AES_ECB,
    CKM_CAMELLIA_ECB,
    CKM_SEED_ECB,
    UNKNOWN_WRAP_MECHANISM
};

static int
ssl_FindIndexByWrapMechanism(CK_MECHANISM_TYPE mech)
{
    const CK_MECHANISM_TYPE *pMech = wrapMechanismList;

    while (mech != *pMech && *pMech != UNKNOWN_WRAP_MECHANISM) {
        ++pMech;
    }
    return (*pMech == UNKNOWN_WRAP_MECHANISM) ? -1
                                              : (pMech - wrapMechanismList);
}

static PK11SymKey *
ssl_UnwrapSymWrappingKey(
    SSLWrappedSymWrappingKey *pWswk,
    SECKEYPrivateKey *svrPrivKey,
    SSLAuthType authType,
    CK_MECHANISM_TYPE masterWrapMech,
    void *pwArg)
{
    PK11SymKey *unwrappedWrappingKey = NULL;
    SECItem wrappedKey;
    PK11SymKey *Ks;
    SECKEYPublicKey pubWrapKey;
    ECCWrappedKeyInfo *ecWrapped;

    /* found the wrapping key on disk. */
    PORT_Assert(pWswk->symWrapMechanism == masterWrapMech);
    PORT_Assert(pWswk->authType == authType);
    if (pWswk->symWrapMechanism != masterWrapMech ||
        pWswk->authType != authType) {
        goto loser;
    }
    wrappedKey.type = siBuffer;
    wrappedKey.data = pWswk->wrappedSymmetricWrappingkey;
    wrappedKey.len = pWswk->wrappedSymKeyLen;
    PORT_Assert(wrappedKey.len <= sizeof pWswk->wrappedSymmetricWrappingkey);

    switch (authType) {

        case ssl_auth_rsa_decrypt:
        case ssl_auth_rsa_sign: /* bad: see Bug 1248320 */
            unwrappedWrappingKey =
                PK11_PubUnwrapSymKey(svrPrivKey, &wrappedKey,
                                     masterWrapMech, CKA_UNWRAP, 0);
            break;

        case ssl_auth_ecdsa:
        case ssl_auth_ecdh_rsa:
        case ssl_auth_ecdh_ecdsa:
            /*
             * For ssl_auth_ecd*, we first create an EC public key based on
             * data stored with the wrappedSymmetricWrappingkey. Next,
             * we do an ECDH computation involving this public key and
             * the SSL server's (long-term) EC private key. The resulting
             * shared secret is treated the same way as Fortezza's Ks, i.e.,
             * it is used to recover the symmetric wrapping key.
             *
             * The data in wrappedSymmetricWrappingkey is laid out as defined
             * in the ECCWrappedKeyInfo structure.
             */
            ecWrapped = (ECCWrappedKeyInfo *)pWswk->wrappedSymmetricWrappingkey;

            PORT_Assert(ecWrapped->encodedParamLen + ecWrapped->pubValueLen +
                            ecWrapped->wrappedKeyLen <=
                        MAX_EC_WRAPPED_KEY_BUFLEN);

            if (ecWrapped->encodedParamLen + ecWrapped->pubValueLen +
                    ecWrapped->wrappedKeyLen >
                MAX_EC_WRAPPED_KEY_BUFLEN) {
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                goto loser;
            }

            pubWrapKey.keyType = ecKey;
            pubWrapKey.u.ec.size = ecWrapped->size;
            pubWrapKey.u.ec.DEREncodedParams.len = ecWrapped->encodedParamLen;
            pubWrapKey.u.ec.DEREncodedParams.data = ecWrapped->var;
            pubWrapKey.u.ec.publicValue.len = ecWrapped->pubValueLen;
            pubWrapKey.u.ec.publicValue.data = ecWrapped->var +
                                               ecWrapped->encodedParamLen;

            wrappedKey.len = ecWrapped->wrappedKeyLen;
            wrappedKey.data = ecWrapped->var + ecWrapped->encodedParamLen +
                              ecWrapped->pubValueLen;

            /* Derive Ks using ECDH */
            Ks = PK11_PubDeriveWithKDF(svrPrivKey, &pubWrapKey, PR_FALSE, NULL,
                                       NULL, CKM_ECDH1_DERIVE, masterWrapMech,
                                       CKA_DERIVE, 0, CKD_NULL, NULL, NULL);
            if (Ks == NULL) {
                goto loser;
            }

            /*  Use Ks to unwrap the wrapping key */
            unwrappedWrappingKey = PK11_UnwrapSymKey(Ks, masterWrapMech, NULL,
                                                     &wrappedKey, masterWrapMech,
                                                     CKA_UNWRAP, 0);
            PK11_FreeSymKey(Ks);

            break;

        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            goto loser;
    }
loser:
    return unwrappedWrappingKey;
}

/* Each process sharing the server session ID cache has its own array of SymKey
 * pointers for the symmetric wrapping keys that are used to wrap the master
 * secrets.  There is one key for each authentication type.  These Symkeys
 * correspond to the wrapped SymKeys kept in the server session cache.
 */

typedef struct {
    PK11SymKey *symWrapKey[ssl_auth_size];
} ssl3SymWrapKey;

static PZLock *symWrapKeysLock = NULL;
static ssl3SymWrapKey symWrapKeys[SSL_NUM_WRAP_MECHS];

SECStatus
ssl_FreeSymWrapKeysLock(void)
{
    if (symWrapKeysLock) {
        PZ_DestroyLock(symWrapKeysLock);
        symWrapKeysLock = NULL;
        return SECSuccess;
    }
    PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
    return SECFailure;
}

SECStatus
SSL3_ShutdownServerCache(void)
{
    int i, j;

    if (!symWrapKeysLock)
        return SECSuccess; /* lock was never initialized */
    PZ_Lock(symWrapKeysLock);
    /* get rid of all symWrapKeys */
    for (i = 0; i < SSL_NUM_WRAP_MECHS; ++i) {
        for (j = 0; j < ssl_auth_size; ++j) {
            PK11SymKey **pSymWrapKey;
            pSymWrapKey = &symWrapKeys[i].symWrapKey[j];
            if (*pSymWrapKey) {
                PK11_FreeSymKey(*pSymWrapKey);
                *pSymWrapKey = NULL;
            }
        }
    }

    PZ_Unlock(symWrapKeysLock);
    ssl_FreeSessionCacheLocks();
    return SECSuccess;
}

SECStatus
ssl_InitSymWrapKeysLock(void)
{
    symWrapKeysLock = PZ_NewLock(nssILockOther);
    return symWrapKeysLock ? SECSuccess : SECFailure;
}

/* Try to get wrapping key for mechanism from in-memory array.
 * If that fails, look for one on disk.
 * If that fails, generate a new one, put the new one on disk,
 * Put the new key in the in-memory array.
 *
 * Note that this function performs some fairly inadvisable functions with
 * certificate private keys.  ECDSA keys are used with ECDH; similarly, RSA
 * signing keys are used to encrypt.  Bug 1248320.
 */
PK11SymKey *
ssl3_GetWrappingKey(sslSocket *ss,
                    PK11SlotInfo *masterSecretSlot,
                    const sslServerCert *serverCert,
                    CK_MECHANISM_TYPE masterWrapMech,
                    void *pwArg)
{
    SSLAuthType authType;
    SECKEYPrivateKey *svrPrivKey;
    SECKEYPublicKey *svrPubKey = NULL;
    PK11SymKey *unwrappedWrappingKey = NULL;
    PK11SymKey **pSymWrapKey;
    CK_MECHANISM_TYPE asymWrapMechanism = CKM_INVALID_MECHANISM;
    int length;
    int symWrapMechIndex;
    SECStatus rv;
    SECItem wrappedKey;
    SSLWrappedSymWrappingKey wswk;
    PK11SymKey *Ks = NULL;
    SECKEYPublicKey *pubWrapKey = NULL;
    SECKEYPrivateKey *privWrapKey = NULL;
    ECCWrappedKeyInfo *ecWrapped;

    PORT_Assert(serverCert);
    PORT_Assert(serverCert->serverKeyPair);
    PORT_Assert(serverCert->serverKeyPair->privKey);
    PORT_Assert(serverCert->serverKeyPair->pubKey);
    if (!serverCert || !serverCert->serverKeyPair ||
        !serverCert->serverKeyPair->privKey ||
        !serverCert->serverKeyPair->pubKey) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return NULL; /* hmm */
    }
    authType = serverCert->certType.authType;
    svrPrivKey = serverCert->serverKeyPair->privKey;

    symWrapMechIndex = ssl_FindIndexByWrapMechanism(masterWrapMech);
    PORT_Assert(symWrapMechIndex >= 0);
    if (symWrapMechIndex < 0)
        return NULL; /* invalid masterWrapMech. */

    pSymWrapKey = &symWrapKeys[symWrapMechIndex].symWrapKey[authType];

    ssl_InitSessionCacheLocks(PR_TRUE);

    PZ_Lock(symWrapKeysLock);

    unwrappedWrappingKey = *pSymWrapKey;
    if (unwrappedWrappingKey != NULL) {
        if (PK11_VerifyKeyOK(unwrappedWrappingKey)) {
            unwrappedWrappingKey = PK11_ReferenceSymKey(unwrappedWrappingKey);
            goto done;
        }
        /* slot series has changed, so this key is no good any more. */
        PK11_FreeSymKey(unwrappedWrappingKey);
        *pSymWrapKey = unwrappedWrappingKey = NULL;
    }

    /* Try to get wrapped SymWrapping key out of the (disk) cache. */
    /* Following call fills in wswk on success. */
    if (ssl_GetWrappingKey(symWrapMechIndex, authType, &wswk)) {
        /* found the wrapped sym wrapping key on disk. */
        unwrappedWrappingKey =
            ssl_UnwrapSymWrappingKey(&wswk, svrPrivKey, authType,
                                     masterWrapMech, pwArg);
        if (unwrappedWrappingKey) {
            goto install;
        }
    }

    if (!masterSecretSlot) /* caller doesn't want to create a new one. */
        goto loser;

    length = PK11_GetBestKeyLength(masterSecretSlot, masterWrapMech);
    /* Zero length means fixed key length algorithm, or error.
     * It's ambiguous.
     */
    unwrappedWrappingKey = PK11_KeyGen(masterSecretSlot, masterWrapMech, NULL,
                                       length, pwArg);
    if (!unwrappedWrappingKey) {
        goto loser;
    }

    /* Prepare the buffer to receive the wrappedWrappingKey,
     * the symmetric wrapping key wrapped using the server's pub key.
     */
    PORT_Memset(&wswk, 0, sizeof wswk); /* eliminate UMRs. */

    svrPubKey = serverCert->serverKeyPair->pubKey;
    wrappedKey.type = siBuffer;
    wrappedKey.len = SECKEY_PublicKeyStrength(svrPubKey);
    wrappedKey.data = wswk.wrappedSymmetricWrappingkey;

    PORT_Assert(wrappedKey.len <= sizeof wswk.wrappedSymmetricWrappingkey);
    if (wrappedKey.len > sizeof wswk.wrappedSymmetricWrappingkey)
        goto loser;

    /* wrap symmetric wrapping key in server's public key. */
    switch (authType) {
        case ssl_auth_rsa_decrypt:
        case ssl_auth_rsa_sign: /* bad: see Bug 1248320 */
            asymWrapMechanism = CKM_RSA_PKCS;
            rv = PK11_PubWrapSymKey(asymWrapMechanism, svrPubKey,
                                    unwrappedWrappingKey, &wrappedKey);
            break;

        case ssl_auth_ecdsa:
        case ssl_auth_ecdh_rsa:
        case ssl_auth_ecdh_ecdsa:
            /*
             * We generate an ephemeral EC key pair. Perform an ECDH
             * computation involving this ephemeral EC public key and
             * the SSL server's (long-term) EC private key. The resulting
             * shared secret is treated in the same way as Fortezza's Ks,
             * i.e., it is used to wrap the wrapping key. To facilitate
             * unwrapping in ssl_UnwrapWrappingKey, we also store all
             * relevant info about the ephemeral EC public key in
             * wswk.wrappedSymmetricWrappingkey and lay it out as
             * described in the ECCWrappedKeyInfo structure.
             */
            PORT_Assert(SECKEY_GetPublicKeyType(svrPubKey) == ecKey);
            if (SECKEY_GetPublicKeyType(svrPubKey) != ecKey) {
                /* something is wrong in sslsecur.c if this isn't an ecKey */
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                rv = SECFailure;
                goto ec_cleanup;
            }

            privWrapKey = SECKEY_CreateECPrivateKey(
                &svrPubKey->u.ec.DEREncodedParams, &pubWrapKey, NULL);
            if ((privWrapKey == NULL) || (pubWrapKey == NULL)) {
                rv = SECFailure;
                goto ec_cleanup;
            }

            /* Set the key size in bits */
            if (pubWrapKey->u.ec.size == 0) {
                pubWrapKey->u.ec.size = SECKEY_PublicKeyStrengthInBits(svrPubKey);
            }

            PORT_Assert(pubWrapKey->u.ec.DEREncodedParams.len +
                            pubWrapKey->u.ec.publicValue.len <
                        MAX_EC_WRAPPED_KEY_BUFLEN);
            if (pubWrapKey->u.ec.DEREncodedParams.len +
                    pubWrapKey->u.ec.publicValue.len >=
                MAX_EC_WRAPPED_KEY_BUFLEN) {
                PORT_SetError(SEC_ERROR_INVALID_KEY);
                rv = SECFailure;
                goto ec_cleanup;
            }

            /* Derive Ks using ECDH */
            Ks = PK11_PubDeriveWithKDF(svrPrivKey, pubWrapKey, PR_FALSE, NULL,
                                       NULL, CKM_ECDH1_DERIVE, masterWrapMech,
                                       CKA_DERIVE, 0, CKD_NULL, NULL, NULL);
            if (Ks == NULL) {
                rv = SECFailure;
                goto ec_cleanup;
            }

            ecWrapped = (ECCWrappedKeyInfo *)(wswk.wrappedSymmetricWrappingkey);
            ecWrapped->size = pubWrapKey->u.ec.size;
            ecWrapped->encodedParamLen = pubWrapKey->u.ec.DEREncodedParams.len;
            PORT_Memcpy(ecWrapped->var, pubWrapKey->u.ec.DEREncodedParams.data,
                        pubWrapKey->u.ec.DEREncodedParams.len);

            ecWrapped->pubValueLen = pubWrapKey->u.ec.publicValue.len;
            PORT_Memcpy(ecWrapped->var + ecWrapped->encodedParamLen,
                        pubWrapKey->u.ec.publicValue.data,
                        pubWrapKey->u.ec.publicValue.len);

            wrappedKey.len = MAX_EC_WRAPPED_KEY_BUFLEN -
                             (ecWrapped->encodedParamLen + ecWrapped->pubValueLen);
            wrappedKey.data = ecWrapped->var + ecWrapped->encodedParamLen +
                              ecWrapped->pubValueLen;

            /* wrap symmetricWrapping key with the local Ks */
            rv = PK11_WrapSymKey(masterWrapMech, NULL, Ks,
                                 unwrappedWrappingKey, &wrappedKey);

            if (rv != SECSuccess) {
                goto ec_cleanup;
            }

            /* Write down the length of wrapped key in the buffer
             * wswk.wrappedSymmetricWrappingkey at the appropriate offset
             */
            ecWrapped->wrappedKeyLen = wrappedKey.len;

        ec_cleanup:
            if (privWrapKey)
                SECKEY_DestroyPrivateKey(privWrapKey);
            if (pubWrapKey)
                SECKEY_DestroyPublicKey(pubWrapKey);
            if (Ks)
                PK11_FreeSymKey(Ks);
            asymWrapMechanism = masterWrapMech;
            break;

        default:
            rv = SECFailure;
            break;
    }

    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    PORT_Assert(asymWrapMechanism != CKM_INVALID_MECHANISM);

    wswk.symWrapMechanism = masterWrapMech;
    wswk.symWrapMechIndex = symWrapMechIndex;
    wswk.asymWrapMechanism = asymWrapMechanism;
    wswk.authType = authType;
    wswk.wrappedSymKeyLen = wrappedKey.len;

    /* put it on disk. */
    /* If the wrapping key for this KEA type has already been set,
     * then abandon the value we just computed and
     * use the one we got from the disk.
     */
    if (ssl_SetWrappingKey(&wswk)) {
        /* somebody beat us to it.  The original contents of our wswk
         * has been replaced with the content on disk.  Now, discard
         * the key we just created and unwrap this new one.
         */
        PK11_FreeSymKey(unwrappedWrappingKey);

        unwrappedWrappingKey =
            ssl_UnwrapSymWrappingKey(&wswk, svrPrivKey, authType,
                                     masterWrapMech, pwArg);
    }

install:
    if (unwrappedWrappingKey) {
        *pSymWrapKey = PK11_ReferenceSymKey(unwrappedWrappingKey);
    }

loser:
done:
    PZ_Unlock(symWrapKeysLock);
    return unwrappedWrappingKey;
}

#ifdef NSS_ALLOW_SSLKEYLOGFILE
/* hexEncode hex encodes |length| bytes from |in| and writes it as |length*2|
 * bytes to |out|. */
static void
hexEncode(char *out, const unsigned char *in, unsigned int length)
{
    static const char hextable[] = "0123456789abcdef";
    unsigned int i;

    for (i = 0; i < length; i++) {
        *(out++) = hextable[in[i] >> 4];
        *(out++) = hextable[in[i] & 15];
    }
}
#endif

/* Called from ssl3_SendClientKeyExchange(). */
static SECStatus
ssl3_SendRSAClientKeyExchange(sslSocket *ss, SECKEYPublicKey *svrPubKey)
{
    PK11SymKey *pms = NULL;
    SECStatus rv = SECFailure;
    SECItem enc_pms = { siBuffer, NULL, 0 };
    PRBool isTLS;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    /* Generate the pre-master secret ...  */
    ssl_GetSpecWriteLock(ss);
    isTLS = (PRBool)(ss->ssl3.pwSpec->version > SSL_LIBRARY_VERSION_3_0);

    pms = ssl3_GenerateRSAPMS(ss, ss->ssl3.pwSpec, NULL);
    ssl_ReleaseSpecWriteLock(ss);
    if (pms == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    /* Get the wrapped (encrypted) pre-master secret, enc_pms */
    enc_pms.len = SECKEY_PublicKeyStrength(svrPubKey);
    enc_pms.data = (unsigned char *)PORT_Alloc(enc_pms.len);
    if (enc_pms.data == NULL) {
        goto loser; /* err set by PORT_Alloc */
    }

    /* wrap pre-master secret in server's public key. */
    rv = PK11_PubWrapSymKey(CKM_RSA_PKCS, svrPubKey, pms, &enc_pms);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

#ifdef NSS_ALLOW_SSLKEYLOGFILE
    if (ssl_keylog_iob) {
        SECStatus extractRV = PK11_ExtractKeyValue(pms);
        if (extractRV == SECSuccess) {
            SECItem *keyData = PK11_GetKeyData(pms);
            if (keyData && keyData->data && keyData->len) {
#ifdef TRACE
                if (ssl_trace >= 100) {
                    ssl_PrintBuf(ss, "Pre-Master Secret",
                                 keyData->data, keyData->len);
                }
#endif
                if (ssl_keylog_iob && enc_pms.len >= 8 && keyData->len == 48) {
                    /* https://developer.mozilla.org/en/NSS_Key_Log_Format */

                    /* There could be multiple, concurrent writers to the
                     * keylog, so we have to do everything in a single call to
                     * fwrite. */
                    char buf[4 + 8 * 2 + 1 + 48 * 2 + 1];

                    strcpy(buf, "RSA ");
                    hexEncode(buf + 4, enc_pms.data, 8);
                    buf[20] = ' ';
                    hexEncode(buf + 21, keyData->data, 48);
                    buf[sizeof(buf) - 1] = '\n';

                    fwrite(buf, sizeof(buf), 1, ssl_keylog_iob);
                    fflush(ssl_keylog_iob);
                }
            }
        }
    }
#endif

    rv = ssl3_AppendHandshakeHeader(ss, client_key_exchange,
                                    isTLS ? enc_pms.len + 2
                                          : enc_pms.len);
    if (rv != SECSuccess) {
        goto loser; /* err set by ssl3_AppendHandshake* */
    }
    if (isTLS) {
        rv = ssl3_AppendHandshakeVariable(ss, enc_pms.data, enc_pms.len, 2);
    } else {
        rv = ssl3_AppendHandshake(ss, enc_pms.data, enc_pms.len);
    }
    if (rv != SECSuccess) {
        goto loser; /* err set by ssl3_AppendHandshake* */
    }

    rv = ssl3_InitPendingCipherSpec(ss, pms);
    PK11_FreeSymKey(pms);
    pms = NULL;

    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    rv = SECSuccess;

loser:
    if (enc_pms.data != NULL) {
        PORT_Free(enc_pms.data);
    }
    if (pms != NULL) {
        PK11_FreeSymKey(pms);
    }
    return rv;
}

/* DH shares need to be padded to the size of their prime.  Some implementations
 * require this.  TLS 1.3 also requires this. */
SECStatus
ssl_AppendPaddedDHKeyShare(const sslSocket *ss, const SECKEYPublicKey *pubKey,
                           PRBool appendLength)
{
    SECStatus rv;
    unsigned int pad = pubKey->u.dh.prime.len - pubKey->u.dh.publicValue.len;

    if (appendLength) {
        rv = ssl3_ExtAppendHandshakeNumber(ss, pubKey->u.dh.prime.len, 2);
        if (rv != SECSuccess) {
            return rv;
        }
    }
    while (pad) {
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 1);
        if (rv != SECSuccess) {
            return rv;
        }
        --pad;
    }
    rv = ssl3_ExtAppendHandshake(ss, pubKey->u.dh.publicValue.data,
                                 pubKey->u.dh.publicValue.len);
    if (rv != SECSuccess) {
        return rv;
    }
    return SECSuccess;
}

/* Called from ssl3_SendClientKeyExchange(). */
static SECStatus
ssl3_SendDHClientKeyExchange(sslSocket *ss, SECKEYPublicKey *svrPubKey)
{
    PK11SymKey *pms = NULL;
    SECStatus rv;
    PRBool isTLS;
    CK_MECHANISM_TYPE target;

    const ssl3DHParams *params;
    ssl3DHParams customParams;
    const sslNamedGroupDef *groupDef;
    static const sslNamedGroupDef customGroupDef = {
        ssl_grp_ffdhe_custom, 0, ssl_kea_dh, SEC_OID_TLS_DHE_CUSTOM, PR_FALSE
    };
    sslEphemeralKeyPair *keyPair = NULL;
    SECKEYPublicKey *pubKey;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    isTLS = (PRBool)(ss->ssl3.pwSpec->version > SSL_LIBRARY_VERSION_3_0);

    /* Copy DH parameters from server key */

    if (SECKEY_GetPublicKeyType(svrPubKey) != dhKey) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }

    /* Work out the parameters. */
    rv = ssl_ValidateDHENamedGroup(ss, &svrPubKey->u.dh.prime,
                                   &svrPubKey->u.dh.base,
                                   &groupDef, &params);
    if (rv != SECSuccess) {
        /* If we require named groups, we will have already validated the group
         * in ssl_HandleDHServerKeyExchange() */
        PORT_Assert(!ss->opt.requireDHENamedGroups &&
                    !ss->xtnData.peerSupportsFfdheGroups);

        customParams.name = ssl_grp_ffdhe_custom;
        customParams.prime.data = svrPubKey->u.dh.prime.data;
        customParams.prime.len = svrPubKey->u.dh.prime.len;
        customParams.base.data = svrPubKey->u.dh.base.data;
        customParams.base.len = svrPubKey->u.dh.base.len;
        params = &customParams;
        groupDef = &customGroupDef;
    }
    ss->sec.keaGroup = groupDef;

    rv = ssl_CreateDHEKeyPair(groupDef, params, &keyPair);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SEC_ERROR_KEYGEN_FAIL);
        goto loser;
    }
    pubKey = keyPair->keys->pubKey;
    PRINT_BUF(50, (ss, "DH public value:",
                   pubKey->u.dh.publicValue.data,
                   pubKey->u.dh.publicValue.len));

    if (isTLS)
        target = CKM_TLS_MASTER_KEY_DERIVE_DH;
    else
        target = CKM_SSL3_MASTER_KEY_DERIVE_DH;

    /* Determine the PMS */
    pms = PK11_PubDerive(keyPair->keys->privKey, svrPubKey,
                         PR_FALSE, NULL, NULL, CKM_DH_PKCS_DERIVE,
                         target, CKA_DERIVE, 0, NULL);

    if (pms == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    /* Note: send the DH share padded to avoid triggering bugs. */
    rv = ssl3_AppendHandshakeHeader(ss, client_key_exchange,
                                    params->prime.len + 2);
    if (rv != SECSuccess) {
        goto loser; /* err set by ssl3_AppendHandshake* */
    }
    rv = ssl_AppendPaddedDHKeyShare(ss, pubKey, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser; /* err set by ssl_AppendPaddedDHKeyShare */
    }

    rv = ssl3_InitPendingCipherSpec(ss, pms);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    PK11_FreeSymKey(pms);
    ssl_FreeEphemeralKeyPair(keyPair);
    return SECSuccess;

loser:
    if (pms)
        PK11_FreeSymKey(pms);
    if (keyPair)
        ssl_FreeEphemeralKeyPair(keyPair);
    return SECFailure;
}

/* Called from ssl3_HandleServerHelloDone(). */
static SECStatus
ssl3_SendClientKeyExchange(sslSocket *ss)
{
    SECKEYPublicKey *serverKey = NULL;
    SECStatus rv = SECFailure;

    SSL_TRC(3, ("%d: SSL3[%d]: send client_key_exchange handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->sec.peerKey == NULL) {
        serverKey = CERT_ExtractPublicKey(ss->sec.peerCert);
        if (serverKey == NULL) {
            ssl_MapLowLevelError(SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE);
            return SECFailure;
        }
    } else {
        serverKey = ss->sec.peerKey;
        ss->sec.peerKey = NULL; /* we're done with it now */
    }

    ss->sec.keaType = ss->ssl3.hs.kea_def->exchKeyType;
    ss->sec.keaKeyBits = SECKEY_PublicKeyStrengthInBits(serverKey);

    switch (ss->ssl3.hs.kea_def->exchKeyType) {
        case ssl_kea_rsa:
            rv = ssl3_SendRSAClientKeyExchange(ss, serverKey);
            break;

        case ssl_kea_dh:
            rv = ssl3_SendDHClientKeyExchange(ss, serverKey);
            break;

        case ssl_kea_ecdh:
            rv = ssl3_SendECDHClientKeyExchange(ss, serverKey);
            break;

        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            break;
    }

    SSL_TRC(3, ("%d: SSL3[%d]: DONE sending client_key_exchange",
                SSL_GETPID(), ss->fd));

    SECKEY_DestroyPublicKey(serverKey);
    return rv; /* err code already set. */
}

SECStatus
ssl_PickSignatureScheme(sslSocket *ss,
                        SECKEYPublicKey *pubKey,
                        SECKEYPrivateKey *privKey,
                        const SSLSignatureScheme *peerSchemes,
                        unsigned int peerSchemeCount,
                        PRBool requireSha1)
{
    unsigned int i, j;
    const sslNamedGroupDef *group = NULL;
    KeyType keyType;
    PK11SlotInfo *slot;
    PRBool slotDoesPss;
    PRBool isTLS13 = ss->version >= SSL_LIBRARY_VERSION_TLS_1_3;

    /* We can't require SHA-1 in TLS 1.3. */
    PORT_Assert(!(requireSha1 && isTLS13));
    if (!pubKey || !privKey) {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    slot = PK11_GetSlotFromPrivateKey(privKey);
    if (!slot) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    slotDoesPss = PK11_DoesMechanism(slot, auth_alg_defs[ssl_auth_rsa_pss]);
    PK11_FreeSlot(slot);

    keyType = SECKEY_GetPublicKeyType(pubKey);
    if (keyType == ecKey) {
        group = ssl_ECPubKey2NamedGroup(pubKey);
    }

    /* Here we look for the first local preference that the client has
     * indicated support for in their signature_algorithms extension. */
    for (i = 0; i < ss->ssl3.signatureSchemeCount; ++i) {
        SSLHashType hashType;
        SECOidTag hashOID;
        SSLSignatureScheme preferred = ss->ssl3.signatureSchemes[i];
        PRUint32 policy;

        if (!ssl_SignatureSchemeValidForKey(!isTLS13 /* allowSha1 */,
                                            PR_TRUE /* matchGroup */,
                                            keyType, group, preferred)) {
            continue;
        }

        /* Skip RSA-PSS schemes when the certificate's private key slot does
         * not support this signature mechanism. */
        if (ssl_IsRsaPssSignatureScheme(preferred) && !slotDoesPss) {
            continue;
        }

        hashType = ssl_SignatureSchemeToHashType(preferred);
        if (requireSha1 && (hashType != ssl_hash_sha1)) {
            continue;
        }
        hashOID = ssl3_HashTypeToOID(hashType);
        if ((NSS_GetAlgorithmPolicy(hashOID, &policy) == SECSuccess) &&
            !(policy & NSS_USE_ALG_IN_SSL_KX)) {
            /* we ignore hashes we don't support */
            continue;
        }

        for (j = 0; j < peerSchemeCount; j++) {
            if (peerSchemes[j] == preferred) {
                ss->ssl3.hs.signatureScheme = preferred;
                return SECSuccess;
            }
        }
    }

    PORT_SetError(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
    return SECFailure;
}

/* ssl3_PickServerSignatureScheme selects a signature scheme for signing the
 * handshake.  Most of this is determined by the key pair we are using.
 * Prior to TLS 1.2, the MD5/SHA1 combination is always used. With TLS 1.2, a
 * client may advertise its support for signature and hash combinations. */
static SECStatus
ssl3_PickServerSignatureScheme(sslSocket *ss)
{
    sslKeyPair *keyPair = ss->sec.serverCert->serverKeyPair;
    PRBool isTLS12 = ss->version >= SSL_LIBRARY_VERSION_TLS_1_2;

    if (!isTLS12 || !ssl3_ExtensionNegotiated(ss, ssl_signature_algorithms_xtn)) {
        /* If the client didn't provide any signature_algorithms extension then
         * we can assume that they support SHA-1: RFC5246, Section 7.4.1.4.1. */
        switch (SECKEY_GetPublicKeyType(keyPair->pubKey)) {
            case rsaKey:
                if (isTLS12) {
                    ss->ssl3.hs.signatureScheme = ssl_sig_rsa_pkcs1_sha1;
                } else {
                    ss->ssl3.hs.signatureScheme = ssl_sig_rsa_pkcs1_sha1md5;
                }
                break;
            case ecKey:
                ss->ssl3.hs.signatureScheme = ssl_sig_ecdsa_sha1;
                break;
            case dsaKey:
                ss->ssl3.hs.signatureScheme = ssl_sig_dsa_sha1;
                break;
            default:
                PORT_Assert(0);
                PORT_SetError(SEC_ERROR_INVALID_KEY);
                return SECFailure;
        }
        return SECSuccess;
    }

    /* Sets error code, if needed. */
    return ssl_PickSignatureScheme(ss, keyPair->pubKey, keyPair->privKey,
                                   ss->xtnData.clientSigSchemes,
                                   ss->xtnData.numClientSigScheme,
                                   PR_FALSE /* requireSha1 */);
}

static SECStatus
ssl_PickClientSignatureScheme(sslSocket *ss, const SSLSignatureScheme *schemes,
                              unsigned int numSchemes)
{
    SECKEYPrivateKey *privKey = ss->ssl3.clientPrivateKey;
    SECKEYPublicKey *pubKey;
    SECStatus rv;

    pubKey = CERT_ExtractPublicKey(ss->ssl3.clientCertificate);
    PORT_Assert(pubKey);
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3 &&
        (SECKEY_GetPublicKeyType(pubKey) == rsaKey ||
         SECKEY_GetPublicKeyType(pubKey) == dsaKey) &&
        SECKEY_PublicKeyStrengthInBits(pubKey) <= 1024) {
        /* If the key is a 1024-bit RSA or DSA key, assume conservatively that
         * it may be unable to sign SHA-256 hashes. This is the case for older
         * Estonian ID cards that have 1024-bit RSA keys. In FIPS 186-2 and
         * older, DSA key size is at most 1024 bits and the hash function must
         * be SHA-1.
         */
        rv = ssl_PickSignatureScheme(ss, pubKey, privKey, schemes, numSchemes,
                                     PR_TRUE /* requireSha1 */);
        if (rv == SECSuccess) {
            SECKEY_DestroyPublicKey(pubKey);
            return SECSuccess;
        }
        /* If this fails, that's because the peer doesn't advertise SHA-1,
         * so fall back to the full negotiation. */
    }
    rv = ssl_PickSignatureScheme(ss, pubKey, privKey, schemes, numSchemes,
                                 PR_FALSE /* requireSha1 */);
    SECKEY_DestroyPublicKey(pubKey);
    return rv;
}

/* Called from ssl3_HandleServerHelloDone(). */
static SECStatus
ssl3_SendCertificateVerify(sslSocket *ss, SECKEYPrivateKey *privKey)
{
    SECStatus rv = SECFailure;
    PRBool isTLS12;
    SECItem buf = { siBuffer, NULL, 0 };
    SSL3Hashes hashes;
    unsigned int len;
    SSLHashType hashAlg;

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    SSL_TRC(3, ("%d: SSL3[%d]: send certificate_verify handshake",
                SSL_GETPID(), ss->fd));

    ssl_GetSpecReadLock(ss);

    if (ss->ssl3.hs.hashType == handshake_hash_record) {
        hashAlg = ssl_SignatureSchemeToHashType(ss->ssl3.hs.signatureScheme);
    } else {
        /* Use ssl_hash_none to represent the MD5+SHA1 combo. */
        hashAlg = ssl_hash_none;
    }
    if (ss->ssl3.hs.hashType == handshake_hash_record &&
        hashAlg != ssl3_GetSuitePrfHash(ss)) {
        rv = ssl3_ComputeHandshakeHash(ss->ssl3.hs.messages.buf,
                                       ss->ssl3.hs.messages.len,
                                       hashAlg, &hashes);
        if (rv != SECSuccess) {
            ssl_MapLowLevelError(SSL_ERROR_DIGEST_FAILURE);
        }
    } else {
        rv = ssl3_ComputeHandshakeHashes(ss, ss->ssl3.pwSpec, &hashes, 0);
    }
    ssl_ReleaseSpecReadLock(ss);
    if (rv != SECSuccess) {
        goto done; /* err code was set by ssl3_ComputeHandshakeHash(es) */
    }

    isTLS12 = (PRBool)(ss->version == SSL_LIBRARY_VERSION_TLS_1_2);
    PORT_Assert(ss->version <= SSL_LIBRARY_VERSION_TLS_1_2);

    rv = ssl3_SignHashes(ss, &hashes, privKey, &buf);
    if (rv == SECSuccess && !ss->sec.isServer) {
        /* Remember the info about the slot that did the signing.
        ** Later, when doing an SSL restart handshake, verify this.
        ** These calls are mere accessors, and can't fail.
        */
        PK11SlotInfo *slot;
        sslSessionID *sid = ss->sec.ci.sid;

        slot = PK11_GetSlotFromPrivateKey(privKey);
        sid->u.ssl3.clAuthSeries = PK11_GetSlotSeries(slot);
        sid->u.ssl3.clAuthSlotID = PK11_GetSlotID(slot);
        sid->u.ssl3.clAuthModuleID = PK11_GetModuleID(slot);
        sid->u.ssl3.clAuthValid = PR_TRUE;
        PK11_FreeSlot(slot);
    }
    if (rv != SECSuccess) {
        goto done; /* err code was set by ssl3_SignHashes */
    }

    len = buf.len + 2 + (isTLS12 ? 2 : 0);

    rv = ssl3_AppendHandshakeHeader(ss, certificate_verify, len);
    if (rv != SECSuccess) {
        goto done; /* error code set by AppendHandshake */
    }
    if (isTLS12) {
        rv = ssl3_AppendHandshakeNumber(ss, ss->ssl3.hs.signatureScheme, 2);
        if (rv != SECSuccess) {
            goto done; /* err set by AppendHandshake. */
        }
    }
    rv = ssl3_AppendHandshakeVariable(ss, buf.data, buf.len, 2);
    if (rv != SECSuccess) {
        goto done; /* error code set by AppendHandshake */
    }

done:
    if (buf.data)
        PORT_Free(buf.data);
    return rv;
}

/* Once a cipher suite has been selected, make sure that the necessary secondary
 * information is properly set. */
SECStatus
ssl3_SetCipherSuite(sslSocket *ss, ssl3CipherSuite chosenSuite,
                    PRBool initHashes)
{
    ss->ssl3.hs.cipher_suite = chosenSuite;
    ss->ssl3.hs.suite_def = ssl_LookupCipherSuiteDef(chosenSuite);
    if (!ss->ssl3.hs.suite_def) {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    ss->ssl3.hs.kea_def = &kea_defs[ss->ssl3.hs.suite_def->key_exchange_alg];
    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_cipher_suite;

    if (!initHashes) {
        return SECSuccess;
    }
    /* Now we've have a cipher suite, initialize the handshake hashes. */
    return ssl3_InitHandshakeHashes(ss);
}

/* Called from ssl3_HandleHandshakeMessage() when it has deciphered a complete
 * ssl3 ServerHello message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleServerHello(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    PRInt32 temp; /* allow for consume number failure */
    PRBool suite_found = PR_FALSE;
    int i;
    int errCode = SSL_ERROR_RX_MALFORMED_SERVER_HELLO;
    SECStatus rv;
    SECItem sidBytes = { siBuffer, NULL, 0 };
    PRBool isTLS = PR_FALSE;
    SSL3AlertDescription desc = illegal_parameter;
#ifndef TLS_1_3_DRAFT_VERSION
    SSL3ProtocolVersion downgradeCheckVersion;
#endif

    SSL_TRC(3, ("%d: SSL3[%d]: handle server_hello handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->ssl3.initialized);

    if (ss->ssl3.hs.ws != wait_server_hello) {
        errCode = SSL_ERROR_RX_UNEXPECTED_SERVER_HELLO;
        desc = unexpected_message;
        goto alert_loser;
    }

    /* clean up anything left from previous handshake. */
    if (ss->ssl3.clientCertChain != NULL) {
        CERT_DestroyCertificateList(ss->ssl3.clientCertChain);
        ss->ssl3.clientCertChain = NULL;
    }
    if (ss->ssl3.clientCertificate != NULL) {
        CERT_DestroyCertificate(ss->ssl3.clientCertificate);
        ss->ssl3.clientCertificate = NULL;
    }
    if (ss->ssl3.clientPrivateKey != NULL) {
        SECKEY_DestroyPrivateKey(ss->ssl3.clientPrivateKey);
        ss->ssl3.clientPrivateKey = NULL;
    }

    rv = ssl_ClientReadVersion(ss, &b, &length, &ss->version);
    if (rv != SECSuccess) {
        goto loser; /* alert has been sent */
    }

    /* We got a HelloRetryRequest, but the server didn't pick 1.3.  Scream. */
    if (ss->ssl3.hs.helloRetry && ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        desc = illegal_parameter;
        errCode = SSL_ERROR_RX_MALFORMED_SERVER_HELLO;
        goto alert_loser;
    }

    /* Check that the server negotiated the same version as it did
     * in the first handshake. This isn't really the best place for
     * us to be getting this version number, but it's what we have.
     * (1294697). */
    if (ss->firstHsDone && (ss->version != ss->ssl3.crSpec->version)) {
        desc = illegal_parameter;
        errCode = SSL_ERROR_UNSUPPORTED_VERSION;
        goto alert_loser;
    }
    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_version;
    isTLS = (ss->version > SSL_LIBRARY_VERSION_3_0);

    rv = ssl3_ConsumeHandshake(
        ss, &ss->ssl3.hs.server_random, SSL3_RANDOM_LENGTH, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* alert has been sent */
    }

#ifndef TLS_1_3_DRAFT_VERSION
    /* Check the ServerHello.random per
     * [draft-ietf-tls-tls13-11 Section 6.3.1.1].
     *
     * TLS 1.3 clients receiving a TLS 1.2 or below ServerHello MUST check
     * that the top eight octets are not equal to either of these values.
     * TLS 1.2 clients SHOULD also perform this check if the ServerHello
     * indicates TLS 1.1 or below.  If a match is found the client MUST
     * abort the handshake with a fatal "illegal_parameter" alert.
     *
     * Disable this test during the TLS 1.3 draft version period.
     */
    downgradeCheckVersion = ss->ssl3.downgradeCheckVersion ? ss->ssl3.downgradeCheckVersion
                                                           : ss->vrange.max;

    if (downgradeCheckVersion >= SSL_LIBRARY_VERSION_TLS_1_2 &&
        downgradeCheckVersion > ss->version) {
        /* Both sections use the same sentinel region. */
        unsigned char *downgrade_sentinel =
            ss->ssl3.hs.server_random.rand +
            SSL3_RANDOM_LENGTH - sizeof(tls13_downgrade_random);
        if (!PORT_Memcmp(downgrade_sentinel,
                         tls13_downgrade_random,
                         sizeof(tls13_downgrade_random)) ||
            !PORT_Memcmp(downgrade_sentinel,
                         tls12_downgrade_random,
                         sizeof(tls12_downgrade_random))) {
            desc = illegal_parameter;
            errCode = SSL_ERROR_RX_MALFORMED_SERVER_HELLO;
            goto alert_loser;
        }
    }
#endif

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = ssl3_ConsumeHandshakeVariable(ss, &sidBytes, 1, &b, &length);
        if (rv != SECSuccess) {
            goto loser; /* alert has been sent */
        }
        if (sidBytes.len > SSL3_SESSIONID_BYTES) {
            if (isTLS)
                desc = decode_error;
            goto alert_loser; /* malformed. */
        }
    }

    /* find selected cipher suite in our list. */
    temp = ssl3_ConsumeHandshakeNumber(ss, 2, &b, &length);
    if (temp < 0) {
        goto loser; /* alert has been sent */
    }
    i = ssl3_config_match_init(ss);
    PORT_Assert(i > 0);
    if (i <= 0) {
        errCode = PORT_GetError();
        goto loser;
    }
    for (i = 0; i < ssl_V3_SUITES_IMPLEMENTED; i++) {
        ssl3CipherSuiteCfg *suite = &ss->cipherSuites[i];
        if (temp == suite->cipher_suite) {
            SSLVersionRange vrange = { ss->version, ss->version };
            if (!config_match(suite, ss->ssl3.policy, &vrange, ss)) {
                /* config_match already checks whether the cipher suite is
                 * acceptable for the version, but the check is repeated here
                 * in order to give a more precise error code. */
                if (!ssl3_CipherSuiteAllowedForVersionRange(temp, &vrange)) {
                    desc = handshake_failure;
                    errCode = SSL_ERROR_CIPHER_DISALLOWED_FOR_VERSION;
                    goto alert_loser;
                }

                break; /* failure */
            }

            suite_found = PR_TRUE;
            break; /* success */
        }
    }
    if (!suite_found) {
        desc = handshake_failure;
        errCode = SSL_ERROR_NO_CYPHER_OVERLAP;
        goto alert_loser;
    }

    rv = ssl3_SetCipherSuite(ss, (ssl3CipherSuite)temp, PR_TRUE);
    if (rv != SECSuccess) {
        desc = internal_error;
        errCode = PORT_GetError();
        goto alert_loser;
    }

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        /* find selected compression method in our list. */
        temp = ssl3_ConsumeHandshakeNumber(ss, 1, &b, &length);
        if (temp < 0) {
            goto loser; /* alert has been sent */
        }
        suite_found = PR_FALSE;
        for (i = 0; i < ssl_compression_method_count; i++) {
            if (temp == ssl_compression_methods[i]) {
                if (!ssl_CompressionEnabled(ss, ssl_compression_methods[i])) {
                    break; /* failure */
                }
                suite_found = PR_TRUE;
                break; /* success */
            }
        }
        if (!suite_found) {
            desc = handshake_failure;
            errCode = SSL_ERROR_NO_COMPRESSION_OVERLAP;
            goto alert_loser;
        }
        ss->ssl3.hs.compression = (SSLCompressionMethod)temp;
    } else {
        ss->ssl3.hs.compression = ssl_compression_null;
    }

    /* Note that if !isTLS and the extra stuff is not extensions, we
     * do NOT goto alert_loser.
     * There are some old SSL 3.0 implementations that do send stuff
     * after the end of the server hello, and we deliberately ignore
     * such stuff in the interest of maximal interoperability (being
     * "generous in what you accept").
     * Update: Starting in NSS 3.12.6, we handle the renegotiation_info
     * extension in SSL 3.0.
     */
    if (length != 0) {
        SECItem extensions;
        rv = ssl3_ConsumeHandshakeVariable(ss, &extensions, 2, &b, &length);
        if (rv != SECSuccess || length != 0) {
            if (isTLS)
                goto alert_loser;
        } else {
            rv = ssl3_HandleExtensions(ss, &extensions.data,
                                       &extensions.len, server_hello);
            if (rv != SECSuccess)
                goto alert_loser;
        }
    }

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = tls13_HandleServerHelloPart2(ss);
        if (rv != SECSuccess) {
            errCode = PORT_GetError();
            goto loser;
        }
    } else {
        rv = ssl3_HandleServerHelloPart2(ss, &sidBytes, &errCode);
        if (rv != SECSuccess)
            goto loser;
    }

    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);

loser:
    /* Clean up the temporary pointer to the handshake buffer. */
    ss->xtnData.signedCertTimestamps.len = 0;
    ssl_MapLowLevelError(errCode);
    return SECFailure;
}

static SECStatus
ssl3_HandleServerHelloPart2(sslSocket *ss, const SECItem *sidBytes,
                            int *retErrCode)
{
    SSL3AlertDescription desc = handshake_failure;
    int errCode = SSL_ERROR_RX_MALFORMED_SERVER_HELLO;
    SECStatus rv;
    PRBool sid_match;
    sslSessionID *sid = ss->sec.ci.sid;

    if ((ss->opt.requireSafeNegotiation ||
         (ss->firstHsDone && (ss->peerRequestedProtection ||
                              ss->opt.enableRenegotiation ==
                                  SSL_RENEGOTIATE_REQUIRES_XTN))) &&
        !ssl3_ExtensionNegotiated(ss, ssl_renegotiation_info_xtn)) {
        desc = handshake_failure;
        errCode = ss->firstHsDone ? SSL_ERROR_RENEGOTIATION_NOT_ALLOWED
                                  : SSL_ERROR_UNSAFE_NEGOTIATION;
        goto alert_loser;
    }

    /* Any errors after this point are not "malformed" errors. */
    desc = handshake_failure;

    /* we need to call ssl3_SetupPendingCipherSpec here so we can check the
     * key exchange algorithm. */
    rv = ssl3_SetupPendingCipherSpec(ss);
    if (rv != SECSuccess) {
        goto alert_loser; /* error code is set. */
    }

    /* We may or may not have sent a session id, we may get one back or
     * not and if so it may match the one we sent.
     * Attempt to restore the master secret to see if this is so...
     * Don't consider failure to find a matching SID an error.
     */
    sid_match = (PRBool)(sidBytes->len > 0 &&
                         sidBytes->len ==
                             sid->u.ssl3.sessionIDLength &&
                         !PORT_Memcmp(sid->u.ssl3.sessionID,
                                      sidBytes->data, sidBytes->len));

    if (sid_match) {
        if (sid->version != ss->version ||
            sid->u.ssl3.cipherSuite != ss->ssl3.hs.cipher_suite) {
            errCode = SSL_ERROR_RX_MALFORMED_SERVER_HELLO;
            goto alert_loser;
        }
        do {
            ssl3CipherSpec *pwSpec = ss->ssl3.pwSpec;

            SECItem wrappedMS; /* wrapped master secret. */

            /* [draft-ietf-tls-session-hash-06; Section 5.3]
             *
             * o  If the original session did not use the "extended_master_secret"
             *    extension but the new ServerHello contains the extension, the
             *    client MUST abort the handshake.
             */
            if (!sid->u.ssl3.keys.extendedMasterSecretUsed &&
                ssl3_ExtensionNegotiated(ss, ssl_extended_master_secret_xtn)) {
                errCode = SSL_ERROR_UNEXPECTED_EXTENDED_MASTER_SECRET;
                goto alert_loser;
            }

            /*
             *   o  If the original session used an extended master secret but the new
             *      ServerHello does not contain the "extended_master_secret"
             *      extension, the client SHOULD abort the handshake.
             *
             * TODO(ekr@rtfm.com): Add option to refuse to resume when EMS is not
             * used at all (bug 1176526).
             */
            if (sid->u.ssl3.keys.extendedMasterSecretUsed &&
                !ssl3_ExtensionNegotiated(ss, ssl_extended_master_secret_xtn)) {
                errCode = SSL_ERROR_MISSING_EXTENDED_MASTER_SECRET;
                goto alert_loser;
            }

            ss->sec.authType = sid->authType;
            ss->sec.authKeyBits = sid->authKeyBits;
            ss->sec.keaType = sid->keaType;
            ss->sec.keaKeyBits = sid->keaKeyBits;

            if (sid->u.ssl3.keys.msIsWrapped) {
                PK11SlotInfo *slot;
                PK11SymKey *wrapKey; /* wrapping key */
                CK_FLAGS keyFlags = 0;

                /* unwrap master secret */
                slot = SECMOD_LookupSlot(sid->u.ssl3.masterModuleID,
                                         sid->u.ssl3.masterSlotID);
                if (slot == NULL) {
                    break; /* not considered an error. */
                }
                if (!PK11_IsPresent(slot)) {
                    PK11_FreeSlot(slot);
                    break; /* not considered an error. */
                }
                wrapKey = PK11_GetWrapKey(slot, sid->u.ssl3.masterWrapIndex,
                                          sid->u.ssl3.masterWrapMech,
                                          sid->u.ssl3.masterWrapSeries,
                                          ss->pkcs11PinArg);
                PK11_FreeSlot(slot);
                if (wrapKey == NULL) {
                    break; /* not considered an error. */
                }

                if (ss->version > SSL_LIBRARY_VERSION_3_0) { /* isTLS */
                    keyFlags =
                        CKF_SIGN | CKF_VERIFY;
                }

                wrappedMS.data = sid->u.ssl3.keys.wrapped_master_secret;
                wrappedMS.len = sid->u.ssl3.keys.wrapped_master_secret_len;
                pwSpec->master_secret =
                    PK11_UnwrapSymKeyWithFlags(wrapKey, sid->u.ssl3.masterWrapMech,
                                               NULL, &wrappedMS, CKM_SSL3_MASTER_KEY_DERIVE,
                                               CKA_DERIVE, sizeof(SSL3MasterSecret), keyFlags);
                errCode = PORT_GetError();
                PK11_FreeSymKey(wrapKey);
                if (pwSpec->master_secret == NULL) {
                    break; /* errorCode set just after call to UnwrapSymKey. */
                }
            } else {
                /* need to import the raw master secret to session object */
                PK11SlotInfo *slot = PK11_GetInternalSlot();
                wrappedMS.data = sid->u.ssl3.keys.wrapped_master_secret;
                wrappedMS.len = sid->u.ssl3.keys.wrapped_master_secret_len;
                pwSpec->master_secret =
                    PK11_ImportSymKey(slot, CKM_SSL3_MASTER_KEY_DERIVE,
                                      PK11_OriginUnwrap, CKA_ENCRYPT,
                                      &wrappedMS, NULL);
                PK11_FreeSlot(slot);
                if (pwSpec->master_secret == NULL) {
                    break;
                }
            }

            /* Got a Match */
            SSL_AtomicIncrementLong(&ssl3stats.hsh_sid_cache_hits);

            /* If we sent a session ticket, then this is a stateless resume. */
            if (ss->xtnData.sentSessionTicketInClientHello)
                SSL_AtomicIncrementLong(&ssl3stats.hsh_sid_stateless_resumes);

            if (ssl3_ExtensionNegotiated(ss, ssl_session_ticket_xtn))
                ss->ssl3.hs.ws = wait_new_session_ticket;
            else
                ss->ssl3.hs.ws = wait_change_cipher;

            ss->ssl3.hs.isResuming = PR_TRUE;

            /* copy the peer cert from the SID */
            if (sid->peerCert != NULL) {
                ss->sec.peerCert = CERT_DupCertificate(sid->peerCert);
            }

            /* NULL value for PMS because we are reusing the old MS */
            rv = ssl3_InitPendingCipherSpec(ss, NULL);
            if (rv != SECSuccess) {
                goto alert_loser; /* err code was set */
            }
            return SECSuccess;
        } while (0);
    }

    if (sid_match)
        SSL_AtomicIncrementLong(&ssl3stats.hsh_sid_cache_not_ok);
    else
        SSL_AtomicIncrementLong(&ssl3stats.hsh_sid_cache_misses);

    /* throw the old one away */
    sid->u.ssl3.keys.resumable = PR_FALSE;
    ss->sec.uncache(sid);
    ssl_FreeSID(sid);

    /* get a new sid */
    ss->sec.ci.sid = sid = ssl3_NewSessionID(ss, PR_FALSE);
    if (sid == NULL) {
        goto alert_loser; /* memory error is set. */
    }

    sid->version = ss->version;
    sid->u.ssl3.sessionIDLength = sidBytes->len;
    if (sidBytes->len > 0) {
        PORT_Memcpy(sid->u.ssl3.sessionID, sidBytes->data, sidBytes->len);
    }

    sid->u.ssl3.keys.extendedMasterSecretUsed =
        ssl3_ExtensionNegotiated(ss, ssl_extended_master_secret_xtn);

    /* Copy Signed Certificate Timestamps, if any. */
    if (ss->xtnData.signedCertTimestamps.len) {
        rv = SECITEM_CopyItem(NULL, &sid->u.ssl3.signedCertTimestamps,
                              &ss->xtnData.signedCertTimestamps);
        ss->xtnData.signedCertTimestamps.len = 0;
        if (rv != SECSuccess)
            goto loser;
    }

    ss->ssl3.hs.isResuming = PR_FALSE;
    if (ss->ssl3.hs.kea_def->authKeyType != ssl_auth_null) {
        /* All current cipher suites other than those with ssl_auth_null (i.e.,
         * (EC)DH_anon_* suites) require a certificate, so use that signal. */
        ss->ssl3.hs.ws = wait_server_cert;
    } else {
        /* All the remaining cipher suites must be (EC)DH_anon_* and so
         * must be ephemeral. Note, if we ever add PSK this might
         * change. */
        PORT_Assert(ss->ssl3.hs.kea_def->ephemeral);
        ss->ssl3.hs.ws = wait_server_key;
    }
    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);

loser:
    *retErrCode = errCode;
    return SECFailure;
}

static SECStatus
ssl_HandleDHServerKeyExchange(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;
    int errCode = SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH;
    SSL3AlertDescription desc = illegal_parameter;
    SSLHashType hashAlg;
    PRBool isTLS = ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0;
    SSLSignatureScheme sigScheme;

    SECItem dh_p = { siBuffer, NULL, 0 };
    SECItem dh_g = { siBuffer, NULL, 0 };
    SECItem dh_Ys = { siBuffer, NULL, 0 };
    unsigned dh_p_bits;
    unsigned dh_g_bits;
    PRInt32 minDH;

    SSL3Hashes hashes;
    SECItem signature = { siBuffer, NULL, 0 };
    PLArenaPool *arena = NULL;
    SECKEYPublicKey *peerKey = NULL;

    rv = ssl3_ConsumeHandshakeVariable(ss, &dh_p, 2, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }

    rv = NSS_OptionGet(NSS_DH_MIN_KEY_SIZE, &minDH);
    if (rv != SECSuccess) {
        minDH = SSL_DH_MIN_P_BITS;
    }
    dh_p_bits = SECKEY_BigIntegerBitLength(&dh_p);
    if (dh_p_bits < minDH) {
        errCode = SSL_ERROR_WEAK_SERVER_EPHEMERAL_DH_KEY;
        goto alert_loser;
    }
    rv = ssl3_ConsumeHandshakeVariable(ss, &dh_g, 2, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }
    /* Abort if dh_g is 0, 1, or obviously too big. */
    dh_g_bits = SECKEY_BigIntegerBitLength(&dh_g);
    if (dh_g_bits > dh_p_bits || dh_g_bits <= 1) {
        goto alert_loser;
    }
    if (ss->opt.requireDHENamedGroups) {
        /* If we're doing named groups, make sure it's good. */
        rv = ssl_ValidateDHENamedGroup(ss, &dh_p, &dh_g, NULL, NULL);
        if (rv != SECSuccess) {
            errCode = SSL_ERROR_WEAK_SERVER_EPHEMERAL_DH_KEY;
            goto alert_loser;
        }
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &dh_Ys, 2, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }
    if (!ssl_IsValidDHEShare(&dh_p, &dh_Ys)) {
        errCode = SSL_ERROR_RX_MALFORMED_DHE_KEY_SHARE;
        goto alert_loser;
    }

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_2) {
        rv = ssl_ConsumeSignatureScheme(ss, &b, &length, &sigScheme);
        if (rv != SECSuccess) {
            goto loser; /* malformed or unsupported. */
        }
        rv = ssl_CheckSignatureSchemeConsistency(ss, sigScheme,
                                                 ss->sec.peerCert);
        if (rv != SECSuccess) {
            goto loser;
        }
        hashAlg = ssl_SignatureSchemeToHashType(sigScheme);
    } else {
        /* Use ssl_hash_none to represent the MD5+SHA1 combo. */
        hashAlg = ssl_hash_none;
        sigScheme = ssl_sig_none;
    }
    rv = ssl3_ConsumeHandshakeVariable(ss, &signature, 2, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }
    if (length != 0) {
        if (isTLS) {
            desc = decode_error;
        }
        goto alert_loser; /* malformed. */
    }

    PRINT_BUF(60, (NULL, "Server DH p", dh_p.data, dh_p.len));
    PRINT_BUF(60, (NULL, "Server DH g", dh_g.data, dh_g.len));
    PRINT_BUF(60, (NULL, "Server DH Ys", dh_Ys.data, dh_Ys.len));

    /* failures after this point are not malformed handshakes. */
    /* TLS: send decrypt_error if signature failed. */
    desc = isTLS ? decrypt_error : handshake_failure;

    /*
     * Check to make sure the hash is signed by right guy.
     */
    rv = ssl3_ComputeDHKeyHash(ss, hashAlg, &hashes,
                               dh_p, dh_g, dh_Ys, PR_FALSE /* padY */);
    if (rv != SECSuccess) {
        errCode =
            ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        goto alert_loser;
    }
    rv = ssl3_VerifySignedHashes(ss, sigScheme, &hashes, &signature);
    if (rv != SECSuccess) {
        errCode =
            ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        goto alert_loser;
    }

    /*
     * we really need to build a new key here because we can no longer
     * ignore calling SECKEY_DestroyPublicKey. Using the key may allocate
     * pkcs11 slots and ID's.
     */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        errCode = SEC_ERROR_NO_MEMORY;
        goto loser;
    }

    peerKey = PORT_ArenaZNew(arena, SECKEYPublicKey);
    if (peerKey == NULL) {
        errCode = SEC_ERROR_NO_MEMORY;
        goto loser;
    }

    peerKey->arena = arena;
    peerKey->keyType = dhKey;
    peerKey->pkcs11Slot = NULL;
    peerKey->pkcs11ID = CK_INVALID_HANDLE;

    if (SECITEM_CopyItem(arena, &peerKey->u.dh.prime, &dh_p) ||
        SECITEM_CopyItem(arena, &peerKey->u.dh.base, &dh_g) ||
        SECITEM_CopyItem(arena, &peerKey->u.dh.publicValue, &dh_Ys)) {
        errCode = SEC_ERROR_NO_MEMORY;
        goto loser;
    }
    ss->sec.peerKey = peerKey;
    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);
loser:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    PORT_SetError(ssl_MapLowLevelError(errCode));
    return SECFailure;
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered a
 * complete ssl3 ServerKeyExchange message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleServerKeyExchange(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: handle server_key_exchange handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->ssl3.hs.ws != wait_server_key) {
        SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH);
        return SECFailure;
    }

    switch (ss->ssl3.hs.kea_def->exchKeyType) {
        case ssl_kea_dh:
            rv = ssl_HandleDHServerKeyExchange(ss, b, length);
            break;

        case ssl_kea_ecdh:
            rv = ssl3_HandleECDHServerKeyExchange(ss, b, length);
            break;

        default:
            SSL3_SendAlert(ss, alert_fatal, handshake_failure);
            PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
            rv = SECFailure;
            break;
    }

    if (rv == SECSuccess) {
        ss->ssl3.hs.ws = wait_cert_request;
    }
    /* All Handle*ServerKeyExchange functions set the error code. */
    return rv;
}

typedef struct dnameNode {
    struct dnameNode *next;
    SECItem name;
} dnameNode;

/*
 * Parse the ca_list structure in a CertificateRequest.
 *
 * Called from:
 * ssl3_HandleCertificateRequest
 * tls13_HandleCertificateRequest
 */
SECStatus
ssl3_ParseCertificateRequestCAs(sslSocket *ss, SSL3Opaque **b, PRUint32 *length,
                                PLArenaPool *arena, CERTDistNames *ca_list)
{
    PRInt32 remaining;
    int nnames = 0;
    dnameNode *node;
    int i;

    remaining = ssl3_ConsumeHandshakeNumber(ss, 2, b, length);
    if (remaining < 0)
        return SECFailure; /* malformed, alert has been sent */

    if ((PRUint32)remaining > *length)
        goto alert_loser;

    ca_list->head = node = PORT_ArenaZNew(arena, dnameNode);
    if (node == NULL)
        goto no_mem;

    while (remaining > 0) {
        PRInt32 len;

        if (remaining < 2)
            goto alert_loser; /* malformed */

        node->name.len = len = ssl3_ConsumeHandshakeNumber(ss, 2, b, length);
        if (len <= 0)
            return SECFailure; /* malformed, alert has been sent */

        remaining -= 2;
        if (remaining < len)
            goto alert_loser; /* malformed */

        node->name.data = *b;
        *b += len;
        *length -= len;
        remaining -= len;
        nnames++;
        if (remaining <= 0)
            break; /* success */

        node->next = PORT_ArenaZNew(arena, dnameNode);
        node = node->next;
        if (node == NULL)
            goto no_mem;
    }

    ca_list->nnames = nnames;
    ca_list->names = PORT_ArenaNewArray(arena, SECItem, nnames);
    if (nnames > 0 && ca_list->names == NULL)
        goto no_mem;

    for (i = 0, node = (dnameNode *)ca_list->head;
         i < nnames;
         i++, node = node->next) {
        ca_list->names[i] = node->name;
    }

    return SECSuccess;

no_mem:
    PORT_SetError(SEC_ERROR_NO_MEMORY);
    return SECFailure;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal,
                         ss->version < SSL_LIBRARY_VERSION_TLS_1_0 ? illegal_parameter
                                                                   : decode_error);
    PORT_SetError(SSL_ERROR_RX_MALFORMED_CERT_REQUEST);
    return SECFailure;
}

SECStatus
ssl_ParseSignatureSchemes(const sslSocket *ss, PLArenaPool *arena,
                          SSLSignatureScheme **schemesOut,
                          unsigned int *numSchemesOut,
                          unsigned char **b, unsigned int *len)
{
    SECStatus rv;
    SECItem buf;
    SSLSignatureScheme *schemes;
    unsigned int numSchemes = 0;
    unsigned int max;

    rv = ssl3_ExtConsumeHandshakeVariable(ss, &buf, 2, b, len);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* An empty or odd-length value is invalid. */
    if (buf.len == 0 || (buf.len & 1) != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        return SECFailure;
    }

    /* Limit the number of schemes we read. */
    max = PR_MIN(buf.len / 2, MAX_SIGNATURE_SCHEMES);

    if (arena) {
        schemes = PORT_ArenaZNewArray(arena, SSLSignatureScheme, max);
    } else {
        schemes = PORT_ZNewArray(SSLSignatureScheme, max);
    }
    if (!schemes) {
        ssl3_ExtSendAlert(ss, alert_fatal, internal_error);
        return SECFailure;
    }

    for (; max; --max) {
        PRInt32 tmp;
        tmp = ssl3_ExtConsumeHandshakeNumber(ss, 2, &buf.data, &buf.len);
        if (tmp < 0) {
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
        if (ssl_IsSupportedSignatureScheme((SSLSignatureScheme)tmp)) {
            schemes[numSchemes++] = (SSLSignatureScheme)tmp;
        }
    }

    if (!numSchemes) {
        if (!arena) {
            PORT_Free(schemes);
        }
        schemes = NULL;
    }

    *schemesOut = schemes;
    *numSchemesOut = numSchemes;
    return SECSuccess;
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered
 * a complete ssl3 Certificate Request message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleCertificateRequest(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    PLArenaPool *arena = NULL;
    PRBool isTLS = PR_FALSE;
    PRBool isTLS12 = PR_FALSE;
    int errCode = SSL_ERROR_RX_MALFORMED_CERT_REQUEST;
    SECStatus rv;
    SSL3AlertDescription desc = illegal_parameter;
    SECItem cert_types = { siBuffer, NULL, 0 };
    SSLSignatureScheme *signatureSchemes = NULL;
    unsigned int signatureSchemeCount = 0;
    CERTDistNames ca_list;

    SSL_TRC(3, ("%d: SSL3[%d]: handle certificate_request handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->ssl3.hs.ws != wait_cert_request) {
        desc = unexpected_message;
        errCode = SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST;
        goto alert_loser;
    }

    PORT_Assert(ss->ssl3.clientCertChain == NULL);
    PORT_Assert(ss->ssl3.clientCertificate == NULL);
    PORT_Assert(ss->ssl3.clientPrivateKey == NULL);

    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);
    isTLS12 = (PRBool)(ss->ssl3.prSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2);
    rv = ssl3_ConsumeHandshakeVariable(ss, &cert_types, 1, &b, &length);
    if (rv != SECSuccess)
        goto loser; /* malformed, alert has been sent */

    arena = ca_list.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        goto no_mem;

    if (isTLS12) {
        rv = ssl_ParseSignatureSchemes(ss, arena,
                                       &signatureSchemes,
                                       &signatureSchemeCount,
                                       &b, &length);
        if (rv != SECSuccess) {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_CERT_REQUEST);
            goto loser; /* malformed, alert has been sent */
        }
    }

    rv = ssl3_ParseCertificateRequestCAs(ss, &b, &length, arena, &ca_list);
    if (rv != SECSuccess)
        goto done; /* alert sent in ssl3_ParseCertificateRequestCAs */

    if (length != 0)
        goto alert_loser; /* malformed */

    ss->ssl3.hs.ws = wait_hello_done;

    rv = ssl3_CompleteHandleCertificateRequest(ss, signatureSchemes,
                                               signatureSchemeCount, &ca_list);
    if (rv == SECFailure) {
        PORT_Assert(0);
        errCode = SEC_ERROR_LIBRARY_FAILURE;
        desc = internal_error;
        goto alert_loser;
    }
    goto done;

no_mem:
    rv = SECFailure;
    PORT_SetError(SEC_ERROR_NO_MEMORY);
    goto done;

alert_loser:
    if (isTLS && desc == illegal_parameter)
        desc = decode_error;
    (void)SSL3_SendAlert(ss, alert_fatal, desc);
loser:
    PORT_SetError(errCode);
    rv = SECFailure;
done:
    if (arena != NULL)
        PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

SECStatus
ssl3_CompleteHandleCertificateRequest(sslSocket *ss,
                                      const SSLSignatureScheme *signatureSchemes,
                                      unsigned int signatureSchemeCount,
                                      CERTDistNames *ca_list)
{
    SECStatus rv;

    if (ss->getClientAuthData != NULL) {
        PORT_Assert((ss->ssl3.hs.preliminaryInfo & ssl_preinfo_all) ==
                    ssl_preinfo_all);
        /* XXX Should pass cert_types and algorithms in this call!! */
        rv = (SECStatus)(*ss->getClientAuthData)(ss->getClientAuthDataArg,
                                                 ss->fd, ca_list,
                                                 &ss->ssl3.clientCertificate,
                                                 &ss->ssl3.clientPrivateKey);
    } else {
        rv = SECFailure; /* force it to send a no_certificate alert */
    }
    switch (rv) {
        case SECWouldBlock: /* getClientAuthData has put up a dialog box. */
            ssl3_SetAlwaysBlock(ss);
            break; /* not an error */

        case SECSuccess:
            /* check what the callback function returned */
            if ((!ss->ssl3.clientCertificate) || (!ss->ssl3.clientPrivateKey)) {
                /* we are missing either the key or cert */
                if (ss->ssl3.clientCertificate) {
                    /* got a cert, but no key - free it */
                    CERT_DestroyCertificate(ss->ssl3.clientCertificate);
                    ss->ssl3.clientCertificate = NULL;
                }
                if (ss->ssl3.clientPrivateKey) {
                    /* got a key, but no cert - free it */
                    SECKEY_DestroyPrivateKey(ss->ssl3.clientPrivateKey);
                    ss->ssl3.clientPrivateKey = NULL;
                }
                goto send_no_certificate;
            }
            /* Setting ssl3.clientCertChain non-NULL will cause
             * ssl3_HandleServerHelloDone to call SendCertificate.
             */
            ss->ssl3.clientCertChain = CERT_CertChainFromCert(
                ss->ssl3.clientCertificate,
                certUsageSSLClient, PR_FALSE);
            if (ss->ssl3.clientCertChain == NULL) {
                CERT_DestroyCertificate(ss->ssl3.clientCertificate);
                ss->ssl3.clientCertificate = NULL;
                SECKEY_DestroyPrivateKey(ss->ssl3.clientPrivateKey);
                ss->ssl3.clientPrivateKey = NULL;
                goto send_no_certificate;
            }
            if (ss->ssl3.hs.hashType == handshake_hash_record ||
                ss->ssl3.hs.hashType == handshake_hash_single) {
                rv = ssl_PickClientSignatureScheme(ss, signatureSchemes,
                                                   signatureSchemeCount);
            }
            break; /* not an error */

        case SECFailure:
        default:
        send_no_certificate:
            if (ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0) {
                ss->ssl3.sendEmptyCert = PR_TRUE;
            } else {
                (void)SSL3_SendAlert(ss, alert_warning, no_certificate);
            }
            rv = SECSuccess;
            break;
    }

    return rv;
}

static SECStatus
ssl3_CheckFalseStart(sslSocket *ss)
{
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(!ss->ssl3.hs.authCertificatePending);
    PORT_Assert(!ss->ssl3.hs.canFalseStart);

    if (!ss->canFalseStartCallback) {
        SSL_TRC(3, ("%d: SSL[%d]: no false start callback so no false start",
                    SSL_GETPID(), ss->fd));
    } else {
        PRBool maybeFalseStart;
        SECStatus rv;

        /* An attacker can control the selected ciphersuite so we only wish to
         * do False Start in the case that the selected ciphersuite is
         * sufficiently strong that the attack can gain no advantage.
         * Therefore we always require an 80-bit cipher. */
        ssl_GetSpecReadLock(ss);
        maybeFalseStart = ss->ssl3.cwSpec->cipher_def->secret_key_size >= 10;
        ssl_ReleaseSpecReadLock(ss);

        if (!maybeFalseStart) {
            SSL_TRC(3, ("%d: SSL[%d]: no false start due to weak cipher",
                        SSL_GETPID(), ss->fd));
        } else {
            PORT_Assert((ss->ssl3.hs.preliminaryInfo & ssl_preinfo_all) ==
                        ssl_preinfo_all);
            rv = (ss->canFalseStartCallback)(ss->fd,
                                             ss->canFalseStartCallbackData,
                                             &ss->ssl3.hs.canFalseStart);
            if (rv == SECSuccess) {
                SSL_TRC(3, ("%d: SSL[%d]: false start callback returned %s",
                            SSL_GETPID(), ss->fd,
                            ss->ssl3.hs.canFalseStart ? "TRUE"
                                                      : "FALSE"));
            } else {
                SSL_TRC(3, ("%d: SSL[%d]: false start callback failed (%s)",
                            SSL_GETPID(), ss->fd,
                            PR_ErrorToName(PR_GetError())));
            }
            return rv;
        }
    }

    ss->ssl3.hs.canFalseStart = PR_FALSE;
    return SECSuccess;
}

PRBool
ssl3_WaitingForServerSecondRound(sslSocket *ss)
{
    PRBool result;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    switch (ss->ssl3.hs.ws) {
        case wait_new_session_ticket:
        case wait_change_cipher:
        case wait_finished:
            result = PR_TRUE;
            break;
        default:
            result = PR_FALSE;
            break;
    }

    return result;
}

static SECStatus ssl3_SendClientSecondRound(sslSocket *ss);

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered
 * a complete ssl3 Server Hello Done message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleServerHelloDone(sslSocket *ss)
{
    SECStatus rv;
    SSL3WaitState ws = ss->ssl3.hs.ws;

    SSL_TRC(3, ("%d: SSL3[%d]: handle server_hello_done handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    /* Skipping CertificateRequest is always permitted. */
    if (ws != wait_hello_done &&
        ws != wait_cert_request) {
        SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
        return SECFailure;
    }

    rv = ssl3_SendClientSecondRound(ss);

    return rv;
}

/* Called from ssl3_HandleServerHelloDone and ssl3_AuthCertificateComplete.
 *
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_SendClientSecondRound(sslSocket *ss)
{
    SECStatus rv;
    PRBool sendClientCert;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    sendClientCert = !ss->ssl3.sendEmptyCert &&
                     ss->ssl3.clientCertChain != NULL &&
                     ss->ssl3.clientPrivateKey != NULL;

    /* We must wait for the server's certificate to be authenticated before
     * sending the client certificate in order to disclosing the client
     * certificate to an attacker that does not have a valid cert for the
     * domain we are connecting to.
     *
     * XXX: We should do the same for the NPN extension, but for that we
     * need an option to give the application the ability to leak the NPN
     * information to get better performance.
     *
     * During the initial handshake on a connection, we never send/receive
     * application data until we have authenticated the server's certificate;
     * i.e. we have fully authenticated the handshake before using the cipher
     * specs agreed upon for that handshake. During a renegotiation, we may
     * continue sending and receiving application data during the handshake
     * interleaved with the handshake records. If we were to send the client's
     * second round for a renegotiation before the server's certificate was
     * authenticated, then the application data sent/received after this point
     * would be using cipher spec that hadn't been authenticated. By waiting
     * until the server's certificate has been authenticated during
     * renegotiations, we ensure that renegotiations have the same property
     * as initial handshakes; i.e. we have fully authenticated the handshake
     * before using the cipher specs agreed upon for that handshake for
     * application data.
     */
    if (ss->ssl3.hs.restartTarget) {
        PR_NOT_REACHED("unexpected ss->ssl3.hs.restartTarget");
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (ss->ssl3.hs.authCertificatePending &&
        (sendClientCert || ss->ssl3.sendEmptyCert || ss->firstHsDone)) {
        SSL_TRC(3, ("%d: SSL3[%p]: deferring ssl3_SendClientSecondRound because"
                    " certificate authentication is still pending.",
                    SSL_GETPID(), ss->fd));
        ss->ssl3.hs.restartTarget = ssl3_SendClientSecondRound;
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

    rv = ssl3_SendClientKeyExchange(ss);
    if (rv != SECSuccess) {
        goto loser; /* err is set. */
    }

    if (sendClientCert) {
        rv = ssl3_SendCertificateVerify(ss, ss->ssl3.clientPrivateKey);
        SECKEY_DestroyPrivateKey(ss->ssl3.clientPrivateKey);
        ss->ssl3.clientPrivateKey = NULL;
        if (rv != SECSuccess) {
            goto loser; /* err is set. */
        }
    }

    rv = ssl3_SendChangeCipherSpecs(ss);
    if (rv != SECSuccess) {
        goto loser; /* err code was set. */
    }

    /* This must be done after we've set ss->ssl3.cwSpec in
     * ssl3_SendChangeCipherSpecs because SSL_GetChannelInfo uses information
     * from cwSpec. This must be done before we call ssl3_CheckFalseStart
     * because the false start callback (if any) may need the information from
     * the functions that depend on this being set.
     */
    ss->enoughFirstHsDone = PR_TRUE;

    if (!ss->firstHsDone) {
        /* XXX: If the server's certificate hasn't been authenticated by this
         * point, then we may be leaking this NPN message to an attacker.
         */
        rv = ssl3_SendNextProto(ss);
        if (rv != SECSuccess) {
            goto loser; /* err code was set. */
        }

        if (ss->opt.enableFalseStart) {
            if (!ss->ssl3.hs.authCertificatePending) {
                /* When we fix bug 589047, we will need to know whether we are
                 * false starting before we try to flush the client second
                 * round to the network. With that in mind, we purposefully
                 * call ssl3_CheckFalseStart before calling ssl3_SendFinished,
                 * which includes a call to ssl3_FlushHandshake, so that
                 * no application develops a reliance on such flushing being
                 * done before its false start callback is called.
                 */
                ssl_ReleaseXmitBufLock(ss);
                rv = ssl3_CheckFalseStart(ss);
                ssl_GetXmitBufLock(ss);
                if (rv != SECSuccess) {
                    goto loser;
                }
            } else {
                /* The certificate authentication and the server's Finished
                 * message are racing each other. If the certificate
                 * authentication wins, then we will try to false start in
                 * ssl3_AuthCertificateComplete.
                 */
                SSL_TRC(3, ("%d: SSL3[%p]: deferring false start check because"
                            " certificate authentication is still pending.",
                            SSL_GETPID(), ss->fd));
            }
        }
    }

    rv = ssl3_SendFinished(ss, 0);
    if (rv != SECSuccess) {
        goto loser; /* err code was set. */
    }

    ssl_ReleaseXmitBufLock(ss); /*******************************/

    if (ssl3_ExtensionNegotiated(ss, ssl_session_ticket_xtn))
        ss->ssl3.hs.ws = wait_new_session_ticket;
    else
        ss->ssl3.hs.ws = wait_change_cipher;

    PORT_Assert(ssl3_WaitingForServerSecondRound(ss));

    return SECSuccess;

loser:
    ssl_ReleaseXmitBufLock(ss);
    return rv;
}

/*
 * Routines used by servers
 */
static SECStatus
ssl3_SendHelloRequest(sslSocket *ss)
{
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: send hello_request handshake", SSL_GETPID(),
                ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    rv = ssl3_AppendHandshakeHeader(ss, hello_request, 0);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake */
    }
    rv = ssl3_FlushHandshake(ss, 0);
    if (rv != SECSuccess) {
        return rv; /* error code set by ssl3_FlushHandshake */
    }
    ss->ssl3.hs.ws = wait_client_hello;
    return SECSuccess;
}

/*
 * Called from:
 *  ssl3_HandleClientHello()
 */
static SECComparison
ssl3_ServerNameCompare(const SECItem *name1, const SECItem *name2)
{
    if (!name1 != !name2) {
        return SECLessThan;
    }
    if (!name1) {
        return SECEqual;
    }
    if (name1->type != name2->type) {
        return SECLessThan;
    }
    return SECITEM_CompareItem(name1, name2);
}

/* Sets memory error when returning NULL.
 * Called from:
 *  ssl3_SendClientHello()
 *  ssl3_HandleServerHello()
 *  ssl3_HandleClientHello()
 *  ssl3_HandleV2ClientHello()
 */
sslSessionID *
ssl3_NewSessionID(sslSocket *ss, PRBool is_server)
{
    sslSessionID *sid;

    sid = PORT_ZNew(sslSessionID);
    if (sid == NULL)
        return sid;

    if (is_server) {
        const SECItem *srvName;
        SECStatus rv = SECSuccess;

        ssl_GetSpecReadLock(ss); /********************************/
        srvName = &ss->ssl3.hs.srvVirtName;
        if (srvName->len && srvName->data) {
            rv = SECITEM_CopyItem(NULL, &sid->u.ssl3.srvName, srvName);
        }
        ssl_ReleaseSpecReadLock(ss); /************************************/
        if (rv != SECSuccess) {
            PORT_Free(sid);
            return NULL;
        }
    }
    sid->peerID = (ss->peerID == NULL) ? NULL : PORT_Strdup(ss->peerID);
    sid->urlSvrName = (ss->url == NULL) ? NULL : PORT_Strdup(ss->url);
    sid->addr = ss->sec.ci.peer;
    sid->port = ss->sec.ci.port;
    sid->references = 1;
    sid->cached = never_cached;
    sid->version = ss->version;

    sid->u.ssl3.keys.resumable = PR_TRUE;
    sid->u.ssl3.policy = SSL_ALLOWED;
    sid->u.ssl3.clientWriteKey = NULL;
    sid->u.ssl3.serverWriteKey = NULL;
    sid->u.ssl3.keys.extendedMasterSecretUsed = PR_FALSE;

    if (is_server) {
        SECStatus rv;
        int pid = SSL_GETPID();

        sid->u.ssl3.sessionIDLength = SSL3_SESSIONID_BYTES;
        sid->u.ssl3.sessionID[0] = (pid >> 8) & 0xff;
        sid->u.ssl3.sessionID[1] = pid & 0xff;
        rv = PK11_GenerateRandom(sid->u.ssl3.sessionID + 2,
                                 SSL3_SESSIONID_BYTES - 2);
        if (rv != SECSuccess) {
            ssl_FreeSID(sid);
            ssl_MapLowLevelError(SSL_ERROR_GENERATE_RANDOM_FAILURE);
            return NULL;
        }
    }
    return sid;
}

/* Called from:  ssl3_HandleClientHello, ssl3_HandleV2ClientHello */
static SECStatus
ssl3_SendServerHelloSequence(sslSocket *ss)
{
    const ssl3KEADef *kea_def;
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: begin send server_hello sequence",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    rv = ssl3_SendServerHello(ss);
    if (rv != SECSuccess) {
        return rv; /* err code is set. */
    }
    rv = ssl3_SendCertificate(ss);
    if (rv != SECSuccess) {
        return rv; /* error code is set. */
    }
    rv = ssl3_SendCertificateStatus(ss);
    if (rv != SECSuccess) {
        return rv; /* error code is set. */
    }
    /* We have to do this after the call to ssl3_SendServerHello,
     * because kea_def is set up by ssl3_SendServerHello().
     */
    kea_def = ss->ssl3.hs.kea_def;

    if (kea_def->ephemeral) {
        rv = ssl3_SendServerKeyExchange(ss);
        if (rv != SECSuccess) {
            return rv; /* err code was set. */
        }
    }

    if (ss->opt.requestCertificate) {
        rv = ssl3_SendCertificateRequest(ss);
        if (rv != SECSuccess) {
            return rv; /* err code is set. */
        }
    }
    rv = ssl3_SendServerHelloDone(ss);
    if (rv != SECSuccess) {
        return rv; /* err code is set. */
    }

    ss->ssl3.hs.ws = (ss->opt.requestCertificate) ? wait_client_cert
                                                  : wait_client_key;
    return SECSuccess;
}

/* An empty TLS Renegotiation Info (RI) extension */
static const PRUint8 emptyRIext[5] = { 0xff, 0x01, 0x00, 0x01, 0x00 };

static PRBool
ssl3_KEASupportsTickets(const ssl3KEADef *kea_def)
{
    if (kea_def->signKeyType == dsaKey) {
        /* TODO: Fix session tickets for DSS. The server code rejects the
         * session ticket received from the client. Bug 1174677 */
        return PR_FALSE;
    }
    return PR_TRUE;
}

/* Select a cipher suite.
**
** NOTE: This suite selection algorithm should be the same as the one in
** ssl3_HandleV2ClientHello().
**
** If TLS 1.0 is enabled, we could handle the case where the client
** offered TLS 1.1 but offered only export cipher suites by choosing TLS
** 1.0 and selecting one of those export cipher suites. However, a secure
** TLS 1.1 client should not have export cipher suites enabled at all,
** and a TLS 1.1 client should definitely not be offering *only* export
** cipher suites. Therefore, we refuse to negotiate export cipher suites
** with any client that indicates support for TLS 1.1 or higher when we
** (the server) have TLS 1.1 support enabled.
*/
SECStatus
ssl3_NegotiateCipherSuite(sslSocket *ss, const SECItem *suites,
                          PRBool initHashes)
{
    int j;
    int i;

    for (j = 0; j < ssl_V3_SUITES_IMPLEMENTED; j++) {
        ssl3CipherSuiteCfg *suite = &ss->cipherSuites[j];
        SSLVersionRange vrange = { ss->version, ss->version };
        if (!config_match(suite, ss->ssl3.policy, &vrange, ss)) {
            continue;
        }
        for (i = 0; i + 1 < suites->len; i += 2) {
            PRUint16 suite_i = (suites->data[i] << 8) | suites->data[i + 1];
            if (suite_i == suite->cipher_suite) {
                return ssl3_SetCipherSuite(ss, suite_i, initHashes);
            }
        }
    }
    return SECFailure;
}

/*
 * Call the SNI config hook.
 *
 * Called from:
 *   ssl3_HandleClientHello
 *   tls13_HandleClientHelloPart2
 */
SECStatus
ssl3_ServerCallSNICallback(sslSocket *ss)
{
    int errCode = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
    SSL3AlertDescription desc = illegal_parameter;
    int ret = 0;

#ifdef SSL_SNI_ALLOW_NAME_CHANGE_2HS
#error("No longer allowed to set SSL_SNI_ALLOW_NAME_CHANGE_2HS")
#endif
    if (!ssl3_ExtensionNegotiated(ss, ssl_server_name_xtn)) {
        if (ss->firstHsDone) {
            /* Check that we don't have the name is current spec
             * if this extension was not negotiated on the 2d hs. */
            PRBool passed = PR_TRUE;
            ssl_GetSpecReadLock(ss); /*******************************/
            if (ss->ssl3.hs.srvVirtName.data) {
                passed = PR_FALSE;
            }
            ssl_ReleaseSpecReadLock(ss); /***************************/
            if (!passed) {
                errCode = SSL_ERROR_UNRECOGNIZED_NAME_ALERT;
                desc = handshake_failure;
                goto alert_loser;
            }
        }
        return SECSuccess;
    }

    if (ss->sniSocketConfig)
        do { /* not a loop */
            PORT_Assert((ss->ssl3.hs.preliminaryInfo & ssl_preinfo_all) ==
                        ssl_preinfo_all);

            ret = SSL_SNI_SEND_ALERT;
            /* If extension is negotiated, the len of names should > 0. */
            if (ss->xtnData.sniNameArrSize) {
                /* Calling client callback to reconfigure the socket. */
                ret = (SECStatus)(*ss->sniSocketConfig)(ss->fd,
                                                        ss->xtnData.sniNameArr,
                                                        ss->xtnData.sniNameArrSize,
                                                        ss->sniSocketConfigArg);
            }
            if (ret <= SSL_SNI_SEND_ALERT) {
                /* Application does not know the name or was not able to
                 * properly reconfigure the socket. */
                errCode = SSL_ERROR_UNRECOGNIZED_NAME_ALERT;
                desc = unrecognized_name;
                break;
            } else if (ret == SSL_SNI_CURRENT_CONFIG_IS_USED) {
                SECStatus rv = SECSuccess;
                SECItem pwsNameBuf = { 0, NULL, 0 };
                SECItem *pwsName = &pwsNameBuf;
                SECItem *cwsName;

                ssl_GetSpecWriteLock(ss); /*******************************/
                cwsName = &ss->ssl3.hs.srvVirtName;
                /* not allow name change on the 2d HS */
                if (ss->firstHsDone) {
                    if (ssl3_ServerNameCompare(pwsName, cwsName)) {
                        ssl_ReleaseSpecWriteLock(ss); /******************/
                        errCode = SSL_ERROR_UNRECOGNIZED_NAME_ALERT;
                        desc = handshake_failure;
                        ret = SSL_SNI_SEND_ALERT;
                        break;
                    }
                }
                if (pwsName->data) {
                    SECITEM_FreeItem(pwsName, PR_FALSE);
                }
                if (cwsName->data) {
                    rv = SECITEM_CopyItem(NULL, pwsName, cwsName);
                }
                ssl_ReleaseSpecWriteLock(ss); /**************************/
                if (rv != SECSuccess) {
                    errCode = SSL_ERROR_INTERNAL_ERROR_ALERT;
                    desc = internal_error;
                    ret = SSL_SNI_SEND_ALERT;
                    break;
                }
            } else if ((unsigned int)ret < ss->xtnData.sniNameArrSize) {
                /* Application has configured new socket info. Lets check it
                 * and save the name. */
                SECStatus rv;
                SECItem *name = &ss->xtnData.sniNameArr[ret];
                int configedCiphers;
                SECItem *pwsName;

                /* get rid of the old name and save the newly picked. */
                /* This code is protected by ssl3HandshakeLock. */
                ssl_GetSpecWriteLock(ss); /*******************************/
                /* not allow name change on the 2d HS */
                if (ss->firstHsDone) {
                    SECItem *cwsName = &ss->ssl3.hs.srvVirtName;
                    if (ssl3_ServerNameCompare(name, cwsName)) {
                        ssl_ReleaseSpecWriteLock(ss); /******************/
                        errCode = SSL_ERROR_UNRECOGNIZED_NAME_ALERT;
                        desc = handshake_failure;
                        ret = SSL_SNI_SEND_ALERT;
                        break;
                    }
                }
                pwsName = &ss->ssl3.hs.srvVirtName;
                if (pwsName->data) {
                    SECITEM_FreeItem(pwsName, PR_FALSE);
                }
                rv = SECITEM_CopyItem(NULL, pwsName, name);
                ssl_ReleaseSpecWriteLock(ss); /***************************/
                if (rv != SECSuccess) {
                    errCode = SSL_ERROR_INTERNAL_ERROR_ALERT;
                    desc = internal_error;
                    ret = SSL_SNI_SEND_ALERT;
                    break;
                }
                configedCiphers = ssl3_config_match_init(ss);
                if (configedCiphers <= 0) {
                    /* no ciphers are working/supported */
                    errCode = PORT_GetError();
                    desc = handshake_failure;
                    ret = SSL_SNI_SEND_ALERT;
                    break;
                }
                /* Need to tell the client that application has picked
                 * the name from the offered list and reconfigured the socket.
                 */
                ssl3_RegisterExtensionSender(ss, &ss->xtnData, ssl_server_name_xtn,
                                             ssl3_SendServerNameXtn);
            } else {
                /* Callback returned index outside of the boundary. */
                PORT_Assert((unsigned int)ret < ss->xtnData.sniNameArrSize);
                errCode = SSL_ERROR_INTERNAL_ERROR_ALERT;
                desc = internal_error;
                ret = SSL_SNI_SEND_ALERT;
                break;
            }
        } while (0);
    ssl3_FreeSniNameArray(&ss->xtnData);
    if (ret <= SSL_SNI_SEND_ALERT) {
        /* desc and errCode should be set. */
        goto alert_loser;
    }

    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);
    PORT_SetError(errCode);
    return SECFailure;
}

SECStatus
ssl3_SelectServerCert(sslSocket *ss)
{
    const ssl3KEADef *kea_def = ss->ssl3.hs.kea_def;
    PRCList *cursor;

    /* This picks the first certificate that has:
     * a) the right authentication method, and
     * b) the right named curve (EC only)
     *
     * We might want to do some sort of ranking here later.  For now, it's all
     * based on what order they are configured in. */
    for (cursor = PR_NEXT_LINK(&ss->serverCerts);
         cursor != &ss->serverCerts;
         cursor = PR_NEXT_LINK(cursor)) {
        sslServerCert *cert = (sslServerCert *)cursor;
        if (cert->certType.authType != kea_def->authKeyType) {
            continue;
        }
        if ((cert->certType.authType == ssl_auth_ecdsa ||
             cert->certType.authType == ssl_auth_ecdh_rsa ||
             cert->certType.authType == ssl_auth_ecdh_ecdsa) &&
            !ssl_NamedGroupEnabled(ss, cert->certType.namedCurve)) {
            continue;
        }

        /* Found one. */
        ss->sec.serverCert = cert;
        ss->sec.authType = cert->certType.authType;
        ss->sec.authKeyBits = cert->serverKeyBits;

        /* Don't pick a signature scheme if we aren't going to use it. */
        if (kea_def->signKeyType == nullKey) {
            return SECSuccess;
        }
        return ssl3_PickServerSignatureScheme(ss);
    }

    PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
    return SECFailure;
}

/* Called from ssl3_HandleHandshakeMessage() when it has deciphered a complete
 * ssl3 Client Hello message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleClientHello(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    sslSessionID *sid = NULL;
    PRInt32 tmp;
    unsigned int i;
    SECStatus rv;
    int errCode = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
    SSL3AlertDescription desc = illegal_parameter;
    SSL3AlertLevel level = alert_fatal;
    SSL3ProtocolVersion version;
    TLSExtension *versionExtension;
    SECItem sidBytes = { siBuffer, NULL, 0 };
    SECItem cookieBytes = { siBuffer, NULL, 0 };
    SECItem suites = { siBuffer, NULL, 0 };
    SECItem comps = { siBuffer, NULL, 0 };
    PRBool isTLS13;

    SSL_TRC(3, ("%d: SSL3[%d]: handle client_hello handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->ssl3.initialized);
    ss->ssl3.hs.preliminaryInfo = 0;

    if (!ss->sec.isServer ||
        (ss->ssl3.hs.ws != wait_client_hello &&
         ss->ssl3.hs.ws != idle_handshake)) {
        desc = unexpected_message;
        errCode = SSL_ERROR_RX_UNEXPECTED_CLIENT_HELLO;
        goto alert_loser;
    }
    if (ss->ssl3.hs.ws == idle_handshake) {
        /* Refuse re-handshake when we have already negotiated TLS 1.3. */
        if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
            desc = unexpected_message;
            errCode = SSL_ERROR_RENEGOTIATION_NOT_ALLOWED;
            goto alert_loser;
        }
        if (ss->opt.enableRenegotiation == SSL_RENEGOTIATE_NEVER) {
            desc = no_renegotiation;
            level = alert_warning;
            errCode = SSL_ERROR_RENEGOTIATION_NOT_ALLOWED;
            goto alert_loser;
        }
    }

    /* Get peer name of client */
    rv = ssl_GetPeerInfo(ss);
    if (rv != SECSuccess) {
        return rv; /* error code is set. */
    }

    /* We might be starting session renegotiation in which case we should
     * clear previous state.
     */
    ssl3_ResetExtensionData(&ss->xtnData);
    ss->statelessResume = PR_FALSE;

    if (IS_DTLS(ss)) {
        dtls_RehandshakeCleanup(ss);
    }

    tmp = ssl3_ConsumeHandshakeNumber(ss, 2, &b, &length);
    if (tmp < 0)
        goto loser; /* malformed, alert already sent */

    /* Translate the version. */
    if (IS_DTLS(ss)) {
        ss->clientHelloVersion = version =
            dtls_DTLSVersionToTLSVersion((SSL3ProtocolVersion)tmp);
    } else {
        ss->clientHelloVersion = version = (SSL3ProtocolVersion)tmp;
    }

    /* Grab the client random data. */
    rv = ssl3_ConsumeHandshake(
        ss, &ss->ssl3.hs.client_random, SSL3_RANDOM_LENGTH, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed */
    }

    /* Grab the client's SID, if present. */
    rv = ssl3_ConsumeHandshakeVariable(ss, &sidBytes, 1, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed */
    }

    /* Grab the client's cookie, if present. */
    if (IS_DTLS(ss)) {
        rv = ssl3_ConsumeHandshakeVariable(ss, &cookieBytes, 1, &b, &length);
        if (rv != SECSuccess) {
            goto loser; /* malformed */
        }
    }

    /* Grab the list of cipher suites. */
    rv = ssl3_ConsumeHandshakeVariable(ss, &suites, 2, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed */
    }

    /* Grab the list of compression methods. */
    rv = ssl3_ConsumeHandshakeVariable(ss, &comps, 1, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed */
    }

    /* Handle TLS hello extensions for SSL3 & TLS. We do not know if
     * we are restarting a previous session until extensions have been
     * parsed, since we might have received a SessionTicket extension.
     * Note: we allow extensions even when negotiating SSL3 for the sake
     * of interoperability (and backwards compatibility).
     */

    if (length) {
        /* Get length of hello extensions */
        PRInt32 extension_length;
        extension_length = ssl3_ConsumeHandshakeNumber(ss, 2, &b, &length);
        if (extension_length < 0) {
            goto loser; /* alert already sent */
        }
        if (extension_length != length) {
            ssl3_DecodeError(ss); /* send alert */
            goto loser;
        }

        rv = ssl3_ParseExtensions(ss, &b, &length);
        if (rv != SECSuccess) {
            goto loser; /* malformed */
        }
    }

    versionExtension = ssl3_FindExtension(ss, ssl_tls13_supported_versions_xtn);
    if (versionExtension) {
        rv = tls13_NegotiateVersion(ss, versionExtension);
        if (rv != SECSuccess) {
            errCode = PORT_GetError();
            desc = (errCode == SSL_ERROR_UNSUPPORTED_VERSION) ? protocol_version : illegal_parameter;
            goto alert_loser;
        }
    } else {
        /* The PR_MIN here ensures that we never negotiate 1.3 if the
         * peer didn't offer "supported_versions". */
        rv = ssl3_NegotiateVersion(ss,
                                   PR_MIN(version,
                                          SSL_LIBRARY_VERSION_TLS_1_2),
                                   PR_TRUE);
        if (rv != SECSuccess) {
            desc = (version > SSL_LIBRARY_VERSION_3_0) ? protocol_version
                                                       : handshake_failure;
            errCode = SSL_ERROR_UNSUPPORTED_VERSION;
            goto alert_loser;
        }
    }
    isTLS13 = ss->version >= SSL_LIBRARY_VERSION_TLS_1_3;
    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_version;

    /* You can't resume TLS 1.3 like this. */
    if (isTLS13 && sidBytes.len) {
        goto alert_loser;
    }

    /* Generate the Server Random now so it is available
     * when we process the ClientKeyShare in TLS 1.3 */
    rv = ssl3_GetNewRandom(&ss->ssl3.hs.server_random);
    if (rv != SECSuccess) {
        errCode = SSL_ERROR_GENERATE_RANDOM_FAILURE;
        goto loser;
    }

#ifndef TLS_1_3_DRAFT_VERSION
    /*
     * [draft-ietf-tls-tls13-11 Section 6.3.1.1].
     * TLS 1.3 server implementations which respond to a ClientHello with a
     * client_version indicating TLS 1.2 or below MUST set the last eight
     * bytes of their Random value to the bytes:
     *
     * 44 4F 57 4E 47 52 44 01
     *
     * TLS 1.2 server implementations which respond to a ClientHello with a
     * client_version indicating TLS 1.1 or below SHOULD set the last eight
     * bytes of their Random value to the bytes:
     *
     * 44 4F 57 4E 47 52 44 00
     *
     * TODO(ekr@rtfm.com): Note this change was not added in the SSLv2
     * compat processing code since that will most likely be removed before
     * we ship the final version of TLS 1.3. Bug 1306672.
     */
    if (ss->vrange.max > ss->version) {
        unsigned char *downgrade_sentinel =
            ss->ssl3.hs.server_random.rand +
            SSL3_RANDOM_LENGTH - sizeof(tls13_downgrade_random);

        switch (ss->vrange.max) {
            case SSL_LIBRARY_VERSION_TLS_1_3:
                PORT_Memcpy(downgrade_sentinel,
                            tls13_downgrade_random,
                            sizeof(tls13_downgrade_random));
                break;
            case SSL_LIBRARY_VERSION_TLS_1_2:
                PORT_Memcpy(downgrade_sentinel,
                            tls12_downgrade_random,
                            sizeof(tls12_downgrade_random));
                break;
            default:
                /* Do not change random. */
                break;
        }
    }
#endif

    /* Now parse the rest of the extensions. */
    rv = ssl3_HandleParsedExtensions(ss, client_hello);
    if (rv != SECSuccess) {
        goto loser; /* malformed */
    }

    /* If the ClientHello version is less than our maximum version, check for a
     * TLS_FALLBACK_SCSV and reject the connection if found. */
    if (ss->vrange.max > ss->clientHelloVersion) {
        for (i = 0; i + 1 < suites.len; i += 2) {
            PRUint16 suite_i = (suites.data[i] << 8) | suites.data[i + 1];
            if (suite_i != TLS_FALLBACK_SCSV)
                continue;
            desc = inappropriate_fallback;
            errCode = SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT;
            goto alert_loser;
        }
    }

    /* TLS 1.3 requires that compression only include null. */
    if (isTLS13) {
        if (comps.len != 1 || comps.data[0] != ssl_compression_null) {
            goto alert_loser;
        }
    }

    if (!ssl3_ExtensionNegotiated(ss, ssl_renegotiation_info_xtn)) {
        /* If we didn't receive an RI extension, look for the SCSV,
         * and if found, treat it just like an empty RI extension
         * by processing a local copy of an empty RI extension.
         */
        for (i = 0; i + 1 < suites.len; i += 2) {
            PRUint16 suite_i = (suites.data[i] << 8) | suites.data[i + 1];
            if (suite_i == TLS_EMPTY_RENEGOTIATION_INFO_SCSV) {
                SSL3Opaque *b2 = (SSL3Opaque *)emptyRIext;
                PRUint32 L2 = sizeof emptyRIext;
                (void)ssl3_HandleExtensions(ss, &b2, &L2, client_hello);
                break;
            }
        }
    }
    /* This is a second check for TLS 1.3 and re-handshake to stop us
     * from re-handshake up to TLS 1.3, so it happens after version
     * negotiation. */
    if (ss->firstHsDone && ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        desc = unexpected_message;
        errCode = SSL_ERROR_RENEGOTIATION_NOT_ALLOWED;
        goto alert_loser;
    }
    if (ss->firstHsDone &&
        (ss->opt.enableRenegotiation == SSL_RENEGOTIATE_REQUIRES_XTN ||
         ss->opt.enableRenegotiation == SSL_RENEGOTIATE_TRANSITIONAL) &&
        !ssl3_ExtensionNegotiated(ss, ssl_renegotiation_info_xtn)) {
        desc = no_renegotiation;
        level = alert_warning;
        errCode = SSL_ERROR_RENEGOTIATION_NOT_ALLOWED;
        goto alert_loser;
    }
    if ((ss->opt.requireSafeNegotiation ||
         (ss->firstHsDone && ss->peerRequestedProtection)) &&
        !ssl3_ExtensionNegotiated(ss, ssl_renegotiation_info_xtn)) {
        desc = handshake_failure;
        errCode = SSL_ERROR_UNSAFE_NEGOTIATION;
        goto alert_loser;
    }

    /* We do stateful resumes only if we are in TLS < 1.3 and
     * either of the following conditions are satisfied:
     * (1) the client does not support the session ticket extension, or
     * (2) the client support the session ticket extension, but sent an
     * empty ticket.
     */
    if ((ss->version < SSL_LIBRARY_VERSION_TLS_1_3) &&
        (!ssl3_ExtensionNegotiated(ss, ssl_session_ticket_xtn) ||
         ss->xtnData.emptySessionTicket)) {
        if (sidBytes.len > 0 && !ss->opt.noCache) {
            SSL_TRC(7, ("%d: SSL3[%d]: server, lookup client session-id for 0x%08x%08x%08x%08x",
                        SSL_GETPID(), ss->fd, ss->sec.ci.peer.pr_s6_addr32[0],
                        ss->sec.ci.peer.pr_s6_addr32[1],
                        ss->sec.ci.peer.pr_s6_addr32[2],
                        ss->sec.ci.peer.pr_s6_addr32[3]));
            if (ssl_sid_lookup) {
                sid = (*ssl_sid_lookup)(&ss->sec.ci.peer, sidBytes.data,
                                        sidBytes.len, ss->dbHandle);
            } else {
                errCode = SSL_ERROR_SERVER_CACHE_NOT_CONFIGURED;
                goto loser;
            }
        }
    } else if (ss->statelessResume) {
        /* Fill in the client's session ID if doing a stateless resume.
         * (When doing stateless resumes, server echos client's SessionID.)
         * This branch also handles TLS 1.3 resumption-PSK.
         */
        sid = ss->sec.ci.sid;
        PORT_Assert(sid != NULL); /* Should have already been filled in.*/

        if (sidBytes.len > 0 && sidBytes.len <= SSL3_SESSIONID_BYTES) {
            sid->u.ssl3.sessionIDLength = sidBytes.len;
            PORT_Memcpy(sid->u.ssl3.sessionID, sidBytes.data,
                        sidBytes.len);
            sid->u.ssl3.sessionIDLength = sidBytes.len;
        } else {
            sid->u.ssl3.sessionIDLength = 0;
        }
        ss->sec.ci.sid = NULL;
    }

    /* Free a potentially leftover session ID from a previous handshake. */
    if (ss->sec.ci.sid) {
        ssl_FreeSID(ss->sec.ci.sid);
        ss->sec.ci.sid = NULL;
    }

    if (sid != NULL) {
        /* We've found a session cache entry for this client.
         * Now, if we're going to require a client-auth cert,
         * and we don't already have this client's cert in the session cache,
         * and this is the first handshake on this connection (not a redo),
         * then drop this old cache entry and start a new session.
         */
        if ((sid->peerCert == NULL) && ss->opt.requestCertificate &&
            ((ss->opt.requireCertificate == SSL_REQUIRE_ALWAYS) ||
             (ss->opt.requireCertificate == SSL_REQUIRE_NO_ERROR) ||
             ((ss->opt.requireCertificate == SSL_REQUIRE_FIRST_HANDSHAKE) &&
              !ss->firstHsDone))) {

            SSL_AtomicIncrementLong(&ssl3stats.hch_sid_cache_not_ok);
            ss->sec.uncache(sid);
            ssl_FreeSID(sid);
            sid = NULL;
        }
    }

    if (IS_DTLS(ss)) {
        ssl3_DisableNonDTLSSuites(ss);
    }

#ifdef PARANOID
    /* Look for a matching cipher suite. */
    j = ssl3_config_match_init(ss);
    if (j <= 0) {                  /* no ciphers are working/supported by PK11 */
        errCode = PORT_GetError(); /* error code is already set. */
        goto alert_loser;
    }
#endif

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = tls13_HandleClientHelloPart2(ss, &suites, sid);
    } else {
        rv = ssl3_HandleClientHelloPart2(ss, &suites, &comps, sid);
    }
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        goto loser;
    }
    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, level, desc);
/* FALLTHRU */
loser:
    PORT_SetError(errCode);
    return SECFailure;
}

static SECStatus
ssl3_HandleClientHelloPart2(sslSocket *ss,
                            SECItem *suites,
                            SECItem *comps,
                            sslSessionID *sid)
{
    PRBool haveSpecWriteLock = PR_FALSE;
    PRBool haveXmitBufLock = PR_FALSE;
    int errCode = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
    SSL3AlertDescription desc = illegal_parameter;
    SECStatus rv;
    unsigned int i;
    int j;

    /* If we already have a session for this client, be sure to pick the
    ** same cipher suite and compression method we picked before.
    ** This is not a loop, despite appearances.
    */
    if (sid)
        do {
            ssl3CipherSuiteCfg *suite;
#ifdef PARANOID
            SSLVersionRange vrange = { ss->version, ss->version };
#endif

            /* Check that the cached compression method is still enabled. */
            if (!ssl_CompressionEnabled(ss, sid->u.ssl3.compression))
                break;

            /* Check that the cached compression method is in the client's list */
            for (i = 0; i < comps->len; i++) {
                if (comps->data[i] == sid->u.ssl3.compression)
                    break;
            }
            if (i == comps->len)
                break;

            suite = ss->cipherSuites;
            /* Find the entry for the cipher suite used in the cached session. */
            for (j = ssl_V3_SUITES_IMPLEMENTED; j > 0; --j, ++suite) {
                if (suite->cipher_suite == sid->u.ssl3.cipherSuite)
                    break;
            }
            PORT_Assert(j > 0);
            if (j <= 0)
                break;
#ifdef PARANOID
            /* Double check that the cached cipher suite is still enabled,
             * implemented, and allowed by policy.  Might have been disabled.
             * The product policy won't change during the process lifetime.
             * Implemented ("isPresent") shouldn't change for servers.
             */
            if (!config_match(suite, ss->ssl3.policy, &vrange, ss))
                break;
#else
            if (!suite->enabled)
                break;
#endif
            /* Double check that the cached cipher suite is in the client's
             * list.  If it isn't, fall through and start a new session. */
            for (i = 0; i + 1 < suites->len; i += 2) {
                PRUint16 suite_i = (suites->data[i] << 8) | suites->data[i + 1];
                if (suite_i == suite->cipher_suite) {
                    rv = ssl3_SetCipherSuite(ss, suite_i, PR_TRUE);
                    if (rv != SECSuccess) {
                        desc = internal_error;
                        errCode = PORT_GetError();
                        goto alert_loser;
                    }

                    /* Use the cached compression method. */
                    ss->ssl3.hs.compression =
                        sid->u.ssl3.compression;
                    goto compression_found;
                }
            }
        } while (0);
/* START A NEW SESSION */

#ifndef PARANOID
    /* Look for a matching cipher suite. */
    j = ssl3_config_match_init(ss);
    if (j <= 0) { /* no ciphers are working/supported by PK11 */
        desc = internal_error;
        errCode = PORT_GetError(); /* error code is already set. */
        goto alert_loser;
    }
#endif

    rv = ssl3_NegotiateCipherSuite(ss, suites, PR_TRUE);
    if (rv != SECSuccess) {
        desc = handshake_failure;
        errCode = SSL_ERROR_NO_CYPHER_OVERLAP;
        goto alert_loser;
    }

    /* Select a compression algorithm. */
    for (i = 0; i < comps->len; i++) {
        SSLCompressionMethod method = (SSLCompressionMethod)comps->data[i];
        if (!ssl_CompressionEnabled(ss, method))
            continue;
        for (j = 0; j < ssl_compression_method_count; j++) {
            if (method == ssl_compression_methods[j]) {
                ss->ssl3.hs.compression = ssl_compression_methods[j];
                goto compression_found;
            }
        }
    }
    errCode = SSL_ERROR_NO_COMPRESSION_OVERLAP;
    /* null compression must be supported */
    goto alert_loser;

compression_found:
    suites->data = NULL;
    comps->data = NULL;

    /* If there are any failures while processing the old sid,
     * we don't consider them to be errors.  Instead, We just behave
     * as if the client had sent us no sid to begin with, and make a new one.
     * The exception here is attempts to resume extended_master_secret
     * sessions without the extension, which causes an alert.
     */
    if (sid != NULL)
        do {
            ssl3CipherSpec *pwSpec;
            SECItem wrappedMS; /* wrapped key */
            const sslServerCert *serverCert;

            if (sid->version != ss->version ||
                sid->u.ssl3.cipherSuite != ss->ssl3.hs.cipher_suite ||
                sid->u.ssl3.compression != ss->ssl3.hs.compression) {
                break; /* not an error */
            }

            serverCert = ssl_FindServerCert(ss, &sid->certType);
            if (!serverCert || !serverCert->serverCert) {
                /* A compatible certificate must not have been configured.  It
                 * might not be the same certificate, but we only find that out
                 * when the ticket fails to decrypt. */
                break;
            }

            /* [draft-ietf-tls-session-hash-06; Section 5.3]
             * o  If the original session did not use the "extended_master_secret"
             *    extension but the new ClientHello contains the extension, then the
             *    server MUST NOT perform the abbreviated handshake.  Instead, it
             *    SHOULD continue with a full handshake (as described in
             *    Section 5.2) to negotiate a new session.
             *
             * o  If the original session used the "extended_master_secret"
             *    extension but the new ClientHello does not contain the extension,
             *    the server MUST abort the abbreviated handshake.
             */
            if (ssl3_ExtensionNegotiated(ss, ssl_extended_master_secret_xtn)) {
                if (!sid->u.ssl3.keys.extendedMasterSecretUsed) {
                    break; /* not an error */
                }
            } else {
                if (sid->u.ssl3.keys.extendedMasterSecretUsed) {
                    /* Note: we do not destroy the session */
                    desc = handshake_failure;
                    errCode = SSL_ERROR_MISSING_EXTENDED_MASTER_SECRET;
                    goto alert_loser;
                }
            }

            if (ss->sec.ci.sid) {
                ss->sec.uncache(ss->sec.ci.sid);
                PORT_Assert(ss->sec.ci.sid != sid); /* should be impossible, but ... */
                if (ss->sec.ci.sid != sid) {
                    ssl_FreeSID(ss->sec.ci.sid);
                }
                ss->sec.ci.sid = NULL;
            }
            /* we need to resurrect the master secret.... */

            ssl_GetSpecWriteLock(ss);
            haveSpecWriteLock = PR_TRUE;
            pwSpec = ss->ssl3.pwSpec;
            if (sid->u.ssl3.keys.msIsWrapped) {
                PK11SymKey *wrapKey; /* wrapping key */
                CK_FLAGS keyFlags = 0;

                wrapKey = ssl3_GetWrappingKey(ss, NULL, serverCert,
                                              sid->u.ssl3.masterWrapMech,
                                              ss->pkcs11PinArg);
                if (!wrapKey) {
                    /* we have a SID cache entry, but no wrapping key for it??? */
                    break;
                }

                if (ss->version > SSL_LIBRARY_VERSION_3_0) { /* isTLS */
                    keyFlags = CKF_SIGN | CKF_VERIFY;
                }

                wrappedMS.data = sid->u.ssl3.keys.wrapped_master_secret;
                wrappedMS.len = sid->u.ssl3.keys.wrapped_master_secret_len;

                /* unwrap the master secret. */
                pwSpec->master_secret =
                    PK11_UnwrapSymKeyWithFlags(wrapKey, sid->u.ssl3.masterWrapMech,
                                               NULL, &wrappedMS, CKM_SSL3_MASTER_KEY_DERIVE,
                                               CKA_DERIVE, sizeof(SSL3MasterSecret), keyFlags);
                PK11_FreeSymKey(wrapKey);
                if (pwSpec->master_secret == NULL) {
                    break; /* not an error */
                }
            } else {
                /* need to import the raw master secret to session object */
                PK11SlotInfo *slot;
                wrappedMS.data = sid->u.ssl3.keys.wrapped_master_secret;
                wrappedMS.len = sid->u.ssl3.keys.wrapped_master_secret_len;
                slot = PK11_GetInternalSlot();
                pwSpec->master_secret =
                    PK11_ImportSymKey(slot, CKM_SSL3_MASTER_KEY_DERIVE,
                                      PK11_OriginUnwrap, CKA_ENCRYPT, &wrappedMS,
                                      NULL);
                PK11_FreeSlot(slot);
                if (pwSpec->master_secret == NULL) {
                    break; /* not an error */
                }
            }
            ss->sec.ci.sid = sid;
            if (sid->peerCert != NULL) {
                ss->sec.peerCert = CERT_DupCertificate(sid->peerCert);
            }

            /*
             * Old SID passed all tests, so resume this old session.
             *
             * XXX make sure compression still matches
             */
            SSL_AtomicIncrementLong(&ssl3stats.hch_sid_cache_hits);
            if (ss->statelessResume)
                SSL_AtomicIncrementLong(&ssl3stats.hch_sid_stateless_resumes);
            ss->ssl3.hs.isResuming = PR_TRUE;

            ss->sec.authType = sid->authType;
            ss->sec.authKeyBits = sid->authKeyBits;
            ss->sec.keaType = sid->keaType;
            ss->sec.keaKeyBits = sid->keaKeyBits;

            /* server sids don't remember the server cert we previously sent,
            ** but they do remember the slot we originally used, so we
            ** can locate it again, provided that the current ssl socket
            ** has had its server certs configured the same as the previous one.
            */
            ss->sec.serverCert = serverCert;
            ss->sec.localCert = CERT_DupCertificate(serverCert->serverCert);

            /* Copy cached name in to pending spec */
            if (sid != NULL &&
                sid->version > SSL_LIBRARY_VERSION_3_0 &&
                sid->u.ssl3.srvName.len && sid->u.ssl3.srvName.data) {
                /* Set server name from sid */
                SECItem *sidName = &sid->u.ssl3.srvName;
                SECItem *pwsName = &ss->ssl3.hs.srvVirtName;
                if (pwsName->data) {
                    SECITEM_FreeItem(pwsName, PR_FALSE);
                }
                rv = SECITEM_CopyItem(NULL, pwsName, sidName);
                if (rv != SECSuccess) {
                    errCode = PORT_GetError();
                    desc = internal_error;
                    goto alert_loser;
                }
            }

            /* Clean up sni name array */
            ssl3_FreeSniNameArray(&ss->xtnData);

            ssl_GetXmitBufLock(ss);
            haveXmitBufLock = PR_TRUE;

            rv = ssl3_SendServerHello(ss);
            if (rv != SECSuccess) {
                errCode = PORT_GetError();
                goto loser;
            }

            if (haveSpecWriteLock) {
                ssl_ReleaseSpecWriteLock(ss);
                haveSpecWriteLock = PR_FALSE;
            }

            /* NULL value for PMS because we are re-using the old MS */
            rv = ssl3_InitPendingCipherSpec(ss, NULL);
            if (rv != SECSuccess) {
                errCode = PORT_GetError();
                goto loser;
            }

            rv = ssl3_SendChangeCipherSpecs(ss);
            if (rv != SECSuccess) {
                errCode = PORT_GetError();
                goto loser;
            }
            rv = ssl3_SendFinished(ss, 0);
            ss->ssl3.hs.ws = wait_change_cipher;
            if (rv != SECSuccess) {
                errCode = PORT_GetError();
                goto loser;
            }

            if (haveXmitBufLock) {
                ssl_ReleaseXmitBufLock(ss);
            }

            return SECSuccess;
        } while (0);

    if (haveSpecWriteLock) {
        ssl_ReleaseSpecWriteLock(ss);
        haveSpecWriteLock = PR_FALSE;
    }

    if (sid) { /* we had a sid, but it's no longer valid, free it */
        SSL_AtomicIncrementLong(&ssl3stats.hch_sid_cache_not_ok);
        ss->sec.uncache(sid);
        ssl_FreeSID(sid);
        sid = NULL;
    }
    SSL_AtomicIncrementLong(&ssl3stats.hch_sid_cache_misses);

    /* We only send a session ticket extension if the client supports
     * the extension and we are unable to resume.
     *
     * TODO: send a session ticket if performing a stateful
     * resumption.  (As per RFC4507, a server may issue a session
     * ticket while doing a (stateless or stateful) session resume,
     * but OpenSSL-0.9.8g does not accept session tickets while
     * resuming.)
     */
    if (ssl3_ExtensionNegotiated(ss, ssl_session_ticket_xtn) &&
        ssl3_KEASupportsTickets(ss->ssl3.hs.kea_def)) {
        ssl3_RegisterExtensionSender(ss, &ss->xtnData,
                                     ssl_session_ticket_xtn,
                                     ssl3_SendSessionTicketXtn);
    }

    rv = ssl3_ServerCallSNICallback(ss);
    if (rv != SECSuccess) {
        /* The alert has already been sent. */
        errCode = PORT_GetError();
        goto loser;
    }

    rv = ssl3_SelectServerCert(ss);
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        desc = handshake_failure;
        goto alert_loser;
    }

    sid = ssl3_NewSessionID(ss, PR_TRUE);
    if (sid == NULL) {
        errCode = PORT_GetError();
        goto loser; /* memory error is set. */
    }
    ss->sec.ci.sid = sid;

    sid->u.ssl3.keys.extendedMasterSecretUsed =
        ssl3_ExtensionNegotiated(ss, ssl_extended_master_secret_xtn);
    ss->ssl3.hs.isResuming = PR_FALSE;

    ssl_GetXmitBufLock(ss);
    rv = ssl3_SendServerHelloSequence(ss);
    ssl_ReleaseXmitBufLock(ss);
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        desc = handshake_failure;
        goto alert_loser;
    }

    if (haveXmitBufLock) {
        ssl_ReleaseXmitBufLock(ss);
    }

    return SECSuccess;

alert_loser:
    if (haveSpecWriteLock) {
        ssl_ReleaseSpecWriteLock(ss);
        haveSpecWriteLock = PR_FALSE;
    }
    (void)SSL3_SendAlert(ss, alert_fatal, desc);
/* FALLTHRU */
loser:
    if (sid && sid != ss->sec.ci.sid) {
        ss->sec.uncache(sid);
        ssl_FreeSID(sid);
    }

    if (haveSpecWriteLock) {
        ssl_ReleaseSpecWriteLock(ss);
    }

    if (haveXmitBufLock) {
        ssl_ReleaseXmitBufLock(ss);
    }

    PORT_SetError(errCode);
    return SECFailure;
}

/*
 * ssl3_HandleV2ClientHello is used when a V2 formatted hello comes
 * in asking to use the V3 handshake.
 */
SECStatus
ssl3_HandleV2ClientHello(sslSocket *ss, unsigned char *buffer, int length,
                         PRUint8 padding)
{
    sslSessionID *sid = NULL;
    unsigned char *suites;
    unsigned char *random;
    SSL3ProtocolVersion version;
    SECStatus rv;
    int i;
    int j;
    int sid_length;
    int suite_length;
    int rand_length;
    int errCode = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
    SSL3AlertDescription desc = handshake_failure;
    unsigned int total = SSL_HL_CLIENT_HELLO_HBYTES;

    SSL_TRC(3, ("%d: SSL3[%d]: handle v2 client_hello", SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));

    ssl_GetSSL3HandshakeLock(ss);

    ssl3_ResetExtensionData(&ss->xtnData);

    version = (buffer[1] << 8) | buffer[2];
    if (version < SSL_LIBRARY_VERSION_3_0) {
        goto loser;
    }

    rv = ssl3_InitState(ss);
    if (rv != SECSuccess) {
        ssl_ReleaseSSL3HandshakeLock(ss);
        return rv; /* ssl3_InitState has set the error code. */
    }
    rv = ssl3_RestartHandshakeHashes(ss);
    if (rv != SECSuccess) {
        ssl_ReleaseSSL3HandshakeLock(ss);
        return rv;
    }

    if (ss->ssl3.hs.ws != wait_client_hello) {
        desc = unexpected_message;
        errCode = SSL_ERROR_RX_UNEXPECTED_CLIENT_HELLO;
        goto alert_loser;
    }

    total += suite_length = (buffer[3] << 8) | buffer[4];
    total += sid_length = (buffer[5] << 8) | buffer[6];
    total += rand_length = (buffer[7] << 8) | buffer[8];
    total += padding;
    ss->clientHelloVersion = version;

    if (version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        /* [draft-ietf-tls-tls-11; C.3] forbids sending a TLS 1.3
         * ClientHello using the backwards-compatible format. */
        desc = illegal_parameter;
        errCode = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
        goto alert_loser;
    }

    rv = ssl3_NegotiateVersion(ss, version, PR_TRUE);
    if (rv != SECSuccess) {
        /* send back which ever alert client will understand. */
        desc = (version > SSL_LIBRARY_VERSION_3_0) ? protocol_version
                                                   : handshake_failure;
        errCode = SSL_ERROR_UNSUPPORTED_VERSION;
        goto alert_loser;
    }
    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_version;

    /* if we get a non-zero SID, just ignore it. */
    if (length != total) {
        SSL_DBG(("%d: SSL3[%d]: bad v2 client hello message, len=%d should=%d",
                 SSL_GETPID(), ss->fd, length, total));
        desc = illegal_parameter;
        errCode = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
        goto alert_loser;
    }

    suites = buffer + SSL_HL_CLIENT_HELLO_HBYTES;
    random = suites + suite_length + sid_length;

    if (rand_length < SSL_MIN_CHALLENGE_BYTES ||
        rand_length > SSL_MAX_CHALLENGE_BYTES) {
        desc = illegal_parameter;
        errCode = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
        goto alert_loser;
    }

    PORT_Assert(SSL_MAX_CHALLENGE_BYTES == SSL3_RANDOM_LENGTH);

    PORT_Memset(&ss->ssl3.hs.client_random, 0, SSL3_RANDOM_LENGTH);
    PORT_Memcpy(
        &ss->ssl3.hs.client_random.rand[SSL3_RANDOM_LENGTH - rand_length],
        random, rand_length);

    PRINT_BUF(60, (ss, "client random:", &ss->ssl3.hs.client_random.rand[0],
                   SSL3_RANDOM_LENGTH));
    i = ssl3_config_match_init(ss);
    if (i <= 0) {
        errCode = PORT_GetError(); /* error code is already set. */
        goto alert_loser;
    }

    /* Select a cipher suite.
    **
    ** NOTE: This suite selection algorithm should be the same as the one in
    ** ssl3_HandleClientHello().
    **
    ** See the comments about export cipher suites in ssl3_HandleClientHello().
    */
    for (j = 0; j < ssl_V3_SUITES_IMPLEMENTED; j++) {
        ssl3CipherSuiteCfg *suite = &ss->cipherSuites[j];
        SSLVersionRange vrange = { ss->version, ss->version };
        if (!config_match(suite, ss->ssl3.policy, &vrange, ss)) {
            continue;
        }
        for (i = 0; i + 2 < suite_length; i += 3) {
            PRUint32 suite_i = (suites[i] << 16) | (suites[i + 1] << 8) | suites[i + 2];
            if (suite_i == suite->cipher_suite) {
                rv = ssl3_SetCipherSuite(ss, suite_i, PR_TRUE);
                if (rv != SECSuccess) {
                    desc = internal_error;
                    errCode = PORT_GetError();
                    goto alert_loser;
                }
                goto suite_found;
            }
        }
    }
    errCode = SSL_ERROR_NO_CYPHER_OVERLAP;
    goto alert_loser;

suite_found:

    /* If the ClientHello version is less than our maximum version, check for a
     * TLS_FALLBACK_SCSV and reject the connection if found. */
    if (ss->vrange.max > ss->clientHelloVersion) {
        for (i = 0; i + 2 < suite_length; i += 3) {
            PRUint16 suite_i = (suites[i] << 16) | (suites[i + 1] << 8) | suites[i + 2];
            if (suite_i == TLS_FALLBACK_SCSV) {
                desc = inappropriate_fallback;
                errCode = SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT;
                goto alert_loser;
            }
        }
    }

    /* Look for the SCSV, and if found, treat it just like an empty RI
     * extension by processing a local copy of an empty RI extension.
     */
    for (i = 0; i + 2 < suite_length; i += 3) {
        PRUint32 suite_i = (suites[i] << 16) | (suites[i + 1] << 8) | suites[i + 2];
        if (suite_i == TLS_EMPTY_RENEGOTIATION_INFO_SCSV) {
            SSL3Opaque *b2 = (SSL3Opaque *)emptyRIext;
            PRUint32 L2 = sizeof emptyRIext;
            (void)ssl3_HandleExtensions(ss, &b2, &L2, client_hello);
            break;
        }
    }

    if (ss->opt.requireSafeNegotiation &&
        !ssl3_ExtensionNegotiated(ss, ssl_renegotiation_info_xtn)) {
        desc = handshake_failure;
        errCode = SSL_ERROR_UNSAFE_NEGOTIATION;
        goto alert_loser;
    }

    ss->ssl3.hs.compression = ssl_compression_null;

    rv = ssl3_SelectServerCert(ss);
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        desc = handshake_failure;
        goto alert_loser;
    }

    /* we don't even search for a cache hit here.  It's just a miss. */
    SSL_AtomicIncrementLong(&ssl3stats.hch_sid_cache_misses);
    sid = ssl3_NewSessionID(ss, PR_TRUE);
    if (sid == NULL) {
        errCode = PORT_GetError();
        goto loser; /* memory error is set. */
    }
    ss->sec.ci.sid = sid;
    /* do not worry about memory leak of sid since it now belongs to ci */

    /* We have to update the handshake hashes before we can send stuff */
    rv = ssl3_UpdateHandshakeHashes(ss, buffer, length);
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        goto loser;
    }

    ssl_GetXmitBufLock(ss);
    rv = ssl3_SendServerHelloSequence(ss);
    ssl_ReleaseXmitBufLock(ss);
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        goto loser;
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    return SECSuccess;

alert_loser:
    SSL3_SendAlert(ss, alert_fatal, desc);
loser:
    ssl_ReleaseSSL3HandshakeLock(ss);
    PORT_SetError(errCode);
    return SECFailure;
}

/* The negotiated version number has been already placed in ss->version.
**
** Called from:  ssl3_HandleClientHello                     (resuming session),
**  ssl3_SendServerHelloSequence <- ssl3_HandleClientHello   (new session),
**  ssl3_SendServerHelloSequence <- ssl3_HandleV2ClientHello (new session)
*/
SECStatus
ssl3_SendServerHello(sslSocket *ss)
{
    sslSessionID *sid;
    SECStatus rv;
    PRUint32 maxBytes = 65535;
    PRUint32 length;
    PRInt32 extensions_len = 0;
    SSL3ProtocolVersion version;

    SSL_TRC(3, ("%d: SSL3[%d]: send server_hello handshake", SSL_GETPID(),
                ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    PORT_Assert(MSB(ss->version) == MSB(SSL_LIBRARY_VERSION_3_0));
    if (MSB(ss->version) != MSB(SSL_LIBRARY_VERSION_3_0)) {
        PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
        return SECFailure;
    }

    sid = ss->sec.ci.sid;

    extensions_len = ssl3_CallHelloExtensionSenders(
        ss, PR_FALSE, maxBytes, &ss->xtnData.serverHelloSenders[0]);
    if (extensions_len > 0)
        extensions_len += 2; /* Add sizeof total extension length */

    /* TLS 1.3 doesn't use the session_id or compression_method
     * fields in the ServerHello. */
    length = sizeof(SSL3ProtocolVersion) + SSL3_RANDOM_LENGTH;
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        length += 1 + ((sid == NULL) ? 0 : sid->u.ssl3.sessionIDLength);
    }
    length += sizeof(ssl3CipherSuite);
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        length += 1; /* Compression */
    }
    length += extensions_len;

    rv = ssl3_AppendHandshakeHeader(ss, server_hello, length);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }

    if (IS_DTLS(ss) && ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        version = dtls_TLSVersionToDTLSVersion(ss->version);
    } else {
        version = tls13_EncodeDraftVersion(ss->version);
    }

    rv = ssl3_AppendHandshakeNumber(ss, version, 2);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    /* Random already generated in ssl3_HandleClientHello */
    rv = ssl3_AppendHandshake(
        ss, &ss->ssl3.hs.server_random, SSL3_RANDOM_LENGTH);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        if (sid) {
            rv = ssl3_AppendHandshakeVariable(
                ss, sid->u.ssl3.sessionID, sid->u.ssl3.sessionIDLength, 1);
        } else {
            rv = ssl3_AppendHandshakeNumber(ss, 0, 1);
        }
        if (rv != SECSuccess) {
            return rv; /* err set by AppendHandshake. */
        }
    }

    rv = ssl3_AppendHandshakeNumber(ss, ss->ssl3.hs.cipher_suite, 2);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = ssl3_AppendHandshakeNumber(ss, ss->ssl3.hs.compression, 1);
        if (rv != SECSuccess) {
            return rv; /* err set by AppendHandshake. */
        }
    }
    if (extensions_len) {
        PRInt32 sent_len;

        extensions_len -= 2;
        rv = ssl3_AppendHandshakeNumber(ss, extensions_len, 2);
        if (rv != SECSuccess)
            return rv; /* err set by ssl3_AppendHandshakeNumber */
        sent_len = ssl3_CallHelloExtensionSenders(ss, PR_TRUE, extensions_len,
                                                  &ss->xtnData.serverHelloSenders[0]);
        PORT_Assert(sent_len == extensions_len);
        if (sent_len != extensions_len) {
            if (sent_len >= 0)
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
    }

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = ssl3_SetupPendingCipherSpec(ss);
        if (rv != SECSuccess) {
            return rv; /* err set by ssl3_SetupPendingCipherSpec */
        }
    }

    return SECSuccess;
}

SECStatus
ssl_CreateDHEKeyPair(const sslNamedGroupDef *groupDef,
                     const ssl3DHParams *params,
                     sslEphemeralKeyPair **keyPair)
{
    SECKEYDHParams dhParam;
    SECKEYPublicKey *pubKey = NULL;   /* Ephemeral DH key */
    SECKEYPrivateKey *privKey = NULL; /* Ephemeral DH key */
    sslEphemeralKeyPair *pair;

    dhParam.prime.data = params->prime.data;
    dhParam.prime.len = params->prime.len;
    dhParam.base.data = params->base.data;
    dhParam.base.len = params->base.len;

    PRINT_BUF(60, (NULL, "Server DH p", dhParam.prime.data,
                   dhParam.prime.len));
    PRINT_BUF(60, (NULL, "Server DH g", dhParam.base.data,
                   dhParam.base.len));

    /* Generate ephemeral DH keypair */
    privKey = SECKEY_CreateDHPrivateKey(&dhParam, &pubKey, NULL);
    if (!privKey || !pubKey) {
        ssl_MapLowLevelError(SEC_ERROR_KEYGEN_FAIL);
        return SECFailure;
    }

    pair = ssl_NewEphemeralKeyPair(groupDef, privKey, pubKey);
    if (!pair) {
        SECKEY_DestroyPrivateKey(privKey);
        SECKEY_DestroyPublicKey(pubKey);

        return SECFailure;
    }

    *keyPair = pair;
    return SECSuccess;
}

static SECStatus
ssl3_SendDHServerKeyExchange(sslSocket *ss)
{
    const ssl3KEADef *kea_def = ss->ssl3.hs.kea_def;
    SECStatus rv = SECFailure;
    int length;
    SECItem signed_hash = { siBuffer, NULL, 0 };
    SSL3Hashes hashes;
    SSLHashType hashAlg;

    const ssl3DHParams *params;
    sslEphemeralKeyPair *keyPair;
    SECKEYPublicKey *pubKey;
    SECKEYPrivateKey *certPrivateKey;
    const sslNamedGroupDef *groupDef;

    if (kea_def->kea != kea_dhe_dss && kea_def->kea != kea_dhe_rsa) {
        /* TODO: Support DH_anon. It might be sufficient to drop the signature.
                 See bug 1170510. */
        PORT_SetError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        return SECFailure;
    }

    rv = ssl_SelectDHEGroup(ss, &groupDef);
    if (rv == SECFailure) {
        PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
        return SECFailure;
    }
    ss->sec.keaGroup = groupDef;

    params = ssl_GetDHEParams(groupDef);
    rv = ssl_CreateDHEKeyPair(groupDef, params, &keyPair);
    if (rv == SECFailure) {
        ssl_MapLowLevelError(SEC_ERROR_KEYGEN_FAIL);
        return SECFailure;
    }
    PR_APPEND_LINK(&keyPair->link, &ss->ephemeralKeyPairs);

    if (ss->ssl3.pwSpec->version == SSL_LIBRARY_VERSION_TLS_1_2) {
        hashAlg = ssl_SignatureSchemeToHashType(ss->ssl3.hs.signatureScheme);
    } else {
        /* Use ssl_hash_none to represent the MD5+SHA1 combo. */
        hashAlg = ssl_hash_none;
    }

    pubKey = keyPair->keys->pubKey;
    PRINT_BUF(50, (ss, "DH public value:",
                   pubKey->u.dh.publicValue.data,
                   pubKey->u.dh.publicValue.len));
    rv = ssl3_ComputeDHKeyHash(ss, hashAlg, &hashes,
                               pubKey->u.dh.prime,
                               pubKey->u.dh.base,
                               pubKey->u.dh.publicValue,
                               PR_TRUE /* padY */);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    certPrivateKey = ss->sec.serverCert->serverKeyPair->privKey;
    rv = ssl3_SignHashes(ss, &hashes, certPrivateKey, &signed_hash);
    if (rv != SECSuccess) {
        goto loser; /* ssl3_SignHashes has set err. */
    }

    length = 2 + pubKey->u.dh.prime.len +
             2 + pubKey->u.dh.base.len +
             2 + pubKey->u.dh.prime.len +
             2 + signed_hash.len;

    if (ss->ssl3.pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2) {
        length += 2;
    }

    rv = ssl3_AppendHandshakeHeader(ss, server_key_exchange, length);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    rv = ssl3_AppendHandshakeVariable(ss, pubKey->u.dh.prime.data,
                                      pubKey->u.dh.prime.len, 2);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    rv = ssl3_AppendHandshakeVariable(ss, pubKey->u.dh.base.data,
                                      pubKey->u.dh.base.len, 2);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    rv = ssl_AppendPaddedDHKeyShare(ss, pubKey, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    if (ss->ssl3.pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2) {
        rv = ssl3_AppendHandshakeNumber(ss, ss->ssl3.hs.signatureScheme, 2);
        if (rv != SECSuccess) {
            goto loser; /* err set by AppendHandshake. */
        }
    }

    rv = ssl3_AppendHandshakeVariable(ss, signed_hash.data,
                                      signed_hash.len, 2);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }
    PORT_Free(signed_hash.data);
    return SECSuccess;

loser:
    if (signed_hash.data)
        PORT_Free(signed_hash.data);
    return SECFailure;
}

static SECStatus
ssl3_SendServerKeyExchange(sslSocket *ss)
{
    const ssl3KEADef *kea_def = ss->ssl3.hs.kea_def;

    SSL_TRC(3, ("%d: SSL3[%d]: send server_key_exchange handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    switch (kea_def->exchKeyType) {
        case ssl_kea_dh: {
            return ssl3_SendDHServerKeyExchange(ss);
        }

        case ssl_kea_ecdh: {
            return ssl3_SendECDHServerKeyExchange(ss);
        }

        case ssl_kea_rsa:
        case ssl_kea_null:
        default:
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            break;
    }

    return SECFailure;
}

SECStatus
ssl3_EncodeSigAlgs(const sslSocket *ss, PRUint8 *buf, unsigned maxLen, PRUint32 *len)
{
    unsigned int i;
    PRUint8 *p = buf;

    PORT_Assert(maxLen >= ss->ssl3.signatureSchemeCount * 2);
    if (maxLen < ss->ssl3.signatureSchemeCount * 2) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    for (i = 0; i < ss->ssl3.signatureSchemeCount; ++i) {
        PRUint32 policy = 0;
        SSLHashType hashType = ssl_SignatureSchemeToHashType(
            ss->ssl3.signatureSchemes[i]);
        SECOidTag hashOID = ssl3_HashTypeToOID(hashType);

        /* Skip RSA-PSS schemes if there are no tokens to verify them. */
        if (ssl_IsRsaPssSignatureScheme(ss->ssl3.signatureSchemes[i]) &&
            !PK11_TokenExists(auth_alg_defs[ssl_auth_rsa_pss])) {
            continue;
        }

        if ((NSS_GetAlgorithmPolicy(hashOID, &policy) != SECSuccess) ||
            (policy & NSS_USE_ALG_IN_SSL_KX)) {
            p = ssl_EncodeUintX((PRUint32)ss->ssl3.signatureSchemes[i], 2, p);
        }
    }

    if (p == buf) {
        PORT_SetError(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
        return SECFailure;
    }
    *len = p - buf;
    return SECSuccess;
}

void
ssl3_GetCertificateRequestCAs(sslSocket *ss, int *calen, SECItem **names,
                              int *nnames)
{
    SECItem *name;
    CERTDistNames *ca_list;
    int i;

    *calen = 0;
    *names = NULL;
    *nnames = 0;

    /* ssl3.ca_list is initialized to NULL, and never changed. */
    ca_list = ss->ssl3.ca_list;
    if (!ca_list) {
        ca_list = ssl3_server_ca_list;
    }

    if (ca_list != NULL) {
        *names = ca_list->names;
        *nnames = ca_list->nnames;
    }

    for (i = 0, name = *names; i < *nnames; i++, name++) {
        *calen += 2 + name->len;
    }
}

static SECStatus
ssl3_SendCertificateRequest(sslSocket *ss)
{
    PRBool isTLS12;
    const PRUint8 *certTypes;
    SECStatus rv;
    int length;
    SECItem *names;
    int calen;
    int nnames;
    SECItem *name;
    int i;
    int certTypesLength;
    PRUint8 sigAlgs[MAX_SIGNATURE_SCHEMES * 2];
    unsigned int sigAlgsLength = 0;

    SSL_TRC(3, ("%d: SSL3[%d]: send certificate_request handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    isTLS12 = (PRBool)(ss->ssl3.pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2);

    ssl3_GetCertificateRequestCAs(ss, &calen, &names, &nnames);
    certTypes = certificate_types;
    certTypesLength = sizeof certificate_types;

    length = 1 + certTypesLength + 2 + calen;
    if (isTLS12) {
        rv = ssl3_EncodeSigAlgs(ss, sigAlgs, sizeof(sigAlgs), &sigAlgsLength);
        if (rv != SECSuccess) {
            return rv;
        }
        length += 2 + sigAlgsLength;
    }

    rv = ssl3_AppendHandshakeHeader(ss, certificate_request, length);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    rv = ssl3_AppendHandshakeVariable(ss, certTypes, certTypesLength, 1);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    if (isTLS12) {
        rv = ssl3_AppendHandshakeVariable(ss, sigAlgs, sigAlgsLength, 2);
        if (rv != SECSuccess) {
            return rv; /* err set by AppendHandshake. */
        }
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

    return SECSuccess;
}

static SECStatus
ssl3_SendServerHelloDone(sslSocket *ss)
{
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: send server_hello_done handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    rv = ssl3_AppendHandshakeHeader(ss, server_hello_done, 0);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    rv = ssl3_FlushHandshake(ss, 0);
    if (rv != SECSuccess) {
        return rv; /* error code set by ssl3_FlushHandshake */
    }
    return SECSuccess;
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered
 * a complete ssl3 Certificate Verify message
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleCertificateVerify(sslSocket *ss, SSL3Opaque *b, PRUint32 length,
                             SSL3Hashes *hashes)
{
    SECItem signed_hash = { siBuffer, NULL, 0 };
    SECStatus rv;
    int errCode = SSL_ERROR_RX_MALFORMED_CERT_VERIFY;
    SSL3AlertDescription desc = handshake_failure;
    PRBool isTLS;
    SSLSignatureScheme sigScheme;
    SSLHashType hashAlg;
    SSL3Hashes localHashes;
    SSL3Hashes *hashesForVerify = NULL;

    SSL_TRC(3, ("%d: SSL3[%d]: handle certificate_verify handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    /* TLS 1.3 is handled by tls13_HandleCertificateVerify */
    PORT_Assert(ss->ssl3.prSpec->version <= SSL_LIBRARY_VERSION_TLS_1_2);

    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);

    if (ss->ssl3.hs.ws != wait_cert_verify) {
        desc = unexpected_message;
        errCode = SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY;
        goto alert_loser;
    }

    if (!hashes) {
        PORT_Assert(0);
        desc = internal_error;
        errCode = SEC_ERROR_LIBRARY_FAILURE;
        goto alert_loser;
    }

    if (ss->ssl3.hs.hashType == handshake_hash_record) {
        rv = ssl_ConsumeSignatureScheme(ss, &b, &length, &sigScheme);
        if (rv != SECSuccess) {
            goto loser; /* malformed or unsupported. */
        }
        rv = ssl_CheckSignatureSchemeConsistency(ss, sigScheme,
                                                 ss->sec.peerCert);
        if (rv != SECSuccess) {
            errCode = PORT_GetError();
            desc = decrypt_error;
            goto alert_loser;
        }

        hashAlg = ssl_SignatureSchemeToHashType(sigScheme);

        if (hashes->u.pointer_to_hash_input.data) {
            rv = ssl3_ComputeHandshakeHash(hashes->u.pointer_to_hash_input.data,
                                           hashes->u.pointer_to_hash_input.len,
                                           hashAlg, &localHashes);
        } else {
            rv = SECFailure;
        }

        if (rv == SECSuccess) {
            hashesForVerify = &localHashes;
        } else {
            errCode = SSL_ERROR_DIGEST_FAILURE;
            desc = decrypt_error;
            goto alert_loser;
        }
    } else {
        hashesForVerify = hashes;
        sigScheme = ssl_sig_none;
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &signed_hash, 2, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }

    /* XXX verify that the key & kea match */
    rv = ssl3_VerifySignedHashes(ss, sigScheme, hashesForVerify, &signed_hash);
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        desc = isTLS ? decrypt_error : handshake_failure;
        goto alert_loser;
    }

    signed_hash.data = NULL;

    if (length != 0) {
        desc = isTLS ? decode_error : illegal_parameter;
        goto alert_loser; /* malformed */
    }
    ss->ssl3.hs.ws = wait_change_cipher;
    return SECSuccess;

alert_loser:
    SSL3_SendAlert(ss, alert_fatal, desc);
loser:
    PORT_SetError(errCode);
    return SECFailure;
}

/* find a slot that is able to generate a PMS and wrap it with RSA.
 * Then generate and return the PMS.
 * If the serverKeySlot parameter is non-null, this function will use
 * that slot to do the job, otherwise it will find a slot.
 *
 * Called from  ssl3_DeriveConnectionKeys()  (above)
 *      ssl3_SendRSAClientKeyExchange()     (above)
 *      ssl3_HandleRSAClientKeyExchange()  (below)
 * Caller must hold the SpecWriteLock, the SSL3HandshakeLock
 */
static PK11SymKey *
ssl3_GenerateRSAPMS(sslSocket *ss, ssl3CipherSpec *spec,
                    PK11SlotInfo *serverKeySlot)
{
    PK11SymKey *pms = NULL;
    PK11SlotInfo *slot = serverKeySlot;
    void *pwArg = ss->pkcs11PinArg;
    SECItem param;
    CK_VERSION version;
    CK_MECHANISM_TYPE mechanism_array[3];

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (slot == NULL) {
        SSLCipherAlgorithm calg;
        /* The specReadLock would suffice here, but we cannot assert on
        ** read locks.  Also, all the callers who call with a non-null
        ** slot already hold the SpecWriteLock.
        */
        PORT_Assert(ss->opt.noLocks || ssl_HaveSpecWriteLock(ss));
        PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

        calg = spec->cipher_def->calg;

        /* First get an appropriate slot.  */
        mechanism_array[0] = CKM_SSL3_PRE_MASTER_KEY_GEN;
        mechanism_array[1] = CKM_RSA_PKCS;
        mechanism_array[2] = ssl3_Alg2Mech(calg);

        slot = PK11_GetBestSlotMultiple(mechanism_array, 3, pwArg);
        if (slot == NULL) {
            /* can't find a slot with all three, find a slot with the minimum */
            slot = PK11_GetBestSlotMultiple(mechanism_array, 2, pwArg);
            if (slot == NULL) {
                PORT_SetError(SSL_ERROR_TOKEN_SLOT_NOT_FOUND);
                return pms; /* which is NULL */
            }
        }
    }

    /* Generate the pre-master secret ...  */
    if (IS_DTLS(ss)) {
        SSL3ProtocolVersion temp;

        temp = dtls_TLSVersionToDTLSVersion(ss->clientHelloVersion);
        version.major = MSB(temp);
        version.minor = LSB(temp);
    } else {
        version.major = MSB(ss->clientHelloVersion);
        version.minor = LSB(ss->clientHelloVersion);
    }

    param.data = (unsigned char *)&version;
    param.len = sizeof version;

    pms = PK11_KeyGen(slot, CKM_SSL3_PRE_MASTER_KEY_GEN, &param, 0, pwArg);
    if (!serverKeySlot)
        PK11_FreeSlot(slot);
    if (pms == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
    }
    return pms;
}

/* Note: The Bleichenbacher attack on PKCS#1 necessitates that we NEVER
 * return any indication of failure of the Client Key Exchange message,
 * where that failure is caused by the content of the client's message.
 * This function must not return SECFailure for any reason that is directly
 * or indirectly caused by the content of the client's encrypted PMS.
 * We must not send an alert and also not drop the connection.
 * Instead, we generate a random PMS.  This will cause a failure
 * in the processing the finished message, which is exactly where
 * the failure must occur.
 *
 * Called from ssl3_HandleClientKeyExchange
 */
static SECStatus
ssl3_HandleRSAClientKeyExchange(sslSocket *ss,
                                SSL3Opaque *b,
                                PRUint32 length,
                                sslKeyPair *serverKeyPair)
{
    SECStatus rv;
    SECItem enc_pms;
    PK11SymKey *tmpPms[2] = { NULL, NULL };
    PK11SlotInfo *slot;
    int useFauxPms = 0;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->ssl3.prSpec == ss->ssl3.pwSpec);

    enc_pms.data = b;
    enc_pms.len = length;

    if (ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0) { /* isTLS */
        PRInt32 kLen;
        kLen = ssl3_ConsumeHandshakeNumber(ss, 2, &enc_pms.data, &enc_pms.len);
        if (kLen < 0) {
            PORT_SetError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
            return SECFailure;
        }
        if ((unsigned)kLen < enc_pms.len) {
            enc_pms.len = kLen;
        }
    }

#define currentPms tmpPms[!useFauxPms]
#define unusedPms tmpPms[useFauxPms]
#define realPms tmpPms[1]
#define fauxPms tmpPms[0]

    /*
     * Get as close to algorithm 2 from RFC 5246; Section 7.4.7.1
     * as we can within the constraints of the PKCS#11 interface.
     *
     * 1. Unconditionally generate a bogus PMS (what RFC 5246
     *    calls R).
     * 2. Attempt the RSA decryption to recover the PMS (what
     *    RFC 5246 calls M).
     * 3. Set PMS = (M == NULL) ? R : M
     * 4. Use ssl3_ComputeMasterSecret(PMS) to attempt to derive
     *    the MS from PMS. This includes performing the version
     *    check and length check.
     * 5. If either the initial RSA decryption failed or
     *    ssl3_ComputeMasterSecret(PMS) failed, then discard
     *    M and set PMS = R. Else, discard R and set PMS = M.
     *
     * We do two derivations here because we can't rely on having
     * a function that only performs the PMS version and length
     * check. The only redundant cost is that this runs the PRF,
     * which isn't necessary here.
     */

    /* Generate the bogus PMS (R) */
    slot = PK11_GetSlotFromPrivateKey(serverKeyPair->privKey);
    if (!slot) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    if (!PK11_DoesMechanism(slot, CKM_SSL3_MASTER_KEY_DERIVE)) {
        PK11_FreeSlot(slot);
        slot = PK11_GetBestSlot(CKM_SSL3_MASTER_KEY_DERIVE, NULL);
        if (!slot) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
    }

    ssl_GetSpecWriteLock(ss);
    fauxPms = ssl3_GenerateRSAPMS(ss, ss->ssl3.prSpec, slot);
    ssl_ReleaseSpecWriteLock(ss);
    PK11_FreeSlot(slot);

    if (fauxPms == NULL) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        return SECFailure;
    }

    /*
     * unwrap pms out of the incoming buffer
     * Note: CKM_SSL3_MASTER_KEY_DERIVE is NOT the mechanism used to do
     *  the unwrap.  Rather, it is the mechanism with which the
     *      unwrapped pms will be used.
     */
    realPms = PK11_PubUnwrapSymKey(serverKeyPair->privKey, &enc_pms,
                                   CKM_SSL3_MASTER_KEY_DERIVE, CKA_DERIVE, 0);
    /* Temporarily use the PMS if unwrapping the real PMS fails. */
    useFauxPms |= (realPms == NULL);

    /* Attempt to derive the MS from the PMS. This is the only way to
     * check the version field in the RSA PMS. If this fails, we
     * then use the faux PMS in place of the PMS. Note that this
     * operation should never fail if we are using the faux PMS
     * since it is correctly formatted. */
    rv = ssl3_ComputeMasterSecret(ss, currentPms, NULL);

    /* If we succeeded, then select the true PMS and discard the
     * FPMS. Else, select the FPMS and select the true PMS */
    useFauxPms |= (rv != SECSuccess);

    if (unusedPms) {
        PK11_FreeSymKey(unusedPms);
    }

    /* This step will derive the MS from the PMS, among other things. */
    rv = ssl3_InitPendingCipherSpec(ss, currentPms);
    PK11_FreeSymKey(currentPms);

    if (rv != SECSuccess) {
        (void)SSL3_SendAlert(ss, alert_fatal, handshake_failure);
        return SECFailure; /* error code set by ssl3_InitPendingCipherSpec */
    }

#undef currentPms
#undef unusedPms
#undef realPms
#undef fauxPms

    return SECSuccess;
}

static SECStatus
ssl3_HandleDHClientKeyExchange(sslSocket *ss,
                               SSL3Opaque *b,
                               PRUint32 length,
                               sslKeyPair *serverKeyPair)
{
    PK11SymKey *pms;
    SECStatus rv;
    SECKEYPublicKey clntPubKey;
    CK_MECHANISM_TYPE target;
    PRBool isTLS;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    clntPubKey.keyType = dhKey;
    clntPubKey.u.dh.prime.len = serverKeyPair->pubKey->u.dh.prime.len;
    clntPubKey.u.dh.prime.data = serverKeyPair->pubKey->u.dh.prime.data;
    clntPubKey.u.dh.base.len = serverKeyPair->pubKey->u.dh.base.len;
    clntPubKey.u.dh.base.data = serverKeyPair->pubKey->u.dh.base.data;

    rv = ssl3_ConsumeHandshakeVariable(ss, &clntPubKey.u.dh.publicValue,
                                       2, &b, &length);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (!ssl_IsValidDHEShare(&serverKeyPair->pubKey->u.dh.prime,
                             &clntPubKey.u.dh.publicValue)) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_DHE_KEY_SHARE);
        return SECFailure;
    }

    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);

    if (isTLS)
        target = CKM_TLS_MASTER_KEY_DERIVE_DH;
    else
        target = CKM_SSL3_MASTER_KEY_DERIVE_DH;

    /* Determine the PMS */
    pms = PK11_PubDerive(serverKeyPair->privKey, &clntPubKey, PR_FALSE, NULL, NULL,
                         CKM_DH_PKCS_DERIVE, target, CKA_DERIVE, 0, NULL);
    if (pms == NULL) {
        ssl_FreeEphemeralKeyPairs(ss);
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        return SECFailure;
    }

    rv = ssl3_InitPendingCipherSpec(ss, pms);
    PK11_FreeSymKey(pms);
    ssl_FreeEphemeralKeyPairs(ss);
    return rv;
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered
 * a complete ssl3 ClientKeyExchange message from the remote client
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleClientKeyExchange(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    sslKeyPair *serverKeyPair = NULL;
    SECStatus rv;
    const ssl3KEADef *kea_def;

    SSL_TRC(3, ("%d: SSL3[%d]: handle client_key_exchange handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->ssl3.hs.ws != wait_client_key) {
        SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CLIENT_KEY_EXCH);
        return SECFailure;
    }

    kea_def = ss->ssl3.hs.kea_def;

    if (kea_def->ephemeral) {
        sslEphemeralKeyPair *keyPair;
        /* There should be exactly one pair. */
        PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->ephemeralKeyPairs));
        PORT_Assert(PR_PREV_LINK(&ss->ephemeralKeyPairs) ==
                    PR_NEXT_LINK(&ss->ephemeralKeyPairs));
        keyPair = (sslEphemeralKeyPair *)PR_NEXT_LINK(&ss->ephemeralKeyPairs);
        serverKeyPair = keyPair->keys;
        ss->sec.keaKeyBits =
            SECKEY_PublicKeyStrengthInBits(serverKeyPair->pubKey);
    } else {
        serverKeyPair = ss->sec.serverCert->serverKeyPair;
        ss->sec.keaKeyBits = ss->sec.serverCert->serverKeyBits;
    }

    if (!serverKeyPair) {
        SSL3_SendAlert(ss, alert_fatal, handshake_failure);
        PORT_SetError(SSL_ERROR_NO_SERVER_KEY_FOR_ALG);
        return SECFailure;
    }
    PORT_Assert(serverKeyPair->pubKey);
    PORT_Assert(serverKeyPair->privKey);

    ss->sec.keaType = kea_def->exchKeyType;

    switch (kea_def->exchKeyType) {
        case ssl_kea_rsa:
            rv = ssl3_HandleRSAClientKeyExchange(ss, b, length, serverKeyPair);
            break;

        case ssl_kea_dh:
            rv = ssl3_HandleDHClientKeyExchange(ss, b, length, serverKeyPair);
            break;

        case ssl_kea_ecdh:
            rv = ssl3_HandleECDHClientKeyExchange(ss, b, length, serverKeyPair);
            break;

        default:
            (void)ssl3_HandshakeFailure(ss);
            PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
            return SECFailure;
    }
    ssl_FreeEphemeralKeyPairs(ss);
    if (rv == SECSuccess) {
        ss->ssl3.hs.ws = ss->sec.peerCert ? wait_cert_verify : wait_change_cipher;
    } else {
        /* PORT_SetError has been called by all the Handle*ClientKeyExchange
         * functions above.  However, not all error paths result in an alert, so
         * this ensures that the server knows about the error.  Note that if an
         * alert was already sent, SSL3_SendAlert() is a noop. */
        PRErrorCode errCode = PORT_GetError();
        (void)SSL3_SendAlert(ss, alert_fatal, handshake_failure);
        PORT_SetError(errCode);
    }
    return rv;
}

/* This is TLS's equivalent of sending a no_certificate alert. */
SECStatus
ssl3_SendEmptyCertificate(sslSocket *ss)
{
    SECStatus rv;
    unsigned int len = 0;
    PRBool isTLS13 = PR_FALSE;
    const SECItem *context;

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_Assert(ss->ssl3.hs.certificateRequest);
        context = &ss->ssl3.hs.certificateRequest->context;
        len = context->len + 1;
        isTLS13 = PR_TRUE;
    }

    rv = ssl3_AppendHandshakeHeader(ss, certificate, len + 3);
    if (rv != SECSuccess) {
        return rv;
    }

    if (isTLS13) {
        rv = ssl3_AppendHandshakeVariable(ss, context->data, context->len, 1);
        if (rv != SECSuccess) {
            return rv;
        }
    }

    return ssl3_AppendHandshakeNumber(ss, 0, 3);
}

/*
 * NewSessionTicket
 * Called from ssl3_HandleFinished
 */
static SECStatus
ssl3_SendNewSessionTicket(sslSocket *ss)
{
    SECItem ticket = { 0, NULL, 0 };
    SECStatus rv;
    NewSessionTicket nticket = { 0 };

    rv = ssl3_EncodeSessionTicket(ss, &nticket, &ticket);
    if (rv != SECSuccess)
        goto loser;

    /* Serialize the handshake message. Length =
     * lifetime (4) + ticket length (2) + ticket. */
    rv = ssl3_AppendHandshakeHeader(ss, new_session_ticket,
                                    4 + 2 + ticket.len);
    if (rv != SECSuccess)
        goto loser;

    /* This is a fixed value. */
    rv = ssl3_AppendHandshakeNumber(ss, TLS_EX_SESS_TICKET_LIFETIME_HINT, 4);
    if (rv != SECSuccess)
        goto loser;

    /* Encode the ticket. */
    rv = ssl3_AppendHandshakeVariable(ss, ticket.data, ticket.len, 2);
    if (rv != SECSuccess)
        goto loser;

    rv = SECSuccess;

loser:
    if (ticket.data) {
        SECITEM_FreeItem(&ticket, PR_FALSE);
    }
    return rv;
}

static SECStatus
ssl3_HandleNewSessionTicket(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;
    SECItem ticketData;

    SSL_TRC(3, ("%d: SSL3[%d]: handle session_ticket handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    PORT_Assert(!ss->ssl3.hs.newSessionTicket.ticket.data);
    PORT_Assert(!ss->ssl3.hs.receivedNewSessionTicket);

    if (ss->ssl3.hs.ws != wait_new_session_ticket) {
        SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET);
        return SECFailure;
    }

    /* RFC5077 Section 3.3: "The client MUST NOT treat the ticket as valid
     * until it has verified the server's Finished message." See the comment in
     * ssl3_FinishHandshake for more details.
     */
    ss->ssl3.hs.newSessionTicket.received_timestamp = ssl_Time();
    if (length < 4) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET);
        return SECFailure;
    }
    ss->ssl3.hs.newSessionTicket.ticket_lifetime_hint =
        (PRUint32)ssl3_ConsumeHandshakeNumber(ss, 4, &b, &length);

    rv = ssl3_ConsumeHandshakeVariable(ss, &ticketData, 2, &b, &length);
    if (rv != SECSuccess || length != 0) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET);
        return SECFailure; /* malformed */
    }
    /* If the server sent a zero-length ticket, ignore it and keep the
     * existing ticket. */
    if (ticketData.len != 0) {
        rv = SECITEM_CopyItem(NULL, &ss->ssl3.hs.newSessionTicket.ticket,
                              &ticketData);
        if (rv != SECSuccess) {
            return rv;
        }
        ss->ssl3.hs.receivedNewSessionTicket = PR_TRUE;
    }

    ss->ssl3.hs.ws = wait_change_cipher;
    return SECSuccess;
}

#ifdef NISCC_TEST
static PRInt32 connNum = 0;

static SECStatus
get_fake_cert(SECItem *pCertItem, int *pIndex)
{
    PRFileDesc *cf;
    char *testdir;
    char *startat;
    char *stopat;
    const char *extension;
    int fileNum;
    PRInt32 numBytes = 0;
    PRStatus prStatus;
    PRFileInfo info;
    char cfn[100];

    pCertItem->data = 0;
    if ((testdir = PR_GetEnvSecure("NISCC_TEST")) == NULL) {
        return SECSuccess;
    }
    *pIndex = (NULL != strstr(testdir, "root"));
    extension = (strstr(testdir, "simple") ? "" : ".der");
    fileNum = PR_ATOMIC_INCREMENT(&connNum) - 1;
    if ((startat = PR_GetEnvSecure("START_AT")) != NULL) {
        fileNum += atoi(startat);
    }
    if ((stopat = PR_GetEnvSecure("STOP_AT")) != NULL &&
        fileNum >= atoi(stopat)) {
        *pIndex = -1;
        return SECSuccess;
    }
    sprintf(cfn, "%s/%08d%s", testdir, fileNum, extension);
    cf = PR_Open(cfn, PR_RDONLY, 0);
    if (!cf) {
        goto loser;
    }
    prStatus = PR_GetOpenFileInfo(cf, &info);
    if (prStatus != PR_SUCCESS) {
        PR_Close(cf);
        goto loser;
    }
    pCertItem = SECITEM_AllocItem(NULL, pCertItem, info.size);
    if (pCertItem) {
        numBytes = PR_Read(cf, pCertItem->data, info.size);
    }
    PR_Close(cf);
    if (numBytes != info.size) {
        SECITEM_FreeItem(pCertItem, PR_FALSE);
        PORT_SetError(SEC_ERROR_IO);
        goto loser;
    }
    fprintf(stderr, "using %s\n", cfn);
    return SECSuccess;

loser:
    fprintf(stderr, "failed to use %s\n", cfn);
    *pIndex = -1;
    return SECFailure;
}
#endif

/*
 * Used by both client and server.
 * Called from HandleServerHelloDone and from SendServerHelloSequence.
 */
static SECStatus
ssl3_SendCertificate(sslSocket *ss)
{
    SECStatus rv;
    CERTCertificateList *certChain;
    int certChainLen = 0;
    int i;
#ifdef NISCC_TEST
    SECItem fakeCert;
    int ndex = -1;
#endif
    PRBool isTLS13 = ss->version >= SSL_LIBRARY_VERSION_TLS_1_3;
    SECItem context = { siBuffer, NULL, 0 };
    unsigned int contextLen = 0;

    SSL_TRC(3, ("%d: SSL3[%d]: send certificate handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->sec.localCert)
        CERT_DestroyCertificate(ss->sec.localCert);
    if (ss->sec.isServer) {
        /* A server certificate is selected in ssl3_HandleClientHello. */
        PORT_Assert(ss->sec.serverCert);

        certChain = ss->sec.serverCert->serverCertChain;
        ss->sec.localCert = CERT_DupCertificate(ss->sec.serverCert->serverCert);
    } else {
        certChain = ss->ssl3.clientCertChain;
        ss->sec.localCert = CERT_DupCertificate(ss->ssl3.clientCertificate);
    }

#ifdef NISCC_TEST
    rv = get_fake_cert(&fakeCert, &ndex);
#endif

    if (isTLS13) {
        contextLen = 1; /* Size of the context length */
        if (!ss->sec.isServer) {
            PORT_Assert(ss->ssl3.hs.certificateRequest);
            context = ss->ssl3.hs.certificateRequest->context;
            contextLen += context.len;
        }
    }
    if (certChain) {
        for (i = 0; i < certChain->len; i++) {
#ifdef NISCC_TEST
            if (fakeCert.len > 0 && i == ndex) {
                certChainLen += fakeCert.len + 3;
            } else {
                certChainLen += certChain->certs[i].len + 3;
            }
#else
            certChainLen += certChain->certs[i].len + 3;
#endif
        }
    }

    rv = ssl3_AppendHandshakeHeader(ss, certificate,
                                    contextLen + certChainLen + 3);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }

    if (isTLS13) {
        rv = ssl3_AppendHandshakeVariable(ss, context.data,
                                          context.len, 1);
        if (rv != SECSuccess) {
            return rv; /* err set by AppendHandshake. */
        }
    }

    rv = ssl3_AppendHandshakeNumber(ss, certChainLen, 3);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    if (certChain) {
        for (i = 0; i < certChain->len; i++) {
#ifdef NISCC_TEST
            if (fakeCert.len > 0 && i == ndex) {
                rv = ssl3_AppendHandshakeVariable(ss, fakeCert.data,
                                                  fakeCert.len, 3);
                SECITEM_FreeItem(&fakeCert, PR_FALSE);
            } else {
                rv = ssl3_AppendHandshakeVariable(ss, certChain->certs[i].data,
                                                  certChain->certs[i].len, 3);
            }
#else
            rv = ssl3_AppendHandshakeVariable(ss, certChain->certs[i].data,
                                              certChain->certs[i].len, 3);
#endif
            if (rv != SECSuccess) {
                return rv; /* err set by AppendHandshake. */
            }
        }
    }

    return SECSuccess;
}

/*
 * Used by server only.
 * single-stapling, send only a single cert status
 */
SECStatus
ssl3_SendCertificateStatus(sslSocket *ss)
{
    SECStatus rv;
    int len = 0;
    SECItemArray *statusToSend = NULL;
    const sslServerCert *serverCert;

    SSL_TRC(3, ("%d: SSL3[%d]: send certificate status handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->sec.isServer);

    if (!ssl3_ExtensionNegotiated(ss, ssl_cert_status_xtn))
        return SECSuccess;

    /* Use certStatus based on the cert being used. */
    serverCert = ss->sec.serverCert;
    if (serverCert->certStatusArray && serverCert->certStatusArray->len) {
        statusToSend = serverCert->certStatusArray;
    }
    if (!statusToSend)
        return SECSuccess;

    /* Use the array's first item only (single stapling) */
    len = 1 + statusToSend->items[0].len + 3;

    rv = ssl3_AppendHandshakeHeader(ss, certificate_status, len);
    if (rv != SECSuccess) {
        return rv; /* err set by AppendHandshake. */
    }
    rv = ssl3_AppendHandshakeNumber(ss, 1 /*ocsp*/, 1);
    if (rv != SECSuccess)
        return rv; /* err set by AppendHandshake. */

    rv = ssl3_AppendHandshakeVariable(ss,
                                      statusToSend->items[0].data,
                                      statusToSend->items[0].len,
                                      3);
    if (rv != SECSuccess)
        return rv; /* err set by AppendHandshake. */

    return SECSuccess;
}

/* This is used to delete the CA certificates in the peer certificate chain
 * from the cert database after they've been validated.
 */
void
ssl3_CleanupPeerCerts(sslSocket *ss)
{
    PLArenaPool *arena = ss->ssl3.peerCertArena;
    ssl3CertNode *certs = (ssl3CertNode *)ss->ssl3.peerCertChain;

    for (; certs; certs = certs->next) {
        CERT_DestroyCertificate(certs->cert);
    }
    if (arena)
        PORT_FreeArena(arena, PR_FALSE);
    ss->ssl3.peerCertArena = NULL;
    ss->ssl3.peerCertChain = NULL;

    if (ss->sec.peerCert != NULL) {
        if (ss->sec.peerKey) {
            SECKEY_DestroyPublicKey(ss->sec.peerKey);
            ss->sec.peerKey = NULL;
        }
        CERT_DestroyCertificate(ss->sec.peerCert);
        ss->sec.peerCert = NULL;
    }
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered
 * a complete ssl3 CertificateStatus message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleCertificateStatus(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;

    if (ss->ssl3.hs.ws != wait_certificate_status) {
        (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CERT_STATUS);
        return SECFailure;
    }

    rv = ssl_ReadCertificateStatus(ss, b, length);
    if (rv != SECSuccess) {
        return SECFailure; /* code already set */
    }

    return ssl3_AuthCertificate(ss);
}

SECStatus
ssl_ReadCertificateStatus(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    PRInt32 status, len;

    PORT_Assert(!ss->sec.isServer);

    /* Consume the CertificateStatusType enum */
    status = ssl3_ConsumeHandshakeNumber(ss, 1, &b, &length);
    if (status != 1 /* ocsp */) {
        ssl3_DecodeError(ss); /* sets error code */
        return SECFailure;
    }

    len = ssl3_ConsumeHandshakeNumber(ss, 3, &b, &length);
    if (len != length) {
        ssl3_DecodeError(ss); /* sets error code */
        return SECFailure;
    }

#define MAX_CERTSTATUS_LEN 0x1ffff /* 128k - 1 */
    if (length > MAX_CERTSTATUS_LEN) {
        ssl3_DecodeError(ss); /* sets error code */
        return SECFailure;
    }
#undef MAX_CERTSTATUS_LEN

    /* Array size 1, because we currently implement single-stapling only */
    SECITEM_AllocArray(NULL, &ss->sec.ci.sid->peerCertStatus, 1);
    if (!ss->sec.ci.sid->peerCertStatus.items)
        return SECFailure; /* code already set */

    ss->sec.ci.sid->peerCertStatus.items[0].data = PORT_Alloc(length);

    if (!ss->sec.ci.sid->peerCertStatus.items[0].data) {
        SECITEM_FreeArray(&ss->sec.ci.sid->peerCertStatus, PR_FALSE);
        return SECFailure; /* code already set */
    }

    PORT_Memcpy(ss->sec.ci.sid->peerCertStatus.items[0].data, b, length);
    ss->sec.ci.sid->peerCertStatus.items[0].len = length;
    ss->sec.ci.sid->peerCertStatus.items[0].type = siBuffer;
    return SECSuccess;
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered
 * a complete ssl3 Certificate message.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleCertificate(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SSL_TRC(3, ("%d: SSL3[%d]: handle certificate handshake",
                SSL_GETPID(), ss->fd));
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if ((ss->sec.isServer && ss->ssl3.hs.ws != wait_client_cert) ||
        (!ss->sec.isServer && ss->ssl3.hs.ws != wait_server_cert)) {
        (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CERTIFICATE);
        return SECFailure;
    }

    return ssl3_CompleteHandleCertificate(ss, b, length);
}

/* Called from ssl3_HandleCertificate
 */
SECStatus
ssl3_CompleteHandleCertificate(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    ssl3CertNode *c;
    ssl3CertNode *lastCert = NULL;
    PRInt32 remaining = 0;
    PRInt32 size;
    SECStatus rv;
    PRBool isServer = ss->sec.isServer;
    PRBool isTLS;
    SSL3AlertDescription desc;
    int errCode = SSL_ERROR_RX_MALFORMED_CERTIFICATE;
    SECItem certItem;

    ssl3_CleanupPeerCerts(ss);
    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);

    /* It is reported that some TLS client sends a Certificate message
    ** with a zero-length message body.  We'll treat that case like a
    ** normal no_certificates message to maximize interoperability.
    */
    if (length) {
        remaining = ssl3_ConsumeHandshakeNumber(ss, 3, &b, &length);
        if (remaining < 0)
            goto loser; /* fatal alert already sent by ConsumeHandshake. */
        if ((PRUint32)remaining > length)
            goto decode_loser;
    }

    if (!remaining) {
        if (!(isTLS && isServer)) {
            desc = bad_certificate;
            goto alert_loser;
        }
        /* This is TLS's version of a no_certificate alert. */
        /* I'm a server. I've requested a client cert. He hasn't got one. */
        rv = ssl3_HandleNoCertificate(ss);
        if (rv != SECSuccess) {
            errCode = PORT_GetError();
            goto loser;
        }

        if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
            ss->ssl3.hs.ws = wait_client_key;
        } else {
            TLS13_SET_HS_STATE(ss, wait_finished);
        }
        return SECSuccess;
    }

    ss->ssl3.peerCertArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (ss->ssl3.peerCertArena == NULL) {
        goto loser; /* don't send alerts on memory errors */
    }

    /* First get the peer cert. */
    remaining -= 3;
    if (remaining < 0)
        goto decode_loser;

    size = ssl3_ConsumeHandshakeNumber(ss, 3, &b, &length);
    if (size <= 0)
        goto loser; /* fatal alert already sent by ConsumeHandshake. */

    if (remaining < size)
        goto decode_loser;

    certItem.data = b;
    certItem.len = size;
    b += size;
    length -= size;
    remaining -= size;

    ss->sec.peerCert = CERT_NewTempCertificate(ss->dbHandle, &certItem, NULL,
                                               PR_FALSE, PR_TRUE);
    if (ss->sec.peerCert == NULL) {
        /* We should report an alert if the cert was bad, but not if the
         * problem was just some local problem, like memory error.
         */
        goto ambiguous_err;
    }

    /* Now get all of the CA certs. */
    while (remaining > 0) {
        remaining -= 3;
        if (remaining < 0)
            goto decode_loser;

        size = ssl3_ConsumeHandshakeNumber(ss, 3, &b, &length);
        if (size <= 0)
            goto loser; /* fatal alert already sent by ConsumeHandshake. */

        if (remaining < size)
            goto decode_loser;

        certItem.data = b;
        certItem.len = size;
        b += size;
        length -= size;
        remaining -= size;

        c = PORT_ArenaNew(ss->ssl3.peerCertArena, ssl3CertNode);
        if (c == NULL) {
            goto loser; /* don't send alerts on memory errors */
        }

        c->cert = CERT_NewTempCertificate(ss->dbHandle, &certItem, NULL,
                                          PR_FALSE, PR_TRUE);
        if (c->cert == NULL) {
            goto ambiguous_err;
        }

        c->next = NULL;
        if (lastCert) {
            lastCert->next = c;
        } else {
            ss->ssl3.peerCertChain = c;
        }
        lastCert = c;
    }

    if (remaining != 0)
        goto decode_loser;

    SECKEY_UpdateCertPQG(ss->sec.peerCert);

    if (!isServer &&
        ss->version < SSL_LIBRARY_VERSION_TLS_1_3 &&
        ssl3_ExtensionNegotiated(ss, ssl_cert_status_xtn)) {
        ss->ssl3.hs.ws = wait_certificate_status;
        rv = SECSuccess;
    } else {
        rv = ssl3_AuthCertificate(ss); /* sets ss->ssl3.hs.ws */
    }

    return rv;

ambiguous_err:
    errCode = PORT_GetError();
    switch (errCode) {
        case PR_OUT_OF_MEMORY_ERROR:
        case SEC_ERROR_BAD_DATABASE:
        case SEC_ERROR_NO_MEMORY:
            if (isTLS) {
                desc = internal_error;
                goto alert_loser;
            }
            goto loser;
    }
    ssl3_SendAlertForCertError(ss, errCode);
    goto loser;

decode_loser:
    desc = isTLS ? decode_error : bad_certificate;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);

loser:
    (void)ssl_MapLowLevelError(errCode);
    return SECFailure;
}

SECStatus
ssl3_AuthCertificate(sslSocket *ss)
{
    SECStatus rv;
    PRBool isServer = ss->sec.isServer;
    int errCode;

    ss->ssl3.hs.authCertificatePending = PR_FALSE;

    PORT_Assert((ss->ssl3.hs.preliminaryInfo & ssl_preinfo_all) ==
                ssl_preinfo_all);
    /*
     * Ask caller-supplied callback function to validate cert chain.
     */
    rv = (SECStatus)(*ss->authCertificate)(ss->authCertificateArg, ss->fd,
                                           PR_TRUE, isServer);
    if (rv != SECSuccess) {
        errCode = PORT_GetError();
        if (rv != SECWouldBlock) {
            if (ss->handleBadCert) {
                rv = (*ss->handleBadCert)(ss->badCertArg, ss->fd);
            }
        }

        if (rv == SECWouldBlock) {
            if (ss->sec.isServer) {
                errCode = SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SERVERS;
                goto loser;
            }

            ss->ssl3.hs.authCertificatePending = PR_TRUE;
            rv = SECSuccess;
        }

        if (rv != SECSuccess) {
            ssl3_SendAlertForCertError(ss, errCode);
            goto loser;
        }
    }

    ss->sec.ci.sid->peerCert = CERT_DupCertificate(ss->sec.peerCert);

    if (!ss->sec.isServer) {
        CERTCertificate *cert = ss->sec.peerCert;

        /* set the server authentication type and size from the value
        ** in the cert. */
        SECKEYPublicKey *pubKey = CERT_ExtractPublicKey(cert);
        if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
            /* These are filled in in tls13_HandleCertificateVerify and
             * tls13_HandleServerKeyShare. */
            ss->sec.authType = ss->ssl3.hs.kea_def->authKeyType;
            ss->sec.keaType = ss->ssl3.hs.kea_def->exchKeyType;
        }
        if (pubKey) {
            KeyType pubKeyType;
            PRInt32 minKey;
            /* This partly fixes Bug 124230 and may cause problems for
             * callers which depend on the old (wrong) behavior. */
            ss->sec.authKeyBits = SECKEY_PublicKeyStrengthInBits(pubKey);
            pubKeyType = SECKEY_GetPublicKeyType(pubKey);
            minKey = ss->sec.authKeyBits;
            switch (pubKeyType) {
                case rsaKey:
                case rsaPssKey:
                case rsaOaepKey:
                    rv =
                        NSS_OptionGet(NSS_RSA_MIN_KEY_SIZE, &minKey);
                    if (rv !=
                        SECSuccess) {
                        minKey =
                            SSL_RSA_MIN_MODULUS_BITS;
                    }
                    break;
                case dsaKey:
                    rv =
                        NSS_OptionGet(NSS_DSA_MIN_KEY_SIZE, &minKey);
                    if (rv !=
                        SECSuccess) {
                        minKey =
                            SSL_DSA_MIN_P_BITS;
                    }
                    break;
                case dhKey:
                    rv =
                        NSS_OptionGet(NSS_DH_MIN_KEY_SIZE, &minKey);
                    if (rv !=
                        SECSuccess) {
                        minKey =
                            SSL_DH_MIN_P_BITS;
                    }
                    break;
                default:
                    break;
            }

            /* Too small: not good enough. Send a fatal alert. */
            /* We aren't checking EC here on the understanding that we only
             * support curves we like, a decision that might need revisiting. */
            if (ss->sec.authKeyBits < minKey) {
                PORT_SetError(SSL_ERROR_WEAK_SERVER_CERT_KEY);
                (void)SSL3_SendAlert(ss, alert_fatal,
                                     ss->version >= SSL_LIBRARY_VERSION_TLS_1_0
                                         ? insufficient_security
                                         : illegal_parameter);
                SECKEY_DestroyPublicKey(pubKey);
                return SECFailure;
            }
            SECKEY_DestroyPublicKey(pubKey);
            pubKey = NULL;
        }

        if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
            TLS13_SET_HS_STATE(ss, wait_cert_verify);
        } else {
            /* Ephemeral suites require ServerKeyExchange. */
            if (ss->ssl3.hs.kea_def->ephemeral) {
                /* require server_key_exchange */
                ss->ssl3.hs.ws = wait_server_key;
            } else {
                /* disallow server_key_exchange */
                ss->ssl3.hs.ws = wait_cert_request;
                /* This is static RSA key exchange so set the key exchange
                 * details to compensate for that. */
                ss->sec.keaKeyBits = ss->sec.authKeyBits;
                ss->sec.signatureScheme = ssl_sig_none;
                ss->sec.keaGroup = NULL;
            }
        }
    } else {
        /* Server */
        if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
            ss->ssl3.hs.ws = wait_client_key;
        } else {
            TLS13_SET_HS_STATE(ss, wait_cert_verify);
        }
    }

    PORT_Assert(rv == SECSuccess);
    if (rv != SECSuccess) {
        errCode = SEC_ERROR_LIBRARY_FAILURE;
        goto loser;
    }

    return SECSuccess;

loser:
    (void)ssl_MapLowLevelError(errCode);
    return SECFailure;
}

static SECStatus ssl3_FinishHandshake(sslSocket *ss);

static SECStatus
ssl3_AlwaysFail(sslSocket *ss)
{
    PORT_SetError(PR_INVALID_STATE_ERROR);
    return SECFailure;
}

/* Caller must hold 1stHandshakeLock.
*/
SECStatus
ssl3_AuthCertificateComplete(sslSocket *ss, PRErrorCode error)
{
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_Have1stHandshakeLock(ss));

    if (ss->sec.isServer) {
        PORT_SetError(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SERVERS);
        return SECFailure;
    }

    ssl_GetRecvBufLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    if (!ss->ssl3.hs.authCertificatePending) {
        PORT_SetError(PR_INVALID_STATE_ERROR);
        rv = SECFailure;
        goto done;
    }

    ss->ssl3.hs.authCertificatePending = PR_FALSE;

    if (error != 0) {
        ss->ssl3.hs.restartTarget = ssl3_AlwaysFail;
        ssl3_SendAlertForCertError(ss, error);
        rv = SECSuccess;
    } else if (ss->ssl3.hs.restartTarget != NULL) {
        sslRestartTarget target = ss->ssl3.hs.restartTarget;
        ss->ssl3.hs.restartTarget = NULL;

        if (target == ssl3_FinishHandshake) {
            SSL_TRC(3, ("%d: SSL3[%p]: certificate authentication lost the race"
                        " with peer's finished message",
                        SSL_GETPID(), ss->fd));
        }

        rv = target(ss);
        /* Even if we blocked here, we have accomplished enough to claim
         * success. Any remaining work will be taken care of by subsequent
         * calls to SSL_ForceHandshake/PR_Send/PR_Read/etc.
         */
        if (rv == SECWouldBlock) {
            rv = SECSuccess;
        }
    } else {
        SSL_TRC(3, ("%d: SSL3[%p]: certificate authentication won the race with"
                    " peer's finished message",
                    SSL_GETPID(), ss->fd));

        PORT_Assert(!ss->ssl3.hs.isResuming);
        PORT_Assert(ss->ssl3.hs.ws != idle_handshake);

        if (ss->opt.enableFalseStart &&
            !ss->firstHsDone &&
            !ss->ssl3.hs.isResuming &&
            ssl3_WaitingForServerSecondRound(ss)) {
            /* ssl3_SendClientSecondRound deferred the false start check because
             * certificate authentication was pending, so we do it now if we still
             * haven't received all of the server's second round yet.
             */
            rv = ssl3_CheckFalseStart(ss);
        } else {
            rv = SECSuccess;
        }
    }

done:
    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_ReleaseRecvBufLock(ss);

    return rv;
}

static SECStatus
ssl3_ComputeTLSFinished(sslSocket *ss, ssl3CipherSpec *spec,
                        PRBool isServer,
                        const SSL3Hashes *hashes,
                        TLSFinished *tlsFinished)
{
    SECStatus rv;
    CK_TLS_MAC_PARAMS tls_mac_params;
    SECItem param = { siBuffer, NULL, 0 };
    PK11Context *prf_context;
    unsigned int retLen;

    PORT_Assert(spec->master_secret);
    if (!spec->master_secret) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    if (spec->version < SSL_LIBRARY_VERSION_TLS_1_2) {
        tls_mac_params.prfMechanism = CKM_TLS_PRF;
    } else {
        tls_mac_params.prfMechanism = ssl3_GetPrfHashMechanism(ss);
    }
    tls_mac_params.ulMacLength = 12;
    tls_mac_params.ulServerOrClient = isServer ? 1 : 2;
    param.data = (unsigned char *)&tls_mac_params;
    param.len = sizeof(tls_mac_params);
    prf_context = PK11_CreateContextBySymKey(CKM_TLS_MAC, CKA_SIGN,
                                             spec->master_secret, &param);
    if (!prf_context)
        return SECFailure;

    rv = PK11_DigestBegin(prf_context);
    rv |= PK11_DigestOp(prf_context, hashes->u.raw, hashes->len);
    rv |= PK11_DigestFinal(prf_context, tlsFinished->verify_data, &retLen,
                           sizeof tlsFinished->verify_data);
    PORT_Assert(rv != SECSuccess || retLen == sizeof tlsFinished->verify_data);

    PK11_DestroyContext(prf_context, PR_TRUE);

    return rv;
}

/* The calling function must acquire and release the appropriate
 * lock (e.g., ssl_GetSpecReadLock / ssl_ReleaseSpecReadLock for
 * ss->ssl3.crSpec).
 */
SECStatus
ssl3_TLSPRFWithMasterSecret(sslSocket *ss, ssl3CipherSpec *spec,
                            const char *label, unsigned int labelLen,
                            const unsigned char *val, unsigned int valLen,
                            unsigned char *out, unsigned int outLen)
{
    SECStatus rv = SECSuccess;

    if (spec->master_secret) {
        SECItem param = { siBuffer, NULL, 0 };
        CK_MECHANISM_TYPE mech = CKM_TLS_PRF_GENERAL;
        PK11Context *prf_context;
        unsigned int retLen;

        if (spec->version >= SSL_LIBRARY_VERSION_TLS_1_2) {
            /* Bug 1312976 non-SHA256 exporters are broken. */
            if (ssl3_GetPrfHashMechanism(ss) != CKM_SHA256) {
                PORT_Assert(0);
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                return SECFailure;
            }
            mech = CKM_NSS_TLS_PRF_GENERAL_SHA256;
        }
        prf_context = PK11_CreateContextBySymKey(mech, CKA_SIGN,
                                                 spec->master_secret, &param);
        if (!prf_context)
            return SECFailure;

        rv = PK11_DigestBegin(prf_context);
        rv |= PK11_DigestOp(prf_context, (unsigned char *)label, labelLen);
        rv |= PK11_DigestOp(prf_context, val, valLen);
        rv |= PK11_DigestFinal(prf_context, out, &retLen, outLen);
        PORT_Assert(rv != SECSuccess || retLen == outLen);

        PK11_DestroyContext(prf_context, PR_TRUE);
    } else {
        PORT_Assert(spec->master_secret);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        rv = SECFailure;
    }
    return rv;
}

/* called from ssl3_SendClientSecondRound
 *             ssl3_HandleFinished
 */
static SECStatus
ssl3_SendNextProto(sslSocket *ss)
{
    SECStatus rv;
    int padding_len;
    static const unsigned char padding[32] = { 0 };

    if (ss->xtnData.nextProto.len == 0 ||
        ss->xtnData.nextProtoState == SSL_NEXT_PROTO_SELECTED) {
        return SECSuccess;
    }

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    padding_len = 32 - ((ss->xtnData.nextProto.len + 2) % 32);

    rv = ssl3_AppendHandshakeHeader(ss, next_proto, ss->xtnData.nextProto.len +
                                                        2 +
                                                        padding_len);
    if (rv != SECSuccess) {
        return rv; /* error code set by AppendHandshakeHeader */
    }
    rv = ssl3_AppendHandshakeVariable(ss, ss->xtnData.nextProto.data,
                                      ss->xtnData.nextProto.len, 1);
    if (rv != SECSuccess) {
        return rv; /* error code set by AppendHandshake */
    }
    rv = ssl3_AppendHandshakeVariable(ss, padding, padding_len, 1);
    if (rv != SECSuccess) {
        return rv; /* error code set by AppendHandshake */
    }
    return rv;
}

/* called from ssl3_SendFinished
 *
 * This function is simply a debugging aid and therefore does not return a
 * SECStatus. */
static void
ssl3_RecordKeyLog(sslSocket *ss)
{
#ifdef NSS_ALLOW_SSLKEYLOGFILE
    SECStatus rv;
    SECItem *keyData;
    char buf[14 /* "CLIENT_RANDOM " */ +
             SSL3_RANDOM_LENGTH * 2 /* client_random */ +
             1 /* " " */ +
             48 * 2 /* master secret */ +
             1 /* new line */];
    unsigned int j;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (!ssl_keylog_iob)
        return;

    rv = PK11_ExtractKeyValue(ss->ssl3.cwSpec->master_secret);
    if (rv != SECSuccess)
        return;

    ssl_GetSpecReadLock(ss);

    /* keyData does not need to be freed. */
    keyData = PK11_GetKeyData(ss->ssl3.cwSpec->master_secret);
    if (!keyData || !keyData->data || keyData->len != 48) {
        ssl_ReleaseSpecReadLock(ss);
        return;
    }

    /* https://developer.mozilla.org/en/NSS_Key_Log_Format */

    /* There could be multiple, concurrent writers to the
     * keylog, so we have to do everything in a single call to
     * fwrite. */

    memcpy(buf, "CLIENT_RANDOM ", 14);
    j = 14;
    hexEncode(buf + j, ss->ssl3.hs.client_random.rand, SSL3_RANDOM_LENGTH);
    j += SSL3_RANDOM_LENGTH * 2;
    buf[j++] = ' ';
    hexEncode(buf + j, keyData->data, 48);
    j += 48 * 2;
    buf[j++] = '\n';

    PORT_Assert(j == sizeof(buf));

    ssl_ReleaseSpecReadLock(ss);

    if (fwrite(buf, sizeof(buf), 1, ssl_keylog_iob) != 1)
        return;
    fflush(ssl_keylog_iob);
    return;
#endif
}

/* called from ssl3_SendClientSecondRound
 *             ssl3_HandleClientHello
 *             ssl3_HandleFinished
 */
static SECStatus
ssl3_SendFinished(sslSocket *ss, PRInt32 flags)
{
    ssl3CipherSpec *cwSpec;
    PRBool isTLS;
    PRBool isServer = ss->sec.isServer;
    SECStatus rv;
    SSL3Sender sender = isServer ? sender_server : sender_client;
    SSL3Hashes hashes;
    TLSFinished tlsFinished;

    SSL_TRC(3, ("%d: SSL3[%d]: send finished handshake", SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    ssl_GetSpecReadLock(ss);
    cwSpec = ss->ssl3.cwSpec;
    isTLS = (PRBool)(cwSpec->version > SSL_LIBRARY_VERSION_3_0);
    rv = ssl3_ComputeHandshakeHashes(ss, cwSpec, &hashes, sender);
    if (isTLS && rv == SECSuccess) {
        rv = ssl3_ComputeTLSFinished(ss, cwSpec, isServer, &hashes, &tlsFinished);
    }
    ssl_ReleaseSpecReadLock(ss);
    if (rv != SECSuccess) {
        goto fail; /* err code was set by ssl3_ComputeHandshakeHashes */
    }

    if (isTLS) {
        if (isServer)
            ss->ssl3.hs.finishedMsgs.tFinished[1] = tlsFinished;
        else
            ss->ssl3.hs.finishedMsgs.tFinished[0] = tlsFinished;
        ss->ssl3.hs.finishedBytes = sizeof tlsFinished;
        rv = ssl3_AppendHandshakeHeader(ss, finished, sizeof tlsFinished);
        if (rv != SECSuccess)
            goto fail; /* err set by AppendHandshake. */
        rv = ssl3_AppendHandshake(ss, &tlsFinished, sizeof tlsFinished);
        if (rv != SECSuccess)
            goto fail; /* err set by AppendHandshake. */
    } else {
        if (isServer)
            ss->ssl3.hs.finishedMsgs.sFinished[1] = hashes.u.s;
        else
            ss->ssl3.hs.finishedMsgs.sFinished[0] = hashes.u.s;
        PORT_Assert(hashes.len == sizeof hashes.u.s);
        ss->ssl3.hs.finishedBytes = sizeof hashes.u.s;
        rv = ssl3_AppendHandshakeHeader(ss, finished, sizeof hashes.u.s);
        if (rv != SECSuccess)
            goto fail; /* err set by AppendHandshake. */
        rv = ssl3_AppendHandshake(ss, &hashes.u.s, sizeof hashes.u.s);
        if (rv != SECSuccess)
            goto fail; /* err set by AppendHandshake. */
    }
    rv = ssl3_FlushHandshake(ss, flags);
    if (rv != SECSuccess) {
        goto fail; /* error code set by ssl3_FlushHandshake */
    }

    ssl3_RecordKeyLog(ss);

    return SECSuccess;

fail:
    return rv;
}

/* wrap the master secret, and put it into the SID.
 * Caller holds the Spec read lock.
 */
SECStatus
ssl3_CacheWrappedMasterSecret(sslSocket *ss, sslSessionID *sid,
                              ssl3CipherSpec *spec, SSLAuthType authType)
{
    PK11SymKey *wrappingKey = NULL;
    PK11SlotInfo *symKeySlot;
    void *pwArg = ss->pkcs11PinArg;
    SECStatus rv = SECFailure;
    PRBool isServer = ss->sec.isServer;
    CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM;

    symKeySlot = PK11_GetSlotFromKey(spec->master_secret);
    if (!isServer) {
        int wrapKeyIndex;
        int incarnation;

        /* these next few functions are mere accessors and don't fail. */
        sid->u.ssl3.masterWrapIndex = wrapKeyIndex =
            PK11_GetCurrentWrapIndex(symKeySlot);
        PORT_Assert(wrapKeyIndex == 0); /* array has only one entry! */

        sid->u.ssl3.masterWrapSeries = incarnation =
            PK11_GetSlotSeries(symKeySlot);
        sid->u.ssl3.masterSlotID = PK11_GetSlotID(symKeySlot);
        sid->u.ssl3.masterModuleID = PK11_GetModuleID(symKeySlot);
        sid->u.ssl3.masterValid = PR_TRUE;
        /* Get the default wrapping key, for wrapping the master secret before
         * placing it in the SID cache entry. */
        wrappingKey = PK11_GetWrapKey(symKeySlot, wrapKeyIndex,
                                      CKM_INVALID_MECHANISM, incarnation,
                                      pwArg);
        if (wrappingKey) {
            mechanism = PK11_GetMechanism(wrappingKey); /* can't fail. */
        } else {
            int keyLength;
            /* if the wrappingKey doesn't exist, attempt to create it.
             * Note: we intentionally ignore errors here.  If we cannot
             * generate a wrapping key, it is not fatal to this SSL connection,
             * but we will not be able to restart this session.
             */
            mechanism = PK11_GetBestWrapMechanism(symKeySlot);
            keyLength = PK11_GetBestKeyLength(symKeySlot, mechanism);
            /* Zero length means fixed key length algorithm, or error.
             * It's ambiguous.
             */
            wrappingKey = PK11_KeyGen(symKeySlot, mechanism, NULL,
                                      keyLength, pwArg);
            if (wrappingKey) {
                PK11_SetWrapKey(symKeySlot, wrapKeyIndex, wrappingKey);
            }
        }
    } else {
        /* server socket using session cache. */
        mechanism = PK11_GetBestWrapMechanism(symKeySlot);
        if (mechanism != CKM_INVALID_MECHANISM) {
            wrappingKey =
                ssl3_GetWrappingKey(ss, symKeySlot, ss->sec.serverCert,
                                    mechanism, pwArg);
            if (wrappingKey) {
                mechanism = PK11_GetMechanism(wrappingKey); /* can't fail. */
            }
        }
    }

    sid->u.ssl3.masterWrapMech = mechanism;
    PK11_FreeSlot(symKeySlot);

    if (wrappingKey) {
        SECItem wmsItem;

        wmsItem.data = sid->u.ssl3.keys.wrapped_master_secret;
        wmsItem.len = sizeof sid->u.ssl3.keys.wrapped_master_secret;
        rv = PK11_WrapSymKey(mechanism, NULL, wrappingKey,
                             spec->master_secret, &wmsItem);
        /* rv is examined below. */
        sid->u.ssl3.keys.wrapped_master_secret_len = wmsItem.len;
        PK11_FreeSymKey(wrappingKey);
    }
    return rv;
}

/* Called from ssl3_HandlePostHelloHandshakeMessage() when it has deciphered
 * a complete ssl3 Finished message from the peer.
 * Caller must hold Handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleFinished(sslSocket *ss, SSL3Opaque *b, PRUint32 length,
                    const SSL3Hashes *hashes)
{
    sslSessionID *sid = ss->sec.ci.sid;
    SECStatus rv = SECSuccess;
    PRBool isServer = ss->sec.isServer;
    PRBool isTLS;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    SSL_TRC(3, ("%d: SSL3[%d]: handle finished handshake",
                SSL_GETPID(), ss->fd));

    if (ss->ssl3.hs.ws != wait_finished) {
        SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_FINISHED);
        return SECFailure;
    }

    if (!hashes) {
        PORT_Assert(0);
        SSL3_SendAlert(ss, alert_fatal, internal_error);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    isTLS = (PRBool)(ss->ssl3.crSpec->version > SSL_LIBRARY_VERSION_3_0);
    if (isTLS) {
        TLSFinished tlsFinished;

        if (length != sizeof(tlsFinished)) {
#ifndef UNSAFE_FUZZER_MODE
            (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
            PORT_SetError(SSL_ERROR_RX_MALFORMED_FINISHED);
            return SECFailure;
#endif
        }
        rv = ssl3_ComputeTLSFinished(ss, ss->ssl3.crSpec, !isServer,
                                     hashes, &tlsFinished);
        if (!isServer)
            ss->ssl3.hs.finishedMsgs.tFinished[1] = tlsFinished;
        else
            ss->ssl3.hs.finishedMsgs.tFinished[0] = tlsFinished;
        ss->ssl3.hs.finishedBytes = sizeof(tlsFinished);
        if (rv != SECSuccess ||
            0 != NSS_SecureMemcmp(&tlsFinished, b,
                                  PR_MIN(length, ss->ssl3.hs.finishedBytes))) {
#ifndef UNSAFE_FUZZER_MODE
            (void)SSL3_SendAlert(ss, alert_fatal, decrypt_error);
            PORT_SetError(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
            return SECFailure;
#endif
        }
    } else {
        if (length != sizeof(SSL3Finished)) {
            (void)ssl3_IllegalParameter(ss);
            PORT_SetError(SSL_ERROR_RX_MALFORMED_FINISHED);
            return SECFailure;
        }

        if (!isServer)
            ss->ssl3.hs.finishedMsgs.sFinished[1] = hashes->u.s;
        else
            ss->ssl3.hs.finishedMsgs.sFinished[0] = hashes->u.s;
        PORT_Assert(hashes->len == sizeof hashes->u.s);
        ss->ssl3.hs.finishedBytes = sizeof hashes->u.s;
        if (0 != NSS_SecureMemcmp(&hashes->u.s, b, length)) {
            (void)ssl3_HandshakeFailure(ss);
            PORT_SetError(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
            return SECFailure;
        }
    }

    ssl_GetXmitBufLock(ss); /*************************************/

    if ((isServer && !ss->ssl3.hs.isResuming) ||
        (!isServer && ss->ssl3.hs.isResuming)) {
        PRInt32 flags = 0;

        /* Send a NewSessionTicket message if the client sent us
         * either an empty session ticket, or one that did not verify.
         * (Note that if either of these conditions was met, then the
         * server has sent a SessionTicket extension in the
         * ServerHello message.)
         */
        if (isServer && !ss->ssl3.hs.isResuming &&
            ssl3_ExtensionNegotiated(ss, ssl_session_ticket_xtn) &&
            ssl3_KEASupportsTickets(ss->ssl3.hs.kea_def)) {
            /* RFC 5077 Section 3.3: "In the case of a full handshake, the
             * server MUST verify the client's Finished message before sending
             * the ticket." Presumably, this also means that the client's
             * certificate, if any, must be verified beforehand too.
             */
            rv = ssl3_SendNewSessionTicket(ss);
            if (rv != SECSuccess) {
                goto xmit_loser;
            }
        }

        rv = ssl3_SendChangeCipherSpecs(ss);
        if (rv != SECSuccess) {
            goto xmit_loser; /* err is set. */
        }
        /* If this thread is in SSL_SecureSend (trying to write some data)
        ** then set the ssl_SEND_FLAG_FORCE_INTO_BUFFER flag, so that the
        ** last two handshake messages (change cipher spec and finished)
        ** will be sent in the same send/write call as the application data.
        */
        if (ss->writerThread == PR_GetCurrentThread()) {
            flags = ssl_SEND_FLAG_FORCE_INTO_BUFFER;
        }

        if (!isServer && !ss->firstHsDone) {
            rv = ssl3_SendNextProto(ss);
            if (rv != SECSuccess) {
                goto xmit_loser; /* err code was set. */
            }
        }

        if (IS_DTLS(ss)) {
            flags |= ssl_SEND_FLAG_NO_RETRANSMIT;
        }

        rv = ssl3_SendFinished(ss, flags);
        if (rv != SECSuccess) {
            goto xmit_loser; /* err is set. */
        }
    }

xmit_loser:
    ssl_ReleaseXmitBufLock(ss); /*************************************/
    if (rv != SECSuccess) {
        return rv;
    }

    if (sid->cached == never_cached && !ss->opt.noCache) {
        rv = ssl3_FillInCachedSID(ss, sid);

        /* If the wrap failed, we don't cache the sid.
         * The connection continues normally however.
         */
        ss->ssl3.hs.cacheSID = rv == SECSuccess;
    }

    if (ss->ssl3.hs.authCertificatePending) {
        if (ss->ssl3.hs.restartTarget) {
            PR_NOT_REACHED("ssl3_HandleFinished: unexpected restartTarget");
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }

        ss->ssl3.hs.restartTarget = ssl3_FinishHandshake;
        return SECWouldBlock;
    }

    rv = ssl3_FinishHandshake(ss);
    return rv;
}

SECStatus
ssl3_FillInCachedSID(sslSocket *ss, sslSessionID *sid)
{
    SECStatus rv;

    /* fill in the sid */
    sid->u.ssl3.cipherSuite = ss->ssl3.hs.cipher_suite;
    sid->u.ssl3.compression = ss->ssl3.hs.compression;
    sid->u.ssl3.policy = ss->ssl3.policy;
    sid->version = ss->version;
    sid->authType = ss->sec.authType;
    sid->authKeyBits = ss->sec.authKeyBits;
    sid->keaType = ss->sec.keaType;
    sid->keaKeyBits = ss->sec.keaKeyBits;
    sid->lastAccessTime = sid->creationTime = ssl_Time();
    sid->expirationTime = sid->creationTime + ssl3_sid_timeout;
    sid->localCert = CERT_DupCertificate(ss->sec.localCert);
    if (ss->sec.isServer) {
        memcpy(&sid->certType, &ss->sec.serverCert->certType, sizeof(sid->certType));
    } else {
        sid->certType.authType = ssl_auth_null;
    }

    if (ss->xtnData.nextProtoState != SSL_NEXT_PROTO_NO_SUPPORT &&
        ss->xtnData.nextProto.data) {
        if (SECITEM_CopyItem(
                NULL, &sid->u.ssl3.alpnSelection, &ss->xtnData.nextProto) != SECSuccess) {
            return SECFailure; /* error already set. */
        }
    }

    ssl_GetSpecReadLock(ss); /*************************************/

    /* Copy the master secret (wrapped or unwrapped) into the sid */
    if (ss->ssl3.crSpec->msItem.len && ss->ssl3.crSpec->msItem.data) {
        sid->u.ssl3.keys.wrapped_master_secret_len =
            ss->ssl3.crSpec->msItem.len;
        memcpy(sid->u.ssl3.keys.wrapped_master_secret,
               ss->ssl3.crSpec->msItem.data, ss->ssl3.crSpec->msItem.len);
        sid->u.ssl3.masterValid = PR_TRUE;
        sid->u.ssl3.keys.msIsWrapped = PR_FALSE;
        rv = SECSuccess;
    } else {
        rv = ssl3_CacheWrappedMasterSecret(ss, ss->sec.ci.sid,
                                           ss->ssl3.crSpec,
                                           ss->ssl3.hs.kea_def->authKeyType);
        sid->u.ssl3.keys.msIsWrapped = PR_TRUE;
    }
    ssl_ReleaseSpecReadLock(ss); /*************************************/

    return rv;
}

/* The return type is SECStatus instead of void because this function needs
 * to have type sslRestartTarget.
 */
SECStatus
ssl3_FinishHandshake(sslSocket *ss)
{
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->ssl3.hs.restartTarget == NULL);

    /* The first handshake is now completed. */
    ss->handshake = NULL;

    /* RFC 5077 Section 3.3: "The client MUST NOT treat the ticket as valid
     * until it has verified the server's Finished message." When the server
     * sends a NewSessionTicket in a resumption handshake, we must wait until
     * the handshake is finished (we have verified the server's Finished
     * AND the server's certificate) before we update the ticket in the sid.
     *
     * This must be done before we call ss->sec.cache(ss->sec.ci.sid)
     * because CacheSID requires the session ticket to already be set, and also
     * because of the lazy lock creation scheme used by CacheSID and
     * ssl3_SetSIDSessionTicket.
     */
    if (ss->ssl3.hs.receivedNewSessionTicket) {
        PORT_Assert(!ss->sec.isServer);
        ssl3_SetSIDSessionTicket(ss->sec.ci.sid, &ss->ssl3.hs.newSessionTicket);
        /* The sid took over the ticket data */
        PORT_Assert(!ss->ssl3.hs.newSessionTicket.ticket.data);
        ss->ssl3.hs.receivedNewSessionTicket = PR_FALSE;
    }

    if (ss->ssl3.hs.cacheSID) {
        PORT_Assert(ss->sec.ci.sid->cached == never_cached);
        ss->sec.cache(ss->sec.ci.sid);
        ss->ssl3.hs.cacheSID = PR_FALSE;
    }

    ss->ssl3.hs.canFalseStart = PR_FALSE; /* False Start phase is complete */
    ss->ssl3.hs.ws = idle_handshake;

    ssl_FinishHandshake(ss);

    return SECSuccess;
}

/* Called from ssl3_HandleHandshake() when it has gathered a complete ssl3
 * hanshake message.
 * Caller must hold Handshake and RecvBuf locks.
 */
SECStatus
ssl3_HandleHandshakeMessage(sslSocket *ss, SSL3Opaque *b, PRUint32 length,
                            PRBool endOfRecord)
{
    SECStatus rv = SECSuccess;
    SSL3HandshakeType type = ss->ssl3.hs.msg_type;
    SSL3Hashes hashes;            /* computed hashes are put here. */
    SSL3Hashes *hashesPtr = NULL; /* Set when hashes are computed */
    PRUint8 hdr[4];
    PRUint8 dtlsData[8];
    PRBool computeHashes = PR_FALSE;
    PRUint16 epoch;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    /*
     * We have to compute the hashes before we update them with the
     * current message.
     */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        if ((type == finished) && (ss->ssl3.hs.ws == wait_finished)) {
            computeHashes = PR_TRUE;
        } else if ((type == certificate_verify) && (ss->ssl3.hs.ws == wait_cert_verify)) {
            if (ss->ssl3.hs.hashType == handshake_hash_record) {
                /* We cannot compute the hash yet. We must wait until we have
                 * decoded the certificate_verify message in
                 * ssl3_HandleCertificateVerify, which will tell us which
                 * hash function we must use.
                 *
                 * (ssl3_HandleCertificateVerify cannot simply look at the
                 * buffer length itself, because at the time we reach it,
                 * additional handshake messages will have been added to the
                 * buffer, e.g. the certificate_verify message itself.)
                 *
                 * Therefore, we use SSL3Hashes.u.pointer_to_hash_input
                 * to signal the current state of the buffer.
                 *
                 * ssl3_HandleCertificateVerify will detect
                 *     hashType == handshake_hash_record
                 * and use that information to calculate the hash.
                 */
                hashes.u.pointer_to_hash_input.data = ss->ssl3.hs.messages.buf;
                hashes.u.pointer_to_hash_input.len = ss->ssl3.hs.messages.len;
                hashesPtr = &hashes;
            } else {
                computeHashes = PR_TRUE;
            }
        }
    } else {
        if (type == certificate_verify) {
            computeHashes = TLS13_IN_HS_STATE(ss, wait_cert_verify);
        } else if (type == finished) {
            computeHashes =
                TLS13_IN_HS_STATE(ss, wait_cert_request, wait_finished);
        }
    }

    ssl_GetSpecReadLock(ss); /************************************/
    if (computeHashes) {
        SSL3Sender sender = (SSL3Sender)0;
        ssl3CipherSpec *rSpec = ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 ? ss->ssl3.crSpec
                                                                           : ss->ssl3.prSpec;

        if (type == finished) {
            sender = ss->sec.isServer ? sender_client : sender_server;
            rSpec = ss->ssl3.crSpec;
        }
        rv = ssl3_ComputeHandshakeHashes(ss, rSpec, &hashes, sender);
        if (rv == SECSuccess) {
            hashesPtr = &hashes;
        }
    }
    ssl_ReleaseSpecReadLock(ss); /************************************/
    if (rv != SECSuccess) {
        return rv; /* error code was set by ssl3_ComputeHandshakeHashes*/
    }
    SSL_TRC(30, ("%d: SSL3[%d]: handle handshake message: %s", SSL_GETPID(),
                 ss->fd, ssl3_DecodeHandshakeType(ss->ssl3.hs.msg_type)));

    hdr[0] = (PRUint8)ss->ssl3.hs.msg_type;
    hdr[1] = (PRUint8)(length >> 16);
    hdr[2] = (PRUint8)(length >> 8);
    hdr[3] = (PRUint8)(length);

    /* Start new handshake hashes when we start a new handshake.  Unless this is
     * TLS 1.3 and we sent a HelloRetryRequest. */
    if (ss->ssl3.hs.msg_type == client_hello && !ss->ssl3.hs.helloRetry) {
        rv = ssl3_RestartHandshakeHashes(ss);
        if (rv != SECSuccess) {
            return rv;
        }
    }
    /* We should not include hello_request and hello_verify_request messages
     * in the handshake hashes */
    if ((ss->ssl3.hs.msg_type != hello_request) &&
        (ss->ssl3.hs.msg_type != hello_verify_request)) {
        rv = ssl3_UpdateHandshakeHashes(ss, (unsigned char *)hdr, 4);
        if (rv != SECSuccess)
            return rv; /* err code already set. */

        /* Extra data to simulate a complete DTLS handshake fragment */
        if (IS_DTLS(ss)) {
            /* Sequence number */
            dtlsData[0] = MSB(ss->ssl3.hs.recvMessageSeq);
            dtlsData[1] = LSB(ss->ssl3.hs.recvMessageSeq);

            /* Fragment offset */
            dtlsData[2] = 0;
            dtlsData[3] = 0;
            dtlsData[4] = 0;

            /* Fragment length */
            dtlsData[5] = (PRUint8)(length >> 16);
            dtlsData[6] = (PRUint8)(length >> 8);
            dtlsData[7] = (PRUint8)(length);

            rv = ssl3_UpdateHandshakeHashes(ss, (unsigned char *)dtlsData,
                                            sizeof(dtlsData));
            if (rv != SECSuccess)
                return rv; /* err code already set. */
        }

        /* The message body */
        rv = ssl3_UpdateHandshakeHashes(ss, b, length);
        if (rv != SECSuccess)
            return rv; /* err code already set. */
    }

    PORT_SetError(0); /* each message starts with no error. */

    if (ss->ssl3.hs.ws == wait_certificate_status &&
        ss->ssl3.hs.msg_type != certificate_status) {
        /* If we negotiated the certificate_status extension then we deferred
         * certificate validation until we get the CertificateStatus messsage.
         * But the CertificateStatus message is optional. If the server did
         * not send it then we need to validate the certificate now. If the
         * server does send the CertificateStatus message then we will
         * authenticate the certificate in ssl3_HandleCertificateStatus.
         */
        rv = ssl3_AuthCertificate(ss); /* sets ss->ssl3.hs.ws */
        PORT_Assert(rv != SECWouldBlock);
        if (rv != SECSuccess) {
            return rv;
        }
    }

    epoch = ss->ssl3.crSpec->epoch;
    switch (ss->ssl3.hs.msg_type) {
        case client_hello:
            if (!ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CLIENT_HELLO);
                return SECFailure;
            }
            rv = ssl3_HandleClientHello(ss, b, length);
            break;
        case server_hello:
            if (ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_SERVER_HELLO);
                return SECFailure;
            }
            rv = ssl3_HandleServerHello(ss, b, length);
            break;
        default:
            if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
                rv = ssl3_HandlePostHelloHandshakeMessage(ss, b, length, hashesPtr);
            } else {
                rv = tls13_HandlePostHelloHandshakeMessage(ss, b, length,
                                                           hashesPtr);
            }
            break;
    }
    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 &&
        (epoch != ss->ssl3.crSpec->epoch) && !endOfRecord) {
        /* If we changed read cipher states, there must not be any
         * data in the input queue. */
        (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
        return SECFailure;
    }

    if (IS_DTLS(ss) && (rv != SECFailure)) {
        /* Increment the expected sequence number */
        ss->ssl3.hs.recvMessageSeq++;
    }
    return rv;
}

static SECStatus
ssl3_HandlePostHelloHandshakeMessage(sslSocket *ss, SSL3Opaque *b,
                                     PRUint32 length, SSL3Hashes *hashesPtr)
{
    SECStatus rv;
    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

    switch (ss->ssl3.hs.msg_type) {
        case hello_request:
            if (length != 0) {
                (void)ssl3_DecodeError(ss);
                PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_REQUEST);
                return SECFailure;
            }
            if (ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_HELLO_REQUEST);
                return SECFailure;
            }
            rv = ssl3_HandleHelloRequest(ss);
            break;

        case hello_retry_request:
            /* This arrives here because - as a client - we haven't received a
             * final decision on the version from the server. */
            rv = tls13_HandleHelloRetryRequest(ss, b, length);
            break;

        case hello_verify_request:
            if (!IS_DTLS(ss) || ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_HELLO_VERIFY_REQUEST);
                return SECFailure;
            }
            rv = dtls_HandleHelloVerifyRequest(ss, b, length);
            break;
        case certificate:
            rv = ssl3_HandleCertificate(ss, b, length);
            break;
        case certificate_status:
            rv = ssl3_HandleCertificateStatus(ss, b, length);
            break;
        case server_key_exchange:
            if (ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH);
                return SECFailure;
            }
            rv = ssl3_HandleServerKeyExchange(ss, b, length);
            break;
        case certificate_request:
            if (ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST);
                return SECFailure;
            }
            rv = ssl3_HandleCertificateRequest(ss, b, length);
            break;
        case server_hello_done:
            if (length != 0) {
                (void)ssl3_DecodeError(ss);
                PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_DONE);
                return SECFailure;
            }
            if (ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
                return SECFailure;
            }
            rv = ssl3_HandleServerHelloDone(ss);
            break;
        case certificate_verify:
            if (!ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY);
                return SECFailure;
            }
            rv = ssl3_HandleCertificateVerify(ss, b, length, hashesPtr);
            break;
        case client_key_exchange:
            if (!ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_CLIENT_KEY_EXCH);
                return SECFailure;
            }
            rv = ssl3_HandleClientKeyExchange(ss, b, length);
            break;
        case new_session_ticket:
            if (ss->sec.isServer) {
                (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET);
                return SECFailure;
            }
            rv = ssl3_HandleNewSessionTicket(ss, b, length);
            break;
        case finished:
            rv = ssl3_HandleFinished(ss, b, length, hashesPtr);
            break;
        default:
            (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
            PORT_SetError(SSL_ERROR_RX_UNKNOWN_HANDSHAKE);
            rv = SECFailure;
    }

    return rv;
}

/* Called only from ssl3_HandleRecord, for each (deciphered) ssl3 record.
 * origBuf is the decrypted ssl record content.
 * Caller must hold the handshake and RecvBuf locks.
 */
static SECStatus
ssl3_HandleHandshake(sslSocket *ss, sslBuffer *origBuf)
{
    /*
     * There may be a partial handshake message already in the handshake
     * state. The incoming buffer may contain another portion, or a
     * complete message or several messages followed by another portion.
     *
     * Each message is made contiguous before being passed to the actual
     * message parser.
     */
    sslBuffer *buf = &ss->ssl3.hs.msgState; /* do not lose the original buffer pointer */
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (buf->buf == NULL) {
        *buf = *origBuf;
    }
    while (buf->len > 0) {
        if (ss->ssl3.hs.header_bytes < 4) {
            PRUint8 t;
            t = *(buf->buf++);
            buf->len--;
            if (ss->ssl3.hs.header_bytes++ == 0)
                ss->ssl3.hs.msg_type = (SSL3HandshakeType)t;
            else
                ss->ssl3.hs.msg_len = (ss->ssl3.hs.msg_len << 8) + t;
            if (ss->ssl3.hs.header_bytes < 4)
                continue;

#define MAX_HANDSHAKE_MSG_LEN 0x1ffff /* 128k - 1 */
            if (ss->ssl3.hs.msg_len > MAX_HANDSHAKE_MSG_LEN) {
                (void)ssl3_DecodeError(ss);
                PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
                return SECFailure;
            }
#undef MAX_HANDSHAKE_MSG_LEN

            /* If msg_len is zero, be sure we fall through,
            ** even if buf->len is zero.
            */
            if (ss->ssl3.hs.msg_len > 0)
                continue;
        }

        /*
         * Header has been gathered and there is at least one byte of new
         * data available for this message. If it can be done right out
         * of the original buffer, then use it from there.
         */
        if (ss->ssl3.hs.msg_body.len == 0 && buf->len >= ss->ssl3.hs.msg_len) {
            /* handle it from input buffer */
            rv = ssl3_HandleHandshakeMessage(ss, buf->buf, ss->ssl3.hs.msg_len,
                                             buf->len == ss->ssl3.hs.msg_len);
            if (rv == SECFailure) {
                /* This test wants to fall through on either
                 * SECSuccess or SECWouldBlock.
                 * ssl3_HandleHandshakeMessage MUST set the error code.
                 */
                return rv;
            }
            buf->buf += ss->ssl3.hs.msg_len;
            buf->len -= ss->ssl3.hs.msg_len;
            ss->ssl3.hs.msg_len = 0;
            ss->ssl3.hs.header_bytes = 0;
            if (rv != SECSuccess) { /* return if SECWouldBlock. */
                return rv;
            }
        } else {
            /* must be copied to msg_body and dealt with from there */
            unsigned int bytes;

            PORT_Assert(ss->ssl3.hs.msg_body.len < ss->ssl3.hs.msg_len);
            bytes = PR_MIN(buf->len, ss->ssl3.hs.msg_len - ss->ssl3.hs.msg_body.len);

            /* Grow the buffer if needed */
            rv = sslBuffer_Grow(&ss->ssl3.hs.msg_body, ss->ssl3.hs.msg_len);
            if (rv != SECSuccess) {
                /* sslBuffer_Grow has set a memory error code. */
                return SECFailure;
            }

            PORT_Memcpy(ss->ssl3.hs.msg_body.buf + ss->ssl3.hs.msg_body.len,
                        buf->buf, bytes);
            ss->ssl3.hs.msg_body.len += bytes;
            buf->buf += bytes;
            buf->len -= bytes;

            PORT_Assert(ss->ssl3.hs.msg_body.len <= ss->ssl3.hs.msg_len);

            /* if we have a whole message, do it */
            if (ss->ssl3.hs.msg_body.len == ss->ssl3.hs.msg_len) {
                rv = ssl3_HandleHandshakeMessage(
                    ss, ss->ssl3.hs.msg_body.buf, ss->ssl3.hs.msg_len,
                    buf->len == 0);
                if (rv == SECFailure) {
                    /* This test wants to fall through on either
                     * SECSuccess or SECWouldBlock.
                     * ssl3_HandleHandshakeMessage MUST set error code.
                     */
                    return rv;
                }
                ss->ssl3.hs.msg_body.len = 0;
                ss->ssl3.hs.msg_len = 0;
                ss->ssl3.hs.header_bytes = 0;
                if (rv != SECSuccess) { /* return if SECWouldBlock. */
                    return rv;
                }
            } else {
                PORT_Assert(buf->len == 0);
                break;
            }
        }
    } /* end loop */

    origBuf->len = 0; /* So ssl3_GatherAppDataRecord will keep looping. */
    buf->buf = NULL;  /* not a leak. */
    return SECSuccess;
}

/* These macros return the given value with the MSB copied to all the other
 * bits. They use the fact that arithmetic shift shifts-in the sign bit.
 * However, this is not ensured by the C standard so you may need to replace
 * them with something else for odd compilers. */
#define DUPLICATE_MSB_TO_ALL(x) ((unsigned)((int)(x) >> (sizeof(int) * 8 - 1)))
#define DUPLICATE_MSB_TO_ALL_8(x) ((unsigned char)(DUPLICATE_MSB_TO_ALL(x)))

/* SECStatusToMask returns, in constant time, a mask value of all ones if
 * rv == SECSuccess.  Otherwise it returns zero. */
static unsigned int
SECStatusToMask(SECStatus rv)
{
    unsigned int good;
    /* rv ^ SECSuccess is zero iff rv == SECSuccess. Subtracting one results
     * in the MSB being set to one iff it was zero before. */
    good = rv ^ SECSuccess;
    good--;
    return DUPLICATE_MSB_TO_ALL(good);
}

/* ssl_ConstantTimeGE returns 0xff if a>=b and 0x00 otherwise. */
static unsigned char
ssl_ConstantTimeGE(unsigned int a, unsigned int b)
{
    a -= b;
    return DUPLICATE_MSB_TO_ALL(~a);
}

/* ssl_ConstantTimeEQ8 returns 0xff if a==b and 0x00 otherwise. */
static unsigned char
ssl_ConstantTimeEQ8(unsigned char a, unsigned char b)
{
    unsigned int c = a ^ b;
    c--;
    return DUPLICATE_MSB_TO_ALL_8(c);
}

/* ssl_constantTimeSelect return a if mask is 0xFF and b if mask is 0x00 */
static unsigned char
ssl_constantTimeSelect(unsigned char mask, unsigned char a, unsigned char b)
{
    return (mask & a) | (~mask & b);
}

static SECStatus
ssl_RemoveSSLv3CBCPadding(sslBuffer *plaintext,
                          unsigned int blockSize,
                          unsigned int macSize)
{
    unsigned int paddingLength, good, t;
    const unsigned int overhead = 1 /* padding length byte */ + macSize;

    /* These lengths are all public so we can test them in non-constant
     * time. */
    if (overhead > plaintext->len) {
        return SECFailure;
    }

    paddingLength = plaintext->buf[plaintext->len - 1];
    /* SSLv3 padding bytes are random and cannot be checked. */
    t = plaintext->len;
    t -= paddingLength + overhead;
    /* If len >= paddingLength+overhead then the MSB of t is zero. */
    good = DUPLICATE_MSB_TO_ALL(~t);
    /* SSLv3 requires that the padding is minimal. */
    t = blockSize - (paddingLength + 1);
    good &= DUPLICATE_MSB_TO_ALL(~t);
    plaintext->len -= good & (paddingLength + 1);
    return (good & SECSuccess) | (~good & SECFailure);
}

SECStatus
ssl_RemoveTLSCBCPadding(sslBuffer *plaintext, unsigned int macSize)
{
    unsigned int paddingLength, good, t, toCheck, i;
    const unsigned int overhead = 1 /* padding length byte */ + macSize;

    /* These lengths are all public so we can test them in non-constant
     * time. */
    if (overhead > plaintext->len) {
        return SECFailure;
    }

    paddingLength = plaintext->buf[plaintext->len - 1];
    t = plaintext->len;
    t -= paddingLength + overhead;
    /* If len >= paddingLength+overhead then the MSB of t is zero. */
    good = DUPLICATE_MSB_TO_ALL(~t);

    /* The padding consists of a length byte at the end of the record and then
     * that many bytes of padding, all with the same value as the length byte.
     * Thus, with the length byte included, there are paddingLength+1 bytes of
     * padding.
     *
     * We can't check just |paddingLength+1| bytes because that leaks
     * decrypted information. Therefore we always have to check the maximum
     * amount of padding possible. (Again, the length of the record is
     * public information so we can use it.) */
    toCheck = 256; /* maximum amount of padding + 1. */
    if (toCheck > plaintext->len) {
        toCheck = plaintext->len;
    }

    for (i = 0; i < toCheck; i++) {
        unsigned int t = paddingLength - i;
        /* If i <= paddingLength then the MSB of t is zero and mask is
         * 0xff.  Otherwise, mask is 0. */
        unsigned char mask = DUPLICATE_MSB_TO_ALL(~t);
        unsigned char b = plaintext->buf[plaintext->len - 1 - i];
        /* The final |paddingLength+1| bytes should all have the value
         * |paddingLength|. Therefore the XOR should be zero. */
        good &= ~(mask & (paddingLength ^ b));
    }

    /* If any of the final |paddingLength+1| bytes had the wrong value,
     * one or more of the lower eight bits of |good| will be cleared. We
     * AND the bottom 8 bits together and duplicate the result to all the
     * bits. */
    good &= good >> 4;
    good &= good >> 2;
    good &= good >> 1;
    good <<= sizeof(good) * 8 - 1;
    good = DUPLICATE_MSB_TO_ALL(good);

    plaintext->len -= good & (paddingLength + 1);
    return (good & SECSuccess) | (~good & SECFailure);
}

/* On entry:
 *   originalLength >= macSize
 *   macSize <= MAX_MAC_LENGTH
 *   plaintext->len >= macSize
 */
static void
ssl_CBCExtractMAC(sslBuffer *plaintext,
                  unsigned int originalLength,
                  SSL3Opaque *out,
                  unsigned int macSize)
{
    unsigned char rotatedMac[MAX_MAC_LENGTH];
    /* macEnd is the index of |plaintext->buf| just after the end of the
     * MAC. */
    unsigned macEnd = plaintext->len;
    unsigned macStart = macEnd - macSize;
    /* scanStart contains the number of bytes that we can ignore because
     * the MAC's position can only vary by 255 bytes. */
    unsigned scanStart = 0;
    unsigned i, j;
    unsigned char rotateOffset;

    if (originalLength > macSize + 255 + 1) {
        scanStart = originalLength - (macSize + 255 + 1);
    }

    /* We want to compute
     * rotateOffset = (macStart - scanStart) % macSize
     * But the time to compute this varies based on the amount of padding. Thus
     * we explicitely handle all mac sizes with (hopefully) constant time modulo
     * using Barrett reduction:
     *  q := (rotateOffset * m) >> k
     *  rotateOffset -= q * n
     *  if (n <= rotateOffset) rotateOffset -= n
     */
    rotateOffset = macStart - scanStart;
    /* rotateOffset < 255 + 1 + 48 = 304 */
    if (macSize == 16) {
        rotateOffset &= 15;
    } else if (macSize == 20) {
        /*
         * Correctness: rotateOffset * ( 1/20 - 25/2^9 ) < 1
         *              with rotateOffset <= 853
         */
        unsigned q = (rotateOffset * 25) >> 9;
        rotateOffset -= q * 20;
        rotateOffset -= ssl_constantTimeSelect(ssl_ConstantTimeGE(rotateOffset, 20),
                                               20, 0);
    } else if (macSize == 32) {
        rotateOffset &= 31;
    } else if (macSize == 48) {
        /*
         * Correctness: rotateOffset * ( 1/48 - 10/2^9 ) < 1
         *              with rotateOffset < 768
         */
        unsigned q = (rotateOffset * 10) >> 9;
        rotateOffset -= q * 48;
        rotateOffset -= ssl_constantTimeSelect(ssl_ConstantTimeGE(rotateOffset, 48),
                                               48, 0);
    } else {
        /*
         * SHA384 (macSize == 48) is the largest we support. We should never
         * get here.
         */
        PORT_Assert(0);
        rotateOffset = rotateOffset % macSize;
    }

    memset(rotatedMac, 0, macSize);
    for (i = scanStart; i < originalLength;) {
        for (j = 0; j < macSize && i < originalLength; i++, j++) {
            unsigned char macStarted = ssl_ConstantTimeGE(i, macStart);
            unsigned char macEnded = ssl_ConstantTimeGE(i, macEnd);
            unsigned char b = 0;
            b = plaintext->buf[i];
            rotatedMac[j] |= b & macStarted & ~macEnded;
        }
    }

    /* Now rotate the MAC. If we knew that the MAC fit into a CPU cache line
     * we could line-align |rotatedMac| and rotate in place. */
    memset(out, 0, macSize);
    rotateOffset = macSize - rotateOffset;
    rotateOffset = ssl_constantTimeSelect(ssl_ConstantTimeGE(rotateOffset, macSize),
                                          0, rotateOffset);
    for (i = 0; i < macSize; i++) {
        for (j = 0; j < macSize; j++) {
            out[j] |= rotatedMac[i] & ssl_ConstantTimeEQ8(j, rotateOffset);
        }
        rotateOffset++;
        rotateOffset = ssl_constantTimeSelect(ssl_ConstantTimeGE(rotateOffset, macSize),
                                              0, rotateOffset);
    }
}

/* Unprotect an SSL3 record and leave the result in plaintext.
 *
 * If SECFailure is returned, we:
 * 1. Set |*alert| to the alert to be sent.
 * 2. Call PORT_SetError() with an appropriate code.
 *
 * Called by ssl3_HandleRecord. Caller must hold the spec read lock.
 * Therefore, we MUST not call SSL3_SendAlert().
 *
 */
static SECStatus
ssl3_UnprotectRecord(sslSocket *ss, SSL3Ciphertext *cText, sslBuffer *plaintext,
                     SSL3AlertDescription *alert)
{
    ssl3CipherSpec *crSpec = ss->ssl3.crSpec;
    const ssl3BulkCipherDef *cipher_def = crSpec->cipher_def;
    PRBool isTLS;
    unsigned int good;
    unsigned int ivLen = 0;
    SSL3ContentType rType;
    unsigned int minLength;
    unsigned int originalLen = 0;
    unsigned char header[13];
    unsigned int headerLen;
    SSL3Opaque hash[MAX_MAC_LENGTH];
    SSL3Opaque givenHashBuf[MAX_MAC_LENGTH];
    SSL3Opaque *givenHash;
    unsigned int hashBytes = MAX_MAC_LENGTH + 1;
    SECStatus rv;

    good = ~0U;
    minLength = crSpec->mac_size;
    if (cipher_def->type == type_block) {
        /* CBC records have a padding length byte at the end. */
        minLength++;
        if (crSpec->version >= SSL_LIBRARY_VERSION_TLS_1_1) {
            /* With >= TLS 1.1, CBC records have an explicit IV. */
            minLength += cipher_def->iv_size;
        }
    } else if (cipher_def->type == type_aead) {
        minLength = cipher_def->explicit_nonce_size + cipher_def->tag_size;
    }

    /* We can perform this test in variable time because the record's total
     * length and the ciphersuite are both public knowledge. */
    if (cText->buf->len < minLength) {
        goto decrypt_loser;
    }

    if (cipher_def->type == type_block &&
        crSpec->version >= SSL_LIBRARY_VERSION_TLS_1_1) {
        /* Consume the per-record explicit IV. RFC 4346 Section 6.2.3.2 states
         * "The receiver decrypts the entire GenericBlockCipher structure and
         * then discards the first cipher block corresponding to the IV
         * component." Instead, we decrypt the first cipher block and then
         * discard it before decrypting the rest.
         */
        SSL3Opaque iv[MAX_IV_LENGTH];
        int decoded;

        ivLen = cipher_def->iv_size;
        if (ivLen < 8 || ivLen > sizeof(iv)) {
            *alert = internal_error;
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }

        PRINT_BUF(80, (ss, "IV (ciphertext):", cText->buf->buf, ivLen));

        /* The decryption result is garbage, but since we just throw away
         * the block it doesn't matter.  The decryption of the next block
         * depends only on the ciphertext of the IV block.
         */
        rv = crSpec->decode(crSpec->decodeContext, iv, &decoded,
                            sizeof(iv), cText->buf->buf, ivLen);

        good &= SECStatusToMask(rv);
    }

    PRINT_BUF(80, (ss, "ciphertext:", cText->buf->buf + ivLen,
                   cText->buf->len - ivLen));

    isTLS = (PRBool)(crSpec->version > SSL_LIBRARY_VERSION_3_0);

    if (isTLS && cText->buf->len - ivLen > (MAX_FRAGMENT_LENGTH + 2048)) {
        *alert = record_overflow;
        PORT_SetError(SSL_ERROR_RX_RECORD_TOO_LONG);
        return SECFailure;
    }

    rType = cText->type;
    if (cipher_def->type == type_aead) {
        /* XXX For many AEAD ciphers, the plaintext is shorter than the
         * ciphertext by a fixed byte count, but it is not true in general.
         * Each AEAD cipher should provide a function that returns the
         * plaintext length for a given ciphertext. */
        unsigned int decryptedLen =
            cText->buf->len - cipher_def->explicit_nonce_size -
            cipher_def->tag_size;
        headerLen = ssl3_BuildRecordPseudoHeader(
            header, IS_DTLS(ss) ? cText->seq_num : crSpec->read_seq_num,
            rType, isTLS, cText->version, IS_DTLS(ss), decryptedLen);
        PORT_Assert(headerLen <= sizeof(header));
        rv = crSpec->aead(
            ss->sec.isServer ? &crSpec->client : &crSpec->server,
            PR_TRUE,                /* do decrypt */
            plaintext->buf,         /* out */
            (int *)&plaintext->len, /* outlen */
            plaintext->space,       /* maxout */
            cText->buf->buf,        /* in */
            cText->buf->len,        /* inlen */
            header, headerLen);
        if (rv != SECSuccess) {
            good = 0;
        }
    } else {
        if (cipher_def->type == type_block &&
            ((cText->buf->len - ivLen) % cipher_def->block_size) != 0) {
            goto decrypt_loser;
        }

        /* decrypt from cText buf to plaintext. */
        rv = crSpec->decode(
            crSpec->decodeContext, plaintext->buf, (int *)&plaintext->len,
            plaintext->space, cText->buf->buf + ivLen, cText->buf->len - ivLen);
        if (rv != SECSuccess) {
            goto decrypt_loser;
        }

        PRINT_BUF(80, (ss, "cleartext:", plaintext->buf, plaintext->len));

        originalLen = plaintext->len;

        /* If it's a block cipher, check and strip the padding. */
        if (cipher_def->type == type_block) {
            const unsigned int blockSize = cipher_def->block_size;
            const unsigned int macSize = crSpec->mac_size;

            if (!isTLS) {
                good &= SECStatusToMask(ssl_RemoveSSLv3CBCPadding(
                    plaintext, blockSize, macSize));
            } else {
                good &= SECStatusToMask(ssl_RemoveTLSCBCPadding(
                    plaintext, macSize));
            }
        }

        /* compute the MAC */
        headerLen = ssl3_BuildRecordPseudoHeader(
            header, IS_DTLS(ss) ? cText->seq_num : crSpec->read_seq_num,
            rType, isTLS, cText->version, IS_DTLS(ss),
            plaintext->len - crSpec->mac_size);
        PORT_Assert(headerLen <= sizeof(header));
        if (cipher_def->type == type_block) {
            rv = ssl3_ComputeRecordMACConstantTime(
                crSpec, (PRBool)(!ss->sec.isServer), header, headerLen,
                plaintext->buf, plaintext->len, originalLen,
                hash, &hashBytes);

            ssl_CBCExtractMAC(plaintext, originalLen, givenHashBuf,
                              crSpec->mac_size);
            givenHash = givenHashBuf;

            /* plaintext->len will always have enough space to remove the MAC
             * because in ssl_Remove{SSLv3|TLS}CBCPadding we only adjust
             * plaintext->len if the result has enough space for the MAC and we
             * tested the unadjusted size against minLength, above. */
            plaintext->len -= crSpec->mac_size;
        } else {
            /* This is safe because we checked the minLength above. */
            plaintext->len -= crSpec->mac_size;

            rv = ssl3_ComputeRecordMAC(
                crSpec, (PRBool)(!ss->sec.isServer), header, headerLen,
                plaintext->buf, plaintext->len, hash, &hashBytes);

            /* We can read the MAC directly from the record because its location
             * is public when a stream cipher is used. */
            givenHash = plaintext->buf + plaintext->len;
        }

        good &= SECStatusToMask(rv);

        if (hashBytes != (unsigned)crSpec->mac_size ||
            NSS_SecureMemcmp(givenHash, hash, crSpec->mac_size) != 0) {
            /* We're allowed to leak whether or not the MAC check was correct */
            good = 0;
        }
    }

    if (good == 0) {
    decrypt_loser:
        /* always log mac error, in case attacker can read server logs. */
        PORT_SetError(SSL_ERROR_BAD_MAC_READ);
        *alert = bad_record_mac;
        return SECFailure;
    }
    return SECSuccess;
}

/* if cText is non-null, then decipher, check MAC, and decompress the
 * SSL record from cText->buf (typically gs->inbuf)
 * into databuf (typically gs->buf), and any previous contents of databuf
 * is lost.  Then handle databuf according to its SSL record type,
 * unless it's an application record.
 *
 * If cText is NULL, then the ciphertext has previously been deciphered and
 * checked, and is already sitting in databuf.  It is processed as an SSL
 * Handshake message.
 *
 * DOES NOT process the decrypted/decompressed application data.
 * On return, databuf contains the decrypted/decompressed record.
 *
 * Called from ssl3_GatherCompleteHandshake
 *             ssl3_RestartHandshakeAfterCertReq
 *
 * Caller must hold the RecvBufLock.
 *
 * This function aquires and releases the SSL3Handshake Lock, holding the
 * lock around any calls to functions that handle records other than
 * Application Data records.
 */
SECStatus
ssl3_HandleRecord(sslSocket *ss, SSL3Ciphertext *cText, sslBuffer *databuf)
{
    SECStatus rv;
    PRBool isTLS;
    sslSequenceNumber seq_num = 0;
    ssl3CipherSpec *crSpec;
    SSL3ContentType rType;
    sslBuffer *plaintext;
    sslBuffer temp_buf;
    SSL3AlertDescription alert = internal_error;
    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));

    if (!ss->ssl3.initialized) {
        ssl_GetSSL3HandshakeLock(ss);
        rv = ssl3_InitState(ss);
        ssl_ReleaseSSL3HandshakeLock(ss);
        if (rv != SECSuccess) {
            return rv; /* ssl3_InitState has set the error code. */
        }
    }

    /* check for Token Presence */
    if (!ssl3_ClientAuthTokenPresent(ss->sec.ci.sid)) {
        PORT_SetError(SSL_ERROR_TOKEN_INSERTION_REMOVAL);
        return SECFailure;
    }

    /* cText is NULL when we're called from ssl3_RestartHandshakeAfterXXX().
     * This implies that databuf holds a previously deciphered SSL Handshake
     * message.
     */
    if (cText == NULL) {
        SSL_DBG(("%d: SSL3[%d]: HandleRecord, resuming handshake",
                 SSL_GETPID(), ss->fd));
        rType = content_handshake;
        goto process_it;
    }

    ssl_GetSpecReadLock(ss); /******************************************/
    crSpec = ss->ssl3.crSpec;
    isTLS = (PRBool)(crSpec->version > SSL_LIBRARY_VERSION_3_0);

    if (IS_DTLS(ss)) {
        PRBool sameEpoch;
        if (!dtls_IsRelevant(ss, cText, &sameEpoch, &seq_num)) {
            ssl_ReleaseSpecReadLock(ss); /*****************************/
            databuf->len = 0;            /* Needed to ensure data not left around */

            /* Maybe retransmit if needed. */
            return dtls_MaybeRetransmitHandshake(ss, cText, sameEpoch);
        }
    } else {
        seq_num = crSpec->read_seq_num + 1;
    }
    if (seq_num >= crSpec->cipher_def->max_records) {
        ssl_ReleaseSpecReadLock(ss); /*****************************/
        SSL_TRC(3, ("%d: SSL[%d]: read sequence number at limit 0x%0llx",
                    SSL_GETPID(), ss->fd, seq_num));
        PORT_SetError(SSL_ERROR_TOO_MANY_RECORDS);
        return SECFailure;
    }

    /* If we will be decompressing the buffer we need to decrypt somewhere
     * other than into databuf */
    if (crSpec->decompressor) {
        temp_buf.buf = NULL;
        temp_buf.space = 0;
        plaintext = &temp_buf;
    } else {
        plaintext = databuf;
    }

    plaintext->len = 0; /* filled in by Unprotect call below. */
    if (plaintext->space < MAX_FRAGMENT_LENGTH) {
        rv = sslBuffer_Grow(plaintext, MAX_FRAGMENT_LENGTH + 2048);
        if (rv != SECSuccess) {
            ssl_ReleaseSpecReadLock(ss); /*************************/
            SSL_DBG(("%d: SSL3[%d]: HandleRecord, tried to get %d bytes",
                     SSL_GETPID(), ss->fd, MAX_FRAGMENT_LENGTH + 2048));
            /* sslBuffer_Grow has set a memory error code. */
            /* Perhaps we should send an alert. (but we have no memory!) */
            return SECFailure;
        }
    }

    /* We're waiting for another ClientHello, which will appear unencrypted.
     * Use the content type to tell whether this is should be discarded.
     *
     * XXX If we decide to remove the content type from encrypted records, this
     *     will become much more difficult to manage. */
    if (ss->ssl3.hs.zeroRttIgnore == ssl_0rtt_ignore_hrr &&
        cText->type == content_application_data) {
        ssl_ReleaseSpecReadLock(ss); /*****************************/
        PORT_Assert(ss->ssl3.hs.ws == wait_client_hello);
        databuf->len = 0;
        return SECSuccess;
    }

#ifdef UNSAFE_FUZZER_MODE
    rv = Null_Cipher(NULL, plaintext->buf, (int *)&plaintext->len,
                     plaintext->space, cText->buf->buf, cText->buf->len);
#else
    /* IMPORTANT: Unprotect functions MUST NOT send alerts
     * because we still hold the spec read lock. Instead, if they
     * return SECFailure, they set *alert to the alert to be sent. */
    if (crSpec->version < SSL_LIBRARY_VERSION_TLS_1_3 ||
        crSpec->cipher_def->calg == ssl_calg_null) {
        /* Unencrypted TLS 1.3 records use the pre-TLS 1.3 format. */
        rv = ssl3_UnprotectRecord(ss, cText, plaintext, &alert);
    } else {
        rv = tls13_UnprotectRecord(ss, cText, plaintext, &alert);
    }
#endif

    if (rv != SECSuccess) {
        ssl_ReleaseSpecReadLock(ss); /***************************/

        SSL_DBG(("%d: SSL3[%d]: decryption failed", SSL_GETPID(), ss->fd));

        if (IS_DTLS(ss) ||
            (ss->sec.isServer &&
             ss->ssl3.hs.zeroRttIgnore == ssl_0rtt_ignore_trial)) {
            /* Silently drop the packet */
            databuf->len = 0; /* Needed to ensure data not left around */
            return SECSuccess;
        } else {
            int errCode = PORT_GetError();
            SSL3_SendAlert(ss, alert_fatal, alert);
            /* Reset the error code in case SSL3_SendAlert called
             * PORT_SetError(). */
            PORT_SetError(errCode);
            return SECFailure;
        }
    }

    /* SECSuccess */
    crSpec->read_seq_num = seq_num;
    if (IS_DTLS(ss)) {
        dtls_RecordSetRecvd(&crSpec->recvdRecords, seq_num);
    }

    ssl_ReleaseSpecReadLock(ss); /*****************************************/

    /*
     * The decrypted data is now in plaintext.
     */
    rType = cText->type; /* This must go after decryption because TLS 1.3
                          * has encrypted content types. */

    /* possibly decompress the record. If we aren't using compression then
     * plaintext == databuf and so the uncompressed data is already in
     * databuf. */
    if (crSpec->decompressor) {
        if (databuf->space < plaintext->len + SSL3_COMPRESSION_MAX_EXPANSION) {
            rv = sslBuffer_Grow(
                databuf, plaintext->len + SSL3_COMPRESSION_MAX_EXPANSION);
            if (rv != SECSuccess) {
                SSL_DBG(("%d: SSL3[%d]: HandleRecord, tried to get %d bytes",
                         SSL_GETPID(), ss->fd,
                         plaintext->len +
                             SSL3_COMPRESSION_MAX_EXPANSION));
                /* sslBuffer_Grow has set a memory error code. */
                /* Perhaps we should send an alert. (but we have no memory!) */
                PORT_Free(plaintext->buf);
                return SECFailure;
            }
        }

        rv = crSpec->decompressor(crSpec->decompressContext,
                                  databuf->buf,
                                  (int *)&databuf->len,
                                  databuf->space,
                                  plaintext->buf,
                                  plaintext->len);

        if (rv != SECSuccess) {
            int err = ssl_MapLowLevelError(SSL_ERROR_DECOMPRESSION_FAILURE);
            SSL3_SendAlert(ss, alert_fatal,
                           isTLS ? decompression_failure
                                 : bad_record_mac);

            /* There appears to be a bug with (at least) Apache + OpenSSL where
             * resumed SSLv3 connections don't actually use compression. See
             * comments 93-95 of
             * https://bugzilla.mozilla.org/show_bug.cgi?id=275744
             *
             * So, if we get a decompression error, and the record appears to
             * be already uncompressed, then we return a more specific error
             * code to hopefully save somebody some debugging time in the
             * future.
             */
            if (plaintext->len >= 4) {
                unsigned int len = ((unsigned int)plaintext->buf[1] << 16) |
                                   ((unsigned int)plaintext->buf[2] << 8) |
                                   (unsigned int)plaintext->buf[3];
                if (len == plaintext->len - 4) {
                    /* This appears to be uncompressed already */
                    err = SSL_ERROR_RX_UNEXPECTED_UNCOMPRESSED_RECORD;
                }
            }

            PORT_Free(plaintext->buf);
            PORT_SetError(err);
            return SECFailure;
        }

        PORT_Free(plaintext->buf);
    }

    /*
    ** Having completed the decompression, check the length again.
    */
    if (isTLS && databuf->len > (MAX_FRAGMENT_LENGTH + 1024)) {
        SSL3_SendAlert(ss, alert_fatal, record_overflow);
        PORT_SetError(SSL_ERROR_RX_RECORD_TOO_LONG);
        return SECFailure;
    }

    /* Application data records are processed by the caller of this
    ** function, not by this function.
    */
    if (rType == content_application_data) {
        if (ss->firstHsDone)
            return SECSuccess;
        if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 &&
            ss->sec.isServer &&
            ss->ssl3.hs.zeroRttState == ssl_0rtt_accepted) {
            return tls13_HandleEarlyApplicationData(ss, databuf);
        }
        (void)SSL3_SendAlert(ss, alert_fatal, unexpected_message);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA);
        return SECFailure;
    }

/* It's a record that must be handled by ssl itself, not the application.
    */
process_it:
    /* XXX  Get the xmit lock here.  Odds are very high that we'll be xmiting
     * data ang getting the xmit lock here prevents deadlocks.
     */
    ssl_GetSSL3HandshakeLock(ss);

    /* All the functions called in this switch MUST set error code if
    ** they return SECFailure or SECWouldBlock.
    */
    switch (rType) {
        case content_change_cipher_spec:
            rv = ssl3_HandleChangeCipherSpecs(ss, databuf);
            break;
        case content_alert:
            rv = ssl3_HandleAlert(ss, databuf);
            break;
        case content_handshake:
            if (!IS_DTLS(ss)) {
                rv = ssl3_HandleHandshake(ss, databuf);
            } else {
                rv = dtls_HandleHandshake(ss, databuf);
            }
            break;
        /*
        case content_application_data is handled before this switch
        */
        default:
            SSL_DBG(("%d: SSL3[%d]: bogus content type=%d",
                     SSL_GETPID(), ss->fd, cText->type));
            PORT_SetError(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE);
            ssl3_DecodeError(ss);
            rv = SECFailure;
            break;
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    return rv;
}

/*
 * Initialization functions
 */

void
ssl_InitSecState(sslSecurityInfo *sec)
{
    sec->authType = ssl_auth_null;
    sec->authKeyBits = 0;
    sec->signatureScheme = ssl_sig_none;
    sec->keaType = ssl_kea_null;
    sec->keaKeyBits = 0;
    sec->keaGroup = NULL;
}

/* Called from ssl3_InitState, immediately below. */
/* Caller must hold the SpecWriteLock. */
void
ssl3_InitCipherSpec(ssl3CipherSpec *spec)
{
    spec->cipher_def = &bulk_cipher_defs[cipher_null];
    PORT_Assert(spec->cipher_def->cipher == cipher_null);
    spec->mac_def = &mac_defs[mac_null];
    PORT_Assert(spec->mac_def->mac == mac_null);
    spec->encode = Null_Cipher;
    spec->decode = Null_Cipher;
    spec->compressor = NULL;
    spec->decompressor = NULL;
    spec->destroyCompressContext = NULL;
    spec->destroyDecompressContext = NULL;
    spec->mac_size = 0;
    spec->master_secret = NULL;

    spec->msItem.data = NULL;
    spec->msItem.len = 0;

    spec->client.write_key = NULL;
    spec->client.write_mac_key = NULL;
    spec->client.write_mac_context = NULL;

    spec->server.write_key = NULL;
    spec->server.write_mac_key = NULL;
    spec->server.write_mac_context = NULL;

    spec->write_seq_num = 0;
    spec->read_seq_num = 0;
    spec->epoch = 0;

    spec->refCt = 128; /* Arbitrarily high number to prevent
                        * non-TLS 1.3 cipherSpecs from being
                        * GCed. This will be overwritten with
                        * a valid refCt for TLS 1.3. */
    dtls_InitRecvdRecords(&spec->recvdRecords);
}

/* Called from: ssl3_SendRecord
**      ssl3_SendClientHello()
**      ssl3_HandleV2ClientHello()
**      ssl3_HandleRecord()
**
** This function should perhaps acquire and release the SpecWriteLock.
**
**
*/
SECStatus
ssl3_InitState(sslSocket *ss)
{
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->ssl3.initialized)
        return SECSuccess; /* Function should be idempotent */

    ss->ssl3.policy = SSL_ALLOWED;

    ssl_InitSecState(&ss->sec);

    ssl_GetSpecWriteLock(ss);
    ss->ssl3.crSpec = ss->ssl3.cwSpec = &ss->ssl3.specs[0];
    ss->ssl3.prSpec = ss->ssl3.pwSpec = &ss->ssl3.specs[1];
    ssl3_InitCipherSpec(ss->ssl3.crSpec);
    ssl3_InitCipherSpec(ss->ssl3.prSpec);
    ss->ssl3.crSpec->version = ss->ssl3.prSpec->version = ss->vrange.max;
    ssl_ReleaseSpecWriteLock(ss);

    ss->ssl3.hs.sendingSCSV = PR_FALSE;
    ss->ssl3.hs.preliminaryInfo = 0;
    ss->ssl3.hs.ws = (ss->sec.isServer) ? wait_client_hello : wait_server_hello;

    ssl3_ResetExtensionData(&ss->xtnData);
    PR_INIT_CLIST(&ss->ssl3.hs.remoteExtensions);
    if (IS_DTLS(ss)) {
        ss->ssl3.hs.sendMessageSeq = 0;
        ss->ssl3.hs.recvMessageSeq = 0;
        ss->ssl3.hs.rtTimeoutMs = DTLS_RETRANSMIT_INITIAL_MS;
        ss->ssl3.hs.rtRetries = 0;
        ss->ssl3.hs.recvdHighWater = -1;
        PR_INIT_CLIST(&ss->ssl3.hs.lastMessageFlight);
        dtls_SetMTU(ss, 0); /* Set the MTU to the highest plateau */
    }

    ss->ssl3.hs.currentSecret = NULL;
    ss->ssl3.hs.resumptionMasterSecret = NULL;
    ss->ssl3.hs.dheSecret = NULL;
    ss->ssl3.hs.pskBinderKey = NULL;
    ss->ssl3.hs.clientEarlyTrafficSecret = NULL;
    ss->ssl3.hs.clientHsTrafficSecret = NULL;
    ss->ssl3.hs.serverHsTrafficSecret = NULL;
    ss->ssl3.hs.clientTrafficSecret = NULL;
    ss->ssl3.hs.serverTrafficSecret = NULL;
    ss->ssl3.hs.certificateRequest = NULL;
    PR_INIT_CLIST(&ss->ssl3.hs.cipherSpecs);

    PORT_Assert(!ss->ssl3.hs.messages.buf && !ss->ssl3.hs.messages.space);
    ss->ssl3.hs.messages.buf = NULL;
    ss->ssl3.hs.messages.space = 0;

    ss->ssl3.hs.receivedNewSessionTicket = PR_FALSE;
    PORT_Memset(&ss->ssl3.hs.newSessionTicket, 0,
                sizeof(ss->ssl3.hs.newSessionTicket));

    ss->ssl3.hs.zeroRttState = ssl_0rtt_none;

    ssl_FilterSupportedGroups(ss);

    ss->ssl3.initialized = PR_TRUE;
    return SECSuccess;
}

/* record the export policy for this cipher suite */
SECStatus
ssl3_SetPolicy(ssl3CipherSuite which, int policy)
{
    ssl3CipherSuiteCfg *suite;

    suite = ssl_LookupCipherSuiteCfgMutable(which, cipherSuites);
    if (suite == NULL) {
        return SECFailure; /* err code was set by ssl_LookupCipherSuiteCfg */
    }
    suite->policy = policy;

    return SECSuccess;
}

SECStatus
ssl3_GetPolicy(ssl3CipherSuite which, PRInt32 *oPolicy)
{
    const ssl3CipherSuiteCfg *suite;
    PRInt32 policy;
    SECStatus rv;

    suite = ssl_LookupCipherSuiteCfg(which, cipherSuites);
    if (suite) {
        policy = suite->policy;
        rv = SECSuccess;
    } else {
        policy = SSL_NOT_ALLOWED;
        rv = SECFailure; /* err code was set by Lookup. */
    }
    *oPolicy = policy;
    return rv;
}

/* record the user preference for this suite */
SECStatus
ssl3_CipherPrefSetDefault(ssl3CipherSuite which, PRBool enabled)
{
    ssl3CipherSuiteCfg *suite;

    suite = ssl_LookupCipherSuiteCfgMutable(which, cipherSuites);
    if (suite == NULL) {
        return SECFailure; /* err code was set by ssl_LookupCipherSuiteCfg */
    }
    suite->enabled = enabled;
    return SECSuccess;
}

/* return the user preference for this suite */
SECStatus
ssl3_CipherPrefGetDefault(ssl3CipherSuite which, PRBool *enabled)
{
    const ssl3CipherSuiteCfg *suite;
    PRBool pref;
    SECStatus rv;

    suite = ssl_LookupCipherSuiteCfg(which, cipherSuites);
    if (suite) {
        pref = suite->enabled;
        rv = SECSuccess;
    } else {
        pref = SSL_NOT_ALLOWED;
        rv = SECFailure; /* err code was set by Lookup. */
    }
    *enabled = pref;
    return rv;
}

SECStatus
ssl3_CipherPrefSet(sslSocket *ss, ssl3CipherSuite which, PRBool enabled)
{
    ssl3CipherSuiteCfg *suite;

    suite = ssl_LookupCipherSuiteCfgMutable(which, ss->cipherSuites);
    if (suite == NULL) {
        return SECFailure; /* err code was set by ssl_LookupCipherSuiteCfg */
    }
    suite->enabled = enabled;
    return SECSuccess;
}

SECStatus
ssl3_CipherPrefGet(const sslSocket *ss, ssl3CipherSuite which, PRBool *enabled)
{
    const ssl3CipherSuiteCfg *suite;
    PRBool pref;
    SECStatus rv;

    suite = ssl_LookupCipherSuiteCfg(which, ss->cipherSuites);
    if (suite) {
        pref = suite->enabled;
        rv = SECSuccess;
    } else {
        pref = SSL_NOT_ALLOWED;
        rv = SECFailure; /* err code was set by Lookup. */
    }
    *enabled = pref;
    return rv;
}

SECStatus
SSL_SignatureSchemePrefSet(PRFileDesc *fd, const SSLSignatureScheme *schemes,
                           unsigned int count)
{
    sslSocket *ss;
    unsigned int i;
    unsigned int supported = 0;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SignatureSchemePrefSet",
                 SSL_GETPID(), fd));
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!count) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    for (i = 0; i < count; ++i) {
        if (ssl_IsSupportedSignatureScheme(schemes[i])) {
            ++supported;
        }
    }
    /* We don't check for duplicates, so it's possible to get too many. */
    if (supported > MAX_SIGNATURE_SCHEMES) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    ss->ssl3.signatureSchemeCount = 0;
    for (i = 0; i < count; ++i) {
        if (!ssl_IsSupportedSignatureScheme(schemes[i])) {
            SSL_DBG(("%d: SSL[%d]: invalid signature scheme %d ignored",
                     SSL_GETPID(), fd, schemes[i]));
            continue;
        }

        ss->ssl3.signatureSchemes[ss->ssl3.signatureSchemeCount++] = schemes[i];
    }

    if (ss->ssl3.signatureSchemeCount == 0) {
        PORT_SetError(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus
SSL_SignaturePrefSet(PRFileDesc *fd, const SSLSignatureAndHashAlg *algorithms,
                     unsigned int count)
{
    SSLSignatureScheme schemes[MAX_SIGNATURE_SCHEMES];
    unsigned int i;

    count = PR_MIN(PR_ARRAY_SIZE(schemes), count);
    for (i = 0; i < count; ++i) {
        schemes[i] = (algorithms[i].hashAlg << 8) | algorithms[i].sigAlg;
    }
    return SSL_SignatureSchemePrefSet(fd, schemes, count);
}

SECStatus
SSL_SignatureSchemePrefGet(PRFileDesc *fd, SSLSignatureScheme *schemes,
                           unsigned int *count, unsigned int maxCount)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SignatureSchemePrefGet",
                 SSL_GETPID(), fd));
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!schemes || !count ||
        maxCount < ss->ssl3.signatureSchemeCount) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    PORT_Memcpy(schemes, ss->ssl3.signatureSchemes,
                ss->ssl3.signatureSchemeCount * sizeof(SSLSignatureScheme));
    *count = ss->ssl3.signatureSchemeCount;
    return SECSuccess;
}

SECStatus
SSL_SignaturePrefGet(PRFileDesc *fd, SSLSignatureAndHashAlg *algorithms,
                     unsigned int *count, unsigned int maxCount)
{
    sslSocket *ss;
    unsigned int i;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SignaturePrefGet",
                 SSL_GETPID(), fd));
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!algorithms || !count ||
        maxCount < ss->ssl3.signatureSchemeCount) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    for (i = 0; i < ss->ssl3.signatureSchemeCount; ++i) {
        algorithms[i].hashAlg = (ss->ssl3.signatureSchemes[i] >> 8) & 0xff;
        algorithms[i].sigAlg = ss->ssl3.signatureSchemes[i] & 0xff;
    }
    *count = ss->ssl3.signatureSchemeCount;
    return SECSuccess;
}

unsigned int
SSL_SignatureMaxCount()
{
    return MAX_SIGNATURE_SCHEMES;
}

/* copy global default policy into socket. */
void
ssl3_InitSocketPolicy(sslSocket *ss)
{
    PORT_Memcpy(ss->cipherSuites, cipherSuites, sizeof(cipherSuites));
    PORT_Memcpy(ss->ssl3.signatureSchemes, defaultSignatureSchemes,
                sizeof(defaultSignatureSchemes));
    ss->ssl3.signatureSchemeCount = PR_ARRAY_SIZE(defaultSignatureSchemes);
}

/*
** If ssl3 socket has completed the first handshake, and is in idle state,
** then start a new handshake.
** If flushCache is true, the SID cache will be flushed first, forcing a
** "Full" handshake (not a session restart handshake), to be done.
**
** called from SSL_RedoHandshake(), which already holds the handshake locks.
*/
SECStatus
ssl3_RedoHandshake(sslSocket *ss, PRBool flushCache)
{
    sslSessionID *sid = ss->sec.ci.sid;
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (!ss->firstHsDone ||
        (ss->ssl3.initialized && (ss->ssl3.hs.ws != idle_handshake))) {
        PORT_SetError(SSL_ERROR_HANDSHAKE_NOT_COMPLETED);
        return SECFailure;
    }

    if (IS_DTLS(ss)) {
        dtls_RehandshakeCleanup(ss);
    }

    if (ss->opt.enableRenegotiation == SSL_RENEGOTIATE_NEVER ||
        ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED);
        return SECFailure;
    }
    if (sid && flushCache) {
        ss->sec.uncache(sid); /* remove it from whichever cache it's in. */
        ssl_FreeSID(sid);     /* dec ref count and free if zero. */
        ss->sec.ci.sid = NULL;
    }

    ssl_GetXmitBufLock(ss); /**************************************/

    /* start off a new handshake. */
    if (ss->sec.isServer) {
        rv = ssl3_SendHelloRequest(ss);
    } else {
        rv = ssl3_SendClientHello(ss, client_hello_renegotiation);
    }

    ssl_ReleaseXmitBufLock(ss); /**************************************/
    return rv;
}

/* Called from ssl_DestroySocketContents() in sslsock.c */
void
ssl3_DestroySSL3Info(sslSocket *ss)
{

    if (ss->ssl3.clientCertificate != NULL)
        CERT_DestroyCertificate(ss->ssl3.clientCertificate);

    if (ss->ssl3.clientPrivateKey != NULL)
        SECKEY_DestroyPrivateKey(ss->ssl3.clientPrivateKey);

    if (ss->ssl3.peerCertArena != NULL)
        ssl3_CleanupPeerCerts(ss);

    if (ss->ssl3.clientCertChain != NULL) {
        CERT_DestroyCertificateList(ss->ssl3.clientCertChain);
        ss->ssl3.clientCertChain = NULL;
    }
    if (ss->ssl3.ca_list) {
        CERT_FreeDistNames(ss->ssl3.ca_list);
    }

    /* clean up handshake */
    if (ss->ssl3.hs.md5) {
        PK11_DestroyContext(ss->ssl3.hs.md5, PR_TRUE);
    }
    if (ss->ssl3.hs.sha) {
        PK11_DestroyContext(ss->ssl3.hs.sha, PR_TRUE);
    }
    if (ss->ssl3.hs.messages.buf) {
        sslBuffer_Clear(&ss->ssl3.hs.messages);
    }

    /* free the SSL3Buffer (msg_body) */
    PORT_Free(ss->ssl3.hs.msg_body.buf);

    SECITEM_FreeItem(&ss->ssl3.hs.newSessionTicket.ticket, PR_FALSE);
    SECITEM_FreeItem(&ss->ssl3.hs.srvVirtName, PR_FALSE);

    if (ss->ssl3.hs.certificateRequest) {
        PORT_FreeArena(ss->ssl3.hs.certificateRequest->arena, PR_FALSE);
        ss->ssl3.hs.certificateRequest = NULL;
    }

    /* free up the CipherSpecs */
    ssl3_DestroyCipherSpec(&ss->ssl3.specs[0], PR_TRUE /*freeSrvName*/);
    ssl3_DestroyCipherSpec(&ss->ssl3.specs[1], PR_TRUE /*freeSrvName*/);

    /* Destroy the DTLS data */
    if (IS_DTLS(ss)) {
        dtls_FreeHandshakeMessages(&ss->ssl3.hs.lastMessageFlight);
        if (ss->ssl3.hs.recvdFragments.buf) {
            PORT_Free(ss->ssl3.hs.recvdFragments.buf);
        }
    }

    /* Destroy remote extensions */
    ssl3_DestroyRemoteExtensions(&ss->ssl3.hs.remoteExtensions);
    ssl3_ResetExtensionData(&ss->xtnData);

    /* Destroy TLS 1.3 cipher specs */
    tls13_DestroyCipherSpecs(&ss->ssl3.hs.cipherSpecs);

    /* Destroy TLS 1.3 keys */
    if (ss->ssl3.hs.currentSecret)
        PK11_FreeSymKey(ss->ssl3.hs.currentSecret);
    if (ss->ssl3.hs.resumptionMasterSecret)
        PK11_FreeSymKey(ss->ssl3.hs.resumptionMasterSecret);
    if (ss->ssl3.hs.dheSecret)
        PK11_FreeSymKey(ss->ssl3.hs.dheSecret);
    if (ss->ssl3.hs.pskBinderKey)
        PK11_FreeSymKey(ss->ssl3.hs.pskBinderKey);
    if (ss->ssl3.hs.clientEarlyTrafficSecret)
        PK11_FreeSymKey(ss->ssl3.hs.clientEarlyTrafficSecret);
    if (ss->ssl3.hs.clientHsTrafficSecret)
        PK11_FreeSymKey(ss->ssl3.hs.clientHsTrafficSecret);
    if (ss->ssl3.hs.serverHsTrafficSecret)
        PK11_FreeSymKey(ss->ssl3.hs.serverHsTrafficSecret);
    if (ss->ssl3.hs.clientTrafficSecret)
        PK11_FreeSymKey(ss->ssl3.hs.clientTrafficSecret);
    if (ss->ssl3.hs.serverTrafficSecret)
        PK11_FreeSymKey(ss->ssl3.hs.serverTrafficSecret);
    if (ss->ssl3.hs.earlyExporterSecret)
        PK11_FreeSymKey(ss->ssl3.hs.earlyExporterSecret);
    if (ss->ssl3.hs.exporterSecret)
        PK11_FreeSymKey(ss->ssl3.hs.exporterSecret);

    ss->ssl3.hs.zeroRttState = ssl_0rtt_none;
    /* Destroy TLS 1.3 buffered early data. */
    tls13_DestroyEarlyData(&ss->ssl3.hs.bufferedEarlyData);

    ss->ssl3.initialized = PR_FALSE;

    SECITEM_FreeItem(&ss->xtnData.nextProto, PR_FALSE);
}

#define MAP_NULL(x) (((x) != 0) ? (x) : SEC_OID_NULL_CIPHER)

SECStatus
ssl3_ApplyNSSPolicy(void)
{
    unsigned i;
    SECStatus rv;
    PRUint32 policy = 0;

    rv = NSS_GetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY, &policy);
    if (rv != SECSuccess || !(policy & NSS_USE_POLICY_IN_SSL)) {
        return SECSuccess; /* do nothing */
    }

    /* disable every ciphersuite */
    for (i = 1; i < PR_ARRAY_SIZE(cipher_suite_defs); ++i) {
        const ssl3CipherSuiteDef *suite = &cipher_suite_defs[i];
        SECOidTag policyOid;

        policyOid = MAP_NULL(kea_defs[suite->key_exchange_alg].oid);
        rv = NSS_GetAlgorithmPolicy(policyOid, &policy);
        if (rv == SECSuccess && !(policy & NSS_USE_ALG_IN_SSL_KX)) {
            ssl_CipherPrefSetDefault(suite->cipher_suite, PR_FALSE);
            ssl_CipherPolicySet(suite->cipher_suite, SSL_NOT_ALLOWED);
            continue;
        }

        policyOid = MAP_NULL(ssl_GetBulkCipherDef(suite)->oid);
        rv = NSS_GetAlgorithmPolicy(policyOid, &policy);
        if (rv == SECSuccess && !(policy & NSS_USE_ALG_IN_SSL)) {
            ssl_CipherPrefSetDefault(suite->cipher_suite, PR_FALSE);
            ssl_CipherPolicySet(suite->cipher_suite, SSL_NOT_ALLOWED);
            continue;
        }

        if (ssl_GetBulkCipherDef(suite)->type != type_aead) {
            policyOid = MAP_NULL(mac_defs[suite->mac_alg].oid);
            rv = NSS_GetAlgorithmPolicy(policyOid, &policy);
            if (rv == SECSuccess && !(policy & NSS_USE_ALG_IN_SSL)) {
                ssl_CipherPrefSetDefault(suite->cipher_suite, PR_FALSE);
                ssl_CipherPolicySet(suite->cipher_suite,
                                    SSL_NOT_ALLOWED);
                continue;
            }
        }
    }

    rv = ssl3_ConstrainRangeByPolicy();

    return rv;
}

/* End of ssl3con.c */
