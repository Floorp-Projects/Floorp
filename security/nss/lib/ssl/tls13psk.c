/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslproto.h"
#include "sslimpl.h"
#include "ssl3exthandle.h"
#include "tls13exthandle.h"
#include "tls13hkdf.h"
#include "tls13psk.h"

SECStatus
SSLExp_AddExternalPsk0Rtt(PRFileDesc *fd, PK11SymKey *key, const PRUint8 *identity,
                          unsigned int identityLen, SSLHashType hash,
                          PRUint16 zeroRttSuite, PRUint32 maxEarlyData)
{

    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSLExp_SetExternalPsk",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    if (!key || !identity || !identityLen || identityLen > 0xFFFF ||
        (hash != ssl_hash_sha256 && hash != ssl_hash_sha384)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    SECItem label = { siBuffer, CONST_CAST(unsigned char, identity), identityLen };
    sslPsk *psk = tls13_MakePsk(PK11_ReferenceSymKey(key), ssl_psk_external,
                                hash, &label);
    if (!psk) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    psk->zeroRttSuite = zeroRttSuite;
    psk->maxEarlyData = maxEarlyData;
    SECStatus rv = SECFailure;

    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    if (ss->psk) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        tls13_DestroyPsk(psk);
    } else {
        ss->psk = psk;
        rv = SECSuccess;
        tls13_ResetHandshakePsks(ss, &ss->ssl3.hs.psks);
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return rv;
}

SECStatus
SSLExp_AddExternalPsk(PRFileDesc *fd, PK11SymKey *key, const PRUint8 *identity,
                      unsigned int identityLen, SSLHashType hash)
{
    return SSLExp_AddExternalPsk0Rtt(fd, key, identity, identityLen,
                                     hash, TLS_NULL_WITH_NULL_NULL, 0);
}

SECStatus
SSLExp_RemoveExternalPsk(PRFileDesc *fd, const PRUint8 *identity, unsigned int identityLen)
{
    if (!identity || !identityLen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetPSK",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    SECItem removeIdentity = { siBuffer,
                               (unsigned char *)identity,
                               identityLen };

    SECStatus rv;
    ssl_Get1stHandshakeLock(ss);
    ssl_GetSSL3HandshakeLock(ss);

    if (!ss->psk || SECITEM_CompareItem(&ss->psk->label, &removeIdentity) != SECEqual) {
        PORT_SetError(SEC_ERROR_NO_KEY);
        rv = SECFailure;
    } else {
        tls13_DestroyPsk(ss->psk);
        ss->psk = NULL;
        tls13_ResetHandshakePsks(ss, &ss->ssl3.hs.psks);
        rv = SECSuccess;
    }

    ssl_ReleaseSSL3HandshakeLock(ss);
    ssl_Release1stHandshakeLock(ss);

    return rv;
}

sslPsk *
tls13_CopyPsk(sslPsk *opsk)
{
    if (!opsk || !opsk->key) {
        return NULL;
    }

    sslPsk *psk = PORT_ZNew(sslPsk);
    if (!psk) {
        return NULL;
    }

    SECStatus rv = SECITEM_CopyItem(NULL, &psk->label, &opsk->label);
    if (rv != SECSuccess) {
        PORT_Free(psk);
        return NULL;
    }
    /* We should only have the initial key. Binder keys
     * are derived during the handshake.  */
    PORT_Assert(opsk->type == ssl_psk_external);
    PORT_Assert(opsk->key);
    PORT_Assert(opsk->binderKey);
    psk->hash = opsk->hash;
    psk->type = opsk->type;
    psk->key = opsk->key ? PK11_ReferenceSymKey(opsk->key) : NULL;
    psk->binderKey = opsk->binderKey ? PK11_ReferenceSymKey(opsk->binderKey) : NULL;
    return psk;
}

void
tls13_DestroyPsk(sslPsk *psk)
{
    if (!psk) {
        return;
    }
    if (psk->key) {
        PK11_FreeSymKey(psk->key);
        psk->key = NULL;
    }
    if (psk->binderKey) {
        PK11_FreeSymKey(psk->binderKey);
        psk->binderKey = NULL;
    }
    SECITEM_ZfreeItem(&psk->label, PR_FALSE);
    PORT_ZFree(psk, sizeof(*psk));
}

void
tls13_DestroyPskList(PRCList *list)
{
    PRCList *cur_p;
    while (!PR_CLIST_IS_EMPTY(list)) {
        cur_p = PR_LIST_TAIL(list);
        PR_REMOVE_LINK(cur_p);
        tls13_DestroyPsk((sslPsk *)cur_p);
    }
}

sslPsk *
tls13_MakePsk(PK11SymKey *key, SSLPskType pskType, SSLHashType hashType, const SECItem *label)
{
    sslPsk *psk = PORT_ZNew(sslPsk);
    if (!psk) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }
    psk->type = pskType;
    psk->hash = hashType;
    psk->key = key;

    /* Label is NULL in the resumption case. */
    if (label) {
        PORT_Assert(psk->type != ssl_psk_resume);
        SECStatus rv = SECITEM_CopyItem(NULL, &psk->label, label);
        if (rv != SECSuccess) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            tls13_DestroyPsk(psk);
            return NULL;
        }
    }

    return psk;
}

/* Destroy any existing PSKs in |list| then copy
 * in the configured |ss->psk|, if any.*/
SECStatus
tls13_ResetHandshakePsks(sslSocket *ss, PRCList *list)
{
    tls13_DestroyPskList(list);
    PORT_Assert(!ss->xtnData.selectedPsk);
    ss->xtnData.selectedPsk = NULL;
    if (ss->psk) {
        PORT_Assert(ss->psk->type == ssl_psk_external);
        PORT_Assert(ss->psk->key);
        PORT_Assert(!ss->psk->binderKey);

        sslPsk *epsk = tls13_MakePsk(PK11_ReferenceSymKey(ss->psk->key),
                                     ss->psk->type, ss->psk->hash, &ss->psk->label);
        if (!epsk) {
            return SECFailure;
        }
        epsk->zeroRttSuite = ss->psk->zeroRttSuite;
        epsk->maxEarlyData = ss->psk->maxEarlyData;
        PR_APPEND_LINK(&epsk->link, list);
    }
    return SECSuccess;
}