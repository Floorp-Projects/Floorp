/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm__CertVerifier_h
#define mozilla_psm__CertVerifier_h

#include "pkix/pkixtypes.h"
#include "OCSPCache.h"
#include "ScopedNSSTypes.h"

namespace mozilla { namespace psm {

struct ChainValidationCallbackState;

// These values correspond to the CERT_CHAIN_KEY_SIZE_STATUS telemetry.
enum class KeySizeStatus {
  NeverChecked = 0,
  LargeMinimumSucceeded = 1,
  CompatibilityRisk = 2,
  AlreadyBad = 3,
};

class CertVerifier
{
public:
  typedef unsigned int Flags;
  // XXX: FLAG_LOCAL_ONLY is ignored in the classic verification case
  static const Flags FLAG_LOCAL_ONLY;
  // Don't perform fallback DV validation on EV validation failure.
  static const Flags FLAG_MUST_BE_EV;

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
                       Flags flags = 0,
       /*optional in*/ const SECItem* stapledOCSPResponse = nullptr,
      /*optional out*/ ScopedCERTCertList* builtChain = nullptr,
      /*optional out*/ SECOidTag* evOidPolicy = nullptr,
      /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus = nullptr,
      /*optional out*/ KeySizeStatus* keySizeStatus = nullptr);

  SECStatus VerifySSLServerCert(
                    CERTCertificate* peerCert,
       /*optional*/ const SECItem* stapledOCSPResponse,
                    mozilla::pkix::Time time,
       /*optional*/ void* pinarg,
                    const char* hostname,
                    bool saveIntermediatesInPermanentDatabase = false,
                    Flags flags = 0,
   /*optional out*/ ScopedCERTCertList* builtChain = nullptr,
   /*optional out*/ SECOidTag* evOidPolicy = nullptr,
   /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus = nullptr,
   /*optional out*/ KeySizeStatus* keySizeStatus = nullptr);

  enum PinningMode {
    pinningDisabled = 0,
    pinningAllowUserCAMITM = 1,
    pinningStrict = 2,
    pinningEnforceTestMode = 3
  };

  enum OcspDownloadConfig { ocspOff = 0, ocspOn };
  enum OcspStrictConfig { ocspRelaxed = 0, ocspStrict };
  enum OcspGetConfig { ocspGetDisabled = 0, ocspGetEnabled = 1 };

  bool IsOCSPDownloadEnabled() const { return mOCSPDownloadEnabled; }

  CertVerifier(OcspDownloadConfig odc, OcspStrictConfig osc,
               OcspGetConfig ogc, uint32_t certShortLifetimeInDays,
               PinningMode pinningMode);
  ~CertVerifier();

  void ClearOCSPCache() { mOCSPCache.Clear(); }

  const bool mOCSPDownloadEnabled;
  const bool mOCSPStrict;
  const bool mOCSPGETEnabled;
  const uint32_t mCertShortLifetimeInDays;
  const PinningMode mPinningMode;

private:
  OCSPCache mOCSPCache;
};

void InitCertVerifierLog();
SECStatus IsCertBuiltInRoot(CERTCertificate* cert, bool& result);
mozilla::pkix::Result CertListContainsExpectedKeys(
  const CERTCertList* certList, const char* hostname, mozilla::pkix::Time time,
  CertVerifier::PinningMode pinningMode);

} } // namespace mozilla::psm

#endif // mozilla_psm__CertVerifier_h
