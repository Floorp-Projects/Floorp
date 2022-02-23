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
#include "tls13hashstate.h"
#include "tls13hkdf.h"

extern SECStatus
ssl3_UpdateHandshakeHashesInt(sslSocket *ss, const unsigned char *b,
                              unsigned int l, sslBuffer *transcriptBuf);
extern SECStatus
ssl3_HandleClientHelloPreamble(sslSocket *ss, PRUint8 **b, PRUint32 *length, SECItem *sidBytes,
                               SECItem *cookieBytes, SECItem *suites, SECItem *comps);
extern SECStatus
tls13_DeriveSecret(sslSocket *ss, PK11SymKey *key,
                   const char *label,
                   unsigned int labelLen,
                   const SSL3Hashes *hashes,
                   PK11SymKey **dest,
                   SSLHashType hash);

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

void
tls13_DestroyEchXtnState(sslEchXtnState *state)
{
    if (!state) {
        return;
    }
    SECITEM_FreeItem(&state->innerCh, PR_FALSE);
    SECITEM_FreeItem(&state->senderPubKey, PR_FALSE);
    SECITEM_FreeItem(&state->retryConfigs, PR_FALSE);
    PORT_ZFree(state, sizeof(*state));
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
        newConfig->contents.configId = config->contents.configId;
        newConfig->contents.kemId = config->contents.kemId;
        newConfig->contents.kdfId = config->contents.kdfId;
        newConfig->contents.aeadId = config->contents.aeadId;
        newConfig->contents.maxNameLen = config->contents.maxNameLen;
        newConfig->version = config->version;
        PR_APPEND_LINK(&newConfig->link, configs);
    }
    return SECSuccess;

loser:
    tls13_DestroyEchConfig(newConfig);
    tls13_DestroyEchConfigs(configs);
    return SECFailure;
}

/*
 * struct {
 *     HpkeKdfId kdf_id;
 *     HpkeAeadId aead_id;
 * } HpkeSymmetricCipherSuite;
 *
 * struct {
 *     uint8 config_id;
 *     HpkeKemId kem_id;
 *     HpkePublicKey public_key;
 *     HpkeSymmetricCipherSuite cipher_suites<4..2^16-4>;
 * } HpkeKeyConfig;
 *
 * struct {
 *     HpkeKeyConfig key_config;
 *     uint16 maximum_name_length;
 *     opaque public_name<1..2^16-1>;
 *     Extension extensions<0..2^16-1>;
 * } ECHConfigContents;
 *
 * struct {
 *     uint16 version;
 *     uint16 length;
 *     select (ECHConfig.version) {
 *       case 0xfe0a: ECHConfigContents contents;
 *     }
 * } ECHConfig;
 */
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

    /* HpkeKeyConfig key_config */
    /* uint8 config_id */
    rv = sslRead_ReadNumber(&configReader, 1, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    contents.configId = tmpn;

    /* HpkeKemId kem_id */
    rv = sslRead_ReadNumber(&configReader, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    contents.kemId = tmpn;

    /* HpkePublicKey public_key */
    rv = sslRead_ReadVariable(&configReader, 2, &tmpBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECITEM_MakeItem(NULL, &contents.publicKey, (PRUint8 *)tmpBuf.buf, tmpBuf.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* HpkeSymmetricCipherSuite cipher_suites<4..2^16-4> */
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
        /* HpkeKdfId kdf_id */
        rv = sslRead_ReadNumber(&suiteReader, 2, &tmpn);
        if (rv != SECSuccess) {
            goto loser;
        }
        /* HpkeAeadId aead_id */
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

    /* uint16 maximum_name_length */
    rv = sslRead_ReadNumber(&configReader, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    contents.maxNameLen = (PRUint16)tmpn;

    /* opaque public_name<1..2^16-1> */
    rv = sslRead_ReadVariable(&configReader, 2, &tmpBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (tmpBuf.len == 0) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
        goto loser;
    }
    if (!tls13_IsLDH(tmpBuf.buf, tmpBuf.len) ||
        tls13_IsIp(tmpBuf.buf, tmpBuf.len)) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
        goto loser;
    }

    contents.publicName = PORT_ZAlloc(tmpBuf.len + 1);
    if (!contents.publicName) {
        goto loser;
    }
    PORT_Memcpy(contents.publicName, (PRUint8 *)tmpBuf.buf, tmpBuf.len);

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

/* Decode an ECHConfigList struct and store each ECHConfig
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
        /* uint16 version */
        rv = sslRead_ReadNumber(&configsReader, 2, &version);
        if (rv != SECSuccess) {
            goto loser;
        }
        /* uint16 length */
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

/* Encode an ECHConfigList structure. We only create one config, and as the
 * primary use for this function is to generate test inputs, we don't
 * validate against what HPKE and libssl can actually support. */
SECStatus
SSLExp_EncodeEchConfigId(PRUint8 configId, const char *publicName, unsigned int maxNameLen,
                         HpkeKemId kemId, const SECKEYPublicKey *pubKey,
                         const HpkeSymmetricSuite *hpkeSuites, unsigned int hpkeSuiteCount,
                         PRUint8 *out, unsigned int *outlen, unsigned int maxlen)
{
    SECStatus rv;
    unsigned int savedOffset;
    unsigned int len;
    sslBuffer b = SSL_BUFFER_EMPTY;
    PRUint8 tmpBuf[66]; // Large enough for an EC public key, currently only X25519.
    unsigned int tmpLen;

    if (!publicName || !hpkeSuites || hpkeSuiteCount == 0 ||
        !pubKey || maxNameLen == 0 || !out || !outlen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* ECHConfig ECHConfigList<1..2^16-1>; */
    rv = sslBuffer_Skip(&b, 2, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }

    /*
     * struct {
     *     uint16 version;
     *     uint16 length;
     *     select (ECHConfig.version) {
     *       case 0xfe0a: ECHConfigContents contents;
     *     }
     * } ECHConfig;
    */
    rv = sslBuffer_AppendNumber(&b, TLS13_ECH_VERSION, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_Skip(&b, 2, &savedOffset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /*
     * struct {
     *     uint8 config_id;
     *     HpkeKemId kem_id;
     *     HpkePublicKey public_key;
     *     HpkeSymmetricCipherSuite cipher_suites<4..2^16-4>;
     * } HpkeKeyConfig;
     */
    rv = sslBuffer_AppendNumber(&b, configId, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(&b, kemId, 2);
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

    rv = sslBuffer_AppendNumber(&b, hpkeSuiteCount * 4, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    for (unsigned int i = 0; i < hpkeSuiteCount; i++) {
        rv = sslBuffer_AppendNumber(&b, hpkeSuites[i].kdfId, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = sslBuffer_AppendNumber(&b, hpkeSuites[i].aeadId, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    /*
     * struct {
     *     HpkeKeyConfig key_config;
     *     uint16 maximum_name_length;
     *     opaque public_name<1..2^16-1>;
     *     Extension extensions<0..2^16-1>;
     * } ECHConfigContents;
     */
    rv = sslBuffer_AppendNumber(&b, maxNameLen, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    len = PORT_Strlen(publicName);
    if (len > 0xffff) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    rv = sslBuffer_AppendVariable(&b, (const PRUint8 *)publicName, len, 2);
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

    /* We don't distinguish between "handshake completed
     * without retry configs", and "handshake not completed".
     * An application should only call this after receiving a
     * RETRY_WITH_ECH error code, which implies retry_configs. */
    if (!ss->xtnData.ech || !ss->xtnData.ech->retryConfigsValid) {
        PORT_SetError(SSL_ERROR_HANDSHAKE_NOT_COMPLETED);
        return SECFailure;
    }

    /* May be empty. */
    rv = SECITEM_CopyItem(NULL, &out, &ss->xtnData.ech->retryConfigs);
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

    SECKEY_DestroyPrivateKey(ss->echPrivKey);
    ss->echPrivKey = NULL;
    SECKEY_DestroyPublicKey(ss->echPubKey);
    ss->echPubKey = NULL;
    tls13_DestroyEchConfigs(&ss->echConfigs);

    /* Also remove any retry_configs and handshake context. */
    if (ss->xtnData.ech && ss->xtnData.ech->retryConfigs.len) {
        SECITEM_FreeItem(&ss->xtnData.ech->retryConfigs, PR_FALSE);
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
}

/* Client enable. For now, we'll use the first
 * compatible config (server preference). */
SECStatus
SSLExp_SetClientEchConfigs(PRFileDesc *fd,
                           const PRUint8 *echConfigs,
                           unsigned int echConfigsLen)
{
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
    sslEchConfig *cfg = NULL;

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
            if (!ss->ssl3.hs.echHpkeCtx || !ss->ssl3.hs.echPublicName) {
                FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
                return SECFailure;
            }
            /* Nothing else to do. */
            return SECSuccess;
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

    PRINT_BUF(50, (ss, "Info", hpkeInfo.data, hpkeInfo.len));

    /* Setup with an ephemeral sender keypair. */
    rv = PK11_HPKE_SetupS(cx, NULL, NULL, pkR, &hpkeInfo);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = ssl3_GetNewRandom(ss->ssl3.hs.client_inner_random);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }

    /* If ECH is rejected, the application will use SSLChannelInfo
     * to fetch this field and perform cert chain verification. */
    ss->ssl3.hs.echPublicName = PORT_Strdup(cfg->contents.publicName);
    if (!ss->ssl3.hs.echPublicName) {
        goto loser;
    }

    ss->ssl3.hs.echHpkeCtx = cx;
    SECKEY_DestroyPublicKey(pkR);
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    return SECSuccess;

loser:
    PK11_HPKE_DestroyContext(cx, PR_TRUE);
    SECKEY_DestroyPublicKey(pkR);
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    PORT_Assert(PORT_GetError() != 0);
    return SECFailure;
}

/*
 *  enum {
 *     encrypted_client_hello(0xfe0a), (65535)
 *  } ExtensionType;
 *
 *  struct {
 *      HpkeKdfId kdf_id;
 *      HpkeAeadId aead_id;
 *  } HpkeSymmetricCipherSuite;
 *  struct {
 *     HpkeSymmetricCipherSuite cipher_suite;
 *     uint8 config_id;
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
    PRINT_BUF(50, (ss, "aad", outerAAD->buf, outerAAD->len));
    PRINT_BUF(50, (ss, "inner", chInner->buf, chInner->len));

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
    PRINT_BUF(50, (ss, "cipher", chCt->data, chCt->len));
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

    rv = sslBuffer_AppendNumber(chInner, cfg->contents.configId, 1);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (!ss->ssl3.hs.helloRetry) {
        rv = sslBuffer_AppendVariable(chInner, hpkeEnc->data, hpkeEnc->len, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
    } else {
        /* |enc| is empty. */
        rv = sslBuffer_AppendNumber(chInner, 0, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
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
tls13_GetMatchingEchConfigs(const sslSocket *ss, HpkeKdfId kdf, HpkeAeadId aead,
                            const PRUint8 configId, const sslEchConfig *cur, sslEchConfig **next)
{
    SSL_TRC(50, ("%d: TLS13[%d]: GetMatchingEchConfig %d",
                 SSL_GETPID(), ss->fd, configId));

    /* If |cur|, resume the search at that node, else the list head. */
    for (PRCList *cur_p = cur ? ((PRCList *)cur)->next : PR_LIST_HEAD(&ss->echConfigs);
         cur_p != &ss->echConfigs;
         cur_p = PR_NEXT_LINK(cur_p)) {
        sslEchConfig *echConfig = (sslEchConfig *)cur_p;
        if (echConfig->contents.configId == configId &&
            echConfig->contents.aeadId == aead &&
            echConfig->contents.kdfId == kdf) {
            *next = echConfig;
            return SECSuccess;
        }
    }

    *next = NULL;
    return SECSuccess;
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

/*
 *   struct {
 *      HpkeSymmetricCipherSuite cipher_suite;  // kdfid_, aead_id
 *      uint8 config_id;
 *      opaque enc<1..2^16-1>;
 *      opaque outer_hello<1..2^24-1>;
 *   } ClientHelloOuterAAD;
 */
static SECStatus
tls13_MakeChOuterAAD(sslSocket *ss, const SECItem *outer, SECItem *outerAAD)
{
    SECStatus rv;
    sslBuffer aad = SSL_BUFFER_EMPTY;
    sslReadBuffer aadXtns = { 0 };
    sslReader chReader = SSL_READER(outer->data, outer->len);
    PRUint64 tmpn;
    sslReadBuffer tmpvar = { 0 };
    unsigned int offset;
    unsigned int savedOffset;
    PORT_Assert(ss->xtnData.ech);

    rv = sslBuffer_AppendNumber(&aad, ss->xtnData.ech->kdfId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(&aad, ss->xtnData.ech->aeadId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(&aad, ss->xtnData.ech->configId, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (!ss->ssl3.hs.helloRetry) {
        rv = sslBuffer_AppendVariable(&aad, ss->xtnData.ech->senderPubKey.data,
                                      ss->xtnData.ech->senderPubKey.len, 2);
    } else {
        /* |enc| is empty for HelloRetryRequest. */
        rv = sslBuffer_AppendNumber(&aad, 0, 2);
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Skip 3 bytes for the CHOuter length. */
    rv = sslBuffer_Skip(&aad, 3, &savedOffset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* aad := preamble, aadXtn := extensions */
    rv = tls13_CopyChPreamble(&chReader, NULL, &aad, &aadXtns);
    if (rv != SECSuccess) {
        goto loser;
    }
    sslReader xtnsReader = SSL_READER(aadXtns.buf, aadXtns.len);

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

    rv = sslBuffer_InsertLength(&aad, offset, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_InsertLength(&aad, savedOffset, 3);
    if (rv != SECSuccess) {
        goto loser;
    }

    outerAAD->data = aad.buf;
    outerAAD->len = aad.len;
    return SECSuccess;

loser:
    sslBuffer_Clear(&aad);
    return SECFailure;
}

SECStatus
tls13_OpenClientHelloInner(sslSocket *ss, const SECItem *outer, const SECItem *outerAAD, sslEchConfig *cfg, SECItem **chInner)
{
    SECStatus rv;
    HpkeContext *cx = NULL;
    SECItem *decryptedChInner = NULL;
    SECItem hpkeInfo = { siBuffer, NULL, 0 };
    SSL_TRC(50, ("%d: TLS13[%d]: Server opening ECH Inner%s", SSL_GETPID(),
                 ss->fd, ss->ssl3.hs.helloRetry ? " after HRR" : ""));

    if (!ss->ssl3.hs.helloRetry) {
        PORT_Assert(!ss->ssl3.hs.echHpkeCtx);
        cx = PK11_HPKE_NewContext(cfg->contents.kemId, cfg->contents.kdfId,
                                  cfg->contents.aeadId, NULL, NULL);
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
                              &ss->xtnData.ech->senderPubKey, &hpkeInfo);
        if (rv != SECSuccess) {
            goto loser; /* code set */
        }
    } else {
        PORT_Assert(ss->ssl3.hs.echHpkeCtx);
        cx = ss->ssl3.hs.echHpkeCtx;
    }

#ifndef UNSAFE_FUZZER_MODE
    rv = PK11_HPKE_Open(cx, outerAAD, &ss->xtnData.ech->innerCh, &decryptedChInner);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }
#else
    rv = SECITEM_CopyItem(NULL, decryptedChInner, &ss->xtnData.ech->innerCh);
    if (rv != SECSuccess) {
        goto loser;
    }
    decryptedChInner->len -= 16; /* Fake tag */
#endif

    /* Stash the context, we may need it for HRR. */
    ss->ssl3.hs.echHpkeCtx = cx;
    *chInner = decryptedChInner;
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    return SECSuccess;

loser:
    SECITEM_FreeItem(decryptedChInner, PR_TRUE);
    SECITEM_FreeItem(&hpkeInfo, PR_FALSE);
    if (cx != ss->ssl3.hs.echHpkeCtx) {
        /* Don't double-free if it's already global. */
        PK11_HPKE_DestroyContext(cx, PR_TRUE);
    }
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
                 SSL_GETPID(), ss->fd, compress ? "with" : "without"));

    /* When offering the "encrypted_client_hello" extension in its
     * ClientHelloOuter, the client MUST also offer an empty
     * "encrypted_client_hello" extension in its ClientHelloInner. */
    rv = sslBuffer_AppendNumber(chInnerXtns, ssl_tls13_ech_is_inner_xtn, 2);
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
    sslBuffer aad = SSL_BUFFER_EMPTY;
    unsigned int encodedChLen;
    unsigned int preambleLen;
    const SECItem *hpkeEnc = NULL;
    unsigned int savedOffset;

    SSL_TRC(50, ("%d: TLS13[%d]: Constructing ECH inner", SSL_GETPID(), ss->fd));

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

    rv = ssl3_UpdateHandshakeHashesInt(ss, chInner.buf, chInner.len,
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

    rv = tls13_EncodeClientHelloInner(ss, &chInner, &chInnerXtns, &encodedChInner);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Pad the outer prior to appending ECH (for the AAD).
     * Encoded extension size is (echCipherSuite + enc + configId + payload + tag).
     * Post-encryption, we'll assert that this was correct. */
    encodedChLen = 4 + 1 + 2 + 2 + encodedChInner.len + 16;
    if (!ss->ssl3.hs.helloRetry) {
        encodedChLen += 32; /* enc */
    }
    rv = ssl_InsertPaddingExtension(ss, chOuter->len + encodedChLen, chOuterXtnsBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->echConfigs));
    sslEchConfig *cfg = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);
    rv = sslBuffer_AppendNumber(&aad, cfg->contents.kdfId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(&aad, cfg->contents.aeadId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(&aad, cfg->contents.configId, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (!ss->ssl3.hs.helloRetry) {
        hpkeEnc = PK11_HPKE_GetEncapPubKey(ss->ssl3.hs.echHpkeCtx);
        if (!hpkeEnc) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            goto loser;
        }
        rv = sslBuffer_AppendVariable(&aad, hpkeEnc->data, hpkeEnc->len, 2);
    } else {
        /* 2B for empty enc length. */
        rv = sslBuffer_AppendNumber(&aad, 0, 2);
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_Skip(&aad, 3, &savedOffset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Skip the handshake header. */
    PORT_Assert(chOuter->len > 4);
    rv = sslBuffer_Append(&aad, &chOuter->buf[4], chOuter->len - 4);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendBufferVariable(&aad, chOuterXtnsBuf, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_InsertLength(&aad, savedOffset, 3);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Insert the encrypted_client_hello xtn and coalesce. */
    rv = tls13_EncryptClientHello(ss, &aad, &encodedChInner);
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
    sslBuffer_Clear(&chInner);
    sslBuffer_Clear(&encodedChInner);
    sslBuffer_Clear(&chInnerXtns);
    sslBuffer_Clear(&pskXtn);
    sslBuffer_Clear(&aad);
    return SECSuccess;

loser:
    sslBuffer_Clear(&chInner);
    sslBuffer_Clear(&encodedChInner);
    sslBuffer_Clear(&chInnerXtns);
    sslBuffer_Clear(&pskXtn);
    sslBuffer_Clear(&aad);
    PORT_Assert(PORT_GetError() != 0);
    return SECFailure;
}

/* Compute the ECH signal using the transcript (up to, excluding) Server Hello.
 * We'll append an artificial SH (ServerHelloECHConf). The server sources
 * this transcript prefix from ss->ssl3.hs.messages, as it never uses
 * ss->ssl3.hs.echInnerMessages. The client uses the inner transcript, echInnerMessages. */
static SECStatus
tls13_ComputeEchSignal(sslSocket *ss, const PRUint8 *sh, unsigned int shLen, PRUint8 *out)
{
    SECStatus rv;
    PK11SymKey *confirmationKey = NULL;
    sslBuffer confMsgs = SSL_BUFFER_EMPTY;
    sslBuffer *chSource = ss->sec.isServer ? &ss->ssl3.hs.messages : &ss->ssl3.hs.echInnerMessages;
    SSL3Hashes hashes;
    SECItem *confirmationBytes;
    unsigned int offset = sizeof(SSL3ProtocolVersion) +
                          SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN;
    PORT_Assert(sh && shLen > offset);
    PORT_Assert(TLS13_ECH_SIGNAL_LEN <= SSL3_RANDOM_LENGTH);

    rv = sslBuffer_AppendBuffer(&confMsgs, chSource);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Re-create the message header. */
    rv = sslBuffer_AppendNumber(&confMsgs, ssl_hs_server_hello, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(&confMsgs, shLen, 3);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Copy the version and 24B of server_random. */
    rv = sslBuffer_Append(&confMsgs, sh, offset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Zero the signal placeholder. */
    rv = sslBuffer_AppendNumber(&confMsgs, 0, TLS13_ECH_SIGNAL_LEN);
    if (rv != SECSuccess) {
        goto loser;
    }
    offset += TLS13_ECH_SIGNAL_LEN;

    /* Use the remainder of SH. */
    rv = sslBuffer_Append(&confMsgs, &sh[offset], shLen - offset);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_ComputeHash(ss, &hashes, confMsgs.buf, confMsgs.len,
                           tls13_GetHash(ss));
    if (rv != SECSuccess) {
        goto loser;
    }

    /*  accept_confirmation =
     *          Derive-Secret(Handshake Secret,
     *                        "ech accept confirmation",
     *                        ClientHelloInner...ServerHelloECHConf)
     */
    rv = tls13_DeriveSecret(ss, ss->ssl3.hs.currentSecret,
                            kHkdfInfoEchConfirm, strlen(kHkdfInfoEchConfirm),
                            &hashes, &confirmationKey, tls13_GetHash(ss));
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = PK11_ExtractKeyValue(confirmationKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    confirmationBytes = PK11_GetKeyData(confirmationKey);
    if (!confirmationBytes) {
        rv = SECFailure;
        PORT_SetError(SSL_ERROR_ECH_FAILED);
        goto loser;
    }
    if (confirmationBytes->len < TLS13_ECH_SIGNAL_LEN) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        goto loser;
    }
    SSL_TRC(50, ("%d: TLS13[%d]: %s computed ECH signal", SSL_GETPID(), ss->fd, SSL_ROLE(ss)));
    PRINT_BUF(50, (ss, "", out, TLS13_ECH_SIGNAL_LEN));

    PORT_Memcpy(out, confirmationBytes->data, TLS13_ECH_SIGNAL_LEN);
    PK11_FreeSymKey(confirmationKey);
    sslBuffer_Clear(&confMsgs);
    sslBuffer_Clear(&ss->ssl3.hs.messages);
    sslBuffer_Clear(&ss->ssl3.hs.echInnerMessages);
    return SECSuccess;

loser:
    PK11_FreeSymKey(confirmationKey);
    sslBuffer_Clear(&confMsgs);
    sslBuffer_Clear(&ss->ssl3.hs.messages);
    sslBuffer_Clear(&ss->ssl3.hs.echInnerMessages);
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
    /* 1B aead determinant (don't send), 1B config_id, 32B enc, payload */
    const int kNonPayloadLen = 34;

    if (!ss->opt.enableTls13GreaseEch || ss->ssl3.hs.echHpkeCtx) {
        return SECSuccess;
    }

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3 ||
        IS_DTLS(ss)) {
        return SECSuccess;
    }

    /* In draft-09, CH2 sends exactly the same GREASE ECH extension. */
    if (ss->ssl3.hs.helloRetry) {
        return ssl3_EmplaceExtension(ss, buf, ssl_tls13_encrypted_client_hello_xtn,
                                     ss->ssl3.hs.greaseEchBuf.buf,
                                     ss->ssl3.hs.greaseEchBuf.len, PR_TRUE);
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
                                       CKA_DERIVE, kNonPayloadLen + payloadLen,
                                       CKF_VERIFY);
    if (!derivedData) {
        goto loser;
    }

    rv = PK11_ExtractKeyValue(derivedData);
    if (rv != SECSuccess) {
        goto loser;
    }

    rawData = PK11_GetKeyData(derivedData);
    if (!rawData) {
        goto loser;
    }
    PORT_Assert(rawData->len == kNonPayloadLen + payloadLen);

    /* struct {
       HpkeSymmetricCipherSuite cipher_suite; // kdf_id, aead_id
       PRUint8 config_id;
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

    /* config_id */
    rv = sslBuffer_AppendNumber(&greaseBuf, rawData->data[1], 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* enc len is fixed 32B for X25519. */
    rv = sslBuffer_AppendVariable(&greaseBuf, &rawData->data[2], 32, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendVariable(&greaseBuf, &rawData->data[kNonPayloadLen], payloadLen, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Mark ECH as advertised so that we can validate any response.
     * We'll use echHpkeCtx to determine if we sent real or GREASE ECH. */
    rv = ssl3_EmplaceExtension(ss, buf, ssl_tls13_encrypted_client_hello_xtn,
                               greaseBuf.buf, greaseBuf.len, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Stash the GREASE ECH extension - in the case of HRR, CH2 must echo it. */
    ss->ssl3.hs.greaseEchBuf = greaseBuf;

    sslBuffer_Clear(&chInnerXtns);
    PK11_FreeSymKey(hmacPrk);
    PK11_FreeSymKey(derivedData);
    PK11_FreeSlot(slot);
    return SECSuccess;

loser:
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
        echExtension = ssl3_FindExtension(ss, ssl_tls13_ech_is_inner_xtn);
        if (!echExtension) {
            FATAL_ERROR(ss, SSL_ERROR_MISSING_ECH_EXTENSION, illegal_parameter);
            goto loser;
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
    PORT_Assert(PORT_GetError() != 0);
    return SECFailure;
}

SECStatus
tls13_MaybeHandleEchSignal(sslSocket *ss, const PRUint8 *sh, PRUint32 shLen)
{
    SECStatus rv;
    PRUint8 computed[TLS13_ECH_SIGNAL_LEN];
    const PRUint8 *signal = &ss->ssl3.hs.server_random[SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN];
    PORT_Assert(!ss->sec.isServer);

    /* If !echHpkeCtx, we either didn't advertise or sent GREASE ECH. */
    if (!ss->ssl3.hs.echHpkeCtx) {
        ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_ech;
        return SECSuccess;
    }

    PORT_Assert(ssl3_ExtensionAdvertised(ss, ssl_tls13_encrypted_client_hello_xtn));
    rv = tls13_ComputeEchSignal(ss, sh, shLen, computed);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    ss->ssl3.hs.echAccepted = !PORT_Memcmp(computed, signal, TLS13_ECH_SIGNAL_LEN);
    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_ech;
    if (ss->ssl3.hs.echAccepted) {
        if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_SERVER_HELLO, illegal_parameter);
            return SECFailure;
        }
        /* |enc| must not be included in CH2.ClientECH. */
        if (ss->ssl3.hs.helloRetry && ss->sec.isServer &&
            ss->xtnData.ech->senderPubKey.len) {
            ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
            PORT_SetError(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
            return SECFailure;
        }

        ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ssl_tls13_encrypted_client_hello_xtn;
        PORT_Memcpy(ss->ssl3.hs.client_random, ss->ssl3.hs.client_inner_random, SSL3_RANDOM_LENGTH);
    }
    /* If rejected, leave echHpkeCtx and echPublicName for rejection paths. */
    ssl3_CoalesceEchHandshakeHashes(ss);
    SSL_TRC(50, ("%d: TLS13[%d]: ECH %s accepted by server",
                 SSL_GETPID(), ss->fd, ss->ssl3.hs.echAccepted ? "is" : "is not"));
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
    SECItem outerAAD = { siBuffer, NULL, 0 };
    SECItem cookieData = { siBuffer, NULL, 0 };
    sslEchConfig *candidate = NULL; /* non-owning */
    TLSExtension *hrrXtn;

    if (!ss->xtnData.ech) {
        return SECSuccess;
    }

    PORT_Assert(ss->xtnData.ech->innerCh.data);

    if (ss->ssl3.hs.helloRetry) {
        PORT_Assert(!ss->ssl3.hs.echHpkeCtx);
        hrrXtn = ssl3_FindExtension(ss, ssl_tls13_cookie_xtn);
        if (!hrrXtn) {
            /* If the client doesn't echo cookie, we can't decrypt. */
            return SECSuccess;
        }

        PORT_Assert(!ss->ssl3.hs.echHpkeCtx);

        PRUint8 *tmp = hrrXtn->data.data;
        PRUint32 len = hrrXtn->data.len;
        rv = ssl3_ExtConsumeHandshakeVariable(ss, &cookieData, 2,
                                              &tmp, &len);
        if (rv != SECSuccess) {
            return SECFailure;
        }

        /* Extract ECH info without restoring hash state. If there's
         * something wrong with the cookie, continue without ECH
         * and let HRR code handle the problem. */
        HpkeContext *ch1EchHpkeCtx = NULL;
        PRUint8 echConfigId;
        HpkeKdfId echKdfId;
        HpkeAeadId echAeadId;
        rv = tls13_HandleHrrCookie(ss, cookieData.data, cookieData.len,
                                   NULL, NULL, NULL, &echKdfId, &echAeadId,
                                   &echConfigId, &ch1EchHpkeCtx, PR_FALSE);
        if (rv != SECSuccess) {
            return SECSuccess;
        }

        ss->ssl3.hs.echHpkeCtx = ch1EchHpkeCtx;

        if (echConfigId != ss->xtnData.ech->configId ||
            echKdfId != ss->xtnData.ech->kdfId ||
            echAeadId != ss->xtnData.ech->aeadId) {
            FATAL_ERROR(ss, SSL_ERROR_BAD_2ND_CLIENT_HELLO,
                        illegal_parameter);
            return SECFailure;
        }

        if (!ss->ssl3.hs.echHpkeCtx) {
            return SECSuccess;
        }
    }

    /* Cookie data was good, proceed with ECH. */
    rv = tls13_GetMatchingEchConfigs(ss, ss->xtnData.ech->kdfId, ss->xtnData.ech->aeadId,
                                     ss->xtnData.ech->configId, candidate, &candidate);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    if (candidate) {
        rv = tls13_MakeChOuterAAD(ss, &outer, &outerAAD);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }

    while (candidate) {
        rv = tls13_OpenClientHelloInner(ss, &outer, &outerAAD, candidate, &decryptedChInner);
        if (rv != SECSuccess) {
            /* Get the next matching config */
            rv = tls13_GetMatchingEchConfigs(ss, ss->xtnData.ech->kdfId, ss->xtnData.ech->aeadId,
                                             ss->xtnData.ech->configId, candidate, &candidate);
            if (rv != SECSuccess) {
                FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
                SECITEM_FreeItem(&outerAAD, PR_FALSE);
                return SECFailure;
            }
            continue;
        }
        break;
    }
    SECITEM_FreeItem(&outerAAD, PR_FALSE);

    if (rv != SECSuccess || !decryptedChInner) {
        if (ss->ssl3.hs.helloRetry) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ECH_EXTENSION, decrypt_error);
            return SECFailure;
        } else {
            /* Send retry_configs (if we have any) when we fail to decrypt or
            * found no candidates. This does *not* count as negotiating ECH. */
            return ssl3_RegisterExtensionSender(ss, &ss->xtnData,
                                                ssl_tls13_encrypted_client_hello_xtn,
                                                tls13_ServerSendEchXtn);
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
        return SECFailure; /* code set */
    }
    *chInner = decryptedChInner;
    return SECSuccess;
}

SECStatus
tls13_WriteServerEchSignal(sslSocket *ss, PRUint8 *sh, unsigned int shLen)
{
    SECStatus rv;
    PRUint8 signal[TLS13_ECH_SIGNAL_LEN];
    PRUint8 *msg_random = &sh[sizeof(SSL3ProtocolVersion)];

    PORT_Assert(shLen > sizeof(SSL3ProtocolVersion) + SSL3_RANDOM_LENGTH);
    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);

    rv = tls13_ComputeEchSignal(ss, sh, shLen, signal);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PRUint8 *dest = &msg_random[SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN];
    PORT_Memcpy(dest, signal, TLS13_ECH_SIGNAL_LEN);

    /* Keep the socket copy consistent. */
    PORT_Assert(0 == memcmp(msg_random, &ss->ssl3.hs.server_random, SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN));
    dest = &ss->ssl3.hs.server_random[SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN];
    PORT_Memcpy(dest, signal, TLS13_ECH_SIGNAL_LEN);

    return SECSuccess;
}
