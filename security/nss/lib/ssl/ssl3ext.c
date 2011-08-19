/*
 * SSL3 Protocol
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dr Vipul Gupta <vipul.gupta@sun.com> and
 *   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
 *   Nagendra Modadugu <ngm@google.com>, Google Inc.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* TLS extension code moved here from ssl3ecc.c */
/* $Id: ssl3ext.c,v 1.16 2011/03/24 01:40:14 alexei.volkov.bugs%sun.com Exp $ */

#include "nssrenam.h"
#include "nss.h"
#include "ssl.h"
#include "sslproto.h"
#include "sslimpl.h"
#include "pk11pub.h"
#include "blapi.h"
#include "prinit.h"

static unsigned char  key_name[SESS_TICKET_KEY_NAME_LEN];
static PK11SymKey    *session_ticket_enc_key_pkcs11 = NULL;
static PK11SymKey    *session_ticket_mac_key_pkcs11 = NULL;

static unsigned char  session_ticket_enc_key[AES_256_KEY_LENGTH];
static unsigned char  session_ticket_mac_key[SHA256_LENGTH];

static PRBool         session_ticket_keys_initialized = PR_FALSE;
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
static SECStatus ssl3_GetSessionTicketKeys(const unsigned char **aes_key,
    PRUint32 *aes_key_length, const unsigned char **mac_key,
    PRUint32 *mac_key_length);
static PRInt32 ssl3_SendRenegotiationInfoXtn(sslSocket * ss,
    PRBool append, PRUint32 maxBytes);
static SECStatus ssl3_HandleRenegotiationInfoXtn(sslSocket *ss, 
    PRUint16 ex_type, SECItem *data);

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
    uint8     b[4];
    uint8 *   p = b;

    switch (lenSize) {
    case 4:
	*p++ = (uint8) (num >> 24);
    case 3:
	*p++ = (uint8) (num >> 16);
    case 2:
	*p++ = (uint8) (num >> 8);
    case 1:
	*p = (uint8) num;
    }
    rv = ssl3_AppendToItem(item, &b[0], lenSize);
    return rv;
}

static SECStatus ssl3_SessionTicketShutdown(void* appData, void* nssData)
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
    SECKEYPrivateKey *svrPrivKey = ss->serverCerts[kt_rsa].SERVERKEY;
    SECKEYPublicKey *svrPubKey = ss->serverCerts[kt_rsa].serverKeyPair->pubKey;

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
	    ssl3_GenerateSessionTicketKeysPKCS11, ss) != PR_SUCCESS)
	return SECFailure;

    if (session_ticket_enc_key_pkcs11 == NULL ||
	session_ticket_mac_key_pkcs11 == NULL)
	return SECFailure;

    *aes_key = session_ticket_enc_key_pkcs11;
    *mac_key = session_ticket_mac_key_pkcs11;
    return SECSuccess;
}

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
	    ssl3_GenerateSessionTicketKeys) != SECSuccess)
	return SECFailure;

    if (!session_ticket_keys_initialized)
	return SECFailure;

    *aes_key = session_ticket_enc_key;
    *aes_key_length = sizeof(session_ticket_enc_key);
    *mac_key = session_ticket_mac_key;
    *mac_key_length = sizeof(session_ticket_mac_key);

    return SECSuccess;
}

/* Table of handlers for received TLS hello extensions, one per extension.
 * In the second generation, this table will be dynamic, and functions
 * will be registered here.
 */
/* This table is used by the server, to handle client hello extensions. */
static const ssl3HelloExtensionHandler clientHelloHandlers[] = {
    { ssl_server_name_xtn,        &ssl3_HandleServerNameXtn },
#ifdef NSS_ENABLE_ECC
    { ssl_elliptic_curves_xtn,    &ssl3_HandleSupportedCurvesXtn },
    { ssl_ec_point_formats_xtn,   &ssl3_HandleSupportedPointFormatsXtn },
#endif
    { ssl_session_ticket_xtn,     &ssl3_ServerHandleSessionTicketXtn },
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { -1, NULL }
};

/* These two tables are used by the client, to handle server hello
 * extensions. */
static const ssl3HelloExtensionHandler serverHelloHandlersTLS[] = {
    { ssl_server_name_xtn,        &ssl3_HandleServerNameXtn },
    /* TODO: add a handler for ssl_ec_point_formats_xtn */
    { ssl_session_ticket_xtn,     &ssl3_ClientHandleSessionTicketXtn },
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { -1, NULL }
};

static const ssl3HelloExtensionHandler serverHelloHandlersSSL3[] = {
    { ssl_renegotiation_info_xtn, &ssl3_HandleRenegotiationInfoXtn },
    { -1, NULL }
};

/* Tables of functions to format TLS hello extensions, one function per
 * extension.
 * These static tables are for the formatting of client hello extensions.
 * The server's table of hello senders is dynamic, in the socket struct,
 * and sender functions are registered there.
 */
static const 
ssl3HelloExtensionSender clientHelloSendersTLS[SSL_MAX_EXTENSIONS] = {
    { ssl_server_name_xtn,        &ssl3_SendServerNameXtn        },
    { ssl_renegotiation_info_xtn, &ssl3_SendRenegotiationInfoXtn },
#ifdef NSS_ENABLE_ECC
    { ssl_elliptic_curves_xtn,    &ssl3_SendSupportedCurvesXtn },
    { ssl_ec_point_formats_xtn,   &ssl3_SendSupportedPointFormatsXtn },
#endif
    { ssl_session_ticket_xtn,     &ssl3_SendSessionTicketXtn }
    /* any extra entries will appear as { 0, NULL }    */
};

static const 
ssl3HelloExtensionSender clientHelloSendersSSL3[SSL_MAX_EXTENSIONS] = {
    { ssl_renegotiation_info_xtn, &ssl3_SendRenegotiationInfoXtn }
    /* any extra entries will appear as { 0, NULL }    */
};

static PRBool
arrayContainsExtension(const PRUint16 *array, PRUint32 len, PRUint16 ex_type)
{
    int i;
    for (i = 0; i < len; i++) {
	if (ex_type == array[i])
	    return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool
ssl3_ExtensionNegotiated(sslSocket *ss, PRUint16 ex_type) {
    TLSExtensionData *xtnData = &ss->xtnData;
    return arrayContainsExtension(xtnData->negotiated,
	                          xtnData->numNegotiated, ex_type);
}

static PRBool
ssl3_ClientExtensionAdvertised(sslSocket *ss, PRUint16 ex_type) {
    TLSExtensionData *xtnData = &ss->xtnData;
    return arrayContainsExtension(xtnData->advertised,
	                          xtnData->numAdvertised, ex_type);
}

/* Format an SNI extension, using the name from the socket's URL,
 * unless that name is a dotted decimal string.
 * Used by client and server.
 */
PRInt32
ssl3_SendServerNameXtn(sslSocket * ss, PRBool append,
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
        len  = PORT_Strlen(ss->url);
        if (append && maxBytes >= len + 9) {
            /* extension_type */
            rv = ssl3_AppendHandshakeNumber(ss, ssl_server_name_xtn, 2); 
            if (rv != SECSuccess) return -1;
            /* length of extension_data */
            rv = ssl3_AppendHandshakeNumber(ss, len + 5, 2); 
            if (rv != SECSuccess) return -1;
            /* length of server_name_list */
            rv = ssl3_AppendHandshakeNumber(ss, len + 3, 2);
            if (rv != SECSuccess) return -1;
            /* Name Type (sni_host_name) */
            rv = ssl3_AppendHandshake(ss,       "\0",    1);
            if (rv != SECSuccess) return -1;
            /* HostName (length and value) */
            rv = ssl3_AppendHandshakeVariable(ss, (PRUint8 *)ss->url, len, 2);
            if (rv != SECSuccess) return -1;
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
        if (rv != SECSuccess)  return -1;
        /* length of extension_data */
        rv = ssl3_AppendHandshakeNumber(ss, 0, 2);
        if (rv != SECSuccess) return -1;
    }
    return 4;
}

/* handle an incoming SNI extension, by ignoring it. */
SECStatus
ssl3_HandleServerNameXtn(sslSocket * ss, PRUint16 ex_type, SECItem *data)
{
    SECItem *names = NULL;
    PRUint32 listCount = 0, namesPos = 0, i;
    TLSExtensionData *xtnData = &ss->xtnData;
    SECItem  ldata;
    PRInt32  listLenBytes = 0;

    if (!ss->sec.isServer) {
        /* Verify extension_data is empty. */
        if (data->data || data->len ||
            !ssl3_ExtensionNegotiated(ss, ssl_server_name_xtn)) {
            /* malformed or was not initiated by the client.*/
            return SECFailure;
        }
        return SECSuccess;
    }

    /* Server side - consume client data and register server sender. */
    /* do not parse the data if don't have user extension handling function. */
    if (!ss->sniSocketConfig) {
        return SECSuccess;
    }
    /* length of server_name_list */
    listLenBytes = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len); 
    if (listLenBytes == 0 || listLenBytes != data->len) {
        return SECFailure;
    }
    ldata = *data;
    /* Calculate the size of the array.*/
    while (listLenBytes > 0) {
        SECItem litem;
        SECStatus rv;
        PRInt32  type;
        /* Name Type (sni_host_name) */
        type = ssl3_ConsumeHandshakeNumber(ss, 1, &ldata.data, &ldata.len); 
        if (!ldata.len) {
            return SECFailure;
        }
        rv = ssl3_ConsumeHandshakeVariable(ss, &litem, 2, &ldata.data, &ldata.len);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        /* Adjust total length for cunsumed item, item len and type.*/
        listLenBytes -= litem.len + 3;
        if (listLenBytes > 0 && !ldata.len) {
            return SECFailure;
        }
        listCount += 1;
    }
    if (!listCount) {
        return SECFailure;
    }
    names = PORT_ZNewArray(SECItem, listCount);
    if (!names) {
        return SECFailure;
    }
    for (i = 0;i < listCount;i++) {
        int j;
        PRInt32  type;
        SECStatus rv;
        PRBool nametypePresent = PR_FALSE;
        /* Name Type (sni_host_name) */
        type = ssl3_ConsumeHandshakeNumber(ss, 1, &data->data, &data->len); 
        /* Check if we have such type in the list */
        for (j = 0;j < listCount && names[j].data;j++) {
            if (names[j].type == type) {
                nametypePresent = PR_TRUE;
                break;
            }
        }
        /* HostName (length and value) */
        rv = ssl3_ConsumeHandshakeVariable(ss, &names[namesPos], 2,
                                           &data->data, &data->len);
        if (rv != SECSuccess) {
            goto loser;
        }
        if (nametypePresent == PR_FALSE) {
            namesPos += 1;
        }
    }
    /* Free old and set the new data. */
    if (xtnData->sniNameArr) {
        PORT_Free(ss->xtnData.sniNameArr);
    }
    xtnData->sniNameArr = names;
    xtnData->sniNameArrSize = namesPos;
    xtnData->negotiated[xtnData->numNegotiated++] = ssl_server_name_xtn;

    return SECSuccess;

loser:
    PORT_Free(names);
    return SECFailure;
}
        
/* Called by both clients and servers.
 * Clients sends a filled in session ticket if one is available, and otherwise
 * sends an empty ticket.  Servers always send empty tickets.
 */
PRInt32
ssl3_SendSessionTicketXtn(
			sslSocket * ss,
			PRBool      append,
			PRUint32    maxBytes)
{
    PRInt32 extension_length;
    NewSessionTicket *session_ticket = NULL;

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
	sslSessionID *sid = ss->sec.ci.sid;
	session_ticket = &sid->u.ssl3.sessionTicket;
	if (session_ticket->ticket.data) {
	    if (ss->xtnData.ticketTimestampVerified) {
		extension_length += session_ticket->ticket.len;
	    } else if (!append &&
		(session_ticket->ticket_lifetime_hint == 0 ||
		(session_ticket->ticket_lifetime_hint +
		    session_ticket->received_timestamp > ssl_Time()))) {
		extension_length += session_ticket->ticket.len;
		ss->xtnData.ticketTimestampVerified = PR_TRUE;
	    }
	}
    }

    if (append && maxBytes >= extension_length) {
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
    } else if (maxBytes < extension_length) {
	PORT_Assert(0);
        return 0;
    }
    return extension_length;

 loser:
    ss->xtnData.ticketTimestampVerified = PR_FALSE;
    return -1;
}

/*
 * NewSessionTicket
 * Called from ssl3_HandleFinished
 */
SECStatus
ssl3_SendNewSessionTicket(sslSocket *ss)
{
    int                  i;
    SECStatus            rv;
    NewSessionTicket     ticket;
    SECItem              plaintext;
    SECItem              plaintext_item = {0, NULL, 0};
    SECItem              ciphertext     = {0, NULL, 0};
    PRUint32             ciphertext_length;
    PRBool               ms_is_wrapped;
    unsigned char        wrapped_ms[SSL3_MASTER_SECRET_LENGTH];
    SECItem              ms_item = {0, NULL, 0};
    SSL3KEAType          effectiveExchKeyType = ssl_kea_null;
    PRUint32             padding_length;
    PRUint32             message_length;
    PRUint32             cert_length;
    uint8                length_buf[4];
    PRUint32             now;
    PK11SymKey          *aes_key_pkcs11;
    PK11SymKey          *mac_key_pkcs11;
    const unsigned char *aes_key;
    const unsigned char *mac_key;
    PRUint32             aes_key_length;
    PRUint32             mac_key_length;
    PRUint64             aes_ctx_buf[MAX_CIPHER_CONTEXT_LLONGS];
    AESContext          *aes_ctx;
    CK_MECHANISM_TYPE    cipherMech = CKM_AES_CBC;
    PK11Context         *aes_ctx_pkcs11;
    const SECHashObject *hashObj = NULL;
    PRUint64             hmac_ctx_buf[MAX_MAC_CONTEXT_LLONGS];
    HMACContext         *hmac_ctx;
    CK_MECHANISM_TYPE    macMech = CKM_SHA256_HMAC;
    PK11Context         *hmac_ctx_pkcs11;
    unsigned char        computed_mac[TLS_EX_SESS_TICKET_MAC_LENGTH];
    unsigned int         computed_mac_length;
    unsigned char        iv[AES_BLOCK_SIZE];
    SECItem              ivItem;
    SECItem             *srvName = NULL;
    PRUint32             srvNameLen = 0;
    CK_MECHANISM_TYPE    msWrapMech = 0; /* dummy default value,
                                          * must be >= 0 */

    SSL_TRC(3, ("%d: SSL3[%d]: send session_ticket handshake",
		SSL_GETPID(), ss->fd));

    PORT_Assert( ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert( ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    ticket.ticket_lifetime_hint = TLS_EX_SESS_TICKET_LIFETIME_HINT;
    cert_length = (ss->opt.requestCertificate && ss->sec.ci.sid->peerCert) ?
	3 + ss->sec.ci.sid->peerCert->derCert.len : 0;

    /* Get IV and encryption keys */
    ivItem.data = iv;
    ivItem.len = sizeof(iv);
    rv = PK11_GenerateRandom(iv, sizeof(iv));
    if (rv != SECSuccess) goto loser;

    if (ss->opt.bypassPKCS11) {
	rv = ssl3_GetSessionTicketKeys(&aes_key, &aes_key_length,
	    &mac_key, &mac_key_length);
    } else {
	rv = ssl3_GetSessionTicketKeysPKCS11(ss, &aes_key_pkcs11,
	    &mac_key_pkcs11);
    }
    if (rv != SECSuccess) goto loser;

    if (ss->ssl3.pwSpec->msItem.len && ss->ssl3.pwSpec->msItem.data) {
	/* The master secret is available unwrapped. */
	ms_item.data = ss->ssl3.pwSpec->msItem.data;
	ms_item.len = ss->ssl3.pwSpec->msItem.len;
	ms_is_wrapped = PR_FALSE;
    } else {
	/* Extract the master secret wrapped. */
	sslSessionID sid;
	PORT_Memset(&sid, 0, sizeof(sslSessionID));

	if (ss->ssl3.hs.kea_def->kea == kea_ecdhe_rsa) {
	    effectiveExchKeyType = kt_rsa;
	} else {
	    effectiveExchKeyType = ss->ssl3.hs.kea_def->exchKeyType;
	}

	rv = ssl3_CacheWrappedMasterSecret(ss, &sid, ss->ssl3.pwSpec,
	    effectiveExchKeyType);
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
    srvName = &ss->ssl3.pwSpec->srvVirtName;
    if (srvName->data && srvName->len) {
        srvNameLen = 2 + srvName->len; /* len bytes + name len */
    }

    ciphertext_length = 
	sizeof(PRUint16)                     /* ticket_version */
	+ sizeof(SSL3ProtocolVersion)        /* ssl_version */
	+ sizeof(ssl3CipherSuite)            /* ciphersuite */
	+ 1                                  /* compression */
	+ 10                                 /* cipher spec parameters */
	+ 1                                  /* SessionTicket.ms_is_wrapped */
	+ 1                                  /* effectiveExchKeyType */
	+ 4                                  /* msWrapMech */
	+ 2                                  /* master_secret.length */
	+ ms_item.len                        /* master_secret */
	+ 1                                  /* client_auth_type */
	+ cert_length                        /* cert */
        + 1                                  /* server name type */
        + srvNameLen                         /* name len + length field */
	+ sizeof(ticket.ticket_lifetime_hint);
    padding_length =  AES_BLOCK_SIZE -
	(ciphertext_length % AES_BLOCK_SIZE);
    ciphertext_length += padding_length;

    message_length =
	sizeof(ticket.ticket_lifetime_hint)    /* ticket_lifetime_hint */
	+ 2 /* length field for NewSessionTicket.ticket */
	+ SESS_TICKET_KEY_NAME_LEN             /* key_name */
	+ AES_BLOCK_SIZE                       /* iv */
	+ 2 /* length field for NewSessionTicket.ticket.encrypted_state */
	+ ciphertext_length                    /* encrypted_state */
	+ TLS_EX_SESS_TICKET_MAC_LENGTH;       /* mac */

    if (SECITEM_AllocItem(NULL, &plaintext_item, ciphertext_length) == NULL)
	goto loser;

    plaintext = plaintext_item;

    /* ticket_version */
    rv = ssl3_AppendNumberToItem(&plaintext, TLS_EX_SESS_TICKET_VERSION,
	sizeof(PRUint16));
    if (rv != SECSuccess) goto loser;

    /* ssl_version */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->version,
	sizeof(SSL3ProtocolVersion));
    if (rv != SECSuccess) goto loser;

    /* ciphersuite */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->ssl3.hs.cipher_suite, 
	sizeof(ssl3CipherSuite));
    if (rv != SECSuccess) goto loser;
    
    /* compression */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->ssl3.hs.compression, 1);
    if (rv != SECSuccess) goto loser;

    /* cipher spec parameters */
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.authAlgorithm, 1);
    if (rv != SECSuccess) goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.authKeyBits, 4);
    if (rv != SECSuccess) goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.keaType, 1);
    if (rv != SECSuccess) goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ss->sec.keaKeyBits, 4);
    if (rv != SECSuccess) goto loser;

    /* master_secret */
    rv = ssl3_AppendNumberToItem(&plaintext, ms_is_wrapped, 1);
    if (rv != SECSuccess) goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, effectiveExchKeyType, 1);
    if (rv != SECSuccess) goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, msWrapMech, 4);
    if (rv != SECSuccess) goto loser;
    rv = ssl3_AppendNumberToItem(&plaintext, ms_item.len, 2);
    if (rv != SECSuccess) goto loser;
    rv = ssl3_AppendToItem(&plaintext, ms_item.data, ms_item.len);
    if (rv != SECSuccess) goto loser;

    /* client_identity */
    if (ss->opt.requestCertificate && ss->sec.ci.sid->peerCert) {
	rv = ssl3_AppendNumberToItem(&plaintext, CLIENT_AUTH_CERTIFICATE, 1);
	if (rv != SECSuccess) goto loser;
	rv = ssl3_AppendNumberToItem(&plaintext,
	    ss->sec.ci.sid->peerCert->derCert.len, 3);
	if (rv != SECSuccess) goto loser;
	rv = ssl3_AppendToItem(&plaintext,
	    ss->sec.ci.sid->peerCert->derCert.data,
	    ss->sec.ci.sid->peerCert->derCert.len);
	if (rv != SECSuccess) goto loser;
    } else {
	rv = ssl3_AppendNumberToItem(&plaintext, 0, 1);
	if (rv != SECSuccess) goto loser;
    }

    /* timestamp */
    now = ssl_Time();
    rv = ssl3_AppendNumberToItem(&plaintext, now,
	sizeof(ticket.ticket_lifetime_hint));
    if (rv != SECSuccess) goto loser;

    if (srvNameLen) {
        /* Name Type (sni_host_name) */
        rv = ssl3_AppendNumberToItem(&plaintext, srvName->type, 1);
        if (rv != SECSuccess) goto loser;
        /* HostName (length and value) */
        rv = ssl3_AppendNumberToItem(&plaintext, srvName->len, 2);
        if (rv != SECSuccess) goto loser;
        rv = ssl3_AppendToItem(&plaintext, srvName->data, srvName->len);
        if (rv != SECSuccess) goto loser;
    } else {
        /* No Name */
        rv = ssl3_AppendNumberToItem(&plaintext, (char)TLS_STE_NO_SERVER_NAME,
                                     1);
        if (rv != SECSuccess) goto loser;
    }

    PORT_Assert(plaintext.len == padding_length);
    for (i = 0; i < padding_length; i++)
	plaintext.data[i] = (unsigned char)padding_length;

    if (SECITEM_AllocItem(NULL, &ciphertext, ciphertext_length) == NULL) {
	rv = SECFailure;
	goto loser;
    }

    /* Generate encrypted portion of ticket. */
    if (ss->opt.bypassPKCS11) {
	aes_ctx = (AESContext *)aes_ctx_buf;
	rv = AES_InitContext(aes_ctx, aes_key, aes_key_length, iv, 
	    NSS_AES_CBC, 1, AES_BLOCK_SIZE);
	if (rv != SECSuccess) goto loser;

	rv = AES_Encrypt(aes_ctx, ciphertext.data, &ciphertext.len,
	    ciphertext.len, plaintext_item.data,
	    plaintext_item.len);
	if (rv != SECSuccess) goto loser;
    } else {
	aes_ctx_pkcs11 = PK11_CreateContextBySymKey(cipherMech,
	    CKA_ENCRYPT, aes_key_pkcs11, &ivItem);
	if (!aes_ctx_pkcs11) 
	    goto loser;

	rv = PK11_CipherOp(aes_ctx_pkcs11, ciphertext.data,
	    (int *)&ciphertext.len, ciphertext.len,
	    plaintext_item.data, plaintext_item.len);
	PK11_Finalize(aes_ctx_pkcs11);
	PK11_DestroyContext(aes_ctx_pkcs11, PR_TRUE);
	if (rv != SECSuccess) goto loser;
    }

    /* Convert ciphertext length to network order. */
    length_buf[0] = (ciphertext.len >> 8) & 0xff;
    length_buf[1] = (ciphertext.len     ) & 0xff;

    /* Compute MAC. */
    if (ss->opt.bypassPKCS11) {
	hmac_ctx = (HMACContext *)hmac_ctx_buf;
	hashObj = HASH_GetRawHashObject(HASH_AlgSHA256);
	if (HMAC_Init(hmac_ctx, hashObj, mac_key,
		mac_key_length, PR_FALSE) != SECSuccess)
	    goto loser;

	HMAC_Begin(hmac_ctx);
	HMAC_Update(hmac_ctx, key_name, SESS_TICKET_KEY_NAME_LEN);
	HMAC_Update(hmac_ctx, iv, sizeof(iv));
	HMAC_Update(hmac_ctx, (unsigned char *)length_buf, 2);
	HMAC_Update(hmac_ctx, ciphertext.data, ciphertext.len);
	HMAC_Finish(hmac_ctx, computed_mac, &computed_mac_length,
	    sizeof(computed_mac));
    } else {
	SECItem macParam;
	macParam.data = NULL;
	macParam.len = 0;
	hmac_ctx_pkcs11 = PK11_CreateContextBySymKey(macMech,
	    CKA_SIGN, mac_key_pkcs11, &macParam);
	if (!hmac_ctx_pkcs11)
	    goto loser;

	rv = PK11_DigestBegin(hmac_ctx_pkcs11);
	rv = PK11_DigestOp(hmac_ctx_pkcs11, key_name,
	    SESS_TICKET_KEY_NAME_LEN);
	rv = PK11_DigestOp(hmac_ctx_pkcs11, iv, sizeof(iv));
	rv = PK11_DigestOp(hmac_ctx_pkcs11, (unsigned char *)length_buf, 2);
	rv = PK11_DigestOp(hmac_ctx_pkcs11, ciphertext.data, ciphertext.len);
	rv = PK11_DigestFinal(hmac_ctx_pkcs11, computed_mac,
	    &computed_mac_length, sizeof(computed_mac));
	PK11_DestroyContext(hmac_ctx_pkcs11, PR_TRUE);
	if (rv != SECSuccess) goto loser;
    }

    /* Serialize the handshake message. */
    rv = ssl3_AppendHandshakeHeader(ss, new_session_ticket, message_length);
    if (rv != SECSuccess) goto loser;

    rv = ssl3_AppendHandshakeNumber(ss, ticket.ticket_lifetime_hint,
	sizeof(ticket.ticket_lifetime_hint));
    if (rv != SECSuccess) goto loser;

    rv = ssl3_AppendHandshakeNumber(ss,
	message_length - sizeof(ticket.ticket_lifetime_hint) - 2, 2);
    if (rv != SECSuccess) goto loser;

    rv = ssl3_AppendHandshake(ss, key_name, SESS_TICKET_KEY_NAME_LEN);
    if (rv != SECSuccess) goto loser;

    rv = ssl3_AppendHandshake(ss, iv, sizeof(iv));
    if (rv != SECSuccess) goto loser;

    rv = ssl3_AppendHandshakeVariable(ss, ciphertext.data, ciphertext.len, 2);
    if (rv != SECSuccess) goto loser;

    rv = ssl3_AppendHandshake(ss, computed_mac, computed_mac_length);
    if (rv != SECSuccess) goto loser;

loser:
    if (plaintext_item.data)
	SECITEM_FreeItem(&plaintext_item, PR_FALSE);
    if (ciphertext.data)
	SECITEM_FreeItem(&ciphertext, PR_FALSE);

    return rv;
}

/* When a client receives a SessionTicket extension a NewSessionTicket
 * message is expected during the handshake.
 */
SECStatus
ssl3_ClientHandleSessionTicketXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    if (data->len != 0)
	return SECFailure;

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;
    return SECSuccess;
}

SECStatus
ssl3_ServerHandleSessionTicketXtn(sslSocket *ss, PRUint16 ex_type,
                                  SECItem *data)
{
    SECStatus rv;
    SECItem *decrypted_state = NULL;
    SessionTicket *parsed_session_ticket = NULL;
    sslSessionID *sid = NULL;
    SSL3Statistics *ssl3stats;

    /* Ignore the SessionTicket extension if processing is disabled. */
    if (!ss->opt.enableSessionTickets)
	return SECSuccess;

    /* Keep track of negotiated extensions. */
    ss->xtnData.negotiated[ss->xtnData.numNegotiated++] = ex_type;

    /* Parse the received ticket sent in by the client.  We are
     * lenient about some parse errors, falling back to a fullshake
     * instead of terminating the current connection.
     */
    if (data->len == 0) {
	ss->xtnData.emptySessionTicket = PR_TRUE;
    } else {
	int                    i;
	SECItem                extension_data;
	EncryptedSessionTicket enc_session_ticket;
	unsigned char          computed_mac[TLS_EX_SESS_TICKET_MAC_LENGTH];
	unsigned int           computed_mac_length;
	const SECHashObject   *hashObj;
	const unsigned char   *aes_key;
	const unsigned char   *mac_key;
	PK11SymKey            *aes_key_pkcs11;
	PK11SymKey            *mac_key_pkcs11;
	PRUint32               aes_key_length;
	PRUint32               mac_key_length;
	PRUint64               hmac_ctx_buf[MAX_MAC_CONTEXT_LLONGS];
	HMACContext           *hmac_ctx;
	PK11Context           *hmac_ctx_pkcs11;
	CK_MECHANISM_TYPE      macMech = CKM_SHA256_HMAC;
	PRUint64               aes_ctx_buf[MAX_CIPHER_CONTEXT_LLONGS];
	AESContext            *aes_ctx;
	PK11Context           *aes_ctx_pkcs11;
	CK_MECHANISM_TYPE      cipherMech = CKM_AES_CBC;
	unsigned char *        padding;
	PRUint32               padding_length;
	unsigned char         *buffer;
	unsigned int           buffer_len;
	PRInt32                temp;
	SECItem                cert_item;
        PRInt8                 nameType = TLS_STE_NO_SERVER_NAME;

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

	if (ssl3_ParseEncryptedSessionTicket(ss, data, &enc_session_ticket)
	    != SECSuccess)
	    return SECFailure;

	/* Get session ticket keys. */
	if (ss->opt.bypassPKCS11) {
	    rv = ssl3_GetSessionTicketKeys(&aes_key, &aes_key_length,
		&mac_key, &mac_key_length);
	} else {
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
	if (ss->opt.bypassPKCS11) {
	    hmac_ctx = (HMACContext *)hmac_ctx_buf;
	    hashObj = HASH_GetRawHashObject(HASH_AlgSHA256);
	    if (HMAC_Init(hmac_ctx, hashObj, mac_key,
		    sizeof(session_ticket_mac_key), PR_FALSE) != SECSuccess)
		goto no_ticket;
	    HMAC_Begin(hmac_ctx);
	    HMAC_Update(hmac_ctx, extension_data.data,
		extension_data.len - TLS_EX_SESS_TICKET_MAC_LENGTH);
	    if (HMAC_Finish(hmac_ctx, computed_mac, &computed_mac_length,
		    sizeof(computed_mac)) != SECSuccess)
		goto no_ticket;
	} else {
	    SECItem macParam;
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
	    rv = PK11_DigestOp(hmac_ctx_pkcs11, extension_data.data,
		extension_data.len - TLS_EX_SESS_TICKET_MAC_LENGTH);
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
		computed_mac_length) != 0) {
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

	if (ss->opt.bypassPKCS11) {
	    aes_ctx = (AESContext *)aes_ctx_buf;
	    rv = AES_InitContext(aes_ctx, aes_key,
		sizeof(session_ticket_enc_key), enc_session_ticket.iv,
		NSS_AES_CBC, 0,AES_BLOCK_SIZE);
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
	} else {
	    SECItem ivItem;
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

	/* Read ticket_version (which is ignored for now.) */
	temp = ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->ticket_version = (SSL3ProtocolVersion)temp;

	/* Read SSLVersion. */
	temp = ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->ssl_version = (SSL3ProtocolVersion)temp;

	/* Read cipher_suite. */
	temp =  ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->cipher_suite = (ssl3CipherSuite)temp;

	/* Read compression_method. */
	temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->compression_method = (SSLCompressionMethod)temp;

	/* Read cipher spec parameters. */
	temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->authAlgorithm = (SSLSignType)temp;
	temp = ssl3_ConsumeHandshakeNumber(ss, 4, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->authKeyBits = (PRUint32)temp;
	temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->keaType = (SSLKEAType)temp;
	temp = ssl3_ConsumeHandshakeNumber(ss, 4, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->keaKeyBits = (PRUint32)temp;

	/* Read wrapped master_secret. */
	temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->ms_is_wrapped = (PRBool)temp;

	temp = ssl3_ConsumeHandshakeNumber(ss, 1, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->exchKeyType = (SSL3KEAType)temp;

	temp = ssl3_ConsumeHandshakeNumber(ss, 4, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->msWrapMech = (CK_MECHANISM_TYPE)temp;

	temp = ssl3_ConsumeHandshakeNumber(ss, 2, &buffer, &buffer_len);
	if (temp < 0) goto no_ticket;
	parsed_session_ticket->ms_length = (PRUint16)temp;
	if (parsed_session_ticket->ms_length == 0 ||  /* sanity check MS. */
	    parsed_session_ticket->ms_length >
	    sizeof(parsed_session_ticket->master_secret))
	    goto no_ticket;
	
	/* Allow for the wrapped master secret to be longer. */
	if (buffer_len < sizeof(SSL3_MASTER_SECRET_LENGTH))
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
	switch(parsed_session_ticket->client_identity.client_auth_type) {
            case CLIENT_AUTH_ANONYMOUS:
		break;
            case CLIENT_AUTH_CERTIFICATE:
		rv = ssl3_ConsumeHandshakeVariable(ss, &cert_item, 3,
		    &buffer, &buffer_len);
		if (rv != SECSuccess) goto no_ticket;
		rv = SECITEM_CopyItem(NULL, &parsed_session_ticket->peer_cert,
		    &cert_item);
		if (rv != SECSuccess) goto no_ticket;
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
            if (rv != SECSuccess) goto no_ticket;
            rv = SECITEM_CopyItem(NULL, &parsed_session_ticket->srvName,
                                  &name_item);
            if (rv != SECSuccess) goto no_ticket;
            parsed_session_ticket->srvName.type = nameType;
        }

	/* Done parsing.  Check that all bytes have been consumed. */
	if (buffer_len != padding_length)
	    goto no_ticket;

	/* Use the ticket if it has not expired, otherwise free the allocated
	 * memory since the ticket is of no use.
	 */
	if (parsed_session_ticket->timestamp != 0 &&
	    parsed_session_ticket->timestamp +
	    TLS_EX_SESS_TICKET_LIFETIME_HINT > ssl_Time()) {

	    sid = ssl3_NewSessionID(ss, PR_TRUE);
	    if (sid == NULL) {
		rv = SECFailure;
		goto loser;
	    }

	    /* Copy over parameters. */
	    sid->version = parsed_session_ticket->ssl_version;
	    sid->u.ssl3.cipherSuite = parsed_session_ticket->cipher_suite;
	    sid->u.ssl3.compression = parsed_session_ticket->compression_method;
	    sid->authAlgorithm = parsed_session_ticket->authAlgorithm;
	    sid->authKeyBits = parsed_session_ticket->authKeyBits;
	    sid->keaType = parsed_session_ticket->keaType;
	    sid->keaKeyBits = parsed_session_ticket->keaKeyBits;

	    /* Copy master secret. */
	    if (ss->opt.bypassPKCS11 &&
		    parsed_session_ticket->ms_is_wrapped)
		goto no_ticket;
	    if (parsed_session_ticket->ms_length >
		    sizeof(sid->u.ssl3.keys.wrapped_master_secret))
		goto no_ticket;
	    PORT_Memcpy(sid->u.ssl3.keys.wrapped_master_secret,
		parsed_session_ticket->master_secret,
		parsed_session_ticket->ms_length);
	    sid->u.ssl3.keys.wrapped_master_secret_len =
		parsed_session_ticket->ms_length;
	    sid->u.ssl3.exchKeyType = parsed_session_ticket->exchKeyType;
	    sid->u.ssl3.masterWrapMech = parsed_session_ticket->msWrapMech;
	    sid->u.ssl3.keys.msIsWrapped =
		parsed_session_ticket->ms_is_wrapped;
	    sid->u.ssl3.masterValid    = PR_TRUE;
	    sid->u.ssl3.keys.resumable = PR_TRUE;

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
                sid->u.ssl3.srvName = parsed_session_ticket->srvName;
            }
	    ss->statelessResume = PR_TRUE;
	    ss->sec.ci.sid = sid;
	}
    }

    if (0) {
no_ticket:
	SSL_DBG(("%d: SSL[%d]: Session ticket parsing failed.",
			SSL_GETPID(), ss->fd));
	ssl3stats = SSL_GetStatistics();
	SSL_AtomicIncrementLong(& ssl3stats->hch_sid_ticket_parse_failures );
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
	PORT_ZFree(parsed_session_ticket, sizeof(SessionTicket));
    }

    return rv;
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
	    SESS_TICKET_KEY_NAME_LEN) != SECSuccess)
	return SECFailure;
    if (ssl3_ConsumeFromItem(data, &enc_session_ticket->iv,
	    AES_BLOCK_SIZE) != SECSuccess)
	return SECFailure;
    if (ssl3_ConsumeHandshakeVariable(ss, &enc_session_ticket->encrypted_state,
	    2, &data->data, &data->len) != SECSuccess)
	return SECFailure;
    if (ssl3_ConsumeFromItem(data, &enc_session_ticket->mac,
	    TLS_EX_SESS_TICKET_MAC_LENGTH) != SECSuccess)
	return SECFailure;
    if (data->len != 0)  /* Make sure that we have consumed all bytes. */
	return SECFailure;

    return SECSuccess;
}

/* go through hello extensions in buffer "b".
 * For each one, find the extension handler in the table, and 
 * if present, invoke that handler.  
 * Servers ignore any extensions with unknown extension types.
 * Clients reject any extensions with unadvertised extension types.
 */
SECStatus 
ssl3_HandleHelloExtensions(sslSocket *ss, SSL3Opaque **b, PRUint32 *length)
{
    const ssl3HelloExtensionHandler * handlers;

    if (ss->sec.isServer) {
        handlers = clientHelloHandlers;
    } else if (ss->version > SSL_LIBRARY_VERSION_3_0) {
        handlers = serverHelloHandlersTLS;
    } else {
        handlers = serverHelloHandlersSSL3;
    }

    while (*length) {
	const ssl3HelloExtensionHandler * handler;
	SECStatus rv;
	PRInt32   extension_type;
	SECItem   extension_data;

	/* Get the extension's type field */
	extension_type = ssl3_ConsumeHandshakeNumber(ss, 2, b, length);
	if (extension_type < 0)  /* failure to decode extension_type */
	    return SECFailure;   /* alert already sent */

	/* get the data for this extension, so we can pass it or skip it. */
	rv = ssl3_ConsumeHandshakeVariable(ss, &extension_data, 2, b, length);
	if (rv != SECSuccess)
	    return rv;

	/* Check whether the server sent an extension which was not advertised
	 * in the ClientHello.
	 */
	if (!ss->sec.isServer &&
	    !ssl3_ClientExtensionAdvertised(ss, extension_type))
	    return SECFailure;  /* TODO: send unsupported_extension alert */

	/* Check whether an extension has been sent multiple times. */
	if (ssl3_ExtensionNegotiated(ss, extension_type))
	    return SECFailure;

	/* find extension_type in table of Hello Extension Handlers */
	for (handler = handlers; handler->ex_type >= 0; handler++) {
	    /* if found, call this handler */
	    if (handler->ex_type == extension_type) {
		rv = (*handler->ex_handler)(ss, (PRUint16)extension_type, 
	                                         	&extension_data);
		/* Ignore this result */
		/* Treat all bad extensions as unrecognized types. */
	        break;
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
    ssl3HelloExtensionSender *sender = &ss->xtnData.serverSenders[0];

    for (i = 0; i < SSL_MAX_EXTENSIONS; ++i, ++sender) {
        if (!sender->ex_sender) {
	    sender->ex_type   = ex_type;
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
    	sender = ss->version > SSL_LIBRARY_VERSION_3_0 ?
                 &clientHelloSendersTLS[0] : &clientHelloSendersSSL3[0];
    }

    for (i = 0; i < SSL_MAX_EXTENSIONS; ++i, ++sender) {
	if (sender->ex_sender) {
	    PRInt32 extLen = (*sender->ex_sender)(ss, append, maxBytes);
	    if (extLen < 0)
	    	return -1;
	    maxBytes        -= extLen;
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
			sslSocket * ss,
			PRBool      append,
			PRUint32    maxBytes)
{
    PRInt32 len, needed;

    /* In draft-ietf-tls-renegotiation-03, it is NOT RECOMMENDED to send
     * both the SCSV and the empty RI, so when we send SCSV in 
     * the initial handshake, we don't also send RI.
     */
    if (!ss || ss->ssl3.hs.sendingSCSV)
    	return 0;
    len = !ss->firstHsDone ? 0 : 
	   (ss->sec.isServer ? ss->ssl3.hs.finishedBytes * 2 
			     : ss->ssl3.hs.finishedBytes);
    needed = 5 + len;
    if (append && maxBytes >= needed) {
	SECStatus rv;
	/* extension_type */
	rv = ssl3_AppendHandshakeNumber(ss, ssl_renegotiation_info_xtn, 2); 
	if (rv != SECSuccess) return -1;
	/* length of extension_data */
	rv = ssl3_AppendHandshakeNumber(ss, len + 1, 2); 
	if (rv != SECSuccess) return -1;
	/* verify_Data from previous Finished message(s) */
	rv = ssl3_AppendHandshakeVariable(ss, 
		  ss->ssl3.hs.finishedMsgs.data, len, 1);
	if (rv != SECSuccess) return -1;
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
    if (data->len != 1 + len  ||
	data->data[0] != len  || (len && 
	NSS_SecureMemcmp(ss->ssl3.hs.finishedMsgs.data,
	                 data->data + 1, len))) {
	/* Can we do this here? Or, must we arrange for the caller to do it? */     
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

