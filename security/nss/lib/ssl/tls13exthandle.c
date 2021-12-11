/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nssrenam.h"
#include "nss.h"
#include "ssl.h"
#include "sslproto.h"
#include "sslimpl.h"
#include "pk11pub.h"
#include "ssl3ext.h"
#include "ssl3exthandle.h"
#include "tls13ech.h"
#include "tls13exthandle.h"
#include "tls13psk.h"
#include "tls13subcerts.h"

SECStatus
tls13_ServerSendStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                 sslBuffer *buf, PRBool *added)
{
    const sslServerCert *serverCert = ss->sec.serverCert;
    const SECItem *item;
    SECStatus rv;

    if (!serverCert->certStatusArray ||
        !serverCert->certStatusArray->len) {
        return SECSuccess;
    }

    item = &serverCert->certStatusArray->items[0];

    /* Only send the first entry. */
    /* status_type == ocsp */
    rv = sslBuffer_AppendNumber(buf, 1 /*ocsp*/, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* opaque OCSPResponse<1..2^24-1> */
    rv = sslBuffer_AppendVariable(buf, item->data, item->len, 3);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

/*
 *     [draft-ietf-tls-tls13-11] Section 6.3.2.3.
 *
 *     struct {
 *         NamedGroup group;
 *         opaque key_exchange<1..2^16-1>;
 *     } KeyShareEntry;
 *
 *     struct {
 *         select (role) {
 *             case client:
 *                 KeyShareEntry client_shares<4..2^16-1>;
 *
 *             case server:
 *                 KeyShareEntry server_share;
 *         }
 *     } KeyShare;
 *
 * DH is Section 6.3.2.3.1.
 *
 *     opaque dh_Y<1..2^16-1>;
 *
 * ECDH is Section 6.3.2.3.2.
 *
 *     opaque point <1..2^8-1>;
 */
PRUint32
tls13_SizeOfKeyShareEntry(const SECKEYPublicKey *pubKey)
{
    /* Size = NamedGroup(2) + length(2) + opaque<?> share */
    switch (pubKey->keyType) {
        case ecKey:
            return 2 + 2 + pubKey->u.ec.publicValue.len;
        case dhKey:
            return 2 + 2 + pubKey->u.dh.prime.len;
        default:
            PORT_Assert(0);
    }
    return 0;
}

SECStatus
tls13_EncodeKeyShareEntry(sslBuffer *buf, SSLNamedGroup group,
                          SECKEYPublicKey *pubKey)
{
    SECStatus rv;
    unsigned int size = tls13_SizeOfKeyShareEntry(pubKey);

    rv = sslBuffer_AppendNumber(buf, group, 2);
    if (rv != SECSuccess)
        return rv;
    rv = sslBuffer_AppendNumber(buf, size - 4, 2);
    if (rv != SECSuccess)
        return rv;

    switch (pubKey->keyType) {
        case ecKey:
            rv = sslBuffer_Append(buf, pubKey->u.ec.publicValue.data,
                                  pubKey->u.ec.publicValue.len);
            break;
        case dhKey:
            rv = ssl_AppendPaddedDHKeyShare(buf, pubKey, PR_FALSE);
            break;
        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            break;
    }

    return rv;
}

SECStatus
tls13_ClientSendKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                            sslBuffer *buf, PRBool *added)
{
    SECStatus rv;
    PRCList *cursor;
    unsigned int lengthOffset;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* Optimistically try to send an ECDHE key using the
     * preexisting key (in future will be keys) */
    SSL_TRC(3, ("%d: TLS13[%d]: send client key share xtn",
                SSL_GETPID(), ss->fd));

    /* Save the offset to the length. */
    rv = sslBuffer_Skip(buf, 2, &lengthOffset);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    for (cursor = PR_NEXT_LINK(&ss->ephemeralKeyPairs);
         cursor != &ss->ephemeralKeyPairs;
         cursor = PR_NEXT_LINK(cursor)) {
        sslEphemeralKeyPair *keyPair = (sslEphemeralKeyPair *)cursor;
        rv = tls13_EncodeKeyShareEntry(buf,
                                       keyPair->group->name,
                                       keyPair->keys->pubKey);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    rv = sslBuffer_InsertLength(buf, lengthOffset, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_DecodeKeyShareEntry(sslReader *rdr, TLS13KeyShareEntry **ksp)
{
    SECStatus rv;
    PRUint64 group;
    const sslNamedGroupDef *groupDef;
    TLS13KeyShareEntry *ks = NULL;
    sslReadBuffer share;

    rv = sslRead_ReadNumber(rdr, 2, &group);
    if (rv != SECSuccess) {
        goto loser;
    }
    groupDef = ssl_LookupNamedGroup(group);
    rv = sslRead_ReadVariable(rdr, 2, &share);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* This has to happen here because we want to consume
     * the entire entry even if the group is unknown
     * or disabled. */
    /* If the group is disabled, continue. */
    if (!groupDef) {
        return SECSuccess;
    }

    ks = PORT_ZNew(TLS13KeyShareEntry);
    if (!ks) {
        goto loser;
    }
    ks->group = groupDef;

    rv = SECITEM_MakeItem(NULL, &ks->key_exchange,
                          share.buf, share.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    *ksp = ks;
    return SECSuccess;

loser:
    tls13_DestroyKeyShareEntry(ks);

    return SECFailure;
}
/* Handle an incoming KeyShare extension at the client and copy to
 * |xtnData->remoteKeyShares| for future use. The key
 * share is processed in tls13_HandleServerKeyShare(). */
SECStatus
tls13_ClientHandleKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                              SECItem *data)
{
    SECStatus rv;
    PORT_Assert(PR_CLIST_IS_EMPTY(&xtnData->remoteKeyShares));
    TLS13KeyShareEntry *ks = NULL;

    PORT_Assert(!ss->sec.isServer);

    /* The server must not send this extension when negotiating < TLS 1.3. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION);
        return SECFailure;
    }

    SSL_TRC(3, ("%d: SSL3[%d]: handle key_share extension",
                SSL_GETPID(), ss->fd));

    sslReader rdr = SSL_READER(data->data, data->len);
    rv = tls13_DecodeKeyShareEntry(&rdr, &ks);
    if ((rv != SECSuccess) || !ks) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
        return SECFailure;
    }

    if (SSL_READER_REMAINING(&rdr)) {
        tls13_DestroyKeyShareEntry(ks);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
        return SECFailure;
    }
    PR_APPEND_LINK(&ks->link, &xtnData->remoteKeyShares);

    return SECSuccess;
}

SECStatus
tls13_ClientHandleKeyShareXtnHrr(const sslSocket *ss, TLSExtensionData *xtnData,
                                 SECItem *data)
{
    SECStatus rv;
    PRUint32 tmp;
    const sslNamedGroupDef *group;

    PORT_Assert(!ss->sec.isServer);
    PORT_Assert(ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3);

    SSL_TRC(3, ("%d: SSL3[%d]: handle key_share extension in HRR",
                SSL_GETPID(), ss->fd));

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &tmp, 2, &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure; /* error code already set */
    }
    if (data->len) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }

    group = ssl_LookupNamedGroup((SSLNamedGroup)tmp);
    /* If the group is not enabled, or we already have a share for the
     * requested group, abort. */
    if (!ssl_NamedGroupEnabled(ss, group) ||
        ssl_HaveEphemeralKeyPair(ss, group)) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }

    /* Now delete all the key shares per [draft-ietf-tls-tls13 S 4.1.2] */
    ssl_FreeEphemeralKeyPairs(CONST_CAST(sslSocket, ss));

    /* And replace with our new share. */
    rv = tls13_AddKeyShare(CONST_CAST(sslSocket, ss), group);
    if (rv != SECSuccess) {
        ssl3_ExtSendAlert(ss, alert_fatal, internal_error);
        PORT_SetError(SEC_ERROR_KEYGEN_FAIL);
        return SECFailure;
    }

    return SECSuccess;
}

/* Handle an incoming KeyShare extension at the server and copy to
 * |xtnData->remoteKeyShares| for future use. The key
 * share is processed in tls13_HandleClientKeyShare(). */
SECStatus
tls13_ServerHandleKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                              SECItem *data)
{
    SECStatus rv;
    PRUint32 length;

    PORT_Assert(ss->sec.isServer);
    PORT_Assert(PR_CLIST_IS_EMPTY(&xtnData->remoteKeyShares));

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    SSL_TRC(3, ("%d: SSL3[%d]: handle key_share extension",
                SSL_GETPID(), ss->fd));

    /* Redundant length because of TLS encoding (this vector consumes
     * the entire extension.) */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &length, 2, &data->data,
                                        &data->len);
    if (rv != SECSuccess)
        goto loser;
    if (length != data->len) {
        /* Check for consistency */
        PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
        goto loser;
    }

    sslReader rdr = SSL_READER(data->data, data->len);
    while (SSL_READER_REMAINING(&rdr)) {
        TLS13KeyShareEntry *ks = NULL;
        rv = tls13_DecodeKeyShareEntry(&rdr, &ks);
        if (rv != SECSuccess) {
            PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
            goto loser;
        }
        if (ks) {
            /* |ks| == NULL if this is an unknown group. */
            PR_APPEND_LINK(&ks->link, &xtnData->remoteKeyShares);
        }
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] =
        ssl_tls13_key_share_xtn;

    return SECSuccess;

loser:
    tls13_DestroyKeyShares(&xtnData->remoteKeyShares);
    return SECFailure;
}

SECStatus
tls13_ServerSendKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                            sslBuffer *buf, PRBool *added)
{
    SECStatus rv;
    sslEphemeralKeyPair *keyPair;

    /* There should be exactly one key share. */
    PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->ephemeralKeyPairs));
    PORT_Assert(PR_PREV_LINK(&ss->ephemeralKeyPairs) ==
                PR_NEXT_LINK(&ss->ephemeralKeyPairs));

    keyPair = (sslEphemeralKeyPair *)PR_NEXT_LINK(&ss->ephemeralKeyPairs);

    rv = tls13_EncodeKeyShareEntry(buf, keyPair->group->name,
                                   keyPair->keys->pubKey);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

/* Called by clients.
 *
 *      struct {
 *         opaque identity<0..2^16-1>;
 *         uint32 obfuscated_ticket_age;
 *     } PskIdentity;
 *
 *     opaque PskBinderEntry<32..255>;
 *
 *     struct {
 *         select (Handshake.msg_type) {
 *             case client_hello:
 *                 PskIdentity identities<6..2^16-1>;
 *                 PskBinderEntry binders<33..2^16-1>;
 *
 *             case server_hello:
 *                 uint16 selected_identity;
 *         };
 *
 *     } PreSharedKeyExtension;
 */
SECStatus
tls13_ClientSendPreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                sslBuffer *buf, PRBool *added)
{
    const static PRUint8 binder[TLS13_MAX_FINISHED_SIZE] = { 0 };
    unsigned int binderLen;
    unsigned int identityLen = 0;
    const PRUint8 *identity = NULL;
    PRTime age;
    SECStatus rv;

    /* Exit early if no PSKs or max version < 1.3. */
    if (PR_CLIST_IS_EMPTY(&ss->ssl3.hs.psks) ||
        ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* ...or if PSK type is resumption, but we're not resuming. */
    sslPsk *psk = (sslPsk *)PR_LIST_HEAD(&ss->ssl3.hs.psks);
    if (psk->type == ssl_psk_resume && !ss->statelessResume) {
        return SECSuccess;
    }

    /* Save where this extension starts so that if we have to add padding, it
    * can be inserted before this extension. */
    PORT_Assert(buf->len >= 4);
    xtnData->lastXtnOffset = buf->len - 4;
    PORT_Assert(psk->type == ssl_psk_resume || psk->type == ssl_psk_external);
    binderLen = tls13_GetHashSizeForHash(psk->hash);
    if (psk->type == ssl_psk_resume) {
        /* Send a single ticket identity. */
        NewSessionTicket *session_ticket = &ss->sec.ci.sid->u.ssl3.locked.sessionTicket;
        identityLen = session_ticket->ticket.len;
        identity = session_ticket->ticket.data;

        /* Obfuscated age. */
        age = ssl_Time(ss) - session_ticket->received_timestamp;
        age /= PR_USEC_PER_MSEC;
        age += session_ticket->ticket_age_add;
        PRINT_BUF(50, (ss, "Sending Resumption PSK with identity", identity, identityLen));
    } else if (psk->type == ssl_psk_external) {
        identityLen = psk->label.len;
        identity = psk->label.data;
        age = 0;
        PRINT_BUF(50, (ss, "Sending External PSK with label", identity, identityLen));
    } else {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* Length is len(identityLen) + identityLen + len(age) */
    rv = sslBuffer_AppendNumber(buf, 2 + identityLen + 4, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendVariable(buf, identity,
                                  identityLen, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_AppendNumber(buf, age, 4);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Write out the binder list length. */
    rv = sslBuffer_AppendNumber(buf, binderLen + 1, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Write zeroes for the binder for the moment. These
     * are overwritten in tls13_WriteExtensionsWithBinder. */
    rv = sslBuffer_AppendVariable(buf, binder, binderLen, 1);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (psk->type == ssl_psk_resume) {
        xtnData->sentSessionTicketInClientHello = PR_TRUE;
    }

    *added = PR_TRUE;
    return SECSuccess;

loser:
    xtnData->ticketTimestampVerified = PR_FALSE;
    return SECFailure;
}

/* Handle a TLS 1.3 PreSharedKey Extension. */
SECStatus
tls13_ServerHandlePreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                  SECItem *data)
{
    SECItem inner;
    SECStatus rv;
    unsigned int numIdentities = 0;
    unsigned int numBinders = 0;
    SECItem *appToken;

    SSL_TRC(3, ("%d: SSL3[%d]: handle pre_shared_key extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* The application token is set via the cookie extension if this is the
     * second ClientHello.  Don't set it twice.  The cookie extension handler
     * sets |helloRetry| and that will have been called already because this
     * extension always comes last. */
    if (!ss->ssl3.hs.helloRetry) {
        appToken = &xtnData->applicationToken;
    } else {
        appToken = NULL;
    }

    /* Parse the identities list. */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &inner, 2,
                                          &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    while (inner.len) {
        SECItem label;
        PRUint32 obfuscatedAge;

        rv = ssl3_ExtConsumeHandshakeVariable(ss, &label, 2,
                                              &inner.data, &inner.len);
        if (rv != SECSuccess)
            return rv;
        if (!label.len) {
            goto alert_loser;
        }

        rv = ssl3_ExtConsumeHandshakeNumber(ss, &obfuscatedAge, 4,
                                            &inner.data, &inner.len);
        if (rv != SECSuccess)
            return rv;

        if (!numIdentities) {
            /* Check any configured external PSK for a matching label.
             * If none exists, try to parse it as a ticket. */
            PORT_Assert(!xtnData->selectedPsk);
            for (PRCList *cur_p = PR_LIST_HEAD(&ss->ssl3.hs.psks);
                 cur_p != &ss->ssl3.hs.psks;
                 cur_p = PR_NEXT_LINK(cur_p)) {
                sslPsk *psk = (sslPsk *)cur_p;
                if (psk->type != ssl_psk_external ||
                    SECITEM_CompareItem(&psk->label, &label) != SECEqual) {
                    continue;
                }
                PRINT_BUF(50, (ss, "Using External PSK with label",
                               psk->label.data, psk->label.len));
                xtnData->selectedPsk = psk;
            }

            if (!xtnData->selectedPsk) {
                PRINT_BUF(50, (ss, "Handling PreSharedKey value",
                               label.data, label.len));
                rv = ssl3_ProcessSessionTicketCommon(
                    CONST_CAST(sslSocket, ss), &label, appToken);
                /* This only happens if we have an internal error, not
                * a malformed ticket. Bogus tickets just don't resume
                * and return SECSuccess. */
                if (rv != SECSuccess) {
                    return SECFailure;
                }

                if (ss->sec.ci.sid) {
                    /* xtnData->ticketAge contains the baseline we use for
                    * calculating the ticket age (i.e., our RTT estimate less the
                    * value of ticket_age_add).
                    *
                    * Add that to the obfuscated ticket age to recover the client's
                    * view of the ticket age plus the estimated RTT.
                    *
                    * See ssl3_EncodeSessionTicket() for details. */
                    xtnData->ticketAge += obfuscatedAge;

                    /* We are not committed to resumption until after unwrapping the
                    * RMS in tls13_HandleClientHelloPart2. The RPSK will be stored
                    * in ss->xtnData.selectedPsk at that point, so continue. */
                }
            }
        }

        ++numIdentities;
    }

    xtnData->pskBindersLen = data->len;

    /* Parse the binders list. */
    rv = ssl3_ExtConsumeHandshakeVariable(ss,
                                          &inner, 2, &data->data, &data->len);
    if (rv != SECSuccess)
        return SECFailure;
    if (data->len) {
        goto alert_loser;
    }

    while (inner.len) {
        SECItem binder;
        rv = ssl3_ExtConsumeHandshakeVariable(ss, &binder, 1,
                                              &inner.data, &inner.len);
        if (rv != SECSuccess)
            return rv;
        if (binder.len < 32) {
            goto alert_loser;
        }

        if (!numBinders) {
            xtnData->pskBinder = binder;
        }
        ++numBinders;
    }

    if (numBinders != numIdentities)
        goto alert_loser;

    if (ss->statelessResume) {
        PORT_Assert(!ss->xtnData.selectedPsk);
    } else if (!xtnData->selectedPsk) {
        /* No matching EPSK. */
        return SECSuccess;
    }

    xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_pre_shared_key_xtn;
    return SECSuccess;

alert_loser:
    ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
    PORT_SetError(SSL_ERROR_MALFORMED_PRE_SHARED_KEY);
    return SECFailure;
}

SECStatus
tls13_ServerSendPreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    /* We only process the first session ticket the client sends,
     * so the index is always 0. */
    rv = sslBuffer_AppendNumber(buf, 0, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

/* Handle a TLS 1.3 PreSharedKey Extension. */
SECStatus
tls13_ClientHandlePreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                  SECItem *data)
{
    PRUint32 index;
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: handle pre_shared_key extension",
                SSL_GETPID(), ss->fd));

    /* The server must not send this extension when negotiating < TLS 1.3. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION);
        return SECFailure;
    }

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &index, 2, &data->data, &data->len);
    if (rv != SECSuccess)
        return SECFailure;

    /* This should be the end of the extension. */
    if (data->len) {
        PORT_SetError(SSL_ERROR_MALFORMED_PRE_SHARED_KEY);
        return SECFailure;
    }

    /* We only sent one PSK label so index must be equal to 0 */
    if (index) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_MALFORMED_PRE_SHARED_KEY);
        return SECFailure;
    }

    PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->ssl3.hs.psks));
    sslPsk *candidate = (sslPsk *)PR_LIST_HEAD(&ss->ssl3.hs.psks);

    /* Check that the server-selected ciphersuite hash and PSK hash match. */
    if (candidate->hash != tls13_GetHashForCipherSuite(ss->ssl3.hs.cipher_suite)) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_pre_shared_key_xtn;
    xtnData->selectedPsk = candidate;

    return SECSuccess;
}

/*
 *  struct { } EarlyDataIndication;
 */
SECStatus
tls13_ClientSendEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             sslBuffer *buf, PRBool *added)
{
    if (!tls13_ClientAllow0Rtt(ss, ss->sec.ci.sid)) {
        return SECSuccess;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ServerHandleEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                               SECItem *data)
{
    SSL_TRC(3, ("%d: TLS13[%d]: handle early_data extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    if (ss->ssl3.hs.helloRetry) {
        ssl3_ExtSendAlert(ss, alert_fatal, unsupported_extension);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
        return SECFailure;
    }

    if (data->len) {
        PORT_SetError(SSL_ERROR_MALFORMED_EARLY_DATA);
        return SECFailure;
    }

    xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_early_data_xtn;

    return SECSuccess;
}

/* This will only be called if we also offered the extension. */
SECStatus
tls13_ClientHandleEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                               SECItem *data)
{
    SSL_TRC(3, ("%d: TLS13[%d]: handle early_data extension",
                SSL_GETPID(), ss->fd));

    /* The server must not send this extension when negotiating < TLS 1.3. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION);
        return SECFailure;
    }

    if (data->len) {
        PORT_SetError(SSL_ERROR_MALFORMED_EARLY_DATA);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_early_data_xtn;

    return SECSuccess;
}

SECStatus
tls13_ClientHandleTicketEarlyDataXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                     SECItem *data)
{
    PRUint32 utmp;
    SECStatus rv;

    SSL_TRC(3, ("%d: TLS13[%d]: handle ticket early_data extension",
                SSL_GETPID(), ss->fd));

    /* The server must not send this extension when negotiating < TLS 1.3. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION);
        return SECFailure;
    }

    rv = ssl3_ExtConsumeHandshake(ss, &utmp, sizeof(utmp),
                                  &data->data, &data->len);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET);
        return SECFailure;
    }
    if (data->len) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET);
        return SECFailure;
    }

    xtnData->max_early_data_size = PR_ntohl(utmp);

    return SECSuccess;
}

/*
 *     struct {
 *       select (Handshake.msg_type) {
 *       case client_hello:
 *          ProtocolVersion versions<2..254>;
 *       case server_hello:
 *          ProtocolVersion version;
 *       };
 *     } SupportedVersions;
 */
SECStatus
tls13_ClientSendSupportedVersionsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                     sslBuffer *buf, PRBool *added)
{
    PRUint16 version;
    unsigned int lengthOffset;
    SECStatus rv;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    SSL_TRC(3, ("%d: TLS13[%d]: client send supported_versions extension",
                SSL_GETPID(), ss->fd));

    rv = sslBuffer_Skip(buf, 1, &lengthOffset);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    PORT_Assert(!ss->ssl3.hs.echHpkeCtx || ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3);
    for (version = ss->vrange.max; version >= ss->vrange.min; --version) {
        PRUint16 wire = tls13_EncodeVersion(version,
                                            ss->protocolVariant);
        rv = sslBuffer_AppendNumber(buf, wire, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }

        if (ss->opt.enableDtls13VersionCompat &&
            ss->protocolVariant == ssl_variant_datagram) {
            switch (version) {
                case SSL_LIBRARY_VERSION_TLS_1_2:
                case SSL_LIBRARY_VERSION_TLS_1_1:
                    rv = sslBuffer_AppendNumber(buf, (PRUint16)version, 2);
                    break;
                default:
                    continue;
            }
            if (rv != SECSuccess) {
                return SECFailure;
            }
        }
    }

    rv = sslBuffer_InsertLength(buf, lengthOffset, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ServerSendSupportedVersionsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                     sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    SSL_TRC(3, ("%d: TLS13[%d]: server send supported_versions extension",
                SSL_GETPID(), ss->fd));

    PRUint16 ver = tls13_EncodeVersion(SSL_LIBRARY_VERSION_TLS_1_3,
                                       ss->protocolVariant);
    rv = sslBuffer_AppendNumber(buf, ver, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

/*
 *    struct {
 *        opaque cookie<1..2^16-1>;
 *    } Cookie;
 */
SECStatus
tls13_ClientHandleHrrCookie(const sslSocket *ss, TLSExtensionData *xtnData,
                            SECItem *data)
{
    SECStatus rv;

    SSL_TRC(3, ("%d: TLS13[%d]: handle cookie extension",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3);

    /* IMPORTANT: this is only valid while the HelloRetryRequest is still valid. */
    rv = ssl3_ExtConsumeHandshakeVariable(
        ss, &CONST_CAST(sslSocket, ss)->ssl3.hs.cookie, 2,
        &data->data, &data->len);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }
    if (!ss->ssl3.hs.cookie.len || data->len) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }

    return SECSuccess;
}

SECStatus
tls13_ClientSendHrrCookieXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3 ||
        !ss->ssl3.hs.cookie.len) {
        return SECSuccess;
    }

    SSL_TRC(3, ("%d: TLS13[%d]: send cookie extension", SSL_GETPID(), ss->fd));
    rv = sslBuffer_AppendVariable(buf, ss->ssl3.hs.cookie.data,
                                  ss->ssl3.hs.cookie.len, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ServerHandleCookieXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                            SECItem *data)
{
    SECStatus rv;

    SSL_TRC(3, ("%d: TLS13[%d]: handle cookie extension",
                SSL_GETPID(), ss->fd));

    rv = ssl3_ExtConsumeHandshakeVariable(ss, &xtnData->cookie, 2,
                                          &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (xtnData->cookie.len == 0) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    if (data->len) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_cookie_xtn;

    return SECSuccess;
}

SECStatus
tls13_ClientSendPostHandshakeAuthXtn(const sslSocket *ss,
                                     TLSExtensionData *xtnData,
                                     sslBuffer *buf, PRBool *added)
{
    /* Only one post-handshake message is supported: a single
     * NST immediately following the client Finished. */
    if (!IS_DTLS(ss)) {
        SSL_TRC(3, ("%d: TLS13[%d]: send post_handshake_auth extension",
                    SSL_GETPID(), ss->fd));
        *added = ss->opt.enablePostHandshakeAuth;
    }
    return SECSuccess;
}

SECStatus
tls13_ServerHandlePostHandshakeAuthXtn(const sslSocket *ss,
                                       TLSExtensionData *xtnData,
                                       SECItem *data)
{
    SSL_TRC(3, ("%d: TLS13[%d]: handle post_handshake_auth extension",
                SSL_GETPID(), ss->fd));

    if (data->len) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    /* Only one post-handshake message is supported: a single
     * NST immediately following the client Finished. */
    if (!IS_DTLS(ss)) {
        /* Keep track of negotiated extensions. */
        xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_post_handshake_auth_xtn;
    }

    return SECSuccess;
}

/*
 *     enum { psk_ke(0), psk_dhe_ke(1), (255) } PskKeyExchangeMode;
 *
 *     struct {
 *         PskKeyExchangeMode ke_modes<1..255>;
 *     } PskKeyExchangeModes;
 */
SECStatus
tls13_ClientSendPskModesXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                            sslBuffer *buf, PRBool *added)
{
    static const PRUint8 ke_modes[] = { tls13_psk_dh_ke };
    SECStatus rv;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3 ||
        ss->opt.noCache) {
        return SECSuccess;
    }

    SSL_TRC(3, ("%d: TLS13[%d]: send psk key exchange modes extension",
                SSL_GETPID(), ss->fd));

    rv = sslBuffer_AppendVariable(buf, ke_modes, sizeof(ke_modes), 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ServerHandlePskModesXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                              SECItem *data)
{
    SECStatus rv;

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    SSL_TRC(3, ("%d: TLS13[%d]: handle PSK key exchange modes extension",
                SSL_GETPID(), ss->fd));

    /* IMPORTANT: We aren't copying these values, just setting pointers.
     * They will only be valid as long as the ClientHello is in memory. */
    rv = ssl3_ExtConsumeHandshakeVariable(ss,
                                          &xtnData->psk_ke_modes, 1,
                                          &data->data, &data->len);
    if (rv != SECSuccess)
        return rv;
    if (!xtnData->psk_ke_modes.len || data->len) {
        PORT_SetError(SSL_ERROR_MALFORMED_PSK_KEY_EXCHANGE_MODES);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] =
        ssl_tls13_psk_key_exchange_modes_xtn;

    return SECSuccess;
}

SECStatus
tls13_SendCertAuthoritiesXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             sslBuffer *buf, PRBool *added)
{
    unsigned int calen;
    const SECItem *name;
    unsigned int nnames;
    SECStatus rv;

    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);

    rv = ssl_GetCertificateRequestCAs(ss, &calen, &name, &nnames);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (!calen) {
        return SECSuccess;
    }

    rv = sslBuffer_AppendNumber(buf, calen, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    while (nnames) {
        rv = sslBuffer_AppendVariable(buf, name->data, name->len, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        ++name;
        --nnames;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ClientHandleCertAuthoritiesXtn(const sslSocket *ss,
                                     TLSExtensionData *xtnData,
                                     SECItem *data)
{
    SECStatus rv;
    PLArenaPool *arena;

    if (!data->len) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CERT_REQUEST);
        return SECFailure;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    xtnData->certReqAuthorities.arena = arena;
    rv = ssl3_ParseCertificateRequestCAs((sslSocket *)ss,
                                         &data->data, &data->len,
                                         &xtnData->certReqAuthorities);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (data->len) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CERT_REQUEST);
        goto loser;
    }
    return SECSuccess;

loser:
    PORT_FreeArena(arena, PR_FALSE);
    xtnData->certReqAuthorities.arena = NULL;
    return SECFailure;
}

SECStatus
tls13_ServerSendHrrKeyShareXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                               sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);

    if (!xtnData->selectedGroup) {
        return SECSuccess;
    }

    rv = sslBuffer_AppendNumber(buf, xtnData->selectedGroup->name, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ServerSendHrrCookieXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
    PORT_Assert(xtnData->cookie.len > 0);

    rv = sslBuffer_AppendVariable(buf,
                                  xtnData->cookie.data, xtnData->cookie.len, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ClientHandleEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                         SECItem *data)
{
    SECStatus rv;
    PRCList parsedConfigs;
    PR_INIT_CLIST(&parsedConfigs);

    PORT_Assert(!xtnData->ech);
    xtnData->ech = PORT_ZNew(sslEchXtnState);
    if (!xtnData->ech) {
        return SECFailure;
    }

    /* Parse the list to determine 1) That the configs are valid
     * and properly encoded, and 2) If any are compatible. */
    rv = tls13_DecodeEchConfigs(data, &parsedConfigs);
    if (rv == SECFailure) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
        return SECFailure;
    }
    /* Don't mark ECH negotiated on rejection with retry_config.
     * Save the the raw configs so the application can retry. If
     * we sent GREASE ECH (no echHpkeCtx), don't apply retry_configs. */
    if (ss->ssl3.hs.echHpkeCtx && !PR_CLIST_IS_EMPTY(&parsedConfigs)) {
        rv = SECITEM_CopyItem(NULL, &xtnData->ech->retryConfigs, data);
    }
    tls13_DestroyEchConfigs(&parsedConfigs);

    return rv;
}

/* Indicates support for the delegated credentials extension. This should be
 * hooked while processing the ClientHello. */
SECStatus
tls13_ClientSendDelegatedCredentialsXtn(const sslSocket *ss,
                                        TLSExtensionData *xtnData,
                                        sslBuffer *buf, PRBool *added)
{
    /* Only send the extension if support is enabled and the client can
     * negotiate TLS 1.3. */
    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3 ||
        !ss->opt.enableDelegatedCredentials) {
        return SECSuccess;
    }

    /* Filter the schemes that are enabled and acceptable. Save these in
     * the "advertised" list, then encode them to be sent. If we receive
     * a DC in response, validate that it matches one of the advertised
     * schemes. */
    SSLSignatureScheme filtered[MAX_SIGNATURE_SCHEMES] = { 0 };
    unsigned int filteredCount = 0;
    SECStatus rv = ssl3_FilterSigAlgs(ss, ss->vrange.max,
                                      PR_TRUE /* disableRsae */,
                                      PR_FALSE /* forCert */,
                                      MAX_SIGNATURE_SCHEMES,
                                      filtered,
                                      &filteredCount);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* If no schemes available for the DC extension, don't send it. */
    if (!filteredCount) {
        return SECSuccess;
    }

    rv = ssl3_EncodeFilteredSigAlgs(ss, filtered, filteredCount, buf);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    SSLSignatureScheme *dcSchemesAdvertised = PORT_ZNewArray(SSLSignatureScheme,
                                                             filteredCount);
    if (!dcSchemesAdvertised) {
        return SECFailure;
    }
    for (unsigned int i = 0; i < filteredCount; i++) {
        dcSchemesAdvertised[i] = filtered[i];
    }

    if (xtnData->delegCredSigSchemesAdvertised) {
        PORT_Free(xtnData->delegCredSigSchemesAdvertised);
    }
    xtnData->delegCredSigSchemesAdvertised = dcSchemesAdvertised;
    xtnData->numDelegCredSigSchemesAdvertised = filteredCount;
    *added = PR_TRUE;
    return SECSuccess;
}

/* Parses the delegated credential (DC) offered by the server. This should be
 * hooked while processing the server's CertificateVerify.
 *
 * Only the DC sent with the end-entity certificate is to be parsed. This is
 * ensured by |tls13_HandleCertificateEntry|, which only processes extensions
 * for the first certificate in the chain.
 */
SECStatus
tls13_ClientHandleDelegatedCredentialsXtn(const sslSocket *ss,
                                          TLSExtensionData *xtnData,
                                          SECItem *data)
{
    if (!ss->opt.enableDelegatedCredentials ||
        ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
        return SECFailure;
    }

    sslDelegatedCredential *dc = NULL;
    SECStatus rv = tls13_ReadDelegatedCredential(data->data, data->len, &dc);
    if (rv != SECSuccess) {
        goto loser; /* code already set */
    }

    /* When using RSA, the public key MUST NOT use the rsaEncryption OID. */
    if (dc->expectedCertVerifyAlg == ssl_sig_rsa_pss_rsae_sha256 ||
        dc->expectedCertVerifyAlg == ssl_sig_rsa_pss_rsae_sha384 ||
        dc->expectedCertVerifyAlg == ssl_sig_rsa_pss_rsae_sha512) {
        goto alert_loser;
    }

    /* The algorithm and expected_cert_verify_algorithm fields MUST be of a
     * type advertised by the client in the SignatureSchemeList and are
     * considered invalid otherwise.  Clients that receive invalid delegated
     * credentials MUST terminate the connection with an "illegal_parameter"
     * alert. */
    PRBool found = PR_FALSE;
    for (unsigned int i = 0; i < ss->xtnData.numDelegCredSigSchemesAdvertised; ++i) {
        if (dc->expectedCertVerifyAlg == ss->xtnData.delegCredSigSchemesAdvertised[i]) {
            found = PR_TRUE;
            break;
        }
    }
    if (found == PR_FALSE) {
        goto alert_loser;
    }

    // Check the dc->alg, if necessary.
    if (dc->alg != dc->expectedCertVerifyAlg) {
        found = PR_FALSE;
        for (unsigned int i = 0; i < ss->xtnData.numDelegCredSigSchemesAdvertised; ++i) {
            if (dc->alg == ss->xtnData.delegCredSigSchemesAdvertised[i]) {
                found = PR_TRUE;
                break;
            }
        }
        if (found == PR_FALSE) {
            goto alert_loser;
        }
    }

    xtnData->peerDelegCred = dc;
    xtnData->negotiated[xtnData->numNegotiated++] =
        ssl_delegated_credentials_xtn;
    return SECSuccess;
alert_loser:
    ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
    PORT_SetError(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
loser:
    tls13_DestroyDelegatedCredential(dc);
    return SECFailure;
}

/* Adds the DC extension if we're committed to authenticating with a DC. */
static SECStatus
tls13_ServerSendDelegatedCredentialsXtn(const sslSocket *ss,
                                        TLSExtensionData *xtnData,
                                        sslBuffer *buf, PRBool *added)
{
    if (tls13_IsSigningWithDelegatedCredential(ss)) {
        const SECItem *dc = &ss->sec.serverCert->delegCred;
        SECStatus rv;
        rv = sslBuffer_Append(buf, dc->data, dc->len);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        *added = PR_TRUE;
    }
    return SECSuccess;
}

/* The client has indicated support of DCs. We can't act on this information
 * until we've committed to signing with a DC, so just set a callback for
 * sending the DC extension later. */
SECStatus
tls13_ServerHandleDelegatedCredentialsXtn(const sslSocket *ss,
                                          TLSExtensionData *xtnData,
                                          SECItem *data)
{
    if (xtnData->delegCredSigSchemes) {
        PORT_Free(xtnData->delegCredSigSchemes);
        xtnData->delegCredSigSchemes = NULL;
        xtnData->numDelegCredSigSchemes = 0;
    }
    SECStatus rv = ssl_ParseSignatureSchemes(ss, NULL,
                                             &xtnData->delegCredSigSchemes,
                                             &xtnData->numDelegCredSigSchemes,
                                             &data->data, &data->len);
    if (rv != SECSuccess) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }
    if (xtnData->numDelegCredSigSchemes == 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, handshake_failure);
        PORT_SetError(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
        return SECFailure;
    }
    /* Check for trailing data. */
    if (data->len != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->peerRequestedDelegCred = PR_TRUE;
    xtnData->negotiated[xtnData->numNegotiated++] =
        ssl_delegated_credentials_xtn;

    return ssl3_RegisterExtensionSender(
        ss, xtnData, ssl_delegated_credentials_xtn,
        tls13_ServerSendDelegatedCredentialsXtn);
}

/* Adds the ECH extension containing server retry_configs */
SECStatus
tls13_ServerSendEchXtn(const sslSocket *ss,
                       TLSExtensionData *xtnData,
                       sslBuffer *buf, PRBool *added)
{
    SECStatus rv;
    PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
    if (PR_CLIST_IS_EMPTY(&ss->echConfigs)) {
        return SECSuccess;
    }

    const sslEchConfig *cfg = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);
    rv = sslBuffer_AppendVariable(buf, cfg->raw.data, cfg->raw.len, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ServerHandleEchXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                         SECItem *data)
{
    SECStatus rv;
    HpkeKdfId kdf;
    HpkeAeadId aead;
    PRUint32 tmp;
    PRUint8 configId;
    SECItem senderPubKey;
    SECItem encryptedCh;

    /* Ignore it if not doing 1.3+. If we have no ECHConfigs,
     * proceed to save the config_id for HRR validation. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3 ||
        IS_DTLS(ss)) {
        return SECSuccess;
    }

    if (ss->ssl3.hs.echAccepted) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
        return SECFailure;
    }

    if (ssl3_FindExtension(CONST_CAST(sslSocket, ss), ssl_tls13_ech_is_inner_xtn)) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
        return SECFailure;
    }

    PORT_Assert(!xtnData->ech);
    xtnData->ech = PORT_ZNew(sslEchXtnState);
    if (!xtnData->ech) {
        return SECFailure;
    }

    /* Parse the KDF and AEAD. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &tmp, 2,
                                        &data->data, &data->len);
    if (rv != SECSuccess) {
        goto alert_loser;
    }
    kdf = (HpkeKdfId)tmp;
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &tmp, 2,
                                        &data->data, &data->len);
    if (rv != SECSuccess) {
        goto alert_loser;
    }
    aead = (HpkeAeadId)tmp;

    /* config_id */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &tmp, 1,
                                        &data->data, &data->len);
    if (rv != SECSuccess) {
        goto alert_loser;
    }
    configId = tmp;

    /* enc */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &senderPubKey, 2,
                                          &data->data, &data->len);
    if (rv != SECSuccess) {
        goto alert_loser;
    }

    /* payload, which must be final and non-empty. */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &encryptedCh, 2,
                                          &data->data, &data->len);
    if (rv != SECSuccess) {
        goto alert_loser;
    }
    if (data->len || !encryptedCh.len) {
        goto alert_loser;
    }

    if (!ss->ssl3.hs.helloRetry) {
        /* In the real ECH HRR case, config_id and enc should be empty. This
         * is checked after acceptance, because it might be GREASE ECH. */
        if (!senderPubKey.len) {
            goto alert_loser;
        }

        rv = SECITEM_CopyItem(NULL, &xtnData->ech->senderPubKey, &senderPubKey);
        if (rv == SECFailure) {
            return SECFailure;
        }
    }

    rv = SECITEM_CopyItem(NULL, &xtnData->ech->innerCh, &encryptedCh);
    if (rv == SECFailure) {
        return SECFailure;
    }
    xtnData->ech->configId = configId;
    xtnData->ech->kdfId = kdf;
    xtnData->ech->aeadId = aead;

    /* Not negotiated until tls13_MaybeAcceptEch. */
    return SECSuccess;

alert_loser:
    ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
    PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
    return SECFailure;
}

SECStatus
tls13_ServerHandleEchIsInnerXtn(const sslSocket *ss,
                                TLSExtensionData *xtnData,
                                SECItem *data)
{
    SSL_TRC(3, ("%d: TLS13[%d]: handle ech_is_inner extension",
                SSL_GETPID(), ss->fd));

    if (data->len) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
        return SECFailure;
    }

    if (ssl3_FindExtension(CONST_CAST(sslSocket, ss), ssl_tls13_encrypted_client_hello_xtn)) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
        return SECFailure;
    }

    /* Consider encrypted_client_hello_xtn negotiated if we performed the
     * CHOuter decryption. This is only supported in shared mode, so we'll also
     * handle ech_is_inner in that case. We might, however, receive a CHInner
     * that was forwarded by a different client-facing server. In this case,
     * mark ech_is_inner as negotiated, which triggers sending of the ECH
     * acceptance signal. ech_is_inner_xtn being negotiated does not imply
     * that any other ECH state actually exists. */
    if (ss->ssl3.hs.echAccepted) {
        xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_encrypted_client_hello_xtn;
    }
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_ech_is_inner_xtn;
    return SECSuccess;
}
