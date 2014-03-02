/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1
#endif

#include "AppTrustDomain.h"
#include "certdb.h"
#include "insanity/pkix.h"
#include "mozilla/ArrayUtils.h"
#include "nsIX509CertDB.h"
#include "prerror.h"
#include "secerr.h"

// Generated in Makefile.in
#include "marketplace-prod-public.inc"
#include "marketplace-prod-reviewers.inc"
#include "marketplace-dev-public.inc"
#include "marketplace-dev-reviewers.inc"
#include "xpcshell.inc"

using namespace insanity::pkix;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

namespace mozilla { namespace psm {

AppTrustDomain::AppTrustDomain(void* pinArg)
  : mPinArg(pinArg)
{
}

SECStatus
AppTrustDomain::SetTrustedRoot(AppTrustedRoot trustedRoot)
{
  SECItem trustedDER;

  // Load the trusted certificate into the in-memory NSS database so that
  // CERT_CreateSubjectCertList can find it.

  switch (trustedRoot)
  {
    case nsIX509CertDB::AppMarketplaceProdPublicRoot:
      trustedDER.data = const_cast<uint8_t*>(marketplaceProdPublicRoot);
      trustedDER.len = mozilla::ArrayLength(marketplaceProdPublicRoot);
      break;

    case nsIX509CertDB::AppMarketplaceProdReviewersRoot:
      trustedDER.data = const_cast<uint8_t*>(marketplaceProdReviewersRoot);
      trustedDER.len = mozilla::ArrayLength(marketplaceProdReviewersRoot);
      break;

    case nsIX509CertDB::AppMarketplaceDevPublicRoot:
      trustedDER.data = const_cast<uint8_t*>(marketplaceDevPublicRoot);
      trustedDER.len = mozilla::ArrayLength(marketplaceDevPublicRoot);
      break;

    case nsIX509CertDB::AppMarketplaceDevReviewersRoot:
      trustedDER.data = const_cast<uint8_t*>(marketplaceDevReviewersRoot);
      trustedDER.len = mozilla::ArrayLength(marketplaceDevReviewersRoot);
      break;

    case nsIX509CertDB::AppXPCShellRoot:
      trustedDER.data = const_cast<uint8_t*>(xpcshellRoot);
      trustedDER.len = mozilla::ArrayLength(xpcshellRoot);
      break;

    default:
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
  }

  mTrustedRoot = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                         &trustedDER, nullptr, false, true);
  if (!mTrustedRoot) {
    return SECFailure;
  }

  return SECSuccess;
}

SECStatus
AppTrustDomain::FindPotentialIssuers(const SECItem* encodedIssuerName,
                                     PRTime time,
                             /*out*/ insanity::pkix::ScopedCERTCertList& results)
{
  MOZ_ASSERT(mTrustedRoot);
  if (!mTrustedRoot) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  results = CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                       encodedIssuerName, time, true);
  return SECSuccess;
}

SECStatus
AppTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                             SECOidTag policy,
                             const CERTCertificate* candidateCert,
                     /*out*/ TrustLevel* trustLevel)
{
  MOZ_ASSERT(policy == SEC_OID_X509_ANY_POLICY);
  MOZ_ASSERT(candidateCert);
  MOZ_ASSERT(trustLevel);
  MOZ_ASSERT(mTrustedRoot);
  if (!candidateCert || !trustLevel || policy != SEC_OID_X509_ANY_POLICY) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  if (!mTrustedRoot) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // Handle active distrust of the certificate.
  CERTCertTrust trust;
  if (CERT_GetCertTrust(candidateCert, &trust) == SECSuccess) {
    PRUint32 flags = SEC_GET_TRUST_FLAGS(&trust, trustObjectSigning);

    // For DISTRUST, we use the CERTDB_TRUSTED or CERTDB_TRUSTED_CA bit,
    // because we can have active distrust for either type of cert. Note that
    // CERTDB_TERMINAL_RECORD means "stop trying to inherit trust" so if the
    // relevant trust bit isn't set then that means the cert must be considered
    // distrusted.
    PRUint32 relevantTrustBit = endEntityOrCA == MustBeCA
                              ? CERTDB_TRUSTED_CA
                              : CERTDB_TRUSTED;
    if (((flags & (relevantTrustBit | CERTDB_TERMINAL_RECORD)))
            == CERTDB_TERMINAL_RECORD) {
      *trustLevel = ActivelyDistrusted;
      return SECSuccess;
    }

#ifdef MOZ_B2G_CERTDATA
    // XXX(Bug 972201): We have to allow the old way of supporting additional
    // roots until we fix bug 889744. Remove this along with the rest of the
    // MOZ_B2G_CERTDATA stuff.

    // For TRUST, we only use the CERTDB_TRUSTED_CA bit, because Gecko hasn't
    // needed to consider end-entity certs to be their own trust anchors since
    // Gecko implemented nsICertOverrideService.
    if (flags & CERTDB_TRUSTED_CA) {
      *trustLevel = TrustAnchor;
      return SECSuccess;
    }
#endif
  }

  // mTrustedRoot is the only trust anchor for this validation.
  if (CERT_CompareCerts(mTrustedRoot.get(), candidateCert)) {
    *trustLevel = TrustAnchor;
    return SECSuccess;
  }

  *trustLevel = InheritsTrust;
  return SECSuccess;
}

SECStatus
AppTrustDomain::VerifySignedData(const CERTSignedData* signedData,
                                  const CERTCertificate* cert)
{
  return ::insanity::pkix::VerifySignedData(signedData, cert, mPinArg);
}

SECStatus
AppTrustDomain::CheckRevocation(EndEntityOrCA,
                                const CERTCertificate*,
                                /*const*/ CERTCertificate*,
                                PRTime time,
                                /*optional*/ const SECItem*)
{
  // We don't currently do revocation checking. If we need to distrust an Apps
  // certificate, we will use the active distrust mechanism.
  return SECSuccess;
}

} }
