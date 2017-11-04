/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrustOverrides_h
#define TrustOverrides_h

#include "mozilla/PodOperations.h"

struct DataAndLength {
  const uint8_t* data;
  uint32_t len;
};

template<size_t T>
static bool
CertDNIsInList(const CERTCertificate* aCert, const DataAndLength (&aDnList)[T])
{
  MOZ_ASSERT(aCert);
  if (!aCert) {
    return false;
  }

  for (auto &dn: aDnList) {
    if (aCert->derSubject.len == dn.len &&
        mozilla::PodEqual(aCert->derSubject.data, dn.data, dn.len)) {
      return true;
    }
  }
  return false;
}

template<size_t T, size_t R>
static bool
CertMatchesStaticData(const CERTCertificate* cert,
                      const unsigned char (&subject)[T],
                      const unsigned char (&spki)[R]) {
  MOZ_ASSERT(cert);
  if (!cert) {
    return false;
  }
  return cert->derSubject.len == T &&
         mozilla::PodEqual(cert->derSubject.data, subject, T) &&
         cert->derPublicKey.len == R &&
         mozilla::PodEqual(cert->derPublicKey.data, spki, R);
}

#endif // TrustOverrides_h
