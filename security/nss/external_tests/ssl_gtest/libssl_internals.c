/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file contains functions for frobbing the internals of libssl */
#include "libssl_internals.h"

#include "nss.h"
#include "seccomon.h"
#include "ssl.h"
#include "sslimpl.h"

SECStatus
SSLInt_IncrementClientHandshakeVersion(PRFileDesc *fd)
{
    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure;
    }

    ++ss->clientHelloVersion;

    return SECSuccess;
}

PRUint32
SSLInt_DetermineKEABits(PRUint16 serverKeyBits, SSLAuthType authAlgorithm) {
    // For ECDSA authentication we expect a curve for key exchange with the
    // same strength as the one used for the certificate's signature.
    if (authAlgorithm == ssl_auth_ecdsa ||
        authAlgorithm == ssl_auth_ecdh_rsa ||
        authAlgorithm == ssl_auth_ecdh_ecdsa) {
        return serverKeyBits;
    }

    PORT_Assert(authAlgorithm == ssl_auth_rsa_decrypt ||
                authAlgorithm == ssl_auth_rsa_sign);
    PRUint32 minKeaBits;
#ifdef NSS_ECC_MORE_THAN_SUITE_B
    // P-192 is the smallest curve we want to use.
    minKeaBits = 192U;
#else
    // P-256 is the smallest supported curve.
    minKeaBits = 256U;
#endif

    return PR_MAX(SSL_RSASTRENGTH_TO_ECSTRENGTH(serverKeyBits), minKeaBits);
}

/* Use this function to update the ClientRandom of a client's handshake state
 * after replacing its ClientHello message. We for example need to do this
 * when replacing an SSLv3 ClientHello with its SSLv2 equivalent. */
SECStatus
SSLInt_UpdateSSLv2ClientRandom(PRFileDesc *fd, uint8_t *rnd, size_t rnd_len,
                               uint8_t *msg, size_t msg_len)
{
    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure;
    }

    SECStatus rv = ssl3_InitState(ss);
    if (rv != SECSuccess) {
        return rv;
    }

    rv = ssl3_RestartHandshakeHashes(ss);
    if (rv != SECSuccess) {
        return rv;
    }

    // Zero the client_random struct.
    PORT_Memset(&ss->ssl3.hs.client_random, 0, SSL3_RANDOM_LENGTH);

    // Copy over the challenge bytes.
    size_t offset = SSL3_RANDOM_LENGTH - rnd_len;
    PORT_Memcpy(&ss->ssl3.hs.client_random.rand[offset], rnd, rnd_len);

    // Rehash the SSLv2 client hello message.
    return ssl3_UpdateHandshakeHashes(ss, msg, msg_len);
}

PRBool
SSLInt_ExtensionNegotiated(PRFileDesc *fd, PRUint16 ext)
{
    sslSocket *ss = ssl_FindSocket(fd);
    return (PRBool)(ss && ssl3_ExtensionNegotiated(ss, ext));
}

void
SSLInt_ClearSessionTicketKey()
{
  ssl3_SessionTicketShutdown(NULL, NULL);
  NSS_UnregisterShutdown(ssl3_SessionTicketShutdown, NULL);
}

SECStatus
SSLInt_SetMTU(PRFileDesc *fd, PRUint16 mtu)
{
  sslSocket *ss = ssl_FindSocket(fd);
  if (ss) {
    ss->ssl3.mtu = mtu;
    return SECSuccess;
  }
  return SECFailure;
}

PRInt32 SSLInt_CountTls13CipherSpecs(PRFileDesc *fd)
{
  PRCList *cur_p;
  PRInt32 ct = 0;

  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return -1;
  }

  for (cur_p = PR_NEXT_LINK(&ss->ssl3.hs.cipherSpecs);
       cur_p != &ss->ssl3.hs.cipherSpecs;
       cur_p = PR_NEXT_LINK(cur_p)) {
    ++ct;
  }
  return ct;
}

/* Force a timer expiry by backdating when the timer was started.
 * We could set the remaining time to 0 but then backoff would not
 * work properly if we decide to test it. */
void SSLInt_ForceTimerExpiry(PRFileDesc *fd)
{
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return;
  }

  if (!ss->ssl3.hs.rtTimerCb)
    return;

  ss->ssl3.hs.rtTimerStarted = PR_IntervalNow() -
      PR_MillisecondsToInterval(ss->ssl3.hs.rtTimeoutMs + 1);
}
