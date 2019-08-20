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

struct BadCertAndPinningHost {
  const char* mHostName;
  const char* mCertName;
};

// Hostname, cert nickname pairs.
const BadCertAndPinningHost sBadCertAndPinningHosts[] = {
    {"expired.example.com", "expired-ee"},
    {"notyetvalid.example.com", "notYetValid"},
    {"before-epoch.example.com", "beforeEpoch"},
    {"selfsigned.example.com", "selfsigned"},
    {"unknownissuer.example.com", "unknownissuer"},
    {"mismatch.example.com", "mismatch"},
    {"mismatch-CN.example.com", "mismatchCN"},
    {"mitm.example.com", "mitm"},
    {"expiredissuer.example.com", "expiredissuer"},
    {"notyetvalidissuer.example.com", "notYetValidIssuer"},
    {"before-epoch-issuer.example.com", "beforeEpochIssuer"},
    {"md5signature.example.com", "md5signature"},
    {"untrusted.example.com", "default-ee"},
    {"untrustedissuer.example.com", "untrustedissuer"},
    {"mismatch-expired.example.com", "mismatch-expired"},
    {"mismatch-notYetValid.example.com", "mismatch-notYetValid"},
    {"mismatch-untrusted.example.com", "mismatch-untrusted"},
    {"untrusted-expired.example.com", "untrusted-expired"},
    {"md5signature-expired.example.com", "md5signature-expired"},
    {"mismatch-untrusted-expired.example.com", "mismatch-untrusted-expired"},
    {"inadequatekeyusage.example.com", "inadequatekeyusage-ee"},
    {"selfsigned-inadequateEKU.example.com", "selfsigned-inadequateEKU"},
    {"self-signed-end-entity-with-cA-true.example.com",
     "self-signed-EE-with-cA-true"},
    {"ca-used-as-end-entity.example.com", "ca-used-as-end-entity"},
    {"ca-used-as-end-entity-name-mismatch.example.com",
     "ca-used-as-end-entity"},
    // All of include-subdomains.pinning.example.com is pinned to End Entity
    // Test Cert with nick default-ee. Any other nick will only
    // pass pinning when security.cert_pinning.enforcement.level != strict and
    // otherCA is added as a user-specified trust anchor. See StaticHPKPins.h.
    {"include-subdomains.pinning.example.com", "default-ee"},
    {"good.include-subdomains.pinning.example.com", "default-ee"},
    {"bad.include-subdomains.pinning.example.com", "other-issuer-ee"},
    {"bad.include-subdomains.pinning.example.com.", "other-issuer-ee"},
    {"bad.include-subdomains.pinning.example.com..", "other-issuer-ee"},
    {"exclude-subdomains.pinning.example.com", "default-ee"},
    {"sub.exclude-subdomains.pinning.example.com", "other-issuer-ee"},
    {"test-mode.pinning.example.com", "other-issuer-ee"},
    {"unknownissuer.include-subdomains.pinning.example.com", "unknownissuer"},
    {"unknownissuer.test-mode.pinning.example.com", "unknownissuer"},
    {"nsCertTypeNotCritical.example.com", "nsCertTypeNotCritical"},
    {"nsCertTypeCriticalWithExtKeyUsage.example.com",
     "nsCertTypeCriticalWithExtKeyUsage"},
    {"nsCertTypeCritical.example.com", "nsCertTypeCritical"},
    {"end-entity-issued-by-v1-cert.example.com", "eeIssuedByV1Cert"},
    {"end-entity-issued-by-non-CA.example.com", "eeIssuedByNonCA"},
    {"inadequate-key-size-ee.example.com", "inadequateKeySizeEE"},
    {"badSubjectAltNames.example.com", "badSubjectAltNames"},
    {"ipAddressAsDNSNameInSAN.example.com", "ipAddressAsDNSNameInSAN"},
    {"noValidNames.example.com", "noValidNames"},
    {"bug413909.xn--hxajbheg2az3al.xn--jxalpdlp", "idn-certificate"},
    {"emptyissuername.example.com", "emptyIssuerName"},
    {"ev-test.example.com", "ev-test"},
    {"ee-from-missing-intermediate.example.com",
     "ee-from-missing-intermediate"},
    {"imminently-distrusted.example.com", "ee-imminently-distrusted"},
    {"localhost", "unknownissuer"},
    {"a.pinning.example.com", "default-ee"},
    {"b.pinning.example.com", "default-ee"},
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
  const BadCertAndPinningHost* host =
      GetHostForSNI(aSrvNameArr, aSrvNameArrSize, sBadCertAndPinningHosts);
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
