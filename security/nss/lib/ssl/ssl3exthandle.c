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
#include "blapit.h"
#include "prinit.h"
#include "selfencrypt.h"
#include "ssl3encode.h"
#include "ssl3ext.h"
#include "ssl3exthandle.h"
#include "tls13exthandle.h" /* For tls13_ServerSendStatusRequestXtn. */

/* Format an SNI extension, using the name from the socket's URL,
 * unless that name is a dotted decimal string.
 * Used by client and server.
 */
PRInt32
ssl3_SendServerNameXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                       PRUint32 maxBytes)
{
    SECStatus rv;
    if (!ss)
        return 0;
    if (!ss->sec.isServer) {
        PRUint32 len;
        PRNetAddr netAddr;

        /* must have a hostname */
        if (!ss->url || !ss->url[0])
            return 0;
        /* must not be an IPv4 or IPv6 address */
        if (PR_SUCCESS == PR_StringToNetAddr(ss->url, &netAddr)) {
            /* is an IP address (v4 or v6) */
            return 0;
        }
        len = PORT_Strlen(ss->url);
        if (append && maxBytes >= len + 9) {
            /* extension_type */
            rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_server_name_xtn, 2);
            if (rv != SECSuccess)
                return -1;
            /* length of extension_data */
            rv = ssl3_ExtAppendHandshakeNumber(ss, len + 5, 2);
            if (rv != SECSuccess)
                return -1;
            /* length of server_name_list */
            rv = ssl3_ExtAppendHandshakeNumber(ss, len + 3, 2);
            if (rv != SECSuccess)
                return -1;
            /* Name Type (sni_host_name) */
            rv = ssl3_ExtAppendHandshake(ss, "\0", 1);
            if (rv != SECSuccess)
                return -1;
            /* HostName (length and value) */
            rv = ssl3_ExtAppendHandshakeVariable(ss, (PRUint8 *)ss->url, len, 2);
            if (rv != SECSuccess)
                return -1;
            if (!ss->sec.isServer) {
                xtnData->advertised[xtnData->numAdvertised++] =
                    ssl_server_name_xtn;
            }
        }
        return len + 9;
    }
    /* Server side */
    if (append && maxBytes >= 4) {
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_server_name_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* length of extension_data */
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
    }
    return 4;
}

/* Handle an incoming SNI extension. */
SECStatus
ssl3_HandleServerNameXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type, SECItem *data)
{
    SECItem *names = NULL;
    PRUint32 listLenBytes = 0;
    SECStatus rv;

    if (!ss->sec.isServer) {
        return SECSuccess; /* ignore extension */
    }

    /* Server side - consume client data and register server sender. */
    /* do not parse the data if don't have user extension handling function. */
    if (!ss->sniSocketConfig) {
        return SECSuccess;
    }

    /* length of server_name_list */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &listLenBytes, 2, &data->data, &data->len);
    if (rv != SECSuccess) {
        goto loser; /* alert already sent */
    }
    if (listLenBytes == 0 || listLenBytes != data->len) {
        goto alert_loser;
    }

    /* Read ServerNameList. */
    while (data->len > 0) {
        SECItem tmp;
        PRUint32 type;

        /* Read Name Type. */
        rv = ssl3_ExtConsumeHandshakeNumber(ss, &type, 1, &data->data, &data->len);
        if (rv != SECSuccess) {
            /* alert sent in ConsumeHandshakeNumber */
            goto loser;
        }

        /* Read ServerName (length and value). */
        rv = ssl3_ExtConsumeHandshakeVariable(ss, &tmp, 2, &data->data, &data->len);
        if (rv != SECSuccess) {
            goto loser;
        }

        /* Record the value for host_name(0). */
        if (type == sni_nametype_hostname) {
            /* Fail if we encounter a second host_name entry. */
            if (names) {
                goto alert_loser;
            }

            /* Create an array for the only supported NameType. */
            names = PORT_ZNewArray(SECItem, 1);
            if (!names) {
                goto loser;
            }

            /* Copy ServerName into the array. */
            if (SECITEM_CopyItem(NULL, &names[0], &tmp) != SECSuccess) {
                goto loser;
            }
        }

        /* Even if we don't support NameTypes other than host_name at the
         * moment, we continue parsing the whole list to check its validity.
         * We do not check for duplicate entries with NameType != host_name(0).
         */
    }
    if (names) {
        /* Free old and set the new data. */
        ssl3_FreeSniNameArray(xtnData);
        xtnData->sniNameArr = names;
        xtnData->sniNameArrSize = 1;
        xtnData->negotiated[xtnData->numNegotiated++] = ssl_server_name_xtn;
    }
    return SECSuccess;

alert_loser:
    ssl3_ExtDecodeError(ss);
loser:
    if (names) {
        PORT_Free(names);
    }
    return SECFailure;
}

/* Frees a given xtnData->sniNameArr and its elements. */
void
ssl3_FreeSniNameArray(TLSExtensionData *xtnData)
{
    PRUint32 i;

    if (!xtnData->sniNameArr) {
        return;
    }

    for (i = 0; i < xtnData->sniNameArrSize; i++) {
        SECITEM_FreeItem(&xtnData->sniNameArr[i], PR_FALSE);
    }

    PORT_Free(xtnData->sniNameArr);
    xtnData->sniNameArr = NULL;
    xtnData->sniNameArrSize = 0;
}

/* Called by both clients and servers.
 * Clients sends a filled in session ticket if one is available, and otherwise
 * sends an empty ticket.  Servers always send empty tickets.
 */
PRInt32
ssl3_SendSessionTicketXtn(
    const sslSocket *ss,
    TLSExtensionData *xtnData,
    PRBool append,
    PRUint32 maxBytes)
{
    PRInt32 extension_length;
    NewSessionTicket *session_ticket = NULL;
    sslSessionID *sid = ss->sec.ci.sid;

    /* Never send an extension with a ticket for TLS 1.3, but
     * OK to send the empty one in case the server does 1.2. */
    if (sid->cached == in_client_cache &&
        sid->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        return 0;
    }

    /* Ignore the SessionTicket extension if processing is disabled. */
    if (!ss->opt.enableSessionTickets)
        return 0;

    /* Empty extension length = extension_type (2-bytes) +
     * length(extension_data) (2-bytes)
     */
    extension_length = 4;

    /* If we are a client then send a session ticket if one is availble.
     * Servers that support the extension and are willing to negotiate the
     * the extension always respond with an empty extension.
     */
    if (!ss->sec.isServer) {
        /* The caller must be holding sid->u.ssl3.lock for reading. We cannot
         * just acquire and release the lock within this function because the
         * caller will call this function twice, and we need the inputs to be
         * consistent between the two calls. Note that currently the caller
         * will only be holding the lock when we are the client and when we're
         * attempting to resume an existing session.
         */

        session_ticket = &sid->u.ssl3.locked.sessionTicket;
        if (session_ticket->ticket.data) {
            if (xtnData->ticketTimestampVerified) {
                extension_length += session_ticket->ticket.len;
            } else if (!append && ssl_TicketTimeValid(session_ticket)) {
                extension_length += session_ticket->ticket.len;
                xtnData->ticketTimestampVerified = PR_TRUE;
            }
        }
    }

    if (maxBytes < (PRUint32)extension_length) {
        PORT_Assert(0);
        return 0;
    }
    if (append) {
        SECStatus rv;
        /* extension_type */
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_session_ticket_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        if (session_ticket && session_ticket->ticket.data &&
            xtnData->ticketTimestampVerified) {
            rv = ssl3_ExtAppendHandshakeVariable(ss, session_ticket->ticket.data,
                                                 session_ticket->ticket.len, 2);
            xtnData->ticketTimestampVerified = PR_FALSE;
            xtnData->sentSessionTicketInClientHello = PR_TRUE;
        } else {
            rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        }
        if (rv != SECSuccess)
            goto loser;

        if (!ss->sec.isServer) {
            xtnData->advertised[xtnData->numAdvertised++] =
                ssl_session_ticket_xtn;
        }
    }
    return extension_length;

loser:
    xtnData->ticketTimestampVerified = PR_FALSE;
    return -1;
}

PRBool
ssl_AlpnTagAllowed(const sslSocket *ss, const SECItem *tag)
{
    const unsigned char *data = ss->opt.nextProtoNego.data;
    unsigned int length = ss->opt.nextProtoNego.len;
    unsigned int offset = 0;

    if (!tag->len)
        return PR_TRUE;

    while (offset < length) {
        unsigned int taglen = (unsigned int)data[offset];
        if ((taglen == tag->len) &&
            !PORT_Memcmp(data + offset + 1, tag->data, tag->len))
            return PR_TRUE;
        offset += 1 + taglen;
    }

    return PR_FALSE;
}

/* handle an incoming Next Protocol Negotiation extension. */
SECStatus
ssl3_ServerHandleNextProtoNegoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                  SECItem *data)
{
    if (ss->firstHsDone || data->len != 0) {
        /* Clients MUST send an empty NPN extension, if any. */
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;

    /* TODO: server side NPN support would require calling
     * ssl3_RegisterServerHelloExtensionSender here in order to echo the
     * extension back to the client. */

    return SECSuccess;
}

/* ssl3_ValidateNextProtoNego checks that the given block of data is valid: none
 * of the lengths may be 0 and the sum of the lengths must equal the length of
 * the block. */
SECStatus
ssl3_ValidateNextProtoNego(const unsigned char *data, unsigned int length)
{
    unsigned int offset = 0;

    while (offset < length) {
        unsigned int newOffset = offset + 1 + (unsigned int)data[offset];
        /* Reject embedded nulls to protect against buggy applications that
         * store protocol identifiers in null-terminated strings.
         */
        if (newOffset > length || data[offset] == 0) {
            return SECFailure;
        }
        offset = newOffset;
    }

    return SECSuccess;
}

/* protocol selection handler for ALPN (server side) and NPN (client side) */
static SECStatus
ssl3_SelectAppProtocol(const sslSocket *ss, TLSExtensionData *xtnData,
                       PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    unsigned char resultBuffer[255];
    SECItem result = { siBuffer, resultBuffer, 0 };

    rv = ssl3_ValidateNextProtoNego(data->data, data->len);
    if (rv != SECSuccess) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return rv;
    }

    PORT_Assert(ss->nextProtoCallback);
    /* For ALPN, the cipher suite isn't selected yet.  Note that extensions
     * sometimes affect what cipher suite is selected, e.g., for ECC. */
    PORT_Assert((ss->ssl3.hs.preliminaryInfo &
                 ssl_preinfo_all & ~ssl_preinfo_cipher_suite) ==
                (ssl_preinfo_all & ~ssl_preinfo_cipher_suite));
    rv = ss->nextProtoCallback(ss->nextProtoArg, ss->fd, data->data, data->len,
                               result.data, &result.len, sizeof(resultBuffer));
    if (rv != SECSuccess) {
        /* Expect callback to call PORT_SetError() */
        ssl3_ExtSendAlert(ss, alert_fatal, internal_error);
        return SECFailure;
    }

    /* If the callback wrote more than allowed to |result| it has corrupted our
     * stack. */
    if (result.len > sizeof(resultBuffer)) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        /* TODO: crash */
        return SECFailure;
    }

    SECITEM_FreeItem(&xtnData->nextProto, PR_FALSE);

    if (ex_type == ssl_app_layer_protocol_xtn &&
        xtnData->nextProtoState != SSL_NEXT_PROTO_NEGOTIATED) {
        /* The callback might say OK, but then it picks a default value - one
         * that was not listed.  That's OK for NPN, but not ALPN. */
        ssl3_ExtSendAlert(ss, alert_fatal, no_application_protocol);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_NO_PROTOCOL);
        return SECFailure;
    }

    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    return SECITEM_CopyItem(NULL, &xtnData->nextProto, &result);
}

/* handle an incoming ALPN extension at the server */
SECStatus
ssl3_ServerHandleAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type, SECItem *data)
{
    PRUint32 count;
    SECStatus rv;

    /* We expressly don't want to allow ALPN on renegotiation,
     * despite it being permitted by the spec. */
    if (ss->firstHsDone || data->len == 0) {
        /* Clients MUST send a non-empty ALPN extension. */
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    /* Unlike NPN, ALPN has extra redundant length information so that
     * the extension is the same in both ClientHello and ServerHello. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &count, 2, &data->data, &data->len);
    if (rv != SECSuccess || count != data->len) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }

    if (!ss->nextProtoCallback) {
        /* we're not configured for it */
        return SECSuccess;
    }

    rv = ssl3_SelectAppProtocol(ss, xtnData, ex_type, data);
    if (rv != SECSuccess) {
        return rv;
    }

    /* prepare to send back a response, if we negotiated */
    if (xtnData->nextProtoState == SSL_NEXT_PROTO_NEGOTIATED) {
        rv = ssl3_RegisterExtensionSender(
            ss, xtnData, ex_type, ssl3_ServerSendAppProtoXtn);
        if (rv != SECSuccess) {
            ssl3_ExtSendAlert(ss, alert_fatal, internal_error);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return rv;
        }
    }
    return SECSuccess;
}

SECStatus
ssl3_ClientHandleNextProtoNegoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                  SECItem *data)
{
    PORT_Assert(!ss->firstHsDone);

    if (ssl3_ExtensionNegotiated(ss, ssl_app_layer_protocol_xtn)) {
        /* If the server negotiated ALPN then it has already told us what
         * protocol to use, so it doesn't make sense for us to try to negotiate
         * a different one by sending the NPN handshake message. However, if
         * we've negotiated NPN then we're required to send the NPN handshake
         * message. Thus, these two extensions cannot both be negotiated on the
         * same connection. */
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_BAD_SERVER);
        return SECFailure;
    }

    /* We should only get this call if we sent the extension, so
     * ss->nextProtoCallback needs to be non-NULL.  However, it is possible
     * that an application erroneously cleared the callback between the time
     * we sent the ClientHello and now. */
    if (!ss->nextProtoCallback) {
        PORT_Assert(0);
        ssl3_ExtSendAlert(ss, alert_fatal, internal_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_NO_CALLBACK);
        return SECFailure;
    }

    return ssl3_SelectAppProtocol(ss, xtnData, ex_type, data);
}

SECStatus
ssl3_ClientHandleAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    PRUint32 list_len;
    SECItem protocol_name;

    if (ssl3_ExtensionNegotiated(ss, ssl_next_proto_nego_xtn)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* The extension data from the server has the following format:
     *   uint16 name_list_len;
     *   uint8 len;  // where len >= 1
     *   uint8 protocol_name[len]; */
    if (data->len < 4 || data->len > 2 + 1 + 255) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &list_len, 2, &data->data,
                                        &data->len);
    /* The list has to be the entire extension. */
    if (rv != SECSuccess || list_len != data->len) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    rv = ssl3_ExtConsumeHandshakeVariable(ss, &protocol_name, 1,
                                          &data->data, &data->len);
    /* The list must have exactly one value. */
    if (rv != SECSuccess || data->len != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    if (!ssl_AlpnTagAllowed(ss, &protocol_name)) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    SECITEM_FreeItem(&xtnData->nextProto, PR_FALSE);
    xtnData->nextProtoState = SSL_NEXT_PROTO_SELECTED;
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    return SECITEM_CopyItem(NULL, &xtnData->nextProto, &protocol_name);
}

PRInt32
ssl3_ClientSendNextProtoNegoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                PRUint32 maxBytes)
{
    PRInt32 extension_length;

    /* Renegotiations do not send this extension. */
    if (!ss->opt.enableNPN || !ss->nextProtoCallback || ss->firstHsDone) {
        return 0;
    }

    extension_length = 4;

    if (maxBytes < (PRUint32)extension_length) {
        return 0;
    }
    if (append) {
        SECStatus rv;
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_next_proto_nego_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            goto loser;
        xtnData->advertised[xtnData->numAdvertised++] =
            ssl_next_proto_nego_xtn;
    }

    return extension_length;

loser:
    return -1;
}

PRInt32
ssl3_ClientSendAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_length;
    unsigned char *alpn_protos = NULL;

    /* Renegotiations do not send this extension. */
    if (!ss->opt.enableALPN || !ss->opt.nextProtoNego.data || ss->firstHsDone) {
        return 0;
    }

    extension_length = 2 /* extension type */ + 2 /* extension length */ +
                       2 /* protocol name list length */ +
                       ss->opt.nextProtoNego.len;

    if (maxBytes < (PRUint32)extension_length) {
        return 0;
    }
    if (append) {
        /* NPN requires that the client's fallback protocol is first in the
         * list. However, ALPN sends protocols in preference order. So we
         * allocate a buffer and move the first protocol to the end of the
         * list. */
        SECStatus rv;
        const unsigned int len = ss->opt.nextProtoNego.len;

        alpn_protos = PORT_Alloc(len);
        if (alpn_protos == NULL) {
            return SECFailure;
        }
        if (len > 0) {
            /* Each protocol string is prefixed with a single byte length. */
            unsigned int i = ss->opt.nextProtoNego.data[0] + 1;
            if (i <= len) {
                memcpy(alpn_protos, &ss->opt.nextProtoNego.data[i], len - i);
                memcpy(alpn_protos + len - i, ss->opt.nextProtoNego.data, i);
            } else {
                /* This seems to be invalid data so we'll send as-is. */
                memcpy(alpn_protos, ss->opt.nextProtoNego.data, len);
            }
        }

        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_app_layer_protocol_xtn, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = ssl3_ExtAppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = ssl3_ExtAppendHandshakeVariable(ss, alpn_protos, len, 2);
        PORT_Free(alpn_protos);
        alpn_protos = NULL;
        if (rv != SECSuccess) {
            goto loser;
        }
        xtnData->advertised[xtnData->numAdvertised++] =
            ssl_app_layer_protocol_xtn;
    }

    return extension_length;

loser:
    if (alpn_protos) {
        PORT_Free(alpn_protos);
    }
    return -1;
}

PRInt32
ssl3_ServerSendAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_length;

    /* we're in over our heads if any of these fail */
    PORT_Assert(ss->opt.enableALPN);
    PORT_Assert(xtnData->nextProto.data);
    PORT_Assert(xtnData->nextProto.len > 0);
    PORT_Assert(xtnData->nextProtoState == SSL_NEXT_PROTO_NEGOTIATED);
    PORT_Assert(!ss->firstHsDone);

    extension_length = 2 /* extension type */ + 2 /* extension length */ +
                       2 /* protocol name list */ + 1 /* name length */ +
                       xtnData->nextProto.len;

    if (maxBytes < (PRUint32)extension_length) {
        return 0;
    }
    if (append) {
        SECStatus rv;
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_app_layer_protocol_xtn, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_ExtAppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_ExtAppendHandshakeNumber(ss, xtnData->nextProto.len + 1, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_ExtAppendHandshakeVariable(ss, xtnData->nextProto.data,
                                             xtnData->nextProto.len, 1);
        if (rv != SECSuccess) {
            return -1;
        }
    }

    return extension_length;
}

SECStatus
ssl3_ServerHandleStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                  SECItem *data)
{
    ssl3HelloExtensionSenderFunc sender;

    PORT_Assert(ss->sec.isServer);

    /* remember that we got this extension. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        sender = tls13_ServerSendStatusRequestXtn;
    } else {
        sender = ssl3_ServerSendStatusRequestXtn;
    }
    return ssl3_RegisterExtensionSender(ss, xtnData, ex_type, sender);
}

PRInt32
ssl3_ServerSendStatusRequestXtn(
    const sslSocket *ss,
    TLSExtensionData *xtnData,
    PRBool append,
    PRUint32 maxBytes)
{
    PRInt32 extension_length;
    const sslServerCert *serverCert = ss->sec.serverCert;
    SECStatus rv;

    if (!serverCert->certStatusArray ||
        !serverCert->certStatusArray->len) {
        return 0;
    }

    extension_length = 2 + 2;
    if (maxBytes < (PRUint32)extension_length) {
        return 0;
    }
    if (append) {
        /* extension_type */
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_cert_status_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* length of extension_data */
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
        /* The certificate status data is sent in ssl3_SendCertificateStatus. */
    }

    return extension_length;
}

/* ssl3_ClientSendStatusRequestXtn builds the status_request extension on the
 * client side. See RFC 6066 section 8. */
PRInt32
ssl3_ClientSendStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                PRUint32 maxBytes)
{
    PRInt32 extension_length;

    if (!ss->opt.enableOCSPStapling)
        return 0;

    /* extension_type (2-bytes) +
     * length(extension_data) (2-bytes) +
     * status_type (1) +
     * responder_id_list length (2) +
     * request_extensions length (2)
     */
    extension_length = 9;

    if (maxBytes < (PRUint32)extension_length) {
        PORT_Assert(0);
        return 0;
    }
    if (append) {
        SECStatus rv;

        /* extension_type */
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_cert_status_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        rv = ssl3_ExtAppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            return -1;
        rv = ssl3_ExtAppendHandshakeNumber(ss, 1 /* status_type ocsp */, 1);
        if (rv != SECSuccess)
            return -1;
        /* A zero length responder_id_list means that the responders are
         * implicitly known to the server. */
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
        /* A zero length request_extensions means that there are no extensions.
         * Specifically, we don't set the id-pkix-ocsp-nonce extension. This
         * means that the server can replay a cached OCSP response to us. */
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;

        xtnData->advertised[xtnData->numAdvertised++] = ssl_cert_status_xtn;
    }
    return extension_length;
}

SECStatus
ssl3_ClientHandleStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                  SECItem *data)
{
    /* In TLS 1.3, the extension carries the OCSP response. */
    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        SECStatus rv;
        rv = ssl_ReadCertificateStatus(CONST_CAST(sslSocket, ss),
                                       data->data, data->len);
        if (rv != SECSuccess) {
            return SECFailure; /* code already set */
        }
    } else if (data->len != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    return SECSuccess;
}

PRUint32 ssl_ticket_lifetime = 2 * 24 * 60 * 60; /* 2 days in seconds */
#define TLS_EX_SESS_TICKET_VERSION (0x0105)

/*
 * Called from ssl3_SendNewSessionTicket, tls13_SendNewSessionTicket
 */
SECStatus
ssl3_EncodeSessionTicket(sslSocket *ss,
                         const NewSessionTicket *ticket,
                         SECItem *ticket_data)
{
    SECStatus rv;
    SECItem plaintext;
    SECItem plaintext_item = { 0, NULL, 0 };
    PRUint32 plaintext_length;
    SECItem ticket_buf = { 0, NULL, 0 };
    PRBool ms_is_wrapped;
    unsigned char wrapped_ms[SSL3_MASTER_SECRET_LENGTH];
    SECItem ms_item = { 0, NULL, 0 };
    PRUint32 cert_length = 0;
    PRUint32 now;
    SECItem *srvName = NULL;
    CK_MECHANISM_TYPE msWrapMech = 0; /* dummy default value,
                                          * must be >= 0 */
    ssl3CipherSpec *spec;
    SECItem *alpnSelection = NULL;

    SSL_TRC(3, ("%d: SSL3[%d]: send session_ticket handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->opt.requestCertificate && ss->sec.ci.sid->peerCert) {
        cert_length = 2 + ss->sec.ci.sid->peerCert->derCert.len;
    }

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        spec = ss->ssl3.cwSpec;
    } else {
        spec = ss->ssl3.pwSpec;
    }
    if (spec->msItem.len && spec->msItem.data) {
        /* The master secret is available unwrapped. */
        ms_item.data = spec->msItem.data;
        ms_item.len = spec->msItem.len;
        ms_is_wrapped = PR_FALSE;
    } else {
        /* Extract the master secret wrapped. */
        sslSessionID sid;
        PORT_Memset(&sid, 0, sizeof(sslSessionID));

        rv = ssl3_CacheWrappedMasterSecret(ss, &sid, spec);
        if (rv == SECSuccess) {
            if (sid.u.ssl3.keys.wrapped_master_secret_len > sizeof(wrapped_ms))
                goto loser;
            memcpy(wrapped_ms, sid.u.ssl3.keys.wrapped_master_secret,
                   sid.u.ssl3.keys.wrapped_master_secret_len);
            ms_item.data = wrapped_ms;
            ms_item.len = sid.u.ssl3.keys.wrapped_master_secret_len;
            msWrapMech = sid.u.ssl3.masterWrapMech;
        } else {
            /* TODO: else send an empty ticket. */
            goto loser;
        }
        ms_is_wrapped = PR_TRUE;
    }
    /* Prep to send negotiated name */
    srvName = &ss->sec.ci.sid->u.ssl3.srvName;

    PORT_Assert(ss->xtnData.nextProtoState == SSL_NEXT_PROTO_SELECTED ||
                ss->xtnData.nextProtoState == SSL_NEXT_PROTO_NEGOTIATED ||
                ss->xtnData.nextProto.len == 0);
    alpnSelection = &ss->xtnData.nextProto;

    plaintext_length =
        sizeof(PRUint16)                       /* ticket version */
        + sizeof(SSL3ProtocolVersion)          /* ssl_version */
        + sizeof(ssl3CipherSuite)              /* ciphersuite */
        + 1                                    /* compression */
        + 10                                   /* cipher spec parameters */
        + 1                                    /* certType arguments */
        + 1                                    /* SessionTicket.ms_is_wrapped */
        + 4                                    /* msWrapMech */
        + 2                                    /* master_secret.length */
        + ms_item.len                          /* master_secret */
        + 1                                    /* client_auth_type */
        + cert_length                          /* cert */
        + 2 + srvName->len                     /* name len + length field */
        + 1                                    /* extendedMasterSecretUsed */
        + sizeof(ticket->ticket_lifetime_hint) /* ticket lifetime hint */
        + sizeof(ticket->flags)                /* ticket flags */
        + 1 + alpnSelection->len               /* alpn value + length field */
        + 4;                                   /* maxEarlyData */

    if (SECITEM_AllocItem(NULL, &plaintext_item, plaintext_length) == NULL)
        goto loser;

    plaintext = plaintext_item;

    /* ticket version */
    rv = ssl3_AppendNumberToItem(&plaintext, TLS_EX_SESS_TICKET_VERSION,
                                 sizeof(PRUint16));
    if (rv != SECSuccess)
        goto loser;

    /* ssl_version */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->version,
                                 sizeof(SSL3ProtocolVersion));
    if (rv != SECSuccess)
        goto loser;

    /* ciphersuite */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->ssl3.hs.cipher_suite,
                                 sizeof(ssl3CipherSuite));
    if (rv != SECSuccess)
        goto loser;

    /* compression */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->ssl3.hs.compression, 1);
    if (rv != SECSuccess)
        goto loser;

    /* cipher spec parameters */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.authType, 1);
    if (rv != SECSuccess)
        goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.authKeyBits, 4);
    if (rv != SECSuccess)
        goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.keaType, 1);
    if (rv != SECSuccess)
        goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.keaKeyBits, 4);
    if (rv != SECSuccess)
        goto loser;

    /* certificate type */
    PORT_Assert(SSL_CERT_IS(ss->sec.serverCert, ss->sec.authType));
    if (SSL_CERT_IS_EC(ss->sec.serverCert)) {
        const sslServerCert *cert = ss->sec.serverCert;
        PORT_Assert(cert->namedCurve);
        /* EC curves only use the second of the two bytes. */
        PORT_Assert(cert->namedCurve->name < 256);
        rv = ssl3_AppendNumberToItem(&plaintext, cert->namedCurve->name, 1);
    } else {
        rv = ssl3_AppendNumberToItem(&plaintext, 0, 1);
    }
    if (rv != SECSuccess)
        goto loser;

    /* master_secret */
    rv = ssl3_AppendNumberToItem(&plaintext, ms_is_wrapped, 1);
    if (rv != SECSuccess)
        goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, msWrapMech, 4);
    if (rv != SECSuccess)
        goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ms_item.len, 2);
    if (rv != SECSuccess)
        goto loser;
    rv = ssl3_AppendToItem(&plaintext, ms_item.data, ms_item.len);
    if (rv != SECSuccess)
        goto loser;

    /* client identity */
    if (ss->opt.requestCertificate && ss->sec.ci.sid->peerCert) {
        rv = ssl3_AppendNumberToItem(&plaintext, CLIENT_AUTH_CERTIFICATE, 1);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendNumberToItem(&plaintext,
                                     ss->sec.ci.sid->peerCert->derCert.len, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendToItem(&plaintext,
                               ss->sec.ci.sid->peerCert->derCert.data,
                               ss->sec.ci.sid->peerCert->derCert.len);
        if (rv != SECSuccess)
            goto loser;
    } else {
        rv = ssl3_AppendNumberToItem(&plaintext, 0, 1);
        if (rv != SECSuccess)
            goto loser;
    }

    /* timestamp */
    now = ssl_Time();
    rv = ssl3_AppendNumberToItem(&plaintext, now,
                                 sizeof(ticket->ticket_lifetime_hint));
    if (rv != SECSuccess)
        goto loser;

    /* HostName (length and value) */
    rv = ssl3_AppendNumberToItem(&plaintext, srvName->len, 2);
    if (rv != SECSuccess)
        goto loser;
    if (srvName->len) {
        rv = ssl3_AppendToItem(&plaintext, srvName->data, srvName->len);
        if (rv != SECSuccess)
            goto loser;
    }

    /* extendedMasterSecretUsed */
    rv = ssl3_AppendNumberToItem(
        &plaintext, ss->sec.ci.sid->u.ssl3.keys.extendedMasterSecretUsed, 1);
    if (rv != SECSuccess)
        goto loser;

    /* Flags */
    rv = ssl3_AppendNumberToItem(&plaintext, ticket->flags,
                                 sizeof(ticket->flags));
    if (rv != SECSuccess)
        goto loser;

    /* ALPN value. */
    PORT_Assert(alpnSelection->len < 256);
    rv = ssl3_AppendNumberToItem(&plaintext, alpnSelection->len, 1);
    if (rv != SECSuccess)
        goto loser;
    if (alpnSelection->len) {
        rv = ssl3_AppendToItem(&plaintext, alpnSelection->data,
                               alpnSelection->len);
        if (rv != SECSuccess)
            goto loser;
    }

    rv = ssl3_AppendNumberToItem(&plaintext, ssl_max_early_data_size, 4);
    if (rv != SECSuccess)
        goto loser;

    /* Check that we are totally full. */
    PORT_Assert(plaintext.len == 0);

    /* 128 just gives us enough room for overhead. */
    if (SECITEM_AllocItem(NULL, &ticket_buf, plaintext_length + 128) == NULL) {
        goto loser;
    }

    /* Finally, encrypt the ticket. */
    rv = ssl_SelfEncryptProtect(ss, plaintext_item.data, plaintext_item.len,
                                ticket_buf.data, &ticket_buf.len, ticket_buf.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Give ownership of memory to caller. */
    *ticket_data = ticket_buf;

    SECITEM_FreeItem(&plaintext_item, PR_FALSE);
    return SECSuccess;

loser:
    if (plaintext_item.data) {
        SECITEM_FreeItem(&plaintext_item, PR_FALSE);
    }
    if (ticket_buf.data) {
        SECITEM_FreeItem(&ticket_buf, PR_FALSE);
    }

    return SECFailure;
}

/* When a client receives a SessionTicket extension a NewSessionTicket
 * message is expected during the handshake.
 */
SECStatus
ssl3_ClientHandleSessionTicketXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                  SECItem *data)
{
    if (data->len != 0) {
        return SECSuccess; /* Ignore the extension. */
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    return SECSuccess;
}

static SECStatus
ssl_ParseSessionTicket(sslSocket *ss, const SECItem *decryptedTicket,
                       SessionTicket *parsedTicket)
{
    PRUint32 temp;
    SECStatus rv;

    PRUint8 *buffer = decryptedTicket->data;
    unsigned int len = decryptedTicket->len;

    PORT_Memset(parsedTicket, 0, sizeof(*parsedTicket));
    parsedTicket->valid = PR_FALSE;

    /* If the decrypted ticket is empty, then report success, but leave the
     * ticket marked as invalid. */
    if (decryptedTicket->len == 0) {
        return SECSuccess;
    }

    /* Read ticket version. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 2, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* Skip the ticket if the version is wrong.  This won't result in a
     * handshake failure, just a failure to resume. */
    if (temp != TLS_EX_SESS_TICKET_VERSION) {
        return SECSuccess;
    }

    /* Read SSLVersion. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 2, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->ssl_version = (SSL3ProtocolVersion)temp;
    if (!ssl3_VersionIsSupported(ss->protocolVariant,
                                 parsedTicket->ssl_version)) {
        /* This socket doesn't support the version from the ticket. */
        return SECSuccess;
    }

    /* Read cipher_suite. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 2, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->cipher_suite = (ssl3CipherSuite)temp;

    /* Read compression_method. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 1, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->compression_method = (SSLCompressionMethod)temp;

    /* Read cipher spec parameters. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 1, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->authType = (SSLAuthType)temp;
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->authKeyBits = temp;
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 1, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->keaType = (SSLKEAType)temp;
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->keaKeyBits = temp;

    /* Read the optional named curve. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 1, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (parsedTicket->authType == ssl_auth_ecdsa ||
        parsedTicket->authType == ssl_auth_ecdh_rsa ||
        parsedTicket->authType == ssl_auth_ecdh_ecdsa) {
        const sslNamedGroupDef *group =
            ssl_LookupNamedGroup((SSLNamedGroup)temp);
        if (!group || group->keaType != ssl_kea_ecdh) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
        parsedTicket->namedCurve = group;
    }

    /* Read the master secret (and how it is wrapped). */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 1, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    PORT_Assert(temp == PR_TRUE || temp == PR_FALSE);
    parsedTicket->ms_is_wrapped = (PRBool)temp;

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->msWrapMech = (CK_MECHANISM_TYPE)temp;

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 2, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (temp == 0 || temp > sizeof(parsedTicket->master_secret)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->ms_length = (PRUint16)temp;

    /* Read the master secret. */
    rv = ssl3_ExtConsumeHandshake(ss, parsedTicket->master_secret,
                                  parsedTicket->ms_length, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    /* Read client identity */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 1, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->client_auth_type = (ClientAuthenticationType)temp;
    switch (parsedTicket->client_auth_type) {
        case CLIENT_AUTH_ANONYMOUS:
            break;
        case CLIENT_AUTH_CERTIFICATE:
            rv = ssl3_ExtConsumeHandshakeVariable(ss, &parsedTicket->peer_cert, 2,
                                                  &buffer, &len);
            if (rv != SECSuccess) {
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                return SECFailure;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
    }
    /* Read timestamp. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->timestamp = temp;

    /* Read server name */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &parsedTicket->srvName, 2,
                                          &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* Read extendedMasterSecretUsed */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 1, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    PORT_Assert(temp == PR_TRUE || temp == PR_FALSE);
    parsedTicket->extendedMasterSecretUsed = (PRBool)temp;

    rv = ssl3_ExtConsumeHandshake(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->flags = PR_ntohl(temp);

    rv = ssl3_ExtConsumeHandshakeVariable(ss, &parsedTicket->alpnSelection, 1,
                                          &buffer, &len);
    PORT_Assert(parsedTicket->alpnSelection.len < 256);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->maxEarlyData = temp;

#ifndef UNSAFE_FUZZER_MODE
    /* Done parsing.  Check that all bytes have been consumed. */
    if (len != 0) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
#endif

    parsedTicket->valid = PR_TRUE;
    return SECSuccess;
}

static SECStatus
ssl_CreateSIDFromTicket(sslSocket *ss, const SECItem *rawTicket,
                        SessionTicket *parsedTicket, sslSessionID **out)
{
    sslSessionID *sid;
    SECStatus rv;

    sid = ssl3_NewSessionID(ss, PR_TRUE);
    if (sid == NULL) {
        return SECFailure;
    }

    /* Copy over parameters. */
    sid->version = parsedTicket->ssl_version;
    sid->u.ssl3.cipherSuite = parsedTicket->cipher_suite;
    sid->u.ssl3.compression = parsedTicket->compression_method;
    sid->authType = parsedTicket->authType;
    sid->authKeyBits = parsedTicket->authKeyBits;
    sid->keaType = parsedTicket->keaType;
    sid->keaKeyBits = parsedTicket->keaKeyBits;
    sid->namedCurve = parsedTicket->namedCurve;

    rv = SECITEM_CopyItem(NULL, &sid->u.ssl3.locked.sessionTicket.ticket,
                          rawTicket);
    if (rv != SECSuccess) {
        goto loser;
    }
    sid->u.ssl3.locked.sessionTicket.flags = parsedTicket->flags;
    sid->u.ssl3.locked.sessionTicket.max_early_data_size =
        parsedTicket->maxEarlyData;

    if (parsedTicket->ms_length >
        sizeof(sid->u.ssl3.keys.wrapped_master_secret)) {
        goto loser;
    }
    PORT_Memcpy(sid->u.ssl3.keys.wrapped_master_secret,
                parsedTicket->master_secret, parsedTicket->ms_length);
    sid->u.ssl3.keys.wrapped_master_secret_len = parsedTicket->ms_length;
    sid->u.ssl3.masterWrapMech = parsedTicket->msWrapMech;
    sid->u.ssl3.keys.msIsWrapped = parsedTicket->ms_is_wrapped;
    sid->u.ssl3.masterValid = PR_TRUE;
    sid->u.ssl3.keys.resumable = PR_TRUE;
    sid->u.ssl3.keys.extendedMasterSecretUsed = parsedTicket->extendedMasterSecretUsed;

    /* Copy over client cert from session ticket if there is one. */
    if (parsedTicket->peer_cert.data != NULL) {
        PORT_Assert(!sid->peerCert);
        sid->peerCert = CERT_NewTempCertificate(ss->dbHandle,
                                                &parsedTicket->peer_cert,
                                                NULL, PR_FALSE, PR_TRUE);
        if (!sid->peerCert) {
            goto loser;
        }
    }

    /* Transfer ownership of the remaining items. */
    if (parsedTicket->srvName.data != NULL) {
        SECITEM_FreeItem(&sid->u.ssl3.srvName, PR_FALSE);
        rv = SECITEM_CopyItem(NULL, &sid->u.ssl3.srvName,
                              &parsedTicket->srvName);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    if (parsedTicket->alpnSelection.data != NULL) {
        rv = SECITEM_CopyItem(NULL, &sid->u.ssl3.alpnSelection,
                              &parsedTicket->alpnSelection);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    *out = sid;
    return SECSuccess;

loser:
    ssl_FreeSID(sid);
    return SECFailure;
}

/* Generic ticket processing code, common to all TLS versions. */
SECStatus
ssl3_ProcessSessionTicketCommon(sslSocket *ss, SECItem *data)
{
    SECItem decryptedTicket = { siBuffer, NULL, 0 };
    SessionTicket parsedTicket;
    SECStatus rv;

    if (ss->sec.ci.sid != NULL) {
        ss->sec.uncache(ss->sec.ci.sid);
        ssl_FreeSID(ss->sec.ci.sid);
        ss->sec.ci.sid = NULL;
    }

    if (!SECITEM_AllocItem(NULL, &decryptedTicket, data->len)) {
        return SECFailure;
    }

    /* Decrypt the ticket. */
    rv = ssl_SelfEncryptUnprotect(ss, data->data, data->len,
                                  decryptedTicket.data,
                                  &decryptedTicket.len,
                                  decryptedTicket.len);
    if (rv != SECSuccess) {
        SECITEM_ZfreeItem(&decryptedTicket, PR_FALSE);

        /* Fail with no ticket if we're not a recipient. Otherwise
         * it's a hard failure. */
        if (PORT_GetError() != SEC_ERROR_NOT_A_RECIPIENT) {
            SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
            return SECFailure;
        }

        /* We didn't have the right key, so pretend we don't have a
         * ticket. */
    }

    rv = ssl_ParseSessionTicket(ss, &decryptedTicket, &parsedTicket);
    if (rv != SECSuccess) {
        SSL3Statistics *ssl3stats;

        SSL_DBG(("%d: SSL[%d]: Session ticket parsing failed.",
                 SSL_GETPID(), ss->fd));
        ssl3stats = SSL_GetStatistics();
        SSL_AtomicIncrementLong(&ssl3stats->hch_sid_ticket_parse_failures);
        goto loser; /* code already set */
    }

    /* Use the ticket if it is valid and unexpired. */
    if (parsedTicket.valid &&
        parsedTicket.timestamp + ssl_ticket_lifetime > ssl_Time()) {
        sslSessionID *sid;

        rv = ssl_CreateSIDFromTicket(ss, data, &parsedTicket, &sid);
        if (rv != SECSuccess) {
            goto loser; /* code already set */
        }
        ss->statelessResume = PR_TRUE;
        ss->sec.ci.sid = sid;
    }

    SECITEM_ZfreeItem(&decryptedTicket, PR_FALSE);
    PORT_Memset(&parsedTicket, 0, sizeof(parsedTicket));
    return SECSuccess;

loser:
    SECITEM_ZfreeItem(&decryptedTicket, PR_FALSE);
    PORT_Memset(&parsedTicket, 0, sizeof(parsedTicket));
    return SECFailure;
}

SECStatus
ssl3_ServerHandleSessionTicketXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                  SECItem *data)
{

    /* Ignore the SessionTicket extension if processing is disabled. */
    if (!ss->opt.enableSessionTickets) {
        return SECSuccess;
    }

    /* If we are doing TLS 1.3, then ignore this. */
    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;

    /* Parse the received ticket sent in by the client.  We are
     * lenient about some parse errors, falling back to a fullshake
     * instead of terminating the current connection.
     */
    if (data->len == 0) {
        xtnData->emptySessionTicket = PR_TRUE;
        return SECSuccess;
    }

    return ssl3_ProcessSessionTicketCommon(CONST_CAST(sslSocket, ss), data);
}

/* Extension format:
 * Extension number:   2 bytes
 * Extension length:   2 bytes
 * Verify Data Length: 1 byte
 * Verify Data (TLS): 12 bytes (client) or 24 bytes (server)
 * Verify Data (SSL): 36 bytes (client) or 72 bytes (server)
 */
PRInt32
ssl3_SendRenegotiationInfoXtn(
    const sslSocket *ss,
    TLSExtensionData *xtnData,
    PRBool append,
    PRUint32 maxBytes)
{
    PRInt32 len = 0;
    PRInt32 needed;

    /* In draft-ietf-tls-renegotiation-03, it is NOT RECOMMENDED to send
     * both the SCSV and the empty RI, so when we send SCSV in
     * the initial handshake, we don't also send RI.
     */
    if (!ss || ss->ssl3.hs.sendingSCSV)
        return 0;
    if (ss->firstHsDone) {
        len = ss->sec.isServer ? ss->ssl3.hs.finishedBytes * 2
                               : ss->ssl3.hs.finishedBytes;
    }
    needed = 5 + len;
    if (maxBytes < (PRUint32)needed) {
        return 0;
    }
    if (append) {
        SECStatus rv;
        /* extension_type */
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_renegotiation_info_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* length of extension_data */
        rv = ssl3_ExtAppendHandshakeNumber(ss, len + 1, 2);
        if (rv != SECSuccess)
            return -1;
        /* verify_Data from previous Finished message(s) */
        rv = ssl3_ExtAppendHandshakeVariable(ss,
                                             ss->ssl3.hs.finishedMsgs.data, len, 1);
        if (rv != SECSuccess)
            return -1;
        if (!ss->sec.isServer) {
            xtnData->advertised[xtnData->numAdvertised++] =
                ssl_renegotiation_info_xtn;
        }
    }
    return needed;
}

/* This function runs in both the client and server.  */
SECStatus
ssl3_HandleRenegotiationInfoXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv = SECSuccess;
    PRUint32 len = 0;

    if (ss->firstHsDone) {
        len = ss->sec.isServer ? ss->ssl3.hs.finishedBytes
                               : ss->ssl3.hs.finishedBytes * 2;
    }
    if (data->len != 1 + len || data->data[0] != len) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }
    if (len && NSS_SecureMemcmp(ss->ssl3.hs.finishedMsgs.data,
                                data->data + 1, len)) {
        ssl3_ExtSendAlert(ss, alert_fatal, handshake_failure);
        PORT_SetError(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
        return SECFailure;
    }
    /* remember that we got this extension and it was correct. */
    CONST_CAST(sslSocket, ss)
        ->peerRequestedProtection = 1;
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    if (ss->sec.isServer) {
        /* prepare to send back the appropriate response */
        rv = ssl3_RegisterExtensionSender(ss, xtnData, ex_type,
                                          ssl3_SendRenegotiationInfoXtn);
    }
    return rv;
}

PRInt32
ssl3_ClientSendUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append, PRUint32 maxBytes)
{
    PRUint32 ext_data_len;
    PRInt16 i;
    SECStatus rv;

    if (!ss)
        return 0;

    if (!IS_DTLS(ss) || !ss->ssl3.dtlsSRTPCipherCount)
        return 0; /* Not relevant */

    ext_data_len = 2 + 2 * ss->ssl3.dtlsSRTPCipherCount + 1;

    if (append && maxBytes >= 4 + ext_data_len) {
        /* Extension type */
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_use_srtp_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* Length of extension data */
        rv = ssl3_ExtAppendHandshakeNumber(ss, ext_data_len, 2);
        if (rv != SECSuccess)
            return -1;
        /* Length of the SRTP cipher list */
        rv = ssl3_ExtAppendHandshakeNumber(ss,
                                           2 * ss->ssl3.dtlsSRTPCipherCount,
                                           2);
        if (rv != SECSuccess)
            return -1;
        /* The SRTP ciphers */
        for (i = 0; i < ss->ssl3.dtlsSRTPCipherCount; i++) {
            rv = ssl3_ExtAppendHandshakeNumber(ss,
                                               ss->ssl3.dtlsSRTPCiphers[i],
                                               2);
            if (rv != SECSuccess)
                return -1;
        }
        /* Empty MKI value */
        ssl3_ExtAppendHandshakeVariable(ss, NULL, 0, 1);

        xtnData->advertised[xtnData->numAdvertised++] =
            ssl_use_srtp_xtn;
    }

    return 4 + ext_data_len;
}

PRInt32
ssl3_ServerSendUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append, PRUint32 maxBytes)
{
    SECStatus rv;

    /* Server side */
    if (!append || maxBytes < 9) {
        return 9;
    }

    /* Extension type */
    rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_use_srtp_xtn, 2);
    if (rv != SECSuccess)
        return -1;
    /* Length of extension data */
    rv = ssl3_ExtAppendHandshakeNumber(ss, 5, 2);
    if (rv != SECSuccess)
        return -1;
    /* Length of the SRTP cipher list */
    rv = ssl3_ExtAppendHandshakeNumber(ss, 2, 2);
    if (rv != SECSuccess)
        return -1;
    /* The selected cipher */
    rv = ssl3_ExtAppendHandshakeNumber(ss, xtnData->dtlsSRTPCipherSuite, 2);
    if (rv != SECSuccess)
        return -1;
    /* Empty MKI value */
    ssl3_ExtAppendHandshakeVariable(ss, NULL, 0, 1);

    return 9;
}

SECStatus
ssl3_ClientHandleUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    SECItem ciphers = { siBuffer, NULL, 0 };
    PRUint16 i;
    PRUint16 cipher = 0;
    PRBool found = PR_FALSE;
    SECItem litem;

    if (!data->data || !data->len) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }

    /* Get the cipher list */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &ciphers, 2,
                                          &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure; /* fatal alert already sent */
    }
    /* Now check that the server has picked just 1 (i.e., len = 2) */
    if (ciphers.len != 2) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }

    /* Get the selected cipher */
    cipher = (ciphers.data[0] << 8) | ciphers.data[1];

    /* Now check that this is one of the ciphers we offered */
    for (i = 0; i < ss->ssl3.dtlsSRTPCipherCount; i++) {
        if (cipher == ss->ssl3.dtlsSRTPCiphers[i]) {
            found = PR_TRUE;
            break;
        }
    }

    if (!found) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
        return SECFailure;
    }

    /* Get the srtp_mki value */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &litem, 1,
                                          &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure; /* alert already sent */
    }

    /* We didn't offer an MKI, so this must be 0 length */
    if (litem.len != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
        return SECFailure;
    }

    /* extra trailing bytes */
    if (data->len != 0) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }

    /* OK, this looks fine. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_use_srtp_xtn;
    xtnData->dtlsSRTPCipherSuite = cipher;
    return SECSuccess;
}

SECStatus
ssl3_ServerHandleUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    SECItem ciphers = { siBuffer, NULL, 0 };
    PRUint16 i;
    unsigned int j;
    PRUint16 cipher = 0;
    PRBool found = PR_FALSE;
    SECItem litem;

    if (!IS_DTLS(ss) || !ss->ssl3.dtlsSRTPCipherCount) {
        /* Ignore the extension if we aren't doing DTLS or no DTLS-SRTP
         * preferences have been set. */
        return SECSuccess;
    }

    if (!data->data || data->len < 5) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }

    /* Get the cipher list */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &ciphers, 2,
                                          &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure; /* alert already sent */
    }
    /* Check that the list is even length */
    if (ciphers.len % 2) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }

    /* Walk through the offered list and pick the most preferred of our
     * ciphers, if any */
    for (i = 0; !found && i < ss->ssl3.dtlsSRTPCipherCount; i++) {
        for (j = 0; j + 1 < ciphers.len; j += 2) {
            cipher = (ciphers.data[j] << 8) | ciphers.data[j + 1];
            if (cipher == ss->ssl3.dtlsSRTPCiphers[i]) {
                found = PR_TRUE;
                break;
            }
        }
    }

    /* Get the srtp_mki value */
    rv = ssl3_ExtConsumeHandshakeVariable(ss, &litem, 1, &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (data->len != 0) {
        ssl3_ExtDecodeError(ss); /* trailing bytes */
        return SECFailure;
    }

    /* Now figure out what to do */
    if (!found) {
        /* No matching ciphers, pretend we don't support use_srtp */
        return SECSuccess;
    }

    /* OK, we have a valid cipher and we've selected it */
    xtnData->dtlsSRTPCipherSuite = cipher;
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_use_srtp_xtn;

    return ssl3_RegisterExtensionSender(ss, xtnData,
                                        ssl_use_srtp_xtn,
                                        ssl3_ServerSendUseSRTPXtn);
}

/* ssl3_ServerHandleSigAlgsXtn handles the signature_algorithms extension
 * from a client.
 * See https://tools.ietf.org/html/rfc5246#section-7.4.1.4.1 */
SECStatus
ssl3_ServerHandleSigAlgsXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;

    /* Ignore this extension if we aren't doing TLS 1.2 or greater. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_2) {
        return SECSuccess;
    }

    if (xtnData->clientSigSchemes) {
        PORT_Free(xtnData->clientSigSchemes);
        xtnData->clientSigSchemes = NULL;
    }
    rv = ssl_ParseSignatureSchemes(ss, NULL,
                                   &xtnData->clientSigSchemes,
                                   &xtnData->numClientSigScheme,
                                   &data->data, &data->len);
    if (rv != SECSuccess || xtnData->numClientSigScheme == 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }
    /* Check for trailing data. */
    if (data->len != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    return SECSuccess;
}

/* ssl3_ClientSendSigAlgsXtn sends the signature_algorithm extension for TLS
 * 1.2 ClientHellos. */
PRInt32
ssl3_ClientSendSigAlgsXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_length;
    PRUint8 buf[MAX_SIGNATURE_SCHEMES * 2];
    PRUint32 len;
    SECStatus rv;

    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_2) {
        return 0;
    }

    rv = ssl3_EncodeSigAlgs(ss, buf, sizeof(buf), &len);
    if (rv != SECSuccess) {
        return -1;
    }

    extension_length =
        2 /* extension type */ +
        2 /* extension length */ +
        2 /* supported_signature_algorithms length */ +
        len;

    if (maxBytes < extension_length) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        SECStatus rv;
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_signature_algorithms_xtn, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_ExtAppendHandshakeNumber(ss, len + 2, 2);
        if (rv != SECSuccess) {
            return -1;
        }

        rv = ssl3_ExtAppendHandshakeVariable(ss, buf, len, 2);
        if (rv != SECSuccess) {
            return -1;
        }

        xtnData->advertised[xtnData->numAdvertised++] =
            ssl_signature_algorithms_xtn;
    }

    return extension_length;
}

/* Takes the size of the ClientHello, less the record header, and determines how
 * much padding is required. */
void
ssl3_CalculatePaddingExtLen(sslSocket *ss,
                            unsigned int clientHelloLength)
{
    unsigned int recordLength = 1 /* handshake message type */ +
                                3 /* handshake message length */ +
                                clientHelloLength;
    unsigned int extensionLen;

    /* Don't pad for DTLS, for SSLv3, or for renegotiation. */
    if (IS_DTLS(ss) ||
        ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_0 ||
        ss->firstHsDone) {
        return;
    }

    /* A padding extension may be included to ensure that the record containing
     * the ClientHello doesn't have a length between 256 and 511 bytes
     * (inclusive). Initial ClientHello records with such lengths trigger bugs
     * in F5 devices. */
    if (recordLength < 256 || recordLength >= 512) {
        return;
    }

    extensionLen = 512 - recordLength;
    /* Extensions take at least four bytes to encode. Always include at least
     * one byte of data if we are padding. Some servers will time out or
     * terminate the connection if the last ClientHello extension is empty. */
    if (extensionLen < 4 + 1) {
        extensionLen = 4 + 1;
    }

    ss->xtnData.paddingLen = extensionLen - 4;
}

/* ssl3_SendPaddingExtension possibly adds an extension which ensures that a
 * ClientHello record is either < 256 bytes or is >= 512 bytes. This ensures
 * that we don't trigger bugs in F5 products. */
PRInt32
ssl3_ClientSendPaddingExtension(const sslSocket *ss, TLSExtensionData *xtnData,
                                PRBool append, PRUint32 maxBytes)
{
    static unsigned char padding[252] = { 0 };
    unsigned int extensionLen;
    SECStatus rv;

    /* On the length-calculation pass, report zero total length.  The record
     * will be larger on the second pass if needed. */
    if (!append || !xtnData->paddingLen) {
        return 0;
    }

    extensionLen = xtnData->paddingLen + 4;
    if (extensionLen > maxBytes ||
        xtnData->paddingLen > sizeof(padding)) {
        PORT_Assert(0);
        return -1;
    }

    rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_padding_xtn, 2);
    if (rv != SECSuccess) {
        return -1;
    }
    rv = ssl3_ExtAppendHandshakeVariable(ss, padding, xtnData->paddingLen, 2);
    if (rv != SECSuccess) {
        return -1;
    }

    return extensionLen;
}

PRInt32
ssl3_SendExtendedMasterSecretXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                 PRUint32 maxBytes)
{
    PRInt32 extension_length;

    if (!ss->opt.enableExtendedMS) {
        return 0;
    }

    /* Always send the extension in this function, since the
     * client always sends it and this function is only called on
     * the server if we negotiated the extension. */
    extension_length = 4; /* Type + length (0) */
    if (maxBytes < extension_length) {
        PORT_Assert(0);
        return 0;
    }

    if (append) {
        SECStatus rv;
        rv = ssl3_ExtAppendHandshakeNumber(ss, ssl_extended_master_secret_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            goto loser;
        xtnData->advertised[xtnData->numAdvertised++] =
            ssl_extended_master_secret_xtn;
    }

    return extension_length;

loser:
    return -1;
}

SECStatus
ssl3_HandleExtendedMasterSecretXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                   SECItem *data)
{
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_0) {
        return SECSuccess;
    }

    if (!ss->opt.enableExtendedMS) {
        return SECSuccess;
    }

    if (data->len != 0) {
        SSL_TRC(30, ("%d: SSL3[%d]: Bogus extended master secret extension",
                     SSL_GETPID(), ss->fd));
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        return SECFailure;
    }

    SSL_DBG(("%d: SSL[%d]: Negotiated extended master secret extension.",
             SSL_GETPID(), ss->fd));

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;

    if (ss->sec.isServer) {
        return ssl3_RegisterExtensionSender(
            ss, xtnData, ex_type, ssl3_SendExtendedMasterSecretXtn);
    }
    return SECSuccess;
}

/* ssl3_ClientSendSignedCertTimestampXtn sends the signed_certificate_timestamp
 * extension for TLS ClientHellos. */
PRInt32
ssl3_ClientSendSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRBool append,
                                      PRUint32 maxBytes)
{
    PRInt32 extension_length = 2 /* extension_type */ +
                               2 /* length(extension_data) */;

    /* Only send the extension if processing is enabled. */
    if (!ss->opt.enableSignedCertTimestamps)
        return 0;

    if (append && maxBytes >= extension_length) {
        SECStatus rv;
        /* extension_type */
        rv = ssl3_ExtAppendHandshakeNumber(ss,
                                           ssl_signed_cert_timestamp_xtn,
                                           2);
        if (rv != SECSuccess)
            goto loser;
        /* zero length */
        rv = ssl3_ExtAppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            goto loser;
        xtnData->advertised[xtnData->numAdvertised++] =
            ssl_signed_cert_timestamp_xtn;
    } else if (maxBytes < extension_length) {
        PORT_Assert(0);
        return 0;
    }

    return extension_length;
loser:
    return -1;
}

SECStatus
ssl3_ClientHandleSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData, PRUint16 ex_type,
                                        SECItem *data)
{
    /* We do not yet know whether we'll be resuming a session or creating
     * a new one, so we keep a pointer to the data in the TLSExtensionData
     * structure. This pointer is only valid in the scope of
     * ssl3_HandleServerHello, and, if not resuming a session, the data is
     * copied once a new session structure has been set up.
     * All parsing is currently left to the application and we accept
     * everything, including empty data.
     */
    SECItem *scts = &xtnData->signedCertTimestamps;
    PORT_Assert(!scts->data && !scts->len);

    if (!data->len) {
        /* Empty extension data: RFC 6962 mandates non-empty contents. */
        return SECFailure;
    }
    *scts = *data;
    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    return SECSuccess;
}

PRInt32
ssl3_ServerSendSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                      PRBool append,
                                      PRUint32 maxBytes)
{
    PRInt32 extension_length;
    const SECItem *scts = &ss->sec.serverCert->signedCertTimestamps;

    if (!scts->len) {
        /* No timestamps to send */
        return 0;
    }

    extension_length = 2 /* extension_type */ +
                       2 /* length(extension_data) */ +
                       scts->len;

    if (maxBytes < extension_length) {
        PORT_Assert(0);
        return 0;
    }
    if (append) {
        SECStatus rv;
        /* extension_type */
        rv = ssl3_ExtAppendHandshakeNumber(ss,
                                           ssl_signed_cert_timestamp_xtn,
                                           2);
        if (rv != SECSuccess) {
            return -1;
        }
        /* extension_data */
        rv = ssl3_ExtAppendHandshakeVariable(ss, scts->data, scts->len, 2);
        if (rv != SECSuccess) {
            return -1;
        }
    }

    return extension_length;
}

SECStatus
ssl3_ServerHandleSignedCertTimestampXtn(const sslSocket *ss,
                                        TLSExtensionData *xtnData,
                                        PRUint16 ex_type,
                                        SECItem *data)
{
    if (data->len != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;
    PORT_Assert(ss->sec.isServer);
    return ssl3_RegisterExtensionSender(
        ss, xtnData, ex_type, ssl3_ServerSendSignedCertTimestampXtn);
}

/* Just make sure that the remote client supports uncompressed points,
 * Since that is all we support.  Disable ECC cipher suites if it doesn't.
 */
SECStatus
ssl3_HandleSupportedPointFormatsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                    PRUint16 ex_type,
                                    SECItem *data)
{
    int i;

    if (data->len < 2 || data->len > 255 || !data->data ||
        data->len != (unsigned int)data->data[0] + 1) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }
    for (i = data->len; --i > 0;) {
        if (data->data[i] == 0) {
            /* indicate that we should send a reply */
            SECStatus rv;
            rv = ssl3_RegisterExtensionSender(ss, xtnData, ex_type,
                                              &ssl3_SendSupportedPointFormatsXtn);
            return rv;
        }
    }

    /* Poor client doesn't support uncompressed points. */
    PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
    return SECFailure;
}

static SECStatus
ssl_UpdateSupportedGroups(sslSocket *ss, SECItem *data)
{
    SECStatus rv;
    PRUint32 list_len;
    unsigned int i;
    const sslNamedGroupDef *enabled[SSL_NAMED_GROUP_COUNT] = { 0 };
    PORT_Assert(SSL_NAMED_GROUP_COUNT == PR_ARRAY_SIZE(enabled));

    if (!data->data || data->len < 4) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* get the length of elliptic_curve_list */
    rv = ssl3_ConsumeHandshakeNumber(ss, &list_len, 2, &data->data, &data->len);
    if (rv != SECSuccess || data->len != list_len || (data->len % 2) != 0) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* disable all groups and remember the enabled groups */
    for (i = 0; i < SSL_NAMED_GROUP_COUNT; ++i) {
        enabled[i] = ss->namedGroupPreferences[i];
        ss->namedGroupPreferences[i] = NULL;
    }

    /* Read groups from data and enable if in |enabled| */
    while (data->len) {
        const sslNamedGroupDef *group;
        PRUint32 curve_name;
        rv = ssl3_ConsumeHandshakeNumber(ss, &curve_name, 2, &data->data,
                                         &data->len);
        if (rv != SECSuccess) {
            return SECFailure; /* fatal alert already sent */
        }
        group = ssl_LookupNamedGroup(curve_name);
        if (group) {
            for (i = 0; i < SSL_NAMED_GROUP_COUNT; ++i) {
                if (enabled[i] && group == enabled[i]) {
                    ss->namedGroupPreferences[i] = enabled[i];
                    break;
                }
            }
        }

        /* "Codepoints in the NamedCurve registry with a high byte of 0x01 (that
         * is, between 256 and 511 inclusive) are set aside for FFDHE groups,"
         * -- https://tools.ietf.org/html/draft-ietf-tls-negotiated-ff-dhe-10
         */
        if ((curve_name & 0xff00) == 0x0100) {
            ss->xtnData.peerSupportsFfdheGroups = PR_TRUE;
        }
    }

    /* Note: if ss->opt.requireDHENamedGroups is set, we disable DHE cipher
     * suites, but we do that in ssl3_config_match(). */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3 &&
        !ss->opt.requireDHENamedGroups && !ss->xtnData.peerSupportsFfdheGroups) {
        /* If we don't require that DHE use named groups, and no FFDHE was
         * included, we pretend that they support all the FFDHE groups we do. */
        for (i = 0; i < SSL_NAMED_GROUP_COUNT; ++i) {
            if (enabled[i] && enabled[i]->keaType == ssl_kea_dh) {
                ss->namedGroupPreferences[i] = enabled[i];
            }
        }
    }

    return SECSuccess;
}

/* Ensure that the curve in our server cert is one of the ones supported
 * by the remote client, and disable all ECC cipher suites if not.
 */
SECStatus
ssl_HandleSupportedGroupsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;

    rv = ssl_UpdateSupportedGroups(CONST_CAST(sslSocket, ss), data);
    if (rv != SECSuccess)
        return SECFailure;

    /* TLS 1.3 permits the server to send this extension so make it so. */
    if (ss->sec.isServer && ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = ssl3_RegisterExtensionSender(ss, xtnData, ex_type,
                                          &ssl_SendSupportedGroupsXtn);
        if (rv != SECSuccess) {
            return SECFailure; /* error already set. */
        }
    }

    /* Remember that we negotiated this extension. */
    xtnData->negotiated[xtnData->numNegotiated++] = ex_type;

    return SECSuccess;
}
