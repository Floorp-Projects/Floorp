/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CertVerifier_h
#define CertVerifier_h

#include "BRNameMatchingPolicy.h"
#include "OCSPCache.h"
#include "ScopedNSSTypes.h"
#include "mozilla/Telemetry.h"
#include "pkix/pkixtypes.h"

namespace mozilla { namespace psm {

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
  SucceededWithSHA1Before2016 = 2,
  SucceededWithImportedRoot = 3,
  SucceededWithSHA1 = 4,
  Failed = 5,
};

enum class NetscapeStepUpPolicy : uint32_t;

class PinningTelemetryInfo
{
public:
  // Should we accumulate pinning telemetry for the result?
  bool accumulateResult;
  Telemetry::ID certPinningResultHistogram;
  int32_t certPinningResultBucket;
  // Should we accumulate telemetry for the root?
  bool accumulateForRoot;
  int32_t rootBucket;

  void Reset() { accumulateForRoot = false; accumulateResult = false; }
};

class CertVerifier
{
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
  SECStatus VerifyCert(CERTCertificate* cert,
                       SECCertificateUsage usage,
                       mozilla::pkix::Time time,
                       void* pinArg,
                       const char* hostname,
               /*out*/ UniqueCERTCertList& builtChain,
                       Flags flags = 0,
       /*optional in*/ const SECItem* stapledOCSPResponse = nullptr,
      /*optional out*/ SECOidTag* evOidPolicy = nullptr,
      /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus = nullptr,
      /*optional out*/ KeySizeStatus* keySizeStatus = nullptr,
      /*optional out*/ SHA1ModeResult* sha1ModeResult = nullptr,
      /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo = nullptr);

  SECStatus VerifySSLServerCert(
                    const UniqueCERTCertificate& peerCert,
       /*optional*/ const SECItem* stapledOCSPResponse,
                    mozilla::pkix::Time time,
       /*optional*/ void* pinarg,
                    const char* hostname,
            /*out*/ UniqueCERTCertList& builtChain,
       /*optional*/ bool saveIntermediatesInPermanentDatabase = false,
       /*optional*/ Flags flags = 0,
   /*optional out*/ SECOidTag* evOidPolicy = nullptr,
   /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus = nullptr,
   /*optional out*/ KeySizeStatus* keySizeStatus = nullptr,
   /*optional out*/ SHA1ModeResult* sha1ModeResult = nullptr,
   /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo = nullptr);

  enum PinningMode {
    pinningDisabled = 0,
    pinningAllowUserCAMITM = 1,
    pinningStrict = 2,
    pinningEnforceTestMode = 3
  };

  enum class SHA1Mode {
    Allowed = 0,
    Forbidden = 1,
    Before2016 = 2,
    ImportedRoot = 3,
  };

  enum OcspDownloadConfig {
    ocspOff = 0,
    ocspOn = 1,
    ocspEVOnly = 2
  };
  enum OcspStrictConfig { ocspRelaxed = 0, ocspStrict };
  enum OcspGetConfig { ocspGetDisabled = 0, ocspGetEnabled = 1 };

  CertVerifier(OcspDownloadConfig odc, OcspStrictConfig osc,
               OcspGetConfig ogc, uint32_t certShortLifetimeInDays,
               PinningMode pinningMode, SHA1Mode sha1Mode,
               BRNameMatchingPolicy::Mode nameMatchingMode,
               NetscapeStepUpPolicy netscapeStepUpPolicy);
  ~CertVerifier();

  void ClearOCSPCache() { mOCSPCache.Clear(); }

  const OcspDownloadConfig mOCSPDownloadConfig;
  const bool mOCSPStrict;
  const bool mOCSPGETEnabled;
  const uint32_t mCertShortLifetimeInDays;
  const PinningMode mPinningMode;
  const SHA1Mode mSHA1Mode;
  const BRNameMatchingPolicy::Mode mNameMatchingMode;
  const NetscapeStepUpPolicy mNetscapeStepUpPolicy;

private:
  OCSPCache mOCSPCache;

  // Returns true if the configured SHA1 mode is more restrictive than the given
  // mode. SHA1Mode::Forbidden is more restrictive than any other mode except
  // Forbidden. Next is Before2016, then ImportedRoot, then Allowed.
  // (A mode is never more restrictive than itself.)
  bool SHA1ModeMoreRestrictiveThanGivenMode(SHA1Mode mode);
};

void InitCertVerifierLog();
mozilla::pkix::Result IsCertBuiltInRoot(CERTCertificate* cert, bool& result);
mozilla::pkix::Result CertListContainsExpectedKeys(
  const CERTCertList* certList, const char* hostname, mozilla::pkix::Time time,
  CertVerifier::PinningMode pinningMode);

} } // namespace mozilla::psm

#endif // CertVerifier_h
