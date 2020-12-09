/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrustOverrides_h
#define TrustOverrides_h

#include "X509CertValidity.h"
#include "nsNSSCertificate.h"
#include "mozilla/ArrayUtils.h"

using namespace mozilla;

struct DataAndLength {
  const uint8_t* data;
  uint32_t len;
};

template <size_t T>
static bool CertDNIsInList(const CERTCertificate* aCert,
                           const DataAndLength (&aDnList)[T]) {
  MOZ_ASSERT(aCert);
  if (!aCert) {
    return false;
  }

  for (auto& dn : aDnList) {
    if (aCert->derSubject.len == dn.len &&
        mozilla::ArrayEqual(aCert->derSubject.data, dn.data, dn.len)) {
      return true;
    }
  }
  return false;
}

template <size_t T>
static bool CertSPKIIsInList(const CERTCertificate* aCert,
                             const DataAndLength (&aSpkiList)[T]) {
  MOZ_ASSERT(aCert);
  if (!aCert) {
    return false;
  }

  for (auto& spki : aSpkiList) {
    if (aCert->derPublicKey.len == spki.len &&
        mozilla::ArrayEqual(aCert->derPublicKey.data, spki.data, spki.len)) {
      return true;
    }
  }
  return false;
}

template <size_t T, size_t R>
static bool CertMatchesStaticData(const CERTCertificate* cert,
                                  const unsigned char (&subject)[T],
                                  const unsigned char (&spki)[R]) {
  MOZ_ASSERT(cert);
  if (!cert) {
    return false;
  }
  return cert->derSubject.len == T &&
         mozilla::ArrayEqual(cert->derSubject.data, subject, T) &&
         cert->derPublicKey.len == R &&
         mozilla::ArrayEqual(cert->derPublicKey.data, spki, R);
}

// Implements the graduated Symantec distrust algorithm from Bug 1409257.
// This accepts a pre-segmented certificate chain (e.g. SegmentCertificateChain)
// as |intCerts|, and pre-assumes that the root has been identified
// as being affected (this is to avoid duplicate Segment operations in the
// NSSCertDBTrustDomain). Each of the |intCerts| is evaluated against a
// |allowlist| of SPKI entries, and if a match is found, then this returns
// "not distrusted." Otherwise, due to the precondition holding, the chain is
// "distrusted."
template <size_t T>
static nsresult CheckForSymantecDistrust(
    const nsTArray<RefPtr<nsIX509Cert>>& intCerts,
    const DataAndLength (&allowlist)[T],
    /* out */ bool& isDistrusted) {
  // PRECONDITION: The rootCert is already verified as being one of the
  // affected Symantec roots

  isDistrusted = true;

  for (const auto& cert : intCerts) {
    UniqueCERTCertificate nssCert(cert->GetCert());
    if (CertSPKIIsInList(nssCert.get(), allowlist)) {
      isDistrusted = false;
      break;
    }
  }
  return NS_OK;
}

#endif  // TrustOverrides_h
