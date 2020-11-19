/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11func.h"
#include "ssl.h"
#include "sslt.h"
#include "sslimpl.h"
#include "selfencrypt.h"
#include "tls13con.h"
#include "tls13ech.h"
#include "tls13err.h"
#include "tls13hashstate.h"

/*
 * The cookie is structured as a self-encrypted structure with the
 * inner value being.
 *
 * struct {
 *     uint8 indicator = 0xff;            // To disambiguate from tickets.
 *     uint16 cipherSuite;                // Selected cipher suite.
 *     uint16 keyShare;                   // Requested key share group (0=none)
 *     opaque applicationToken<0..65535>; // Application token
 *     echConfigId<0..255>;               // Encrypted Client Hello config_id
 *     echHrrPsk<0..255>;                 // Encrypted Client Hello HRR PSK
 *     opaque ch_hash[rest_of_buffer];    // H(ClientHello)
 * } CookieInner;
 *
 * An empty echConfigId means that ECH was not offered in the first ClientHello.
 * An empty echHrrPsk means that ECH was not accepted in CH1.
 */
SECStatus
tls13_MakeHrrCookie(sslSocket *ss, const sslNamedGroupDef *selectedGroup,
                    const PRUint8 *appToken, unsigned int appTokenLen,
                    PRUint8 *buf, unsigned int *len, unsigned int maxlen)
{
    SECStatus rv;
    SSL3Hashes hashes;
    PRUint8 cookie[1024];
    sslBuffer cookieBuf = SSL_BUFFER(cookie);
    static const PRUint8 indicator = 0xff;
    SECItem hrrNonceInfoItem = { siBuffer, (unsigned char *)kHpkeInfoEchHrr,
                                 strlen(kHpkeInfoEchHrr) };
    PK11SymKey *echHrrPsk = NULL;
    SECItem *rawEchPsk = NULL;

    /* Encode header. */
    rv = sslBuffer_Append(&cookieBuf, &indicator, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_AppendNumber(&cookieBuf, ss->ssl3.hs.cipher_suite, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_AppendNumber(&cookieBuf,
                                selectedGroup ? selectedGroup->name : 0, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Application token. */
    rv = sslBuffer_AppendVariable(&cookieBuf, appToken, appTokenLen, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Received ECH config_id, regardless of acceptance or possession
     * of a matching ECHConfig. If rejecting ECH, this is essentially a boolean
     * indicating that ECH was offered in CH1. If accepting ECH, this config_id
     * will be used for the ECH decryption in CH2. */
    if (ss->xtnData.echConfigId.len) {
        rv = sslBuffer_AppendVariable(&cookieBuf, ss->xtnData.echConfigId.data,
                                      ss->xtnData.echConfigId.len, 1);
    } else {
        PORT_Assert(!ssl3_FindExtension(ss, ssl_tls13_encrypted_client_hello_xtn));
        rv = sslBuffer_AppendNumber(&cookieBuf, 0, 1);
    }
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Extract and encode the ech-hrr-key, if ECH was accepted
     * (i.e. an Open() succeeded. */
    if (ss->ssl3.hs.echAccepted) {
        rv = PK11_HPKE_ExportSecret(ss->ssl3.hs.echHpkeCtx, &hrrNonceInfoItem, 32, &echHrrPsk);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        rv = PK11_ExtractKeyValue(echHrrPsk);
        if (rv != SECSuccess) {
            PK11_FreeSymKey(echHrrPsk);
            return SECFailure;
        }
        rawEchPsk = PK11_GetKeyData(echHrrPsk);
        if (!rawEchPsk) {
            PK11_FreeSymKey(echHrrPsk);
            return SECFailure;
        }
        rv = sslBuffer_AppendVariable(&cookieBuf, rawEchPsk->data, rawEchPsk->len, 1);
        PK11_FreeSymKey(echHrrPsk);
    } else {
        /* Zero length ech_hrr_key. */
        rv = sslBuffer_AppendNumber(&cookieBuf, 0, 1);
    }
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Compute and encode hashes. */
    rv = tls13_ComputeHandshakeHashes(ss, &hashes);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Append(&cookieBuf, hashes.u.raw, hashes.len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Encrypt right into the buffer. */
    rv = ssl_SelfEncryptProtect(ss, cookieBuf.buf, cookieBuf.len,
                                buf, len, maxlen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

/* Recover the hash state from the cookie. */
SECStatus
tls13_RecoverHashState(sslSocket *ss,
                       unsigned char *cookie, unsigned int cookieLen,
                       ssl3CipherSuite *previousCipherSuite,
                       const sslNamedGroupDef **previousGroup,
                       PRBool *previousEchOffered)
{
    SECStatus rv;
    unsigned char plaintext[1024];
    unsigned int plaintextLen = 0;
    sslBuffer messageBuf = SSL_BUFFER_EMPTY;
    sslReadBuffer echPskBuf;
    sslReadBuffer echConfigIdBuf;
    PRUint64 sentinel;
    PRUint64 cipherSuite;
    PRUint64 group;
    const sslNamedGroupDef *selectedGroup;
    PRUint64 appTokenLen;

    rv = ssl_SelfEncryptUnprotect(ss, cookie, cookieLen,
                                  plaintext, &plaintextLen, sizeof(plaintext));
    if (rv != SECSuccess) {
        return SECFailure;
    }

    sslReader reader = SSL_READER(plaintext, plaintextLen);

    /* Should start with the sentinel value. */
    rv = sslRead_ReadNumber(&reader, 1, &sentinel);
    if ((rv != SECSuccess) || (sentinel != TLS13_COOKIE_SENTINEL)) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    /* The cipher suite should be the same or there are some shenanigans. */
    rv = sslRead_ReadNumber(&reader, 2, &cipherSuite);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }

    /* The named group, if any. */
    rv = sslRead_ReadNumber(&reader, 2, &group);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    selectedGroup = ssl_LookupNamedGroup(group);

    /* Application token. */
    PORT_Assert(ss->xtnData.applicationToken.len == 0);
    rv = sslRead_ReadNumber(&reader, 2, &appTokenLen);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    if (SECITEM_AllocItem(NULL, &ss->xtnData.applicationToken,
                          appTokenLen) == NULL) {
        FATAL_ERROR(ss, PORT_GetError(), internal_error);
        return SECFailure;
    }
    ss->xtnData.applicationToken.len = appTokenLen;
    sslReadBuffer appTokenReader = { 0 };
    rv = sslRead_Read(&reader, appTokenLen, &appTokenReader);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    PORT_Assert(appTokenReader.len == appTokenLen);
    PORT_Memcpy(ss->xtnData.applicationToken.data, appTokenReader.buf, appTokenLen);

    /* ECH Config ID, which may be empty. */
    rv = sslRead_ReadVariable(&reader, 1, &echConfigIdBuf);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    /* ECH HRR PSK, if present, is already used by tls13_GetEchInfoFromCookie */
    rv = sslRead_ReadVariable(&reader, 1, &echPskBuf);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }

    /* The remainder is the hash. */
    unsigned int hashLen = SSL_READER_REMAINING(&reader);
    if (hashLen != tls13_GetHashSize(ss)) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }

    /* Now reinject the message. */
    SSL_ASSERT_HASHES_EMPTY(ss);
    rv = ssl_HashHandshakeMessageInt(ss, ssl_hs_message_hash, 0,
                                     SSL_READER_CURRENT(&reader), hashLen,
                                     ssl3_UpdateHandshakeHashes);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* And finally reinject the HRR. */
    rv = tls13_ConstructHelloRetryRequest(ss, cipherSuite,
                                          selectedGroup,
                                          cookie, cookieLen,
                                          &messageBuf);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = ssl_HashHandshakeMessageInt(ss, ssl_hs_server_hello, 0,
                                     SSL_BUFFER_BASE(&messageBuf),
                                     SSL_BUFFER_LEN(&messageBuf),
                                     ssl3_UpdateHandshakeHashes);
    sslBuffer_Clear(&messageBuf);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *previousCipherSuite = cipherSuite;
    *previousGroup = selectedGroup;
    *previousEchOffered = echConfigIdBuf.len > 0;
    return SECSuccess;
}
