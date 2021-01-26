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
 *     HpkeKdfId kdfId;                   // ECH KDF (uint16)
 *     HpkeAeadId aeadId;                 // ECH AEAD (uint16)
 *     opaque echConfigId<0..255>;        // ECH config_id
 *     opaque echHpkeCtx<0..65535>;       // ECH serialized HPKE context
 *     opaque applicationToken<0..65535>; // Application token
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
    SECItem *echHpkeCtx = NULL;

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

    if (ss->xtnData.ech) {
        rv = sslBuffer_AppendNumber(&cookieBuf, ss->xtnData.ech->kdfId, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        rv = sslBuffer_AppendNumber(&cookieBuf, ss->xtnData.ech->aeadId, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }

        /* Received ECH config_id, regardless of acceptance or possession
         * of a matching ECHConfig. */
        PORT_Assert(ss->xtnData.ech->configId.len == 8);
        rv = sslBuffer_AppendVariable(&cookieBuf, ss->xtnData.ech->configId.data,
                                      ss->xtnData.ech->configId.len, 1);
        if (rv != SECSuccess) {
            return SECFailure;
        }

        /* There might be no HPKE Context, e.g. when we lack a matching ECHConfig. */
        if (ss->ssl3.hs.echHpkeCtx) {
            rv = PK11_HPKE_ExportContext(ss->ssl3.hs.echHpkeCtx, NULL, &echHpkeCtx);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            rv = sslBuffer_AppendVariable(&cookieBuf, echHpkeCtx->data, echHpkeCtx->len, 2);
            SECITEM_ZfreeItem(echHpkeCtx, PR_TRUE);
        } else {
            /* Zero length HPKE context. */
            rv = sslBuffer_AppendNumber(&cookieBuf, 0, 2);
        }
        if (rv != SECSuccess) {
            return SECFailure;
        }
    } else {
        rv = sslBuffer_AppendNumber(&cookieBuf, 0, 7);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }

    /* Application token. */
    rv = sslBuffer_AppendVariable(&cookieBuf, appToken, appTokenLen, 2);
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

/* Given a cookie and cookieLen, decrypt and parse, returning
 * any values that were requested via the "previous_" params. If
 * recoverHashState is true, the transcript state is recovered */
SECStatus
tls13_HandleHrrCookie(sslSocket *ss,
                      unsigned char *cookie, unsigned int cookieLen,
                      ssl3CipherSuite *previousCipherSuite,
                      const sslNamedGroupDef **previousGroup,
                      PRBool *previousEchOffered,
                      HpkeKdfId *previousEchKdfId,
                      HpkeAeadId *previousEchAeadId,
                      SECItem *previousEchConfigId,
                      HpkeContext **previousEchHpkeCtx,
                      PRBool recoverHashState)
{
    SECStatus rv;
    unsigned char plaintext[1024];
    unsigned int plaintextLen = 0;
    sslBuffer messageBuf = SSL_BUFFER_EMPTY;
    sslReadBuffer echHpkeBuf = { 0 };
    sslReadBuffer echConfigIdBuf = { 0 };
    PRUint64 sentinel;
    PRUint64 cipherSuite;
    HpkeContext *hpkeContext = NULL;
    HpkeKdfId echKdfId;
    HpkeAeadId echAeadId;
    PRUint64 group;
    PRUint64 tmp64;
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

    /* ECH Ciphersuite */
    rv = sslRead_ReadNumber(&reader, 2, &tmp64);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    echKdfId = (HpkeKdfId)tmp64;

    rv = sslRead_ReadNumber(&reader, 2, &tmp64);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    echAeadId = (HpkeAeadId)tmp64;

    /* ECH Config ID and HPKE context may be empty. */
    rv = sslRead_ReadVariable(&reader, 1, &echConfigIdBuf);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }
    rv = sslRead_ReadVariable(&reader, 2, &echHpkeBuf);
    if (rv != SECSuccess) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, illegal_parameter);
        return SECFailure;
    }

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

    /* The remainder is the hash. */
    if (recoverHashState) {
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
    }

    if (previousEchHpkeCtx && echHpkeBuf.len) {
        const SECItem hpkeItem = { siBuffer, CONST_CAST(unsigned char, echHpkeBuf.buf),
                                   echHpkeBuf.len };
        hpkeContext = PK11_HPKE_ImportContext(&hpkeItem, NULL);
        if (!hpkeContext) {
            FATAL_ERROR(ss, PORT_GetError(), internal_error);
            return SECFailure;
        }
    }

    if (previousEchConfigId && echConfigIdBuf.len) {
        SECItem tmp = { siBuffer, NULL, 0 };
        rv = SECITEM_MakeItem(NULL, &tmp, echConfigIdBuf.buf, echConfigIdBuf.len);
        if (rv != SECSuccess) {
            PK11_HPKE_DestroyContext(hpkeContext, PR_TRUE);
            FATAL_ERROR(ss, PORT_GetError(), internal_error);
            return SECFailure;
        }
        *previousEchConfigId = tmp;
    }

    if (previousEchKdfId) {
        *previousEchKdfId = echKdfId;
    }
    if (previousEchAeadId) {
        *previousEchAeadId = echAeadId;
    }
    if (previousEchHpkeCtx) {
        *previousEchHpkeCtx = hpkeContext;
    }
    if (previousCipherSuite) {
        *previousCipherSuite = cipherSuite;
    }
    if (previousGroup) {
        *previousGroup = selectedGroup;
    }
    if (previousEchOffered) {
        *previousEchOffered = echConfigIdBuf.len > 0;
    }
    return SECSuccess;
}
