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

PRBool
tls13_Debug_CheckXtnBegins(const PRUint8 *start, const PRUint16 xtnType)
{
#ifdef DEBUG
    SECStatus rv;
    sslReader ext_reader = SSL_READER(start, 2);
    PRUint64 extension_number;
    rv = sslRead_ReadNumber(&ext_reader, 2, &extension_number);
    return ((rv == SECSuccess) && (extension_number == xtnType));
#else
    return PR_TRUE;
#endif
}

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
 *       case 0xfe0d: ECHConfigContents contents;
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
    PRBool unsupportedMandatoryXtn = PR_FALSE;

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

    /* uint8 maximum_name_length */
    rv = sslRead_ReadNumber(&configReader, 1, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    contents.maxNameLen = (PRUint8)tmpn;

    /* opaque public_name<1..2^16-1> */
    rv = sslRead_ReadVariable(&configReader, 1, &tmpBuf);
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

        /* Clients MUST parse the extension list and check for unsupported
         * mandatory extensions.  If an unsupported mandatory extension is
         * present, clients MUST ignore the ECHConfig
         * [draft-ietf-tls-esni, Section 4.2]. */
        if (tmpn & (1 << 15)) {
            unsupportedMandatoryXtn = PR_TRUE;
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

    /* If the ciphersuites were compatible AND if NO unsupported mandatory
     * extensions were found set the outparam. Return success either way if the
     * config was well-formed. */
    if (hasValidSuite && !unsupportedMandatoryXtn) {
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
    SSL_TRC(100, ("Read EchConfig list of size %u", SSL_READER_REMAINING(&rdr)));
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
     *       case 0xfe0d: ECHConfigContents contents;
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
     *     uint8 maximum_name_length;
     *     opaque public_name<1..255>;
     *     Extension extensions<0..2^16-1>;
     * } ECHConfigContents;
     */
    rv = sslBuffer_AppendNumber(&b, maxNameLen, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    len = PORT_Strlen(publicName);
    if (len > 0xff) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    rv = sslBuffer_AppendVariable(&b, (const PRUint8 *)publicName, len, 1);
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
 * outerAAD - The associated data for the AEAD (the entire client hello with the ECH payload zeroed)
 * chInner - The plaintext which will be encrypted (the ClientHelloInner plus padding)
 * echPayload - Output location. A buffer containing all-zeroes of at least chInner->len + TLS13_ECH_AEAD_TAG_LEN bytes.
 *
 * echPayload may point into outerAAD to avoid the need to duplicate the ClientHelloOuter buffer.
 */
static SECStatus
tls13_EncryptClientHello(sslSocket *ss, SECItem *aadItem, const sslBuffer *chInner, PRUint8 *echPayload)
{
    SECStatus rv;
    SECItem chPt = { siBuffer, chInner->buf, chInner->len };
    SECItem *chCt = NULL;

    PRINT_BUF(50, (ss, "aad for ECH Encrypt", aadItem->data, aadItem->len));
    PRINT_BUF(50, (ss, "plaintext for ECH Encrypt", chInner->buf, chInner->len));

#ifndef UNSAFE_FUZZER_MODE
    rv = PK11_HPKE_Seal(ss->ssl3.hs.echHpkeCtx, aadItem, &chPt, &chCt);
    if (rv != SECSuccess) {
        goto loser;
    }
    PRINT_BUF(50, (ss, "ciphertext from ECH Encrypt", chCt->data, chCt->len));
#else
    /* Fake a tag. */
    SECITEM_AllocItem(NULL, chCt, chPt.len + TLS13_ECH_AEAD_TAG_LEN);
    if (!chCt) {
        goto loser;
    }
    PORT_Memcpy(chCt->data, chPt.data, chPt.len);
#endif

#ifdef DEBUG
    /* When encrypting in-place, the payload is part of the AAD and must be zeroed. */
    PRUint8 val = 0;
    for (int i = 0; i < chCt->len; i++) {
        val |= *(echPayload + i);
    }
    PRINT_BUF(100, (ss, "Empty Placeholder for output of ECH Encryption", echPayload, chCt->len));
    PR_ASSERT(val == 0);
#endif

    PORT_Memcpy(echPayload, chCt->data, chCt->len);
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
tls13_CopyChPreamble(sslSocket *ss, sslReader *reader, const SECItem *explicitSid, sslBuffer *writer, sslReadBuffer *extensions)
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

    /* padding (optional) */
    sslReadBuffer padding;
    rv = sslRead_Read(reader, SSL_READER_REMAINING(reader), &padding);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PRUint8 result = 0;
    for (int i = 0; i < padding.len; i++) {
        result |= padding.buf[i];
    }
    if (result) {
        SSL_TRC(50, ("%d: TLS13: Invalid ECH ClientHelloInner padding decoded", SSL_GETPID()));
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ECH_EXTENSION, illegal_parameter);
        return SECFailure;
    }
    return SECSuccess;
}

/*
 * The ClientHelloOuterAAD is a serialized ClientHello structure, defined in
 * Section 4.1.2 of [RFC8446], which matches the ClientHelloOuter except the
 * payload field of the "encrypted_client_hello" is replaced with a byte
 * string of the same length but whose contents are zeros. This value does
 * not include the four-byte header from the Handshake structure.
 */
static SECStatus
tls13_ServerMakeChOuterAAD(sslSocket *ss, const PRUint8 *outerCh, unsigned int outerChLen, SECItem *outerAAD)
{
    SECStatus rv;
    sslBuffer aad = SSL_BUFFER_EMPTY;
    const unsigned int echPayloadLen = ss->xtnData.ech->innerCh.len;               /* Length of incoming payload */
    const unsigned int echPayloadOffset = ss->xtnData.ech->payloadStart - outerCh; /* Offset from start of CHO */

    PORT_Assert(outerChLen > echPayloadLen);
    PORT_Assert(echPayloadOffset + echPayloadLen <= outerChLen);
    PORT_Assert(ss->sec.isServer);
    PORT_Assert(ss->xtnData.ech);

#ifdef DEBUG
    /* Safety check that payload length pointed to by offset matches expected length */
    sslReader echXtnReader = SSL_READER(outerCh + echPayloadOffset - 2, 2);
    PRUint64 parsedXtnSize;
    rv = sslRead_ReadNumber(&echXtnReader, 2, &parsedXtnSize);
    PR_ASSERT(rv == SECSuccess);
    PR_ASSERT(parsedXtnSize == echPayloadLen);
#endif

    rv = sslBuffer_Append(&aad, outerCh, outerChLen);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_Memset(aad.buf + echPayloadOffset, 0, echPayloadLen);

    PRINT_BUF(50, (ss, "AAD for ECH Decryption", aad.buf, aad.len));

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
        SSL_TRC(10, ("%d: SSL3[%d]: Failed to decrypt inner CH with this candidate",
                     SSL_GETPID(), ss->fd));
        goto loser; /* code set */
    }
#else
    rv = SECITEM_CopyItem(NULL, decryptedChInner, &ss->xtnData.ech->innerCh);
    if (rv != SECSuccess) {
        goto loser;
    }
    decryptedChInner->len -= TLS13_ECH_AEAD_TAG_LEN; /* Fake tag */
#endif

    /* Stash the context, we may need it for HRR. */
    ss->ssl3.hs.echHpkeCtx = cx;
    *chInner = decryptedChInner;
    PRINT_BUF(100, (ss, "Decrypted ECH Inner", decryptedChInner->data, decryptedChInner->len));
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

/* This is the maximum number of extension hooks that the following functions can handle. */
#define MAX_EXTENSION_WRITERS 32

static SECStatus
tls13_WriteDupXtnsToChInner(PRBool compressing, sslBuffer *dupXtns, sslBuffer *chInnerXtns)
{
    SECStatus rv;
    if (compressing && SSL_BUFFER_LEN(dupXtns) > 0) {
        rv = sslBuffer_AppendNumber(chInnerXtns, ssl_tls13_outer_extensions_xtn, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        rv = sslBuffer_AppendNumber(chInnerXtns, dupXtns->len + 1, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        rv = sslBuffer_AppendBufferVariable(chInnerXtns, dupXtns, 1);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    } else {
        /* dupXtns carries whole extensions with lengths on each. */
        rv = sslBuffer_AppendBuffer(chInnerXtns, dupXtns);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    sslBuffer_Clear(dupXtns);
    return SECSuccess;
}

/* Add ordinary extensions to CHInner.
 * The value of the extension from CHOuter is in |extensionData|.
 *
 * If the value is to be compressed, it is written to |dupXtns|.
 * Otherwise, a full extension is written to |chInnerXtns|.
 *
 * This function is always called twice:
 * once without compression and once with compression if possible.
 *
 * Because we want to allow extensions that did not appear in CHOuter
 * to be included in CHInner, we also need to track which extensions
 * have been included.  This is what |called| and |nCalled| track.
 */
static SECStatus
tls13_ChInnerAppendExtension(sslSocket *ss, PRUint16 extensionType,
                             const sslReadBuffer *extensionData,
                             sslBuffer *dupXtns, sslBuffer *chInnerXtns,
                             PRBool compressing,
                             PRUint16 *called, unsigned int *nCalled)
{
    PRUint8 buf[1024] = { 0 };
    const PRUint8 *p;
    unsigned int len = 0;
    PRBool willCompress;

    PORT_Assert(extensionType != ssl_tls13_encrypted_client_hello_xtn);
    sslCustomExtensionHooks *hook = ss->opt.callExtensionWriterOnEchInner
                                        ? ssl_FindCustomExtensionHooks(ss, extensionType)
                                        : NULL;
    if (hook && hook->writer) {
        if (*nCalled >= MAX_EXTENSION_WRITERS) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE); /* TODO new code? */
            return SECFailure;
        }

        PRBool append = (*hook->writer)(ss->fd, ssl_hs_client_hello,
                                        buf, &len, sizeof(buf), hook->writerArg);
        called[(*nCalled)++] = extensionType;
        if (!append) {
            /* This extension is not going to appear in CHInner. */
            /* TODO: consider removing this extension from ss->xtnData.advertised.
             * The consequence of not removing it is that we won't complain
             * if the server accepts ECH and then includes this extension.
             * The cost is a complete reworking of ss->xtnData.advertised.
             */
            return SECSuccess;
        }
        /* It can be compressed if it is the same as the outer value. */
        willCompress = (len == extensionData->len &&
                        NSS_SecureMemcmp(buf, extensionData->buf, len) == 0);
        p = buf;
    } else {
        /* Non-custom extensions are duplicated when compressing. */
        willCompress = PR_TRUE;
        p = extensionData->buf;
        len = extensionData->len;
    }

    /* Duplicated extensions all need to go together. */
    sslBuffer *dst = willCompress ? dupXtns : chInnerXtns;
    SECStatus rv = sslBuffer_AppendNumber(dst, extensionType, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    if (!willCompress || !compressing) {
        rv = sslBuffer_AppendVariable(dst, p, len, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    /* As this function is called twice, we only want to update our state the second time. */
    if (compressing) {
        ss->xtnData.echAdvertised[ss->xtnData.echNumAdvertised++] = extensionType;
        SSL_TRC(50, ("Appending extension=%d to the Client Hello Inner. Compressed?=%d", extensionType, willCompress));
    }
    return SECSuccess;
}

/* Call any custom extension handlers that didn't want to be added to CHOuter. */
static SECStatus
tls13_ChInnerAdditionalExtensionWriters(sslSocket *ss, const PRUint16 *called,
                                        unsigned int nCalled, sslBuffer *chInnerXtns)
{
    if (!ss->opt.callExtensionWriterOnEchInner) {
        return SECSuccess;
    }

    for (PRCList *cursor = PR_NEXT_LINK(&ss->extensionHooks);
         cursor != &ss->extensionHooks;
         cursor = PR_NEXT_LINK(cursor)) {
        sslCustomExtensionHooks *hook = (sslCustomExtensionHooks *)cursor;

        /* Skip if this hook was already called. */
        PRBool hookCalled = PR_FALSE;
        for (unsigned int i = 0; i < nCalled; ++i) {
            if (called[i] == hook->type) {
                hookCalled = PR_TRUE;
                break;
            }
        }
        if (hookCalled) {
            continue;
        }

        /* This is a cut-down version of ssl_CallCustomExtensionSenders(). */
        PRUint8 buf[1024];
        unsigned int len = 0;
        PRBool append = (*hook->writer)(ss->fd, ssl_hs_client_hello,
                                        buf, &len, sizeof(buf), hook->writerArg);
        if (!append) {
            continue;
        }

        SECStatus rv = sslBuffer_AppendNumber(chInnerXtns, hook->type, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        rv = sslBuffer_AppendVariable(chInnerXtns, buf, len, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        ss->xtnData.echAdvertised[ss->xtnData.echNumAdvertised++] = hook->type;
    }
    return SECSuccess;
}

/* Take the PSK extension CHOuter and fill it with junk. */
static SECStatus
tls13_RandomizePsk(PRUint8 *buf, unsigned int len)
{
    sslReader rdr = SSL_READER(buf, len);

    /* Read the length of identities. */
    PRUint64 outerLen = 0;
    SECStatus rv = sslRead_ReadNumber(&rdr, 2, &outerLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PORT_Assert(outerLen < len + 2);

    /* Read the length of PskIdentity.identity */
    PRUint64 innerLen = 0;
    rv = sslRead_ReadNumber(&rdr, 2, &innerLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* identities should contain just one identity. */
    PORT_Assert(outerLen == innerLen + 6);

    /* Randomize PskIdentity.{identity,obfuscated_ticket_age}. */
    rv = PK11_GenerateRandom(buf + rdr.offset, innerLen + 4);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rdr.offset += innerLen + 4;

    /* Read the length of binders. */
    rv = sslRead_ReadNumber(&rdr, 2, &outerLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PORT_Assert(outerLen + rdr.offset == len);

    /* Read the length of the binder. */
    rv = sslRead_ReadNumber(&rdr, 1, &innerLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* binders should contain just one binder. */
    PORT_Assert(outerLen == innerLen + 1);

    /* Randomize the binder. */
    rv = PK11_GenerateRandom(buf + rdr.offset, innerLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

/* Given a buffer of extensions prepared for CHOuter, translate those extensions to a
 * buffer suitable for CHInner. This is intended to be called twice: once without
 * compression for the transcript hash and binders, and once with compression for
 * encoding the actual CHInner value.
 *
 * Compressed extensions are moved in both runs.  When compressing, they are moved
 * to a single outer_extensions extension, which lists extensions from CHOuter.
 * When not compressing, this produces the ClientHello that will be reconstructed
 * from the compressed ClientHello (that is, what goes into the handshake transcript),
 * so all the compressed extensions need to appear in the same place that the
 * outer_extensions extension appears.
 *
 * On the first run, if |inOutPskXtn| and OuterXtnsBuf contains a PSK extension,
 * remove it and return in the outparam.he caller will compute the binder value
 * based on the uncompressed output. Next, if |compress|, consolidate duplicated
 * extensions (that would otherwise be copied) into a single outer_extensions
 * extension. If |inOutPskXtn|, the extension contains a binder, it is appended
 * after the deduplicated outer_extensions. In the case of GREASE ECH, one call
 * is made to estimate size (wiith compression, null inOutPskXtn).
 */
SECStatus
tls13_ConstructInnerExtensionsFromOuter(sslSocket *ss, sslBuffer *chOuterXtnsBuf,
                                        sslBuffer *chInnerXtns, sslBuffer *inOutPskXtn,
                                        PRBool shouldCompress)
{
    SECStatus rv;
    PRUint64 extensionType;
    sslReadBuffer extensionData;
    sslBuffer pskXtn = SSL_BUFFER_EMPTY;
    sslBuffer dupXtns = SSL_BUFFER_EMPTY; /* Duplicated extensions, types-only if |compress|. */
    unsigned int tmpOffset;
    unsigned int tmpLen;
    unsigned int srcXtnBase; /* To truncate CHOuter and remove the PSK extension. */

    PRUint16 called[MAX_EXTENSION_WRITERS] = { 0 }; /* For tracking which has been called. */
    unsigned int nCalled = 0;

    SSL_TRC(50, ("%d: TLS13[%d]: Constructing ECH inner extensions %s compression",
                 SSL_GETPID(), ss->fd, shouldCompress ? "with" : "without"));

    /* When offering the "encrypted_client_hello" extension in its
     * ClientHelloOuter, the client MUST also offer an empty
     * "encrypted_client_hello" extension in its ClientHelloInner. */
    rv = sslBuffer_AppendNumber(chInnerXtns, ssl_tls13_encrypted_client_hello_xtn, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(chInnerXtns, 1, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(chInnerXtns, ech_xtn_type_inner, 1);
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

        /* Skip extensions that are TLS < 1.3 only, since CHInner MUST
         * negotiate TLS 1.3 or above.
         * If the extension is supported by default (sslSupported) but unknown
         * to TLS 1.3 it must be a TLS < 1.3 only extension. */
        SSLExtensionSupport sslSupported;
        (void)SSLExp_GetExtensionSupport(extensionType, &sslSupported);
        if (sslSupported != ssl_ext_none &&
            tls13_ExtensionStatus(extensionType, ssl_hs_client_hello) == tls13_extension_unknown) {
            continue;
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
                /* Only update state on second invocation of this function */
                if (shouldCompress) {
                    ss->xtnData.echAdvertised[ss->xtnData.echNumAdvertised++] = extensionType;
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
                /* Only update state on second invocation of this function */
                if (shouldCompress) {
                    ss->xtnData.echAdvertised[ss->xtnData.echNumAdvertised++] = extensionType;
                }
                break;
            case ssl_tls13_pre_shared_key_xtn:
                if (inOutPskXtn && !shouldCompress) {
                    rv = sslBuffer_AppendNumber(&pskXtn, extensionType, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    rv = sslBuffer_AppendVariable(&pskXtn, extensionData.buf,
                                                  extensionData.len, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    /* This should be the last extension. */
                    PORT_Assert(srcXtnBase == ss->xtnData.lastXtnOffset);
                    PORT_Assert(chOuterXtnsBuf->len - srcXtnBase == extensionData.len + 4);
                    rv = tls13_RandomizePsk(chOuterXtnsBuf->buf + srcXtnBase + 4,
                                            chOuterXtnsBuf->len - srcXtnBase - 4);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                } else if (!inOutPskXtn) {
                    /* When GREASEing, only the length is used.
                     * Order doesn't matter, so just copy the extension. */
                    rv = sslBuffer_AppendNumber(chInnerXtns, extensionType, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    rv = sslBuffer_AppendVariable(chInnerXtns, extensionData.buf,
                                                  extensionData.len, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                }
                /* Only update state on second invocation of this function */
                if (shouldCompress) {
                    ss->xtnData.echAdvertised[ss->xtnData.echNumAdvertised++] = extensionType;
                }
                break;
            default: {
                /* This is a regular extension.  We can maybe compress these. */
                rv = tls13_ChInnerAppendExtension(ss, extensionType,
                                                  &extensionData,
                                                  &dupXtns, chInnerXtns,
                                                  shouldCompress,
                                                  called, &nCalled);
                if (rv != SECSuccess) {
                    goto loser;
                }
                break;
            }
        }
    }

    rv = tls13_WriteDupXtnsToChInner(shouldCompress, &dupXtns, chInnerXtns);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Now call custom extension handlers that didn't choose to append anything to
     * the outer ClientHello. */
    rv = tls13_ChInnerAdditionalExtensionWriters(ss, called, nCalled, chInnerXtns);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (inOutPskXtn) {
        /* On the first, non-compress run, append the (bad) PSK binder.
         * On the second compression run, the caller is responsible for
         * providing an extension with a valid binder, so append that. */
        if (shouldCompress) {
            rv = sslBuffer_AppendBuffer(chInnerXtns, inOutPskXtn);
        } else {
            rv = sslBuffer_AppendBuffer(chInnerXtns, &pskXtn);
            *inOutPskXtn = pskXtn;
        }
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    return SECSuccess;

loser:
    sslBuffer_Clear(&pskXtn);
    sslBuffer_Clear(&dupXtns);
    return SECFailure;
}

static SECStatus
tls13_EncodeClientHelloInner(sslSocket *ss, const sslBuffer *chInner, const sslBuffer *chInnerXtns, sslBuffer *out)
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
tls13_PadChInner(sslBuffer *chInner, uint8_t maxNameLen, uint8_t serverNameLen)
{
    SECStatus rv;
    PORT_Assert(chInner);
    PORT_Assert(serverNameLen > 0);
    static unsigned char padding[256 + 32] = { 0 };
    int16_t name_padding = (int16_t)maxNameLen - (int16_t)serverNameLen;
    if (name_padding < 0) {
        name_padding = 0;
    }
    unsigned int rounding_padding = 31 - ((SSL_BUFFER_LEN(chInner) + name_padding) % 32);
    unsigned int total_padding = name_padding + rounding_padding;
    PORT_Assert(total_padding < sizeof(padding));
    SSL_TRC(100, ("computed ECH Inner Client Hello padding of size %u", total_padding));
    rv = sslBuffer_Append(chInner, padding, total_padding);
    if (rv != SECSuccess) {
        sslBuffer_Clear(chInner);
        return SECFailure;
    }
    return SECSuccess;
}

/* Build an ECH Xtn body with a zeroed payload for the client hello inner
 *
 *   enum { outer(0), inner(1) } ECHClientHelloType;
 *
 *   struct {
 *      ECHClientHelloType type;
 *      select (ECHClientHello.type) {
 *          case outer:
 *              HpkeSymmetricCipherSuite cipher_suite;
 *              uint8 config_id;
 *              opaque enc<0..2^16-1>;
 *              opaque payload<1..2^16-1>;
 *          case inner:
 *              Empty;
 *      };
 *  } ECHClientHello;
 *
 * payloadLen = Size of zeroed placeholder field for payload.
 * payloadOffset = Out parameter, start of payload field
 * echXtn = Out parameter, constructed ECH Xtn with zeroed placeholder field.
*/
SECStatus
tls13_BuildEchXtn(sslEchConfig *cfg, const SECItem *hpkeEnc, unsigned int payloadLen, PRUint16 *payloadOffset, sslBuffer *echXtn)
{
    SECStatus rv;
    /* Format the encrypted_client_hello extension. */
    rv = sslBuffer_AppendNumber(echXtn, ech_xtn_type_outer, 1);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(echXtn, cfg->contents.kdfId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_AppendNumber(echXtn, cfg->contents.aeadId, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(echXtn, cfg->contents.configId, 1);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (hpkeEnc) {
        /* Public Key */
        rv = sslBuffer_AppendVariable(echXtn, hpkeEnc->data, hpkeEnc->len, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
    } else {
        /* |enc| is empty. */
        rv = sslBuffer_AppendNumber(echXtn, 0, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    payloadLen += TLS13_ECH_AEAD_TAG_LEN;
    rv = sslBuffer_AppendNumber(echXtn, payloadLen, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    *payloadOffset = echXtn->len;
    rv = sslBuffer_Fill(echXtn, 0, payloadLen);
    if (rv != SECSuccess) {
        goto loser;
    }
    PRINT_BUF(100, (NULL, "ECH Xtn with Placeholder:", echXtn->buf, echXtn->len));
    return SECSuccess;
loser:
    sslBuffer_Clear(echXtn);
    return SECFailure;
}

SECStatus
tls13_ConstructClientHelloWithEch(sslSocket *ss, const sslSessionID *sid, PRBool freshSid,
                                  sslBuffer *chOuter, sslBuffer *chOuterXtnsBuf)
{
    SECStatus rv;
    sslBuffer chInner = SSL_BUFFER_EMPTY;
    sslBuffer encodedChInner = SSL_BUFFER_EMPTY;
    sslBuffer paddingChInner = SSL_BUFFER_EMPTY;
    sslBuffer chInnerXtns = SSL_BUFFER_EMPTY;
    sslBuffer pskXtn = SSL_BUFFER_EMPTY;
    unsigned int preambleLen;

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
        rv = tls13_WriteExtensionsWithBinder(ss, &chInnerXtns, &chInner);
        /* Update the stolen PSK extension with the binder value. */
        PORT_Memcpy(pskXtn.buf, &chInnerXtns.buf[chInnerXtns.len - pskXtn.len], pskXtn.len);
    } else {
        rv = sslBuffer_AppendBufferVariable(&chInner, &chInnerXtns, 2);
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    PRINT_BUF(50, (ss, "Uncompressed CHInner", chInner.buf, chInner.len));
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
    PRINT_BUF(50, (ss, "Compressed CHInner", encodedChInner.buf, encodedChInner.len));

    PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->echConfigs));
    sslEchConfig *cfg = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);

    /* We are using ECH so SNI must have been included */
    rv = tls13_PadChInner(&encodedChInner, cfg->contents.maxNameLen, strlen(ss->url));
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Build the ECH Xtn with placeholder and put it in chOuterXtnsBuf */
    sslBuffer echXtn = SSL_BUFFER_EMPTY;
    const SECItem *hpkeEnc = NULL;
    if (!ss->ssl3.hs.helloRetry) {
        hpkeEnc = PK11_HPKE_GetEncapPubKey(ss->ssl3.hs.echHpkeCtx);
        if (!hpkeEnc) {
            FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
            goto loser;
        }
    }
    PRUint16 echXtnPayloadOffset; /* Offset from start of ECH Xtn to ECH Payload */
    rv = tls13_BuildEchXtn(cfg, hpkeEnc, encodedChInner.len, &echXtnPayloadOffset, &echXtn);
    if (rv != SECSuccess) {
        goto loser;
    }
    ss->xtnData.echAdvertised[ss->xtnData.echNumAdvertised++] = ssl_tls13_encrypted_client_hello_xtn;
    rv = ssl3_EmplaceExtension(ss, chOuterXtnsBuf, ssl_tls13_encrypted_client_hello_xtn,
                               echXtn.buf, echXtn.len, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Add the padding */
    rv = ssl_InsertPaddingExtension(ss, chOuter->len, chOuterXtnsBuf);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Finish the CHO with the ECH Xtn payload zeroed */
    rv = ssl3_InsertChHeaderSize(ss, chOuter, chOuterXtnsBuf);
    if (rv != SECSuccess) {
        goto loser;
    }
    unsigned int chOuterXtnsOffset = chOuter->len + 2; /* From Start of CHO to Extensions list */
    rv = sslBuffer_AppendBufferVariable(chOuter, chOuterXtnsBuf, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* AAD consists of entire CHO, minus the 4 byte handshake header */
    SECItem aadItem = { siBuffer, chOuter->buf + 4, chOuter->len - 4 };
    /* ECH Payload begins after CHO Header, after ECH Xtn start, after ECH Xtn header */
    PRUint8 *echPayload = chOuter->buf + chOuterXtnsOffset + ss->xtnData.echXtnOffset + 4 + echXtnPayloadOffset;
    /* Insert the encrypted_client_hello xtn and coalesce. */
    rv = tls13_EncryptClientHello(ss, &aadItem, &encodedChInner, echPayload);
    if (rv != SECSuccess) {
        goto loser;
    }

    sslBuffer_Clear(&echXtn);
    sslBuffer_Clear(&chInner);
    sslBuffer_Clear(&encodedChInner);
    sslBuffer_Clear(&paddingChInner);
    sslBuffer_Clear(&chInnerXtns);
    sslBuffer_Clear(&pskXtn);
    return SECSuccess;

loser:
    sslBuffer_Clear(&chInner);
    sslBuffer_Clear(&encodedChInner);
    sslBuffer_Clear(&paddingChInner);
    sslBuffer_Clear(&chInnerXtns);
    sslBuffer_Clear(&pskXtn);
    PORT_Assert(PORT_GetError() != 0);
    return SECFailure;
}

static SECStatus
tls13_ComputeEchHelloRetryTranscript(sslSocket *ss, const PRUint8 *sh, unsigned int shLen, sslBuffer *out)
{
    SECStatus rv;
    PRUint8 zeroedEchSignal[TLS13_ECH_SIGNAL_LEN] = { 0 };
    sslBuffer *previousTranscript;

    if (ss->sec.isServer) {
        previousTranscript = &(ss->ssl3.hs.messages);
    } else {
        previousTranscript = &(ss->ssl3.hs.echInnerMessages);
    }
    /*
     *  This segment calculates the hash of the Client Hello
     *  TODO(djackson@mozilla.com) - Replace with existing function?
     *  e.g. tls13_ReinjectHandshakeTranscript
     *  TODO(djackson@mozilla.com) - Replace with streaming version
     */
    if (!ss->ssl3.hs.helloRetry || !ss->sec.isServer) {
        /*
        * This function can be called in three situations:
        *    - By the server, prior to sending the HRR, when ECH was accepted
        *    - By the client, after receiving the HRR, but before it knows whether ECH was accepted
        *    - By the server, after accepting ECH and receiving CH2 when it needs to reconstruct the HRR
        * In the first two situations, we need to include the message hash of inner ClientHello1 but don't
        * want to alter the buffer containing the current transcript.
        * In the last, the buffer already contains the message hash of inner ClientHello1.
        */
        SSL3Hashes hashes;
        rv = tls13_ComputeHash(ss, &hashes, previousTranscript->buf, previousTranscript->len, tls13_GetHash(ss));
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = sslBuffer_AppendNumber(out, ssl_hs_message_hash, 1);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = sslBuffer_AppendNumber(out, hashes.len, 3);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = sslBuffer_Append(out, hashes.u.raw, hashes.len);
        if (rv != SECSuccess) {
            goto loser;
        }
    } else {
        rv = sslBuffer_AppendBuffer(out, previousTranscript);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    /* Ensure the first ClientHello has been hashed. */
    PR_ASSERT(out->len == tls13_GetHashSize(ss) + 4);
    PRINT_BUF(100, (ss, "ECH Client Hello Message Hash", out->buf, out->len));
    /* Message Header */
    rv = sslBuffer_AppendNumber(out, ssl_hs_server_hello, 1);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* Message Size */
    rv = sslBuffer_AppendNumber(out, shLen, 3);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* Calculate where the HRR ECH Xtn Signal begins */
    unsigned int absEchOffset;
    if (ss->sec.isServer) {
        /* We know the ECH HRR Xtn is last */
        PORT_Assert(shLen >= TLS13_ECH_SIGNAL_LEN);
        absEchOffset = shLen - TLS13_ECH_SIGNAL_LEN;
    } else {
        /* We parsed the offset earlier */
        /* The result of pointer comparision is unspecified
         * (and pointer arithemtic is undefined) if the pointers
         * do not point to the same array or struct. That means these
         * asserts cannot be relied on for correctness in compiled code,
         * but may help the reader understand the requirements.
         */
        PORT_Assert(ss->xtnData.ech->hrrConfirmation > sh);
        PORT_Assert(ss->xtnData.ech->hrrConfirmation < sh + shLen);
        absEchOffset = ss->xtnData.ech->hrrConfirmation - sh;
    }
    PR_ASSERT(tls13_Debug_CheckXtnBegins(sh + absEchOffset - 4, ssl_tls13_encrypted_client_hello_xtn));
    /* The HRR up to the ECH Xtn signal */
    rv = sslBuffer_Append(out, sh, absEchOffset);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_Append(out, zeroedEchSignal, sizeof(zeroedEchSignal));
    if (rv != SECSuccess) {
        goto loser;
    }
    PR_ASSERT(absEchOffset + TLS13_ECH_SIGNAL_LEN <= shLen);
    /* The remainder of the HRR */
    rv = sslBuffer_Append(out, sh + absEchOffset + TLS13_ECH_SIGNAL_LEN, shLen - absEchOffset - TLS13_ECH_SIGNAL_LEN);
    if (rv != SECSuccess) {
        goto loser;
    }
    PR_ASSERT(out->len == tls13_GetHashSize(ss) + 4 + shLen + 4);
    return SECSuccess;
loser:
    sslBuffer_Clear(out);
    return SECFailure;
}

static SECStatus
tls13_ComputeEchServerHelloTranscript(sslSocket *ss, const PRUint8 *sh, unsigned int shLen, sslBuffer *out)
{
    SECStatus rv;
    sslBuffer *chSource = ss->sec.isServer ? &ss->ssl3.hs.messages : &ss->ssl3.hs.echInnerMessages;
    unsigned int offset = sizeof(SSL3ProtocolVersion) +
                          SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN;
    PORT_Assert(sh && shLen > offset);
    PORT_Assert(TLS13_ECH_SIGNAL_LEN <= SSL3_RANDOM_LENGTH);

    /* TODO(djackson@mozilla.com) - Replace with streaming version */

    rv = sslBuffer_AppendBuffer(out, chSource);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Re-create the message header. */
    rv = sslBuffer_AppendNumber(out, ssl_hs_server_hello, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(out, shLen, 3);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Copy the version and 24B of server_random. */
    rv = sslBuffer_Append(out, sh, offset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Zero the signal placeholder. */
    rv = sslBuffer_AppendNumber(out, 0, TLS13_ECH_SIGNAL_LEN);
    if (rv != SECSuccess) {
        goto loser;
    }
    offset += TLS13_ECH_SIGNAL_LEN;

    /* Use the remainder of SH. */
    rv = sslBuffer_Append(out, &sh[offset], shLen - offset);
    if (rv != SECSuccess) {
        goto loser;
    }
    sslBuffer_Clear(&ss->ssl3.hs.messages);
    sslBuffer_Clear(&ss->ssl3.hs.echInnerMessages);
    return SECSuccess;
loser:
    sslBuffer_Clear(&ss->ssl3.hs.messages);
    sslBuffer_Clear(&ss->ssl3.hs.echInnerMessages);
    sslBuffer_Clear(out);
    return SECFailure;
}

/* Compute the ECH signal using the transcript (up to, including)
 * ServerHello. The server sources this transcript prefix from
 * ss->ssl3.hs.messages, as it never uses ss->ssl3.hs.echInnerMessages.
 * The client uses the inner transcript, echInnerMessages. */
SECStatus
tls13_ComputeEchSignal(sslSocket *ss, PRBool isHrr, const PRUint8 *sh, unsigned int shLen, PRUint8 *out)
{
    SECStatus rv;
    sslBuffer confMsgs = SSL_BUFFER_EMPTY;
    SSL3Hashes hashes;
    PK11SymKey *echSecret = NULL;

    const char *hkdfInfo = isHrr ? kHkdfInfoEchHrrConfirm : kHkdfInfoEchConfirm;
    const size_t hkdfInfoLen = strlen(hkdfInfo);

    PRINT_BUF(100, (ss, "ECH Server Hello", sh, shLen));

    if (isHrr) {
        rv = tls13_ComputeEchHelloRetryTranscript(ss, sh, shLen, &confMsgs);
    } else {
        rv = tls13_ComputeEchServerHelloTranscript(ss, sh, shLen, &confMsgs);
    }
    if (rv != SECSuccess) {
        goto loser;
    }
    PRINT_BUF(100, (ss, "ECH Transcript", confMsgs.buf, confMsgs.len));
    rv = tls13_ComputeHash(ss, &hashes, confMsgs.buf, confMsgs.len,
                           tls13_GetHash(ss));
    if (rv != SECSuccess) {
        goto loser;
    }
    PRINT_BUF(100, (ss, "ECH Transcript Hash", &hashes.u, hashes.len));
    rv = tls13_DeriveEchSecret(ss, &echSecret);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = tls13_HkdfExpandLabelRaw(echSecret, tls13_GetHash(ss), hashes.u.raw,
                                  hashes.len, hkdfInfo, hkdfInfoLen, ss->protocolVariant,
                                  out, TLS13_ECH_SIGNAL_LEN);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    SSL_TRC(50, ("%d: TLS13[%d]: %s computed ECH signal", SSL_GETPID(), ss->fd, SSL_ROLE(ss)));
    PRINT_BUF(50, (ss, "Computed ECH Signal", out, TLS13_ECH_SIGNAL_LEN));
    PK11_FreeSymKey(echSecret);
    sslBuffer_Clear(&confMsgs);
    return SECSuccess;

loser:
    PK11_FreeSymKey(echSecret);
    sslBuffer_Clear(&confMsgs);
    return SECFailure;
}

/* Ech Secret is HKDF-Extract(0, ClientHelloInner.random) where 
   "0" is a string of Hash.len bytes of value 0. */
SECStatus
tls13_DeriveEchSecret(const sslSocket *ss, PK11SymKey **output)
{
    SECStatus rv;
    PK11SlotInfo *slot = NULL;
    PK11SymKey *crKey = NULL;
    SECItem rawKey;
    const unsigned char *client_random = ss->sec.isServer ? ss->ssl3.hs.client_random : ss->ssl3.hs.client_inner_random;
    PRINT_BUF(50, (ss, "Client Random for ECH", client_random, SSL3_RANDOM_LENGTH));
    /* We need a SECItem */
    rv = SECITEM_MakeItem(NULL, &rawKey, client_random, SSL3_RANDOM_LENGTH);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    /* We need a slot*/
    slot = PK11_GetBestSlot(CKM_HKDF_DERIVE, NULL);
    if (!slot) {
        rv = SECFailure;
        goto cleanup;
    }
    /* We import the key */
    crKey = PK11_ImportDataKey(slot, CKM_HKDF_DERIVE, PK11_OriginUnwrap,
                               CKA_DERIVE, &rawKey, NULL);
    if (crKey == NULL) {
        rv = SECFailure;
        goto cleanup;
    }
    /* NULL will be expanded to 0s of hash length */
    rv = tls13_HkdfExtract(NULL, crKey, tls13_GetHash(ss), output);
    if (rv != SECSuccess) {
        goto cleanup;
    }
    SSL_TRC(50, ("%d: TLS13[%d]: ECH Confirmation Key Derived.",
                 SSL_GETPID(), ss->fd));
    PRINT_KEY(50, (NULL, "ECH Confirmation Key", *output));
cleanup:
    SECITEM_ZfreeItem(&rawKey, PR_FALSE);
    if (slot) {
        PK11_FreeSlot(slot);
    }
    if (crKey) {
        PK11_FreeSymKey(crKey);
    }
    if (rv != SECSuccess && *output) {
        PK11_FreeSymKey(*output);
        *output = NULL;
    }
    return rv;
}

/* Called just prior to padding the CH. Use the size of the CH to estimate
 * the size of a corresponding ECH extension, then add it to the buffer. */
SECStatus
tls13_MaybeGreaseEch(sslSocket *ss, const sslBuffer *preamble, sslBuffer *buf)
{
    SECStatus rv;
    sslBuffer chInnerXtns = SSL_BUFFER_EMPTY;
    sslBuffer encodedCh = SSL_BUFFER_EMPTY;
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
    PR_ASSERT(!ss->sec.isServer);
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
    rv = tls13_EncodeClientHelloInner(ss, preamble, &chInnerXtns, &encodedCh);
    if (rv != SECSuccess) {
        goto loser; /* Code set */
    }
    rv = tls13_PadChInner(&encodedCh, ss->ssl3.hs.greaseEchSize, strlen(ss->url));

    payloadLen = encodedCh.len;
    payloadLen += TLS13_ECH_AEAD_TAG_LEN; /* Aead tag */

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

    rv = sslBuffer_AppendNumber(&greaseBuf, ech_xtn_type_outer, 1);
    if (rv != SECSuccess) {
        goto loser;
    }
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
    sslBuffer_Clear(&encodedCh);
    PK11_FreeSymKey(hmacPrk);
    PK11_FreeSymKey(derivedData);
    PK11_FreeSlot(slot);
    return SECSuccess;

loser:
    sslBuffer_Clear(&chInnerXtns);
    sslBuffer_Clear(&encodedCh);
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
        rv = tls13_ServerHandleOuterEchXtn(ss, &ss->xtnData, &echExtension->data);
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

        versionExtension = ssl3_FindExtension(ss, ssl_tls13_supported_versions_xtn);
        if (!versionExtension) {
            FATAL_ERROR(ss, SSL_ERROR_UNSUPPORTED_VERSION, illegal_parameter);
            goto loser;
        }
        rv = tls13_NegotiateVersion(ss, versionExtension);
        if (rv != SECSuccess) {
            /* code and alert set by tls13_NegotiateVersion */
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
tls13_MaybeHandleEchSignal(sslSocket *ss, const PRUint8 *sh, PRUint32 shLen, PRBool isHrr)
{
    SECStatus rv;
    PRUint8 computed[TLS13_ECH_SIGNAL_LEN];
    const PRUint8 *signal;
    PORT_Assert(!ss->sec.isServer);

    /* If !echHpkeCtx, we either didn't advertise or sent GREASE ECH. */
    if (!ss->ssl3.hs.echHpkeCtx) {
        SSL_TRC(50, ("%d: TLS13[%d]: client only sent GREASE ECH",
                     SSL_GETPID(), ss->fd));
        ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_ech;
        return SECSuccess;
    }

    if (isHrr) {
        if (ss->xtnData.ech) {
            signal = ss->xtnData.ech->hrrConfirmation;
        } else {
            SSL_TRC(50, ("%d: TLS13[%d]: client did not receive ECH Xtn from Server HRR",
                         SSL_GETPID(), ss->fd));
            signal = NULL;
            ss->ssl3.hs.echAccepted = PR_FALSE;
            ss->ssl3.hs.echDecided = PR_TRUE;
        }
    } else {
        signal = &ss->ssl3.hs.server_random[SSL3_RANDOM_LENGTH - TLS13_ECH_SIGNAL_LEN];
    }

    PORT_Assert(ssl3_ExtensionAdvertised(ss, ssl_tls13_encrypted_client_hello_xtn));

    /* Check ECH Confirmation for HRR ECH Xtn or ServerHello Random */
    if (signal) {
        rv = tls13_ComputeEchSignal(ss, isHrr, sh, shLen, computed);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        PRINT_BUF(100, (ss, "Server Signal", signal, TLS13_ECH_SIGNAL_LEN));
        PRBool new_decision = !NSS_SecureMemcmp(computed, signal, TLS13_ECH_SIGNAL_LEN);
        /* Server can't change its mind on whether to accept ECH */
        if (ss->ssl3.hs.echDecided && new_decision != ss->ssl3.hs.echAccepted) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_SERVER_HELLO, illegal_parameter);
            return SECFailure;
        }
        ss->ssl3.hs.echAccepted = new_decision;
        ss->ssl3.hs.echDecided = PR_TRUE;
    }

    ss->ssl3.hs.preliminaryInfo |= ssl_preinfo_ech;
    if (ss->ssl3.hs.echAccepted) {
        if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
            FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_SERVER_HELLO, illegal_parameter);
            return SECFailure;
        }
        /* Server accepted, but sent an extension which was only advertised in the ClientHelloOuter */
        if (ss->ssl3.hs.echInvalidExtension) {
            (void)SSL3_SendAlert(ss, alert_fatal, unsupported_extension);
            PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
            return SECFailure;
        }

        /* Swap the advertised lists as we've accepted ECH. */
        PRUint16 *tempArray = ss->xtnData.advertised;
        PRUint16 tempNum = ss->xtnData.numAdvertised;

        ss->xtnData.advertised = ss->xtnData.echAdvertised;
        ss->xtnData.numAdvertised = ss->xtnData.echNumAdvertised;

        ss->xtnData.echAdvertised = tempArray;
        ss->xtnData.echNumAdvertised = tempNum;

        /* |enc| must not be included in CH2.ClientECH. */
        if (ss->ssl3.hs.helloRetry && ss->sec.isServer &&
            ss->xtnData.ech->senderPubKey.len) {
            ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
            PORT_SetError(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
            return SECFailure;
        }
        ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ssl_tls13_encrypted_client_hello_xtn;

        /* Only overwrite client_random with client_inner_random if CHInner was
         *  succesfully used for handshake (NOT if HRR is received). */
        if (!isHrr) {
            PORT_Memcpy(ss->ssl3.hs.client_random, ss->ssl3.hs.client_inner_random, SSL3_RANDOM_LENGTH);
        }
    }
    /* If rejected, leave echHpkeCtx and echPublicName for rejection paths. */
    ssl3_CoalesceEchHandshakeHashes(ss);
    SSL_TRC(3, ("%d: TLS13[%d]: ECH %s accepted by server",
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
    TLSExtension *echExtension;
    int error = SSL_ERROR_INTERNAL_ERROR_ALERT;
    int errDesc = internal_error;

    PRINT_BUF(100, (ss, "ECH Inner", chReader.buf.buf, chReader.buf.len));

    /* unencodedChInner := preamble, tmpReadBuf := encoded extensions. */
    rv = tls13_CopyChPreamble(ss, &chReader, sidBytes, &unencodedChInner, &tmpReadBuf);
    if (rv != SECSuccess) {
        goto loser; /* code set */
    }

    /* Parse inner extensions into ss->ssl3.hs.remoteExtensions. */
    tmpB = CONST_CAST(PRUint8, tmpReadBuf.buf);
    rv = ssl3_ParseExtensions(ss, &tmpB, &tmpReadBuf.len);
    if (rv != SECSuccess) {
        goto loser; /* malformed, alert sent. */
    }

    echExtension = ssl3_FindExtension(ss, ssl_tls13_encrypted_client_hello_xtn);
    if (!echExtension) {
        error = SSL_ERROR_MISSING_ECH_EXTENSION;
        errDesc = illegal_parameter;
        goto alert_loser; /* Must have an inner Extension */
    }
    rv = tls13_ServerHandleInnerEchXtn(ss, &ss->xtnData, &echExtension->data);
    if (rv != SECSuccess) {
        goto loser; /* code set, alert sent. */
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
            SSL_TRC(10, ("%d: SSL3[%d]: copying inner extension of type %d and size %d directly", SSL_GETPID(),
                         ss->fd, innerExtension->type, innerExtension->data.len));
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
            SSL_TRC(10, ("%d: SSL3[%d]: ECH Outer Extensions has invalid size.",
                         SSL_GETPID(), ss->fd));
            error = SSL_ERROR_RX_MALFORMED_ECH_EXTENSION;
            errDesc = illegal_parameter;
            goto alert_loser;
        }
        if (SSL_READER_REMAINING(&extensionRdr) || (outerExtensionsList.len % 2) != 0 || !outerExtensionsList.len) {
            SSL_TRC(10, ("%d: SSL3[%d]: ECH Outer Extensions has invalid size.",
                         SSL_GETPID(), ss->fd));
            error = SSL_ERROR_RX_MALFORMED_ECH_EXTENSION;
            errDesc = illegal_parameter;
            goto alert_loser;
        }

        outerCursor = &ss->ssl3.hs.echOuterExtensions;
        sslReader compressedTypes = SSL_READER(outerExtensionsList.buf, outerExtensionsList.len);
        while (SSL_READER_REMAINING(&compressedTypes)) {
            outerFound = PR_FALSE;
            rv = sslRead_ReadNumber(&compressedTypes, 2, &tmp);
            if (rv != SECSuccess) {
                SSL_TRC(10, ("%d: SSL3[%d]: ECH Outer Extensions has invalid contents.",
                             SSL_GETPID(), ss->fd));
                error = SSL_ERROR_RX_MALFORMED_ECH_EXTENSION;
                errDesc = illegal_parameter;
                goto alert_loser;
            }
            if (tmp == ssl_tls13_encrypted_client_hello_xtn ||
                tmp == ssl_tls13_outer_extensions_xtn) {
                SSL_TRC(10, ("%d: SSL3[%d]: ECH Outer Extensions contains an invalid reference.",
                             SSL_GETPID(), ss->fd));
                error = SSL_ERROR_RX_MALFORMED_ECH_EXTENSION;
                errDesc = illegal_parameter;
                goto alert_loser;
            }
            do {
                const TLSExtension *candidate = (TLSExtension *)outerCursor;
                /* Advance the outerCursor, we never consider the same xtn twice. */
                outerCursor = PR_NEXT_LINK(outerCursor);
                if (candidate->type == tmp) {
                    outerFound = PR_TRUE;
                    SSL_TRC(100, ("%d: SSL3[%d]: Decompressing ECH Inner Extension of type %d",
                                  SSL_GETPID(), ss->fd, tmp));
                    rv = sslBuffer_AppendNumber(&unencodedChInner,
                                                candidate->type, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    rv = sslBuffer_AppendVariable(&unencodedChInner,
                                                  candidate->data.data,
                                                  candidate->data.len, 2);
                    if (rv != SECSuccess) {
                        goto loser;
                    }
                    break;
                }
            } while (outerCursor != &ss->ssl3.hs.echOuterExtensions);
            if (!outerFound) {
                SSL_TRC(10, ("%d: SSL3[%d]: ECH Outer Extensions has missing,"
                             " out of order or duplicate references.",
                             SSL_GETPID(), ss->fd));
                error = SSL_ERROR_RX_MALFORMED_ECH_EXTENSION;
                errDesc = illegal_parameter;
                goto alert_loser;
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
        error = SSL_ERROR_RX_MALFORMED_CLIENT_HELLO;
        errDesc = internal_error;
        goto alert_loser;
    }

    rv = ssl3_ParseExtensions(ss, &tmpB, &tmpLength);
    if (rv != SECSuccess) {
        goto loser; /* Error set and alert already sent */
    }

    SECITEM_FreeItem(*echInner, PR_FALSE);
    (*echInner)->data = unencodedChInner.buf;
    (*echInner)->len = unencodedChInner.len;
    return SECSuccess;
alert_loser:
    FATAL_ERROR(ss, error, errDesc);
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
    sslEchCookieData echData;
    sslEchConfig *candidate = NULL; /* non-owning */
    TLSExtension *hrrXtn;
    PRBool previouslyOfferedEch;

    if (!ss->xtnData.ech || ss->xtnData.ech->receivedInnerXtn) {
        ss->ssl3.hs.echDecided = PR_TRUE;
        return SECSuccess;
    }

    PORT_Assert(ss->xtnData.ech->innerCh.data);

    if (ss->ssl3.hs.helloRetry) {
        ss->ssl3.hs.echDecided = PR_TRUE;
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
        rv = tls13_HandleHrrCookie(ss, cookieData.data, cookieData.len,
                                   NULL, NULL, &previouslyOfferedEch, &echData, PR_FALSE);
        if (rv != SECSuccess) {
            return SECSuccess;
        }

        ss->ssl3.hs.echHpkeCtx = echData.hpkeCtx;

        const PRUint8 greaseConstant[TLS13_ECH_SIGNAL_LEN] = { 0 };
        ss->ssl3.hs.echAccepted = previouslyOfferedEch &&
                                  !NSS_SecureMemcmp(greaseConstant, echData.signal, TLS13_ECH_SIGNAL_LEN);

        if (echData.configId != ss->xtnData.ech->configId ||
            echData.kdfId != ss->xtnData.ech->kdfId ||
            echData.aeadId != ss->xtnData.ech->aeadId) {
            FATAL_ERROR(ss, SSL_ERROR_BAD_2ND_CLIENT_HELLO,
                        illegal_parameter);
            return SECFailure;
        }

        if (!ss->ssl3.hs.echHpkeCtx) {
            return SECSuccess;
        }
    }

    if (ss->ssl3.hs.echDecided && !ss->ssl3.hs.echAccepted) {
        /* We don't change our mind */
        return SECSuccess;
    }
    /* Regardless of where we return, the outcome is decided */
    ss->ssl3.hs.echDecided = PR_TRUE;

    /* Cookie data was good, proceed with ECH. */
    rv = tls13_GetMatchingEchConfigs(ss, ss->xtnData.ech->kdfId, ss->xtnData.ech->aeadId,
                                     ss->xtnData.ech->configId, candidate, &candidate);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SEC_ERROR_LIBRARY_FAILURE, internal_error);
        return SECFailure;
    }

    if (candidate) {
        rv = tls13_ServerMakeChOuterAAD(ss, chOuter, chOuterLen, &outerAAD);
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
    PRINT_BUF(50, (ss, "Compressed CHInner", decryptedChInner->data,
                   decryptedChInner->len));

    ss->ssl3.hs.echAccepted = PR_TRUE;

    /* Stash the CHOuter extensions. They're not yet handled (only parsed). If
     * the CHInner contains outer_extensions_xtn, we'll need to reference them. */
    ssl3_MoveRemoteExtensions(&ss->ssl3.hs.echOuterExtensions, &ss->ssl3.hs.remoteExtensions);

    rv = tls13_UnencodeChInner(ss, sidBytes, &decryptedChInner);
    if (rv != SECSuccess) {
        SECITEM_FreeItem(decryptedChInner, PR_TRUE);
        return SECFailure; /* code set */
    }
    PRINT_BUF(50, (ss, "Uncompressed CHInner", decryptedChInner->data,
                   decryptedChInner->len));
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

    rv = tls13_ComputeEchSignal(ss, PR_FALSE, sh, shLen, signal);
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

SECStatus
tls13_WriteServerEchHrrSignal(sslSocket *ss, PRUint8 *sh, unsigned int shLen)
{
    SECStatus rv;
    PR_ASSERT(shLen >= 4 + TLS13_ECH_SIGNAL_LEN);
    /* We put the HRR ECH extension last. */
    PRUint8 *placeholder_location = sh + shLen - TLS13_ECH_SIGNAL_LEN;
    /* Defensive check that we are overwriting the contents of the right extension */
    PR_ASSERT(tls13_Debug_CheckXtnBegins(placeholder_location - 4, ssl_tls13_encrypted_client_hello_xtn));
    /* Calculate signal and overwrite */
    rv = tls13_ComputeEchSignal(ss, PR_TRUE, sh, shLen, placeholder_location);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* Free HRR GREASE/accept_confirmation value, it MUST be restored from
     * cookie when handling CH2 after HRR. */
    sslBuffer_Clear(&ss->ssl3.hs.greaseEchBuf);
    return SECSuccess;
}