/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm__NSSCertDBTrustDomain_h
#define mozilla_psm__NSSCertDBTrustDomain_h

#include "insanity/pkixtypes.h"
#include "secmodt.h"
#include "CertVerifier.h"

namespace mozilla { namespace psm {

SECStatus InitializeNSS(const char* dir, bool readOnly);

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

// Controls the OCSP fetching behavior of the classic verification mode. In the
// classic mode, the OCSP fetching behavior is set globally instead of per
// validation.
void
SetClassicOCSPBehavior(CertVerifier::ocsp_download_config enabled,
                       CertVerifier::ocsp_strict_config strict,
                       CertVerifier::ocsp_get_config get);

// Caller must free the result with PR_Free
char* DefaultServerNicknameForCert(CERTCertificate* cert);

void SaveIntermediateCerts(const insanity::pkix::ScopedCERTCertList& certList);

class NSSCertDBTrustDomain : public insanity::pkix::TrustDomain
{

public:
  NSSCertDBTrustDomain(SECTrustType certDBTrustType,
                       bool ocspDownloadEnabled, bool ocspStrict,
                       void* pinArg);

  virtual SECStatus FindPotentialIssuers(
                        const SECItem* encodedIssuerName,
                        PRTime time,
                /*out*/ insanity::pkix::ScopedCERTCertList& results);

  virtual SECStatus GetCertTrust(insanity::pkix::EndEntityOrCA endEntityOrCA,
                                 const CERTCertificate* candidateCert,
                         /*out*/ TrustLevel* trustLevel);

  virtual SECStatus VerifySignedData(const CERTSignedData* signedData,
                                     const CERTCertificate* cert);

private:
  const SECTrustType mCertDBTrustType;
//  const bool mOCSPDownloadEnabled;
//  const bool mOCSPStrict;
  void* mPinArg; // non-owning!
};

} } // namespace mozilla::psm

#endif // mozilla_psm__NSSCertDBTrustDomain_h
