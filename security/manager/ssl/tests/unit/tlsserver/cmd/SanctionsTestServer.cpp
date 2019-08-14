/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server that uses various bad certificates.
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the certificate.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <stdio.h>

#include "TLSServer.h"

using namespace mozilla;
using namespace mozilla::test;

struct SanctionsCertHost {
  const char* mHostName;
  const char* mCertName;
};

// Hostname, cert nickname pairs.
const SanctionsCertHost sSanctionsCertHosts[] = {
    {"symantec-whitelist-after-cutoff.example.com",
     "symantec-ee-from-whitelist-after-cutoff"},
    {"symantec-whitelist-before-cutoff.example.com",
     "symantec-ee-from-whitelist-before-cutoff"},
    {"symantec-not-whitelisted-after-cutoff.example.com",
     "symantec-ee-not-whitelisted-after-cutoff"},
    {"symantec-not-whitelisted-before-cutoff.example.com",
     "symantec-ee-not-whitelisted-before-cutoff"},
    {"symantec-unaffected.example.com", "symantec-ee-unaffected"},
    {nullptr, nullptr}};

int32_t DoSNISocketConfigBySubjectCN(PRFileDesc* aFd,
                                     const SECItem* aSrvNameArr,
                                     uint32_t aSrvNameArrSize) {
  for (uint32_t i = 0; i < aSrvNameArrSize; i++) {
    UniquePORTString name(
        static_cast<char*>(PORT_ZAlloc(aSrvNameArr[i].len + 1)));
    if (name) {
      PORT_Memcpy(name.get(), aSrvNameArr[i].data, aSrvNameArr[i].len);
      if (ConfigSecureServerWithNamedCert(aFd, name.get(), nullptr, nullptr,
                                          nullptr) == SECSuccess) {
        return 0;
      }
    }
  }

  return SSL_SNI_SEND_ALERT;
}

int32_t DoSNISocketConfig(PRFileDesc* aFd, const SECItem* aSrvNameArr,
                          uint32_t aSrvNameArrSize, void* aArg) {
  const SanctionsCertHost* host =
      GetHostForSNI(aSrvNameArr, aSrvNameArrSize, sSanctionsCertHosts);
  if (!host) {
    // No static cert <-> hostname mapping found. This happens when we use a
    // collection of certificates in a given directory and build a cert DB at
    // runtime, rather than using an NSS cert DB populated at build time.
    // (This will be the default in the future.)
    // For all given server names, check if the runtime-built cert DB contains
    // a certificate with a matching subject CN.
    return DoSNISocketConfigBySubjectCN(aFd, aSrvNameArr, aSrvNameArrSize);
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "found pre-defined host '%s'\n", host->mHostName);
  }

  UniqueCERTCertificate cert;
  SSLKEAType certKEA;
  if (SECSuccess != ConfigSecureServerWithNamedCert(aFd, host->mCertName, &cert,
                                                    &certKEA, nullptr)) {
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  return StartServer(argc, argv, DoSNISocketConfig, nullptr);
}
