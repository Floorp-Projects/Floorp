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

void SaveIntermediateCerts(const ScopedCERTCertList& certList);

class NSSCertDBTrustDomain : public mozilla::pkix::TrustDomain
{

public:
  typedef mozilla::pkix::Result Result;

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
          /*optional*/ CERTChainVerifyCallback* checkChainCallback = nullptr,
          /*optional*/ ScopedCERTCertList* builtChain = nullptr);

  virtual Result FindIssuer(mozilla::pkix::InputBuffer encodedIssuerName,
                            IssuerChecker& checker,
                            PRTime time) MOZ_OVERRIDE;

  virtual Result GetCertTrust(mozilla::pkix::EndEntityOrCA endEntityOrCA,
                              const mozilla::pkix::CertPolicyId& policy,
                              mozilla::pkix::InputBuffer candidateCertDER,
                              /*out*/ mozilla::pkix::TrustLevel& trustLevel)
                              MOZ_OVERRIDE;

  virtual Result CheckPublicKey(mozilla::pkix::InputBuffer subjectPublicKeyInfo)
                                MOZ_OVERRIDE;

  virtual Result VerifySignedData(
                   const mozilla::pkix::SignedDataWithSignature& signedData,
                   mozilla::pkix::InputBuffer subjectPublicKeyInfo)
                   MOZ_OVERRIDE;

  virtual Result DigestBuf(mozilla::pkix::InputBuffer item,
                           /*out*/ uint8_t* digestBuf,
                           size_t digestBufLen) MOZ_OVERRIDE;

  virtual Result CheckRevocation(
                   mozilla::pkix::EndEntityOrCA endEntityOrCA,
                   const mozilla::pkix::CertID& certID,
                   PRTime time,
      /*optional*/ const mozilla::pkix::InputBuffer* stapledOCSPResponse,
      /*optional*/ const mozilla::pkix::InputBuffer* aiaExtension)
                   MOZ_OVERRIDE;

  virtual Result IsChainValid(const mozilla::pkix::DERArray& certChain)
                              MOZ_OVERRIDE;

private:
  enum EncodedResponseSource {
    ResponseIsFromNetwork = 1,
    ResponseWasStapled = 2
  };
  static const PRTime ServerFailureDelay = 5 * 60 * PR_USEC_PER_SEC;
  Result VerifyAndMaybeCacheEncodedOCSPResponse(
    const mozilla::pkix::CertID& certID, PRTime time,
    uint16_t maxLifetimeInDays, mozilla::pkix::InputBuffer encodedResponse,
    EncodedResponseSource responseSource, /*out*/ bool& expired);

  const SECTrustType mCertDBTrustType;
  const OCSPFetching mOCSPFetching;
  OCSPCache& mOCSPCache; // non-owning!
  void* mPinArg; // non-owning!
  const CertVerifier::ocsp_get_config mOCSPGetConfig;
  CERTChainVerifyCallback* mCheckChainCallback; // non-owning!
  ScopedCERTCertList* mBuiltChain; // non-owning
};

} } // namespace mozilla::psm

#endif // mozilla_psm__NSSCertDBTrustDomain_h
