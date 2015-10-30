/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm__NSSCertDBTrustDomain_h
#define mozilla_psm__NSSCertDBTrustDomain_h

#include "CertVerifier.h"
#include "nsICertBlocklist.h"
#include "pkix/pkixtypes.h"
#include "secmodt.h"

namespace mozilla { namespace psm {

enum class ValidityCheckingMode {
  CheckingOff = 0,
  CheckForEV = 1,
};

SECStatus InitializeNSS(const char* dir, bool readOnly, bool loadPKCS11Modules);

void DisableMD5();

extern const char BUILTIN_ROOTS_MODULE_DEFAULT_NAME[];

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

enum SignatureDigestOption {
  AcceptAllAlgorithms,
  DisableSHA1ForEE,
  DisableSHA1ForCA,
  DisableSHA1Everywhere,
};

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
                       CertVerifier::OcspGetConfig ocspGETConfig,
                       uint32_t certShortLifetimeInDays,
                       CertVerifier::PinningMode pinningMode,
                       unsigned int minRSABits,
                       ValidityCheckingMode validityCheckingMode,
                       SignatureDigestOption signatureDigestOption,
                       CertVerifier::SHA1Mode sha1Mode,
          /*optional*/ PinningTelemetryInfo* pinningTelemetryInfo = nullptr,
          /*optional*/ const char* hostname = nullptr,
      /*optional out*/ ScopedCERTCertList* builtChain = nullptr);

  virtual Result FindIssuer(mozilla::pkix::Input encodedIssuerName,
                            IssuerChecker& checker,
                            mozilla::pkix::Time time) override;

  virtual Result GetCertTrust(mozilla::pkix::EndEntityOrCA endEntityOrCA,
                              const mozilla::pkix::CertPolicyId& policy,
                              mozilla::pkix::Input candidateCertDER,
                              /*out*/ mozilla::pkix::TrustLevel& trustLevel)
                              override;

  virtual Result CheckSignatureDigestAlgorithm(
                   mozilla::pkix::DigestAlgorithm digestAlg,
                   mozilla::pkix::EndEntityOrCA endEntityOrCA,
                   mozilla::pkix::Time notBefore) override;

  virtual Result CheckRSAPublicKeyModulusSizeInBits(
                   mozilla::pkix::EndEntityOrCA endEntityOrCA,
                   unsigned int modulusSizeInBits) override;

  virtual Result VerifyRSAPKCS1SignedDigest(
                   const mozilla::pkix::SignedDigest& signedDigest,
                   mozilla::pkix::Input subjectPublicKeyInfo) override;

  virtual Result CheckECDSACurveIsAcceptable(
                   mozilla::pkix::EndEntityOrCA endEntityOrCA,
                   mozilla::pkix::NamedCurve curve) override;

  virtual Result VerifyECDSASignedDigest(
                   const mozilla::pkix::SignedDigest& signedDigest,
                   mozilla::pkix::Input subjectPublicKeyInfo) override;

  virtual Result DigestBuf(mozilla::pkix::Input item,
                           mozilla::pkix::DigestAlgorithm digestAlg,
                           /*out*/ uint8_t* digestBuf,
                           size_t digestBufLen) override;

  virtual Result CheckValidityIsAcceptable(
                   mozilla::pkix::Time notBefore, mozilla::pkix::Time notAfter,
                   mozilla::pkix::EndEntityOrCA endEntityOrCA,
                   mozilla::pkix::KeyPurposeId keyPurpose) override;

  virtual Result CheckRevocation(
                   mozilla::pkix::EndEntityOrCA endEntityOrCA,
                   const mozilla::pkix::CertID& certID,
                   mozilla::pkix::Time time,
                   mozilla::pkix::Duration validityDuration,
      /*optional*/ const mozilla::pkix::Input* stapledOCSPResponse,
      /*optional*/ const mozilla::pkix::Input* aiaExtension)
                   override;

  virtual Result IsChainValid(const mozilla::pkix::DERArray& certChain,
                              mozilla::pkix::Time time) override;

  CertVerifier::OCSPStaplingStatus GetOCSPStaplingStatus() const
  {
    return mOCSPStaplingStatus;
  }
  void ResetOCSPStaplingStatus()
  {
    mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_NEVER_CHECKED;
  }

private:
  enum EncodedResponseSource {
    ResponseIsFromNetwork = 1,
    ResponseWasStapled = 2
  };
  Result VerifyAndMaybeCacheEncodedOCSPResponse(
    const mozilla::pkix::CertID& certID, mozilla::pkix::Time time,
    uint16_t maxLifetimeInDays, mozilla::pkix::Input encodedResponse,
    EncodedResponseSource responseSource, /*out*/ bool& expired);

  const SECTrustType mCertDBTrustType;
  const OCSPFetching mOCSPFetching;
  OCSPCache& mOCSPCache; // non-owning!
  void* mPinArg; // non-owning!
  const CertVerifier::OcspGetConfig mOCSPGetConfig;
  const uint32_t mCertShortLifetimeInDays;
  CertVerifier::PinningMode mPinningMode;
  const unsigned int mMinRSABits;
  ValidityCheckingMode mValidityCheckingMode;
  SignatureDigestOption mSignatureDigestOption;
  CertVerifier::SHA1Mode mSHA1Mode;
  PinningTelemetryInfo* mPinningTelemetryInfo;
  const char* mHostname; // non-owning - only used for pinning checks
  ScopedCERTCertList* mBuiltChain; // non-owning
  nsCOMPtr<nsICertBlocklist> mCertBlocklist;
  CertVerifier::OCSPStaplingStatus mOCSPStaplingStatus;
};

} } // namespace mozilla::psm

#endif // mozilla_psm__NSSCertDBTrustDomain_h
