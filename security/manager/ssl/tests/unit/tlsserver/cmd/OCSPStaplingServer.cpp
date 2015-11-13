/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server that delivers various stapled OCSP responses.
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the OCSP response.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <stdio.h>

#include "OCSPCommon.h"
#include "TLSServer.h"

using namespace mozilla;
using namespace mozilla::test;

const OCSPHost sOCSPHosts[] =
{
  { "ocsp-stapling-good.example.com", ORTGood, nullptr, nullptr },
  { "ocsp-stapling-revoked.example.com", ORTRevoked, nullptr, nullptr },
  { "ocsp-stapling-revoked-old.example.com", ORTRevokedOld, nullptr, nullptr },
  { "ocsp-stapling-unknown.example.com", ORTUnknown, nullptr, nullptr },
  { "ocsp-stapling-unknown-old.example.com", ORTUnknownOld, nullptr, nullptr },
  { "ocsp-stapling-good-other.example.com", ORTGoodOtherCert, "ocspOtherEndEntity", nullptr },
  { "ocsp-stapling-good-other-ca.example.com", ORTGoodOtherCA, "other-test-ca", nullptr },
  { "ocsp-stapling-expired.example.com", ORTExpired, nullptr, nullptr },
  { "ocsp-stapling-expired-fresh-ca.example.com", ORTExpiredFreshCA, nullptr, nullptr },
  { "ocsp-stapling-none.example.com", ORTNone, nullptr, nullptr },
  { "ocsp-stapling-empty.example.com", ORTEmpty, nullptr, nullptr },
  { "ocsp-stapling-malformed.example.com", ORTMalformed, nullptr, nullptr },
  { "ocsp-stapling-srverr.example.com", ORTSrverr, nullptr, nullptr },
  { "ocsp-stapling-trylater.example.com", ORTTryLater, nullptr, nullptr },
  { "ocsp-stapling-needssig.example.com", ORTNeedsSig, nullptr, nullptr },
  { "ocsp-stapling-unauthorized.example.com", ORTUnauthorized, nullptr, nullptr },
  { "ocsp-stapling-with-intermediate.example.com", ORTGood, nullptr, "ocspEEWithIntermediate" },
  { "ocsp-stapling-bad-signature.example.com", ORTBadSignature, nullptr, nullptr },
  { "ocsp-stapling-skip-responseBytes.example.com", ORTSkipResponseBytes, nullptr, nullptr },
  { "ocsp-stapling-critical-extension.example.com", ORTCriticalExtension, nullptr, nullptr },
  { "ocsp-stapling-noncritical-extension.example.com", ORTNoncriticalExtension, nullptr, nullptr },
  { "ocsp-stapling-empty-extensions.example.com", ORTEmptyExtensions, nullptr, nullptr },
  { "ocsp-stapling-delegated-included.example.com", ORTDelegatedIncluded, "delegatedSigner", nullptr },
  { "ocsp-stapling-delegated-included-last.example.com", ORTDelegatedIncludedLast, "delegatedSigner", nullptr },
  { "ocsp-stapling-delegated-missing.example.com", ORTDelegatedMissing, "delegatedSigner", nullptr },
  { "ocsp-stapling-delegated-missing-multiple.example.com", ORTDelegatedMissingMultiple, "delegatedSigner", nullptr },
  { "ocsp-stapling-delegated-no-extKeyUsage.example.com", ORTDelegatedIncluded, "invalidDelegatedSignerNoExtKeyUsage", nullptr },
  { "ocsp-stapling-delegated-from-intermediate.example.com", ORTDelegatedIncluded, "invalidDelegatedSignerFromIntermediate", nullptr },
  { "ocsp-stapling-delegated-keyUsage-crlSigning.example.com", ORTDelegatedIncluded, "invalidDelegatedSignerKeyUsageCrlSigning", nullptr },
  { "ocsp-stapling-delegated-wrong-extKeyUsage.example.com", ORTDelegatedIncluded, "invalidDelegatedSignerWrongExtKeyUsage", nullptr },
  { "ocsp-stapling-ancient-valid.example.com", ORTAncientAlmostExpired, nullptr, nullptr },
  { "keysize-ocsp-delegated.example.com", ORTDelegatedIncluded, "rsa-1016-keysizeDelegatedSigner", nullptr },
  { "revoked-ca-cert-used-as-end-entity.example.com", ORTRevoked, "ca-used-as-end-entity", nullptr },
  { "ocsp-stapling-must-staple.example.com", ORTGood, nullptr, "must-staple-ee" },
  { "ocsp-stapling-must-staple-revoked.example.com", ORTRevoked, nullptr, "must-staple-ee" },
  { "ocsp-stapling-must-staple-missing.example.com", ORTNone, nullptr, "must-staple-ee" },
  { "ocsp-stapling-must-staple-empty.example.com", ORTEmpty, nullptr, "must-staple-ee" },
  { "ocsp-stapling-must-staple-ee-with-must-staple-int.example.com", ORTGood, nullptr, "must-staple-ee-with-must-staple-int" },
  { "ocsp-stapling-plain-ee-with-must-staple-int.example.com", ORTGood, nullptr, "must-staple-missing-ee" },
  { "multi-tls-feature-good.example.com", ORTNone, nullptr, "multi-tls-feature-good-ee" },
  { "multi-tls-feature-bad.example.com", ORTNone, nullptr, "multi-tls-feature-bad-ee" },
  { nullptr, ORTNull, nullptr, nullptr }
};

int32_t
DoSNISocketConfig(PRFileDesc *aFd, const SECItem *aSrvNameArr,
                  uint32_t aSrvNameArrSize, void *aArg)
{
  const OCSPHost *host = GetHostForSNI(aSrvNameArr, aSrvNameArrSize,
                                       sOCSPHosts);
  if (!host) {
    return SSL_SNI_SEND_ALERT;
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "found pre-defined host '%s'\n", host->mHostName);
  }

  const char *certNickname = host->mServerCertName ? host->mServerCertName
                                                   : DEFAULT_CERT_NICKNAME;

  ScopedCERTCertificate cert;
  SSLKEAType certKEA;
  if (SECSuccess != ConfigSecureServerWithNamedCert(aFd, certNickname,
                                                    &cert, &certKEA)) {
    return SSL_SNI_SEND_ALERT;
  }

  // If the OCSP response type is "none", don't staple a response.
  if (host->mORT == ORTNone) {
    return 0;
  }

  PLArenaPool *arena = PORT_NewArena(1024);
  if (!arena) {
    PrintPRError("PORT_NewArena failed");
    return SSL_SNI_SEND_ALERT;
  }

  // response is contained by the arena - freeing the arena will free it
  SECItemArray *response = GetOCSPResponseForType(host->mORT, cert, arena,
                                                  host->mAdditionalCertName);
  if (!response) {
    PORT_FreeArena(arena, PR_FALSE);
    return SSL_SNI_SEND_ALERT;
  }

  // SSL_SetStapledOCSPResponses makes a deep copy of response
  SECStatus st = SSL_SetStapledOCSPResponses(aFd, response, certKEA);
  PORT_FreeArena(arena, PR_FALSE);
  if (st != SECSuccess) {
    PrintPRError("SSL_SetStapledOCSPResponses failed");
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <NSS DB directory>\n", argv[0]);
    return 1;
  }

  return StartServer(argv[1], DoSNISocketConfig, nullptr);
}
