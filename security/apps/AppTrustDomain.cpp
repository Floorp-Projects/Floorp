/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppTrustDomain.h"
#include "certdb.h"
#include "pkix/pkixnss.h"
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
// Trusted Hosted Apps Certificates
#include "manifest-signing-root.inc"
#include "manifest-signing-test-root.inc"

using namespace mozilla::pkix;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static const unsigned int DEFAULT_MINIMUM_NON_ECC_BITS = 2048;

namespace mozilla { namespace psm {

AppTrustDomain::AppTrustDomain(ScopedCERTCertList& certChain, void* pinArg)
  : mCertChain(certChain)
  , mPinArg(pinArg)
  , mMinimumNonECCBits(DEFAULT_MINIMUM_NON_ECC_BITS)
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
      // The staging root was generated with a 1024-bit key.
      mMinimumNonECCBits = 1024u;
      break;

    case nsIX509CertDB::AppXPCShellRoot:
      trustedDER.data = const_cast<uint8_t*>(xpcshellRoot);
      trustedDER.len = mozilla::ArrayLength(xpcshellRoot);
      break;

    case nsIX509CertDB::TrustedHostedAppPublicRoot:
      trustedDER.data = const_cast<uint8_t*>(trustedAppPublicRoot);
      trustedDER.len = mozilla::ArrayLength(trustedAppPublicRoot);
      break;

    case nsIX509CertDB::TrustedHostedAppTestRoot:
      trustedDER.data = const_cast<uint8_t*>(trustedAppTestRoot);
      trustedDER.len = mozilla::ArrayLength(trustedAppTestRoot);
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

Result
AppTrustDomain::FindIssuer(Input encodedIssuerName, IssuerChecker& checker,
                           Time)

{
  MOZ_ASSERT(mTrustedRoot);
  if (!mTrustedRoot) {
    return Result::FATAL_ERROR_INVALID_STATE;
  }

  // TODO(bug 1035418): If/when mozilla::pkix relaxes the restriction that
  // FindIssuer must only pass certificates with a matching subject name to
  // checker.Check, we can stop using CERT_CreateSubjectCertList and instead
  // use logic like this:
  //
  // 1. First, try the trusted trust anchor.
  // 2. Secondly, iterate through the certificates that were stored in the CMS
  //    message, passing each one to checker.Check.
  SECItem encodedIssuerNameSECItem =
    UnsafeMapInputToSECItem(encodedIssuerName);
  ScopedCERTCertList
    candidates(CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                          &encodedIssuerNameSECItem, 0,
                                          false));
  if (candidates) {
    for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
         !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
      Input certDER;
      Result rv = certDER.Init(n->cert->derCert.data, n->cert->derCert.len);
      if (rv != Success) {
        continue; // probably too big
      }

      bool keepGoing;
      rv = checker.Check(certDER, nullptr/*additionalNameConstraints*/,
                         keepGoing);
      if (rv != Success) {
        return rv;
      }
      if (!keepGoing) {
        break;
      }
    }
  }

  return Success;
}

Result
AppTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                             const CertPolicyId& policy,
                             Input candidateCertDER,
                     /*out*/ TrustLevel& trustLevel)
{
  MOZ_ASSERT(policy.IsAnyPolicy());
  MOZ_ASSERT(mTrustedRoot);
  if (!policy.IsAnyPolicy()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  if (!mTrustedRoot) {
    return Result::FATAL_ERROR_INVALID_STATE;
  }

  // Handle active distrust of the certificate.

  // XXX: This would be cleaner and more efficient if we could get the trust
  // information without constructing a CERTCertificate here, but NSS doesn't
  // expose it in any other easy-to-use fashion.
  SECItem candidateCertDERSECItem =
    UnsafeMapInputToSECItem(candidateCertDER);
  ScopedCERTCertificate candidateCert(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &candidateCertDERSECItem,
                            nullptr, false, true));
  if (!candidateCert) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  CERTCertTrust trust;
  if (CERT_GetCertTrust(candidateCert.get(), &trust) == SECSuccess) {
    uint32_t flags = SEC_GET_TRUST_FLAGS(&trust, trustObjectSigning);

    // For DISTRUST, we use the CERTDB_TRUSTED or CERTDB_TRUSTED_CA bit,
    // because we can have active distrust for either type of cert. Note that
    // CERTDB_TERMINAL_RECORD means "stop trying to inherit trust" so if the
    // relevant trust bit isn't set then that means the cert must be considered
    // distrusted.
    uint32_t relevantTrustBit = endEntityOrCA == EndEntityOrCA::MustBeCA
                              ? CERTDB_TRUSTED_CA
                              : CERTDB_TRUSTED;
    if (((flags & (relevantTrustBit | CERTDB_TERMINAL_RECORD)))
            == CERTDB_TERMINAL_RECORD) {
      trustLevel = TrustLevel::ActivelyDistrusted;
      return Success;
    }
  }

  // mTrustedRoot is the only trust anchor for this validation.
  if (CERT_CompareCerts(mTrustedRoot.get(), candidateCert.get())) {
    trustLevel = TrustLevel::TrustAnchor;
    return Success;
  }

  trustLevel = TrustLevel::InheritsTrust;
  return Success;
}

Result
AppTrustDomain::VerifySignedData(const SignedDataWithSignature& signedData,
                                 Input subjectPublicKeyInfo)
{
  return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                           mMinimumNonECCBits, mPinArg);
}

Result
AppTrustDomain::DigestBuf(Input item, /*out*/ uint8_t* digestBuf,
                          size_t digestBufLen)
{
  return ::mozilla::pkix::DigestBuf(item, digestBuf, digestBufLen);
}

Result
AppTrustDomain::CheckRevocation(EndEntityOrCA, const CertID&, Time,
                                /*optional*/ const Input*,
                                /*optional*/ const Input*)
{
  // We don't currently do revocation checking. If we need to distrust an Apps
  // certificate, we will use the active distrust mechanism.
  return Success;
}

Result
AppTrustDomain::IsChainValid(const DERArray& certChain, Time time)
{
  SECStatus srv = ConstructCERTCertListFromReversedDERArray(certChain,
                                                            mCertChain);
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }
  return Success;
}

Result
AppTrustDomain::CheckPublicKey(Input subjectPublicKeyInfo)
{
  return ::mozilla::pkix::CheckPublicKey(subjectPublicKeyInfo,
                                         mMinimumNonECCBits);
}

} } // namespace mozilla::psm
