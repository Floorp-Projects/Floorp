/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSTrustDomain.h"
#include "mozilla/Base64.h"
#include "mozilla/Preferences.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "pkix/pkixnss.h"

using namespace mozilla::pkix;

namespace mozilla { namespace psm {

static LazyLogModule gTrustDomainPRLog("CSTrustDomain");
#define CSTrust_LOG(args) MOZ_LOG(gTrustDomainPRLog, LogLevel::Debug, args)

CSTrustDomain::CSTrustDomain(UniqueCERTCertList& certChain)
  : mCertChain(certChain)
  , mCertBlocklist(do_GetService(NS_CERTBLOCKLIST_CONTRACTID))
{
}

Result
CSTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                            const CertPolicyId& policy, Input candidateCertDER,
                            /*out*/ TrustLevel& trustLevel)
{
  MOZ_ASSERT(policy.IsAnyPolicy());
  if (!policy.IsAnyPolicy()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  SECItem candidateCertDERSECItem = UnsafeMapInputToSECItem(candidateCertDER);
  ScopedCERTCertificate candidateCert(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &candidateCertDERSECItem,
                            nullptr, false, true));
  if (!candidateCert) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  bool isCertRevoked;
  nsresult nsrv = mCertBlocklist->IsCertRevoked(
                    candidateCert->derIssuer.data,
                    candidateCert->derIssuer.len,
                    candidateCert->serialNumber.data,
                    candidateCert->serialNumber.len,
                    candidateCert->derSubject.data,
                    candidateCert->derSubject.len,
                    candidateCert->derPublicKey.data,
                    candidateCert->derPublicKey.len,
                    &isCertRevoked);
  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  if (isCertRevoked) {
    CSTrust_LOG(("CSTrustDomain: certificate is revoked\n"));
    return Result::ERROR_REVOKED_CERTIFICATE;
  }

  // Is this cert our built-in content signing root?
  bool isRoot = false;
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsrv = component->IsCertContentSigningRoot(candidateCert.get(), isRoot);
  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (isRoot) {
    CSTrust_LOG(("CSTrustDomain: certificate is a trust anchor\n"));
    trustLevel = TrustLevel::TrustAnchor;
    return Success;
  }
  CSTrust_LOG(("CSTrustDomain: certificate is *not* a trust anchor\n"));

  trustLevel = TrustLevel::InheritsTrust;
  return Success;
}

Result
CSTrustDomain::FindIssuer(Input encodedIssuerName, IssuerChecker& checker,
                          Time time)
{
  // Loop over the chain, look for a matching subject
  for (CERTCertListNode* n = CERT_LIST_HEAD(mCertChain);
      !CERT_LIST_END(n, mCertChain); n = CERT_LIST_NEXT(n)) {
    Input certDER;
    Result rv = certDER.Init(n->cert->derCert.data, n->cert->derCert.len);
    if (rv != Success) {
      continue; // probably too big
    }

    // if the subject does not match, try the next certificate
    Input subjectDER;
    rv = subjectDER.Init(n->cert->derSubject.data, n->cert->derSubject.len);
    if (rv != Success) {
      continue; // just try the next one
    }
    if (!InputsAreEqual(subjectDER, encodedIssuerName)) {
      CSTrust_LOG(("CSTrustDomain: subjects don't match\n"));
      continue;
    }

    // If the subject does match, try the next step
    bool keepGoing;
    rv = checker.Check(certDER, nullptr/*additionalNameConstraints*/,
                       keepGoing);
    if (rv != Success) {
      return rv;
    }
    if (!keepGoing) {
      CSTrust_LOG(("CSTrustDomain: don't keep going\n"));
      break;
    }
  }

  return Success;
}

Result
CSTrustDomain::CheckRevocation(EndEntityOrCA endEntityOrCA,
                               const CertID& certID, Time time,
                               Duration validityDuration,
                               /*optional*/ const Input* stapledOCSPresponse,
                               /*optional*/ const Input* aiaExtension)
{
  // We're relying solely on the CertBlocklist for revocation - and we're
  // performing checks on this in GetCertTrust (as per nsNSSCertDBTrustDomain)
  return Success;
}

Result
CSTrustDomain::IsChainValid(const DERArray& certChain, Time time)
{
  // Check that our chain is not empty
  if (certChain.GetLength() == 0) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  return Success;
}

Result
CSTrustDomain::CheckSignatureDigestAlgorithm(DigestAlgorithm digestAlg,
                                             EndEntityOrCA endEntityOrCA,
                                             Time notBefore)
{
  if (digestAlg == DigestAlgorithm::sha1) {
    return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
  }
  return Success;
}

Result
CSTrustDomain::CheckRSAPublicKeyModulusSizeInBits(
  EndEntityOrCA endEntityOrCA, unsigned int modulusSizeInBits)
{
  if (modulusSizeInBits < 2048) {
    return Result::ERROR_INADEQUATE_KEY_SIZE;
  }
  return Success;
}

Result
CSTrustDomain::VerifyRSAPKCS1SignedDigest(const SignedDigest& signedDigest,
                                          Input subjectPublicKeyInfo)
{
  return VerifyRSAPKCS1SignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                       nullptr);
}

Result
CSTrustDomain::CheckECDSACurveIsAcceptable(EndEntityOrCA endEntityOrCA,
                                           NamedCurve curve)
{
  switch (curve) {
    case NamedCurve::secp256r1: // fall through
    case NamedCurve::secp384r1: // fall through
    case NamedCurve::secp521r1:
      return Success;
  }

  return Result::ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
}

Result
CSTrustDomain::VerifyECDSASignedDigest(const SignedDigest& signedDigest,
                                       Input subjectPublicKeyInfo)
{
  return VerifyECDSASignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                    nullptr);
}

Result
CSTrustDomain::CheckValidityIsAcceptable(Time notBefore, Time notAfter,
                                         EndEntityOrCA endEntityOrCA,
                                         KeyPurposeId keyPurpose)
{
  return Success;
}

Result
CSTrustDomain::NetscapeStepUpMatchesServerAuth(Time notBefore,
                                               /*out*/ bool& matches)
{
  matches = false;
  return Success;
}

Result
CSTrustDomain::DigestBuf(Input item, DigestAlgorithm digestAlg,
                         /*out*/ uint8_t* digestBuf, size_t digestBufLen)
{
  return DigestBufNSS(item, digestAlg, digestBuf, digestBufLen);
}

} } // end namespace mozilla::psm
