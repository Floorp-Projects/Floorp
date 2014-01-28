/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm__CertVerifier_h
#define mozilla_psm__CertVerifier_h

#include "certt.h"
#include "insanity/pkixtypes.h"

namespace mozilla { namespace psm {

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
          /*optional*/ const SECItem* stapledOCSPResponse,
                       const SECCertificateUsage usage,
                       const PRTime time,
                       void* pinArg,
                       const Flags flags = 0,
      /*optional out*/ insanity::pkix::ScopedCERTCertList* validationChain = nullptr,
      /*optional out*/ SECOidTag* evOidPolicy = nullptr ,
      /*optional out*/ CERTVerifyLog* verifyLog = nullptr);

  SECStatus VerifySSLServerCert(
                    CERTCertificate* peerCert,
       /*optional*/ const SECItem* stapledOCSPResponse,
                    PRTime time,
       /*optional*/ void* pinarg,
                    const char* hostname,
                    bool saveIntermediatesInPermanentDatabase = false,
   /*optional out*/ insanity::pkix::ScopedCERTCertList* certChainOut = nullptr,
   /*optional out*/ SECOidTag* evOidPolicy = nullptr);


  enum implementation_config {
    classic = 0,
#ifndef NSS_NO_LIBPKIX
    libpkix = 1,
#endif
  };

  enum missing_cert_download_config { missing_cert_download_off = 0, missing_cert_download_on };
  enum crl_download_config { crl_local_only = 0, crl_download_allowed };
  enum ocsp_download_config { ocsp_off = 0, ocsp_on };
  enum ocsp_strict_config { ocsp_relaxed = 0, ocsp_strict };
  enum ocsp_get_config { ocsp_get_disabled = 0, ocsp_get_enabled = 1 };

  bool IsOCSPDownloadEnabled() const { return mOCSPDownloadEnabled; }

  CertVerifier(implementation_config ic, missing_cert_download_config ac,
               crl_download_config cdc, ocsp_download_config odc,
               ocsp_strict_config osc, ocsp_get_config ogc);
  ~CertVerifier();

public:
  const implementation_config mImplementation;
  const bool mMissingCertDownloadEnabled;
  const bool mCRLDownloadEnabled;
  const bool mOCSPDownloadEnabled;
  const bool mOCSPStrict;
  const bool mOCSPGETEnabled;
};

void InitCertVerifierLog();
} } // namespace mozilla::psm

#endif // mozilla_psm__CertVerifier_h
