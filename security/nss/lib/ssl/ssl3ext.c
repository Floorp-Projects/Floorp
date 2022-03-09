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
#include "pk11pub.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "ssl3exthandle.h"
#include "tls13ech.h"
#include "tls13err.h"
#include "tls13exthandle.h"
#include "tls13subcerts.h"

/* Callback function that handles a received extension. */
typedef SECStatus (*ssl3ExtensionHandlerFunc)(const sslSocket *ss,
                                              TLSExtensionData *xtnData,
                                              SECItem *data);

/* Row in a table of hello extension handlers. */
typedef struct {
    SSLExtensionType ex_type;
    ssl3ExtensionHandlerFunc ex_handler;
} ssl3ExtensionHandler;

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
    { ssl_app_layer_protocol_xtn, &ssl3_ServerHandleAppProtoXtn },
    { ssl_use_srtp_xtn, &ssl3_ServerHandleUseSRTPXtn },
    { ssl_cert_status_xtn, &ssl3_ServerHandleStatusRequestXtn },
    { ssl_signature_algorithms_xtn, &ssl3_HandleSigAlgsXtn },
    { ssl_extended_master_secret_xtn, &ssl3_HandleExtendedMasterSecretXtn },
    { ssl_signed_cert_timestamp_xtn, &ssl3_ServerHandleSignedCertTimestampXtn },
    { ssl_delegated_credentials_xtn, &tls13_ServerHandleDelegatedCredentialsXtn },
    { ssl_tls13_key_share_xtn, &tls13_ServerHandleKeyShareXtn },
    { ssl_tls13_pre_shared_key_xtn, &tls13_ServerHandlePreSharedKeyXtn },
    { ssl_tls13_early_data_xtn, &tls13_ServerHandleEarlyDataXtn },
    { ssl_tls13_psk_key_exchange_modes_xtn, &tls13_ServerHandlePskModesXtn },
    { ssl_tls13_cookie_xtn, &tls13_ServerHandleCookieXtn },
    { ssl_tls13_post_handshake_auth_xtn, &tls13_ServerHandlePostHandshakeAuthXtn },
    { ssl_record_size_limit_xtn, &ssl_HandleRecordSizeLimitXtn },
    { 0, NULL }
};

/* These two tables are used by the client, to handle server hello
 * extensions. */
static const ssl3ExtensionHandler serverHelloHandlersTLS[] = {
    { ssl_server_name_xtn, &ssl3_HandleServerNameXtn },
    /* TODO: add a handler for ssl_ec_point_formats_xtn */
    { ssl_session_ticket_xtn, &ssl3_ClientHandleSessionTicketXtn },
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { ssl_app_layer_protocol_xtn, &ssl3_ClientHandleAppProtoXtn },
    { ssl_use_srtp_xtn, &ssl3_ClientHandleUseSRTPXtn },
    { ssl_cert_status_xtn, &ssl3_ClientHandleStatusRequestXtn },
    { ssl_extended_master_secret_xtn, &ssl3_HandleExtendedMasterSecretXtn },
    { ssl_signed_cert_timestamp_xtn, &ssl3_ClientHandleSignedCertTimestampXtn },
    { ssl_tls13_key_share_xtn, &tls13_ClientHandleKeyShareXtn },
    { ssl_tls13_pre_shared_key_xtn, &tls13_ClientHandlePreSharedKeyXtn },
    { ssl_tls13_early_data_xtn, &tls13_ClientHandleEarlyDataXtn },
    { ssl_tls13_encrypted_client_hello_xtn, &tls13_ClientHandleEchXtn },
    { ssl_record_size_limit_xtn, &ssl_HandleRecordSizeLimitXtn },
    { 0, NULL }
};

static const ssl3ExtensionHandler helloRetryRequestHandlers[] = {
    { ssl_tls13_key_share_xtn, tls13_ClientHandleKeyShareXtnHrr },
    { ssl_tls13_cookie_xtn, tls13_ClientHandleHrrCookie },
    { ssl_tls13_encrypted_client_hello_xtn, tls13_ClientHandleHrrEchXtn },
    { 0, NULL }
};

static const ssl3ExtensionHandler serverHelloHandlersSSL3[] = {
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { 0, NULL }
};

static const ssl3ExtensionHandler newSessionTicketHandlers[] = {
    { ssl_tls13_early_data_xtn,
      &tls13_ClientHandleTicketEarlyDataXtn },
    { 0, NULL }
};

/* This table is used by the client to handle server certificates in TLS 1.3 */
static const ssl3ExtensionHandler serverCertificateHandlers[] = {
    { ssl_signed_cert_timestamp_xtn, &ssl3_ClientHandleSignedCertTimestampXtn },
    { ssl_cert_status_xtn, &ssl3_ClientHandleStatusRequestXtn },
    { ssl_delegated_credentials_xtn, &tls13_ClientHandleDelegatedCredentialsXtn },
    { 0, NULL }
};

static const ssl3ExtensionHandler certificateRequestHandlers[] = {
    { ssl_signature_algorithms_xtn, &ssl3_HandleSigAlgsXtn },
    { ssl_tls13_certificate_authorities_xtn,
      &tls13_ClientHandleCertAuthoritiesXtn },
    { 0, NULL }
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
static const sslExtensionBuilder clientHelloSendersTLS[] =
    {
      { ssl_server_name_xtn, &ssl3_ClientSendServerNameXtn },
      { ssl_extended_master_secret_xtn, &ssl3_SendExtendedMasterSecretXtn },
      { ssl_renegotiation_info_xtn, &ssl3_SendRenegotiationInfoXtn },
      { ssl_supported_groups_xtn, &ssl_SendSupportedGroupsXtn },
      { ssl_ec_point_formats_xtn, &ssl3_SendSupportedPointFormatsXtn },
      { ssl_session_ticket_xtn, &ssl3_ClientSendSessionTicketXtn },
      { ssl_app_layer_protocol_xtn, &ssl3_ClientSendAppProtoXtn },
      { ssl_use_srtp_xtn, &ssl3_ClientSendUseSRTPXtn },
      { ssl_cert_status_xtn, &ssl3_ClientSendStatusRequestXtn },
      { ssl_delegated_credentials_xtn, &tls13_ClientSendDelegatedCredentialsXtn },
      { ssl_signed_cert_timestamp_xtn, &ssl3_ClientSendSignedCertTimestampXtn },
      { ssl_tls13_key_share_xtn, &tls13_ClientSendKeyShareXtn },
      { ssl_tls13_early_data_xtn, &tls13_ClientSendEarlyDataXtn },
      /* Some servers (e.g. WebSphere Application Server 7.0 and Tomcat) will
       * time out or terminate the connection if the last extension in the
       * client hello is empty. They are not intolerant of TLS 1.2, so list
       * signature_algorithms at the end. See bug 1243641. */
      { ssl_tls13_supported_versions_xtn, &tls13_ClientSendSupportedVersionsXtn },
      { ssl_signature_algorithms_xtn, &ssl3_SendSigAlgsXtn },
      { ssl_tls13_cookie_xtn, &tls13_ClientSendHrrCookieXtn },
      { ssl_tls13_psk_key_exchange_modes_xtn, &tls13_ClientSendPskModesXtn },
      { ssl_tls13_post_handshake_auth_xtn, &tls13_ClientSendPostHandshakeAuthXtn },
      { ssl_record_size_limit_xtn, &ssl_SendRecordSizeLimitXtn },
      /* The pre_shared_key extension MUST be last. */
      { ssl_tls13_pre_shared_key_xtn, &tls13_ClientSendPreSharedKeyXtn },
      { 0, NULL }
    };

static const sslExtensionBuilder clientHelloSendersSSL3[] = {
    { ssl_renegotiation_info_xtn, &ssl3_SendRenegotiationInfoXtn },
    { 0, NULL }
};

static const sslExtensionBuilder tls13_cert_req_senders[] = {
    { ssl_signature_algorithms_xtn, &ssl3_SendSigAlgsXtn },
    { ssl_tls13_certificate_authorities_xtn, &tls13_SendCertAuthoritiesXtn },
    { 0, NULL }
};

static const sslExtensionBuilder tls13_hrr_senders[] = {
    { ssl_tls13_key_share_xtn, &tls13_ServerSendHrrKeyShareXtn },
    { ssl_tls13_cookie_xtn, &tls13_ServerSendHrrCookieXtn },
    { ssl_tls13_supported_versions_xtn, &tls13_ServerSendSupportedVersionsXtn },
    { ssl_tls13_encrypted_client_hello_xtn, &tls13_ServerSendHrrEchXtn },
    { 0, NULL }
};

static const struct {
    SSLExtensionType type;
    SSLExtensionSupport support;
} ssl_supported_extensions[] = {
    { ssl_server_name_xtn, ssl_ext_native_only },
    { ssl_cert_status_xtn, ssl_ext_native },
    { ssl_delegated_credentials_xtn, ssl_ext_native },
    { ssl_supported_groups_xtn, ssl_ext_native_only },
    { ssl_ec_point_formats_xtn, ssl_ext_native },
    { ssl_signature_algorithms_xtn, ssl_ext_native_only },
    { ssl_use_srtp_xtn, ssl_ext_native },
    { ssl_app_layer_protocol_xtn, ssl_ext_native_only },
    { ssl_signed_cert_timestamp_xtn, ssl_ext_native },
    { ssl_padding_xtn, ssl_ext_native },
    { ssl_extended_master_secret_xtn, ssl_ext_native_only },
    { ssl_session_ticket_xtn, ssl_ext_native_only },
    { ssl_tls13_key_share_xtn, ssl_ext_native_only },
    { ssl_tls13_pre_shared_key_xtn, ssl_ext_native_only },
    { ssl_tls13_early_data_xtn, ssl_ext_native_only },
    { ssl_tls13_supported_versions_xtn, ssl_ext_native_only },
    { ssl_tls13_cookie_xtn, ssl_ext_native_only },
    { ssl_tls13_psk_key_exchange_modes_xtn, ssl_ext_native_only },
    { ssl_tls13_ticket_early_data_info_xtn, ssl_ext_native_only },
    { ssl_tls13_certificate_authorities_xtn, ssl_ext_native },
    { ssl_renegotiation_info_xtn, ssl_ext_native },
    { ssl_tls13_encrypted_client_hello_xtn, ssl_ext_native_only },
};

static SSLExtensionSupport
ssl_GetExtensionSupport(PRUint16 type)
{
    unsigned int i;
    for (i = 0; i < PR_ARRAY_SIZE(ssl_supported_extensions); ++i) {
        if (type == ssl_supported_extensions[i].type) {
            return ssl_supported_extensions[i].support;
        }
    }
    return ssl_ext_none;
}

SECStatus
SSLExp_GetExtensionSupport(PRUint16 type, SSLExtensionSupport *support)
{
    *support = ssl_GetExtensionSupport(type);
    return SECSuccess;
}

SECStatus
SSLExp_InstallExtensionHooks(PRFileDesc *fd, PRUint16 extension,
                             SSLExtensionWriter writer, void *writerArg,
                             SSLExtensionHandler handler, void *handlerArg)
{
    sslSocket *ss = ssl_FindSocket(fd);
    PRCList *cursor;
    sslCustomExtensionHooks *hook;

    if (!ss) {
        return SECFailure; /* Code already set. */
    }

    /* Need to specify both or neither, but not just one. */
    if ((writer && !handler) || (!writer && handler)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (ssl_GetExtensionSupport(extension) == ssl_ext_native_only) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (ss->firstHsDone || ((ss->ssl3.hs.ws != idle_handshake) &&
                            (ss->ssl3.hs.ws != wait_client_hello))) {
        PORT_SetError(PR_INVALID_STATE_ERROR);
        return SECFailure;
    }

    /* Remove any old handler. */
    for (cursor = PR_NEXT_LINK(&ss->extensionHooks);
         cursor != &ss->extensionHooks;
         cursor = PR_NEXT_LINK(cursor)) {
        hook = (sslCustomExtensionHooks *)cursor;
        if (hook->type == extension) {
            PR_REMOVE_LINK(&hook->link);
            PORT_Free(hook);
            break;
        }
    }

    if (!writer && !handler) {
        return SECSuccess;
    }

    hook = PORT_ZNew(sslCustomExtensionHooks);
    if (!hook) {
        return SECFailure; /* This removed the old one, oh well. */
    }

    hook->type = extension;
    hook->writer = writer;
    hook->writerArg = writerArg;
    hook->handler = handler;
    hook->handlerArg = handlerArg;
    PR_APPEND_LINK(&hook->link, &ss->extensionHooks);
    return SECSuccess;
}

sslCustomExtensionHooks *
ssl_FindCustomExtensionHooks(sslSocket *ss, PRUint16 extension)
{
    PRCList *cursor;

    for (cursor = PR_NEXT_LINK(&ss->extensionHooks);
         cursor != &ss->extensionHooks;
         cursor = PR_NEXT_LINK(cursor)) {
        sslCustomExtensionHooks *hook = (sslCustomExtensionHooks *)cursor;
        if (hook->type == extension) {
            return hook;
        }
    }

    return NULL;
}

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
ssl3_ExtensionNegotiated(const sslSocket *ss, PRUint16 ex_type)
{
    const TLSExtensionData *xtnData = &ss->xtnData;
    return arrayContainsExtension(xtnData->negotiated,
                                  xtnData->numNegotiated, ex_type);
}

/* This checks for whether an extension was advertised.  On the client, this
 * covers extensions that are sent in ClientHello; on the server, extensions
 * sent in CertificateRequest (TLS 1.3 only). */
PRBool
ssl3_ExtensionAdvertised(const sslSocket *ss, PRUint16 ex_type)
{
    const TLSExtensionData *xtnData = &ss->xtnData;
    return arrayContainsExtension(xtnData->advertised,
                                  xtnData->numAdvertised, ex_type);
}

PRBool
ssl3_ExtensionAdvertisedClientHelloInner(const sslSocket *ss, PRUint16 ex_type)
{
    const TLSExtensionData *xtnData = &ss->xtnData;
    return arrayContainsExtension(xtnData->echAdvertised,
                                  xtnData->echNumAdvertised, ex_type);
}

/* Go through hello extensions in |b| and deserialize
 * them into the list in |ss->ssl3.hs.remoteExtensions|.
 * The only checking we do in this point is for duplicates.
 *
 * IMPORTANT: This list just contains pointers to the incoming
 * buffer so they can only be used during ClientHello processing.
 */
SECStatus
ssl3_ParseExtensions(sslSocket *ss, PRUint8 **b, PRUint32 *length)
{
    /* Clean out the extensions list. */
    ssl3_DestroyRemoteExtensions(&ss->ssl3.hs.remoteExtensions);

    while (*length) {
        SECStatus rv;
        PRUint32 extension_type;
        SECItem extension_data = { siBuffer, NULL, 0 };
        TLSExtension *extension;
        PRCList *cursor;

        /* Get the extension's type field */
        rv = ssl3_ConsumeHandshakeNumber(ss, &extension_type, 2, b, length);
        if (rv != SECSuccess) {
            return SECFailure; /* alert already sent */
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

        SSL_TRC(10, ("%d: SSL3[%d]: parsed extension %d len=%u",
                     SSL_GETPID(), ss->fd, extension_type, extension_data.len));

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

static SECStatus
ssl_CallExtensionHandler(sslSocket *ss, SSLHandshakeType handshakeMessage,
                         TLSExtension *extension,
                         const ssl3ExtensionHandler *handler)
{
    SECStatus rv = SECSuccess;
    SSLAlertDescription alert = handshake_failure;
    sslCustomExtensionHooks *customHooks;

    customHooks = ssl_FindCustomExtensionHooks(ss, extension->type);
    if (customHooks) {
        if (customHooks->handler) {
            rv = customHooks->handler(ss->fd, handshakeMessage,
                                      extension->data.data,
                                      extension->data.len,
                                      &alert, customHooks->handlerArg);
        }
    } else {
        /* Find extension_type in table of Hello Extension Handlers. */
        for (; handler->ex_handler != NULL; ++handler) {
            if (handler->ex_type == extension->type) {
                SECItem tmp = extension->data;

                rv = (*handler->ex_handler)(ss, &ss->xtnData, &tmp);
                break;
            }
        }
    }

    if (rv != SECSuccess) {
        if (!ss->ssl3.fatalAlertSent) {
            /* Send an alert if the handler didn't already. */
            (void)SSL3_SendAlert(ss, alert_fatal, alert);
        }
        return SECFailure;
    }

    return SECSuccess;
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
ssl3_HandleParsedExtensions(sslSocket *ss, SSLHandshakeType message)
{
    const ssl3ExtensionHandler *handlers;
    /* HelloRetryRequest doesn't set ss->version. It might be safe to
     * do so, but we weren't entirely sure. TODO(ekr@rtfm.com). */
    PRBool isTLS13 = (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) ||
                     (message == ssl_hs_hello_retry_request);
    /* The following messages can include extensions that were not included in
     * the original ClientHello. */
    PRBool allowNotOffered = (message == ssl_hs_client_hello) ||
                             (message == ssl_hs_certificate_request) ||
                             (message == ssl_hs_new_session_ticket);
    PRCList *cursor;

    switch (message) {
        case ssl_hs_client_hello:
            handlers = clientHelloHandlers;
            break;
        case ssl_hs_new_session_ticket:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
            handlers = newSessionTicketHandlers;
            break;
        case ssl_hs_hello_retry_request:
            handlers = helloRetryRequestHandlers;
            break;
        case ssl_hs_encrypted_extensions:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
        /* fall through */
        case ssl_hs_server_hello:
            if (ss->version > SSL_LIBRARY_VERSION_3_0) {
                handlers = serverHelloHandlersTLS;
            } else {
                handlers = serverHelloHandlersSSL3;
            }
            break;
        case ssl_hs_certificate:
            PORT_Assert(!ss->sec.isServer);
            handlers = serverCertificateHandlers;
            break;
        case ssl_hs_certificate_request:
            PORT_Assert(!ss->sec.isServer);
            handlers = certificateRequestHandlers;
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
        SECStatus rv;

        /* Check whether the server sent an extension which was not advertised
         * in the ClientHello.
         *
         * Note that a TLS 1.3 server should check if CertificateRequest
         * extensions were sent.  But the extensions used for CertificateRequest
         * do not have any response, so we rely on
         * ssl3_ExtensionAdvertised to return false on the server.  That
         * results in the server only rejecting any extension. */
        if (!allowNotOffered && (extension->type != ssl_tls13_cookie_xtn)) {
            if (!ssl3_ExtensionAdvertised(ss, extension->type)) {
                SSL_TRC(10, ("Server sent xtn type=%d which is invalid for the CHO", extension->type));
                (void)SSL3_SendAlert(ss, alert_fatal, unsupported_extension);
                PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
                return SECFailure;
            }
            /* If we offered ECH, we also check whether the extension is compatible with
            * the Client Hello Inner. We don't yet know whether the server accepted ECH,
            * so we only store this for now. If we later accept, we check this boolean
            * and reject with an unsupported_extension alert if it is set. */
            if (ss->ssl3.hs.echHpkeCtx && !ssl3_ExtensionAdvertisedClientHelloInner(ss, extension->type)) {
                SSL_TRC(10, ("Server sent xtn type=%d which is invalid for the CHI", extension->type));
                ss->ssl3.hs.echInvalidExtension = PR_TRUE;
            }
        }

        /* Check that this is a legal extension in TLS 1.3 */
        if (isTLS13 &&
            !ssl_FindCustomExtensionHooks(ss, extension->type)) {
            switch (tls13_ExtensionStatus(extension->type, message)) {
                case tls13_extension_allowed:
                    break;
                case tls13_extension_unknown:
                    if (allowNotOffered) {
                        continue; /* Skip over unknown extensions. */
                    }
                    /* RFC8446 Section 4.2 - Implementations MUST NOT send extension responses if
                     * the remote endpoint did not send the corresponding extension request ...
                     * Upon receiving such an extension, an endpoint MUST abort the handshake with
                     * an "unsupported_extension" alert. */
                    SSL_TRC(3, ("%d: TLS13: unknown extension %d in message %d",
                                SSL_GETPID(), extension, message));
                    tls13_FatalError(ss, SSL_ERROR_RX_UNEXPECTED_EXTENSION,
                                     unsupported_extension);
                    return SECFailure;
                case tls13_extension_disallowed:
                    /* RFC8446 Section 4.2 - If an implementation receives an extension which it
                     * recognizes and which is not specified for the message in which it appears,
                     * it MUST abort the handshake with an "illegal_parameter" alert. */
                    SSL_TRC(3, ("%d: TLS13: disallowed extension %d in message %d",
                                SSL_GETPID(), extension, message));
                    tls13_FatalError(ss, SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION,
                                     illegal_parameter);
                    return SECFailure;
            }
        }

        /* Special check for this being the last extension if it's
         * PreSharedKey */
        if (ss->sec.isServer && isTLS13 &&
            (extension->type == ssl_tls13_pre_shared_key_xtn) &&
            (PR_NEXT_LINK(cursor) != &ss->ssl3.hs.remoteExtensions)) {
            tls13_FatalError(ss,
                             SSL_ERROR_RX_MALFORMED_CLIENT_HELLO,
                             illegal_parameter);
            return SECFailure;
        }

        rv = ssl_CallExtensionHandler(ss, message, extension, handlers);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    return SECSuccess;
}

/* Syntactic sugar around ssl3_ParseExtensions and
 * ssl3_HandleParsedExtensions. */
SECStatus
ssl3_HandleExtensions(sslSocket *ss,
                      PRUint8 **b, PRUint32 *length,
                      SSLHandshakeType handshakeMessage)
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
ssl3_RegisterExtensionSender(const sslSocket *ss,
                             TLSExtensionData *xtnData,
                             PRUint16 ex_type,
                             sslExtensionBuilderFunc cb)
{
    int i;
    sslExtensionBuilder *sender;
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
        sender = &xtnData->serverHelloSenders[0];
    } else {
        if (tls13_ExtensionStatus(ex_type, ssl_hs_server_hello) ==
            tls13_extension_allowed) {
            PORT_Assert(tls13_ExtensionStatus(ex_type,
                                              ssl_hs_encrypted_extensions) ==
                        tls13_extension_disallowed);
            sender = &xtnData->serverHelloSenders[0];
        } else if (tls13_ExtensionStatus(ex_type,
                                         ssl_hs_encrypted_extensions) ==
                   tls13_extension_allowed) {
            sender = &xtnData->encryptedExtensionsSenders[0];
        } else if (tls13_ExtensionStatus(ex_type, ssl_hs_certificate) ==
                   tls13_extension_allowed) {
            sender = &xtnData->certificateSenders[0];
        } else {
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
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

SECStatus
ssl_CallCustomExtensionSenders(sslSocket *ss, sslBuffer *buf,
                               SSLHandshakeType message)
{
    sslBuffer tail = SSL_BUFFER_EMPTY;
    SECStatus rv;
    PRCList *cursor;

    /* Save any extensions that want to be last. */
    if (ss->xtnData.lastXtnOffset) {
        rv = sslBuffer_Append(&tail, buf->buf + ss->xtnData.lastXtnOffset,
                              buf->len - ss->xtnData.lastXtnOffset);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        buf->len = ss->xtnData.lastXtnOffset;
    }

    /* Reserve the maximum amount of space possible. */
    rv = sslBuffer_Grow(buf, 65535);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    for (cursor = PR_NEXT_LINK(&ss->extensionHooks);
         cursor != &ss->extensionHooks;
         cursor = PR_NEXT_LINK(cursor)) {
        sslCustomExtensionHooks *hook =
            (sslCustomExtensionHooks *)cursor;
        PRBool append = PR_FALSE;
        unsigned int len = 0;

        if (hook->writer) {
            /* The writer writes directly into |buf|.  Provide space that allows
             * for the existing extensions, any tail, plus type and length. */
            unsigned int space = buf->space - (buf->len + tail.len + 4);
            append = (*hook->writer)(ss->fd, message,
                                     buf->buf + buf->len + 4, &len, space,
                                     hook->writerArg);
            if (len > space) {
                PORT_SetError(SEC_ERROR_APPLICATION_CALLBACK_ERROR);
                goto loser;
            }
        }
        if (!append) {
            continue;
        }

        rv = sslBuffer_AppendNumber(buf, hook->type, 2);
        if (rv != SECSuccess) {
            goto loser; /* Code already set. */
        }
        rv = sslBuffer_AppendNumber(buf, len, 2);
        if (rv != SECSuccess) {
            goto loser; /* Code already set. */
        }
        buf->len += len;

        if (message == ssl_hs_client_hello ||
            message == ssl_hs_ech_outer_client_hello ||
            message == ssl_hs_certificate_request) {
            ss->xtnData.advertised[ss->xtnData.numAdvertised++] = hook->type;
        }
    }

    rv = sslBuffer_Append(buf, tail.buf, tail.len);
    if (rv != SECSuccess) {
        goto loser; /* Code already set. */
    }

    sslBuffer_Clear(&tail);
    return SECSuccess;

loser:
    sslBuffer_Clear(&tail);
    return SECFailure;
}

/* Call extension handlers for the given message. */
SECStatus
ssl_ConstructExtensions(sslSocket *ss, sslBuffer *buf, SSLHandshakeType message)
{
    const sslExtensionBuilder *sender;
    SECStatus rv;

    PORT_Assert(buf->len == 0);

    /* Clear out any extensions previously advertised */
    ss->xtnData.numAdvertised = 0;
    ss->xtnData.echNumAdvertised = 0;

    switch (message) {
        case ssl_hs_client_hello:
            if (ss->vrange.max > SSL_LIBRARY_VERSION_3_0) {
                sender = clientHelloSendersTLS;
            } else {
                sender = clientHelloSendersSSL3;
            }
            break;

        case ssl_hs_server_hello:
            sender = ss->xtnData.serverHelloSenders;
            break;

        case ssl_hs_certificate_request:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
            sender = tls13_cert_req_senders;
            break;

        case ssl_hs_certificate:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
            sender = ss->xtnData.certificateSenders;
            break;

        case ssl_hs_encrypted_extensions:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
            sender = ss->xtnData.encryptedExtensionsSenders;
            break;

        case ssl_hs_hello_retry_request:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
            sender = tls13_hrr_senders;
            break;

        default:
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
    }

    for (; sender->ex_sender != NULL; ++sender) {
        PRBool append = PR_FALSE;
        unsigned int start = buf->len;
        unsigned int length;

        if (ssl_FindCustomExtensionHooks(ss, sender->ex_type)) {
            continue;
        }

        /* Save space for the extension type and length. Note that we don't grow
         * the buffer now; rely on sslBuffer_Append* to do that. */
        buf->len += 4;
        rv = (*sender->ex_sender)(ss, &ss->xtnData, buf, &append);
        if (rv != SECSuccess) {
            goto loser;
        }

        /* Save the length and go back to the start. */
        length = buf->len - start - 4;
        buf->len = start;
        if (!append) {
            continue;
        }

        buf->len = start;
        rv = sslBuffer_AppendNumber(buf, sender->ex_type, 2);
        if (rv != SECSuccess) {
            goto loser; /* Code already set. */
        }
        rv = sslBuffer_AppendNumber(buf, length, 2);
        if (rv != SECSuccess) {
            goto loser; /* Code already set. */
        }
        /* Skip over the extension body. */
        buf->len += length;

        if (message == ssl_hs_client_hello ||
            message == ssl_hs_certificate_request) {
            ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
                sender->ex_type;
        }
    }

    if (!PR_CLIST_IS_EMPTY(&ss->extensionHooks)) {
        if (message == ssl_hs_client_hello && ss->opt.callExtensionWriterOnEchInner) {
            message = ssl_hs_ech_outer_client_hello;
        }
        rv = ssl_CallCustomExtensionSenders(ss, buf, message);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    if (buf->len > 0xffff) {
        PORT_SetError(SSL_ERROR_TX_RECORD_TOO_LONG);
        goto loser;
    }

    return SECSuccess;

loser:
    sslBuffer_Clear(buf);
    return SECFailure;
}

/* This extension sender can be used anywhere that an always empty extension is
 * needed. Mostly that is for ServerHello where sender registration is dynamic;
 * ClientHello senders are usually conditional in some way. */
SECStatus
ssl_SendEmptyExtension(const sslSocket *ss, TLSExtensionData *xtnData,
                       sslBuffer *buf, PRBool *append)
{
    *append = PR_TRUE;
    return SECSuccess;
}

/* Takes the size of the ClientHello, less the record header, and determines how
 * much padding is required. */
static unsigned int
ssl_CalculatePaddingExtLen(const sslSocket *ss, unsigned int clientHelloLength)
{
    unsigned int extensionLen;

    /* Don't pad for DTLS, for SSLv3, or for renegotiation. */
    if (IS_DTLS(ss) ||
        ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_0 ||
        ss->firstHsDone) {
        return 0;
    }

    /* A padding extension may be included to ensure that the record containing
     * the ClientHello doesn't have a length between 256 and 511 bytes
     * (inclusive). Initial ClientHello records with such lengths trigger bugs
     * in F5 devices. */
    if (clientHelloLength < 256 || clientHelloLength >= 512) {
        return 0;
    }

    extensionLen = 512 - clientHelloLength;
    /* Extensions take at least four bytes to encode. Always include at least
     * one byte of data if we are padding. Some servers will time out or
     * terminate the connection if the last ClientHello extension is empty. */
    if (extensionLen < 5) {
        extensionLen = 5;
    }

    return extensionLen - 4;
}

/* Manually insert an extension, retaining the position of the PSK
 * extension, if present. */
SECStatus
ssl3_EmplaceExtension(sslSocket *ss, sslBuffer *buf, PRUint16 exType,
                      const PRUint8 *data, unsigned int len, PRBool advertise)
{
    SECStatus rv;
    unsigned int tailLen;

    /* Move the tail if there is one. This only happens if we are sending the
     * TLS 1.3 PSK extension, which needs to be at the end. */
    if (ss->xtnData.lastXtnOffset) {
        PORT_Assert(buf->len > ss->xtnData.lastXtnOffset);
        tailLen = buf->len - ss->xtnData.lastXtnOffset;
        rv = sslBuffer_Grow(buf, buf->len + 4 + len);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        PORT_Memmove(buf->buf + ss->xtnData.lastXtnOffset + 4 + len,
                     buf->buf + ss->xtnData.lastXtnOffset,
                     tailLen);
        buf->len = ss->xtnData.lastXtnOffset;
    } else {
        tailLen = 0;
    }
    if (exType == ssl_tls13_encrypted_client_hello_xtn) {
        ss->xtnData.echXtnOffset = buf->len;
    }
    rv = sslBuffer_AppendNumber(buf, exType, 2);
    if (rv != SECSuccess) {
        return SECFailure; /* Code already set. */
    }
    rv = sslBuffer_AppendVariable(buf, data, len, 2);
    if (rv != SECSuccess) {
        return SECFailure; /* Code already set. */
    }

    if (ss->xtnData.lastXtnOffset) {
        ss->xtnData.lastXtnOffset += 4 + len;
    }

    buf->len += tailLen;

    /* False only to retain behavior with padding_xtn. Maybe
     * we can just mark that advertised as well? TODO */
    if (advertise) {
        ss->xtnData.advertised[ss->xtnData.numAdvertised++] = exType;
    }

    return SECSuccess;
}

/* ssl3_SendPaddingExtension possibly adds an extension which ensures that a
 * ClientHello record is either < 256 bytes or is >= 512 bytes. This ensures
 * that we don't trigger bugs in F5 products.
 *
 * This takes an existing extension buffer, |buf|, and the length of the
 * remainder of the ClientHello, |prefixLen|.  It modifies the extension buffer
 * to insert padding at the right place.
 */
SECStatus
ssl_InsertPaddingExtension(sslSocket *ss, unsigned int prefixLen,
                           sslBuffer *buf)
{
    static unsigned char padding[252] = { 0 };
    unsigned int paddingLen;
    /* Exit early if an application-provided extension hook
     * already added padding. */
    if (ssl3_ExtensionAdvertised(ss, ssl_padding_xtn)) {
        return SECSuccess;
    }

    /* Account for the size of the header, the length field of the extensions
     * block and the size of the existing extensions. */
    paddingLen = ssl_CalculatePaddingExtLen(ss, prefixLen + 2 + buf->len);
    if (!paddingLen) {
        return SECSuccess;
    }

    return ssl3_EmplaceExtension(ss, buf, ssl_padding_xtn, padding, paddingLen, PR_FALSE);
}

void
ssl3_MoveRemoteExtensions(PRCList *dst, PRCList *src)
{
    PRCList *cur_p;
    while (!PR_CLIST_IS_EMPTY(src)) {
        cur_p = PR_LIST_TAIL(src);
        PR_REMOVE_LINK(cur_p);
        PR_INSERT_LINK(cur_p, dst);
    }
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

/* Initialize the extension data block. */
void
ssl3_InitExtensionData(TLSExtensionData *xtnData, const sslSocket *ss)
{
    unsigned int advertisedMax;
    PRCList *cursor;

    /* Set things up to the right starting state. */
    PORT_Memset(xtnData, 0, sizeof(*xtnData));
    xtnData->peerSupportsFfdheGroups = PR_FALSE;
    PR_INIT_CLIST(&xtnData->remoteKeyShares);

    /* Allocate enough to allow for native extensions, plus any custom ones. */
    if (ss->sec.isServer) {
        advertisedMax = PR_MAX(PR_ARRAY_SIZE(certificateRequestHandlers),
                               PR_ARRAY_SIZE(tls13_cert_req_senders));
    } else {
        advertisedMax = PR_MAX(PR_ARRAY_SIZE(clientHelloHandlers),
                               PR_ARRAY_SIZE(clientHelloSendersTLS));
        ++advertisedMax; /* For the RI SCSV, which we also track. */
    }
    for (cursor = PR_NEXT_LINK(&ss->extensionHooks);
         cursor != &ss->extensionHooks;
         cursor = PR_NEXT_LINK(cursor)) {
        ++advertisedMax;
    }
    xtnData->advertised = PORT_ZNewArray(PRUint16, advertisedMax);
    xtnData->echAdvertised = PORT_ZNewArray(PRUint16, advertisedMax);

    xtnData->peerDelegCred = NULL;
    xtnData->peerRequestedDelegCred = PR_FALSE;
    xtnData->sendingDelegCredToPeer = PR_FALSE;
    xtnData->selectedPsk = NULL;
}

void
ssl3_DestroyExtensionData(TLSExtensionData *xtnData)
{
    ssl3_FreeSniNameArray(xtnData);
    PORT_Free(xtnData->sigSchemes);
    PORT_Free(xtnData->delegCredSigSchemes);
    PORT_Free(xtnData->delegCredSigSchemesAdvertised);
    SECITEM_FreeItem(&xtnData->nextProto, PR_FALSE);
    tls13_DestroyKeyShares(&xtnData->remoteKeyShares);
    SECITEM_FreeItem(&xtnData->certReqContext, PR_FALSE);
    SECITEM_FreeItem(&xtnData->applicationToken, PR_FALSE);
    if (xtnData->certReqAuthorities.arena) {
        PORT_FreeArena(xtnData->certReqAuthorities.arena, PR_FALSE);
        xtnData->certReqAuthorities.arena = NULL;
    }
    PORT_Free(xtnData->advertised);
    PORT_Free(xtnData->echAdvertised);
    tls13_DestroyDelegatedCredential(xtnData->peerDelegCred);

    tls13_DestroyEchXtnState(xtnData->ech);
    xtnData->ech = NULL;
}

/* Free everything that has been allocated and then reset back to
 * the starting state. */
void
ssl3_ResetExtensionData(TLSExtensionData *xtnData, const sslSocket *ss)
{
    ssl3_DestroyExtensionData(xtnData);
    ssl3_InitExtensionData(xtnData, ss);
}

/* Thunks to let extension handlers operate on const sslSocket* objects. */
void
ssl3_ExtSendAlert(const sslSocket *ss, SSL3AlertLevel level,
                  SSL3AlertDescription desc)
{
    (void)SSL3_SendAlert((sslSocket *)ss, level, desc);
}

void
ssl3_ExtDecodeError(const sslSocket *ss)
{
    (void)ssl3_DecodeError((sslSocket *)ss);
}

SECStatus
ssl3_ExtConsumeHandshake(const sslSocket *ss, void *v, PRUint32 bytes,
                         PRUint8 **b, PRUint32 *length)
{
    return ssl3_ConsumeHandshake((sslSocket *)ss, v, bytes, b, length);
}

SECStatus
ssl3_ExtConsumeHandshakeNumber(const sslSocket *ss, PRUint32 *num,
                               PRUint32 bytes, PRUint8 **b, PRUint32 *length)
{
    return ssl3_ConsumeHandshakeNumber((sslSocket *)ss, num, bytes, b, length);
}

SECStatus
ssl3_ExtConsumeHandshakeVariable(const sslSocket *ss, SECItem *i,
                                 PRUint32 bytes, PRUint8 **b,
                                 PRUint32 *length)
{
    return ssl3_ConsumeHandshakeVariable((sslSocket *)ss, i, bytes, b, length);
}
