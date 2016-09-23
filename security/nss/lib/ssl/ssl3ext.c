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
#include "sslproto.h"
#include "sslimpl.h"
#include "pk11pub.h"
#ifdef NO_PKCS11_BYPASS
#include "blapit.h"
#else
#include "blapi.h"
#endif
#include "prinit.h"

static unsigned char key_name[SESS_TICKET_KEY_NAME_LEN];
static PK11SymKey *session_ticket_enc_key_pkcs11 = NULL;
static PK11SymKey *session_ticket_mac_key_pkcs11 = NULL;

#ifndef NO_PKCS11_BYPASS
static unsigned char session_ticket_enc_key[AES_256_KEY_LENGTH];
static unsigned char session_ticket_mac_key[SHA256_LENGTH];

static PRBool session_ticket_keys_initialized = PR_FALSE;
#endif
static PRCallOnceType generate_session_keys_once;

/* forward static function declarations */
static SECStatus ssl3_ParseEncryptedSessionTicket(sslSocket *ss,
                                                  SECItem *data, EncryptedSessionTicket *enc_session_ticket);
static SECStatus ssl3_AppendToItem(SECItem *item, const unsigned char *buf,
                                   PRUint32 bytes);
static SECStatus ssl3_AppendNumberToItem(SECItem *item, PRUint32 num,
                                         PRInt32 lenSize);
static SECStatus ssl3_GetSessionTicketKeysPKCS11(sslSocket *ss,
                                                 PK11SymKey **aes_key, PK11SymKey **mac_key);
#ifndef NO_PKCS11_BYPASS
static SECStatus ssl3_GetSessionTicketKeys(const unsigned char **aes_key,
                                           PRUint32 *aes_key_length, const unsigned char **mac_key,
                                           PRUint32 *mac_key_length);
#endif
static PRInt32 ssl3_SendRenegotiationInfoXtn(sslSocket *ss,
                                             PRBool append, PRUint32 maxBytes);
static SECStatus ssl3_HandleRenegotiationInfoXtn(sslSocket *ss,
                                                 PRUint16 ex_type, SECItem *data);
static SECStatus ssl3_ClientHandleNextProtoNegoXtn(sslSocket *ss,
                                                   PRUint16 ex_type, SECItem *data);
static SECStatus ssl3_ClientHandleAppProtoXtn(sslSocket *ss,
                                              PRUint16 ex_type, SECItem *data);
static SECStatus ssl3_ServerHandleNextProtoNegoXtn(sslSocket *ss,
                                                   PRUint16 ex_type, SECItem *data);
static SECStatus ssl3_ServerHandleAppProtoXtn(sslSocket *ss, PRUint16 ex_type,
                                              SECItem *data);
static PRInt32 ssl3_ClientSendNextProtoNegoXtn(sslSocket *ss, PRBool append,
                                               PRUint32 maxBytes);
static PRInt32 ssl3_ClientSendAppProtoXtn(sslSocket *ss, PRBool append,
                                          PRUint32 maxBytes);
static PRInt32 ssl3_ServerSendAppProtoXtn(sslSocket *ss, PRBool append,
                                          PRUint32 maxBytes);
static PRInt32 ssl3_ClientSendUseSRTPXtn(sslSocket *ss, PRBool append,
                                         PRUint32 maxBytes);
static PRInt32 ssl3_ServerSendUseSRTPXtn(sslSocket *ss, PRBool append,
                                         PRUint32 maxBytes);
static SECStatus ssl3_ClientHandleUseSRTPXtn(sslSocket *ss, PRUint16 ex_type,
                                             SECItem *data);
static SECStatus ssl3_ServerHandleUseSRTPXtn(sslSocket *ss, PRUint16 ex_type,
                                             SECItem *data);
static PRInt32 ssl3_ServerSendStatusRequestXtn(sslSocket *ss,
                                               PRBool append, PRUint32 maxBytes);
static PRInt32 tls13_ServerSendStatusRequestXtn(sslSocket *ss,
                                                PRBool append, PRUint32 maxBytes);
static SECStatus ssl3_ServerHandleStatusRequestXtn(sslSocket *ss,
                                                   PRUint16 ex_type, SECItem *data);
static SECStatus ssl3_ClientHandleStatusRequestXtn(sslSocket *ss,
                                                   PRUint16 ex_type,
                                                   SECItem *data);
static PRInt32 ssl3_ClientSendStatusRequestXtn(sslSocket *ss, PRBool append,
                                               PRUint32 maxBytes);
static PRInt32 ssl3_ClientSendSigAlgsXtn(sslSocket *ss, PRBool append,
                                         PRUint32 maxBytes);
static SECStatus ssl3_ServerHandleSigAlgsXtn(sslSocket *ss, PRUint16 ex_type,
                                             SECItem *data);

static PRInt32 ssl3_ClientSendSignedCertTimestampXtn(sslSocket *ss,
                                                     PRBool append,
                                                     PRUint32 maxBytes);
static SECStatus ssl3_ClientHandleSignedCertTimestampXtn(sslSocket *ss,
                                                         PRUint16 ex_type,
                                                         SECItem *data);
static PRInt32 ssl3_ServerSendSignedCertTimestampXtn(sslSocket *ss,
                                                     PRBool append,
                                                     PRUint32 maxBytes);
static SECStatus ssl3_ServerHandleSignedCertTimestampXtn(sslSocket *ss,
                                                         PRUint16 ex_type,
                                                         SECItem *data);
static PRInt32 ssl3_ClientSendDraftVersionXtn(sslSocket *ss, PRBool append,
                                              PRUint32 maxBytes);
static SECStatus ssl3_ServerHandleDraftVersionXtn(sslSocket *ss, PRUint16 ex_type,
                                                  SECItem *data);
static PRInt32 ssl3_SendExtendedMasterSecretXtn(sslSocket *ss, PRBool append,
                                                PRUint32 maxBytes);
static SECStatus ssl3_HandleExtendedMasterSecretXtn(sslSocket *ss,
                                                    PRUint16 ex_type,
                                                    SECItem *data);
static PRInt32 tls13_ClientSendKeyShareXtn(sslSocket *ss, PRBool append,
                                           PRUint32 maxBytes);
static SECStatus tls13_ClientHandleKeyShareXtn(sslSocket *ss,
                                               PRUint16 ex_type,
                                               SECItem *data);
static SECStatus tls13_ServerHandleKeyShareXtn(sslSocket *ss,
                                               PRUint16 ex_type,
                                               SECItem *data);
static PRInt32 tls13_ClientSendPreSharedKeyXtn(sslSocket *ss, PRBool append,
                                               PRUint32 maxBytes);
static SECStatus tls13_ServerHandlePreSharedKeyXtn(sslSocket *ss,
                                                   PRUint16 ex_type,
                                                   SECItem *data);
static SECStatus tls13_ClientHandlePreSharedKeyXtn(sslSocket *ss,
                                                   PRUint16 ex_type,
                                                   SECItem *data);
static PRInt32 tls13_ClientSendEarlyDataXtn(sslSocket *ss,
                                            PRBool append,
                                            PRUint32 maxBytes);
static SECStatus tls13_ServerHandleEarlyDataXtn(sslSocket *ss, PRUint16 ex_type,
                                                SECItem *data);
static SECStatus tls13_ClientHandleEarlyDataXtn(sslSocket *ss, PRUint16 ex_type,
                                                SECItem *data);
static SECStatus tls13_ClientHandleTicketEarlyDataInfoXtn(
    sslSocket *ss, PRUint16 ex_type,
    SECItem *data);
static SECStatus tls13_ClientHandleSigAlgsXtn(sslSocket *ss, PRUint16 ex_type,
                                              SECItem *data);

/*
 * Write bytes.  Using this function means the SECItem structure
 * cannot be freed.  The caller is expected to call this function
 * on a shallow copy of the structure.
 */
static SECStatus
ssl3_AppendToItem(SECItem *item, const unsigned char *buf, PRUint32 bytes)
{
    if (bytes > item->len)
        return SECFailure;

    PORT_Memcpy(item->data, buf, bytes);
    item->data += bytes;
    item->len -= bytes;
    return SECSuccess;
}

/*
 * Write a number in network byte order. Using this function means the
 * SECItem structure cannot be freed.  The caller is expected to call
 * this function on a shallow copy of the structure.
 */
static SECStatus
ssl3_AppendNumberToItem(SECItem *item, PRUint32 num, PRInt32 lenSize)
{
    SECStatus rv;
    PRUint8 b[4];
    PRUint8 *p = b;

    switch (lenSize) {
        case 4:
            *p++ = (PRUint8)(num >> 24);
        case 3:
            *p++ = (PRUint8)(num >> 16);
        case 2:
            *p++ = (PRUint8)(num >> 8);
        case 1:
            *p = (PRUint8)num;
    }
    rv = ssl3_AppendToItem(item, &b[0], lenSize);
    return rv;
}

SECStatus
ssl3_SessionTicketShutdown(void *appData, void *nssData)
{
    if (session_ticket_enc_key_pkcs11) {
        PK11_FreeSymKey(session_ticket_enc_key_pkcs11);
        session_ticket_enc_key_pkcs11 = NULL;
    }
    if (session_ticket_mac_key_pkcs11) {
        PK11_FreeSymKey(session_ticket_mac_key_pkcs11);
        session_ticket_mac_key_pkcs11 = NULL;
    }
    PORT_Memset(&generate_session_keys_once, 0,
                sizeof(generate_session_keys_once));
    return SECSuccess;
}

static PRStatus
ssl3_GenerateSessionTicketKeysPKCS11(void *data)
{
    SECStatus rv;
    sslSocket *ss = (sslSocket *)data;
    sslServerCertType certType = { ssl_auth_rsa_decrypt, NULL };
    const sslServerCert *sc;
    SECKEYPrivateKey *svrPrivKey;
    SECKEYPublicKey *svrPubKey;

    sc = ssl_FindServerCert(ss, &certType);
    if (!sc || !sc->serverKeyPair) {
        SSL_DBG(("%d: SSL[%d]: No ssl_auth_rsa_decrypt cert and key pair",
                 SSL_GETPID(), ss->fd));
        goto loser;
    }
    svrPrivKey = sc->serverKeyPair->privKey;
    svrPubKey = sc->serverKeyPair->pubKey;
    if (svrPrivKey == NULL || svrPubKey == NULL) {
        SSL_DBG(("%d: SSL[%d]: Pub or priv key(s) is NULL.",
                 SSL_GETPID(), ss->fd));
        goto loser;
    }

    /* Get a copy of the session keys from shared memory. */
    PORT_Memcpy(key_name, SESS_TICKET_KEY_NAME_PREFIX,
                sizeof(SESS_TICKET_KEY_NAME_PREFIX));
    if (!ssl_GetSessionTicketKeysPKCS11(svrPrivKey, svrPubKey,
                                        ss->pkcs11PinArg, &key_name[SESS_TICKET_KEY_NAME_PREFIX_LEN],
                                        &session_ticket_enc_key_pkcs11, &session_ticket_mac_key_pkcs11))
        return PR_FAILURE;

    rv = NSS_RegisterShutdown(ssl3_SessionTicketShutdown, NULL);
    if (rv != SECSuccess)
        goto loser;

    return PR_SUCCESS;

loser:
    ssl3_SessionTicketShutdown(NULL, NULL);
    return PR_FAILURE;
}

static SECStatus
ssl3_GetSessionTicketKeysPKCS11(sslSocket *ss, PK11SymKey **aes_key,
                                PK11SymKey **mac_key)
{
    if (PR_CallOnceWithArg(&generate_session_keys_once,
                           ssl3_GenerateSessionTicketKeysPKCS11, ss) !=
        PR_SUCCESS)
        return SECFailure;

    if (session_ticket_enc_key_pkcs11 == NULL ||
        session_ticket_mac_key_pkcs11 == NULL)
        return SECFailure;

    *aes_key = session_ticket_enc_key_pkcs11;
    *mac_key = session_ticket_mac_key_pkcs11;
    return SECSuccess;
}

#ifndef NO_PKCS11_BYPASS
static PRStatus
ssl3_GenerateSessionTicketKeys(void)
{
    PORT_Memcpy(key_name, SESS_TICKET_KEY_NAME_PREFIX,
                sizeof(SESS_TICKET_KEY_NAME_PREFIX));

    if (!ssl_GetSessionTicketKeys(&key_name[SESS_TICKET_KEY_NAME_PREFIX_LEN],
                                  session_ticket_enc_key, session_ticket_mac_key))
        return PR_FAILURE;

    session_ticket_keys_initialized = PR_TRUE;
    return PR_SUCCESS;
}

static SECStatus
ssl3_GetSessionTicketKeys(const unsigned char **aes_key,
                          PRUint32 *aes_key_length, const unsigned char **mac_key,
                          PRUint32 *mac_key_length)
{
    if (PR_CallOnce(&generate_session_keys_once,
                    ssl3_GenerateSessionTicketKeys) != PR_SUCCESS)
        return SECFailure;

    if (!session_ticket_keys_initialized)
        return SECFailure;

    *aes_key = session_ticket_enc_key;
    *aes_key_length = sizeof(session_ticket_enc_key);
    *mac_key = session_ticket_mac_key;
    *mac_key_length = sizeof(session_ticket_mac_key);

    return SECSuccess;
}
#endif

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
    { ssl_tls13_draft_version_xtn, &ssl3_ServerHandleDraftVersionXtn },
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
      { ssl_tls13_draft_version_xtn, &ssl3_ClientSendDraftVersionXtn },
      { ssl_signed_cert_timestamp_xtn, &ssl3_ClientSendSignedCertTimestampXtn },
      { ssl_tls13_key_share_xtn, &tls13_ClientSendKeyShareXtn },
      { ssl_tls13_pre_shared_key_xtn, &tls13_ClientSendPreSharedKeyXtn },
      { ssl_tls13_early_data_xtn, &tls13_ClientSendEarlyDataXtn },
      /* Some servers (e.g. WebSphere Application Server 7.0 and Tomcat) will
       * time out or terminate the connection if the last extension in the
       * client hello is empty. They are not intolerant of TLS 1.2, so list
       * signature_algorithms at the end. See bug 1243641. */
      { ssl_signature_algorithms_xtn, &ssl3_ClientSendSigAlgsXtn }
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

/* Format an SNI extension, using the name from the socket's URL,
 * unless that name is a dotted decimal string.
 * Used by client and server.
 */
PRInt32
ssl3_SendServerNameXtn(sslSocket *ss, PRBool append,
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
            rv = ssl3_AppendHandshakeNumber(ss, ssl_server_name_xtn, 2);
            if (rv != SECSuccess)
                return -1;
            /* length of extension_data */
            rv = ssl3_AppendHandshakeNumber(ss, len + 5, 2);
            if (rv != SECSuccess)
                return -1;
            /* length of server_name_list */
            rv = ssl3_AppendHandshakeNumber(ss, len + 3, 2);
            if (rv != SECSuccess)
                return -1;
            /* Name Type (sni_host_name) */
            rv = ssl3_AppendHandshake(ss, "\0", 1);
            if (rv != SECSuccess)
                return -1;
            /* HostName (length and value) */
            rv = ssl3_AppendHandshakeVariable(ss, (PRUint8 *)ss->url, len, 2);
            if (rv != SECSuccess)
                return -1;
            if (!ss->sec.isServer) {
                TLSExtensionData *xtnData = &ss->xtnData;
                xtnData->advertised[xtnData->numAdvertised++] =
                    ssl_server_name_xtn;
            }
        }
        return len + 9;
    }
    /* Server side */
    if (append && maxBytes >= 4) {
        rv = ssl3_AppendHandshakeNumber(ss, ssl_server_name_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* length of extension_data */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
    }
    return 4;
}

/* Handle an incoming SNI extension. */
SECStatus
ssl3_HandleServerNameXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECItem *names = NULL;
    TLSExtensionData *xtnData = &ss->xtnData;
    PRInt32 listLenBytes = 0;

    if (!ss->sec.isServer) {
        return SECSuccess; /* ignore extension */
    }

    /* Server side - consume client data and register server sender. */
    /* do not parse the data if don't have user extension handling function. */
    if (!ss->sniSocketConfig) {
        return SECSuccess;
    }

    /* length of server_name_list */
    listLenBytes = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    if (listLenBytes < 0) {
        goto loser; /* alert already sent */
    }
    if (listLenBytes == 0 || listLenBytes != data->len) {
        goto alert_loser;
    }

    /* Read ServerNameList. */
    while (data->len > 0) {
        SECItem tmp;
        SECStatus rv;
        PRInt32 type;

        /* Read Name Type. */
        type = ssl3_ConsumeHandshakeNumber(ss, 1, &data->data, &data->len);
        if (type < 0) { /* i.e., SECFailure cast to PRint32 */
            /* alert sent in ConsumeHandshakeNumber */
            goto loser;
        }

        /* Read ServerName (length and value). */
        rv = ssl3_ConsumeHandshakeVariable(ss, &tmp, 2, &data->data, &data->len);
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
    (void)ssl3_DecodeError(ss);
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
    sslSocket *ss,
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
            if (ss->xtnData.ticketTimestampVerified) {
                extension_length += session_ticket->ticket.len;
            } else if (!append &&
                       (session_ticket->ticket_lifetime_hint == 0 ||
                        (session_ticket->ticket_lifetime_hint +
                             session_ticket->received_timestamp >
                         ssl_Time()))) {
                extension_length += session_ticket->ticket.len;
                ss->xtnData.ticketTimestampVerified = PR_TRUE;
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
        rv = ssl3_AppendHandshakeNumber(ss, ssl_session_ticket_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        if (session_ticket && session_ticket->ticket.data &&
            ss->xtnData.ticketTimestampVerified) {
            rv = ssl3_AppendHandshakeVariable(ss, session_ticket->ticket.data,
                                              session_ticket->ticket.len, 2);
            ss->xtnData.ticketTimestampVerified = PR_FALSE;
            ss->xtnData.sentSessionTicketInClientHello = PR_TRUE;
        } else {
            rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        }
        if (rv != SECSuccess)
            goto loser;

        if (!ss->sec.isServer) {
            TLSExtensionData *xtnData = &ss->xtnData;
            xtnData->advertised[xtnData->numAdvertised++] =
                ssl_session_ticket_xtn;
        }
    }
    return extension_length;

loser:
    ss->xtnData.ticketTimestampVerified = PR_FALSE;
    return -1;
}

/* handle an incoming Next Protocol Negotiation extension. */
static SECStatus
ssl3_ServerHandleNextProtoNegoXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    if (ss->firstHsDone || data->len != 0) {
        /* Clients MUST send an empty NPN extension, if any. */
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

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
ssl3_SelectAppProtocol(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    unsigned char resultBuffer[255];
    SECItem result = { siBuffer, resultBuffer, 0 };

    rv = ssl3_ValidateNextProtoNego(data->data, data->len);
    if (rv != SECSuccess) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
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
        (void)SSL3_SendAlert(ss, alert_fatal, internal_error);
        return SECFailure;
    }

    /* If the callback wrote more than allowed to |result| it has corrupted our
     * stack. */
    if (result.len > sizeof(resultBuffer)) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        /* TODO: crash */
        return SECFailure;
    }

    SECITEM_FreeItem(&ss->ssl3.nextProto, PR_FALSE);

    if (ex_type == ssl_app_layer_protocol_xtn &&
        ss->ssl3.nextProtoState != SSL_NEXT_PROTO_NEGOTIATED) {
        /* The callback might say OK, but then it picks a default value - one
         * that was not listed.  That's OK for NPN, but not ALPN. */
        (void)SSL3_SendAlert(ss, alert_fatal, no_application_protocol);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_NO_PROTOCOL);
        return SECFailure;
    }

    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    return SECITEM_CopyItem(NULL, &ss->ssl3.nextProto, &result);
}

/* handle an incoming ALPN extension at the server */
static SECStatus
ssl3_ServerHandleAppProtoXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    int count;
    SECStatus rv;

    /* We expressly don't want to allow ALPN on renegotiation,
     * despite it being permitted by the spec. */
    if (ss->firstHsDone || data->len == 0) {
        /* Clients MUST send a non-empty ALPN extension. */
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    /* Unlike NPN, ALPN has extra redundant length information so that
     * the extension is the same in both ClientHello and ServerHello. */
    count = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    if (count != data->len) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    if (!ss->nextProtoCallback) {
        /* we're not configured for it */
        return SECSuccess;
    }

    rv = ssl3_SelectAppProtocol(ss, ex_type, data);
    if (rv != SECSuccess) {
        return rv;
    }

    /* prepare to send back a response, if we negotiated */
    if (ss->ssl3.nextProtoState == SSL_NEXT_PROTO_NEGOTIATED) {
        rv = ssl3_RegisterServerHelloExtensionSender(
            ss, ex_type, ssl3_ServerSendAppProtoXtn);
        if (rv != SECSuccess) {
            (void)SSL3_SendAlert(ss, alert_fatal, internal_error);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return rv;
        }
    }
    return SECSuccess;
}

static SECStatus
ssl3_ClientHandleNextProtoNegoXtn(sslSocket *ss, PRUint16 ex_type,
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
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_BAD_SERVER);
        return SECFailure;
    }

    /* We should only get this call if we sent the extension, so
     * ss->nextProtoCallback needs to be non-NULL.  However, it is possible
     * that an application erroneously cleared the callback between the time
     * we sent the ClientHello and now. */
    if (!ss->nextProtoCallback) {
        PORT_Assert(0);
        (void)SSL3_SendAlert(ss, alert_fatal, internal_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_NO_CALLBACK);
        return SECFailure;
    }

    return ssl3_SelectAppProtocol(ss, ex_type, data);
}

static SECStatus
ssl3_ClientHandleAppProtoXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    PRInt32 list_len;
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
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    list_len = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    /* The list has to be the entire extension. */
    if (list_len != data->len) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &protocol_name, 1,
                                       &data->data, &data->len);
    /* The list must have exactly one value. */
    if (rv != SECSuccess || data->len != 0) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
        return SECFailure;
    }

    SECITEM_FreeItem(&ss->ssl3.nextProto, PR_FALSE);
    ss->ssl3.nextProtoState = SSL_NEXT_PROTO_SELECTED;
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    return SECITEM_CopyItem(NULL, &ss->ssl3.nextProto, &protocol_name);
}

static PRInt32
ssl3_ClientSendNextProtoNegoXtn(sslSocket *ss, PRBool append,
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
        rv = ssl3_AppendHandshakeNumber(ss, ssl_next_proto_nego_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            goto loser;
        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_next_proto_nego_xtn;
    }

    return extension_length;

loser:
    return -1;
}

static PRInt32
ssl3_ClientSendAppProtoXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
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

        rv = ssl3_AppendHandshakeNumber(ss, ssl_app_layer_protocol_xtn, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess) {
            goto loser;
        }
        rv = ssl3_AppendHandshakeVariable(ss, alpn_protos, len, 2);
        PORT_Free(alpn_protos);
        alpn_protos = NULL;
        if (rv != SECSuccess) {
            goto loser;
        }
        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_app_layer_protocol_xtn;
    }

    return extension_length;

loser:
    if (alpn_protos) {
        PORT_Free(alpn_protos);
    }
    return -1;
}

static PRInt32
ssl3_ServerSendAppProtoXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_length;

    /* we're in over our heads if any of these fail */
    PORT_Assert(ss->opt.enableALPN);
    PORT_Assert(ss->ssl3.nextProto.data);
    PORT_Assert(ss->ssl3.nextProto.len > 0);
    PORT_Assert(ss->ssl3.nextProtoState == SSL_NEXT_PROTO_NEGOTIATED);
    PORT_Assert(!ss->firstHsDone);

    extension_length = 2 /* extension type */ + 2 /* extension length */ +
                       2 /* protocol name list */ + 1 /* name length */ +
                       ss->ssl3.nextProto.len;

    if (maxBytes < (PRUint32)extension_length) {
        return 0;
    }
    if (append) {
        SECStatus rv;
        rv = ssl3_AppendHandshakeNumber(ss, ssl_app_layer_protocol_xtn, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_AppendHandshakeNumber(ss, ss->ssl3.nextProto.len + 1, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_AppendHandshakeVariable(ss, ss->ssl3.nextProto.data,
                                          ss->ssl3.nextProto.len, 1);
        if (rv != SECSuccess) {
            return -1;
        }
    }

    return extension_length;
}

static SECStatus
ssl3_ServerHandleStatusRequestXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    ssl3HelloExtensionSenderFunc sender;

    PORT_Assert(ss->sec.isServer);

    /* remember that we got this extension. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        sender = tls13_ServerSendStatusRequestXtn;
    } else {
        sender = ssl3_ServerSendStatusRequestXtn;
    }
    return ssl3_RegisterServerHelloExtensionSender(ss, ex_type, sender);
}

static PRInt32
ssl3_ServerSendStatusRequestXtn(
    sslSocket *ss,
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
        rv = ssl3_AppendHandshakeNumber(ss, ssl_cert_status_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* length of extension_data */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
        /* The certificate status data is sent in ssl3_SendCertificateStatus. */
    }

    return extension_length;
}

static PRInt32
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

/* ssl3_ClientSendStatusRequestXtn builds the status_request extension on the
 * client side. See RFC 6066 section 8. */
static PRInt32
ssl3_ClientSendStatusRequestXtn(sslSocket *ss, PRBool append,
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
        TLSExtensionData *xtnData;

        /* extension_type */
        rv = ssl3_AppendHandshakeNumber(ss, ssl_cert_status_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            return -1;
        rv = ssl3_AppendHandshakeNumber(ss, 1 /* status_type ocsp */, 1);
        if (rv != SECSuccess)
            return -1;
        /* A zero length responder_id_list means that the responders are
         * implicitly known to the server. */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;
        /* A zero length request_extensions means that there are no extensions.
         * Specifically, we don't set the id-pkix-ocsp-nonce extension. This
         * means that the server can replay a cached OCSP response to us. */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            return -1;

        xtnData = &ss->xtnData;
        xtnData->advertised[xtnData->numAdvertised++] = ssl_cert_status_xtn;
    }
    return extension_length;
}

static SECStatus
ssl3_ClientHandleStatusRequestXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    /* In TLS 1.3, the extension carries the OCSP response. */
    if (ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        SECStatus rv;
        rv = ssl_ReadCertificateStatus(ss, data->data, data->len);
        if (rv != SECSuccess) {
            return SECFailure; /* code already set */
        }
    } else if (data->len != 0) {
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    return SECSuccess;
}

/*
 * Called from ssl3_SendNewSessionTicket, tls13_SendNewSessionTicket
 */
SECStatus
ssl3_EncodeSessionTicket(sslSocket *ss,
                         const NewSessionTicket *ticket,
                         SECItem *ticket_data)
{
    PRUint32 i;
    SECStatus rv;
    SECItem plaintext;
    SECItem plaintext_item = { 0, NULL, 0 };
    SECItem ciphertext = { 0, NULL, 0 };
    PRUint32 ciphertext_length;
    SECItem ticket_buf = { 0, NULL, 0 };
    SECItem ticket_tmp = { 0, NULL, 0 };
    PRBool ms_is_wrapped;
    unsigned char wrapped_ms[SSL3_MASTER_SECRET_LENGTH];
    SECItem ms_item = { 0, NULL, 0 };
    PRUint32 padding_length;
    PRUint32 ticket_length;
    PRUint32 cert_length = 0;
    PRUint8 length_buf[4];
    PRUint32 now;
    PK11SymKey *aes_key_pkcs11 = NULL;
    PK11SymKey *mac_key_pkcs11 = NULL;
#ifndef NO_PKCS11_BYPASS
    const unsigned char *aes_key = NULL;
    const unsigned char *mac_key = NULL;
    PRUint32 aes_key_length;
    PRUint32 mac_key_length;
    PRUint64 aes_ctx_buf[MAX_CIPHER_CONTEXT_LLONGS];
    AESContext *aes_ctx;
    const SECHashObject *hashObj = NULL;
    PRUint64 hmac_ctx_buf[MAX_MAC_CONTEXT_LLONGS];
    HMACContext *hmac_ctx = NULL;
#endif
    CK_MECHANISM_TYPE cipherMech = CKM_AES_CBC;
    PK11Context *aes_ctx_pkcs11;
    CK_MECHANISM_TYPE macMech = CKM_SHA256_HMAC;
    PK11Context *hmac_ctx_pkcs11 = NULL;
    unsigned char computed_mac[TLS_EX_SESS_TICKET_MAC_LENGTH];
    unsigned int computed_mac_length;
    unsigned char iv[AES_BLOCK_SIZE];
    SECItem ivItem;
    SECItem *srvName = NULL;
    PRUint32 srvNameLen = 0;
    CK_MECHANISM_TYPE msWrapMech = 0; /* dummy default value,
                                          * must be >= 0 */
    ssl3CipherSpec *spec = ss->version >= SSL_LIBRARY_VERSION_TLS_1_3 ? ss->ssl3.cwSpec : ss->ssl3.pwSpec;
    const sslServerCertType *certType;
    SECItem alpnSelection = { siBuffer, NULL, 0 };

    SSL_TRC(3, ("%d: SSL3[%d]: send session_ticket handshake",
                SSL_GETPID(), ss->fd));

    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    if (ss->opt.requestCertificate && ss->sec.ci.sid->peerCert) {
        cert_length = 3 + ss->sec.ci.sid->peerCert->derCert.len;
    }

    /* Get IV and encryption keys */
    ivItem.data = iv;
    ivItem.len = sizeof(iv);
    rv = PK11_GenerateRandom(iv, sizeof(iv));
    if (rv != SECSuccess)
        goto loser;

#ifndef NO_PKCS11_BYPASS
    if (ss->opt.bypassPKCS11) {
        rv = ssl3_GetSessionTicketKeys(&aes_key, &aes_key_length,
                                       &mac_key, &mac_key_length);
    } else
#endif
    {
        rv = ssl3_GetSessionTicketKeysPKCS11(ss, &aes_key_pkcs11,
                                             &mac_key_pkcs11);
    }
    if (rv != SECSuccess)
        goto loser;

    if (spec->msItem.len && spec->msItem.data) {
        /* The master secret is available unwrapped. */
        ms_item.data = spec->msItem.data;
        ms_item.len = spec->msItem.len;
        ms_is_wrapped = PR_FALSE;
    } else {
        /* Extract the master secret wrapped. */
        sslSessionID sid;
        PORT_Memset(&sid, 0, sizeof(sslSessionID));

        rv = ssl3_CacheWrappedMasterSecret(ss, &sid, spec,
                                           ss->ssl3.hs.kea_def->authKeyType);
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
    if (srvName->data && srvName->len) {
        srvNameLen = 2 + srvName->len; /* len bytes + name len */
    }

    if (ss->ssl3.nextProtoState != SSL_NEXT_PROTO_NO_SUPPORT &&
        ss->ssl3.nextProto.data) {
        alpnSelection = ss->ssl3.nextProto;
    }

    ciphertext_length =
        sizeof(PRUint16)                                                                        /* ticket_version */
        + sizeof(SSL3ProtocolVersion)                                                           /* ssl_version */
        + sizeof(ssl3CipherSuite)                                                               /* ciphersuite */
        + 1                                                                                     /* compression */
        + 10                                                                                    /* cipher spec parameters */
        + 1                                                                                     /* certType arguments */
        + 1                                                                                     /* SessionTicket.ms_is_wrapped */
        + 4                                                                                     /* msWrapMech */
        + 2                                                                                     /* master_secret.length */
        + ms_item.len                                                                           /* master_secret */
        + 1                                                                                     /* client_auth_type */
        + cert_length                                                                           /* cert */
        + 1                                                                                     /* server name type */
        + srvNameLen                                                                            /* name len + length field */
        + 1                                                                                     /* extendedMasterSecretUsed */
        + sizeof(ticket->ticket_lifetime_hint) + sizeof(ticket->flags) + 1 + alpnSelection.len; /* npn value + length field. */
    padding_length = AES_BLOCK_SIZE -
                     (ciphertext_length %
                      AES_BLOCK_SIZE);
    ciphertext_length += padding_length;

    if (SECITEM_AllocItem(NULL, &plaintext_item, ciphertext_length) == NULL)
        goto loser;

    plaintext = plaintext_item;

    /* ticket_version */
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
    certType = &ss->sec.serverCert->certType;
    PORT_Assert(certType->authType == ss->sec.authType);
    switch (ss->sec.authType) {
        case ssl_auth_ecdsa:
        case ssl_auth_ecdh_rsa:
        case ssl_auth_ecdh_ecdsa:
            PORT_Assert(certType->namedCurve);
            PORT_Assert(certType->namedCurve->keaType == ssl_kea_ecdh);
            /* EC curves only use the second of the two bytes. */
            PORT_Assert(certType->namedCurve->name < 256);
            rv = ssl3_AppendNumberToItem(&plaintext,
                                         certType->namedCurve->name, 1);
            break;
        default:
            rv = ssl3_AppendNumberToItem(&plaintext, 0, 1);
            break;
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

    /* client_identity */
    if (ss->opt.requestCertificate && ss->sec.ci.sid->peerCert) {
        rv = ssl3_AppendNumberToItem(&plaintext, CLIENT_AUTH_CERTIFICATE, 1);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendNumberToItem(&plaintext,
                                     ss->sec.ci.sid->peerCert->derCert.len, 3);
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

    if (srvNameLen) {
        /* Name Type (sni_host_name) */
        rv = ssl3_AppendNumberToItem(&plaintext, srvName->type, 1);
        if (rv != SECSuccess)
            goto loser;
        /* HostName (length and value) */
        rv = ssl3_AppendNumberToItem(&plaintext, srvName->len, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendToItem(&plaintext, srvName->data, srvName->len);
        if (rv != SECSuccess)
            goto loser;
    } else {
        /* No Name */
        rv = ssl3_AppendNumberToItem(&plaintext, (char)TLS_STE_NO_SERVER_NAME, 1);
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

    /* NPN value. */
    PORT_Assert(alpnSelection.len < 256);
    rv = ssl3_AppendNumberToItem(&plaintext, alpnSelection.len, 1);
    if (rv != SECSuccess)
        goto loser;
    if (alpnSelection.len) {
        rv = ssl3_AppendToItem(&plaintext, alpnSelection.data, alpnSelection.len);
        if (rv != SECSuccess)
            goto loser;
    }

    PORT_Assert(plaintext.len == padding_length);
    for (i = 0; i < padding_length; i++)
        plaintext.data[i] = (unsigned char)padding_length;

    if (SECITEM_AllocItem(NULL, &ciphertext, ciphertext_length) == NULL) {
        rv = SECFailure;
        goto loser;
    }

/* Generate encrypted portion of ticket. */
#ifndef NO_PKCS11_BYPASS
    if (ss->opt.bypassPKCS11) {
        PORT_Assert(aes_key);

        aes_ctx = (AESContext *)aes_ctx_buf;
        rv = AES_InitContext(aes_ctx, aes_key, aes_key_length, iv,
                             NSS_AES_CBC, 1, AES_BLOCK_SIZE);
        if (rv != SECSuccess)
            goto loser;

        rv = AES_Encrypt(aes_ctx, ciphertext.data, &ciphertext.len,
                         ciphertext.len, plaintext_item.data,
                         plaintext_item.len);
        if (rv != SECSuccess)
            goto loser;
    } else
#endif
    {
        PORT_Assert(aes_key_pkcs11);
        aes_ctx_pkcs11 = PK11_CreateContextBySymKey(cipherMech,
                                                    CKA_ENCRYPT, aes_key_pkcs11, &ivItem);
        if (!aes_ctx_pkcs11)
            goto loser;

        rv = PK11_CipherOp(aes_ctx_pkcs11, ciphertext.data,
                           (int *)&ciphertext.len, ciphertext.len,
                           plaintext_item.data, plaintext_item.len);
        PK11_Finalize(aes_ctx_pkcs11);
        PK11_DestroyContext(aes_ctx_pkcs11, PR_TRUE);
        if (rv != SECSuccess)
            goto loser;
    }

    /* Convert ciphertext length to network order. */
    length_buf[0] = (ciphertext.len >> 8) & 0xff;
    length_buf[1] = (ciphertext.len) & 0xff;

/* Compute MAC. */
#ifndef NO_PKCS11_BYPASS
    if (ss->opt.bypassPKCS11) {
        PORT_Assert(mac_key);

        hmac_ctx = (HMACContext *)hmac_ctx_buf;
        hashObj = HASH_GetRawHashObject(HASH_AlgSHA256);
        if (HMAC_Init(hmac_ctx, hashObj, mac_key,
                      mac_key_length, PR_FALSE) != SECSuccess) {
            goto loser;
        }

        HMAC_Begin(hmac_ctx);
        HMAC_Update(hmac_ctx, key_name, SESS_TICKET_KEY_NAME_LEN);
        HMAC_Update(hmac_ctx, iv, sizeof(iv));
        HMAC_Update(hmac_ctx, (unsigned char *)length_buf, 2);
        HMAC_Update(hmac_ctx, ciphertext.data, ciphertext.len);
        if (HMAC_Finish(hmac_ctx, computed_mac, &computed_mac_length,
                        sizeof(computed_mac)) != SECSuccess) {
            goto loser;
        }
    } else
#endif
    {
        SECItem macParam;
        PORT_Assert(mac_key_pkcs11);
        macParam.data = NULL;
        macParam.len = 0;
        hmac_ctx_pkcs11 = PK11_CreateContextBySymKey(macMech,
                                                     CKA_SIGN, mac_key_pkcs11, &macParam);
        if (!hmac_ctx_pkcs11)
            goto loser;

        rv = PK11_DigestBegin(hmac_ctx_pkcs11);
        if (rv != SECSuccess)
            goto loser;
        rv = PK11_DigestOp(hmac_ctx_pkcs11, key_name,
                           SESS_TICKET_KEY_NAME_LEN);
        if (rv != SECSuccess)
            goto loser;
        rv = PK11_DigestOp(hmac_ctx_pkcs11, iv, sizeof(iv));
        if (rv != SECSuccess)
            goto loser;
        rv = PK11_DigestOp(hmac_ctx_pkcs11, (unsigned char *)length_buf, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = PK11_DigestOp(hmac_ctx_pkcs11, ciphertext.data, ciphertext.len);
        if (rv != SECSuccess)
            goto loser;
        rv = PK11_DigestFinal(hmac_ctx_pkcs11, computed_mac,
                              &computed_mac_length, sizeof(computed_mac));
        if (rv != SECSuccess)
            goto loser;
    }

    ticket_length =
        +SESS_TICKET_KEY_NAME_LEN        /* key_name */
        + AES_BLOCK_SIZE                 /* iv */
        + 2                              /* length field for NewSessionTicket.ticket.encrypted_state */
        + ciphertext_length              /* encrypted_state */
        + TLS_EX_SESS_TICKET_MAC_LENGTH; /* mac */

    if (SECITEM_AllocItem(NULL, &ticket_buf, ticket_length) == NULL) {
        rv = SECFailure;
        goto loser;
    }
    ticket_tmp = ticket_buf; /* Shallow copy because AppendToItem is
                              * destructive. */

    rv = ssl3_AppendToItem(&ticket_tmp, key_name, SESS_TICKET_KEY_NAME_LEN);
    if (rv != SECSuccess)
        goto loser;

    rv = ssl3_AppendToItem(&ticket_tmp, iv, sizeof(iv));
    if (rv != SECSuccess)
        goto loser;

    rv = ssl3_AppendNumberToItem(&ticket_tmp, ciphertext.len, 2);
    if (rv != SECSuccess)
        goto loser;

    rv = ssl3_AppendToItem(&ticket_tmp, ciphertext.data, ciphertext.len);
    if (rv != SECSuccess)
        goto loser;

    rv = ssl3_AppendToItem(&ticket_tmp, computed_mac, computed_mac_length);
    if (rv != SECSuccess)
        goto loser;

    /* Give ownership of memory to caller. */
    *ticket_data = ticket_buf;
    ticket_buf.data = NULL;

loser:
#ifndef NO_PKCS11_BYPASS
    if (hmac_ctx) {
        HMAC_Destroy(hmac_ctx, PR_FALSE);
    }
#endif
    if (hmac_ctx_pkcs11) {
        PK11_DestroyContext(hmac_ctx_pkcs11, PR_TRUE);
    }
    if (plaintext_item.data) {
        SECITEM_FreeItem(&plaintext_item, PR_FALSE);
    }
    if (ciphertext.data) {
        SECITEM_FreeItem(&ciphertext, PR_FALSE);
    }
    if (ticket_buf.data) {
        SECITEM_FreeItem(&ticket_buf, PR_FALSE);
    }

    return rv;
}

/* When a client receives a SessionTicket extension a NewSessionTicket
 * message is expected during the handshake.
 */
SECStatus
ssl3_ClientHandleSessionTicketXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    if (data->len != 0) {
        return SECSuccess; /* Ignore the extension. */
    }

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    return SECSuccess;
}

/* Generic ticket processing code, common to TLS 1.0-1.2 and
 * TLS 1.3. */
static SECStatus
ssl3_ProcessSessionTicketCommon(sslSocket *ss, SECItem *data)
{
    SECStatus rv;
    SECItem *decrypted_state = NULL;
    SessionTicket *parsed_session_ticket = NULL;
    sslSessionID *sid = NULL;
    SSL3Statistics *ssl3stats;
    PRUint32 i;
    SECItem extension_data;
    EncryptedSessionTicket enc_session_ticket;
    unsigned char computed_mac[TLS_EX_SESS_TICKET_MAC_LENGTH];
    unsigned int computed_mac_length;
#ifndef NO_PKCS11_BYPASS
    const SECHashObject *hashObj;
    const unsigned char *aes_key = NULL;
    const unsigned char *mac_key = NULL;
    PRUint32 aes_key_length;
    PRUint32 mac_key_length;
    PRUint64 hmac_ctx_buf[MAX_MAC_CONTEXT_LLONGS];
    HMACContext *hmac_ctx;
    PRUint64 aes_ctx_buf[MAX_CIPHER_CONTEXT_LLONGS];
    AESContext *aes_ctx;
#endif
    PK11SymKey *aes_key_pkcs11 = NULL;
    PK11SymKey *mac_key_pkcs11 = NULL;
    PK11Context *hmac_ctx_pkcs11;
    CK_MECHANISM_TYPE macMech = CKM_SHA256_HMAC;
    PK11Context *aes_ctx_pkcs11;
    CK_MECHANISM_TYPE cipherMech = CKM_AES_CBC;
    unsigned char *padding;
    PRUint32 padding_length;
    unsigned char *buffer;
    unsigned int buffer_len;
    PRInt32 temp;
    SECItem cert_item;
    PRInt8 nameType = TLS_STE_NO_SERVER_NAME;
    SECItem alpn_item;

    /* Turn off stateless session resumption if the client sends a
     * SessionTicket extension, even if the extension turns out to be
     * malformed (ss->sec.ci.sid is non-NULL when doing session
     * renegotiation.)
     */
    if (ss->sec.ci.sid != NULL) {
        ss->sec.uncache(ss->sec.ci.sid);
        ssl_FreeSID(ss->sec.ci.sid);
        ss->sec.ci.sid = NULL;
    }

    extension_data.data = data->data; /* Keep a copy for future use. */
    extension_data.len = data->len;

    if (ssl3_ParseEncryptedSessionTicket(ss, data, &enc_session_ticket) !=
        SECSuccess) {
        return SECSuccess; /* Pretend it isn't there */
    }

/* Get session ticket keys. */
#ifndef NO_PKCS11_BYPASS
    if (ss->opt.bypassPKCS11) {
        rv = ssl3_GetSessionTicketKeys(&aes_key, &aes_key_length,
                                       &mac_key, &mac_key_length);
    } else
#endif
    {
        rv = ssl3_GetSessionTicketKeysPKCS11(ss, &aes_key_pkcs11,
                                             &mac_key_pkcs11);
    }
    if (rv != SECSuccess) {
        SSL_DBG(("%d: SSL[%d]: Unable to get/generate session ticket keys.",
                 SSL_GETPID(), ss->fd));
        goto loser;
    }

    /* If the ticket sent by the client was generated under a key different
     * from the one we have, bypass ticket processing.
     */
    if (PORT_Memcmp(enc_session_ticket.key_name, key_name,
                    SESS_TICKET_KEY_NAME_LEN) != 0) {
        SSL_DBG(("%d: SSL[%d]: Session ticket key_name sent mismatch.",
                 SSL_GETPID(), ss->fd));
        goto no_ticket;
    }

/* Verify the MAC on the ticket.  MAC verification may also
     * fail if the MAC key has been recently refreshed.
     */
#ifndef NO_PKCS11_BYPASS
    if (ss->opt.bypassPKCS11) {
        PORT_Assert(mac_key);
        hmac_ctx = (HMACContext *)hmac_ctx_buf;
        hashObj = HASH_GetRawHashObject(HASH_AlgSHA256);
        if (HMAC_Init(hmac_ctx, hashObj, mac_key,
                      sizeof(session_ticket_mac_key), PR_FALSE) != SECSuccess)
            goto no_ticket;
        HMAC_Begin(hmac_ctx);
        HMAC_Update(hmac_ctx, extension_data.data,
                    extension_data.len - TLS_EX_SESS_TICKET_MAC_LENGTH);
        rv = HMAC_Finish(hmac_ctx, computed_mac, &computed_mac_length,
                         sizeof(computed_mac));
        HMAC_Destroy(hmac_ctx, PR_FALSE);
        if (rv != SECSuccess) {
            goto no_ticket;
        }
    } else
#endif
    {
        SECItem macParam;
        PORT_Assert(mac_key_pkcs11);
        macParam.data = NULL;
        macParam.len = 0;
        hmac_ctx_pkcs11 = PK11_CreateContextBySymKey(macMech,
                                                     CKA_SIGN, mac_key_pkcs11, &macParam);
        if (!hmac_ctx_pkcs11) {
            SSL_DBG(("%d: SSL[%d]: Unable to create HMAC context: %d.",
                     SSL_GETPID(), ss->fd, PORT_GetError()));
            goto no_ticket;
        } else {
            SSL_DBG(("%d: SSL[%d]: Successfully created HMAC context.",
                     SSL_GETPID(), ss->fd));
        }
        rv = PK11_DigestBegin(hmac_ctx_pkcs11);
        if (rv != SECSuccess) {
            PK11_DestroyContext(hmac_ctx_pkcs11, PR_TRUE);
            goto no_ticket;
        }
        rv = PK11_DigestOp(hmac_ctx_pkcs11, extension_data.data,
                           extension_data.len -
                               TLS_EX_SESS_TICKET_MAC_LENGTH);
        if (rv != SECSuccess) {
            PK11_DestroyContext(hmac_ctx_pkcs11, PR_TRUE);
            goto no_ticket;
        }
        rv = PK11_DigestFinal(hmac_ctx_pkcs11, computed_mac,
                              &computed_mac_length, sizeof(computed_mac));
        PK11_DestroyContext(hmac_ctx_pkcs11, PR_TRUE);
        if (rv != SECSuccess)
            goto no_ticket;
    }
    if (NSS_SecureMemcmp(computed_mac, enc_session_ticket.mac,
                         computed_mac_length) !=
        0) {
        SSL_DBG(("%d: SSL[%d]: Session ticket MAC mismatch.",
                 SSL_GETPID(), ss->fd));
        goto no_ticket;
    }

    /* We ignore key_name for now.
     * This is ok as MAC verification succeeded.
     */

    /* Decrypt the ticket. */

    /* Plaintext is shorter than the ciphertext due to padding. */
    decrypted_state = SECITEM_AllocItem(NULL, NULL,
                                        enc_session_ticket.encrypted_state.len);

#ifndef NO_PKCS11_BYPASS
    if (ss->opt.bypassPKCS11) {
        PORT_Assert(aes_key);
        aes_ctx = (AESContext *)aes_ctx_buf;
        rv = AES_InitContext(aes_ctx, aes_key,
                             sizeof(session_ticket_enc_key), enc_session_ticket.iv,
                             NSS_AES_CBC, 0, AES_BLOCK_SIZE);
        if (rv != SECSuccess) {
            SSL_DBG(("%d: SSL[%d]: Unable to create AES context.",
                     SSL_GETPID(), ss->fd));
            goto no_ticket;
        }

        rv = AES_Decrypt(aes_ctx, decrypted_state->data,
                         &decrypted_state->len, decrypted_state->len,
                         enc_session_ticket.encrypted_state.data,
                         enc_session_ticket.encrypted_state.len);
        if (rv != SECSuccess)
            goto no_ticket;
    } else
#endif
    {
        SECItem ivItem;
        PORT_Assert(aes_key_pkcs11);
        ivItem.data = enc_session_ticket.iv;
        ivItem.len = AES_BLOCK_SIZE;
        aes_ctx_pkcs11 = PK11_CreateContextBySymKey(cipherMech,
                                                    CKA_DECRYPT, aes_key_pkcs11, &ivItem);
        if (!aes_ctx_pkcs11) {
            SSL_DBG(("%d: SSL[%d]: Unable to create AES context.",
                     SSL_GETPID(), ss->fd));
            goto no_ticket;
        }

        rv = PK11_CipherOp(aes_ctx_pkcs11, decrypted_state->data,
                           (int *)&decrypted_state->len, decrypted_state->len,
                           enc_session_ticket.encrypted_state.data,
                           enc_session_ticket.encrypted_state.len);
        PK11_Finalize(aes_ctx_pkcs11);
        PK11_DestroyContext(aes_ctx_pkcs11, PR_TRUE);
        if (rv != SECSuccess)
            goto no_ticket;
    }

    /* Check padding. */
    padding_length =
        (PRUint32)decrypted_state->data[decrypted_state->len - 1];
    if (padding_length == 0 || padding_length > AES_BLOCK_SIZE)
        goto no_ticket;

    padding = &decrypted_state->data[decrypted_state->len - padding_length];
    for (i = 0; i < padding_length; i++, padding++) {
        if (padding_length != (PRUint32)*padding)
            goto no_ticket;
    }

    /* Deserialize session state. */
    buffer = decrypted_state->data;
    buffer_len = decrypted_state->len;

    parsed_session_ticket = PORT_ZAlloc(sizeof(SessionTicket));
    if (parsed_session_ticket == NULL) {
        rv = SECFailure;
        goto loser;
    }

    /* Read ticket_version and reject if the version is wrong */
    temp = ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
    if (temp != TLS_EX_SESS_TICKET_VERSION)
        goto no_ticket;

    parsed_session_ticket->ticket_version = (SSL3ProtocolVersion)temp;

    /* Read SSLVersion. */
    temp = ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->ssl_version = (SSL3ProtocolVersion)temp;

    /* Read cipher_suite. */
    temp = ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->cipher_suite = (ssl3CipherSuite)temp;

    /* Read compression_method. */
    temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->compression_method = (SSLCompressionMethod)temp;

    /* Read cipher spec parameters. */
    temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->authType = (SSLAuthType)temp;
    temp = ssl3_ConsumeHandshakeNumber(ss, 4, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->authKeyBits = (PRUint32)temp;
    temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->keaType = (SSLKEAType)temp;
    temp = ssl3_ConsumeHandshakeNumber(ss, 4, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->keaKeyBits = (PRUint32)temp;

    /* Read certificate slot */
    parsed_session_ticket->certType.authType = parsed_session_ticket->authType;
    temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    switch (parsed_session_ticket->authType) {
        case ssl_auth_ecdsa:
        case ssl_auth_ecdh_rsa:
        case ssl_auth_ecdh_ecdsa: {
            const sslNamedGroupDef *group =
                ssl_LookupNamedGroup((SSLNamedGroup)temp);
            if (!group || group->keaType != ssl_kea_ecdh) {
                goto no_ticket;
            }
            parsed_session_ticket->certType.namedCurve = group;
        } break;
        default:
            break;
    }

    /* Read wrapped master_secret. */
    temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->ms_is_wrapped = (PRBool)temp;

    temp = ssl3_ConsumeHandshakeNumber(ss, 4, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->msWrapMech = (CK_MECHANISM_TYPE)temp;

    temp = ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->ms_length = (PRUint16)temp;
    if (parsed_session_ticket->ms_length == 0 || /* sanity check MS. */
        parsed_session_ticket->ms_length >
            sizeof(parsed_session_ticket->master_secret))
        goto no_ticket;

    /* Allow for the wrapped master secret to be longer. */
    if (buffer_len < parsed_session_ticket->ms_length)
        goto no_ticket;
    PORT_Memcpy(parsed_session_ticket->master_secret, buffer,
                parsed_session_ticket->ms_length);
    buffer += parsed_session_ticket->ms_length;
    buffer_len -= parsed_session_ticket->ms_length;

    /* Read client_identity */
    temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->client_identity.client_auth_type =
        (ClientAuthenticationType)temp;
    switch (parsed_session_ticket->client_identity.client_auth_type) {
        case CLIENT_AUTH_ANONYMOUS:
            break;
        case CLIENT_AUTH_CERTIFICATE:
            rv = ssl3_ConsumeHandshakeVariable(ss, &cert_item, 3,
                                               &buffer, &buffer_len);
            if (rv != SECSuccess)
                goto no_ticket;
            rv = SECITEM_CopyItem(NULL, &parsed_session_ticket->peer_cert,
                                  &cert_item);
            if (rv != SECSuccess)
                goto no_ticket;
            break;
        default:
            goto no_ticket;
    }
    /* Read timestamp. */
    temp = ssl3_ConsumeHandshakeNumber(ss, 4, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    parsed_session_ticket->timestamp = (PRUint32)temp;

    /* Read server name */
    nameType =
        ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (nameType != TLS_STE_NO_SERVER_NAME) {
        SECItem name_item;
        rv = ssl3_ConsumeHandshakeVariable(ss, &name_item, 2, &buffer,
                                           &buffer_len);
        if (rv != SECSuccess)
            goto no_ticket;
        rv = SECITEM_CopyItem(NULL, &parsed_session_ticket->srvName,
                              &name_item);
        if (rv != SECSuccess)
            goto no_ticket;
        parsed_session_ticket->srvName.type = nameType;
    }

    /* Read extendedMasterSecretUsed */
    temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
    if (temp < 0)
        goto no_ticket;
    PORT_Assert(temp == PR_TRUE || temp == PR_FALSE);
    parsed_session_ticket->extendedMasterSecretUsed = (PRBool)temp;

    rv = ssl3_ConsumeHandshake(ss, &parsed_session_ticket->flags, 4,
                               &buffer, &buffer_len);
    if (rv != SECSuccess)
        goto no_ticket;
    parsed_session_ticket->flags = PR_ntohl(parsed_session_ticket->flags);

    rv = ssl3_ConsumeHandshakeVariable(ss, &alpn_item, 1, &buffer, &buffer_len);
    if (rv != SECSuccess)
        goto no_ticket;
    if (alpn_item.len != 0) {
        rv = SECITEM_CopyItem(NULL, &parsed_session_ticket->alpnSelection,
                              &alpn_item);
        if (rv != SECSuccess)
            goto no_ticket;
        if (alpn_item.len >= 256)
            goto no_ticket;
    }

    /* Done parsing.  Check that all bytes have been consumed. */
    if (buffer_len != padding_length)
        goto no_ticket;

    /* Use the ticket if it has not expired, otherwise free the allocated
     * memory since the ticket is of no use.
     */
    if (parsed_session_ticket->timestamp != 0 &&
        parsed_session_ticket->timestamp +
                TLS_EX_SESS_TICKET_LIFETIME_HINT >
            ssl_Time()) {

        sid = ssl3_NewSessionID(ss, PR_TRUE);
        if (sid == NULL) {
            rv = SECFailure;
            goto loser;
        }

        /* Copy over parameters. */
        sid->version = parsed_session_ticket->ssl_version;
        sid->u.ssl3.cipherSuite = parsed_session_ticket->cipher_suite;
        sid->u.ssl3.compression = parsed_session_ticket->compression_method;
        sid->authType = parsed_session_ticket->authType;
        sid->authKeyBits = parsed_session_ticket->authKeyBits;
        sid->keaType = parsed_session_ticket->keaType;
        sid->keaKeyBits = parsed_session_ticket->keaKeyBits;
        memcpy(&sid->certType, &parsed_session_ticket->certType,
               sizeof(sslServerCertType));

        if (SECITEM_CopyItem(NULL, &sid->u.ssl3.locked.sessionTicket.ticket,
                             &extension_data) != SECSuccess)
            goto no_ticket;
        sid->u.ssl3.locked.sessionTicket.flags = parsed_session_ticket->flags;

/* Copy master secret. */
#ifndef NO_PKCS11_BYPASS
        if (ss->opt.bypassPKCS11 &&
            parsed_session_ticket->ms_is_wrapped)
            goto no_ticket;
#endif
        if (parsed_session_ticket->ms_length >
            sizeof(sid->u.ssl3.keys.wrapped_master_secret))
            goto no_ticket;
        PORT_Memcpy(sid->u.ssl3.keys.wrapped_master_secret,
                    parsed_session_ticket->master_secret,
                    parsed_session_ticket->ms_length);
        sid->u.ssl3.keys.wrapped_master_secret_len =
            parsed_session_ticket->ms_length;
        sid->u.ssl3.masterWrapMech = parsed_session_ticket->msWrapMech;
        sid->u.ssl3.keys.msIsWrapped =
            parsed_session_ticket->ms_is_wrapped;
        sid->u.ssl3.masterValid = PR_TRUE;
        sid->u.ssl3.keys.resumable = PR_TRUE;
        sid->u.ssl3.keys.extendedMasterSecretUsed = parsed_session_ticket->extendedMasterSecretUsed;

        /* Copy over client cert from session ticket if there is one. */
        if (parsed_session_ticket->peer_cert.data != NULL) {
            if (sid->peerCert != NULL)
                CERT_DestroyCertificate(sid->peerCert);
            sid->peerCert = CERT_NewTempCertificate(ss->dbHandle,
                                                    &parsed_session_ticket->peer_cert, NULL, PR_FALSE, PR_TRUE);
            if (sid->peerCert == NULL) {
                rv = SECFailure;
                goto loser;
            }
        }
        if (parsed_session_ticket->srvName.data != NULL) {
            if (sid->u.ssl3.srvName.data) {
                SECITEM_FreeItem(&sid->u.ssl3.srvName, PR_FALSE);
            }
            sid->u.ssl3.srvName = parsed_session_ticket->srvName;
        }
        if (parsed_session_ticket->alpnSelection.data != NULL) {
            sid->u.ssl3.alpnSelection = parsed_session_ticket->alpnSelection;
            /* So we don't free below. */
            parsed_session_ticket->alpnSelection.data = NULL;
        }
        ss->statelessResume = PR_TRUE;
        ss->sec.ci.sid = sid;
    }

    if (0) {
    no_ticket:
        SSL_DBG(("%d: SSL[%d]: Session ticket parsing failed.",
                 SSL_GETPID(), ss->fd));
        ssl3stats = SSL_GetStatistics();
        SSL_AtomicIncrementLong(&ssl3stats->hch_sid_ticket_parse_failures);
    }
    rv = SECSuccess;

loser:
    /* ss->sec.ci.sid == sid if it did NOT come here via goto statement
     * in that case do not free sid
     */
    if (sid && (ss->sec.ci.sid != sid)) {
        ssl_FreeSID(sid);
        sid = NULL;
    }
    if (decrypted_state != NULL) {
        SECITEM_FreeItem(decrypted_state, PR_TRUE);
        decrypted_state = NULL;
    }

    if (parsed_session_ticket != NULL) {
        if (parsed_session_ticket->peer_cert.data) {
            SECITEM_FreeItem(&parsed_session_ticket->peer_cert, PR_FALSE);
        }
        if (parsed_session_ticket->alpnSelection.data) {
            SECITEM_FreeItem(&parsed_session_ticket->alpnSelection, PR_FALSE);
        }
        PORT_ZFree(parsed_session_ticket, sizeof(SessionTicket));
    }

    return rv;
}

SECStatus
ssl3_ServerHandleSessionTicketXtn(sslSocket *ss, PRUint16 ex_type,
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
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    /* Parse the received ticket sent in by the client.  We are
     * lenient about some parse errors, falling back to a fullshake
     * instead of terminating the current connection.
     */
    if (data->len == 0) {
        ss->xtnData.emptySessionTicket = PR_TRUE;
        return SECSuccess;
    }

    return ssl3_ProcessSessionTicketCommon(ss, data);
}

/*
 * Read bytes.  Using this function means the SECItem structure
 * cannot be freed.  The caller is expected to call this function
 * on a shallow copy of the structure.
 */
static SECStatus
ssl3_ConsumeFromItem(SECItem *item, unsigned char **buf, PRUint32 bytes)
{
    if (bytes > item->len)
        return SECFailure;

    *buf = item->data;
    item->data += bytes;
    item->len -= bytes;
    return SECSuccess;
}

static SECStatus
ssl3_ParseEncryptedSessionTicket(sslSocket *ss, SECItem *data,
                                 EncryptedSessionTicket *enc_session_ticket)
{
    if (ssl3_ConsumeFromItem(data, &enc_session_ticket->key_name,
                             SESS_TICKET_KEY_NAME_LEN) !=
        SECSuccess)
        return SECFailure;
    if (ssl3_ConsumeFromItem(data, &enc_session_ticket->iv,
                             AES_BLOCK_SIZE) !=
        SECSuccess)
        return SECFailure;
    if (ssl3_ConsumeHandshakeVariable(ss, &enc_session_ticket->encrypted_state,
                                      2, &data->data, &data->len) !=
        SECSuccess)
        return SECFailure;
    if (ssl3_ConsumeFromItem(data, &enc_session_ticket->mac,
                             TLS_EX_SESS_TICKET_MAC_LENGTH) !=
        SECSuccess)
        return SECFailure;
    if (data->len != 0) /* Make sure that we have consumed all bytes. */
        return SECFailure;

    return SECSuccess;
}

/* go through extensions in buffer "b".
 * For each one, find the extension handler in the table, and
 * if present, invoke that handler.
 * Servers ignore any extensions with unknown extension types.
 * Clients reject any extensions with unadvertised extension types
 *
 * In TLS >= 1.3, the client checks that extensions appear in the
 * right phase.
 */
SECStatus
ssl3_HandleExtensions(sslSocket *ss, SSL3Opaque **b, PRUint32 *length,
                      SSL3HandshakeType handshakeMessage)
{
    const ssl3ExtensionHandler *handlers;
    PRBool isTLS13 = ss->version >= SSL_LIBRARY_VERSION_TLS_1_3;

    switch (handshakeMessage) {
        case client_hello:
            handlers = clientHelloHandlers;
            break;
        case new_session_ticket:
            PORT_Assert(ss->version >= SSL_LIBRARY_VERSION_TLS_1_3);
            handlers = newSessionTicketHandlers;
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

    while (*length) {
        const ssl3ExtensionHandler *handler;
        SECStatus rv;
        PRInt32 extension_type;
        SECItem extension_data;

        /* Get the extension's type field */
        extension_type = ssl3_ConsumeHandshakeNumber(ss, 2, b, length);
        if (extension_type < 0) /* failure to decode extension_type */
            return SECFailure;  /* alert already sent */

        /* get the data for this extension, so we can pass it or skip it. */
        rv = ssl3_ConsumeHandshakeVariable(ss, &extension_data, 2, b, length);
        if (rv != SECSuccess)
            return rv; /* alert already sent */

        /* Check whether the server sent an extension which was not advertised
         * in the ClientHello */
        if (!ss->sec.isServer &&
            !ssl3_ClientExtensionAdvertised(ss, extension_type) &&
            (handshakeMessage != new_session_ticket)) {
            (void)SSL3_SendAlert(ss, alert_fatal, unsupported_extension);
            PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
            return SECFailure;
        }

        /* Check whether an extension has been sent multiple times. */
        if (ssl3_ExtensionNegotiated(ss, extension_type)) {
            (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
            PORT_SetError(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
            return SECFailure;
        }

        /* Check that this is a legal extension in TLS 1.3 */
        if (isTLS13 && !tls13_ExtensionAllowed(extension_type, handshakeMessage)) {
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
            if (handler->ex_type == extension_type) {
                rv = (*handler->ex_handler)(ss, (PRUint16)extension_type,
                                            &extension_data);
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
        if (ss->version > SSL_LIBRARY_VERSION_3_0) {
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

/* Extension format:
 * Extension number:   2 bytes
 * Extension length:   2 bytes
 * Verify Data Length: 1 byte
 * Verify Data (TLS): 12 bytes (client) or 24 bytes (server)
 * Verify Data (SSL): 36 bytes (client) or 72 bytes (server)
 */
static PRInt32
ssl3_SendRenegotiationInfoXtn(
    sslSocket *ss,
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
        rv = ssl3_AppendHandshakeNumber(ss, ssl_renegotiation_info_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* length of extension_data */
        rv = ssl3_AppendHandshakeNumber(ss, len + 1, 2);
        if (rv != SECSuccess)
            return -1;
        /* verify_Data from previous Finished message(s) */
        rv = ssl3_AppendHandshakeVariable(ss,
                                          ss->ssl3.hs.finishedMsgs.data, len, 1);
        if (rv != SECSuccess)
            return -1;
        if (!ss->sec.isServer) {
            TLSExtensionData *xtnData = &ss->xtnData;
            xtnData->advertised[xtnData->numAdvertised++] =
                ssl_renegotiation_info_xtn;
        }
    }
    return needed;
}

/* This function runs in both the client and server.  */
static SECStatus
ssl3_HandleRenegotiationInfoXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv = SECSuccess;
    PRUint32 len = 0;

    if (ss->firstHsDone) {
        len = ss->sec.isServer ? ss->ssl3.hs.finishedBytes
                               : ss->ssl3.hs.finishedBytes * 2;
    }
    if (data->len != 1 + len || data->data[0] != len) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }
    if (len && NSS_SecureMemcmp(ss->ssl3.hs.finishedMsgs.data,
                                data->data + 1, len)) {
        (void)SSL3_SendAlert(ss, alert_fatal, handshake_failure);
        PORT_SetError(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
        return SECFailure;
    }
    /* remember that we got this extension and it was correct. */
    ss->peerRequestedProtection = 1;
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    if (ss->sec.isServer) {
        /* prepare to send back the appropriate response */
        rv = ssl3_RegisterServerHelloExtensionSender(ss, ex_type,
                                                     ssl3_SendRenegotiationInfoXtn);
    }
    return rv;
}

static PRInt32
ssl3_ClientSendUseSRTPXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
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
        rv = ssl3_AppendHandshakeNumber(ss, ssl_use_srtp_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        /* Length of extension data */
        rv = ssl3_AppendHandshakeNumber(ss, ext_data_len, 2);
        if (rv != SECSuccess)
            return -1;
        /* Length of the SRTP cipher list */
        rv = ssl3_AppendHandshakeNumber(ss,
                                        2 * ss->ssl3.dtlsSRTPCipherCount,
                                        2);
        if (rv != SECSuccess)
            return -1;
        /* The SRTP ciphers */
        for (i = 0; i < ss->ssl3.dtlsSRTPCipherCount; i++) {
            rv = ssl3_AppendHandshakeNumber(ss,
                                            ss->ssl3.dtlsSRTPCiphers[i],
                                            2);
            if (rv != SECSuccess)
                return -1;
        }
        /* Empty MKI value */
        ssl3_AppendHandshakeVariable(ss, NULL, 0, 1);

        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_use_srtp_xtn;
    }

    return 4 + ext_data_len;
}

static PRInt32
ssl3_ServerSendUseSRTPXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    SECStatus rv;

    /* Server side */
    if (!append || maxBytes < 9) {
        return 9;
    }

    /* Extension type */
    rv = ssl3_AppendHandshakeNumber(ss, ssl_use_srtp_xtn, 2);
    if (rv != SECSuccess)
        return -1;
    /* Length of extension data */
    rv = ssl3_AppendHandshakeNumber(ss, 5, 2);
    if (rv != SECSuccess)
        return -1;
    /* Length of the SRTP cipher list */
    rv = ssl3_AppendHandshakeNumber(ss, 2, 2);
    if (rv != SECSuccess)
        return -1;
    /* The selected cipher */
    rv = ssl3_AppendHandshakeNumber(ss, ss->ssl3.dtlsSRTPCipherSuite, 2);
    if (rv != SECSuccess)
        return -1;
    /* Empty MKI value */
    ssl3_AppendHandshakeVariable(ss, NULL, 0, 1);

    return 9;
}

static SECStatus
ssl3_ClientHandleUseSRTPXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;
    SECItem ciphers = { siBuffer, NULL, 0 };
    PRUint16 i;
    PRUint16 cipher = 0;
    PRBool found = PR_FALSE;
    SECItem litem;

    if (!data->data || !data->len) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* Get the cipher list */
    rv = ssl3_ConsumeHandshakeVariable(ss, &ciphers, 2,
                                       &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure; /* fatal alert already sent */
    }
    /* Now check that the server has picked just 1 (i.e., len = 2) */
    if (ciphers.len != 2) {
        (void)ssl3_DecodeError(ss);
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
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
        return SECFailure;
    }

    /* Get the srtp_mki value */
    rv = ssl3_ConsumeHandshakeVariable(ss, &litem, 1,
                                       &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure; /* alert already sent */
    }

    /* We didn't offer an MKI, so this must be 0 length */
    if (litem.len != 0) {
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
        return SECFailure;
    }

    /* extra trailing bytes */
    if (data->len != 0) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* OK, this looks fine. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ssl_use_srtp_xtn;
    ss->ssl3.dtlsSRTPCipherSuite = cipher;
    return SECSuccess;
}

static SECStatus
ssl3_ServerHandleUseSRTPXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
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
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* Get the cipher list */
    rv = ssl3_ConsumeHandshakeVariable(ss, &ciphers, 2,
                                       &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure; /* alert already sent */
    }
    /* Check that the list is even length */
    if (ciphers.len % 2) {
        (void)ssl3_DecodeError(ss);
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
    rv = ssl3_ConsumeHandshakeVariable(ss, &litem, 1, &data->data, &data->len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (data->len != 0) {
        (void)ssl3_DecodeError(ss); /* trailing bytes */
        return SECFailure;
    }

    /* Now figure out what to do */
    if (!found) {
        /* No matching ciphers, pretend we don't support use_srtp */
        return SECSuccess;
    }

    /* OK, we have a valid cipher and we've selected it */
    ss->ssl3.dtlsSRTPCipherSuite = cipher;
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ssl_use_srtp_xtn;

    return ssl3_RegisterServerHelloExtensionSender(ss, ssl_use_srtp_xtn,
                                                   ssl3_ServerSendUseSRTPXtn);
}

/* ssl3_ServerHandleSigAlgsXtn handles the signature_algorithms extension
 * from a client.
 * See https://tools.ietf.org/html/rfc5246#section-7.4.1.4.1 */
static SECStatus
ssl3_ServerHandleSigAlgsXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;

    /* Ignore this extension if we aren't doing TLS 1.2 or greater. */
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_2) {
        return SECSuccess;
    }

    if (ss->ssl3.hs.clientSigSchemes) {
        PORT_Free(ss->ssl3.hs.clientSigSchemes);
        ss->ssl3.hs.clientSigSchemes = NULL;
    }
    rv = ssl_ParseSignatureSchemes(ss, NULL,
                                   &ss->ssl3.hs.clientSigSchemes,
                                   &ss->ssl3.hs.numClientSigScheme,
                                   &data->data, &data->len);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }
    /* Check for trailing data. */
    if (data->len != 0) {
        (void)SSL3_SendAlert(ss, alert_fatal, decode_error);
        PORT_SetError(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
        return SECFailure;
    }

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    return SECSuccess;
}

/* ssl3_ClientSendSigAlgsXtn sends the signature_algorithm extension for TLS
 * 1.2 ClientHellos. */
static PRInt32
ssl3_ClientSendSigAlgsXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_length;
    PRUint8 buf[MAX_SIGNATURE_SCHEMES * 2];
    PRUint32 len;
    SECStatus rv;

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_2) {
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
        rv = ssl3_AppendHandshakeNumber(ss, ssl_signature_algorithms_xtn, 2);
        if (rv != SECSuccess) {
            return -1;
        }
        rv = ssl3_AppendHandshakeNumber(ss, len + 2, 2);
        if (rv != SECSuccess) {
            return -1;
        }

        rv = ssl3_AppendHandshakeVariable(ss, buf, len, 2);
        if (rv != SECSuccess) {
            return -1;
        }

        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_signature_algorithms_xtn;
    }

    return extension_length;
}

/* Takes the size of the ClientHello, less the record header, and determines how
 * much padding is required. */
unsigned int
ssl3_CalculatePaddingExtensionLength(unsigned int clientHelloLength)
{
    unsigned int recordLength = 1 /* handshake message type */ +
                                3 /* handshake message length */ +
                                clientHelloLength;
    unsigned int extensionLength;

    if (recordLength < 256 || recordLength >= 512) {
        return 0;
    }

    extensionLength = 512 - recordLength;
    /* Extensions take at least four bytes to encode. Always include at least
     * one byte of data if including the extension. Some servers (e.g.
     * WebSphere Application Server 7.0 and Tomcat) will time out or terminate
     * the connection if the last extension in the client hello is empty. */
    if (extensionLength < 4 + 1) {
        extensionLength = 4 + 1;
    }

    return extensionLength;
}

/* ssl3_AppendPaddingExtension possibly adds an extension which ensures that a
 * ClientHello record is either < 256 bytes or is >= 512 bytes. This ensures
 * that we don't trigger bugs in F5 products. */
PRInt32
ssl3_AppendPaddingExtension(sslSocket *ss, unsigned int extensionLen,
                            PRUint32 maxBytes)
{
    unsigned int paddingLen = extensionLen - 4;
    static unsigned char padding[252];

    if (extensionLen == 0) {
        return 0;
    }

    if (extensionLen > maxBytes ||
        !paddingLen ||
        paddingLen > sizeof(padding)) {
        PORT_Assert(0);
        return -1;
    }

    if (SECSuccess != ssl3_AppendHandshakeNumber(ss, ssl_padding_xtn, 2))
        return -1;
    if (SECSuccess != ssl3_AppendHandshakeVariable(ss, padding, paddingLen, 2))
        return -1;

    return extensionLen;
}

/* ssl3_ClientSendDraftVersionXtn sends the TLS 1.3 temporary draft
 * version extension.
 * TODO(ekr@rtfm.com): Remove when TLS 1.3 is published. */
static PRInt32
ssl3_ClientSendDraftVersionXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_length;

    if (ss->version != SSL_LIBRARY_VERSION_TLS_1_3) {
        return 0;
    }

    extension_length = 6; /* Type + length + number */
    if (maxBytes < (PRUint32)extension_length) {
        PORT_Assert(0);
        return 0;
    }
    if (append) {
        SECStatus rv;
        rv = ssl3_AppendHandshakeNumber(ss, ssl_tls13_draft_version_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeNumber(ss, TLS_1_3_DRAFT_VERSION, 2);
        if (rv != SECSuccess)
            goto loser;
        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_tls13_draft_version_xtn;
    }

    return extension_length;

loser:
    return -1;
}

/* ssl3_ServerHandleDraftVersionXtn handles the TLS 1.3 temporary draft
 * version extension.
 * TODO(ekr@rtfm.com): Remove when TLS 1.3 is published. */
static SECStatus
ssl3_ServerHandleDraftVersionXtn(sslSocket *ss, PRUint16 ex_type,
                                 SECItem *data)
{
    PRInt32 draft_version;

    /* Ignore this extension if we aren't doing TLS 1.3 */
    if (ss->version != SSL_LIBRARY_VERSION_TLS_1_3) {
        return SECSuccess;
    }

    if (data->len != 2) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* Get the draft version out of the handshake */
    draft_version = ssl3_ConsumeHandshakeNumber(ss, 2,
                                                &data->data, &data->len);
    if (draft_version < 0) {
        return SECFailure;
    }

    if (draft_version == TLS_1_3_DRAFT_VERSION) {
        /* Mark this as negotiated only if the version matches.  The code in
         * ssl3_HandleClientHello will then reduce the version if needed. */
        ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    } else {
        SSL_TRC(30, ("%d: SSL3[%d]: Incompatible version of TLS 1.3 (%d), "
                     "expected %d",
                     SSL_GETPID(), ss->fd, draft_version, TLS_1_3_DRAFT_VERSION));
    }

    return SECSuccess;
}

static PRInt32
ssl3_SendExtendedMasterSecretXtn(sslSocket *ss, PRBool append,
                                 PRUint32 maxBytes)
{
    PRInt32 extension_length;

    if (!ss->opt.enableExtendedMS) {
        return 0;
    }

#ifndef NO_PKCS11_BYPASS
    /* Extended MS can only be used w/o bypass mode */
    if (ss->opt.bypassPKCS11) {
        PORT_Assert(0);
        PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
        return -1;
    }
#endif

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
        rv = ssl3_AppendHandshakeNumber(ss, ssl_extended_master_secret_xtn, 2);
        if (rv != SECSuccess)
            goto loser;
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            goto loser;
        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_extended_master_secret_xtn;
    }

    return extension_length;

loser:
    return -1;
}

static SECStatus
ssl3_HandleExtendedMasterSecretXtn(sslSocket *ss, PRUint16 ex_type,
                                   SECItem *data)
{
    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_0) {
        return SECSuccess;
    }

    if (!ss->opt.enableExtendedMS) {
        return SECSuccess;
    }

#ifndef NO_PKCS11_BYPASS
    /* Extended MS can only be used w/o bypass mode */
    if (ss->opt.bypassPKCS11) {
        PORT_Assert(0);
        PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
        return SECFailure;
    }
#endif

    if (data->len != 0) {
        SSL_TRC(30, ("%d: SSL3[%d]: Bogus extended master secret extension",
                     SSL_GETPID(), ss->fd));
        return SECFailure;
    }

    SSL_DBG(("%d: SSL[%d]: Negotiated extended master secret extension.",
             SSL_GETPID(), ss->fd));

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    if (ss->sec.isServer) {
        return ssl3_RegisterServerHelloExtensionSender(
            ss, ex_type, ssl3_SendExtendedMasterSecretXtn);
    }
    return SECSuccess;
}

/* ssl3_ClientSendSignedCertTimestampXtn sends the signed_certificate_timestamp
 * extension for TLS ClientHellos. */
static PRInt32
ssl3_ClientSendSignedCertTimestampXtn(sslSocket *ss, PRBool append,
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
        rv = ssl3_AppendHandshakeNumber(ss,
                                        ssl_signed_cert_timestamp_xtn,
                                        2);
        if (rv != SECSuccess)
            goto loser;
        /* zero length */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess)
            goto loser;
        ss->xtnData.advertised[ss->xtnData.numAdvertised++] =
            ssl_signed_cert_timestamp_xtn;
    } else if (maxBytes < extension_length) {
        PORT_Assert(0);
        return 0;
    }

    return extension_length;
loser:
    return -1;
}

static SECStatus
ssl3_ClientHandleSignedCertTimestampXtn(sslSocket *ss, PRUint16 ex_type,
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
    SECItem *scts = &ss->xtnData.signedCertTimestamps;
    PORT_Assert(!scts->data && !scts->len);

    if (!data->len) {
        /* Empty extension data: RFC 6962 mandates non-empty contents. */
        return SECFailure;
    }
    *scts = *data;
    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    return SECSuccess;
}

static PRInt32
ssl3_ServerSendSignedCertTimestampXtn(sslSocket *ss,
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
        rv = ssl3_AppendHandshakeNumber(ss,
                                        ssl_signed_cert_timestamp_xtn,
                                        2);
        if (rv != SECSuccess)
            goto loser;
        /* extension_data */
        rv = ssl3_AppendHandshakeVariable(ss, scts->data, scts->len, 2);
        if (rv != SECSuccess)
            goto loser;
    }

    return extension_length;

loser:
    return -1;
}

static SECStatus
ssl3_ServerHandleSignedCertTimestampXtn(sslSocket *ss, PRUint16 ex_type,
                                        SECItem *data)
{
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    PORT_Assert(ss->sec.isServer);
    return ssl3_RegisterServerHelloExtensionSender(ss, ex_type,
                                                   ssl3_ServerSendSignedCertTimestampXtn);
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
static PRUint32
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

static PRUint32
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

static SECStatus
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

static PRInt32
tls13_ClientSendKeyShareXtn(sslSocket *ss, PRBool append,
                            PRUint32 maxBytes)
{
    PRUint32 extension_length;

    if (ss->version < SSL_LIBRARY_VERSION_TLS_1_3) {
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

static SECStatus
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
static SECStatus
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

/* Handle an incoming KeyShare extension at the server and copy to
 * |ss->ssl3.hs.remoteKeyShares| for future use. The key
 * share is processed in tls13_HandleClientKeyShare(). */
static SECStatus
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
static PRInt32
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

    if (!ss->statelessResume)
        return 0;

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
static SECStatus
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
static SECStatus
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
static PRInt32
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

static SECStatus
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
static SECStatus
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

static SECStatus
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
        return 0;
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
static SECStatus
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
