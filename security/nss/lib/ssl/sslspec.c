/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Handling of cipher specs.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslproto.h"
#include "pk11func.h"
#include "secitem.h"

#include "sslimpl.h"

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
static const ssl3BulkCipherDef ssl_bulk_cipher_defs[] = {
    /*                                        |--------- Lengths ---------| */
    /* cipher             calg                :  s                        : */
    /*                                        :  e                  b     n */
    /* oid                short_name mr       :  c                  l     o */
    /*                                        k  r                  o  t  n */
    /*                                        e  e               i  c  a  c */
    /*                                        y  t  type         v  k  g  e */
    {cipher_null,         ssl_calg_null,      0, 0, type_stream, 0, 0, 0, 0,
     SEC_OID_NULL_CIPHER, "NULL", MR_MAX},
    {cipher_rc4,          ssl_calg_rc4,      16,16, type_stream, 0, 0, 0, 0,
     SEC_OID_RC4,         "RC4", MR_LOW},
    {cipher_des,          ssl_calg_des,       8, 8, type_block,  8, 8, 0, 0,
     SEC_OID_DES_CBC,     "DES-CBC", MR_LOW},
    {cipher_3des,         ssl_calg_3des,     24,24, type_block,  8, 8, 0, 0,
     SEC_OID_DES_EDE3_CBC, "3DES-EDE-CBC", MR_LOW},
    {cipher_aes_128,      ssl_calg_aes,      16,16, type_block, 16,16, 0, 0,
     SEC_OID_AES_128_CBC, "AES-128", MR_128},
    {cipher_aes_256,      ssl_calg_aes,      32,32, type_block, 16,16, 0, 0,
     SEC_OID_AES_256_CBC, "AES-256", MR_128},
    {cipher_camellia_128, ssl_calg_camellia, 16,16, type_block, 16,16, 0, 0,
     SEC_OID_CAMELLIA_128_CBC, "Camellia-128", MR_128},
    {cipher_camellia_256, ssl_calg_camellia, 32,32, type_block, 16,16, 0, 0,
     SEC_OID_CAMELLIA_256_CBC, "Camellia-256", MR_128},
    {cipher_seed,         ssl_calg_seed,     16,16, type_block, 16,16, 0, 0,
     SEC_OID_SEED_CBC,    "SEED-CBC", MR_128},
    {cipher_aes_128_gcm,  ssl_calg_aes_gcm,  16,16, type_aead,   4, 0,16, 8,
     SEC_OID_AES_128_GCM, "AES-128-GCM", MR_128},
    {cipher_aes_256_gcm,  ssl_calg_aes_gcm,  32,32, type_aead,   4, 0,16, 8,
     SEC_OID_AES_256_GCM, "AES-256-GCM", MR_128},
    {cipher_chacha20,     ssl_calg_chacha20, 32,32, type_aead,  12, 0,16, 0,
     SEC_OID_CHACHA20_POLY1305, "ChaCha20-Poly1305", MR_MAX},
    {cipher_missing,      ssl_calg_null,      0, 0, type_stream, 0, 0, 0, 0,
     SEC_OID_UNKNOWN,     "missing", 0U},
};
/* clang-format on */

const ssl3BulkCipherDef *
ssl_GetBulkCipherDef(const ssl3CipherSuiteDef *suiteDef)
{
    SSL3BulkCipher bulkCipher = suiteDef->bulk_cipher_alg;
    PORT_Assert(bulkCipher < PR_ARRAY_SIZE(ssl_bulk_cipher_defs));
    PORT_Assert(ssl_bulk_cipher_defs[bulkCipher].cipher == bulkCipher);
    return &ssl_bulk_cipher_defs[bulkCipher];
}

/* indexed by SSL3MACAlgorithm */
static const ssl3MACDef ssl_mac_defs[] = {
    /* pad_size is only used for SSL 3.0 MAC. See RFC 6101 Sec. 5.2.3.1. */
    /* mac      mmech       pad_size  mac_size                       */
    { ssl_mac_null, CKM_INVALID_MECHANISM, 0, 0, 0 },
    { ssl_mac_md5, CKM_SSL3_MD5_MAC, 48, MD5_LENGTH, SEC_OID_HMAC_MD5 },
    { ssl_mac_sha, CKM_SSL3_SHA1_MAC, 40, SHA1_LENGTH, SEC_OID_HMAC_SHA1 },
    { ssl_hmac_md5, CKM_MD5_HMAC, 0, MD5_LENGTH, SEC_OID_HMAC_MD5 },
    { ssl_hmac_sha, CKM_SHA_1_HMAC, 0, SHA1_LENGTH, SEC_OID_HMAC_SHA1 },
    { ssl_hmac_sha256, CKM_SHA256_HMAC, 0, SHA256_LENGTH, SEC_OID_HMAC_SHA256 },
    { ssl_mac_aead, CKM_INVALID_MECHANISM, 0, 0, 0 },
    { ssl_hmac_sha384, CKM_SHA384_HMAC, 0, SHA384_LENGTH, SEC_OID_HMAC_SHA384 }
};

const ssl3MACDef *
ssl_GetMacDefByAlg(SSL3MACAlgorithm mac)
{
    /* Cast here for clang: https://bugs.llvm.org/show_bug.cgi?id=16154 */
    PORT_Assert((size_t)mac < PR_ARRAY_SIZE(ssl_mac_defs));
    PORT_Assert(ssl_mac_defs[mac].mac == mac);
    return &ssl_mac_defs[mac];
}

const ssl3MACDef *
ssl_GetMacDef(const sslSocket *ss, const ssl3CipherSuiteDef *suiteDef)
{
    SSL3MACAlgorithm mac = suiteDef->mac_alg;
    if (ss->version > SSL_LIBRARY_VERSION_3_0) {
        switch (mac) {
            case ssl_mac_md5:
                mac = ssl_hmac_md5;
                break;
            case ssl_mac_sha:
                mac = ssl_hmac_sha;
                break;
            default:
                break;
        }
    }
    return ssl_GetMacDefByAlg(mac);
}

ssl3CipherSpec *
ssl_FindCipherSpecByEpoch(sslSocket *ss, CipherSpecDirection direction,
                          DTLSEpoch epoch)
{
    PRCList *cur_p;
    for (cur_p = PR_LIST_HEAD(&ss->ssl3.hs.cipherSpecs);
         cur_p != &ss->ssl3.hs.cipherSpecs;
         cur_p = PR_NEXT_LINK(cur_p)) {
        ssl3CipherSpec *spec = (ssl3CipherSpec *)cur_p;

        if (spec->epoch != epoch) {
            continue;
        }
        if (direction != spec->direction) {
            continue;
        }
        return spec;
    }
    return NULL;
}

ssl3CipherSpec *
ssl_CreateCipherSpec(sslSocket *ss, CipherSpecDirection direction)
{
    ssl3CipherSpec *spec = PORT_ZNew(ssl3CipherSpec);
    if (!spec) {
        return NULL;
    }
    spec->refCt = 1;
    spec->version = ss->version;
    spec->direction = direction;
    SSL_TRC(10, ("%d: SSL[%d]: new %s spec %d ct=%d",
                 SSL_GETPID(), ss->fd, SPEC_DIR(spec), spec,
                 spec->refCt));
    return spec;
}

void
ssl_SaveCipherSpec(sslSocket *ss, ssl3CipherSpec *spec)
{
    PR_APPEND_LINK(&spec->link, &ss->ssl3.hs.cipherSpecs);
}

/* Called from ssl3_InitState. */
/* Caller must hold the SpecWriteLock. */
SECStatus
ssl_SetupNullCipherSpec(sslSocket *ss, CipherSpecDirection dir)
{
    ssl3CipherSpec *spec;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSpecWriteLock(ss));

    spec = ssl_CreateCipherSpec(ss, dir);
    if (!spec) {
        return SECFailure;
    }

    /* Set default versions.  This value will be used to generate and send
     * alerts if a version is not negotiated.  These values are overridden when
     * sending a ClientHello and when a version is negotiated. */
    spec->version = SSL_LIBRARY_VERSION_TLS_1_0;
    spec->recordVersion = IS_DTLS(ss)
                              ? SSL_LIBRARY_VERSION_DTLS_1_0_WIRE
                              : SSL_LIBRARY_VERSION_TLS_1_0;
    spec->cipherDef = &ssl_bulk_cipher_defs[cipher_null];
    PORT_Assert(spec->cipherDef->cipher == cipher_null);
    spec->macDef = &ssl_mac_defs[ssl_mac_null];
    PORT_Assert(spec->macDef->mac == ssl_mac_null);
    spec->cipher = Null_Cipher;

    spec->phase = "cleartext";
    dtls_InitRecvdRecords(&spec->recvdRecords);

    ssl_SaveCipherSpec(ss, spec);
    if (dir == CipherSpecRead) {
        ss->ssl3.crSpec = spec;
    } else {
        ss->ssl3.cwSpec = spec;
    }
    return SECSuccess;
}

void
ssl_CipherSpecAddRef(ssl3CipherSpec *spec)
{
    ++spec->refCt;
    SSL_TRC(10, ("%d: SSL[-]: Increment ref ct for %s spec %d. new ct = %d",
                 SSL_GETPID(), SPEC_DIR(spec), spec, spec->refCt));
}

static void
ssl_DestroyKeyMaterial(ssl3KeyMaterial *keyMaterial)
{
    PK11_FreeSymKey(keyMaterial->key);
    PK11_FreeSymKey(keyMaterial->macKey);
    if (keyMaterial->macContext != NULL) {
        PK11_DestroyContext(keyMaterial->macContext, PR_TRUE);
    }
}

static void
ssl_FreeCipherSpec(ssl3CipherSpec *spec)
{
    SSL_TRC(10, ("%d: SSL[-]: Freeing %s spec %d. epoch=%d",
                 SSL_GETPID(), SPEC_DIR(spec), spec, spec->epoch));

    PR_REMOVE_LINK(&spec->link);

    /*  PORT_Assert( ss->opt.noLocks || ssl_HaveSpecWriteLock(ss)); Don't have ss! */
    if (spec->cipherContext) {
        PK11_DestroyContext(spec->cipherContext, PR_TRUE);
    }
    PK11_FreeSymKey(spec->masterSecret);
    ssl_DestroyKeyMaterial(&spec->keyMaterial);

    PORT_ZFree(spec, sizeof(*spec));
}

/* This function is never called on a spec which is on the
 * cipherSpecs list. */
void
ssl_CipherSpecRelease(ssl3CipherSpec *spec)
{
    if (!spec) {
        return;
    }

    PORT_Assert(spec->refCt > 0);
    --spec->refCt;
    SSL_TRC(10, ("%d: SSL[-]: decrement refct for %s spec %d. epoch=%d new ct = %d",
                 SSL_GETPID(), SPEC_DIR(spec), spec, spec->epoch, spec->refCt));
    if (!spec->refCt) {
        ssl_FreeCipherSpec(spec);
    }
}

void
ssl_DestroyCipherSpecs(PRCList *list)
{
    while (!PR_CLIST_IS_EMPTY(list)) {
        ssl3CipherSpec *spec = (ssl3CipherSpec *)PR_LIST_TAIL(list);
        ssl_FreeCipherSpec(spec);
    }
}

void
ssl_CipherSpecReleaseByEpoch(sslSocket *ss, CipherSpecDirection dir,
                             DTLSEpoch epoch)
{
    ssl3CipherSpec *spec;
    SSL_TRC(10, ("%d: SSL[%d]: releasing %s cipher spec for epoch %d",
                 SSL_GETPID(), ss->fd,
                 (dir == CipherSpecRead) ? "read" : "write", epoch));

    spec = ssl_FindCipherSpecByEpoch(ss, dir, epoch);
    if (spec) {
        ssl_CipherSpecRelease(spec);
    }
}
