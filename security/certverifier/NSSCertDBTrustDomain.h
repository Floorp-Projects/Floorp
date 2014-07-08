/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm__NSSCertDBTrustDomain_h
#define mozilla_psm__NSSCertDBTrustDomain_h

#include "pkix/pkixtypes.h"
#include "secmodt.h"
#include "CertVerifier.h"

namespace mozilla { namespace psm {

SECStatus InitializeNSS(const char* dir, bool readOnly);

void DisableMD5();

extern const char BUILTIN_ROOTS_MODULE_DEFAULT_NAME[];

void PORT_Free_string(char* str);

// The dir parameter is the path to the directory containing the NSS builtin
// roots module. Usually this is the same as the path to the other NSS shared
// libraries. If it is null then the (library) path will be searched.
//
// The modNameUTF8 parameter should usually be
// BUILTIN_ROOTS_MODULE_DEFAULT_NAME.
SECStatus LoadLoadableRoots(/*optional*/ const char* dir,
                            const char* modNameUTF8);

void UnloadLoadableRoots(const char* modNameUTF8);

// Caller must free the result with PR_Free
char* DefaultServerNicknameForCert(CERTCertificate* cert);

void SaveIntermediateCerts(const mozilla::pkix::ScopedCERTCertList& certList);

class NSSCertDBTrustDomain : public mozilla::pkix::TrustDomain
{

public:
  enum OCSPFetching {
    NeverFetchOCSP = 0,
    FetchOCSPForDVSoftFail = 1,
    FetchOCSPForDVHardFail = 2,
    FetchOCSPForEV = 3,
    LocalOnlyOCSPForEV = 4,
  };
  NSSCertDBTrustDomain(SECTrustType certDBTrustType, OCSPFetching ocspFetching,
                       OCSPCache& ocspCache, void* pinArg,
                       CertVerifier::ocsp_get_config ocspGETConfig,
                       CERTChainVerifyCallback* checkChainCallback = nullptr);

  virtual SECStatus FindIssuer(const SECItem& encodedIssuerName,
                               IssuerChecker& checker, PRTime time);

  virtual SECStatus GetCertTrust(mozilla::pkix::EndEntityOrCA endEntityOrCA,
                                 const mozilla::pkix::CertPolicyId& policy,
                                 const SECItem& candidateCertDER,
                         /*out*/ mozilla::pkix::TrustLevel* trustLevel);

  virtual SECStatus VerifySignedData(const CERTSignedData& signedData,
                                     const SECItem& subjectPublicKeyInfo);

  virtual SECStatus CheckRevocation(mozilla::pkix::EndEntityOrCA endEntityOrCA,
                                    const mozilla::pkix::CertID& certID,
                                    PRTime time,
                       /*optional*/ const SECItem* stapledOCSPResponse,
                       /*optional*/ const SECItem* aiaExtension);

  virtual SECStatus IsChainValid(const CERTCertList* certChain);

private:
  enum EncodedResponseSource {
    ResponseIsFromNetwork = 1,
    ResponseWasStapled = 2
  };
  static const PRTime ServerFailureDelay = 5 * 60 * PR_USEC_PER_SEC;
  SECStatus VerifyAndMaybeCacheEncodedOCSPResponse(
    const mozilla::pkix::CertID& certID, PRTime time,
    uint16_t maxLifetimeInDays, const SECItem& encodedResponse,
    EncodedResponseSource responseSource, /*out*/ bool& expired);

  const SECTrustType mCertDBTrustType;
  const OCSPFetching mOCSPFetching;
  OCSPCache& mOCSPCache; // non-owning!
  void* mPinArg; // non-owning!
  const CertVerifier::ocsp_get_config mOCSPGetConfig;
  CERTChainVerifyCallback* mCheckChainCallback; // non-owning!
};

} } // namespace mozilla::psm

#endif // mozilla_psm__NSSCertDBTrustDomain_h
