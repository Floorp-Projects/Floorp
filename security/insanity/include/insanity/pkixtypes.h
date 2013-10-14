/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2012 Mozilla Foundation
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

#ifndef insanity_pkix__pkixtypes_h
#define insanity_pkix__pkixtypes_h

#include "insanity/ScopedPtr.h"
#include "plarena.h"
#include "cert.h"
#include "keyhi.h"

namespace insanity { namespace pkix {

typedef ScopedPtr<PLArenaPool, PL_FreeArenaPool> ScopedPLArenaPool;

typedef ScopedPtr<CERTCertificate, CERT_DestroyCertificate>
        ScopedCERTCertificate;
typedef ScopedPtr<CERTCertList, CERT_DestroyCertList> ScopedCERTCertList;
typedef ScopedPtr<SECKEYPublicKey, SECKEY_DestroyPublicKey>
        ScopedSECKEYPublicKey;

typedef unsigned int KeyUsages;

enum EndEntityOrCA { MustBeEndEntity, MustBeCA };

// Applications control the behavior of path building and verification by
// implementing the TrustDomain interface. The TrustDomain is used for all
// cryptography and for determining which certificates are trusted or
// distrusted.
class TrustDomain
{
public:
  virtual ~TrustDomain() { }

  enum TrustLevel {
    TrustAnchor = 1,        // certificate is a trusted root CA certificate or
                            // equivalent
    ActivelyDistrusted = 2, // certificate is known to be bad
    InheritsTrust = 3       // certificate must chain to a trust anchor
  };

  // Determine the level of trust in the given certificate for the given role.
  // This will be called for every certificate encountered during path
  // building.
  virtual SECStatus GetCertTrust(EndEntityOrCA endEntityOrCA,
                                 const CERTCertificate* candidateCert,
                         /*out*/ TrustLevel* trustLevel) = 0;

  // Find all certificates (intermediate and/or root) in the certificate
  // database that have a subject name matching |encodedIssuerName| at
  // the given time. Certificates where the given time is not within the
  // certificate's validity period may be excluded. The results should be
  // added to the |results| certificate list.
  virtual SECStatus FindPotentialIssuers(const SECItem* encodedIssuerName,
                                         PRTime time,
                                 /*out*/ ScopedCERTCertList& results) = 0;

  // Verify the given signature using the public key of the given certificate.
  // The implementation should be careful to ensure that the given certificate
  // has all the public key information needed--i.e. it should ensure that the
  // certificate is not trying to use EC(DSA) parameter inheritance.
  //
  // Most implementations of this function should probably forward the call
  // directly to insanity::pkix::VerifySignedData.
  virtual SECStatus VerifySignedData(const CERTSignedData* signedData,
                                     const CERTCertificate* cert) = 0;

protected:
  TrustDomain() { }

private:
  TrustDomain(const TrustDomain&) /* = delete */;
  void operator=(const TrustDomain&) /* = delete */;
};

} } // namespace insanity::pkix

#endif // insanity_pkix__pkixtypes_h
