/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSCertDBTrustDomain_h
#define NSSCertDBTrustDomain_h

#include "CertVerifier.h"
#include "ScopedNSSTypes.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/TimeStamp.h"
#ifdef MOZ_NEW_CERT_STORAGE
#  include "nsICertStorage.h"
#else
#  include "nsICertBlocklist.h"
#endif
#include "nsString.h"
#include "mozpkix/pkixtypes.h"
#include "secmodt.h"

namespace mozilla {
namespace psm {

enum class ValidityCheckingMode {
  CheckingOff = 0,
  CheckForEV = 1,
};

// Policy options for matching id-Netscape-stepUp with id-kp-serverAuth (for CA
// certificates only):
// * Always match: the step-up OID is considered equivalent to serverAuth
// * Match before 23 August 2016: the OID is considered equivalent if the
//   certificate's notBefore is before 23 August 2016
// * Match before 23 August 2015: similarly, but for 23 August 2015
// * Never match: the OID is never considered equivalent to serverAuth
enum class NetscapeStepUpPolicy : uint32_t {
  AlwaysMatch = 0,
  MatchBefore23August2016 = 1,
  MatchBefore23August2015 = 2,
  NeverMatch = 3,
};

SECStatus InitializeNSS(const nsACString& dir, bool readOnly,
                        bool loadPKCS11Modules);

void DisableMD5();

/**
 * Loads root certificates from a module.
 *
 * @param dir
 *        The path to the directory containing the NSS builtin roots module.
 *        Usually the same as the path to the other NSS shared libraries.
 *        If empty, the (library) path will be searched.
 * @return true if the roots were successfully loaded, false otherwise.
 */
bool LoadLoadableRoots(const nsCString& dir);

void UnloadLoadableRoots();

nsresult DefaultServerNicknameForCert(const CERTCertificate* cert,
                                      /*out*/ nsCString& nickname);

#ifdef MOZ_NEW_CERT_STORAGE
/**
 * Build nsTArray<uint8_t>s out of the issuer, serial, subject and public key
 * data from the supplied certificate for use in revocation checks.
 *
 * @param cert
 *        The CERTCertificate* from which to extract the data.
 * @param out encIssuer
 *        The array to populate with issuer data.
 * @param out encSerial
 *        The array to populate with serial number data.
 * @param out encSubject
 *        The array to populate with subject data.
 * @param out encPubKey
 *        The array to populate with public key data.
 * @return
 *        NS_OK, unless there's a memory allocation problem, in which case
 *        NS_ERROR_OUT_OF_MEMORY.
 */
nsresult BuildRevocationCheckArrays(const UniqueCERTCertificate& cert,
                                    /*out*/ nsTArray<uint8_t>& issuerBytes,
                                    /*out*/ nsTArray<uint8_t>& serialBytes,
                                    /*out*/ nsTArray<uint8_t>& subjectBytes,
                                    /*out*/ nsTArray<uint8_t>& pubKeyBytes);
#else
/**
 * Build strings of base64 encoded issuer, serial, subject and public key data
 * from the supplied certificate for use in revocation checks.
 *
 * @param cert
 *        The CERTCertificate* from which to extract the data.
 * @param out encIssuer
 *        The string to populate with base64 encoded issuer data.
 * @param out encSerial
 *        The string to populate with base64 encoded serial number data.
 * @param out encSubject
 *        The string to populate with base64 encoded subject data.
 * @param out encPubKey
 *        The string to populate with base64 encoded public key data.
 * @return
 *        NS_OK, unless there's a Base64 encoding problem, in which case
 *        NS_ERROR_FAILURE.
 */
nsresult BuildRevocationCheckStrings(const CERTCertificate* cert,
                                     /*out*/ nsCString& encIssuer,
                                     /*out*/ nsCString& encSerial,
                                     /*out*/ nsCString& encSubject,
                                     /*out*/ nsCString& encPubKey);
#endif

void SaveIntermediateCerts(const UniqueCERTCertList& certList);

class NSSCertDBTrustDomain : public mozilla::pkix::TrustDomain {
 public:
  typedef mozilla::pkix::Result Result;

  enum OCSPFetching {
    NeverFetchOCSP = 0,
    FetchOCSPForDVSoftFail = 1,
    FetchOCSPForDVHardFail = 2,
    FetchOCSPForEV = 3,
    LocalOnlyOCSPForEV = 4,
  };

  NSSCertDBTrustDomain(
      SECTrustType certDBTrustType, OCSPFetching ocspFetching,
      OCSPCache& ocspCache, void* pinArg, mozilla::TimeDuration ocspTimeoutSoft,
      mozilla::TimeDuration ocspTimeoutHard, uint32_t certShortLifetimeInDays,
      CertVerifier::PinningMode pinningMode, unsigned int minRSABits,
      ValidityCheckingMode validityCheckingMode,
      CertVerifier::SHA1Mode sha1Mode,
      NetscapeStepUpPolicy netscapeStepUpPolicy,
      DistrustedCAPolicy distrustedCAPolicy,
      const OriginAttributes& originAttributes,
      const Vector<mozilla::pkix::Input>& thirdPartyRootInputs,
      const Vector<mozilla::pkix::Input>& thirdPartyIntermediateInputs,
      /*out*/ UniqueCERTCertList& builtChain,
      /*optional*/ PinningTelemetryInfo* pinningTelemetryInfo = nullptr,
      /*optional*/ const char* hostname = nullptr);

  virtual Result FindIssuer(mozilla::pkix::Input encodedIssuerName,
                            IssuerChecker& checker,
                            mozilla::pkix::Time time) override;

  virtual Result GetCertTrust(
      mozilla::pkix::EndEntityOrCA endEntityOrCA,
      const mozilla::pkix::CertPolicyId& policy,
      mozilla::pkix::Input candidateCertDER,
      /*out*/ mozilla::pkix::TrustLevel& trustLevel) override;

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

  virtual Result NetscapeStepUpMatchesServerAuth(
      mozilla::pkix::Time notBefore,
      /*out*/ bool& matches) override;

  virtual Result CheckRevocation(
      mozilla::pkix::EndEntityOrCA endEntityOrCA,
      const mozilla::pkix::CertID& certID, mozilla::pkix::Time time,
      mozilla::pkix::Duration validityDuration,
      /*optional*/ const mozilla::pkix::Input* stapledOCSPResponse,
      /*optional*/ const mozilla::pkix::Input* aiaExtension) override;

  virtual Result IsChainValid(
      const mozilla::pkix::DERArray& certChain, mozilla::pkix::Time time,
      const mozilla::pkix::CertPolicyId& requiredPolicy) override;

  virtual void NoteAuxiliaryExtension(
      mozilla::pkix::AuxiliaryExtension extension,
      mozilla::pkix::Input extensionData) override;

  // Resets the OCSP stapling status and SCT lists accumulated during
  // the chain building.
  void ResetAccumulatedState();

  CertVerifier::OCSPStaplingStatus GetOCSPStaplingStatus() const {
    return mOCSPStaplingStatus;
  }

  // SCT lists (see Certificate Transparency) extracted during
  // certificate verification. Note that the returned Inputs are invalidated
  // the next time a chain is built and by ResetAccumulatedState method
  // (and when the TrustDomain object is destroyed).

  mozilla::pkix::Input GetSCTListFromCertificate() const;
  mozilla::pkix::Input GetSCTListFromOCSPStapling() const;

  bool GetIsErrorDueToDistrustedCAPolicy() const;

 private:
  enum EncodedResponseSource {
    ResponseIsFromNetwork = 1,
    ResponseWasStapled = 2
  };
  Result VerifyAndMaybeCacheEncodedOCSPResponse(
      const mozilla::pkix::CertID& certID, mozilla::pkix::Time time,
      uint16_t maxLifetimeInDays, mozilla::pkix::Input encodedResponse,
      EncodedResponseSource responseSource, /*out*/ bool& expired);
  TimeDuration GetOCSPTimeout() const;

  Result SynchronousCheckRevocationWithServer(
      const mozilla::pkix::CertID& certID, const nsCString& aiaLocation,
      mozilla::pkix::Time time, uint16_t maxOCSPLifetimeInDays,
      const Result cachedResponseResult,
      const Result stapledOCSPResponseResult);
  Result HandleOCSPFailure(const Result cachedResponseResult,
                           const Result stapledOCSPResponseResult,
                           const Result error);

  const SECTrustType mCertDBTrustType;
  const OCSPFetching mOCSPFetching;
  OCSPCache& mOCSPCache;  // non-owning!
  void* mPinArg;          // non-owning!
  const mozilla::TimeDuration mOCSPTimeoutSoft;
  const mozilla::TimeDuration mOCSPTimeoutHard;
  const uint32_t mCertShortLifetimeInDays;
  CertVerifier::PinningMode mPinningMode;
  const unsigned int mMinRSABits;
  ValidityCheckingMode mValidityCheckingMode;
  CertVerifier::SHA1Mode mSHA1Mode;
  NetscapeStepUpPolicy mNetscapeStepUpPolicy;
  DistrustedCAPolicy mDistrustedCAPolicy;
  bool mSawDistrustedCAByPolicyError;
  const OriginAttributes& mOriginAttributes;
  const Vector<mozilla::pkix::Input>& mThirdPartyRootInputs;  // non-owning
  const Vector<mozilla::pkix::Input>&
      mThirdPartyIntermediateInputs;  // non-owning
  UniqueCERTCertList& mBuiltChain;    // non-owning
  PinningTelemetryInfo* mPinningTelemetryInfo;
  const char* mHostname;  // non-owning - only used for pinning checks
#ifdef MOZ_NEW_CERT_STORAGE
  nsCOMPtr<nsICertStorage> mCertStorage;
#else
  nsCOMPtr<nsICertBlocklist> mCertBlocklist;
#endif
  CertVerifier::OCSPStaplingStatus mOCSPStaplingStatus;
  // Certificate Transparency data extracted during certificate verification
  UniqueSECItem mSCTListFromCertificate;
  UniqueSECItem mSCTListFromOCSPStapling;
};

}  // namespace psm
}  // namespace mozilla

#endif  // NSSCertDBTrustDomain_h
