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

class CertVerifier
{
public:
  typedef unsigned int Flags;
  // XXX: FLAG_LOCAL_ONLY is ignored in the classic verification case
  static const Flags FLAG_LOCAL_ONLY;
  // Don't perform fallback DV validation on EV validation failure.
  static const Flags FLAG_MUST_BE_EV;

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
      /*optional out*/ SECOidTag* evOidPolicy = nullptr);

  SECStatus VerifySSLServerCert(
                    CERTCertificate* peerCert,
       /*optional*/ const SECItem* stapledOCSPResponse,
                    mozilla::pkix::Time time,
       /*optional*/ void* pinarg,
                    const char* hostname,
                    bool saveIntermediatesInPermanentDatabase = false,
                    Flags flags = 0,
   /*optional out*/ ScopedCERTCertList* builtChain = nullptr,
   /*optional out*/ SECOidTag* evOidPolicy = nullptr);

  enum PinningMode {
    pinningDisabled = 0,
    pinningAllowUserCAMITM = 1,
    pinningStrict = 2,
    pinningEnforceTestMode = 3
  };

  enum missing_cert_download_config { missing_cert_download_off = 0, missing_cert_download_on };
  enum crl_download_config { crl_local_only = 0, crl_download_allowed };
  enum ocsp_download_config { ocsp_off = 0, ocsp_on };
  enum ocsp_strict_config { ocsp_relaxed = 0, ocsp_strict };
  enum ocsp_get_config { ocsp_get_disabled = 0, ocsp_get_enabled = 1 };

  bool IsOCSPDownloadEnabled() const { return mOCSPDownloadEnabled; }

  CertVerifier(ocsp_download_config odc, ocsp_strict_config osc,
               ocsp_get_config ogc, PinningMode pinningMode);
  ~CertVerifier();

  void ClearOCSPCache() { mOCSPCache.Clear(); }

  const bool mOCSPDownloadEnabled;
  const bool mOCSPStrict;
  const bool mOCSPGETEnabled;
  const PinningMode mPinningMode;

private:
  OCSPCache mOCSPCache;
};

void InitCertVerifierLog();
SECStatus IsCertBuiltInRoot(CERTCertificate* cert, bool& result);

} } // namespace mozilla::psm

#endif // mozilla_psm__CertVerifier_h
