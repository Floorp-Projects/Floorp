/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_AppsTrustDomain_h
#define mozilla_psm_AppsTrustDomain_h

#include "pkix/pkixtypes.h"
#include "nsDebug.h"
#include "nsIX509CertDB.h"
#include "ScopedNSSTypes.h"

namespace mozilla { namespace psm {

class AppTrustDomain MOZ_FINAL : public mozilla::pkix::TrustDomain
{
public:
  typedef mozilla::pkix::Result Result;

  AppTrustDomain(ScopedCERTCertList&, void* pinArg);

  SECStatus SetTrustedRoot(AppTrustedRoot trustedRoot);

  virtual Result GetCertTrust(mozilla::pkix::EndEntityOrCA endEntityOrCA,
                              const mozilla::pkix::CertPolicyId& policy,
                              const SECItem& candidateCertDER,
                              /*out*/ mozilla::pkix::TrustLevel* trustLevel)
                              MOZ_OVERRIDE;
  virtual Result FindIssuer(const SECItem& encodedIssuerName,
                            IssuerChecker& checker,
                            PRTime time) MOZ_OVERRIDE;
  virtual Result CheckRevocation(mozilla::pkix::EndEntityOrCA endEntityOrCA,
                                 const mozilla::pkix::CertID& certID, PRTime time,
                                 /*optional*/ const SECItem* stapledOCSPresponse,
                                 /*optional*/ const SECItem* aiaExtension);
  virtual Result IsChainValid(const mozilla::pkix::DERArray& certChain)
                              MOZ_OVERRIDE;
  virtual Result CheckPublicKey(const SECItem& subjectPublicKeyInfo)
                                MOZ_OVERRIDE;
  virtual Result VerifySignedData(
           const mozilla::pkix::SignedDataWithSignature& signedData,
           const SECItem& subjectPublicKeyInfo) MOZ_OVERRIDE;
  virtual Result DigestBuf(const SECItem& item, /*out*/ uint8_t* digestBuf,
                           size_t digestBufLen) MOZ_OVERRIDE;

private:
  /*out*/ ScopedCERTCertList& mCertChain;
  void* mPinArg; // non-owning!
  ScopedCERTCertificate mTrustedRoot;
};

} } // namespace mozilla::psm

#endif // mozilla_psm_AppsTrustDomain_h
