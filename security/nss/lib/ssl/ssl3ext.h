/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ssl3ext_h_
#define __ssl3ext_h_

typedef enum {
    sni_nametype_hostname
} SNINameType;
typedef struct TLSExtensionDataStr TLSExtensionData;

/* registerable callback function that either appends extension to buffer
 * or returns length of data that it would have appended.
 */
typedef PRInt32 (*ssl3HelloExtensionSenderFunc)(const sslSocket *ss,
                                                TLSExtensionData *xtnData,
                                                PRBool append,
                                                PRUint32 maxBytes);

/* registerable callback function that handles a received extension,
 * of the given type.
 */
typedef SECStatus (*ssl3ExtensionHandlerFunc)(const sslSocket *ss,
                                              TLSExtensionData *xtnData,
                                              PRUint16 ex_type,
                                              SECItem *data);

/* row in a table of hello extension senders */
typedef struct {
    PRInt32 ex_type;
    ssl3HelloExtensionSenderFunc ex_sender;
} ssl3HelloExtensionSender;

/* row in a table of hello extension handlers */
typedef struct {
    PRInt32 ex_type;
    ssl3ExtensionHandlerFunc ex_handler;
} ssl3ExtensionHandler;

struct TLSExtensionDataStr {
    /* registered callbacks that send server hello extensions */
    ssl3HelloExtensionSender serverHelloSenders[SSL_MAX_EXTENSIONS];
    ssl3HelloExtensionSender encryptedExtensionsSenders[SSL_MAX_EXTENSIONS];
    ssl3HelloExtensionSender certificateSenders[SSL_MAX_EXTENSIONS];

    /* Keep track of the extensions that are negotiated. */
    PRUint16 numAdvertised;
    PRUint16 numNegotiated;
    PRUint16 advertised[SSL_MAX_EXTENSIONS];
    PRUint16 negotiated[SSL_MAX_EXTENSIONS];

    /* SessionTicket Extension related data. */
    PRBool ticketTimestampVerified;
    PRBool emptySessionTicket;
    PRBool sentSessionTicketInClientHello;
    SECItem psk_ke_modes;
    PRUint32 max_early_data_size;

    /* SNI Extension related data
     * Names data is not coppied from the input buffer. It can not be
     * used outside the scope where input buffer is defined and that
     * is beyond ssl3_HandleClientHello function. */
    SECItem *sniNameArr;
    PRUint32 sniNameArrSize;

    /* Signed Certificate Timestamps extracted from the TLS extension.
     * (client only).
     * This container holds a temporary pointer to the extension data,
     * until a session structure (the sec.ci.sid of an sslSocket) is setup
     * that can hold a permanent copy of the data
     * (in sec.ci.sid.u.ssl3.signedCertTimestamps).
     * The data pointed to by this structure is neither explicitly allocated
     * nor copied: the pointer points to the handshake message buffer and is
     * only valid in the scope of ssl3_HandleServerHello.
     */
    SECItem signedCertTimestamps;

    PRBool peerSupportsFfdheGroups; /* if the peer supports named ffdhe groups */

    /* clientSigAndHash contains the contents of the signature_algorithms
     * extension (if any) from the client. This is only valid for TLS 1.2
     * or later. */
    SSLSignatureScheme *clientSigSchemes;
    unsigned int numClientSigScheme;

    /* In a client: if the server supports Next Protocol Negotiation, then
     * this is the protocol that was negotiated.
     */
    SECItem nextProto;
    SSLNextProtoState nextProtoState;

    PRUint16 dtlsSRTPCipherSuite; /* 0 if not selected */

    SECItem pskBinder;                /* The PSK binder for the first PSK (TLS 1.3) */
    unsigned long pskBinderPrefixLen; /* The length of the binder input. */
    PRCList remoteKeyShares;          /* The other side's public keys (TLS 1.3) */
};

typedef struct TLSExtensionStr {
    PRCList link;  /* The linked list link */
    PRUint16 type; /* Extension type */
    SECItem data;  /* Pointers into the handshake data. */
} TLSExtension;

SECStatus ssl3_HandleExtensions(sslSocket *ss,
                                SSL3Opaque **b, PRUint32 *length,
                                SSL3HandshakeType handshakeMessage);
SECStatus ssl3_ParseExtensions(sslSocket *ss,
                               SSL3Opaque **b, PRUint32 *length);
SECStatus ssl3_HandleParsedExtensions(sslSocket *ss,
                                      SSL3HandshakeType handshakeMessage);
TLSExtension *ssl3_FindExtension(sslSocket *ss,
                                 SSLExtensionType extension_type);
void ssl3_DestroyRemoteExtensions(PRCList *list);
void ssl3_InitExtensionData(TLSExtensionData *xtnData);
void ssl3_ResetExtensionData(TLSExtensionData *xtnData);

PRBool ssl3_ExtensionNegotiated(const sslSocket *ss, PRUint16 ex_type);
PRBool ssl3_ClientExtensionAdvertised(const sslSocket *ss, PRUint16 ex_type);

SECStatus ssl3_RegisterExtensionSender(const sslSocket *ss,
                                       TLSExtensionData *xtnData,
                                       PRUint16 ex_type,
                                       ssl3HelloExtensionSenderFunc cb);
PRInt32 ssl3_CallHelloExtensionSenders(sslSocket *ss, PRBool append, PRUint32 maxBytes,
                                       const ssl3HelloExtensionSender *sender);

unsigned int ssl3_CalculatePaddingExtensionLength(unsigned int clientHelloLength);
PRInt32 ssl3_AppendPaddingExtension(sslSocket *ss, unsigned int extensionLen,
                                    PRUint32 maxBytes);

/* Thunks to let us operate on const sslSocket* objects. */
SECStatus ssl3_ExtAppendHandshake(const sslSocket *ss, const void *void_src,
                                  PRInt32 bytes);
SECStatus ssl3_ExtAppendHandshakeNumber(const sslSocket *ss, PRInt32 num,
                                        PRInt32 lenSize);
SECStatus ssl3_ExtAppendHandshakeVariable(const sslSocket *ss,
                                          const SSL3Opaque *src, PRInt32 bytes,
                                          PRInt32 lenSize);
void ssl3_ExtSendAlert(const sslSocket *ss, SSL3AlertLevel level,
                       SSL3AlertDescription desc);
void ssl3_ExtDecodeError(const sslSocket *ss);
SECStatus ssl3_ExtConsumeHandshake(const sslSocket *ss, void *v, PRInt32 bytes,
                                   SSL3Opaque **b, PRUint32 *length);
PRInt32 ssl3_ExtConsumeHandshakeNumber(const sslSocket *ss, PRInt32 bytes,
                                       SSL3Opaque **b, PRUint32 *length);
SECStatus ssl3_ExtConsumeHandshakeVariable(const sslSocket *ss, SECItem *i,
                                           PRInt32 bytes, SSL3Opaque **b,
                                           PRUint32 *length);

#endif
