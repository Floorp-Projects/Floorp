/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrustOverrides_h
#define TrustOverrides_h

#include "mozilla/ArrayUtils.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixutil.h"

using namespace mozilla;
using namespace mozilla::pkix;

struct DataAndLength {
  const uint8_t* data;
  uint32_t len;
};

template <size_t T>
static bool CertDNIsInList(const nsTArray<uint8_t>& aCert,
                           const DataAndLength (&aDnList)[T]) {
  Input certInput;
  mozilla::pkix::Result rv = certInput.Init(aCert.Elements(), aCert.Length());
  if (rv != Success) {
    return false;
  }

  // we don't use the certificate for path building, so this parameter doesn't
  // matter
  EndEntityOrCA notUsedForPaths = EndEntityOrCA::MustBeEndEntity;
  BackCert cert(certInput, notUsedForPaths, nullptr);
  rv = cert.Init();
  if (rv != Success) {
    return false;
  }

  Input subject(cert.GetSubject());

  for (auto& dn : aDnList) {
    Input dnInput;
    rv = dnInput.Init(dn.data, dn.len);
    if (rv != Success) {
      return false;
    }

    if (InputsAreEqual(subject, dnInput)) {
      return true;
    }
  }
  return false;
}

template <size_t T>
static bool CertSPKIIsInList(const nsTArray<uint8_t>& aCert,
                             const DataAndLength (&aSpkiList)[T]) {
  Input certInput;
  mozilla::pkix::Result rv = certInput.Init(aCert.Elements(), aCert.Length());
  if (rv != Success) {
    return false;
  }

  // we don't use the certificate for path building, so this parameter doesn't
  // matter
  EndEntityOrCA notUsedForPaths = EndEntityOrCA::MustBeEndEntity;
  BackCert cert(certInput, notUsedForPaths, nullptr);
  rv = cert.Init();
  if (rv != Success) {
    return false;
  }

  Input publicKey(cert.GetSubjectPublicKeyInfo());

  for (auto& spki : aSpkiList) {
    Input spkiInput;
    rv = spkiInput.Init(spki.data, spki.len);
    if (rv != Success) {
      return false;
    }

    if (InputsAreEqual(publicKey, spkiInput)) {
      return true;
    }
  }
  return false;
}

template <size_t T, size_t R>
static bool CertMatchesStaticData(const nsTArray<uint8_t>& aCert,
                                  const unsigned char (&subject)[T],
                                  const unsigned char (&spki)[R]) {
  Input certInput;
  mozilla::pkix::Result rv = certInput.Init(aCert.Elements(), aCert.Length());
  if (rv != Success) {
    return false;
  }

  // we don't use the certificate for path building, so this parameter doesn't
  // matter
  EndEntityOrCA notUsedForPaths = EndEntityOrCA::MustBeEndEntity;
  BackCert cert(certInput, notUsedForPaths, nullptr);
  rv = cert.Init();
  if (rv != Success) {
    return false;
  }

  Input certSubject(cert.GetSubject());
  Input certSPKI(cert.GetSubjectPublicKeyInfo());

  Input subjectInput;
  rv = subjectInput.Init(subject, T);
  if (rv != Success) {
    return false;
  }

  Input spkiInput;
  rv = spkiInput.Init(spki, R);
  if (rv != Success) {
    return false;
  }

  return InputsAreEqual(certSubject, subjectInput) &&
         InputsAreEqual(certSPKI, spkiInput);
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
    const nsTArray<nsTArray<uint8_t>>& intCerts,
    const DataAndLength (&allowlist)[T],
    /* out */ bool& isDistrusted) {
  // PRECONDITION: The rootCert is already verified as being one of the
  // affected Symantec roots

  isDistrusted = true;

  for (const auto& cert : intCerts) {
    if (CertSPKIIsInList(cert, allowlist)) {
      isDistrusted = false;
      break;
    }
  }
  return NS_OK;
}

#endif  // TrustOverrides_h
