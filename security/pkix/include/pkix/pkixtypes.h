/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix__pkixtypes_h
#define mozilla_pkix__pkixtypes_h

#include "cert.h"
#include "pkix/enumclass.h"
#include "pkix/ScopedPtr.h"
#include "seccomon.h"
#include "secport.h"
#include "stdint.h"

namespace mozilla { namespace pkix {

inline void
PORT_FreeArena_false(PLArenaPool* arena) {
  // PL_FreeArenaPool can't be used because it doesn't actually free the
  // memory, which doesn't work well with memory analysis tools
  return PORT_FreeArena(arena, PR_FALSE);
}

typedef ScopedPtr<PLArenaPool, PORT_FreeArena_false> ScopedPLArenaPool;

typedef ScopedPtr<CERTCertificate, CERT_DestroyCertificate>
        ScopedCERTCertificate;
typedef ScopedPtr<CERTCertList, CERT_DestroyCertList> ScopedCERTCertList;

MOZILLA_PKIX_ENUM_CLASS EndEntityOrCA { MustBeEndEntity = 0, MustBeCA = 1 };

MOZILLA_PKIX_ENUM_CLASS KeyUsage : uint8_t {
  digitalSignature = 0,
  nonRepudiation   = 1,
  keyEncipherment  = 2,
  dataEncipherment = 3,
  keyAgreement     = 4,
  keyCertSign      = 5,
  // cRLSign       = 6,
  // encipherOnly  = 7,
  // decipherOnly  = 8,
  noParticularKeyUsageRequired = 0xff,
};

MOZILLA_PKIX_ENUM_CLASS KeyPurposeId {
  anyExtendedKeyUsage = 0,
  id_kp_serverAuth = 1,           // id-kp-serverAuth
  id_kp_clientAuth = 2,           // id-kp-clientAuth
  id_kp_codeSigning = 3,          // id-kp-codeSigning
  id_kp_emailProtection = 4,      // id-kp-emailProtection
  id_kp_OCSPSigning = 9,          // id-kp-OCSPSigning
};

struct CertPolicyId {
  uint16_t numBytes;
  static const uint16_t MAX_BYTES = 24;
  uint8_t bytes[MAX_BYTES];

  bool IsAnyPolicy() const;

  static const CertPolicyId anyPolicy;
};

MOZILLA_PKIX_ENUM_CLASS TrustLevel {
  TrustAnchor = 1,        // certificate is a trusted root CA certificate or
                          // equivalent *for the given policy*.
  ActivelyDistrusted = 2, // certificate is known to be bad
  InheritsTrust = 3       // certificate must chain to a trust anchor
};

// CertID references the information needed to do revocation checking for the
// certificate issued by the given issuer with the given serial number.
//
// issuer must be the DER-encoded issuer field from the certificate for which
// revocation checking is being done, **NOT** the subject field of the issuer
// certificate. (Those two fields must be equal to each other, but they may not
// be encoded exactly the same, and the encoding matters for OCSP.)
// issuerSubjectPublicKeyInfo is the entire DER-encoded subjectPublicKeyInfo
// field from the issuer's certificate. serialNumber is the entire DER-encoded
// serial number from the subject certificate (the certificate for which we are
// checking the revocation status).
struct CertID
{
public:
  CertID(const SECItem& issuer, const SECItem& issuerSubjectPublicKeyInfo,
         const SECItem& serialNumber)
    : issuer(issuer)
    , issuerSubjectPublicKeyInfo(issuerSubjectPublicKeyInfo)
    , serialNumber(serialNumber)
  {
  }
  const SECItem& issuer;
  const SECItem& issuerSubjectPublicKeyInfo;
  const SECItem& serialNumber;
private:
  void operator=(const CertID&) /*= delete*/;
};

// Applications control the behavior of path building and verification by
// implementing the TrustDomain interface. The TrustDomain is used for all
// cryptography and for determining which certificates are trusted or
// distrusted.
class TrustDomain
{
public:
  virtual ~TrustDomain() { }

  // Determine the level of trust in the given certificate for the given role.
  // This will be called for every certificate encountered during path
  // building.
  //
  // When policy.IsAnyPolicy(), then no policy-related checking should be done.
  // When !policy.IsAnyPolicy(), then GetCertTrust MUST NOT return with
  // *trustLevel == TrustAnchor unless the given cert is considered a trust
  // anchor *for that policy*. In particular, if the user has marked an
  // intermediate certificate as trusted, but that intermediate isn't in the
  // list of EV roots, then GetCertTrust must result in
  // *trustLevel == InheritsTrust instead of *trustLevel == TrustAnchor
  // (assuming the candidate cert is not actively distrusted).
  virtual SECStatus GetCertTrust(EndEntityOrCA endEntityOrCA,
                                 const CertPolicyId& policy,
                                 const SECItem& candidateCertDER,
                         /*out*/ TrustLevel* trustLevel) = 0;

  // Find all certificates (intermediate and/or root) in the certificate
  // database that have a subject name matching |encodedIssuerName| at
  // the given time. Certificates where the given time is not within the
  // certificate's validity period may be excluded. On input, |results|
  // will be null on input. If no potential issuers are found, then this
  // function should return SECSuccess with results being either null or
  // an empty list. Otherwise, this function should construct a
  // CERTCertList and return it in |results|, transfering ownership.
  virtual SECStatus FindPotentialIssuers(const SECItem* encodedIssuerName,
                                         PRTime time,
                                 /*out*/ ScopedCERTCertList& results) = 0;

  // Verify the given signature using the given public key.
  //
  // Most implementations of this function should probably forward the call
  // directly to mozilla::pkix::VerifySignedData.
  virtual SECStatus VerifySignedData(const CERTSignedData* signedData,
                                     const SECItem& subjectPublicKeyInfo) = 0;

  // issuerCertToDup is only non-const so CERT_DupCertificate can be called on
  // it.
  virtual SECStatus CheckRevocation(EndEntityOrCA endEntityOrCA,
                                    const CertID& certID, PRTime time,
                       /*optional*/ const SECItem* stapledOCSPresponse,
                       /*optional*/ const SECItem* aiaExtension) = 0;

  // Called as soon as we think we have a valid chain but before revocation
  // checks are done. Called to compute additional chain level checks, by the
  // TrustDomain.
  virtual SECStatus IsChainValid(const CERTCertList* certChain) = 0;

protected:
  TrustDomain() { }

private:
  TrustDomain(const TrustDomain&) /* = delete */;
  void operator=(const TrustDomain&) /* = delete */;
};

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkixtypes_h
