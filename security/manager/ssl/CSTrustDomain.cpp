/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSTrustDomain.h"
#include "NSSCertDBTrustDomain.h"
#include "cert_storage/src/cert_storage.h"
#include "mozilla/Base64.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozpkix/pkixnss.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNSSComponent.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::pkix;

namespace mozilla {
namespace psm {

static LazyLogModule gTrustDomainPRLog("CSTrustDomain");
#define CSTrust_LOG(args) MOZ_LOG(gTrustDomainPRLog, LogLevel::Debug, args)

CSTrustDomain::CSTrustDomain(nsTArray<nsTArray<uint8_t>>& certList)
    : mCertList(certList), mCertBlocklist(do_GetService(NS_CERT_STORAGE_CID)) {}

Result CSTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                                   const CertPolicyId& policy,
                                   Input candidateCertDER,
                                   /*out*/ TrustLevel& trustLevel) {
  MOZ_ASSERT(policy.IsAnyPolicy());
  if (!policy.IsAnyPolicy()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  nsTArray<uint8_t> issuerBytes;
  nsTArray<uint8_t> serialBytes;
  nsTArray<uint8_t> subjectBytes;
  nsTArray<uint8_t> pubKeyBytes;

  Result result =
      BuildRevocationCheckArrays(candidateCertDER, endEntityOrCA, issuerBytes,
                                 serialBytes, subjectBytes, pubKeyBytes);
  if (result != Success) {
    return result;
  }

  int16_t revocationState;
  nsresult nsrv = mCertBlocklist->GetRevocationState(
      issuerBytes, serialBytes, subjectBytes, pubKeyBytes, &revocationState);
  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  if (revocationState == nsICertStorage::STATE_ENFORCE) {
    CSTrust_LOG(("CSTrustDomain: certificate is revoked\n"));
    return Result::ERROR_REVOKED_CERTIFICATE;
  }

  // Is this cert our built-in content signing root?
  bool isRoot = false;
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsTArray<uint8_t> candidateCert(candidateCertDER.UnsafeGetData(),
                                  candidateCertDER.GetLength());
  nsrv = component->IsCertContentSigningRoot(candidateCert, &isRoot);
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

Result CSTrustDomain::FindIssuer(Input encodedIssuerName,
                                 IssuerChecker& checker, Time time) {
  // Loop over the chain, look for a matching subject
  for (const auto& certBytes : mCertList) {
    Input certInput;
    Result rv = certInput.Init(certBytes.Elements(), certBytes.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    bool keepGoing;
    rv = checker.Check(certInput, nullptr /*additionalNameConstraints*/,
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

Result CSTrustDomain::CheckRevocation(
    EndEntityOrCA endEntityOrCA, const CertID& certID, Time time,
    Duration validityDuration,
    /*optional*/ const Input* stapledOCSPresponse,
    /*optional*/ const Input* aiaExtension,
    /*optional*/ const Input* sctExtension) {
  // We're relying solely on the CertBlocklist for revocation - and we're
  // performing checks on this in GetCertTrust (as per nsNSSCertDBTrustDomain)
  return Success;
}

Result CSTrustDomain::IsChainValid(const DERArray& certChain, Time time,
                                   const CertPolicyId& requiredPolicy) {
  MOZ_ASSERT(requiredPolicy.IsAnyPolicy());
  // Check that our chain is not empty
  if (certChain.GetLength() == 0) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  return Success;
}

Result CSTrustDomain::CheckSignatureDigestAlgorithm(DigestAlgorithm digestAlg,
                                                    EndEntityOrCA endEntityOrCA,
                                                    Time notBefore) {
  switch (digestAlg) {
    case DigestAlgorithm::sha256:  // fall through
    case DigestAlgorithm::sha384:  // fall through
    case DigestAlgorithm::sha512:
      return Success;
    case DigestAlgorithm::sha1:
      return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
  }
}

Result CSTrustDomain::CheckRSAPublicKeyModulusSizeInBits(
    EndEntityOrCA endEntityOrCA, unsigned int modulusSizeInBits) {
  if (modulusSizeInBits < 2048) {
    return Result::ERROR_INADEQUATE_KEY_SIZE;
  }
  return Success;
}

Result CSTrustDomain::VerifyRSAPKCS1SignedData(Input data,
                                               DigestAlgorithm digestAlgorithm,
                                               Input signature,
                                               Input subjectPublicKeyInfo) {
  return VerifyRSAPKCS1SignedDataNSS(data, digestAlgorithm, signature,
                                     subjectPublicKeyInfo, nullptr);
}

Result CSTrustDomain::VerifyRSAPSSSignedData(Input data,
                                             DigestAlgorithm digestAlgorithm,
                                             Input signature,
                                             Input subjectPublicKeyInfo) {
  return VerifyRSAPSSSignedDataNSS(data, digestAlgorithm, signature,
                                   subjectPublicKeyInfo, nullptr);
}

Result CSTrustDomain::CheckECDSACurveIsAcceptable(EndEntityOrCA endEntityOrCA,
                                                  NamedCurve curve) {
  switch (curve) {
    case NamedCurve::secp256r1:  // fall through
    case NamedCurve::secp384r1:  // fall through
    case NamedCurve::secp521r1:
      return Success;
  }

  return Result::ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
}

Result CSTrustDomain::VerifyECDSASignedData(Input data,
                                            DigestAlgorithm digestAlgorithm,
                                            Input signature,
                                            Input subjectPublicKeyInfo) {
  return VerifyECDSASignedDataNSS(data, digestAlgorithm, signature,
                                  subjectPublicKeyInfo, nullptr);
}

Result CSTrustDomain::CheckValidityIsAcceptable(Time notBefore, Time notAfter,
                                                EndEntityOrCA endEntityOrCA,
                                                KeyPurposeId keyPurpose) {
  return Success;
}

Result CSTrustDomain::NetscapeStepUpMatchesServerAuth(Time notBefore,
                                                      /*out*/ bool& matches) {
  matches = false;
  return Success;
}

void CSTrustDomain::NoteAuxiliaryExtension(AuxiliaryExtension /*extension*/,
                                           Input /*extensionData*/) {}

Result CSTrustDomain::DigestBuf(Input item, DigestAlgorithm digestAlg,
                                /*out*/ uint8_t* digestBuf,
                                size_t digestBufLen) {
  return DigestBufNSS(item, digestAlg, digestBuf, digestBufLen);
}

}  // namespace psm
}  // namespace mozilla
