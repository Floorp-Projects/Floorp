/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 tw=80 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server for testing client cert authentication.
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the certificate.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <stdio.h>

#include "hasht.h"
#include "ScopedNSSTypes.h"
#include "ssl.h"
#include "TLSServer.h"

using namespace mozilla;
using namespace mozilla::test;

struct ClientAuthHost
{
  const char *mHostName;
  bool mRequestClientAuth;
  bool mRequireClientAuth;
};

// Hostname, cert nickname pairs.
static const ClientAuthHost sClientAuthHosts[] =
{
  { "noclientauth.example.com", false, false },
  { "requestclientauth.example.com", true, false },
  { "requireclientauth.example.com", true, true },
  { nullptr, false, false }
};

static const unsigned char sClientCertFingerprint[] =
{
  0xD2, 0x2F, 0x00, 0x9A, 0x9E, 0xED, 0x79, 0xDC,
  0x8D, 0x17, 0x98, 0x8E, 0xEC, 0x76, 0x05, 0x91,
  0xA5, 0xF6, 0xC9, 0xFA, 0x16, 0x8B, 0xD2, 0x5F,
  0xE1, 0x52, 0x04, 0x7C, 0xF4, 0x76, 0x42, 0x9D
};

SECStatus
AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checkSig,
                    PRBool isServer)
{
  ScopedCERTCertificate clientCert(SSL_PeerCertificate(fd));

  unsigned char certFingerprint[SHA256_LENGTH];
  SECStatus rv = PK11_HashBuf(SEC_OID_SHA256, certFingerprint,
                              clientCert->derCert.data,
                              clientCert->derCert.len);
  if (rv != SECSuccess) {
    return rv;
  }

  static_assert(sizeof(sClientCertFingerprint) == SHA256_LENGTH,
                "Ensure fingerprint has corrent length");
  bool match = !memcmp(certFingerprint, sClientCertFingerprint,
                       sizeof(certFingerprint));
  return match ? SECSuccess : SECFailure;
}

int32_t
DoSNISocketConfig(PRFileDesc* aFd, const SECItem* aSrvNameArr,
                  uint32_t aSrvNameArrSize, void* aArg)
{
  const ClientAuthHost *host = GetHostForSNI(aSrvNameArr, aSrvNameArrSize,
                                             sClientAuthHosts);
  if (!host) {
    return SSL_SNI_SEND_ALERT;
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "found pre-defined host '%s'\n", host->mHostName);
  }

  SECStatus srv = ConfigSecureServerWithNamedCert(aFd, DEFAULT_CERT_NICKNAME,
                                                  nullptr, nullptr);
  if (srv != SECSuccess) {
    return SSL_SNI_SEND_ALERT;
  }

  SSL_OptionSet(aFd, SSL_REQUEST_CERTIFICATE, host->mRequestClientAuth);
  if (host->mRequireClientAuth) {
    SSL_OptionSet(aFd, SSL_REQUIRE_CERTIFICATE, SSL_REQUIRE_ALWAYS);
  } else {
    SSL_OptionSet(aFd, SSL_REQUIRE_CERTIFICATE, SSL_REQUIRE_NEVER);
  }

  // Override default client auth hook to just check fingerprint
  srv = SSL_AuthCertificateHook(aFd, AuthCertificateHook, nullptr);
  if (srv != SECSuccess) {
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

int
main(int argc, char* argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <NSS DB directory>\n", argv[0]);
    return 1;
  }

  return StartServer(argv[1], DoSNISocketConfig, nullptr);
}
