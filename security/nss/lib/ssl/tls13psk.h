/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13psk_h_
#define __tls13psk_h_

/*
 * Internally, we have track sslPsk pointers in three locations:
 * 1) An external PSK can be configured to the socket, in which case ss->psk will hold an owned reference.
 *    For now, this only holds one external PSK. The value will persist across handshake restarts.
 * 2) When a handshake begins, the ss->psk value is deep-copied into ss->ssl3.hs.psks, which may also hold
 *    a resumption PSK. This is essentially a priority-sorted list (where a resumption PSK has higher
 *    priority than external), and we currently only send one PskIdentity and binder.
 * 3) During negotiation, ss->xtnData.selectedPsk will either be NULL or it will hold a non-owning refernce
 *    to the PSK that has been (or is being) negotiated.
 */

/* Note: When holding a resumption PSK:
 *  1. |hash| comes from the original connection.
 *  2. |label| is ignored: The identity sent in the pre_shared_key_xtn
 *     comes from ss->sec.ci.sid->u.ssl3.locked.sessionTicket.
 */
struct sslPskStr {
    PRCList link;
    PK11SymKey *key;              /* A raw PSK. */
    PK11SymKey *binderKey;        /* The binder key derived from |key|. |key| is NULL after derivation. */
    SSLPskType type;              /* none, resumption, or external. */
    SECItem label;                /* Label (identity) for an external PSK. */
    SSLHashType hash;             /* A hash algorithm associated with a PSK. */
    ssl3CipherSuite zeroRttSuite; /* For EPSKs, an explicitly-configured ciphersuite for 0-Rtt. */
    PRUint32 maxEarlyData;        /* For EPSKs, a limit on early data. Must be > 0 for 0-Rtt. */
};

SECStatus SSLExp_AddExternalPsk(PRFileDesc *fd, PK11SymKey *psk, const PRUint8 *identity,
                                unsigned int identitylen, SSLHashType hash);

SECStatus SSLExp_AddExternalPsk0Rtt(PRFileDesc *fd, PK11SymKey *psk, const PRUint8 *identity,
                                    unsigned int identitylen, SSLHashType hash,
                                    PRUint16 zeroRttSuite, PRUint32 maxEarlyData);

SECStatus SSLExp_RemoveExternalPsk(PRFileDesc *fd, const PRUint8 *identity, unsigned int identitylen);

sslPsk *tls13_CopyPsk(sslPsk *opsk);

void tls13_DestroyPsk(sslPsk *psk);

void tls13_DestroyPskList(PRCList *list);

sslPsk *tls13_MakePsk(PK11SymKey *key, SSLPskType pskType, SSLHashType hashType, const SECItem *label);

SECStatus tls13_ResetHandshakePsks(sslSocket *ss, PRCList *list);

#endif
