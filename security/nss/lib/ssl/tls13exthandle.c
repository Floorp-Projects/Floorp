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
#include "tls13esni.h"
#include "tls13exthandle.h"

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
    unsigned int extStart;
    unsigned int lengthOffset;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* Optimistically try to send an ECDHE key using the
     * preexisting key (in future will be keys) */
    SSL_TRC(3, ("%d: TLS13[%d]: send client key share xtn",
                SSL_GETPID(), ss->fd));

    extStart = SSL_BUFFER_LEN(buf);

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

    rv = SECITEM_MakeItem(NULL, &xtnData->keyShareExtension,
                          SSL_BUFFER_BASE(buf) + extStart,
                          SSL_BUFFER_LEN(buf) - extStart);
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

 * Presently the only way to get a PSK is by resumption, so this is
 * really a ticket label and there will be at most one.
 */
SECStatus
tls13_ClientSendPreSharedKeyXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                sslBuffer *buf, PRBool *added)
{
    NewSessionTicket *session_ticket;
    PRTime age;
    const static PRUint8 binder[TLS13_MAX_FINISHED_SIZE] = { 0 };
    unsigned int binderLen;
    SECStatus rv;

    /* We only set statelessResume on the client in TLS 1.3 code. */
    if (!ss->statelessResume) {
        return SECSuccess;
    }

    /* Save where this extension starts so that if we have to add padding, it
     * can be inserted before this extension. */
    PORT_Assert(buf->len >= 4);
    xtnData->lastXtnOffset = buf->len - 4;

    PORT_Assert(ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3);
    PORT_Assert(ss->sec.ci.sid->version >= SSL_LIBRARY_VERSION_TLS_1_3);

    /* Send a single ticket identity. */
    session_ticket = &ss->sec.ci.sid->u.ssl3.locked.sessionTicket;
    rv = sslBuffer_AppendNumber(buf, 2 +                              /* identity length */
                                         session_ticket->ticket.len + /* ticket */
                                         4 /* obfuscated_ticket_age */,
                                2);
    if (rv != SECSuccess)
        goto loser;
    rv = sslBuffer_AppendVariable(buf, session_ticket->ticket.data,
                                  session_ticket->ticket.len, 2);
    if (rv != SECSuccess)
        goto loser;

    /* Obfuscated age. */
    age = ssl_TimeUsec() - session_ticket->received_timestamp;
    age /= PR_USEC_PER_MSEC;
    age += session_ticket->ticket_age_add;
    rv = sslBuffer_AppendNumber(buf, age, 4);
    if (rv != SECSuccess)
        goto loser;

    /* Write out the binder list length. */
    binderLen = tls13_GetHashSize(ss);
    rv = sslBuffer_AppendNumber(buf, binderLen + 1, 2);
    if (rv != SECSuccess)
        goto loser;
    /* Write zeroes for the binder for the moment. */
    rv = sslBuffer_AppendVariable(buf, binder, binderLen, 1);
    if (rv != SECSuccess)
        goto loser;

    PRINT_BUF(50, (ss, "Sending PreSharedKey value",
                   session_ticket->ticket.data,
                   session_ticket->ticket.len));

    xtnData->sentSessionTicketInClientHello = PR_TRUE;
    *added = PR_TRUE;
    return SECSuccess;

loser:
    xtnData->ticketTimestampVerified = PR_FALSE;
    return SECFailure;
}

/* Handle a TLS 1.3 PreSharedKey Extension. We only accept PSKs
 * that contain session tickets. */
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
            PRINT_BUF(50, (ss, "Handling PreSharedKey value",
                           label.data, label.len));
            rv = ssl3_ProcessSessionTicketCommon(
                CONST_CAST(sslSocket, ss), &label, appToken);
            /* This only happens if we have an internal error, not
             * a malformed ticket. Bogus tickets just don't resume
             * and return SECSuccess. */
            if (rv != SECSuccess)
                return SECFailure;

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

    /* Keep track of negotiated extensions. Note that this does not
     * mean we are resuming. */
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

/* Handle a TLS 1.3 PreSharedKey Extension. We only accept PSKs
 * that contain session tickets. */
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
        PORT_SetError(SSL_ERROR_MALFORMED_PRE_SHARED_KEY);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_tls13_pre_shared_key_xtn;

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

    for (version = ss->vrange.max; version >= ss->vrange.min; --version) {
        PRUint16 wire = tls13_EncodeDraftVersion(version,
                                                 ss->protocolVariant);
        rv = sslBuffer_AppendNumber(buf, wire, 2);
        if (rv != SECSuccess) {
            return SECFailure;
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

    PRUint16 ver = tls13_EncodeDraftVersion(SSL_LIBRARY_VERSION_TLS_1_3,
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
tls13_ClientSendEsniXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                        sslBuffer *buf, PRBool *added)
{
    SECStatus rv;
    PRUint8 sniBuf[1024];
    PRUint8 hash[64];
    sslBuffer sni = SSL_BUFFER(sniBuf);
    const ssl3CipherSuiteDef *suiteDef;
    ssl3KeyMaterial keyMat;
    SSLAEADCipher aead;
    PRUint8 outBuf[1024];
    int outLen;
    unsigned int sniStart;
    unsigned int sniLen;
    sslBuffer aadInput = SSL_BUFFER_EMPTY;
    unsigned int keyShareBufStart;
    unsigned int keyShareBufLen;

    PORT_Memset(&keyMat, 0, sizeof(keyMat));

    if (!ss->xtnData.esniPrivateKey) {
        return SECSuccess;
    }

    /* nonce */
    rv = PK11_GenerateRandom(
        (unsigned char *)xtnData->esniNonce, sizeof(xtnData->esniNonce));
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Append(&sni, xtnData->esniNonce, sizeof(xtnData->esniNonce));
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* sni */
    sniStart = SSL_BUFFER_LEN(&sni);
    rv = ssl3_ClientFormatServerNameXtn(ss, ss->url, xtnData, &sni);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    sniLen = SSL_BUFFER_LEN(&sni) - sniStart;
    /* Padding. */
    if (ss->esniKeys->paddedLength > sniLen) {
        unsigned int paddingRequired = ss->esniKeys->paddedLength - sniLen;
        while (paddingRequired--) {
            rv = sslBuffer_AppendNumber(&sni, 0, 1);
            if (rv != SECSuccess) {
                return SECFailure;
            }
        }
    }

    suiteDef = ssl_LookupCipherSuiteDef(xtnData->esniSuite);
    PORT_Assert(suiteDef);
    if (!suiteDef) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    aead = tls13_GetAead(ssl_GetBulkCipherDef(suiteDef));
    if (!aead) {
        return SECFailure;
    }

    /* Format the first part of the extension so we have the
     * encoded KeyShareEntry. */
    rv = sslBuffer_AppendNumber(buf, xtnData->esniSuite, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    keyShareBufStart = SSL_BUFFER_LEN(buf);
    rv = tls13_EncodeKeyShareEntry(buf,
                                   xtnData->esniPrivateKey->group->name,
                                   xtnData->esniPrivateKey->keys->pubKey);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    keyShareBufLen = SSL_BUFFER_LEN(buf) - keyShareBufStart;

    if (tls13_GetHashSizeForHash(suiteDef->prf_hash) > sizeof(hash)) {
        PORT_Assert(PR_FALSE);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    rv = PK11_HashBuf(ssl3_HashTypeToOID(suiteDef->prf_hash),
                      hash,
                      ss->esniKeys->data.data,
                      ss->esniKeys->data.len);
    if (rv != SECSuccess) {
        PORT_Assert(PR_FALSE);
        return SECFailure;
    }

    rv = sslBuffer_AppendVariable(buf, hash,
                                  tls13_GetHashSizeForHash(suiteDef->prf_hash), 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Compute the ESNI keys. */
    rv = tls13_ComputeESNIKeys(ss, xtnData->peerEsniShare,
                               xtnData->esniPrivateKey->keys,
                               suiteDef,
                               hash,
                               SSL_BUFFER_BASE(buf) + keyShareBufStart,
                               keyShareBufLen,
                               CONST_CAST(PRUint8, ss->ssl3.hs.client_random),
                               &keyMat);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    rv = tls13_FormatEsniAADInput(&aadInput,
                                  xtnData->keyShareExtension.data,
                                  xtnData->keyShareExtension.len);
    if (rv != SECSuccess) {
        ssl_DestroyKeyMaterial(&keyMat);
        return SECFailure;
    }
    /* Now encrypt. */
    rv = aead(&keyMat, PR_FALSE /* Encrypt */,
              outBuf, &outLen, sizeof(outBuf),
              SSL_BUFFER_BASE(&sni),
              SSL_BUFFER_LEN(&sni),
              SSL_BUFFER_BASE(&aadInput),
              SSL_BUFFER_LEN(&aadInput));
    ssl_DestroyKeyMaterial(&keyMat);
    sslBuffer_Clear(&aadInput);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Encode the rest. */
    rv = sslBuffer_AppendVariable(buf, outBuf, outLen, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

static SECStatus
tls13_ServerSendEsniXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                        sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    rv = sslBuffer_Append(buf, xtnData->esniNonce, sizeof(xtnData->esniNonce));
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
tls13_ServerHandleEsniXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                          SECItem *data)
{
    sslReadBuffer buf;
    PRUint8 *plainText = NULL;
    int ptLen;
    SECStatus rv;

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    if (!ss->esniKeys) {
        /* Apparently we used to be configured for ESNI, but
         * no longer. This violates the spec, or the client is
         * broken. */
        return SECFailure;
    }

    plainText = PORT_ZAlloc(data->len);
    if (!plainText) {
        return SECFailure;
    }
    rv = tls13_ServerDecryptEsniXtn(ss, data->data, data->len,
                                    plainText, &ptLen, data->len);
    if (rv) {
        goto loser;
    }

    /* Read out the interior extension. */
    sslReader sniRdr = SSL_READER(plainText, ptLen);

    rv = sslRead_Read(&sniRdr, sizeof(xtnData->esniNonce), &buf);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_Memcpy(xtnData->esniNonce, buf.buf, sizeof(xtnData->esniNonce));

    /* We need to capture the whole block with the length. */
    SECItem sniItem = { siBuffer, (unsigned char *)SSL_READER_CURRENT(&sniRdr), 0 };
    rv = sslRead_ReadVariable(&sniRdr, 2, &buf);
    if (rv != SECSuccess) {
        goto loser;
    }
    sniItem.len = buf.len + 2;

    /* Check the padding. Note we don't need to do this in constant time
     * because it's inside the AEAD boundary. */
    /* TODO(ekr@rtfm.com): check that the padding is the right length. */
    PRUint64 tmp;
    while (SSL_READER_REMAINING(&sniRdr)) {
        rv = sslRead_ReadNumber(&sniRdr, 1, &tmp);
        if (tmp != 0) {
            goto loser;
        }
    }

    rv = ssl3_HandleServerNameXtn(ss, xtnData, &sniItem);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = ssl3_RegisterExtensionSender(ss, xtnData,
                                      ssl_tls13_encrypted_sni_xtn,
                                      tls13_ServerSendEsniXtn);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] =
        ssl_tls13_encrypted_sni_xtn;

    PORT_ZFree(plainText, data->len);
    return SECSuccess;
loser:
    PORT_ZFree(plainText, data->len);
    return SECFailure;
}

/* Function to check the extension. We don't install a handler here
 * because we need to check for the presence of the extension as
 * well and it's easier to do it in one place. */
SECStatus
tls13_ClientCheckEsniXtn(sslSocket *ss)
{
    TLSExtension *esniExtension =
        ssl3_FindExtension(ss, ssl_tls13_encrypted_sni_xtn);
    if (!esniExtension) {
        FATAL_ERROR(ss, SSL_ERROR_MISSING_ESNI_EXTENSION, missing_extension);
        return SECFailure;
    }

    if (esniExtension->data.len != sizeof(ss->xtnData.esniNonce)) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ESNI_EXTENSION, illegal_parameter);
        return SECFailure;
    }

    if (0 != NSS_SecureMemcmp(esniExtension->data.data,
                              ss->xtnData.esniNonce,
                              sizeof(ss->xtnData.esniNonce))) {
        FATAL_ERROR(ss, SSL_ERROR_RX_MALFORMED_ESNI_EXTENSION, illegal_parameter);
        return SECFailure;
    }

    return SECSuccess;
}
