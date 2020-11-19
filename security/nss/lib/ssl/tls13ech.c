/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "pk11func.h"
#include "pk11hpke.h"
#include "ssl.h"
#include "sslproto.h"
#include "sslimpl.h"
#include "selfencrypt.h"
#include "ssl3exthandle.h"
#include "tls13ech.h"
#include "tls13exthandle.h"
#include "tls13hkdf.h"

extern SECStatus
ssl3_UpdateExplicitHandshakeTranscript(sslSocket *ss, const unsigned char *b,
                                       unsigned int l, sslBuffer *transcriptBuf);
extern SECStatus
ssl3_HandleClientHelloPreamble(sslSocket *ss, PRUint8 **b, PRUint32 *length, SECItem *sidBytes,
                               SECItem *cookieBytes, SECItem *suites, SECItem *comps);

void
tls13_DestroyEchConfig(sslEchConfig *config)
{
    if (!config) {
        return;
    }
    SECITEM_FreeItem(&config->contents.publicKey, PR_FALSE);
    SECITEM_FreeItem(&config->contents.suites, PR_FALSE);
    SECITEM_FreeItem(&config->raw, PR_FALSE);
    PORT_Free(config->contents.publicName);
    config->contents.publicName = NULL;
    PORT_ZFree(config, sizeof(*config));
}

void
tls13_DestroyEchConfigs(PRCList *list)
{
    PRCList *cur_p;
    while (!PR_CLIST_IS_EMPTY(list)) {
        cur_p = PR_LIST_TAIL(list);
        PR_REMOVE_LINK(cur_p);
        tls13_DestroyEchConfig((sslEchConfig *)cur_p);
    }
}

SECStatus
tls13_CopyEchConfigs(PRCList *oConfigs, PRCList *configs)
{
    SECStatus rv;
    sslEchConfig *config;
    sslEchConfig *newConfig = NULL;

    for (PRCList *cur_p = PR_LIST_HEAD(oConfigs);
         cur_p != oConfigs;
         cur_p = PR_NEXT_LINK(cur_p)) {
        config = (sslEchConfig *)PR_LIST_TAIL(oConfigs);
        newConfig = PORT_ZNew(sslEchConfig);
        if (!newConfig) {
            goto loser;
        }

        rv = SECITEM_CopyItem(NULL, &newConfig->raw, &config->raw);
        if (rv != SECSuccess) {
            goto loser;
        }
        newConfig->contents.publicName = PORT_Strdup(config->contents.publicName);
        if (!newConfig->contents.publicName) {
            goto loser;
        }
        rv = SECITEM_CopyItem(NULL, &newConfig->contents.publicKey,
                              &config->contents.publicKey);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = SECITEM_CopyItem(NULL, &newConfig->contents.suites,
                              &config->contents.suites);
        if (rv != SECSuccess) {
            goto loser;
        }
        newConfig->contents.kemId = config->contents.kemId;
        newConfig->contents.kdfId = config->contents.kdfId;
        newConfig->contents.aeadId = config->contents.aeadId;
        newConfig->contents.maxNameLen = config->contents.maxNameLen;
        PORT_Memcpy(newConfig->configId, config->configId, sizeof(newConfig->configId));
        PR_APPEND_LINK(&newConfig->link, configs);
    }
    return SECSuccess;

loser:
    tls13_DestroyEchConfig(newConfig);
    tls13_DestroyEchConfigs(configs);
    return SECFailure;
}

static SECStatus
tls13_DigestEchConfig(const sslEchConfig *cfg, PRUint8 *digest, size_t maxDigestLen)
{
    SECStatus rv;
    PK11SymKey *configKey = NULL;
    PK11SymKey *derived = NULL;
    SECItem *derivedItem = NULL;
    CK_HKDF_PARAMS params = { 0 };
    SECItem paramsi = { siBuffer, (unsigned char *)&params, sizeof(params) };
    PK11SlotInfo *slot = PK11_GetInternalSlot();

    if (!slot) {
        goto loser;
    }

    configKey = PK11_ImportDataKey(slot, CKM_HKDF_DATA, PK11_OriginUnwrap,
                                   CKA_DERIVE, CONST_CAST(SECItem, &cfg->raw), NULL);
    if (!configKey) {
        goto loser;
    }

    /* We only support SHA256 KDF. */
    PORT_Assert(cfg->contents.kdfId == HpkeKdfHkdfSha256);
    params.bExtract = CK_TRUE;
    params.bExpand = CK_TRUE;
    params.prfHashMechanism = CKM_SHA256;
    params.ulSaltType = CKF_HKDF_SALT_NULL;
    params.pInfo = CONST_CAST(CK_BYTE, hHkdfInfoEchConfigID);
    params.ulInfoLen = strlen(hHkdfInfoEchConfigID);
    derived = PK11_DeriveWithFlags(configKey, CKM_HKDF_DATA,
                                   &paramsi, CKM_HKDF_DERIVE, CKA_DERIVE, 32,
                                   CKF_SIGN | CKF_VERIFY);

    rv = PK11_ExtractKeyValue(derived);
    if (rv != SECSuccess) {
        goto loser;
    }

    derivedItem = PK11_GetKeyData(derived);
    if (!derivedItem) {
        goto loser;
    }

    if (derivedItem->len != maxDigestLen) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        goto loser;
    }

    PORT_Memcpy(digest, derivedItem->data, derivedItem->len);
    PK11_FreeSymKey(configKey);
    PK11_FreeSymKey(derived);
    PK11_FreeSlot(slot);
    return SECSuccess;

loser:
    PK11_FreeSymKey(configKey);
    PK11_FreeSymKey(derived);
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return SECFailure;
}

static SECStatus
tls13_DecodeEchConfigContents(const sslReadBuffer *rawConfig,
                              sslEchConfig **outConfig)
{
    SECStatus rv;
    sslEchConfigContents contents = { 0 };
    sslEchConfig *decodedConfig;
    PRUint64 tmpn;
    PRUint64 tmpn2;
    sslReadBuffer tmpBuf;
    PRUint16 *extensionTypes = NULL;
    unsigned int extensionIndex = 0;
    sslReader configReader = SSL_READER(rawConfig->buf, rawConfig->len);
    sslReader suiteReader;
    sslReader extensionReader;
    PRBool hasValidSuite = PR_FALSE;

    /* Parse the public_name. */
    rv = sslRead_ReadVariable(&configReader, 2, &tmpBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* Make sure the public name doesn't contain any NULLs.
     * TODO: Just store the SECItem instead. */
    for (tmpn = 0; tmpn < tmpBuf.len; tmpn++) {
        if (tmpBuf.buf[tmpn] == '\0') {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
            goto loser;
        }
    }

    contents.publicName = PORT_ZAlloc(tmpBuf.len + 1);
    if (!contents.publicName) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
        goto loser;
    }
    PORT_Memcpy(contents.publicName, (PRUint8 *)tmpBuf.buf, tmpBuf.len);

    /* Public key. */
    rv = sslRead_ReadVariable(&configReader, 2, &tmpBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECITEM_MakeItem(NULL, &contents.publicKey, (PRUint8 *)tmpBuf.buf, tmpBuf.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslRead_ReadNumber(&configReader, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    contents.kemId = tmpn;

    /* Parse HPKE cipher suites. */
    rv = sslRead_ReadVariable(&configReader, 2, &tmpBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (tmpBuf.len & 1) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
        goto loser;
    }
    suiteReader = (sslReader)SSL_READER(tmpBuf.buf, tmpBuf.len);
    while (SSL_READER_REMAINING(&suiteReader)) {
        /* kdf_id */
        rv = sslRead_ReadNumber(&suiteReader, 2, &tmpn);
        if (rv != SECSuccess) {
            goto loser;
        }
        /* aead_id */
        rv = sslRead_ReadNumber(&suiteReader, 2, &tmpn2);
        if (rv != SECSuccess) {
            goto loser;
        }
        if (!hasValidSuite) {
            /* Use the first compatible ciphersuite. */
            rv = PK11_HPKE_ValidateParameters(contents.kemId, tmpn, tmpn2);
            if (rv == SECSuccess) {
                hasValidSuite = PR_TRUE;
                contents.kdfId = tmpn;
                contents.aeadId = tmpn2;
                break;
            }
        }
    }

    rv = SECITEM_MakeItem(NULL, &contents.suites, (PRUint8 *)tmpBuf.buf, tmpBuf.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Read the max name length. */
    rv = sslRead_ReadNumber(&configReader, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    contents.maxNameLen = (PRUint16)tmpn;

    /* Extensions. We don't support any, but must
     * check for any that are marked critical. */
    rv = sslRead_ReadVariable(&configReader, 2, &tmpBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    extensionReader = (sslReader)SSL_READER(tmpBuf.buf, tmpBuf.len);
    extensionTypes = PORT_NewArray(PRUint16, tmpBuf.len / 2 * sizeof(PRUint16));
    if (!extensionTypes) {
        goto loser;
    }

    while (SSL_READER_REMAINING(&extensionReader)) {
        /* Get the extension's type field */
        rv = sslRead_ReadNumber(&extensionReader, 2, &tmpn);
        if (rv != SECSuccess) {
            goto loser;
        }

        for (unsigned int i = 0; i < extensionIndex; i++) {
            if (extensionTypes[i] == tmpn) {
                PORT_SetError(SEC_ERROR_EXTENSION_VALUE_INVALID);
                goto loser;
            }
        }
        extensionTypes[extensionIndex++] = (PRUint16)tmpn;

        /* If it's mandatory, fail. */
        if (tmpn & (1 << 15)) {
            PORT_SetError(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
            goto loser;
        }

        /* Skip. */
        rv = sslRead_ReadVariable(&extensionReader, 2, &tmpBuf);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    /* Check that we consumed the entire ECHConfig */
    if (SSL_READER_REMAINING(&configReader)) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
        goto loser;
    }

    /* If the ciphersuites weren't compatible, don't
     * set the outparam. Return success to indicate
     * the config was well-formed. */
    if (hasValidSuite) {
        decodedConfig = PORT_ZNew(sslEchConfig);
        if (!decodedConfig) {
            goto loser;
        }
        decodedConfig->contents = contents;
        *outConfig = decodedConfig;
    } else {
        PORT_Free(contents.publicName);
        SECITEM_FreeItem(&contents.publicKey, PR_FALSE);
        SECITEM_FreeItem(&contents.suites, PR_FALSE);
    }
    PORT_Free(extensionTypes);
    return SECSuccess;

loser:
    PORT_Free(extensionTypes);
    PORT_Free(contents.publicName);
    SECITEM_FreeItem(&contents.publicKey, PR_FALSE);
    SECITEM_FreeItem(&contents.suites, PR_FALSE);
    return SECFailure;
}

/* Decode an ECHConfigs struct and store each ECHConfig
 * into |configs|.  */
SECStatus
tls13_DecodeEchConfigs(const SECItem *data, PRCList *configs)
{
    SECStatus rv;
    sslEchConfig *decodedConfig = NULL;
    sslReader rdr = SSL_READER(data->data, data->len);
    sslReadBuffer tmp;
    sslReadBuffer singleConfig;
    PRUint64 version;
    PRUint64 length;
    PORT_Assert(PR_CLIST_IS_EMPTY(configs));

    rv = sslRead_ReadVariable(&rdr, 2, &tmp);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (SSL_READER_REMAINING(&rdr)) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    sslReader configsReader = SSL_READER(tmp.buf, tmp.len);

    if (!SSL_READER_REMAINING(&configsReader)) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    /* Handle each ECHConfig. */
    while (SSL_READER_REMAINING(&configsReader)) {
        singleConfig.buf = SSL_READER_CURRENT(&configsReader);
        /* Version */
        rv = sslRead_ReadNumber(&configsReader, 2, &version);
        if (rv != SECSuccess) {
            goto loser;
        }
        /* Length */
        rv = sslRead_ReadNumber(&configsReader, 2, &length);
        if (rv != SECSuccess) {
            goto loser;
        }
        singleConfig.len = 4 + length;

        rv = sslRead_Read(&configsReader, length, &tmp);
        if (rv != SECSuccess) {
            goto loser;
        }

        if (version == TLS13_ECH_VERSION) {
            rv = tls13_DecodeEchConfigContents(&tmp, &decodedConfig);
            if (rv != SECSuccess) {
                goto loser; /* code set */
            }

            if (decodedConfig) {
                decodedConfig->version = version;
                rv = SECITEM_MakeItem(NULL, &decodedConfig->raw, singleConfig.buf,
                                      singleConfig.len);
                if (rv != SECSuccess) {
                    goto loser;
                }

                rv = tls13_DigestEchConfig(decodedConfig, decodedConfig->configId,
                                           sizeof(decodedConfig->configId));
                if (rv != SECSuccess) {
                    goto loser;
                }
                PR_APPEND_LINK(&decodedConfig->link, configs);
                decodedConfig = NULL;
            }
        }
    }
    return SECSuccess;

loser:
    tls13_DestroyEchConfigs(configs);
    return SECFailure;
}

/* Encode an ECHConfigs structure. We only allow one config, and as the
 * primary use for this function is to generate test inputs, we don't
 * validate against what HPKE and libssl can actually support. */
SECStatus
SSLExp_EncodeEchConfig(const char *publicName, const PRUint32 *hpkeSuites,
                       unsigned int hpkeSuiteCount, HpkeKemId kemId,
                       const SECKEYPublicKey *pubKey, PRUint16 maxNameLen,
                       PRUint8 *out, unsigned int *outlen, unsigned int maxlen)
{
    SECStatus rv;
    unsigned int savedOffset;
    unsigned int len;
    sslBuffer b = SSL_BUFFER_EMPTY;
    PRUint8 tmpBuf[66]; // Large enough for an EC public key, currently only X25519.
    unsigned int tmpLen;

    if (!publicName || PORT_Strlen(publicName) == 0 || !hpkeSuites ||
        hpkeSuiteCount == 0 || !pubKey || maxNameLen == 0 || !out || !outlen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    rv = sslBuffer_Skip(&b, 2, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(&b, TLS13_ECH_VERSION, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_Skip(&b, 2, &savedOffset);
    if (rv != SECSuccess) {
        goto loser;
    }

    len = PORT_Strlen(publicName);
    rv = sslBuffer_AppendVariable(&b, (const PRUint8 *)publicName, len, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = PK11_HPKE_Serialize(pubKey, tmpBuf, &tmpLen, sizeof(tmpBuf));
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendVariable(&b, tmpBuf, tmpLen, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(&b, kemId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(&b, hpkeSuiteCount * 4, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    for (unsigned int i = 0; i < hpkeSuiteCount; i++) {
        rv = sslBuffer_AppendNumber(&b, hpkeSuites[i], 4);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    rv = sslBuffer_AppendNumber(&b, maxNameLen, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* extensions */
    rv = sslBuffer_AppendNumber(&b, 0, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Write the length now that we know it. */
    rv = sslBuffer_InsertLength(&b, 0, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_InsertLength(&b, savedOffset, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (SSL_BUFFER_LEN(&b) > maxlen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    PORT_Memcpy(out, SSL_BUFFER_BASE(&b), SSL_BUFFER_LEN(&b));
    *outlen = SSL_BUFFER_LEN(&b);
    sslBuffer_Clear(&b);
    return SECSuccess;

loser:
    sslBuffer_Clear(&b);
    return SECFailure;
}

SECStatus
SSLExp_GetEchRetryConfigs(PRFileDesc *fd, SECItem *retryConfigs)
{
    SECStatus rv;
    sslSocket *ss;
    SECItem out = { siBuffer, NULL, 0 };

    if (!fd || !retryConfigs) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in %s",
                 SSL_GETPID(), fd, __FUNCTION__));
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (!ss->xtnData.echRetryConfigsValid) {
        PORT_SetError(SSL_ERROR_HANDSHAKE_NOT_COMPLETED);
        return SECFailure;
    }
    /* May be empty. */
    rv = SECITEM_CopyItem(NULL, &out, &ss->xtnData.echRetryConfigs);
    if (rv == SECFailure) {
        return SECFailure;
    }
    *retryConfigs = out;
    return SECSuccess;
}

SECStatus
SSLExp_RemoveEchConfigs(PRFileDesc *fd)
{
    sslSocket *ss;

    if (!fd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in %s",
                 SSL_GETPID(), fd, __FUNCTION__));
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!PR_CLIST_IS_EMPTY(&ss->echConfigs)) {
        tls13_DestroyEchConfigs(&ss->echConfigs);
    }

    /* Also remove any retry_configs and handshake context. */
    if (ss->xtnData.echRetryConfigs.len) {
        SECITEM_FreeItem(&ss->xtnData.echRetryConfigs, PR_FALSE);
    }

    if (ss->ssl3.hs.echHpkeCtx) {
        PK11_HPKE_DestroyContext(ss->ssl3.hs.echHpkeCtx, PR_TRUE);
        ss->ssl3.hs.echHpkeCtx = NULL;
    }
    PORT_Free(CONST_CAST(char, ss->ssl3.hs.echPublicName));
    ss->ssl3.hs.echPublicName = NULL;

    return SECSuccess;
}

/* Import one or more ECHConfigs for the given keypair. The AEAD/KDF
 * may differ , but only X25519 is supported for the KEM.*/
SECStatus
SSLExp_SetServerEchConfigs(PRFileDesc *fd,
                           const SECKEYPublicKey *pubKey, const SECKEYPrivateKey *privKey,
                           const PRUint8 *echConfigs, unsigned int echConfigsLen)
{
#ifndef NSS_ENABLE_DRAFT_HPKE
    PORT_SetError(SSL_ERROR_FEATURE_DISABLED);
    return SECFailure;
#else
    sslSocket *ss;
    SECStatus rv;
    SECItem data = { siBuffer, CONST_CAST(PRUint8, echConfigs), echConfigsLen };

    if (!fd || !pubKey || !privKey || !echConfigs || echConfigsLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in %s",
                 SSL_GETPID(), fd, __FUNCTION__));
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Overwrite if we're already configured. */
    rv = SSLExp_RemoveEchConfigs(fd);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = tls13_DecodeEchConfigs(&data, &ss->echConfigs);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (PR_CLIST_IS_EMPTY(&ss->echConfigs)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    ss->echPubKey = SECKEY_CopyPublicKey(pubKey);
    if (!ss->echPubKey) {
        goto loser;
    }
    ss->echPrivKey = SECKEY_CopyPrivateKey(privKey);
    if (!ss->echPrivKey) {
        goto loser;
    }
    return SECSuccess;

loser:
    tls13_DestroyEchConfigs(&ss->echConfigs);
    SECKEY_DestroyPrivateKey(ss->echPrivKey);
    SECKEY_DestroyPublicKey(ss->echPubKey);
    ss->echPubKey = NULL;
    ss->echPrivKey = NULL;
    return SECFailure;
#endif
}

/* Client enable. For now, we'll use the first
 * compatible config (server preference). */
SECStatus
SSLExp_SetClientEchConfigs(PRFileDesc *fd,
                           const PRUint8 *echConfigs,
                           unsigned int echConfigsLen)
{
#ifndef NSS_ENABLE_DRAFT_HPKE
    PORT_SetError(SSL_ERROR_FEATURE_DISABLED);
    return SECFailure;
#else
    SECStatus rv;
    sslSocket *ss;
    SECItem data = { siBuffer, CONST_CAST(PRUint8, echConfigs), echConfigsLen };

    if (!fd || !echConfigs || echConfigsLen == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in %s",
                 SSL_GETPID(), fd, __FUNCTION__));
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Overwrite if we're already configured. */
    rv = SSLExp_RemoveEchConfigs(fd);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = tls13_DecodeEchConfigs(&data, &ss->echConfigs);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    if (PR_CLIST_IS_EMPTY(&ss->echConfigs)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    return SECSuccess;
#endif
}

/* Set up ECH. This generates an ephemeral sender
 * keypair and the HPKE context */
SECStatus
tls13_ClientSetupEch(sslSocket *ss, sslClientHelloType type)
{
    SECStatus rv;
    HpkeContext *cx = NULL;
    SECKEYPublicKey *pkR = NULL;
    SECItem hpkeInfo = { siBuffer, NULL, 0 };
    PK11SymKey *hrrPsk = NULL;
    sslEchConfig *cfg = NULL;
    const SECItem kEchHrrInfoItem = { siBuffer,
                                      (unsigned char *)kHpkeInfoEchHrr,
                                      strlen(kHpkeInfoEchHrr) };
    const SECItem kEchHrrPskLabelItem = { siBuffer,
                                          (unsigned char *)kHpkeLabelHrrPsk,
                                          strlen(kHpkeLabelHrrPsk) };

    if (PR_CLIST_IS_EMPTY(&ss->echConfigs) ||
        !ssl_ShouldSendSNIExtension(ss, ss->url) ||
        IS_DTLS(ss)) {
        return SECSuccess;
    }

    /* Maybe apply our own priority if >1. For now, we only support
     * one version and one KEM. Each ECHConfig can specify multiple
     * KDF/AEADs, so just use the first. */
    cfg = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);

    /* Skip ECH if the public name matches the private name. */
    if (0 == PORT_Strcmp(cfg->contents.publicName, ss->url)) {
        return SECSuccess;
    }

    SSL_TRC(50, ("%d: TLS13[%d]: Setup client ECH",
                 SSL_GETPID(), ss->fd));

    switch (type) {
        case client_hello_initial:
            PORT_Assert(!ss->ssl3.hs.echHpkeCtx && !ss->ssl3.hs.echPublicName);
            cx = PK11_HPKE_NewContext(cfg->contents.kemId, cfg->contents.kdfId,
                                      cfg->contents.aeadId, NULL, NULL);
            break;
        case client_hello_retry:
            PORT_Assert(ss->ssl3.hs.echHpkeCtx && ss->ssl3.hs.echPublicName);
            rv = PK11_HPKE_ExportSecret(ss->ssl3.hs.echHpkeCtx,
                                        &kEchHrrInfoItem, 32, &hrrPsk);
            if (rv != SECSuccess) {
                goto loser;
            }

            PK11_HPKE_DestroyContext(ss->ssl3.hs.echHpkeCtx, PR_TRUE);
            PORT_Free((void *)ss->ssl3.hs.echPublicName); /* CONST */
            ss->ssl3.hs.echHpkeCtx = NULL;
            ss->ssl3.hs.echPublicName = NULL;
            cx = PK11_HPKE_NewContext(cfg->contents.kemId, cfg->contents.kdfId,
                                      cfg->contents.aeadId, hrrPsk, &kEchHrrPskLabelItem);
            break;
        default:
            PORT_Assert(0);
            goto loser;
    }
    if (!cx) {
        goto loser;
    }

    rv = PK11_HPKE_Deserialize(cx, cfg->contents.publicKey.data, cfg->contents.publicKey.len, &pkR);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (!SECITEM_AllocItem(NULL, &hpkeInfo, strlen(kHpkeInfoEch) + 1 + cfg->raw.len)) {
        goto loser;
    }
    PORT_Memcpy(&hpkeInfo.data[0], kHpkeInfoEch, strlen(kHpkeInfoEch));
    PORT_Memset(&hpkeInfo.data[strlen(kHpkeInfoEch)], 0, 1);
    PORT_Memcpy(&hpkeInfo.data[strlen(kHpkeInfoEch) + 1], cfg->raw.data, cfg->raw.len);

    /* Setup with an ephemeral sender keypair. */
    rv = PK11_HPKE_SetupS(cx, NULL, NULL, pkR, &hpkeInfo);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (!ss->ssl3.hs.helloRetry) {
        rv = ssl3_GetNewRandom(ss->ssl3.hs.client_inner_random);
        if (rv != SECSuccess) {
            goto loser; /* code set */
        }
    }

    /* If ECH is rejected, the application will use SSLChannelInfo
     * to fetch this field and perform cert chain verification. */
    ss->ssl3.hs.echPublicName = PORT_Strdup(cfg->contents.publicName);
    if (!ss->ssl3.hs.echPublicName) {
        goto loser;
    }

    ss->ssl3.hs.echHpkeCtx = cx;
    PK11_FreeSymKey(hrrPsk);
    SECKEY_DestroyPublicKey(pkR);
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    return SECSuccess;

loser:
    PK11_HPKE_DestroyContext(cx, PR_TRUE);
    PK11_FreeSymKey(hrrPsk);
    SECKEY_DestroyPublicKey(pkR);
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    return SECFailure;
}

/*
 *  enum {
 *     encrypted_client_hello(0xfe08), (65535)
 *  } ExtensionType;
 *
 *  struct {
 *      HpkeKdfId kdf_id;
 *      HpkeAeadId aead_id;
 *  } ECHCipherSuite;
 *  struct {
 *     ECHCipherSuite cipher_suite;
 *     opaque config_id<0..255>;
 *     opaque enc<1..2^16-1>;
 *     opaque payload<1..2^16-1>;
 *  } ClientECH;
 *
 * Takes as input the constructed ClientHelloInner and
 * returns a constructed encrypted_client_hello extension
 * (replacing the contents of |chInner|).
 */
static SECStatus
tls13_EncryptClientHello(sslSocket *ss, sslBuffer *outerAAD, sslBuffer *chInner)
{
    SECStatus rv;
    SECItem chPt = { siBuffer, chInner->buf, chInner->len };
    SECItem *chCt = NULL;
    SECItem aadItem = { siBuffer, outerAAD ? outerAAD->buf : NULL, outerAAD ? outerAAD->len : 0 };
    const SECItem *hpkeEnc = NULL;
    const sslEchConfig *cfg = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);
    PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->echConfigs));

    SSL_TRC(50, ("%d: TLS13[%d]: Encrypting Client Hello Inner",
                 SSL_GETPID(), ss->fd));

    hpkeEnc = PK11_HPKE_GetEncapPubKey(ss->ssl3.hs.echHpkeCtx);
    if (!hpkeEnc) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        goto loser;
    }

#ifndef UNSAFE_FUZZER_MODE
    rv = PK11_HPKE_Seal(ss->ssl3.hs.echHpkeCtx, &aadItem, &chPt, &chCt);
    if (rv != SECSuccess) {
        goto loser;
    }
#else
    /* Fake a tag. */
    SECITEM_AllocItem(NULL, chCt, chPt.len + 16);
    if (!chCt) {
        goto loser;
    }
    PORT_Memcpy(chCt->data, chPt.data, chPt.len);
#endif

    /* Format the encrypted_client_hello extension. */
    sslBuffer_Clear(chInner);
    rv = sslBuffer_AppendNumber(chInner, cfg->contents.kdfId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(chInner, cfg->contents.aeadId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendVariable(chInner, cfg->configId, sizeof(cfg->configId), 1);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendVariable(chInner, hpkeEnc->data, hpkeEnc->len, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendVariable(chInner, chCt->data, chCt->len, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    SECITEM_FreeItem(chCt, PR_TRUE);
    return SECSuccess;

loser:
    SECITEM_FreeItem(chCt, PR_TRUE);
    return SECFailure;
}

SECStatus
tls13_GetMatchingEchConfig(const sslSocket *ss, HpkeKdfId kdf, HpkeAeadId aead,
                           const SECItem *configId, sslEchConfig **cfg)
{
    sslEchConfig *candidate;
    PRINT_BUF(50, (ss, "Server GetMatchingEchConfig with digest:",
                   configId->data, configId->len));

    for (PRCList *cur_p = PR_LIST_HEAD(&ss->echConfigs);
         cur_p != &ss->echConfigs;
         cur_p = PR_NEXT_LINK(cur_p)) {
        sslEchConfig *echConfig = (sslEchConfig *)cur_p;
        if (configId->len != sizeof(echConfig->configId) ||
            PORT_Memcmp(echConfig->configId, configId->data, sizeof(echConfig->configId))) {
            continue;
        }
        candidate = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);
        if (candidate->contents.aeadId != aead ||
            candidate->contents.kdfId != kdf) {
            continue;
        }
        *cfg = candidate;
        return SECSuccess;
    }

    SSL_TRC(50, ("%d: TLS13[%d]: Server found no matching ECHConfig",
                 SSL_GETPID(), ss->fd));

    *cfg = NULL;
    return SECSuccess;
}

/* This is unfortunate in that it requires a second decryption of the cookie.
 * This is largely copied from tls13hashstate.c as HRR handling is still in flux.
 * TODO: Consolidate this code no later than -09. */
/* struct {
 *     uint8 indicator = 0xff;            // To disambiguate from tickets.
 *     uint16 cipherSuite;                // Selected cipher suite.
 *     uint16 keyShare;                   // Requested key share group (0=none)
 *     opaque applicationToken<0..65535>; // Application token
 *     opaque echHrrPsk<0..255>;          // Encrypted ClientHello HRR PSK
 *     opaque echConfigId<0..255>;        // ECH config ID selected in CH1, to decrypt the CH2 ECH payload.
 *     opaque ch_hash[rest_of_buffer];    // H(ClientHello)
 * } CookieInner;
 */
SECStatus
tls13_GetEchInfoFromCookie(sslSocket *ss, const TLSExtension *hrrCookie, PK11SymKey **echHrrPsk, SECItem *echConfigId)
{
    SECStatus rv;
    PK11SymKey *hrrKey = NULL;
    PRUint64 tmpn;
    sslReadBuffer tmpReader = { 0 };
    PK11SlotInfo *slot = NULL;
    unsigned char plaintext[1024];
    unsigned int plaintextLen = 0;
    SECItem hrrPskItem = { siBuffer, NULL, 0 };
    SECItem hrrCookieData = { siBuffer, NULL, 0 };
    SECItem saveHrrCookieData = hrrCookieData;
    SECItem previousEchConfigId = { siBuffer, NULL, 0 };

    /* Copy the extension data so as to not consume it in the handler.
     * The extension handler walks the pointer, so save a copy to free. */
    rv = SECITEM_CopyItem(NULL, &hrrCookieData, &hrrCookie->data);
    if (rv != SECSuccess) {
        goto loser;
    }
    saveHrrCookieData = hrrCookieData;

    rv = tls13_ServerHandleCookieXtn(ss, &ss->xtnData, &hrrCookieData);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = ssl_SelfEncryptUnprotect(ss, ss->xtnData.cookie.data, ss->xtnData.cookie.len,
                                  plaintext, &plaintextLen, sizeof(plaintext));
    if (rv != SECSuccess) {
        goto loser;
    }

    sslReader reader = SSL_READER(plaintext, plaintextLen);

    /* Should start with 0xff. */
    rv = sslRead_ReadNumber(&reader, 1, &tmpn);
    if ((rv != SECSuccess) || (tmpn != 0xff)) {
        rv = SECFailure;
        goto loser;
    }
    rv = sslRead_ReadNumber(&reader, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* The named group, if any. */
    rv = sslRead_ReadNumber(&reader, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* Application token. */
    rv = sslRead_ReadNumber(&reader, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslRead_Read(&reader, tmpn, &tmpReader);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* ECH Config ID */
    rv = sslRead_ReadVariable(&reader, 1, &tmpReader);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECITEM_MakeItem(NULL, &previousEchConfigId,
                          tmpReader.buf, tmpReader.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* ECH HRR key. */
    rv = sslRead_ReadVariable(&reader, 1, &tmpReader);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (tmpReader.len) {
        slot = PK11_GetInternalSlot();
        if (!slot) {
            rv = SECFailure;
            goto loser;
        }
        hrrPskItem.len = tmpReader.len;
        hrrPskItem.data = CONST_CAST(PRUint8, tmpReader.buf);
        hrrKey = PK11_ImportSymKey(slot, CKM_HKDF_KEY_GEN, PK11_OriginUnwrap,
                                   CKA_DERIVE, &hrrPskItem, NULL);
        PK11_FreeSlot(slot);
        if (!hrrKey) {
            rv = SECFailure;
            goto loser;
        }
    }
    *echConfigId = previousEchConfigId;
    *echHrrPsk = hrrKey;
    SECITEM_FreeItem(&saveHrrCookieData, PR_FALSE);
    return SECSuccess;

loser:
    SECITEM_FreeItem(&previousEchConfigId, PR_FALSE);
    SECITEM_FreeItem(&saveHrrCookieData, PR_FALSE);
    return SECFailure;
}

/* Given a CH with extensions, copy from the start up to the extensions
 * into |writer| and return the extensions themselves in |extensions|.
 * If |explicitSid|, place this value into |writer| as the SID. Else,
 * the sid is copied from |reader| to |writer|. */
static SECStatus
tls13_CopyChPreamble(sslReader *reader, const SECItem *explicitSid, sslBuffer *writer, sslReadBuffer *extensions)
{
    SECStatus rv;
    sslReadBuffer tmpReadBuf;

    /* Locate the extensions. */
    rv = sslRead_Read(reader, 2 + SSL3_RANDOM_LENGTH, &tmpReadBuf);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Append(writer, tmpReadBuf.buf, tmpReadBuf.len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* legacy_session_id */
    rv = sslRead_ReadVariable(reader, 1, &tmpReadBuf);
    if (explicitSid) {
        /* Encoded SID should be empty when copying from CHOuter. */
        if (tmpReadBuf.len > 0) {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
            return SECFailure;
        }
        rv = sslBuffer_AppendVariable(writer, explicitSid->data, explicitSid->len, 1);
    } else {
        rv = sslBuffer_AppendVariable(writer, tmpReadBuf.buf, tmpReadBuf.len, 1);
    }
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* cipher suites */
    rv = sslRead_ReadVariable(reader, 2, &tmpReadBuf);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_AppendVariable(writer, tmpReadBuf.buf, tmpReadBuf.len, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* compression */
    rv = sslRead_ReadVariable(reader, 1, &tmpReadBuf);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_AppendVariable(writer, tmpReadBuf.buf, tmpReadBuf.len, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* extensions */
    rv = sslRead_ReadVariable(reader, 2, extensions);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (SSL_READER_REMAINING(reader) != 0) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
        return SECFailure;
    }

    return SECSuccess;
}

static SECStatus
tls13_MakeChOuterAAD(const SECItem *outer, sslBuffer *outerAAD)
{
    SECStatus rv;
    sslBuffer aad = SSL_BUFFER_EMPTY;
    sslReadBuffer aadXtns;
    sslReader chReader = SSL_READER(outer->data, outer->len);
    PRUint64 tmpn;
    sslReadBuffer tmpvar;
    unsigned int offset;
    unsigned int preambleLen;

    rv = sslBuffer_Skip(&aad, 4, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* aad := preamble, aadXtn := extensions */
    rv = tls13_CopyChPreamble(&chReader, NULL, &aad, &aadXtns);
    if (rv != SECSuccess) {
        goto loser;
    }

    sslReader xtnsReader = SSL_READER(aadXtns.buf, aadXtns.len);
    preambleLen = SSL_BUFFER_LEN(&aad);

    /* Save room for extensions length. */
    rv = sslBuffer_Skip(&aad, 2, &offset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Append each extension, minus encrypted_client_hello_xtn. */
    while (SSL_READER_REMAINING(&xtnsReader)) {
        rv = sslRead_ReadNumber(&xtnsReader, 2, &tmpn);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = sslRead_ReadVariable(&xtnsReader, 2, &tmpvar);
        if (rv != SECSuccess) {
            goto loser;
        }

        if (tmpn != ssl_tls13_encrypted_client_hello_xtn) {
            rv = sslBuffer_AppendNumber(&aad, tmpn, 2);
            if (rv != SECSuccess) {
                goto loser;
            }
            rv = sslBuffer_AppendVariable(&aad, tmpvar.buf, tmpvar.len, 2);
            if (rv != SECSuccess) {
                goto loser;
            }
        }
    }

    rv = sslBuffer_InsertNumber(&aad, offset, SSL_BUFFER_LEN(&aad) - preambleLen - 2, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Give it a message header. */
    rv = sslBuffer_InsertNumber(&aad, 0, ssl_hs_client_hello, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_InsertLength(&aad, 1, 3);
    if (rv != SECSuccess) {
        goto loser;
    }
    *outerAAD = aad;
    return SECSuccess;

loser:
    sslBuffer_Clear(&aad);
    return SECFailure;
}

SECStatus
tls13_OpenClientHelloInner(sslSocket *ss, const SECItem *outer, sslEchConfig *cfg, PK11SymKey *echHrrPsk, SECItem **chInner)
{
    SECStatus rv;
    sslBuffer outerAAD = SSL_BUFFER_EMPTY;
    HpkeContext *cx = NULL;
    SECItem *decryptedChInner = NULL;
    SECItem hpkeInfo = { siBuffer, NULL, 0 };
    SECItem outerAADItem = { siBuffer, NULL, 0 };
    const SECItem kEchHrrPskLabelItem = { siBuffer,
                                          (unsigned char *)kHpkeLabelHrrPsk,
                                          strlen(kHpkeLabelHrrPsk) };
    SSL_TRC(50, ("%d: TLS13[%d]: Server opening ECH Inner%s", SSL_GETPID(),
                 ss->fd, ss->ssl3.hs.helloRetry ? " after HRR" : ""));

    cx = PK11_HPKE_NewContext(cfg->contents.kemId, cfg->contents.kdfId,
                              cfg->contents.aeadId, echHrrPsk,
                              echHrrPsk ? &kEchHrrPskLabelItem : NULL);
    if (!cx) {
        goto loser;
    }

    if (!SECITEM_AllocItem(NULL, &hpkeInfo, strlen(kHpkeInfoEch) + 1 + cfg->raw.len)) {
        goto loser;
    }
    PORT_Memcpy(&hpkeInfo.data[0], kHpkeInfoEch, strlen(kHpkeInfoEch));
    PORT_Memset(&hpkeInfo.data[strlen(kHpkeInfoEch)], 0, 1);
    PORT_Memcpy(&hpkeInfo.data[strlen(kHpkeInfoEch) + 1], cfg->raw.data, cfg->raw.len);

    rv = PK11_HPKE_SetupR(cx, ss->echPubKey, ss->echPrivKey,
                          &ss->xtnData.echSenderPubKey, &hpkeInfo);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }

    rv = tls13_MakeChOuterAAD(outer, &outerAAD);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }

    outerAADItem.data = outerAAD.buf;
    outerAADItem.len = outerAAD.len;

#ifndef UNSAFE_FUZZER_MODE
    rv = PK11_HPKE_Open(cx, &outerAADItem, &ss->xtnData.innerCh, &decryptedChInner);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }
#else
    rv = SECITEM_CopyItem(NULL, decryptedChInner, &ss->xtnData.innerCh);
    if (rv != SECSuccess) {
        goto loser;
    }
    decryptedChInner->len -= 16; /* Fake tag */
#endif

    /* Stash the context, we may need it for HRR. */
    ss->ssl3.hs.echHpkeCtx = cx;
    *chInner = decryptedChInner;
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    sslBuffer_Clear(&outerAAD);
    return SECSuccess;

loser:
    SECITEM_FreeItem(decryptedChInner, PR_TRUE);
    PK11_HPKE_DestroyContext(cx, PR_TRUE);
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    sslBuffer_Clear(&outerAAD);
    return SECFailure;
}

/* Given a buffer of extensions prepared for CHOuter, translate those extensions to a
 * buffer suitable for CHInner. This is intended to be called twice: once without
 * compression for the transcript hash and binders, and once with compression for
 * encoding the actual CHInner value. On the first run, if |inOutPskXtn| and
 * chOuterXtnsBuf contains a PSK extension, remove it and return in the outparam.
 * The caller will compute the binder value based on the uncompressed output. Next,
 * if |compress|, consolidate duplicated extensions (that would otherwise be copied)
 * into a single outer_extensions extension. If |inOutPskXtn|, the extension contains
 * a binder, it is appended after the deduplicated outer_extensions. In the case of
 * GREASE ECH, one call is made to estimate size (wiith compression, null inOutPskXtn).
 */
SECStatus
tls13_ConstructInnerExtensionsFromOuter(sslSocket *ss, sslBuffer *chOuterXtnsBuf,
                                        sslBuffer *chInnerXtns, sslBuffer *inOutPskXtn,
                                        PRBool compress)
{
    SECStatus rv;
    PRUint64 extensionType;
    sslReadBuffer extensionData;
    sslBuffer pskXtn = SSL_BUFFER_EMPTY;
    sslBuffer dupXtns = SSL_BUFFER_EMPTY; /* Dupcliated extensions, types-only if |compress|. */
    unsigned int tmpOffset;
    unsigned int tmpLen;
    unsigned int srcXtnBase; /* To truncate CHOuter and remove the PSK extension. */
    SSL_TRC(50, ("%d: TLS13[%d]: Constructing ECH inner extensions %s compression",
                 SSL_GETPID(), compress ? "with" : "without"));

    /* When offering the "encrypted_client_hello" extension in its
     * ClientHelloOuter, the client MUST also offer an empty
     * "encrypted_client_hello" extension in its ClientHelloInner. */
    rv = sslBuffer_AppendNumber(chInnerXtns, ssl_tls13_encrypted_client_hello_xtn, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(chInnerXtns, 0, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    sslReader rdr = SSL_READER(chOuterXtnsBuf->buf, chOuterXtnsBuf->len);
    while (SSL_READER_REMAINING(&rdr)) {
        srcXtnBase = rdr.offset;
        rv = sslRead_ReadNumber(&rdr, 2, &extensionType);
        if (rv != SECSuccess) {
            goto loser;
        }

        /* Get the extension data. */
        rv = sslRead_ReadVariable(&rdr, 2, &extensionData);
        if (rv != SECSuccess) {
            goto loser;
        }

        switch (extensionType) {
            case ssl_server_name_xtn:
                /* Write the real (private) SNI value. */
                rv = sslBuffer_AppendNumber(chInnerXtns, extensionType, 2);
                if (rv != SECSuccess) {
                    goto loser;
                }
                rv = sslBuffer_Skip(chInnerXtns, 2, &tmpOffset);
                if (rv != SECSuccess) {
                    goto loser;
                }
                tmpLen = SSL_BUFFER_LEN(chInnerXtns);
                rv = ssl3_ClientFormatServerNameXtn(ss, ss->url,
                                                    strlen(ss->url),
                                                    NULL, chInnerXtns);
                if (rv != SECSuccess) {
                    goto loser;
                }
                tmpLen = SSL_BUFFER_LEN(chInnerXtns) - tmpLen;
                rv = sslBuffer_InsertNumber(chInnerXtns, tmpOffset, tmpLen, 2);
                if (rv != SECSuccess) {
                    goto loser;
                }
                break;
            case ssl_tls13_supported_versions_xtn:
                /* Only TLS 1.3 on CHInner. */
                rv = sslBuffer_AppendNumber(chInnerXtns, extensionType, 2);
                if (rv != SECSuccess) {
                    goto loser;
                }
                rv = sslBuffer_AppendNumber(chInnerXtns, 3, 2);
                if (rv != SECSuccess) {
                    goto loser;
                }
                rv = sslBuffer_AppendNumber(chInnerXtns, 2, 1);
                if (rv != SECSuccess) {
                    goto loser;
                }
                rv = sslBuffer_AppendNumber(chInnerXtns, SSL_LIBRARY_VERSION_TLS_1_3, 2);
                if (rv != SECSuccess) {
                    goto loser;
                }
                break;
            case ssl_tls13_pre_shared_key_xtn:
                /* If GREASEing, the estimated internal length
                 * will be short. However, the presence of a PSK extension in
                 * CHOuter is already a distinguisher. */
                if (inOutPskXtn) {
                    rv = sslBuffer_AppendNumber(&pskXtn, extensionType, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    rv = sslBuffer_AppendVariable(&pskXtn, extensionData.buf,
                                                  extensionData.len, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    /* In terms of CHOuter, the PSK extension no longer exists.
                     * 0 lastXtnOffset means insert padding at the end. */
                    SSL_BUFFER_LEN(chOuterXtnsBuf) = srcXtnBase;
                    ss->xtnData.lastXtnOffset = 0;
                }
                break;
            default:
                PORT_Assert(extensionType != ssl_tls13_encrypted_client_hello_xtn);
                rv = sslBuffer_AppendNumber(&dupXtns, extensionType, 2);
                if (rv != SECSuccess) {
                    goto loser;
                }
                if (!compress) {
                    rv = sslBuffer_AppendVariable(&dupXtns, extensionData.buf,
                                                  extensionData.len, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                }
                break;
        }
    }

    /* Append duplicated extensions, compressing or not. */
    if (SSL_BUFFER_LEN(&dupXtns) && compress) {
        rv = sslBuffer_AppendNumber(chInnerXtns, ssl_tls13_outer_extensions_xtn, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = sslBuffer_AppendNumber(chInnerXtns, dupXtns.len + 1, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = sslBuffer_AppendBufferVariable(chInnerXtns, &dupXtns, 1);
    } else if (SSL_BUFFER_LEN(&dupXtns)) {
        /* Each duplicated extension has its own length. */
        rv = sslBuffer_AppendBuffer(chInnerXtns, &dupXtns);
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    /* On the compression run, append the completed PSK extension (if
     * provided). Else an incomplete (no binder) extension; the caller
     * will compute the binder and call again. */
    if (compress && inOutPskXtn) {
        rv = sslBuffer_AppendBuffer(chInnerXtns, inOutPskXtn);
    } else if (pskXtn.len) {
        rv = sslBuffer_AppendBuffer(chInnerXtns, &pskXtn);
        if (inOutPskXtn) {
            *inOutPskXtn = pskXtn;
        }
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    sslBuffer_Clear(&dupXtns);
    return SECSuccess;

loser:
    sslBuffer_Clear(&pskXtn);
    sslBuffer_Clear(&dupXtns);
    return SECFailure;
}

static SECStatus
tls13_EncodeClientHelloInner(sslSocket *ss, sslBuffer *chInner, sslBuffer *chInnerXtns, sslBuffer *out)
{
    PORT_Assert(ss && chInner && chInnerXtns && out);
    SECStatus rv;
    sslReadBuffer tmpReadBuf;
    sslReader chReader = SSL_READER(chInner->buf, chInner->len);

    rv = sslRead_Read(&chReader, 4, &tmpReadBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslRead_Read(&chReader, 2 + SSL3_RANDOM_LENGTH, &tmpReadBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_Append(out, tmpReadBuf.buf, tmpReadBuf.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Skip the legacy_session_id */
    rv = sslRead_ReadVariable(&chReader, 1, &tmpReadBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(out, 0, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* cipher suites */
    rv = sslRead_ReadVariable(&chReader, 2, &tmpReadBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendVariable(out, tmpReadBuf.buf, tmpReadBuf.len, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* compression methods */
    rv = sslRead_ReadVariable(&chReader, 1, &tmpReadBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendVariable(out, tmpReadBuf.buf, tmpReadBuf.len, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Append the extensions. */
    rv = sslBuffer_AppendBufferVariable(out, chInnerXtns, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    return SECSuccess;

loser:
    sslBuffer_Clear(out);
    return SECFailure;
}

SECStatus
tls13_ConstructClientHelloWithEch(sslSocket *ss, const sslSessionID *sid, PRBool freshSid,
                                  sslBuffer *chOuter, sslBuffer *chOuterXtnsBuf)
{
    SECStatus rv;
    sslBuffer chInner = SSL_BUFFER_EMPTY;
    sslBuffer encodedChInner = SSL_BUFFER_EMPTY;
    sslBuffer chInnerXtns = SSL_BUFFER_EMPTY;
    sslBuffer pskXtn = SSL_BUFFER_EMPTY;
    sslBuffer outerAAD = SSL_BUFFER_EMPTY;
    unsigned int encodedChLen;
    unsigned int preambleLen;
    SSL_TRC(50, ("%d: TLS13[%d]: Constructing ECH inner", SSL_GETPID()));

    /* Create the full (uncompressed) inner extensions and steal any PSK extension.
     * NB: Neither chOuterXtnsBuf nor chInnerXtns are length-prefixed. */
    rv = tls13_ConstructInnerExtensionsFromOuter(ss, chOuterXtnsBuf, &chInnerXtns,
                                                 &pskXtn, PR_FALSE);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }

    rv = ssl3_CreateClientHelloPreamble(ss, sid, PR_FALSE, SSL_LIBRARY_VERSION_TLS_1_3,
                                        PR_TRUE, &chInnerXtns, &chInner);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }
    preambleLen = SSL_BUFFER_LEN(&chInner);

    /* Write handshake header length. tls13_EncryptClientHello will
     * remove this upon encoding, but the transcript needs it. This assumes
     * the 4B stream-variant header. */
    PORT_Assert(!IS_DTLS(ss));
    rv = sslBuffer_InsertNumber(&chInner, 1,
                                chInner.len + 2 + chInnerXtns.len - 4, 3);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (pskXtn.len) {
        PORT_Assert(ssl3_ExtensionAdvertised(ss, ssl_tls13_pre_shared_key_xtn));
        PORT_Assert(ss->xtnData.lastXtnOffset == 0); /* stolen from outer */
        rv = tls13_WriteExtensionsWithBinder(ss, &chInnerXtns, &chInner);
        /* Update the stolen PSK extension with the binder value. */
        PORT_Memcpy(pskXtn.buf, &chInnerXtns.buf[chInnerXtns.len - pskXtn.len], pskXtn.len);
    } else {
        rv = sslBuffer_AppendBufferVariable(&chInner, &chInnerXtns, 2);
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = ssl3_UpdateExplicitHandshakeTranscript(ss, chInner.buf, chInner.len,
                                                &ss->ssl3.hs.echInnerMessages);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }

    /* Un-append the extensions, then append compressed via Encoded. */
    SSL_BUFFER_LEN(&chInner) = preambleLen;
    sslBuffer_Clear(&chInnerXtns);
    rv = tls13_ConstructInnerExtensionsFromOuter(ss, chOuterXtnsBuf,
                                                 &chInnerXtns, &pskXtn, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* TODO: Pad CHInner */
    rv = tls13_EncodeClientHelloInner(ss, &chInner, &chInnerXtns, &encodedChInner);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Pad the outer prior to appending ECH (for the AAD).
     * Encoded extension size is (echCipherSuite + enc + configId + payload + tag).
     * Post-encryption, we'll assert that this was correct. */
    encodedChLen = 4 + 33 + 34 + 2 + encodedChInner.len + 16;
    rv = ssl_InsertPaddingExtension(ss, chOuter->len + encodedChLen, chOuterXtnsBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Make the ClientHelloOuterAAD value, which is complete
     * chOuter minus encrypted_client_hello xtn. */
    rv = sslBuffer_Append(&outerAAD, chOuter->buf, chOuter->len);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendBufferVariable(&outerAAD, chOuterXtnsBuf, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_InsertLength(&outerAAD, 1, 3);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Insert the encrypted_client_hello xtn and coalesce. */
    rv = tls13_EncryptClientHello(ss, &outerAAD, &encodedChInner);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_Assert(encodedChLen == encodedChInner.len);

    rv = ssl3_EmplaceExtension(ss, chOuterXtnsBuf, ssl_tls13_encrypted_client_hello_xtn,
                               encodedChInner.buf, encodedChInner.len, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = ssl3_InsertChHeaderSize(ss, chOuter, chOuterXtnsBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendBufferVariable(chOuter, chOuterXtnsBuf, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

loser:
    sslBuffer_Clear(&chInner);
    sslBuffer_Clear(&encodedChInner);
    sslBuffer_Clear(&chInnerXtns);
    sslBuffer_Clear(&pskXtn);
    sslBuffer_Clear(&outerAAD);
    return rv;
}

static SECStatus
tls13_ComputeEchSignal(sslSocket *ss, PRUint8 *out)
{
    SECStatus rv;
    PRUint8 derived[64];
    SECItem randItem = { siBuffer,
                         ss->sec.isServer ? ss->ssl3.hs.client_random : ss->ssl3.hs.client_inner_random,
                         SSL3_RANDOM_LENGTH };
    SSLHashType hashAlg = tls13_GetHash(ss);
    PK11SymKey *extracted = NULL;
    PK11SymKey *randKey = NULL;
    PK11SlotInfo *slot = PK11_GetInternalSlot();
    if (!slot) {
        goto loser;
    }

    randKey = PK11_ImportDataKey(slot, CKM_HKDF_DATA, PK11_OriginUnwrap,
                                 CKA_DERIVE, &randItem, NULL);
    if (!randKey) {
        goto loser;
    }

    rv = tls13_HkdfExtract(NULL, randKey, hashAlg, &extracted);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabelRaw(extracted, hashAlg, ss->ssl3.hs.server_random, 24,
                                  kHkdfInfoEchConfirm, strlen(kHkdfInfoEchConfirm),
                                  ss->protocolVariant, derived, TLS13_ECH_SIGNAL_LEN);
    if (rv != SECSuccess) {
        goto loser;
    }

    PORT_Memcpy(out, derived, TLS13_ECH_SIGNAL_LEN);
    SSL_TRC(50, ("%d: TLS13[%d]: %s computed ECH signal", SSL_GETPID(), ss->fd, SSL_ROLE(ss)));
    PRINT_BUF(50, (ss, "", out, TLS13_ECH_SIGNAL_LEN));
    PK11_FreeSymKey(extracted);
    PK11_FreeSymKey(randKey);
    PK11_FreeSlot(slot);
    return SECSuccess;

loser:
    PK11_FreeSymKey(extracted);
    PK11_FreeSymKey(randKey);
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return SECFailure;
}

/* Called just prior to padding the CH. Use the size of the CH to estimate
 * the size of a corresponding ECH extension, then add it to the buffer. */
SECStatus
tls13_MaybeGreaseEch(sslSocket *ss, unsigned int preambleLen, sslBuffer *buf)
{
    SECStatus rv;
    sslBuffer chInnerXtns = SSL_BUFFER_EMPTY;
    sslBuffer greaseBuf = SSL_BUFFER_EMPTY;
    unsigned int payloadLen;
    HpkeAeadId aead;
    PK11SlotInfo *slot = NULL;
    PK11SymKey *hmacPrk = NULL;
    PK11SymKey *derivedData = NULL;
    SECItem *rawData;
    CK_HKDF_PARAMS params;
    SECItem paramsi;

    if (!ss->opt.enableTls13GreaseEch || ss->ssl3.hs.echHpkeCtx) {
        return SECSuccess;
    }

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3 ||
        IS_DTLS(ss)) {
        return SECSuccess;
    }

    /* Compress the extensions for payload length. */
    rv = tls13_ConstructInnerExtensionsFromOuter(ss, buf, &chInnerXtns,
                                                 NULL, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser; /* Code set */
    }
    payloadLen = preambleLen + 2 /* Xtns len */ + chInnerXtns.len - 4 /* msg header */;
    payloadLen += 16; /* Aead tag */

    /* HMAC-Expand to get something that will pass for ciphertext. */
    slot = PK11_GetBestSlot(CKM_HKDF_DERIVE, NULL);
    if (!slot) {
        goto loser;
    }

    hmacPrk = PK11_KeyGen(slot, CKM_HKDF_DATA, NULL, SHA256_LENGTH, NULL);
    if (!hmacPrk) {
        goto loser;
    }

    params.bExtract = CK_FALSE;
    params.bExpand = CK_TRUE;
    params.prfHashMechanism = CKM_SHA256;
    params.pInfo = NULL;
    params.ulInfoLen = 0;
    paramsi.data = (unsigned char *)&params;
    paramsi.len = sizeof(params);
    derivedData = PK11_DeriveWithFlags(hmacPrk, CKM_HKDF_DATA,
                                       &paramsi, CKM_HKDF_DATA,
                                       CKA_DERIVE, 65 + payloadLen,
                                       CKF_VERIFY);
    if (!derivedData) {
        goto loser;
    }

    rv = PK11_ExtractKeyValue(derivedData);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* 1B aead determinant (don't send), 32B config_id, 32B enc, payload */
    rawData = PK11_GetKeyData(derivedData);
    if (!rawData) {
        goto loser;
    }
    PORT_Assert(rawData->len == 65 + payloadLen);

    /* struct {
       HpkeKdfId kdf_id;
       HpkeAeadId aead_id;
       opaque config_id<0..255>;
       opaque enc<1..2^16-1>;
       opaque payload<1..2^16-1>;
    } ClientECH; */

    /* Only support SHA256. */
    rv = sslBuffer_AppendNumber(&greaseBuf, HpkeKdfHkdfSha256, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* HpkeAeadAes128Gcm = 1, HpkeAeadChaCha20Poly1305 = 3, */
    aead = (rawData->data[0] & 1) ? HpkeAeadAes128Gcm : HpkeAeadChaCha20Poly1305;
    rv = sslBuffer_AppendNumber(&greaseBuf, aead, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendVariable(&greaseBuf, &rawData->data[1], 32, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* enc len is fixed 32B for X25519. */
    rv = sslBuffer_AppendVariable(&greaseBuf, &rawData->data[33], 32, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendVariable(&greaseBuf, &rawData->data[65], payloadLen, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Mark ECH as advertised so that we can validate any response.
     * We'll use echHpkeCtx to determine if we sent real or GREASE ECH.
     * TODO: Maybe a broader need to similarly track GREASED extensions? */
    rv = ssl3_EmplaceExtension(ss, buf, ssl_tls13_encrypted_client_hello_xtn,
                               greaseBuf.buf, greaseBuf.len, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }
    sslBuffer_Clear(&greaseBuf);
    sslBuffer_Clear(&chInnerXtns);
    PK11_FreeSymKey(hmacPrk);
    PK11_FreeSymKey(derivedData);
    PK11_FreeSlot(slot);
    return SECSuccess;

loser:
    sslBuffer_Clear(&greaseBuf);
    sslBuffer_Clear(&chInnerXtns);
    PK11_FreeSymKey(hmacPrk);
    PK11_FreeSymKey(derivedData);
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return SECFailure;
}

SECStatus
tls13_MaybeHandleEch(sslSocket *ss, const PRUint8 *msg, PRUint32 msgLen, SECItem *sidBytes,
                     SECItem *comps, SECItem *cookieBytes, SECItem *suites, SECItem **echInner)
{
    SECStatus rv;
    int error;
    SSL3AlertDescription desc;
    SECItem *tmpEchInner = NULL;
    PRUint8 *b;
    PRUint32 length;
    TLSExtension *echExtension;
    TLSExtension *versionExtension;
    PORT_Assert(!ss->ssl3.hs.echAccepted);
    SECItem tmpSid = { siBuffer, NULL, 0 };
    SECItem tmpCookie = { siBuffer, NULL, 0 };
    SECItem tmpSuites = { siBuffer, NULL, 0 };
    SECItem tmpComps = { siBuffer, NULL, 0 };

    echExtension = ssl3_FindExtension(ss, ssl_tls13_encrypted_client_hello_xtn);
    if (echExtension) {
        rv = tls13_ServerHandleEchXtn(ss, &ss->xtnData, &echExtension->data);
        if (rv != SECSuccess) {
            goto loser; /* code set, alert sent. */
        }
        rv = tls13_MaybeAcceptEch(ss, sidBytes, msg, msgLen, &tmpEchInner);
        if (rv != SECSuccess) {
            goto loser; /* code set, alert sent. */
        }
    }
    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_ech;

    if (ss->ssl3.hs.echAccepted) {
        PORT_Assert(tmpEchInner);
        PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->ssl3.hs.remoteExtensions));

        /* Start over on ECHInner */
        b = tmpEchInner->data;
        length = tmpEchInner->len;
        rv = ssl3_HandleClientHelloPreamble(ss, &b, &length, &tmpSid,
                                            &tmpCookie, &tmpSuites, &tmpComps);
        if (rv != SECSuccess) {
            goto loser; /* code set, alert sent. */
        }

        /* Since in Outer we explicitly call the ECH handler, do the same on Inner.
         * Extensions are already parsed in tls13_MaybeAcceptEch. */
        echExtension = ssl3_FindExtension(ss, ssl_tls13_encrypted_client_hello_xtn);
        if (!echExtension) {
            FATAL_ERROR(ss, SSL_ERROR_MISSING_ECH_EXTENSION, decode_error);
            goto loser;
        }
        rv = tls13_ServerHandleEchXtn(ss, &ss->xtnData, &echExtension->data);
        if (rv != SECSuccess) {
            goto loser; /* code set, alert sent. */
        }

        versionExtension = ssl3_FindExtension(ss, ssl_tls13_supported_versions_xtn);
        if (!versionExtension) {
            FATAL_ERROR(ss, SSL_ERROR_UNSUPPORTED_VERSION, protocol_version);
            goto loser;
        }
        rv = tls13_NegotiateVersion(ss, versionExtension);
        if (rv != SECSuccess) {
            /* Could be malformed or not allowed in ECH. */
            error = PORT_GetError();
            desc = (error == SSL_ERROR_UNSUPPORTED_VERSION) ? protocol_version : illegal_parameter;
            FATAL_ERROR(ss, error, desc);
            goto loser;
        }

        *comps = tmpComps;
        *cookieBytes = tmpCookie;
        *sidBytes = tmpSid;
        *suites = tmpSuites;
        *echInner = tmpEchInner;
    }
    return SECSuccess;

loser:
    SECITEM_FreeItem(tmpEchInner, PR_TRUE);
    return SECFailure;
}

SECStatus
tls13_MaybeHandleEchSignal(sslSocket *ss)
{
    SECStatus rv;
    PRUint8 computed[TLS13_ECH_SIGNAL_LEN];
    const PRUint8 *signal = &ss->ssl3.hs.server_random[SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN];
    PORT_Assert(!ss->sec.isServer);

    /* If !echHpkeCtx, we either didn't advertise or sent GREASE ECH. */
    if (ss->ssl3.hs.echHpkeCtx) {
        PORT_Assert(ssl3_ExtensionAdvertised(ss, ssl_tls13_encrypted_client_hello_xtn));
        rv = tls13_ComputeEchSignal(ss, computed);
        if (rv != SECSuccess) {
            return SECFailure;
        }

        ss->ssl3.hs.echAccepted = !PORT_Memcmp(computed, signal, TLS13_ECH_SIGNAL_LEN);
        if (ss->ssl3.hs.echAccepted) {
            if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
                FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_SERVER_HELLO, illegal_parameter);
                return SECFailure;
            }
            ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ssl_tls13_encrypted_client_hello_xtn;
            PORT_Memcpy(ss->ssl3.hs.client_random, ss->ssl3.hs.client_inner_random, SSL3_RANDOM_LENGTH);
        }
        /* If rejected, leave echHpkeCtx and echPublicName for rejection paths. */
        ssl3_CoalesceEchHandshakeHashes(ss);
        SSL_TRC(50, ("%d: TLS13[%d]: ECH %s accepted by server",
                     SSL_GETPID(), ss->fd, ss->ssl3.hs.echAccepted ? "is" : "is not"));
    }

    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_ech;
    return SECSuccess;
}

static SECStatus
tls13_UnencodeChInner(sslSocket *ss, const SECItem *sidBytes, SECItem **echInner)
{
    SECStatus rv;
    sslReadBuffer outerExtensionsList;
    sslReadBuffer tmpReadBuf;
    sslBuffer unencodedChInner = SSL_BUFFER_EMPTY;
    PRCList *outerCursor;
    PRCList *innerCursor;
    PRBool outerFound;
    PRUint32 xtnsOffset;
    PRUint64 tmp;
    PRUint8 *tmpB;
    PRUint32 tmpLength;
    sslReader chReader = SSL_READER((*echInner)->data, (*echInner)->len);
    PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->ssl3.hs.echOuterExtensions));
    PORT_Assert(PR_CLIST_IS_EMPTY(&ss->ssl3.hs.remoteExtensions));

    /* unencodedChInner := preamble, tmpReadBuf := encoded extensions. */
    rv = tls13_CopyChPreamble(&chReader, sidBytes, &unencodedChInner, &tmpReadBuf);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }

    /* Parse inner extensions into ss->ssl3.hs.remoteExtensions. */
    tmpB = CONST_CAST(PRUint8, tmpReadBuf.buf);
    rv = ssl3_ParseExtensions(ss, &tmpB, &tmpReadBuf.len);
    if (rv != SECSuccess) {
        goto loser; /* malformed, alert sent. */
    }

    /* Exit early if there are no outer_extensions to decompress. */
    if (!ssl3_FindExtension(ss, ssl_tls13_outer_extensions_xtn)) {
        rv = sslBuffer_AppendVariable(&unencodedChInner, tmpReadBuf.buf, tmpReadBuf.len, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        sslBuffer_Clear(&unencodedChInner);
        return SECSuccess;
    }

    /* Save room for uncompressed length. */
    rv = sslBuffer_Skip(&unencodedChInner, 2, &xtnsOffset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* For each inner extension: If not outer_extensions, copy it to the output.
     * Else if outer_extensions, iterate the compressed extension list and append
     * each full extension as contained in CHOuter. Compressed extensions must be
     * contiguous, so decompress at the point at which outer_extensions appears. */
    for (innerCursor = PR_NEXT_LINK(&ss->ssl3.hs.remoteExtensions);
         innerCursor != &ss->ssl3.hs.remoteExtensions;
         innerCursor = PR_NEXT_LINK(innerCursor)) {
        TLSExtension *innerExtension = (TLSExtension *)innerCursor;
        if (innerExtension->type != ssl_tls13_outer_extensions_xtn) {
            rv = sslBuffer_AppendNumber(&unencodedChInner,
                                        innerExtension->type, 2);
            if (rv != SECSuccess) {
                goto loser;
            }
            rv = sslBuffer_AppendVariable(&unencodedChInner,
                                          innerExtension->data.data,
                                          innerExtension->data.len, 2);
            if (rv != SECSuccess) {
                goto loser;
            }
            continue;
        }

        /* Decompress */
        sslReader extensionRdr = SSL_READER(innerExtension->data.data,
                                            innerExtension->data.len);
        rv = sslRead_ReadVariable(&extensionRdr, 1, &outerExtensionsList);
        if (rv != SECSuccess) {
            goto loser;
        }
        if (SSL_READER_REMAINING(&extensionRdr) || (outerExtensionsList.len % 2) != 0) {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
            goto loser;
        }

        sslReader compressedTypes = SSL_READER(outerExtensionsList.buf, outerExtensionsList.len);
        while (SSL_READER_REMAINING(&compressedTypes)) {
            outerFound = PR_FALSE;
            rv = sslRead_ReadNumber(&compressedTypes, 2, &tmp);
            if (rv != SECSuccess) {
                goto loser;
            }
            if (tmp == ssl_tls13_encrypted_client_hello_xtn ||
                tmp == ssl_tls13_outer_extensions_xtn) {
                FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ECH_EXTENSION, illegal_parameter);
                goto loser;
            }
            for (outerCursor = PR_NEXT_LINK(&ss->ssl3.hs.echOuterExtensions);
                 outerCursor != &ss->ssl3.hs.echOuterExtensions;
                 outerCursor = PR_NEXT_LINK(outerCursor)) {
                if (((TLSExtension *)outerCursor)->type == tmp) {
                    outerFound = PR_TRUE;
                    rv = sslBuffer_AppendNumber(&unencodedChInner,
                                                ((TLSExtension *)outerCursor)->type, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    rv = sslBuffer_AppendVariable(&unencodedChInner,
                                                  ((TLSExtension *)outerCursor)->data.data,
                                                  ((TLSExtension *)outerCursor)->data.len, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    break;
                }
            }
            if (!outerFound) {
                FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ECH_EXTENSION, illegal_parameter);
                goto loser;
            }
        }
    }
    ssl3_DestroyRemoteExtensions(&ss->ssl3.hs.echOuterExtensions);
    ssl3_DestroyRemoteExtensions(&ss->ssl3.hs.remoteExtensions);

    /* Correct the message and extensions sizes. */
    rv = sslBuffer_InsertNumber(&unencodedChInner, xtnsOffset,
                                unencodedChInner.len - xtnsOffset - 2, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    tmpB = &unencodedChInner.buf[xtnsOffset];
    tmpLength = unencodedChInner.len - xtnsOffset;
    rv = ssl3_ConsumeHandshakeNumber64(ss, &tmp, 2, &tmpB, &tmpLength);
    if (rv != SECSuccess || tmpLength != tmp) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, internal_error);
        goto loser;
    }

    rv = ssl3_ParseExtensions(ss, &tmpB, &tmpLength);
    if (rv != SECSuccess) {
        goto loser;
    }

    SECITEM_FreeItem(*echInner, PR_FALSE);
    (*echInner)->data = unencodedChInner.buf;
    (*echInner)->len = unencodedChInner.len;
    return SECSuccess;

loser:
    sslBuffer_Clear(&unencodedChInner);
    return SECFailure;
}

SECStatus
tls13_MaybeAcceptEch(sslSocket *ss, const SECItem *sidBytes, const PRUint8 *chOuter,
                     unsigned int chOuterLen, SECItem **chInner)
{
    SECStatus rv;
    SECItem outer = { siBuffer, CONST_CAST(PRUint8, chOuter), chOuterLen };
    SECItem *decryptedChInner = NULL;
    PK11SymKey *echHrrPsk = NULL;
    SECItem hrrCh1ConfigId = { siBuffer, NULL, 0 };
    HpkeKdfId kdf;
    HpkeAeadId aead;
    sslEchConfig *candidate = NULL; /* non-owning */
    TLSExtension *hrrXtn;
    SECItem *configId = ss->ssl3.hs.helloRetry ? &hrrCh1ConfigId : &ss->xtnData.echConfigId;
    if (!ss->xtnData.innerCh.len) {
        return SECSuccess;
    }

    PORT_Assert(ss->xtnData.echSenderPubKey.data);
    PORT_Assert(ss->xtnData.echConfigId.data);
    PORT_Assert(ss->xtnData.echCipherSuite);

    if (ss->ssl3.hs.helloRetry) {
        hrrXtn = ssl3_FindExtension(ss, ssl_tls13_cookie_xtn);
        if (!hrrXtn) {
            /* If the client doesn't echo cookie, we can't decrypt. */
            return SECSuccess;
        }

        rv = tls13_GetEchInfoFromCookie(ss, hrrXtn, &echHrrPsk, &hrrCh1ConfigId);
        if (rv != SECSuccess) {
            /* If we failed due to an issue with the cookie, continue without
             * ECH and let the HRR code handle the problem. */
            goto exit_success;
        }

        /* No CH1 config_id means ECH wasn't advertised in CH1.
         * No CH1 HRR PSK means that ECH was not accepted in CH1, and the
         * HRR was generated off CH1Outer. */
        if (hrrCh1ConfigId.len == 0) {
            FATAL_ERROR(ss, SSL_ERROR_BAD_2ND_CLIENT_HELLO,
                        illegal_parameter);
            goto loser;
        }
        if (!echHrrPsk) {
            goto exit_success;
        }
    }
    kdf = (HpkeKdfId)(ss->xtnData.echCipherSuite & 0xFFFF);
    aead = (HpkeAeadId)(((ss->xtnData.echCipherSuite) >> 16) & 0xFFFF);
    rv = tls13_GetMatchingEchConfig(ss, kdf, aead, configId, &candidate);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (!candidate || candidate->contents.kdfId != kdf ||
        candidate->contents.aeadId != aead) {
        /* Send retry_configs if we have any.
         * This does *not* count as negotiating ECH. */
        rv = ssl3_RegisterExtensionSender(ss, &ss->xtnData,
                                          ssl_tls13_encrypted_client_hello_xtn,
                                          tls13_ServerSendEchXtn);
        goto exit_success;
    }

    rv = tls13_OpenClientHelloInner(ss, &outer, candidate, echHrrPsk, &decryptedChInner);
    if (rv != SECSuccess) {
        if (ss->ssl3.hs.helloRetry) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ESNI_EXTENSION, decrypt_error);
            goto loser;
        } else {
            rv = ssl3_RegisterExtensionSender(ss, &ss->xtnData,
                                              ssl_tls13_encrypted_client_hello_xtn,
                                              tls13_ServerSendEchXtn);
            goto exit_success;
        }
    }
    SSL_TRC(20, ("%d: TLS13[%d]: Successfully opened ECH inner CH",
                 SSL_GETPID(), ss->fd));
    ss->ssl3.hs.echAccepted = PR_TRUE;

    /* Stash the CHOuter extensions. They're not yet handled (only parsed). If
     * the CHInner contains outer_extensions_xtn, we'll need to reference them. */
    ssl3_MoveRemoteExtensions(&ss->ssl3.hs.echOuterExtensions, &ss->ssl3.hs.remoteExtensions);

    rv = tls13_UnencodeChInner(ss, sidBytes, &decryptedChInner);
    if (rv != SECSuccess) {
        SECITEM_FreeItem(decryptedChInner, PR_TRUE);
        goto loser; /* code set */
    }
    *chInner = decryptedChInner;

exit_success:
    PK11_FreeSymKey(echHrrPsk);
    SECITEM_FreeItem(&hrrCh1ConfigId, PR_FALSE);
    return SECSuccess;

loser:
    PK11_FreeSymKey(echHrrPsk);
    SECITEM_FreeItem(&hrrCh1ConfigId, PR_FALSE);
    return SECFailure;
}

SECStatus
tls13_WriteServerEchSignal(sslSocket *ss)
{
    SECStatus rv;
    PRUint8 signal[TLS13_ECH_SIGNAL_LEN];
    rv = tls13_ComputeEchSignal(ss, signal);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PRUint8 *dest = &ss->ssl3.hs.server_random[SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN];
    PORT_Memcpy(dest, signal, TLS13_ECH_SIGNAL_LEN);
    return SECSuccess;
}
