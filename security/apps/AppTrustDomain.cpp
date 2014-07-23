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
#include "pkix/pkix.h"
#include "mozilla/ArrayUtils.h"
#include "nsIX509CertDB.h"
#include "nsNSSCertificate.h"
#include "prerror.h"
#include "secerr.h"

// Generated in Makefile.in
#include "marketplace-prod-public.inc"
#include "marketplace-prod-reviewers.inc"
#include "marketplace-dev-public.inc"
#include "marketplace-dev-reviewers.inc"
#include "marketplace-stage.inc"
#include "xpcshell.inc"

using namespace mozilla::pkix;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

namespace mozilla { namespace psm {

AppTrustDomain::AppTrustDomain(ScopedCERTCertList& certChain, void* pinArg)
  : mCertChain(certChain)
  , mPinArg(pinArg)
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

    case nsIX509CertDB::AppMarketplaceStageRoot:
      trustedDER.data = const_cast<uint8_t*>(marketplaceStageRoot);
      trustedDER.len = mozilla::ArrayLength(marketplaceStageRoot);
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
AppTrustDomain::FindIssuer(const SECItem& encodedIssuerName,
                           IssuerChecker& checker, PRTime time)

{
  MOZ_ASSERT(mTrustedRoot);
  if (!mTrustedRoot) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // TODO(bug 1035418): If/when mozilla::pkix relaxes the restriction that
  // FindIssuer must only pass certificates with a matching subject name to
  // checker.Check, we can stop using CERT_CreateSubjectCertList and instead
  // use logic like this:
  //
  // 1. First, try the trusted trust anchor.
  // 2. Secondly, iterate through the certificates that were stored in the CMS
  //    message, passing each one to checker.Check.
  ScopedCERTCertList
    candidates(CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                          &encodedIssuerName, time, true));
  if (candidates) {
    for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
         !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
      bool keepGoing;
      SECStatus srv = checker.Check(n->cert->derCert,
                                    nullptr/*additionalNameConstraints*/,
                                    keepGoing);
      if (srv != SECSuccess) {
        return SECFailure;
      }
      if (!keepGoing) {
        break;
      }
    }
  }

  return SECSuccess;
}

SECStatus
AppTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                             const CertPolicyId& policy,
                             const SECItem& candidateCertDER,
                     /*out*/ TrustLevel* trustLevel)
{
  MOZ_ASSERT(policy.IsAnyPolicy());
  MOZ_ASSERT(trustLevel);
  MOZ_ASSERT(mTrustedRoot);
  if (!trustLevel || !policy.IsAnyPolicy()) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  if (!mTrustedRoot) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // Handle active distrust of the certificate.

  // XXX: This would be cleaner and more efficient if we could get the trust
  // information without constructing a CERTCertificate here, but NSS doesn't
  // expose it in any other easy-to-use fashion.
  ScopedCERTCertificate candidateCert(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                            const_cast<SECItem*>(&candidateCertDER), nullptr,
                            false, true));
  if (!candidateCert) {
    return SECFailure;
  }

  CERTCertTrust trust;
  if (CERT_GetCertTrust(candidateCert.get(), &trust) == SECSuccess) {
    PRUint32 flags = SEC_GET_TRUST_FLAGS(&trust, trustObjectSigning);

    // For DISTRUST, we use the CERTDB_TRUSTED or CERTDB_TRUSTED_CA bit,
    // because we can have active distrust for either type of cert. Note that
    // CERTDB_TERMINAL_RECORD means "stop trying to inherit trust" so if the
    // relevant trust bit isn't set then that means the cert must be considered
    // distrusted.
    PRUint32 relevantTrustBit = endEntityOrCA == EndEntityOrCA::MustBeCA
                              ? CERTDB_TRUSTED_CA
                              : CERTDB_TRUSTED;
    if (((flags & (relevantTrustBit | CERTDB_TERMINAL_RECORD)))
            == CERTDB_TERMINAL_RECORD) {
      *trustLevel = TrustLevel::ActivelyDistrusted;
      return SECSuccess;
    }
  }

  // mTrustedRoot is the only trust anchor for this validation.
  if (CERT_CompareCerts(mTrustedRoot.get(), candidateCert.get())) {
    *trustLevel = TrustLevel::TrustAnchor;
    return SECSuccess;
  }

  *trustLevel = TrustLevel::InheritsTrust;
  return SECSuccess;
}

SECStatus
AppTrustDomain::VerifySignedData(const SignedDataWithSignature& signedData,
                                 const SECItem& subjectPublicKeyInfo)
{
  return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                           mPinArg);
}

SECStatus
AppTrustDomain::DigestBuf(const SECItem& item, /*out*/ uint8_t* digestBuf,
                          size_t digestBufLen)
{
  return ::mozilla::pkix::DigestBuf(item, digestBuf, digestBufLen);
}

SECStatus
AppTrustDomain::CheckRevocation(EndEntityOrCA, const CertID&, PRTime time,
                                /*optional*/ const SECItem*,
                                /*optional*/ const SECItem*)
{
  // We don't currently do revocation checking. If we need to distrust an Apps
  // certificate, we will use the active distrust mechanism.
  return SECSuccess;
}

SECStatus
AppTrustDomain::IsChainValid(const DERArray& certChain)
{
  return ConstructCERTCertListFromReversedDERArray(certChain, mCertChain);
}

SECStatus
AppTrustDomain::CheckPublicKey(const SECItem& subjectPublicKeyInfo)
{
  return ::mozilla::pkix::CheckPublicKey(subjectPublicKeyInfo);
}

} } // namespace mozilla::psm
