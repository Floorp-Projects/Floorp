/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SSL3 Protocol
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* TLS extension code moved here from ssl3ecc.c */

#include "nssrenam.h"
#include "nss.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "ssl3exthandle.h"
#include "tls13exthandle.h"

/* Table of handlers for received TLS hello extensions, one per extension.
 * In the second generation, this table will be dynamic, and functions
 * will be registered here.
 */
/* This table is used by the server, to handle client hello extensions. */
static const ssl3ExtensionHandler clientHelloHandlers[] = {
    { ssl_server_name_xtn, &ssl3_HandleServerNameXtn },
    { ssl_supported_groups_xtn, &ssl_HandleSupportedGroupsXtn },
    { ssl_ec_point_formats_xtn, &ssl3_HandleSupportedPointFormatsXtn },
    { ssl_session_ticket_xtn, &ssl3_ServerHandleSessionTicketXtn },
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { ssl_next_proto_nego_xtn, &ssl3_ServerHandleNextProtoNegoXtn },
    { ssl_app_layer_protocol_xtn, &ssl3_ServerHandleAppProtoXtn },
    { ssl_use_srtp_xtn, &ssl3_ServerHandleUseSRTPXtn },
    { ssl_cert_status_xtn, &ssl3_ServerHandleStatusRequestXtn },
    { ssl_signature_algorithms_xtn, &ssl3_ServerHandleSigAlgsXtn },
    { ssl_extended_master_secret_xtn, &ssl3_HandleExtendedMasterSecretXtn },
    { ssl_signed_cert_timestamp_xtn, &ssl3_ServerHandleSignedCertTimestampXtn },
    { ssl_tls13_key_share_xtn, &tls13_ServerHandleKeyShareXtn },
    { ssl_tls13_pre_shared_key_xtn, &tls13_ServerHandlePreSharedKeyXtn },
    { ssl_tls13_early_data_xtn, &tls13_ServerHandleEarlyDataXtn },
    { -1, NULL }
};

/* These two tables are used by the client, to handle server hello
 * extensions. */
static const ssl3ExtensionHandler serverHelloHandlersTLS[] = {
    { ssl_server_name_xtn, &ssl3_HandleServerNameXtn },
    /* TODO: add a handler for ssl_ec_point_formats_xtn */
    { ssl_session_ticket_xtn, &ssl3_ClientHandleSessionTicketXtn },
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { ssl_next_proto_nego_xtn, &ssl3_ClientHandleNextProtoNegoXtn },
    { ssl_app_layer_protocol_xtn, &ssl3_ClientHandleAppProtoXtn },
    { ssl_use_srtp_xtn, &ssl3_ClientHandleUseSRTPXtn },
    { ssl_cert_status_xtn, &ssl3_ClientHandleStatusRequestXtn },
    { ssl_extended_master_secret_xtn, &ssl3_HandleExtendedMasterSecretXtn },
    { ssl_signed_cert_timestamp_xtn, &ssl3_ClientHandleSignedCertTimestampXtn },
    { ssl_tls13_key_share_xtn, &tls13_ClientHandleKeyShareXtn },
    { ssl_tls13_pre_shared_key_xtn, &tls13_ClientHandlePreSharedKeyXtn },
    { ssl_tls13_early_data_xtn, &tls13_ClientHandleEarlyDataXtn },
    { ssl_signature_algorithms_xtn, &tls13_ClientHandleSigAlgsXtn },
    { -1, NULL }
};

static const ssl3ExtensionHandler helloRetryRequestHandlers[] = {
    { ssl_tls13_key_share_xtn, tls13_ClientHandleKeyShareXtnHrr },
    { ssl_tls13_cookie_xtn, tls13_ClientHandleHrrCookie },
    { -1, NULL }
};

static const ssl3ExtensionHandler serverHelloHandlersSSL3[] = {
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { -1, NULL }
};

static const ssl3ExtensionHandler newSessionTicketHandlers[] = {
    { ssl_tls13_ticket_early_data_info_xtn,
      &tls13_ClientHandleTicketEarlyDataInfoXtn },
    { -1, NULL }
};

/* Tables of functions to format TLS hello extensions, one function per
 * extension.
 * These static tables are for the formatting of client hello extensions.
 * The server's table of hello senders is dynamic, in the socket struct,
 * and sender functions are registered there.
 * NB: the order of these extensions can have an impact on compatibility. Some
 * servers (e.g. Tomcat) will terminate the connection if the last extension in
 * the client hello is empty (for example, the extended master secret
 * extension, if it were listed last). See bug 1243641.
 */
static const ssl3HelloExtensionSender clientHelloSendersTLS[SSL_MAX_EXTENSIONS] =
    {
      { ssl_server_name_xtn, &ssl3_SendServerNameXtn },
      { ssl_extended_master_secret_xtn, &ssl3_SendExtendedMasterSecretXtn },
      { ssl_renegotiation_info_xtn, &ssl3_SendRenegotiationInfoXtn },
      { ssl_supported_groups_xtn, &ssl_SendSupportedGroupsXtn },
      { ssl_ec_point_formats_xtn, &ssl3_SendSupportedPointFormatsXtn },
      { ssl_session_ticket_xtn, &ssl3_SendSessionTicketXtn },
      { ssl_next_proto_nego_xtn, &ssl3_ClientSendNextProtoNegoXtn },
      { ssl_app_layer_protocol_xtn, &ssl3_ClientSendAppProtoXtn },
      { ssl_use_srtp_xtn, &ssl3_ClientSendUseSRTPXtn },
      { ssl_cert_status_xtn, &ssl3_ClientSendStatusRequestXtn },
      { ssl_signed_cert_timestamp_xtn, &ssl3_ClientSendSignedCertTimestampXtn },
      { ssl_tls13_key_share_xtn, &tls13_ClientSendKeyShareXtn },
      { ssl_tls13_pre_shared_key_xtn, &tls13_ClientSendPreSharedKeyXtn },
      { ssl_tls13_early_data_xtn, &tls13_ClientSendEarlyDataXtn },
      /* Some servers (e.g. WebSphere Application Server 7.0 and Tomcat) will
       * time out or terminate the connection if the last extension in the
       * client hello is empty. They are not intolerant of TLS 1.2, so list
       * signature_algorithms at the end. See bug 1243641. */
      { ssl_tls13_supported_versions_xtn, &tls13_ClientSendSupportedVersionsXtn },
      { ssl_signature_algorithms_xtn, &ssl3_ClientSendSigAlgsXtn },
      { ssl_tls13_cookie_xtn, &tls13_ClientSendHrrCookieXtn }
      /* any extra entries will appear as { 0, NULL }    */
    };

static const ssl3HelloExtensionSender clientHelloSendersSSL3[SSL_MAX_EXTENSIONS] = {
    { ssl_renegotiation_info_xtn, &ssl3_SendRenegotiationInfoXtn }
    /* any extra entries will appear as { 0, NULL }    */
};

static PRBool
arrayContainsExtension(const PRUint16 *array, PRUint32 len, PRUint16 ex_type)
{
    unsigned int i;
    for (i = 0; i < len; i++) {
        if (ex_type == array[i])
            return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool
ssl3_ExtensionNegotiated(sslSocket *ss, PRUint16 ex_type)
{
    TLSExtensionData *xtnData = &ss->xtnData;
    return arrayContainsExtension(xtnData->negotiated,
                                  xtnData->numNegotiated, ex_type);
}

PRBool
ssl3_ClientExtensionAdvertised(sslSocket *ss, PRUint16 ex_type)
{
    TLSExtensionData *xtnData = &ss->xtnData;
    return arrayContainsExtension(xtnData->advertised,
                                  xtnData->numAdvertised, ex_type);
}

/* Go through hello extensions in |b| and deserialize
 * them into the list in |ss->ssl3.hs.remoteExtensions|.
 * The only checking we do in this point is for duplicates.
 *
 * IMPORTANT: This list just contains pointers to the incoming
 * buffer so they can only be used during ClientHello processing.
 */
SECStatus
ssl3_ParseExtensions(sslSocket *ss, SSL3Opaque **b, PRUint32 *length)
{
    /* Clean out the extensions list. */
    ssl3_DestroyRemoteExtensions(&ss->ssl3.hs.remoteExtensions);

    while (*length) {
        SECStatus rv;
        PRInt32 extension_type;
        SECItem extension_data = { siBuffer, NULL, 0 };
        TLSExtension *extension;
        PRCList *cursor;

        /* Get the extension's type field */
        extension_type = ssl3_ConsumeHandshakeNumber(ss, 2, b, length);
        if (extension_type < 0) { /* failure to decode extension_type */
            return SECFailure;    /* alert already sent */
        }

        /* Check whether an extension has been sent multiple times. */
        for (cursor = PR_NEXT_LINK(&ss->ssl3.hs.remoteExtensions);
             cursor != &ss->ssl3.hs.remoteExtensions;
             cursor = PR_NEXT_LINK(cursor)) {
            if (((TLSExtension *)cursor)->type == extension_type) {
                (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
                return SECFailure;
            }
        }

        /* Get the data for this extension, so we can pass it or skip it. */
        rv = ssl3_ConsumeHandshakeVariable(ss, &extension_data, 2, b, length);
        if (rv != SECSuccess) {
            return rv; /* alert already sent */
        }

        extension = PORT_ZNew(TLSExtension);
        if (!extension) {
            return SECFailure;
        }

        extension->type = (PRUint16)extension_type;
        extension->data = extension_data;
        PR_APPEND_LINK(&extension->link, &ss->ssl3.hs.remoteExtensions);
    }

    return SECSuccess;
}

TLSExtension *
ssl3_FindExtension(sslSocket *ss, SSLExtensionType extension_type)
{
    PRCList *cursor;

    for (cursor = PR_NEXT_LINK(&ss->ssl3.hs.remoteExtensions);
         cursor != &ss->ssl3.hs.remoteExtensions;
         cursor = PR_NEXT_LINK(cursor)) {
        TLSExtension *extension = (TLSExtension *)cursor;

        if (extension->type == extension_type) {
            return extension;
        }
    }

    return NULL;
}

/* Go through the hello extensions in |ss->ssl3.hs.remoteExtensions|.
 * For each one, find the extension handler in the table, and
 * if present, invoke that handler.
 * Servers ignore any extensions with unknown extension types.
 * Clients reject any extensions with unadvertised extension types
 *
 * In TLS >= 1.3, the client checks that extensions appear in the
 * right phase.
 */
SECStatus
ssl3_HandleParsedExtensions(sslSocket *ss,
                            SSL3HandshakeType handshakeMessage)
{
    const ssl3ExtensionHandler *handlers;
    PRBool isTLS13 = ss->version >= SSL_LIBRARY_VERSION_TLS_1_3;
    PRCList *cursor;

    switch (handshakeMessage) {
        case client_hello:
            handlers = clientHelloHandlers;
            break;
        case new_session_ticket:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
            handlers = newSessionTicketHandlers;
            break;
        case hello_retry_request:
            handlers = helloRetryRequestHandlers;
            break;
        case encrypted_extensions:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
        /* fall through */
        case server_hello:
            if (ss->version > SSL_LIBRARY_VERSION_3_0) {
                handlers = serverHelloHandlersTLS;
            } else {
                handlers = serverHelloHandlersSSL3;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            PORT_Assert(0);
            return SECFailure;
    }

    for (cursor = PR_NEXT_LINK(&ss->ssl3.hs.remoteExtensions);
         cursor != &ss->ssl3.hs.remoteExtensions;
         cursor = PR_NEXT_LINK(cursor)) {
        TLSExtension *extension = (TLSExtension *)cursor;
        const ssl3ExtensionHandler *handler;

        /* Check whether the server sent an extension which was not advertised
         * in the ClientHello */
        if (!ss->sec.isServer &&
            !ssl3_ClientExtensionAdvertised(ss, extension->type) &&
            (handshakeMessage != new_session_ticket) &&
            (extension->type != ssl_tls13_cookie_xtn)) {
            (void)SSL3_SendAlert(ss, alert_fatal, unsupported_extension);
            PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
            return SECFailure;
        }

        /* Check that this is a legal extension in TLS 1.3 */
        if (isTLS13 && !tls13_ExtensionAllowed(extension->type, handshakeMessage)) {
            if (handshakeMessage == client_hello) {
                /* Skip extensions not used in TLS 1.3 */
                continue;
            }
            tls13_FatalError(ss, SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION,
                             unsupported_extension);
            return SECFailure;
        }

        /* find extension_type in table of Hello Extension Handlers */
        for (handler = handlers; handler->ex_type >= 0; handler++) {
            /* if found, call this handler */
            if (handler->ex_type == extension->type) {
                SECStatus rv;

                rv = (*handler->ex_handler)(ss, (PRUint16)extension->type,
                                            &extension->data);
                if (rv != SECSuccess) {
                    if (!ss->ssl3.fatalAlertSent) {
                        /* send a generic alert if the handler didn't already */
                        (void)SSL3_SendAlert(ss, alert_fatal, handshake_failure);
                    }
                    return SECFailure;
                }
            }
        }
    }
    return SECSuccess;
}

/* Syntactic sugar around ssl3_ParseExtensions and
 * ssl3_HandleParsedExtensions. */
SECStatus
ssl3_HandleExtensions(sslSocket *ss,
                      SSL3Opaque **b, PRUint32 *length,
                      SSL3HandshakeType handshakeMessage)
{
    SECStatus rv;

    rv = ssl3_ParseExtensions(ss, b, length);
    if (rv != SECSuccess)
        return rv;

    rv = ssl3_HandleParsedExtensions(ss, handshakeMessage);
    if (rv != SECSuccess)
        return rv;

    ssl3_DestroyRemoteExtensions(&ss->ssl3.hs.remoteExtensions);
    return SECSuccess;
}

/* Add a callback function to the table of senders of server hello extensions.
 */
SECStatus
ssl3_RegisterServerHelloExtensionSender(sslSocket *ss, PRUint16 ex_type,
                                        ssl3HelloExtensionSenderFunc cb)
{
    int i;
    ssl3HelloExtensionSender *sender;
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        sender = &ss->xtnData.serverHelloSenders[0];
    } else {
        if (tls13_ExtensionAllowed(ex_type, server_hello)) {
            PORT_Assert(!tls13_ExtensionAllowed(ex_type, encrypted_extensions));
            sender = &ss->xtnData.serverHelloSenders[0];
        } else {
            PORT_Assert(tls13_ExtensionAllowed(ex_type, encrypted_extensions));
            sender = &ss->xtnData.encryptedExtensionsSenders[0];
        }
    }

    for (i = 0; i < SSL_MAX_EXTENSIONS; ++i, ++sender) {
        if (!sender->ex_sender) {
            sender->ex_type = ex_type;
            sender->ex_sender = cb;
            return SECSuccess;
        }
        /* detect duplicate senders */
        PORT_Assert(sender->ex_type != ex_type);
        if (sender->ex_type == ex_type) {
            /* duplicate */
            break;
        }
    }
    PORT_Assert(i < SSL_MAX_EXTENSIONS); /* table needs to grow */
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}

/* call each of the extension senders and return the accumulated length */
PRInt32
ssl3_CallHelloExtensionSenders(sslSocket *ss, PRBool append, PRUint32 maxBytes,
                               const ssl3HelloExtensionSender *sender)
{
    PRInt32 total_exten_len = 0;
    int i;

    if (!sender) {
        if (ss->vrange.max > SSL_LIBRARY_VERSION_3_0) {
            sender = &clientHelloSendersTLS[0];
        } else {
            sender = &clientHelloSendersSSL3[0];
        }
    }

    for (i = 0; i < SSL_MAX_EXTENSIONS; ++i, ++sender) {
        if (sender->ex_sender) {
            PRInt32 extLen = (*sender->ex_sender)(ss, append, maxBytes);
            if (extLen < 0)
                return -1;
            maxBytes -= extLen;
            total_exten_len += extLen;
        }
    }
    return total_exten_len;
}

void
ssl3_DestroyRemoteExtensions(PRCList *list)
{
    PRCList *cur_p;

    while (!PR_CLIST_IS_EMPTY(list)) {
        cur_p = PR_LIST_TAIL(list);
        PR_REMOVE_LINK(cur_p);
        PORT_Free(cur_p);
    }
}
