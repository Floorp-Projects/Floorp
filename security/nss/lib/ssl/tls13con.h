/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13con_h_
#define __tls13con_h_

typedef enum {
    StaticSharedSecret,
    EphemeralSharedSecret
} SharedSecretType;

SECStatus tls13_UnprotectRecord(
    sslSocket *ss, SSL3Ciphertext *cText, sslBuffer *plaintext,
    SSL3AlertDescription *alert);

#if defined(WIN32)
#define __func__ __FUNCTION__
#endif

void tls13_SetHsState(sslSocket *ss, SSL3WaitState ws,
                      const char *func, const char *file, int line);
#define TLS13_SET_HS_STATE(ss, ws) \
    tls13_SetHsState(ss, ws, __func__, __FILE__, __LINE__)

/* Return PR_TRUE if the socket is in one of the given states, else return
 * PR_FALSE. Only call the macro not the function, because the trailing
 * wait_invalid is needed to terminate the argument list. */
PRBool tls13_InHsState(sslSocket *ss, ...);
#define TLS13_IN_HS_STATE(ss, ...) \
    tls13_InHsState(ss, __VA_ARGS__, wait_invalid)

SSLHashType tls13_GetHash(sslSocket *ss);
CK_MECHANISM_TYPE tls13_GetHkdfMechanism(sslSocket *ss);
void tls13_FatalError(sslSocket *ss, PRErrorCode prError,
                      SSL3AlertDescription desc);
SECStatus tls13_SetupClientHello(sslSocket *ss);
SECStatus tls13_MaybeDo0RTTHandshake(sslSocket *ss);
PRBool tls13_AllowPskCipher(const sslSocket *ss,
                            const ssl3CipherSuiteDef *cipher_def);
PRBool tls13_PskSuiteEnabled(sslSocket *ss);
SECStatus tls13_HandleClientHelloPart2(sslSocket *ss,
                                       const SECItem *suites,
                                       sslSessionID *sid);
SECStatus tls13_HandleServerHelloPart2(sslSocket *ss);
SECStatus tls13_HandlePostHelloHandshakeMessage(sslSocket *ss, SSL3Opaque *b,
                                                PRUint32 length,
                                                SSL3Hashes *hashesPtr);
void tls13_DestroyKeyShareEntry(TLS13KeyShareEntry *entry);
void tls13_DestroyKeyShares(PRCList *list);
void tls13_DestroyEarlyData(PRCList *list);
void tls13_CipherSpecAddRef(ssl3CipherSpec *spec);
void tls13_CipherSpecRelease(ssl3CipherSpec *spec);
void tls13_DestroyCipherSpecs(PRCList *list);
PRBool tls13_ExtensionAllowed(PRUint16 extension, SSL3HandshakeType message);
SECStatus tls13_ProtectRecord(sslSocket *ss,
                              ssl3CipherSpec *cwSpec,
                              SSL3ContentType type,
                              const SSL3Opaque *pIn,
                              PRUint32 contentLen,
                              sslBuffer *wrBuf);
PRInt32 tls13_Read0RttData(sslSocket *ss, void *buf, PRInt32 len);
SECStatus tls13_HandleEndOfEarlyData(sslSocket *ss);
SECStatus tls13_HandleEarlyApplicationData(sslSocket *ss, sslBuffer *origBuf);
PRBool tls13_ClientAllow0Rtt(sslSocket *ss, const sslSessionID *sid);

#endif /* __tls13con_h_ */
