/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrustOverrides_h
#define TrustOverrides_h

#include "nsNSSCertificate.h"
#include "nsNSSCertValidity.h"
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
// as |intCerts| and |eeCert|, and pre-assumes that the root has been identified
// as being affected (this is to avoid duplicate Segment operations in the
// NSSCertDBTrustDomain). If |permitAfterDate| is non-zero, this algorithm
// returns "not distrusted" if the NotBefore date of |eeCert| is after
// the |permitAfterDate|. Then each of the |intCerts| is evaluated against a
// |whitelist| of SPKI entries, and if a match is found, then this returns
// "not distrusted." Otherwise, due to the precondition holding, the chain is
// "distrusted."
template <size_t T>
static nsresult CheckForSymantecDistrust(
    const nsCOMPtr<nsIX509CertList>& intCerts,
    const nsCOMPtr<nsIX509Cert>& eeCert, const PRTime& permitAfterDate,
    const DataAndLength (&whitelist)[T],
    /* out */ bool& isDistrusted) {
  // PRECONDITION: The rootCert is already verified as being one of the
  // affected Symantec roots

  // Check the preference to see if this is enabled before proceeding.
  // TODO in Bug 1437754

  isDistrusted = true;

  // Only check the validity period if we're asked
  if (permitAfterDate > 0) {
    // We need to verify the age of the end entity
    nsCOMPtr<nsIX509CertValidity> validity;
    nsresult rv = eeCert->GetValidity(getter_AddRefs(validity));
    if (NS_FAILED(rv)) {
      return rv;
    }

    PRTime notBefore;
    rv = validity->GetNotBefore(&notBefore);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // If the end entity's notBefore date is after the permitAfter date, this
    // algorithm doesn't apply, so exit false before we do any iterating.
    if (notBefore >= permitAfterDate) {
      isDistrusted = false;
      return NS_OK;
    }
  }

  // Look for one of the intermediates to be in the whitelist
  RefPtr<nsNSSCertList> intCertList = intCerts->GetCertList();

  return intCertList->ForEachCertificateInChain(
      [&isDistrusted, &whitelist](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore,
                                  /* out */ bool& aContinue) {
        // We need an owning handle when calling nsIX509Cert::GetCert().
        UniqueCERTCertificate nssCert(aCert->GetCert());
        if (CertSPKIIsInList(nssCert.get(), whitelist)) {
          // In the whitelist
          isDistrusted = false;
          aContinue = false;
        }
        return NS_OK;
      });
}

#endif  // TrustOverrides_h
