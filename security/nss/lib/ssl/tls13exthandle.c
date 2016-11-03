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
#include "ssl3exthandle.h"
#include "tls13exthandle.h"

PRInt32
tls13_ServerSendStatusRequestXtn(
    sslSocket *ss,
    PRBool append,
    PRUint32 maxBytes)
{
    PRInt32 extension_length;
    const sslServerCert *serverCert = ss->sec.serverCert;
    const SECItem *item;
    SECStatus rv;

    if (!serverCert->certStatusArray ||
        !serverCert->certStatusArray->len) {
        return 0;
    }

    item = &serverCert->certStatusArray->items[0];

    /* Only send the first entry. */
    extension_length = 2 + 2 + 1 /* status_type */ + 3 + item->len;
    if (maxBytes < (PRUint32)extension_length) {
        return 0;
    }
    if (append) {
        /* extension_type */
        rv = ssl3_AppendHandshakeNumber(ss, ssl_cert_status_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* length of extension_data */
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            return -1;
        /* status_type == ocsp */
        rv = ssl3_AppendHandshakeNumber(ss, 1 /*ocsp*/, 1);
        if (rv != SECSuccess)
            return rv; /* err set by AppendHandshake. */
        /* opaque OCSPResponse<1..2^24-1> */
        rv = ssl3_AppendHandshakeVariable(ss, item->data, item->len, 3);
        if (rv != SECSuccess)
            return rv; /* err set by AppendHandshake. */
    }

    return extension_length;
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

PRUint32
tls13_SizeOfClientKeyShareExtension(sslSocket *ss)
{
    PRCList *cursor;
    /* Size is: extension(2) + extension_len(2) + client_shares(2) */
    PRUint32 size = 2 + 2 + 2;
    for (cursor = PR_NEXT_LINK(&ss->ephemeralKeyPairs);
         cursor != &ss->ephemeralKeyPairs;
         cursor = PR_NEXT_LINK(cursor)) {
        sslEphemeralKeyPair *keyPair = (sslEphemeralKeyPair *)cursor;
        size += tls13_SizeOfKeyShareEntry(keyPair->keys->pubKey);
    }
    return size;
}

SECStatus
tls13_EncodeKeyShareEntry(sslSocket *ss, const sslEphemeralKeyPair *keyPair)
{
    SECStatus rv;
    SECKEYPublicKey *pubKey = keyPair->keys->pubKey;
    unsigned int size = tls13_SizeOfKeyShareEntry(pubKey);

    rv = ssl3_AppendHandshakeNumber(ss, keyPair->group->name, 2);
    if (rv != SECSuccess)
        return rv;
    rv = ssl3_AppendHandshakeNumber(ss, size - 4, 2);
    if (rv != SECSuccess)
        return rv;

    switch (pubKey->keyType) {
        case ecKey:
            rv = tls13_EncodeECDHEKeyShareKEX(ss, pubKey);
            break;
        case dhKey:
            rv = ssl_AppendPaddedDHKeyShare(ss, pubKey, PR_FALSE);
            break;
        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            break;
    }

    return rv;
}

PRInt32
tls13_ClientSendKeyShareXtn(sslSocket *ss, PRBool append,
                            PRUint32 maxBytes)
{
    PRUint32 extension_length;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3) {
        return 0;
    }

    /* Optimistically try to send an ECDHE key using the
     * preexisting key (in future will be keys) */
    SSL_TRC(3, ("%d: TLS13[%d]: send client key share xtn",
                SSL_GETPID(), ss->fd));

    extension_length = tls13_SizeOfClientKeyShareExtension(ss);
    if (maxBytes < extension_length) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        SECStatus rv;
        PRCList *cursor;

        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_key_share_xtn, 2);
        if (rv != SECSuccess)
            goto loser;

        /* The extension length */
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            goto loser;

        /* The length of KeyShares */
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 6, 2);
        if (rv != SECSuccess)
            goto loser;

        for (cursor = PR_NEXT_LINK(&ss->ephemeralKeyPairs);
             cursor != &ss->ephemeralKeyPairs;
             cursor = PR_NEXT_LINK(cursor)) {
            sslEphemeralKeyPair *keyPair = (sslEphemeralKeyPair *)cursor;
            rv = tls13_EncodeKeyShareEntry(ss, keyPair);
            if (rv != SECSuccess)
                goto loser;
        }

        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_tls13_key_share_xtn;
    }

    return extension_length;

loser:
    return -1;
}

SECStatus
tls13_HandleKeyShareEntry(sslSocket *ss, SECItem *data)
{
    SECStatus rv;
    PRInt32 group;
    const sslNamedGroupDef *groupDef;
    TLS13KeyShareEntry *ks = NULL;
    SECItem share = { siBuffer, NULL, 0 };

    group = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    if (group < 0) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
        goto loser;
    }
    groupDef = ssl_LookupNamedGroup(group);
    rv = ssl3_ConsumeHandshakeVariable(ss, &share, 2, &data->data,
                                       &data->len);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* If the group is disabled, continue. */
    if (!groupDef) {
        return SECSuccess;
    }

    ks = PORT_ZNew(TLS13KeyShareEntry);
    if (!ks)
        goto loser;
    ks->group = groupDef;

    rv = SECITEM_CopyItem(NULL, &ks->key_exchange, &share);
    if (rv != SECSuccess)
        goto loser;

    PR_APPEND_LINK(&ks->link, &ss->ssl3.hs.remoteKeyShares);
    return SECSuccess;

loser:
    if (ks)
        tls13_DestroyKeyShareEntry(ks);
    return SECFailure;
}

/* Handle an incoming KeyShare extension at the client and copy to
 * |ss->ssl3.hs.remoteKeyShares| for future use. The key
 * share is processed in tls13_HandleServerKeyShare(). */
SECStatus
tls13_ClientHandleKeyShareXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;

    PORT_Assert(!ss->sec.isServer);
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        /* This can't happen because the extension processing
         * code filters out TLS 1.3 extensions when not in
         * TLS 1.3 mode. */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    SSL_TRC(3, ("%d: SSL3[%d]: handle key_share extension",
                SSL_GETPID(), ss->fd));

    rv = tls13_HandleKeyShareEntry(ss, data);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
        return SECFailure;
    }

    if (data->len) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
        return SECFailure;
    }

    return SECSuccess;
}

SECStatus
tls13_ClientHandleKeyShareXtnHrr(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    PRInt32 tmp;
    const sslNamedGroupDef *group;

    PORT_Assert(!ss->sec.isServer);
    PORT_Assert(ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3);

    SSL_TRC(3, ("%d: SSL3[%d]: handle key_share extension in HRR",
                SSL_GETPID(), ss->fd));

    tmp = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    if (tmp < 0) {
        return SECFailure; /* error code already set */
    }
    if (data->len) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }

    group = ssl_LookupNamedGroup((SSLNamedGroup)tmp);
    /* If the group is not enabled, or we already have a share for the
     * requested group, abort. */
    if (!ssl_NamedGroupEnabled(ss, group) ||
        ssl_LookupEphemeralKeyPair(ss, group)) {
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }

    rv = tls13_CreateKeyShare(ss, group);
    if (rv != SECSuccess) {
        (void)SSL3_SendAlert(ss, alert_fatal, internal_error);
        PORT_SetError(SEC_ERROR_KEYGEN_FAIL);
        return SECFailure;
    }

    return SECSuccess;
}

/* Handle an incoming KeyShare extension at the server and copy to
 * |ss->ssl3.hs.remoteKeyShares| for future use. The key
 * share is processed in tls13_HandleClientKeyShare(). */
SECStatus
tls13_ServerHandleKeyShareXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    PRInt32 length;

    PORT_Assert(ss->sec.isServer);
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    SSL_TRC(3, ("%d: SSL3[%d]: handle key_share extension",
                SSL_GETPID(), ss->fd));

    /* Redundant length because of TLS encoding (this vector consumes
     * the entire extension.) */
    length = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data,
                                         &data->len);
    if (length < 0)
        goto loser;
    if (length != data->len) {
        /* Check for consistency */
        PORT_SetError(SSL_ERROR_RX_MALFORMED_KEY_SHARE);
        goto loser;
    }

    while (data->len) {
        rv = tls13_HandleKeyShareEntry(ss, data);
        if (rv != SECSuccess)
            goto loser;
    }
    return SECSuccess;

loser:
    tls13_DestroyKeyShares(&ss->ssl3.hs.remoteKeyShares);
    return SECFailure;
}

PRInt32
tls13_ServerSendKeyShareXtn(sslSocket *ss, PRBool append,
                            PRUint32 maxBytes)
{
    PRUint32 extension_length;
    PRUint32 entry_length;
    SECStatus rv;
    sslEphemeralKeyPair *keyPair;

    /* There should be exactly one key share. */
    PORT_Assert(!PR_CLIST_IS_EMPTY(&ss->ephemeralKeyPairs));
    PORT_Assert(PR_PREV_LINK(&ss->ephemeralKeyPairs) ==
                PR_NEXT_LINK(&ss->ephemeralKeyPairs));

    keyPair = (sslEphemeralKeyPair *)PR_NEXT_LINK(&ss->ephemeralKeyPairs);

    entry_length = tls13_SizeOfKeyShareEntry(keyPair->keys->pubKey);
    extension_length = 2 + 2 + entry_length; /* Type + length + entry_length */
    if (maxBytes < extension_length) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_key_share_xtn, 2);
        if (rv != SECSuccess)
            goto loser;

        rv = ssl3_AppendHandshakeNumber(ss, entry_length, 2);
        if (rv != SECSuccess)
            goto loser;

        rv = tls13_EncodeKeyShareEntry(ss, keyPair);
        if (rv != SECSuccess)
            goto loser;
    }

    return extension_length;

loser:
    return -1;
}

/* Called by clients.
 *
 *   struct {
 *     PskKeyExchangeMode ke_modes<1..255>;
 *     PskAuthMode auth_modes<1..255>;
 *     opaque identity<0..2^16-1>;
 *  } PskIdentity;
 *
 *  struct {
 *       select (Role) {
 *           case client:
 *               PskIdentity identities<2..2^16-1>;
 *            case server:
 *               uint16 selected_identity;
 *       }
 *   } PreSharedKeyExtension;
 *
 * Presently the only way to get a PSK is by resumption, so this is
 * really a ticket label and there wll be at most one.
 */
PRInt32
tls13_ClientSendPreSharedKeyXtn(sslSocket *ss,
                                PRBool append,
                                PRUint32 maxBytes)
{
    PRInt32 extension_length;
    static const PRUint8 auth_modes[] = { tls13_psk_auth };
    static const unsigned long auth_modes_len = sizeof(auth_modes);
    static const PRUint8 ke_modes[] = { tls13_psk_dh_ke };
    static const unsigned long ke_modes_len = sizeof(ke_modes);
    NewSessionTicket *session_ticket;

    /* We only set statelessResume on the client in TLS 1.3 code. */
    if (!ss->statelessResume)
        return 0;

    PORT_Assert(ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3);

    session_ticket = &ss->sec.ci.sid->u.ssl3.locked.sessionTicket;

    extension_length =
        2 + 2 + 2 +                     /* Type + length + vector length */
        1 + ke_modes_len +              /* key exchange modes vector */
        1 + auth_modes_len +            /* auth modes vector */
        2 + session_ticket->ticket.len; /* identity length + ticket len */

    if (maxBytes < (PRUint32)extension_length) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        SECStatus rv;
        /* extension_type */
        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_pre_shared_key_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 6, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeVariable(ss, ke_modes, ke_modes_len, 1);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeVariable(ss, auth_modes, auth_modes_len, 1);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeVariable(ss, session_ticket->ticket.data,
                                          session_ticket->ticket.len, 2);
        PRINT_BUF(50, (ss, "Sending PreSharedKey value",
                       session_ticket->ticket.data,
                       session_ticket->ticket.len));
        ss->xtnData.sentSessionTicketInClientHello = PR_TRUE;
        if (rv != SECSuccess)
            goto loser;

        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_tls13_pre_shared_key_xtn;
    }
    return extension_length;

loser:
    ss->xtnData.ticketTimestampVerified = PR_FALSE;
    return -1;
}

/* Handle a TLS 1.3 PreSharedKey Extension. We only accept PSKs
 * that contain session tickets. */
SECStatus
tls13_ServerHandlePreSharedKeyXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    PRInt32 len;
    PRBool first = PR_TRUE;
    SECStatus rv;

    SSL_TRC(3, ("%d: SSL3[%d]: handle pre_shared_key extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    len = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    if (len < 0)
        return SECFailure;

    if (len != data->len) {
        PORT_SetError(SSL_ERROR_MALFORMED_PRE_SHARED_KEY);
        return SECFailure;
    }

    while (data->len) {
        SECItem label;

        /* IMPORTANT: We aren't copying these values, just setting pointers.
         * They will only be valid as long as the ClientHello is in memory. */
        rv = ssl3_ConsumeHandshakeVariable(ss, &ss->xtnData.psk_ke_modes, 1,
                                           &data->data, &data->len);
        if (rv != SECSuccess)
            return rv;
        if (!ss->xtnData.psk_ke_modes.len) {
            goto alert_loser;
        }
        rv = ssl3_ConsumeHandshakeVariable(ss, &ss->xtnData.psk_auth_modes, 1,
                                           &data->data, &data->len);
        if (rv != SECSuccess)
            return rv;
        if (!ss->xtnData.psk_auth_modes.len) {
            goto alert_loser;
        }
        rv = ssl3_ConsumeHandshakeVariable(ss, &label, 2,
                                           &data->data, &data->len);
        if (rv != SECSuccess)
            return rv;
        if (!label.len) {
            goto alert_loser;
        }
        if (first) {
            first = PR_FALSE; /* Continue to read through the extension to check
                               * the format. */

            PRINT_BUF(50, (ss, "Handling PreSharedKey value",
                           label.data, label.len));
            rv = ssl3_ProcessSessionTicketCommon(ss, &label);
            /* This only happens if we have an internal error, not
             * a malformed ticket. Bogus tickets just don't resume
             * and return SECSuccess. */
            if (rv != SECSuccess)
                return rv;
        }
    }

    /* Keep track of negotiated extensions. Note that this does not
     * mean we are resuming. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
    PORT_SetError(SSL_ERROR_MALFORMED_PRE_SHARED_KEY);
    return SECFailure;
}

PRInt32
tls13_ServerSendPreSharedKeyXtn(sslSocket *ss,
                                PRBool append,
                                PRUint32 maxBytes)
{
    PRInt32 extension_length =
        2 + 2 + 2; /* type + len + index */
    SECStatus rv;

    if (maxBytes < (PRUint32)extension_length) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_pre_shared_key_xtn, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeNumber(ss, 2, 2);
        if (rv != SECSuccess)
            return -1;

        /* We only process the first session ticket the client sends,
         * so the index is always 0. */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
    }

    return extension_length;
}

/* Handle a TLS 1.3 PreSharedKey Extension. We only accept PSKs
 * that contain session tickets. */
SECStatus
tls13_ClientHandlePreSharedKeyXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    PRInt32 index;

    SSL_TRC(3, ("%d: SSL3[%d]: handle pre_shared_key extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    index = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    if (index < 0)
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
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    return SECSuccess;
}

/*
 *  struct {
 *       select (Role) {
 *           case client:
 *               uint32 obfuscated_ticket_age;
 *
 *           case server:
 *              struct {};
 *       }
 *   } EarlyDataIndication;
 */
PRInt32
tls13_ClientSendEarlyDataXtn(sslSocket *ss,
                             PRBool append,
                             PRUint32 maxBytes)
{
    PRInt32 extension_length;
    SECStatus rv;
    NewSessionTicket *session_ticket;

    if (!tls13_ClientAllow0Rtt(ss, ss->sec.ci.sid))
        return 0;

    /* type + length + obfuscated ticket age. */
    extension_length = 2 + 2 + 4;

    if (maxBytes < (PRUint32)extension_length) {
        PORT_Assert(0);
        return 0;
    }

    session_ticket = &ss->sec.ci.sid->u.ssl3.locked.sessionTicket;
    if (append) {
        PRUint32 age;

        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_early_data_xtn, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            return -1;

        /* Obfuscated age. */
        age = ssl_Time() - session_ticket->received_timestamp;
        age += session_ticket->ticket_age_add;

        rv = ssl3_AppendHandshakeNumber(ss, age, 4);
        if (rv != SECSuccess)
            return -1;
    }

    ss->ssl3.hs.zeroRttState = ssl_0rtt_sent;
    ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
        ssl_tls13_early_data_xtn;

    return extension_length;
}

SECStatus
tls13_ServerHandleEarlyDataXtn(sslSocket *ss, PRUint16 ex_type,
                               SECItem *data)
{
    PRUint32 obfuscated_ticket_age;
    SECStatus rv;

    SSL_TRC(3, ("%d: TLS13[%d]: handle early_data extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* Obfuscated ticket age. Ignore. Bug 1295163. */
    rv = ssl3_ConsumeHandshake(ss, &obfuscated_ticket_age, 4,
                               &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (data->len) {
        PORT_SetError(SSL_ERROR_MALFORMED_EARLY_DATA);
        return SECFailure;
    }

    if (IS_DTLS(ss)) {
        /* Save the null spec, which we should be currently reading.  We will
         * use this when 0-RTT sending is over. */
        ssl_GetSpecReadLock(ss);
        ss->ssl3.hs.nullSpec = ss->ssl3.crSpec;
        tls13_CipherSpecAddRef(ss->ssl3.hs.nullSpec);
        PORT_Assert(ss->ssl3.hs.nullSpec->cipher_def->cipher == cipher_null);
        ssl_ReleaseSpecReadLock(ss);
    }

    ss->ssl3.hs.zeroRttState = ssl_0rtt_sent;

    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    return SECSuccess;
}

/* This is only registered if we are sending it. */
SECStatus
tls13_ServerSendEarlyDataXtn(sslSocket *ss,
                             PRBool append,
                             PRUint32 maxBytes)
{
    SSL_TRC(3, ("%d: TLS13[%d]: send early_data extension",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->ssl3.hs.zeroRttState == ssl_0rtt_accepted);
    if (maxBytes < 4) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        SECStatus rv;

        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_early_data_xtn, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
    }

    return 4;
}

/* This will only be called if we also offered the extension. */
SECStatus
tls13_ClientHandleEarlyDataXtn(sslSocket *ss, PRUint16 ex_type,
                               SECItem *data)
{
    SSL_TRC(3, ("%d: TLS13[%d]: handle early_data extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION);
        return SECFailure;
    }

    if (data->len) {
        PORT_SetError(SSL_ERROR_MALFORMED_EARLY_DATA);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    ss->ssl3.hs.zeroRttState = ssl_0rtt_accepted;

    return SECSuccess;
}

SECStatus
tls13_ClientHandleTicketEarlyDataInfoXtn(sslSocket *ss, PRUint16 ex_type,
                                         SECItem *data)
{
    PRUint32 utmp;
    SECStatus rv;

    SSL_TRC(3, ("%d: TLS13[%d]: handle early_data_info extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION);
        return SECFailure;
    }

    rv = ssl3_ConsumeHandshake(ss, &utmp, sizeof(utmp),
                               &data->data, &data->len);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET);
        return SECFailure;
    }
    if (data->len) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET);
        return SECFailure;
    }

    ss->xtnData.ticket_age_add_found = PR_TRUE;
    ss->xtnData.ticket_age_add = PR_ntohl(utmp);

    return SECSuccess;
}

/* This is only registered if we are sending it. */
SECStatus
tls13_ServerSendSigAlgsXtn(sslSocket *ss,
                           PRBool append,
                           PRUint32 maxBytes)
{
    SSL_TRC(3, ("%d: TLS13[%d]: send signature_algorithms extension",
                SSL_GETPID(), ss->fd));

    if (maxBytes < 4) {
        PORT_Assert(0);
    }

    if (append) {
        SECStatus rv;

        rv = ssl3_AppendHandshakeNumber(ss, ssl_signature_algorithms_xtn, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
    }

    return 4;
}

/* This will only be called if we also offered the extension. */
SECStatus
tls13_ClientHandleSigAlgsXtn(sslSocket *ss, PRUint16 ex_type,
                             SECItem *data)
{
    SSL_TRC(3, ("%d: TLS13[%d]: handle signature_algorithms extension",
                SSL_GETPID(), ss->fd));

    /* If we are doing < TLS 1.3, then ignore this. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        PORT_SetError(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION);
        return SECFailure;
    }

    if (data->len != 0) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    return SECSuccess;
}

/*
 *     struct {
 *          ProtocolVersion versions<2..254>;
 *     } SupportedVersions;
 */
PRInt32
tls13_ClientSendSupportedVersionsXtn(sslSocket *ss, PRBool append,
                                     PRUint32 maxBytes)
{
    PRInt32 extensions_len;
    PRUint16 version;
    SECStatus rv;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3) {
        return 0;
    }

    SSL_TRC(3, ("%d: TLS13[%d]: send supported_versions extension",
                SSL_GETPID(), ss->fd));

    /* Extension type, extension len fiels, vector len field,
     * length of the values. */
    extensions_len = 2 + 2 + 1 +
                     2 * (ss->vrange.max - ss->vrange.min + 1);

    if (maxBytes < (PRUint32)extensions_len) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_supported_versions_xtn, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeNumber(ss, extensions_len - 4, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeNumber(ss, extensions_len - 5, 1);
        if (rv != SECSuccess)
            return -1;

        for (version = ss->vrange.max; version >= ss->vrange.min; --version) {
            rv = ssl3_AppendHandshakeNumber(
                ss, tls13_EncodeDraftVersion(version), 2);
            if (rv != SECSuccess)
                return -1;
        }
    }

    return extensions_len;
}

/*
 *    struct {
 *        opaque cookie<1..2^16-1>;
 *    } Cookie;
 */
SECStatus
tls13_ClientHandleHrrCookie(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;

    SSL_TRC(3, ("%d: TLS13[%d]: handle cookie extension",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3);

    /* IMPORTANT: this is only valid while the HelloRetryRequest is still valid. */
    rv = ssl3_ConsumeHandshakeVariable(ss, &ss->ssl3.hs.cookie, 2,
                                       &data->data, &data->len);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }
    if (!ss->ssl3.hs.cookie.len || data->len) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
        return SECFailure;
    }

    return SECSuccess;
}

PRInt32
tls13_ClientSendHrrCookieXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_len;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_3 ||
        !ss->ssl3.hs.cookie.len) {
        return 0;
    }

    SSL_TRC(3, ("%d: TLS13[%d]: send cookie extension", SSL_GETPID(), ss->fd));

    /* Extension type, length, cookie length, cookie value. */
    extension_len = 2 + 2 + 2 + ss->ssl3.hs.cookie.len;

    if (maxBytes < (PRUint32)extension_len) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        SECStatus rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_cookie_xtn, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeNumber(ss, extension_len - 4, 2);
        if (rv != SECSuccess)
            return -1;

        rv = ssl3_AppendHandshakeVariable(ss, ss->ssl3.hs.cookie.data,
                                          ss->ssl3.hs.cookie.len, 2);
        if (rv != SECSuccess)
            return -1;
    }
    return extension_len;
}
