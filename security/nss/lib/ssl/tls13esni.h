/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13esni_h_
#define __tls13esni_h_

struct sslEsniKeysStr {
    SECItem data; /* The encoded record. */
    sslEphemeralKeyPair *privKey;
    const char *dummySni;
    PRCList keyShares; /* List of TLS13KeyShareEntry */
    SECItem suites;
    PRUint16 paddedLength;
    PRUint64 notBefore;
    PRUint64 notAfter;
};

SECStatus SSLExp_SetESNIKeyPair(PRFileDesc *fd,
                                SECKEYPrivateKey *privKey,
                                const PRUint8 *record, unsigned int recordLen);

SECStatus SSLExp_EnableESNI(PRFileDesc *fd, const PRUint8 *esniKeys,
                            unsigned int esniKeysLen, const char *dummySNI);
SECStatus SSLExp_EncodeESNIKeys(PRUint16 *cipherSuites, unsigned int cipherSuiteCount,
                                SSLNamedGroup group, SECKEYPublicKey *pubKey,
                                PRUint16 pad, PRUint64 notBefore, PRUint64 notAfter,
                                PRUint8 *out, unsigned int *outlen, unsigned int maxlen);
sslEsniKeys *tls13_CopyESNIKeys(sslEsniKeys *okeys);
void tls13_DestroyESNIKeys(sslEsniKeys *keys);
SECStatus tls13_ClientSetupESNI(sslSocket *ss);
SECStatus tls13_ComputeESNIKeys(const sslSocket *ss,
                                TLS13KeyShareEntry *entry,
                                sslKeyPair *keyPair,
                                const ssl3CipherSuiteDef *suite,
                                const PRUint8 *esniKeysHash,
                                const PRUint8 *keyShareBuf,
                                unsigned int keyShareBufLen,
                                const PRUint8 *clientRandom,
                                ssl3KeyMaterial *keyMat);
SECStatus tls13_FormatEsniAADInput(sslBuffer *aadInput,
                                   PRUint8 *keyShare, unsigned int keyShareLen);

SECStatus tls13_ServerDecryptEsniXtn(const sslSocket *ss, const PRUint8 *in, unsigned int inLen,
                                     PRUint8 *out, unsigned int *outLen, unsigned int maxLen);

#endif
