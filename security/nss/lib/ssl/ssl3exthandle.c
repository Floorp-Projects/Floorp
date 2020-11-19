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
#include "ssl3ext.h"
#include "ssl3exthandle.h"
#include "tls13ech.h"
#include "tls13exthandle.h" /* For tls13_ServerSendStatusRequestXtn. */

PRBool
ssl_ShouldSendSNIExtension(const sslSocket *ss, const char *url)
{
    PRNetAddr netAddr;

    /* must have a hostname */
    if (!url || !url[0]) {
        return PR_FALSE;
    }
    /* must not be an IPv4 or IPv6 address */
    if (PR_SUCCESS == PR_StringToNetAddr(url, &netAddr)) {
        /* is an IP address (v4 or v6) */
        return PR_FALSE;
    }

    return PR_TRUE;
}

/* Format an SNI extension, using the name from the socket's URL,
 * unless that name is a dotted decimal string.
 * Used by client and server.
 */
SECStatus
ssl3_ClientFormatServerNameXtn(const sslSocket *ss, const char *url,
                               unsigned int len, TLSExtensionData *xtnData,
                               sslBuffer *buf)
{
    SECStatus rv;

    /* length of server_name_list */
    rv = sslBuffer_AppendNumber(buf, len + 3, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* Name Type (sni_host_name) */
    rv = sslBuffer_AppendNumber(buf, 0, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* HostName (length and value) */
    rv = sslBuffer_AppendVariable(buf, (const PRUint8 *)url, len, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

SECStatus
ssl3_ClientSendServerNameXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    const char *url = ss->url;

    if (!ssl_ShouldSendSNIExtension(ss, url)) {
        return SECSuccess;
    }

    /* If ECH, write the public name. The real server name
     * is emplaced while constructing CHInner extensions. */
    sslEchConfig *cfg = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);
    const char *sniContents = PR_CLIST_IS_EMPTY(&ss->echConfigs) ? url : cfg->contents.publicName;
    rv = ssl3_ClientFormatServerNameXtn(ss, sniContents, strlen(sniContents), xtnData, buf);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_HandleServerNameXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                         SECItem *data)
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
ssl3_ClientSendSessionTicketXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                sslBuffer *buf, PRBool *added)
{
    NewSessionTicket *session_ticket = NULL;
    sslSessionID *sid = ss->sec.ci.sid;
    SECStatus rv;

    PORT_Assert(!ss->sec.isServer);

    /* Never send an extension with a ticket for TLS 1.3, but
     * OK to send the empty one in case the server does 1.2. */
    if ((sid->cached == in_client_cache || sid->cached == in_external_cache) &&
        sid->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* Ignore the SessionTicket extension if processing is disabled. */
    if (!ss->opt.enableSessionTickets) {
        return SECSuccess;
    }

    /* Send a session ticket if one is available.
     *
     * The caller must be holding sid->u.ssl3.lock for reading. We cannot
     * just acquire and release the lock within this function because the
     * caller will call this function twice, and we need the inputs to be
     * consistent between the two calls. Note that currently the caller
     * will only be holding the lock when we are the client and when we're
     * attempting to resume an existing session.
     */
    session_ticket = &sid->u.ssl3.locked.sessionTicket;
    if (session_ticket->ticket.data &&
        (xtnData->ticketTimestampVerified ||
         ssl_TicketTimeValid(ss, session_ticket))) {

        xtnData->ticketTimestampVerified = PR_FALSE;

        rv = sslBuffer_Append(buf, session_ticket->ticket.data,
                              session_ticket->ticket.len);
        if (rv != SECSuccess) {
            return SECFailure;
        }

        xtnData->sentSessionTicketInClientHello = PR_TRUE;
    }

    *added = PR_TRUE;
    return SECSuccess;
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

/* ssl3_ValidateAppProtocol checks that the given block of data is valid: none
 * of the lengths may be 0 and the sum of the lengths must equal the length of
 * the block. */
SECStatus
ssl3_ValidateAppProtocol(const unsigned char *data, unsigned int length)
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

/* Protocol selection handler for ALPN. */
static SECStatus
ssl3_SelectAppProtocol(const sslSocket *ss, TLSExtensionData *xtnData,
                       PRUint16 extension, SECItem *data)
{
    SECStatus rv;
    unsigned char resultBuffer[255];
    SECItem result = { siBuffer, resultBuffer, 0 };

    rv = ssl3_ValidateAppProtocol(data->data, data->len);
    if (rv != SECSuccess) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return rv;
    }

    PORT_Assert(ss->nextProtoCallback);
    /* Neither the cipher suite nor ECH are selected yet Note that extensions
     * sometimes affect what cipher suite is selected, e.g., for ECC. */
    PORT_Assert((ss->ssl3.hs.preliminaryInfo &
                 ssl_preinfo_all & ~ssl_preinfo_cipher_suite & ~ssl_preinfo_ech) ==
                (ssl_preinfo_all & ~ssl_preinfo_cipher_suite & ~ssl_preinfo_ech));
    /* The callback has to make sure that either rv != SECSuccess or that result
     * is not set if there is no common protocol. */
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
        PORT_Assert(PR_FALSE);
        return SECFailure;
    }

    SECITEM_FreeItem(&xtnData->nextProto, PR_FALSE);

    if (result.len < 1 || !result.data) {
        /* Check that we actually got a result. */
        ssl3_ExtSendAlert(ss, alert_fatal, no_application_protocol);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_NO_PROTOCOL);
        return SECFailure;
    }

    xtnData->nextProtoState = SSL_NEXT_PROTO_NEGOTIATED;
    xtnData->negotiated[xtnData->numNegotiated++] = extension;
    return SECITEM_CopyItem(NULL, &xtnData->nextProto, &result);
}

/* handle an incoming ALPN extension at the server */
SECStatus
ssl3_ServerHandleAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             SECItem *data)
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

    /* ALPN has extra redundant length information so that
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

    rv = ssl3_SelectAppProtocol(ss, xtnData, ssl_app_layer_protocol_xtn, data);
    if (rv != SECSuccess) {
        return rv;
    }

    /* prepare to send back a response, if we negotiated */
    if (xtnData->nextProtoState == SSL_NEXT_PROTO_NEGOTIATED) {
        rv = ssl3_RegisterExtensionSender(ss, xtnData,
                                          ssl_app_layer_protocol_xtn,
                                          ssl3_ServerSendAppProtoXtn);
        if (rv != SECSuccess) {
            ssl3_ExtSendAlert(ss, alert_fatal, internal_error);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return rv;
        }
    }
    return SECSuccess;
}

SECStatus
ssl3_ClientHandleAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             SECItem *data)
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
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_app_layer_protocol_xtn;
    return SECITEM_CopyItem(NULL, &xtnData->nextProto, &protocol_name);
}

SECStatus
ssl3_ClientSendAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                           sslBuffer *buf, PRBool *added)
{
    SECStatus rv;
    const unsigned int len = ss->opt.nextProtoNego.len;

    /* Renegotiations do not send this extension. */
    if (!ss->opt.enableALPN || !ss->opt.nextProtoNego.data || ss->firstHsDone) {
        return SECSuccess;
    }

    if (len > 0) {
        /* Each protocol string is prefixed with a single byte length. */
        rv = sslBuffer_AppendNumber(buf, len, 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        rv = sslBuffer_Append(buf, ss->opt.nextProtoNego.data, len);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_ServerSendAppProtoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                           sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    /* We're in over our heads if any of these fail */
    PORT_Assert(ss->opt.enableALPN);
    PORT_Assert(xtnData->nextProto.data);
    PORT_Assert(xtnData->nextProto.len > 0);
    PORT_Assert(xtnData->nextProtoState == SSL_NEXT_PROTO_NEGOTIATED);
    PORT_Assert(!ss->firstHsDone);

    rv = sslBuffer_AppendNumber(buf, xtnData->nextProto.len + 1, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_AppendVariable(buf, xtnData->nextProto.data,
                                  xtnData->nextProto.len, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_ServerHandleStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                  SECItem *data)
{
    sslExtensionBuilderFunc sender;

    PORT_Assert(ss->sec.isServer);

    /* remember that we got this extension. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_cert_status_xtn;

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        sender = tls13_ServerSendStatusRequestXtn;
    } else {
        sender = ssl3_ServerSendStatusRequestXtn;
    }
    return ssl3_RegisterExtensionSender(ss, xtnData, ssl_cert_status_xtn, sender);
}

SECStatus
ssl3_ServerSendStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                sslBuffer *buf, PRBool *added)
{
    const sslServerCert *serverCert = ss->sec.serverCert;

    if (!serverCert->certStatusArray ||
        !serverCert->certStatusArray->len) {
        return SECSuccess;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

/* ssl3_ClientSendStatusRequestXtn builds the status_request extension on the
 * client side. See RFC 6066 section 8. */
SECStatus
ssl3_ClientSendStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    if (!ss->opt.enableOCSPStapling) {
        return SECSuccess;
    }

    rv = sslBuffer_AppendNumber(buf, 1 /* status_type ocsp */, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* A zero length responder_id_list means that the responders are
     * implicitly known to the server. */
    rv = sslBuffer_AppendNumber(buf, 0, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* A zero length request_extensions means that there are no extensions.
     * Specifically, we don't set the id-pkix-ocsp-nonce extension. This
     * means that the server can replay a cached OCSP response to us. */
    rv = sslBuffer_AppendNumber(buf, 0, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_ClientHandleStatusRequestXtn(const sslSocket *ss, TLSExtensionData *xtnData,
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
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_cert_status_xtn;
    return SECSuccess;
}

#define TLS_EX_SESS_TICKET_VERSION (0x010a)

/*
 * Called from ssl3_SendNewSessionTicket, tls13_SendNewSessionTicket
 */
SECStatus
ssl3_EncodeSessionTicket(sslSocket *ss, const NewSessionTicket *ticket,
                         const PRUint8 *appToken, unsigned int appTokenLen,
                         PK11SymKey *secret, SECItem *ticket_data)
{
    SECStatus rv;
    sslBuffer plaintext = SSL_BUFFER_EMPTY;
    SECItem ticket_buf = { 0, NULL, 0 };
    sslSessionID sid;
    unsigned char wrapped_ms[SSL3_MASTER_SECRET_LENGTH];
    SECItem ms_item = { 0, NULL, 0 };
    PRTime now;
    SECItem *srvName = NULL;
    CK_MECHANISM_TYPE msWrapMech;
    SECItem *alpnSelection = NULL;
    PRUint32 ticketAgeBaseline;

    SSL_TRC(3, ("%d: SSL3[%d]: send session_ticket handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    /* Extract the master secret wrapped. */

    PORT_Memset(&sid, 0, sizeof(sslSessionID));

    PORT_Assert(secret);
    rv = ssl3_CacheWrappedSecret(ss, &sid, secret);
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
    /* Prep to send negotiated name */
    srvName = &ss->sec.ci.sid->u.ssl3.srvName;

    /* ticket version */
    rv = sslBuffer_AppendNumber(&plaintext, TLS_EX_SESS_TICKET_VERSION,
                                sizeof(PRUint16));
    if (rv != SECSuccess)
        goto loser;

    /* ssl_version */
    rv = sslBuffer_AppendNumber(&plaintext, ss->version,
                                sizeof(SSL3ProtocolVersion));
    if (rv != SECSuccess)
        goto loser;

    /* ciphersuite */
    rv = sslBuffer_AppendNumber(&plaintext, ss->ssl3.hs.cipher_suite,
                                sizeof(ssl3CipherSuite));
    if (rv != SECSuccess)
        goto loser;

    /* cipher spec parameters */
    rv = sslBuffer_AppendNumber(&plaintext, ss->sec.authType, 1);
    if (rv != SECSuccess)
        goto loser;
    rv = sslBuffer_AppendNumber(&plaintext, ss->sec.authKeyBits, 4);
    if (rv != SECSuccess)
        goto loser;
    rv = sslBuffer_AppendNumber(&plaintext, ss->sec.keaType, 1);
    if (rv != SECSuccess)
        goto loser;
    rv = sslBuffer_AppendNumber(&plaintext, ss->sec.keaKeyBits, 4);
    if (rv != SECSuccess)
        goto loser;
    if (ss->sec.keaGroup) {
        rv = sslBuffer_AppendNumber(&plaintext, ss->sec.keaGroup->name, 4);
        if (rv != SECSuccess)
            goto loser;
    } else {
        /* No kea group. Write 0 as invalid value. */
        rv = sslBuffer_AppendNumber(&plaintext, 0, 4);
        if (rv != SECSuccess)
            goto loser;
    }
    rv = sslBuffer_AppendNumber(&plaintext, ss->sec.signatureScheme, 4);
    if (rv != SECSuccess)
        goto loser;

    /* certificate type */
    PORT_Assert(SSL_CERT_IS(ss->sec.serverCert, ss->sec.authType));
    if (SSL_CERT_IS_EC(ss->sec.serverCert)) {
        const sslServerCert *cert = ss->sec.serverCert;
        PORT_Assert(cert->namedCurve);
        /* EC curves only use the second of the two bytes. */
        PORT_Assert(cert->namedCurve->name < 256);
        rv = sslBuffer_AppendNumber(&plaintext, cert->namedCurve->name, 1);
    } else {
        rv = sslBuffer_AppendNumber(&plaintext, 0, 1);
    }
    if (rv != SECSuccess)
        goto loser;

    /* master_secret */
    rv = sslBuffer_AppendNumber(&plaintext, msWrapMech, 4);
    if (rv != SECSuccess)
        goto loser;
    rv = sslBuffer_AppendVariable(&plaintext, ms_item.data, ms_item.len, 2);
    if (rv != SECSuccess)
        goto loser;

    /* client identity */
    if (ss->opt.requestCertificate && ss->sec.ci.sid->peerCert) {
        rv = sslBuffer_AppendNumber(&plaintext, CLIENT_AUTH_CERTIFICATE, 1);
        if (rv != SECSuccess)
            goto loser;
        rv = sslBuffer_AppendVariable(&plaintext,
                                      ss->sec.ci.sid->peerCert->derCert.data,
                                      ss->sec.ci.sid->peerCert->derCert.len, 2);
        if (rv != SECSuccess)
            goto loser;
    } else {
        rv = sslBuffer_AppendNumber(&plaintext, 0, 1);
        if (rv != SECSuccess)
            goto loser;
    }

    /* timestamp */
    now = ssl_Time(ss);
    PORT_Assert(sizeof(now) == 8);
    rv = sslBuffer_AppendNumber(&plaintext, now, 8);
    if (rv != SECSuccess)
        goto loser;

    /* HostName (length and value) */
    rv = sslBuffer_AppendVariable(&plaintext, srvName->data, srvName->len, 2);
    if (rv != SECSuccess)
        goto loser;

    /* extendedMasterSecretUsed */
    rv = sslBuffer_AppendNumber(
        &plaintext, ss->sec.ci.sid->u.ssl3.keys.extendedMasterSecretUsed, 1);
    if (rv != SECSuccess)
        goto loser;

    /* Flags */
    rv = sslBuffer_AppendNumber(&plaintext, ticket->flags,
                                sizeof(ticket->flags));
    if (rv != SECSuccess)
        goto loser;

    /* ALPN value. */
    PORT_Assert(ss->xtnData.nextProtoState == SSL_NEXT_PROTO_SELECTED ||
                ss->xtnData.nextProtoState == SSL_NEXT_PROTO_NEGOTIATED ||
                ss->xtnData.nextProto.len == 0);
    alpnSelection = &ss->xtnData.nextProto;
    PORT_Assert(alpnSelection->len < 256);
    rv = sslBuffer_AppendVariable(&plaintext, alpnSelection->data,
                                  alpnSelection->len, 1);
    if (rv != SECSuccess)
        goto loser;

    rv = sslBuffer_AppendNumber(&plaintext, ss->opt.maxEarlyDataSize, 4);
    if (rv != SECSuccess)
        goto loser;

    /*
     * We store this in the ticket:
     *    ticket_age_baseline = 1rtt - ticket_age_add
     *
     * When the client resumes, it will provide:
     *    obfuscated_age = ticket_age_client + ticket_age_add
     *
     * We expect to receive the ticket at:
     *    ticket_create + 1rtt + ticket_age_server
     *
     * We calculate the client's estimate of this as:
     *    ticket_create + ticket_age_baseline + obfuscated_age
     *    = ticket_create + 1rtt + ticket_age_client
     *
     * This is compared to the expected time, which should differ only as a
     * result of clock errors or errors in the RTT estimate.
     */
    ticketAgeBaseline = ss->ssl3.hs.rttEstimate / PR_USEC_PER_MSEC;
    ticketAgeBaseline -= ticket->ticket_age_add;
    rv = sslBuffer_AppendNumber(&plaintext, ticketAgeBaseline, 4);
    if (rv != SECSuccess)
        goto loser;

    /* Application token */
    rv = sslBuffer_AppendVariable(&plaintext, appToken, appTokenLen, 2);
    if (rv != SECSuccess)
        goto loser;

    /* This really only happens if appTokenLen is too much, and that always
     * comes from the using application. */
    if (SSL_BUFFER_LEN(&plaintext) > 0xffff) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    ticket_buf.len = ssl_SelfEncryptGetProtectedSize(SSL_BUFFER_LEN(&plaintext));
    PORT_Assert(ticket_buf.len > 0);
    if (SECITEM_AllocItem(NULL, &ticket_buf, ticket_buf.len) == NULL) {
        goto loser;
    }

    /* Finally, encrypt the ticket. */
    rv = ssl_SelfEncryptProtect(ss, SSL_BUFFER_BASE(&plaintext),
                                SSL_BUFFER_LEN(&plaintext),
                                ticket_buf.data, &ticket_buf.len, ticket_buf.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Give ownership of memory to caller. */
    *ticket_data = ticket_buf;

    sslBuffer_Clear(&plaintext);
    return SECSuccess;

loser:
    sslBuffer_Clear(&plaintext);
    if (ticket_buf.data) {
        SECITEM_FreeItem(&ticket_buf, PR_FALSE);
    }

    return SECFailure;
}

/* When a client receives a SessionTicket extension a NewSessionTicket
 * message is expected during the handshake.
 */
SECStatus
ssl3_ClientHandleSessionTicketXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                  SECItem *data)
{
    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

    if (data->len != 0) {
        return SECSuccess; /* Ignore the extension. */
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_session_ticket_xtn;
    return SECSuccess;
}

PR_STATIC_ASSERT((TLS_EX_SESS_TICKET_VERSION >> 8) == 1);

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

    /* All ticket versions start with 0x01, so check to see if this
     * is a ticket or some other self-encrypted thing. */
    if ((temp >> 8) != 1) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
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
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->originalKeaGroup = temp;
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->signatureScheme = (SSLSignatureScheme)temp;

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

    /* Read timestamp.  This is a 64-bit value and
     * ssl3_ExtConsumeHandshakeNumber only reads 32-bits at a time. */
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* Cast to avoid undefined behavior if the top bit is set. */
    parsedTicket->timestamp = (PRTime)((PRUint64)temp << 32);
    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->timestamp |= (PRTime)temp;

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
#ifndef UNSAFE_FUZZER_MODE
    /* A well-behaving server should only write 0 or 1. */
    PORT_Assert(temp == PR_TRUE || temp == PR_FALSE);
#endif
    parsedTicket->extendedMasterSecretUsed = temp ? PR_TRUE : PR_FALSE;

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

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &temp, 4, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    parsedTicket->ticketAgeBaseline = temp;

    rv = ssl3_ExtConsumeHandshakeVariable(ss, &parsedTicket->applicationToken,
                                          2, &buffer, &len);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

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
    sid->creationTime = parsedTicket->timestamp;
    sid->u.ssl3.cipherSuite = parsedTicket->cipher_suite;
    sid->authType = parsedTicket->authType;
    sid->authKeyBits = parsedTicket->authKeyBits;
    sid->keaType = parsedTicket->keaType;
    sid->keaKeyBits = parsedTicket->keaKeyBits;
    sid->keaGroup = parsedTicket->originalKeaGroup;
    sid->namedCurve = parsedTicket->namedCurve;
    sid->sigScheme = parsedTicket->signatureScheme;

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
        SECITEM_FreeItem(&sid->u.ssl3.alpnSelection, PR_FALSE);
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
ssl3_ProcessSessionTicketCommon(sslSocket *ss, const SECItem *ticket,
                                SECItem *appToken)
{
    SECItem decryptedTicket = { siBuffer, NULL, 0 };
    SessionTicket parsedTicket;
    sslSessionID *sid = NULL;
    SECStatus rv;

    if (ss->sec.ci.sid != NULL) {
        ssl_UncacheSessionID(ss);
        ssl_FreeSID(ss->sec.ci.sid);
        ss->sec.ci.sid = NULL;
    }

    if (!SECITEM_AllocItem(NULL, &decryptedTicket, ticket->len)) {
        return SECFailure;
    }

    /* Decrypt the ticket. */
    rv = ssl_SelfEncryptUnprotect(ss, ticket->data, ticket->len,
                                  decryptedTicket.data,
                                  &decryptedTicket.len,
                                  decryptedTicket.len);
    if (rv != SECSuccess) {
        /* Ignore decryption failure if we are doing TLS 1.3; that
         * means the server rejects the client's resumption
         * attempt. In TLS 1.2, however, it's a hard failure, unless
         * it's just because we're not the recipient of the ticket. */
        if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 ||
            PORT_GetError() == SEC_ERROR_NOT_A_RECIPIENT) {
            SECITEM_ZfreeItem(&decryptedTicket, PR_FALSE);
            return SECSuccess;
        }

        SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        goto loser;
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
    PRTime end = parsedTicket.timestamp + (ssl_ticket_lifetime * PR_USEC_PER_SEC);
    if (end > ssl_Time(ss)) {

        rv = ssl_CreateSIDFromTicket(ss, ticket, &parsedTicket, &sid);
        if (rv != SECSuccess) {
            goto loser; /* code already set */
        }
        if (appToken && parsedTicket.applicationToken.len) {
            rv = SECITEM_CopyItem(NULL, appToken,
                                  &parsedTicket.applicationToken);
            if (rv != SECSuccess) {
                goto loser; /* code already set */
            }
        }

        ss->statelessResume = PR_TRUE;
        ss->sec.ci.sid = sid;

        /* We have the baseline value for the obfuscated ticket age here.  Save
         * that in xtnData temporarily.  This value is updated in
         * tls13_ServerHandlePreSharedKeyXtn with the final estimate. */
        ss->xtnData.ticketAge = parsedTicket.ticketAgeBaseline;
    }

    SECITEM_ZfreeItem(&decryptedTicket, PR_FALSE);
    PORT_Memset(&parsedTicket, 0, sizeof(parsedTicket));
    return SECSuccess;

loser:
    if (sid) {
        ssl_FreeSID(sid);
    }
    SECITEM_ZfreeItem(&decryptedTicket, PR_FALSE);
    PORT_Memset(&parsedTicket, 0, sizeof(parsedTicket));
    return SECFailure;
}

SECStatus
ssl3_ServerHandleSessionTicketXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                  SECItem *data)
{
    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

    /* Ignore the SessionTicket extension if processing is disabled. */
    if (!ss->opt.enableSessionTickets) {
        return SECSuccess;
    }

    /* If we are doing TLS 1.3, then ignore this. */
    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    /* Keep track of negotiated extensions. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_session_ticket_xtn;

    /* Parse the received ticket sent in by the client.  We are
     * lenient about some parse errors, falling back to a fullshake
     * instead of terminating the current connection.
     */
    if (data->len == 0) {
        xtnData->emptySessionTicket = PR_TRUE;
        return SECSuccess;
    }

    return ssl3_ProcessSessionTicketCommon(CONST_CAST(sslSocket, ss), data,
                                           NULL);
}

/* Extension format:
 * Extension number:   2 bytes
 * Extension length:   2 bytes
 * Verify Data Length: 1 byte
 * Verify Data (TLS): 12 bytes (client) or 24 bytes (server)
 * Verify Data (SSL): 36 bytes (client) or 72 bytes (server)
 */
SECStatus
ssl3_SendRenegotiationInfoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                              sslBuffer *buf, PRBool *added)
{
    PRInt32 len = 0;
    SECStatus rv;

    /* In RFC 5746, it is NOT RECOMMENDED to send both the SCSV and the empty
     * RI, so when we send SCSV in the initial handshake, we don't also send RI.
     */
    if (ss->ssl3.hs.sendingSCSV) {
        return 0;
    }
    if (ss->firstHsDone) {
        len = ss->sec.isServer ? ss->ssl3.hs.finishedBytes * 2
                               : ss->ssl3.hs.finishedBytes;
    }

    /* verify_Data from previous Finished message(s) */
    rv = sslBuffer_AppendVariable(buf,
                                  ss->ssl3.hs.finishedMsgs.data, len, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

/* This function runs in both the client and server.  */
SECStatus
ssl3_HandleRenegotiationInfoXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                SECItem *data)
{
    SECStatus rv = SECSuccess;
    PRUint32 len = 0;

    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

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
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_renegotiation_info_xtn;
    if (ss->sec.isServer) {
        /* prepare to send back the appropriate response */
        rv = ssl3_RegisterExtensionSender(ss, xtnData,
                                          ssl_renegotiation_info_xtn,
                                          ssl3_SendRenegotiationInfoXtn);
    }
    return rv;
}

SECStatus
ssl3_ClientSendUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                          sslBuffer *buf, PRBool *added)
{
    unsigned int i;
    SECStatus rv;

    if (!IS_DTLS(ss) || !ss->ssl3.dtlsSRTPCipherCount) {
        return SECSuccess; /* Not relevant */
    }

    /* Length of the SRTP cipher list */
    rv = sslBuffer_AppendNumber(buf, 2 * ss->ssl3.dtlsSRTPCipherCount, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* The SRTP ciphers */
    for (i = 0; i < ss->ssl3.dtlsSRTPCipherCount; i++) {
        rv = sslBuffer_AppendNumber(buf, ss->ssl3.dtlsSRTPCiphers[i], 2);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    }
    /* Empty MKI value */
    rv = sslBuffer_AppendNumber(buf, 0, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_ServerSendUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                          sslBuffer *buf, PRBool *added)
{
    SECStatus rv;

    /* Length of the SRTP cipher list */
    rv = sslBuffer_AppendNumber(buf, 2, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* The selected cipher */
    rv = sslBuffer_AppendNumber(buf, xtnData->dtlsSRTPCipherSuite, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* Empty MKI value */
    rv = sslBuffer_AppendNumber(buf, 0, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_ClientHandleUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                            SECItem *data)
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
ssl3_ServerHandleUseSRTPXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                            SECItem *data)
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

/* ssl3_HandleSigAlgsXtn handles the signature_algorithms extension from a
 * client.  In TLS 1.3, the client uses this to parse CertificateRequest
 * extensions.  See https://tools.ietf.org/html/rfc5246#section-7.4.1.4.1 */
SECStatus
ssl3_HandleSigAlgsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                      SECItem *data)
{
    SECStatus rv;

    /* Ignore this extension if we aren't doing TLS 1.2 or greater. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_2) {
        return SECSuccess;
    }

    if (xtnData->sigSchemes) {
        PORT_Free(xtnData->sigSchemes);
        xtnData->sigSchemes = NULL;
    }
    rv = ssl_ParseSignatureSchemes(ss, NULL,
                                   &xtnData->sigSchemes,
                                   &xtnData->numSigSchemes,
                                   &data->data, &data->len);
    if (rv != SECSuccess) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }
    if (xtnData->numSigSchemes == 0) {
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
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_signature_algorithms_xtn;
    return SECSuccess;
}

/* ssl3_ClientSendSigAlgsXtn sends the signature_algorithm extension for TLS
 * 1.2 ClientHellos. */
SECStatus
ssl3_SendSigAlgsXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                    sslBuffer *buf, PRBool *added)
{
    if (ss->vrange.max < SSL_LIBRARY_VERSION_TLS_1_2) {
        return SECSuccess;
    }

    PRUint16 minVersion;
    if (ss->sec.isServer) {
        minVersion = ss->version; /* CertificateRequest */
    } else {
        minVersion = ss->vrange.min; /* ClientHello */
    }

    SECStatus rv = ssl3_EncodeSigAlgs(ss, minVersion, PR_TRUE /* forCert */, buf);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_SendExtendedMasterSecretXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                 sslBuffer *buf, PRBool *added)
{
    if (!ss->opt.enableExtendedMS) {
        return SECSuccess;
    }

    /* Always send the extension in this function, since the
     * client always sends it and this function is only called on
     * the server if we negotiated the extension. */
    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_HandleExtendedMasterSecretXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                   SECItem *data)
{
    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

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
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_extended_master_secret_xtn;

    if (ss->sec.isServer) {
        return ssl3_RegisterExtensionSender(ss, xtnData,
                                            ssl_extended_master_secret_xtn,
                                            ssl_SendEmptyExtension);
    }
    return SECSuccess;
}

/* ssl3_ClientSendSignedCertTimestampXtn sends the signed_certificate_timestamp
 * extension for TLS ClientHellos. */
SECStatus
ssl3_ClientSendSignedCertTimestampXtn(const sslSocket *ss,
                                      TLSExtensionData *xtnData,
                                      sslBuffer *buf, PRBool *added)
{
    /* Only send the extension if processing is enabled. */
    if (!ss->opt.enableSignedCertTimestamps) {
        return SECSuccess;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_ClientHandleSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData,
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
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_signed_cert_timestamp_xtn;
    return SECSuccess;
}

SECStatus
ssl3_ServerSendSignedCertTimestampXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                                      sslBuffer *buf, PRBool *added)
{
    const SECItem *scts = &ss->sec.serverCert->signedCertTimestamps;
    SECStatus rv;

    if (!scts->len) {
        /* No timestamps to send */
        return SECSuccess;
    }

    rv = sslBuffer_Append(buf, scts->data, scts->len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}

SECStatus
ssl3_ServerHandleSignedCertTimestampXtn(const sslSocket *ss,
                                        TLSExtensionData *xtnData,
                                        SECItem *data)
{
    if (data->len != 0) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    xtnData->negotiated[xtnData->numNegotiated++] = ssl_signed_cert_timestamp_xtn;
    PORT_Assert(ss->sec.isServer);
    return ssl3_RegisterExtensionSender(ss, xtnData,
                                        ssl_signed_cert_timestamp_xtn,
                                        ssl3_ServerSendSignedCertTimestampXtn);
}

/* Just make sure that the remote client supports uncompressed points,
 * Since that is all we support.  Disable ECC cipher suites if it doesn't.
 */
SECStatus
ssl3_HandleSupportedPointFormatsXtn(const sslSocket *ss,
                                    TLSExtensionData *xtnData,
                                    SECItem *data)
{
    int i;

    PORT_Assert(ss->version < SSL_LIBRARY_VERSION_TLS_1_3);

    if (data->len < 2 || data->len > 255 || !data->data ||
        data->len != (unsigned int)data->data[0] + 1) {
        ssl3_ExtDecodeError(ss);
        return SECFailure;
    }
    for (i = data->len; --i > 0;) {
        if (data->data[i] == 0) {
            /* indicate that we should send a reply */
            return ssl3_RegisterExtensionSender(
                ss, xtnData, ssl_ec_point_formats_xtn,
                &ssl3_SendSupportedPointFormatsXtn);
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
                             SECItem *data)
{
    SECStatus rv;

    rv = ssl_UpdateSupportedGroups(CONST_CAST(sslSocket, ss), data);
    if (rv != SECSuccess)
        return SECFailure;

    /* TLS 1.3 permits the server to send this extension so make it so. */
    if (ss->sec.isServer && ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = ssl3_RegisterExtensionSender(ss, xtnData, ssl_supported_groups_xtn,
                                          &ssl_SendSupportedGroupsXtn);
        if (rv != SECSuccess) {
            return SECFailure; /* error already set. */
        }
    }

    /* Remember that we negotiated this extension. */
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_supported_groups_xtn;

    return SECSuccess;
}

SECStatus
ssl_HandleRecordSizeLimitXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                             SECItem *data)
{
    SECStatus rv;
    PRUint32 limit;
    PRUint32 maxLimit = (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3)
                            ? (MAX_FRAGMENT_LENGTH + 1)
                            : MAX_FRAGMENT_LENGTH;

    rv = ssl3_ExtConsumeHandshakeNumber(ss, &limit, 2, &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    if (data->len != 0 || limit < 64) {
        ssl3_ExtSendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
        return SECFailure;
    }

    if (ss->sec.isServer) {
        rv = ssl3_RegisterExtensionSender(ss, xtnData, ssl_record_size_limit_xtn,
                                          &ssl_SendRecordSizeLimitXtn);
        if (rv != SECSuccess) {
            return SECFailure; /* error already set. */
        }
    } else if (limit > maxLimit) {
        /* The client can sensibly check the maximum. */
        ssl3_ExtSendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
        return SECFailure;
    }

    /* We can't enforce the maximum on a server. But we do need to ensure
     * that we don't apply a limit that is too large. */
    xtnData->recordSizeLimit = PR_MIN(maxLimit, limit);
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_record_size_limit_xtn;
    return SECSuccess;
}

SECStatus
ssl_SendRecordSizeLimitXtn(const sslSocket *ss, TLSExtensionData *xtnData,
                           sslBuffer *buf, PRBool *added)
{
    PRUint32 maxLimit;
    if (ss->sec.isServer) {
        maxLimit = (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3)
                       ? (MAX_FRAGMENT_LENGTH + 1)
                       : MAX_FRAGMENT_LENGTH;
    } else {
        maxLimit = (ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3)
                       ? (MAX_FRAGMENT_LENGTH + 1)
                       : MAX_FRAGMENT_LENGTH;
    }
    PRUint32 limit = PR_MIN(ss->opt.recordSizeLimit, maxLimit);
    SECStatus rv = sslBuffer_AppendNumber(buf, limit, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    *added = PR_TRUE;
    return SECSuccess;
}
