/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CertVerifier_h
#define CertVerifier_h

#include "BRNameMatchingPolicy.h"
#include "CTPolicyEnforcer.h"
#include "CTVerifyResult.h"
#include "EnterpriseRoots.h"
#include "OCSPCache.h"
#include "RootCertificateTelemetryUtils.h"
#include "ScopedNSSTypes.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"
#include "mozpkix/pkixtypes.h"
#include "sslt.h"

#if defined(_MSC_VER)
#  pragma warning(push)
// Silence "RootingAPI.h(718): warning C4324: 'js::DispatchWrapper<T>':
// structure was padded due to alignment specifier with [ T=void * ]"
#  pragma warning(disable : 4324)
#endif /* defined(_MSC_VER) */
#include "mozilla/BasePrincipal.h"
#if defined(_MSC_VER)
#  pragma warning(pop) /* popping the pragma in this file */
#endif                 /* defined(_MSC_VER) */

namespace mozilla {
namespace ct {

// Including the headers of the classes below would bring along all of their
// dependent headers and force us to export them in moz.build.
// Just forward-declare the classes here instead.
class MultiLogCTVerifier;
class CTDiversityPolicy;

}  // namespace ct
}  // namespace mozilla

namespace mozilla {
namespace psm {

typedef mozilla::pkix::Result Result;

enum class EVStatus : uint8_t {
  NotEV = 0,
  EV = 1,
};

// These values correspond to the CERT_CHAIN_KEY_SIZE_STATUS telemetry.
enum class KeySizeStatus {
  NeverChecked = 0,
  LargeMinimumSucceeded = 1,
  CompatibilityRisk = 2,
  AlreadyBad = 3,
};

// These values correspond to the CERT_CHAIN_SHA1_POLICY_STATUS telemetry.
enum class SHA1ModeResult {
  NeverChecked = 0,
  SucceededWithoutSHA1 = 1,
  SucceededWithImportedRoot = 2,
  SucceededWithImportedRootOrSHA1Before2016 = 3,
  SucceededWithSHA1 = 4,
  Failed = 5,
};

enum class CRLiteMode {
  Disabled = 0,
  TelemetryOnly = 1,
  Enforce = 2,
};

enum class NetscapeStepUpPolicy : uint32_t;

class PinningTelemetryInfo {
 public:
  PinningTelemetryInfo()
      : certPinningResultBucket(0), rootBucket(ROOT_CERTIFICATE_UNKNOWN) {
    Reset();
  }

  // Should we accumulate pinning telemetry for the result?
  bool accumulateResult;
  Maybe<Telemetry::HistogramID> certPinningResultHistogram;
  int32_t certPinningResultBucket;
  // Should we accumulate telemetry for the root?
  bool accumulateForRoot;
  int32_t rootBucket;

  void Reset() {
    accumulateForRoot = false;
    accumulateResult = false;
  }
};

class CertificateTransparencyInfo {
 public:
  CertificateTransparencyInfo()
      : enabled(false),
        policyCompliance(mozilla::ct::CTPolicyCompliance::Unknown) {
    Reset();
  }

  // Was CT enabled?
  bool enabled;
  // Verification result of the processed SCTs.
  mozilla::ct::CTVerifyResult verifyResult;
  // Connection compliance to the CT Policy.
  mozilla::ct::CTPolicyCompliance policyCompliance;

  void Reset();
};

class DelegatedCredentialInfo {
 public:
  DelegatedCredentialInfo() : scheme(ssl_sig_none), authKeyBits(0) {}
  DelegatedCredentialInfo(SSLSignatureScheme scheme, uint32_t authKeyBits)
      : scheme(scheme), authKeyBits(authKeyBits) {}

  // The signature scheme to be used in CertVerify. This tells us
  // whether to interpret |authKeyBits| in an RSA or ECDSA context.
  SSLSignatureScheme scheme;

  // The size of the key, in bits.
  uint32_t authKeyBits;
};

enum class CRLiteLookupResult {
  NeverChecked = 0,
  FilterNotAvailable = 1,
  IssuerNotEnrolled = 2,
  CertificateTooNew = 3,
  CertificateValid = 4,
  CertificateRevoked = 5,
  LibraryFailure = 6,
  CertRevokedByStash = 7,
};

class NSSCertDBTrustDomain;

class CertVerifier {
 public:
  typedef unsigned int Flags;
  // XXX: FLAG_LOCAL_ONLY is ignored in the classic verification case
  static const Flags FLAG_LOCAL_ONLY;
  // Don't perform fallback DV validation on EV validation failure.
  static const Flags FLAG_MUST_BE_EV;
  // TLS feature request_status should be ignored
  static const Flags FLAG_TLS_IGNORE_STATUS_REQUEST;

  // These values correspond to the SSL_OCSP_STAPLING telemetry.
  enum OCSPStaplingStatus {
    OCSP_STAPLING_NEVER_CHECKED = 0,
    OCSP_STAPLING_GOOD = 1,
    OCSP_STAPLING_NONE = 2,
    OCSP_STAPLING_EXPIRED = 3,
    OCSP_STAPLING_INVALID = 4,
  };

  // *evOidPolicy == SEC_OID_UNKNOWN means the cert is NOT EV
  // Only one usage per verification is supported.
  mozilla::pkix::Result VerifyCert(
      CERTCertificate* cert, SECCertificateUsage usage,
      mozilla::pkix::Time time, void* pinArg, const char* hostname,
      /*out*/ UniqueCERTCertList& builtChain, Flags flags = 0,
      /*optional in*/
      const Maybe<nsTArray<nsTArray<uint8_t>>>& extraCertificates = Nothing(),
      /*optional in*/ const Maybe<nsTArray<uint8_t>>& stapledOCSPResponseArg =
          Nothing(),
      /*optional in*/ const Maybe<nsTArray<uint8_t>>& sctsFromTLS = Nothing(),
      /*optional in*/ const OriginAttributes& originAttributes =
          OriginAttributes(),
      /*optional out*/ EVStatus* evStatus = nullptr,
      /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus = nullptr,
      /*optional out*/ KeySizeStatus* keySizeStatus = nullptr,
      /*optional out*/ SHA1ModeResult* sha1ModeResult = nullptr,
      /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo = nullptr,
      /*optional out*/ CertificateTransparencyInfo* ctInfo = nullptr,
      /*optional out*/ CRLiteLookupResult* crliteLookupResult = nullptr);

  mozilla::pkix::Result VerifySSLServerCert(
      const UniqueCERTCertificate& peerCert, mozilla::pkix::Time time,
      void* pinarg, const nsACString& hostname,
      /*out*/ UniqueCERTCertList& builtChain,
      /*optional*/ Flags flags = 0,
      /*optional*/ const Maybe<nsTArray<nsTArray<uint8_t>>>& extraCertificates =
          Nothing(),
      /*optional*/ const Maybe<nsTArray<uint8_t>>& stapledOCSPResponse =
          Nothing(),
      /*optional*/ const Maybe<nsTArray<uint8_t>>& sctsFromTLS = Nothing(),
      /*optional*/ const Maybe<DelegatedCredentialInfo>& dcInfo = Nothing(),
      /*optional*/ const OriginAttributes& originAttributes =
          OriginAttributes(),
      /*optional out*/ EVStatus* evStatus = nullptr,
      /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus = nullptr,
      /*optional out*/ KeySizeStatus* keySizeStatus = nullptr,
      /*optional out*/ SHA1ModeResult* sha1ModeResult = nullptr,
      /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo = nullptr,
      /*optional out*/ CertificateTransparencyInfo* ctInfo = nullptr,
      /*optional out*/ CRLiteLookupResult* crliteLookupResult = nullptr,
      /*optional out*/ bool* isBuiltCertChainRootBuiltInRoot = nullptr);

  enum PinningMode {
    pinningDisabled = 0,
    pinningAllowUserCAMITM = 1,
    pinningStrict = 2,
    pinningEnforceTestMode = 3
  };

  enum class SHA1Mode {
    Allowed = 0,
    Forbidden = 1,
    // There used to be a policy that only allowed SHA1 for certificates issued
    // before 2016. This is no longer available. If a user has selected this
    // policy in about:config, it now maps to Forbidden.
    UsedToBeBefore2016ButNowIsForbidden = 2,
    ImportedRoot = 3,
    ImportedRootOrBefore2016 = 4,
  };

  enum OcspDownloadConfig { ocspOff = 0, ocspOn = 1, ocspEVOnly = 2 };
  enum OcspStrictConfig { ocspRelaxed = 0, ocspStrict };

  enum class CertificateTransparencyMode {
    Disabled = 0,
    TelemetryOnly = 1,
  };

  CertVerifier(OcspDownloadConfig odc, OcspStrictConfig osc,
               mozilla::TimeDuration ocspTimeoutSoft,
               mozilla::TimeDuration ocspTimeoutHard,
               uint32_t certShortLifetimeInDays, PinningMode pinningMode,
               SHA1Mode sha1Mode, BRNameMatchingPolicy::Mode nameMatchingMode,
               NetscapeStepUpPolicy netscapeStepUpPolicy,
               CertificateTransparencyMode ctMode, CRLiteMode crliteMode,
               uint64_t crliteCTMergeDelaySeconds,
               const Vector<EnterpriseCert>& thirdPartyCerts);
  ~CertVerifier();

  void ClearOCSPCache() { mOCSPCache.Clear(); }

  const OcspDownloadConfig mOCSPDownloadConfig;
  const bool mOCSPStrict;
  const mozilla::TimeDuration mOCSPTimeoutSoft;
  const mozilla::TimeDuration mOCSPTimeoutHard;
  const uint32_t mCertShortLifetimeInDays;
  const PinningMode mPinningMode;
  const SHA1Mode mSHA1Mode;
  const BRNameMatchingPolicy::Mode mNameMatchingMode;
  const NetscapeStepUpPolicy mNetscapeStepUpPolicy;
  const CertificateTransparencyMode mCTMode;
  const CRLiteMode mCRLiteMode;
  const uint64_t mCRLiteCTMergeDelaySeconds;

 private:
  OCSPCache mOCSPCache;
  // We keep a copy of the bytes of each third party root to own.
  Vector<EnterpriseCert> mThirdPartyCerts;
  // This is a reusable, precomputed list of Inputs corresponding to each root
  // in mThirdPartyCerts that wasn't too long to make an Input out of.
  Vector<mozilla::pkix::Input> mThirdPartyRootInputs;
  // Similarly, but with intermediates.
  Vector<mozilla::pkix::Input> mThirdPartyIntermediateInputs;

  // We only have a forward declarations of these classes (see above)
  // so we must allocate dynamically.
  UniquePtr<mozilla::ct::MultiLogCTVerifier> mCTVerifier;
  UniquePtr<mozilla::ct::CTDiversityPolicy> mCTDiversityPolicy;

  void LoadKnownCTLogs();
  mozilla::pkix::Result VerifyCertificateTransparencyPolicy(
      NSSCertDBTrustDomain& trustDomain, const UniqueCERTCertList& builtChain,
      mozilla::pkix::Input sctsFromTLS, mozilla::pkix::Time time,
      /*optional out*/ CertificateTransparencyInfo* ctInfo);

  // Returns true if the configured SHA1 mode is more restrictive than the given
  // mode. SHA1Mode::Forbidden is more restrictive than any other mode except
  // Forbidden. Next is ImportedRoot, then ImportedRootOrBefore2016, then
  // Allowed. (A mode is never more restrictive than itself.)
  bool SHA1ModeMoreRestrictiveThanGivenMode(SHA1Mode mode);
};

mozilla::pkix::Result IsCertBuiltInRoot(CERTCertificate* cert, bool& result);
mozilla::pkix::Result CertListContainsExpectedKeys(
    const CERTCertList* certList, const char* hostname,
    mozilla::pkix::Time time, CertVerifier::PinningMode pinningMode);

}  // namespace psm
}  // namespace mozilla

#endif  // CertVerifier_h
