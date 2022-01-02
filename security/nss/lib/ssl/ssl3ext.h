/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ssl3ext_h_
#define __ssl3ext_h_

#include "pk11hpke.h"
#include "sslencode.h"

typedef enum {
    sni_nametype_hostname
} SNINameType;
typedef struct TLSExtensionDataStr TLSExtensionData;

/* Registerable callback function that either appends extension to buffer
 * or returns length of data that it would have appended.
 */
typedef SECStatus (*sslExtensionBuilderFunc)(const sslSocket *ss,
                                             TLSExtensionData *xtnData,
                                             sslBuffer *buf, PRBool *added);

/* row in a table of hello extension senders */
typedef struct {
    PRInt32 ex_type;
    sslExtensionBuilderFunc ex_sender;
} sslExtensionBuilder;

struct TLSExtensionDataStr {
    /* registered callbacks that send server hello extensions */
    sslExtensionBuilder serverHelloSenders[SSL_MAX_EXTENSIONS];
    sslExtensionBuilder encryptedExtensionsSenders[SSL_MAX_EXTENSIONS];
    sslExtensionBuilder certificateSenders[SSL_MAX_EXTENSIONS];

    /* Keep track of the extensions that are advertised or negotiated. */
    PRUint16 numAdvertised;
    PRUint16 *advertised; /* Allocated dynamically. */
    PRUint16 numNegotiated;
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
     * extension (if any) the other side supports. This is only valid for TLS
     * 1.2 or later. In TLS 1.3, it is also used for CertificateRequest. */
    SSLSignatureScheme *sigSchemes;
    unsigned int numSigSchemes;

    /* Keep track of signature schemes that the remote peer supports for
     * Delegated Credentials signatures, as well was those we have
     * advertised (for purposes of validating any received DC).
     * This list may not be the same as those supported for certificates.
     * Only valid for TLS 1.3. */
    SSLSignatureScheme *delegCredSigSchemes;
    unsigned int numDelegCredSigSchemes;
    SSLSignatureScheme *delegCredSigSchemesAdvertised;
    unsigned int numDelegCredSigSchemesAdvertised;

    SECItem certReqContext;
    CERTDistNames certReqAuthorities;

    /* In a client: if the server supports Next Protocol Negotiation, then
     * this is the protocol that was negotiated.
     */
    SECItem nextProto;
    SSLNextProtoState nextProtoState;

    PRUint16 dtlsSRTPCipherSuite; /* 0 if not selected */

    unsigned int lastXtnOffset; /* Where to insert any other extensions.
                                 * 0 = end, otherwise base of PSK xtn. */
    PRCList remoteKeyShares;    /* The other side's public keys (TLS 1.3) */

    /* The following are used by a TLS 1.3 server. */
    SECItem pskBinder;                     /* The binder for the first PSK. */
    unsigned int pskBindersLen;            /* The length of the binders. */
    PRUint32 ticketAge;                    /* Used to accept early data. */
    SECItem cookie;                        /* HRR Cookie. */
    const sslNamedGroupDef *selectedGroup; /* For HRR. */
    /* The application token contains a value that was passed to the client via
     * a session ticket, or the cookie in a HelloRetryRequest. */
    SECItem applicationToken;

    /* The record size limit set by the peer. Our value is kept in ss->opt. */
    PRUint16 recordSizeLimit;

    /* Delegated credentials.
     *
     * The delegated credential sent by the peer. Set by
     * |tls13_ReadDelegatedCredential|.
     */
    sslDelegatedCredential *peerDelegCred;
    /* Whether the peer requested a delegated credential. */
    PRBool peerRequestedDelegCred;
    /* Whether the host is committed to using a delegated credential. Set by
     * |tls13_MaybeSetDelegatedCredential|.
     */
    PRBool sendingDelegCredToPeer;

    /* A non-owning reference to the selected PSKs. MUST NOT be freed directly,
     * rather through tls13_DestoryPskList(). */
    sslPsk *selectedPsk;

    /* ECH working state. Non-null when a valid Encrypted Client Hello extension
     * was received. */
    sslEchXtnState *ech;
};

typedef struct TLSExtensionStr {
    PRCList link;  /* The linked list link */
    PRUint16 type; /* Extension type */
    SECItem data;  /* Pointers into the handshake data. */
} TLSExtension;

typedef struct sslCustomExtensionHooks {
    PRCList link;
    PRUint16 type;
    SSLExtensionWriter writer;
    void *writerArg;
    SSLExtensionHandler handler;
    void *handlerArg;
} sslCustomExtensionHooks;

SECStatus ssl3_HandleExtensions(sslSocket *ss,
                                PRUint8 **b, PRUint32 *length,
                                SSLHandshakeType handshakeMessage);
SECStatus ssl3_ParseExtensions(sslSocket *ss,
                               PRUint8 **b, PRUint32 *length);
SECStatus ssl3_HandleParsedExtensions(sslSocket *ss,
                                      SSLHandshakeType handshakeMessage);
TLSExtension *ssl3_FindExtension(sslSocket *ss,
                                 SSLExtensionType extension_type);
void ssl3_DestroyRemoteExtensions(PRCList *list);
void ssl3_MoveRemoteExtensions(PRCList *dst, PRCList *src);
void ssl3_InitExtensionData(TLSExtensionData *xtnData, const sslSocket *ss);
void ssl3_DestroyExtensionData(TLSExtensionData *xtnData);
void ssl3_ResetExtensionData(TLSExtensionData *xtnData, const sslSocket *ss);

PRBool ssl3_ExtensionNegotiated(const sslSocket *ss, PRUint16 ex_type);
PRBool ssl3_ExtensionAdvertised(const sslSocket *ss, PRUint16 ex_type);

SECStatus ssl3_RegisterExtensionSender(const sslSocket *ss,
                                       TLSExtensionData *xtnData,
                                       PRUint16 ex_type,
                                       sslExtensionBuilderFunc cb);
SECStatus ssl_ConstructExtensions(sslSocket *ss, sslBuffer *buf,
                                  SSLHandshakeType message);
SECStatus ssl_SendEmptyExtension(const sslSocket *ss, TLSExtensionData *xtnData,
                                 sslBuffer *buf, PRBool *append);
SECStatus ssl3_EmplaceExtension(sslSocket *ss, sslBuffer *buf, PRUint16 exType,
                                const PRUint8 *data, unsigned int len, PRBool advertise);
SECStatus ssl_InsertPaddingExtension(sslSocket *ss, unsigned int prefixLen,
                                     sslBuffer *buf);

/* Thunks to let us operate on const sslSocket* objects. */
void ssl3_ExtSendAlert(const sslSocket *ss, SSL3AlertLevel level,
                       SSL3AlertDescription desc);
void ssl3_ExtDecodeError(const sslSocket *ss);
SECStatus ssl3_ExtConsumeHandshake(const sslSocket *ss, void *v, PRUint32 bytes,
                                   PRUint8 **b, PRUint32 *length);
SECStatus ssl3_ExtConsumeHandshakeNumber(const sslSocket *ss, PRUint32 *num,
                                         PRUint32 bytes, PRUint8 **b,
                                         PRUint32 *length);
SECStatus ssl3_ExtConsumeHandshakeVariable(const sslSocket *ss, SECItem *i,
                                           PRUint32 bytes, PRUint8 **b,
                                           PRUint32 *length);

SECStatus SSLExp_GetExtensionSupport(PRUint16 type,
                                     SSLExtensionSupport *support);
SECStatus SSLExp_InstallExtensionHooks(
    PRFileDesc *fd, PRUint16 extension, SSLExtensionWriter writer,
    void *writerArg, SSLExtensionHandler handler, void *handlerArg);

#endif
