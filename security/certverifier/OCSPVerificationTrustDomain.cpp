/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPVerificationTrustDomain.h"

using namespace mozilla;
using namespace mozilla::pkix;

namespace mozilla { namespace psm {

OCSPVerificationTrustDomain::OCSPVerificationTrustDomain(
  NSSCertDBTrustDomain& certDBTrustDomain)
  : mCertDBTrustDomain(certDBTrustDomain)
{
}

Result
OCSPVerificationTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                                          const CertPolicyId& policy,
                                          Input candidateCertDER,
                                  /*out*/ TrustLevel& trustLevel)
{
  return mCertDBTrustDomain.GetCertTrust(endEntityOrCA, policy,
                                         candidateCertDER, trustLevel);
}


Result
OCSPVerificationTrustDomain::FindIssuer(Input, IssuerChecker&, Time)
{
  // We do not expect this to be called for OCSP signers
  return Result::FATAL_ERROR_LIBRARY_FAILURE;
}

Result
OCSPVerificationTrustDomain::IsChainValid(const DERArray&, Time)
{
  // We do not expect this to be called for OCSP signers
  return Result::FATAL_ERROR_LIBRARY_FAILURE;
}

Result
OCSPVerificationTrustDomain::CheckRevocation(EndEntityOrCA, const CertID&,
                                             Time, Duration, const Input*,
                                             const Input*)
{
  // We do not expect this to be called for OCSP signers
  return Result::FATAL_ERROR_LIBRARY_FAILURE;
}

Result
OCSPVerificationTrustDomain::CheckSignatureDigestAlgorithm(
  DigestAlgorithm aAlg, EndEntityOrCA aEEOrCA, Time notBefore)
{
  // The reason for wrapping the NSSCertDBTrustDomain in an
  // OCSPVerificationTrustDomain is to allow us to bypass the weaker signature
  // algorithm check - thus all allowable signature digest algorithms should
  // always be accepted. This is only needed while we gather telemetry on SHA-1.
  return Success;
}

Result
OCSPVerificationTrustDomain::CheckRSAPublicKeyModulusSizeInBits(
  EndEntityOrCA aEEOrCA, unsigned int aModulusSizeInBits)
{
  return mCertDBTrustDomain.
      CheckRSAPublicKeyModulusSizeInBits(aEEOrCA, aModulusSizeInBits);
}

Result
OCSPVerificationTrustDomain::VerifyRSAPKCS1SignedDigest(
  const SignedDigest& aSignedDigest, Input aSubjectPublicKeyInfo)
{
  return mCertDBTrustDomain.VerifyRSAPKCS1SignedDigest(aSignedDigest,
                                                       aSubjectPublicKeyInfo);
}

Result
OCSPVerificationTrustDomain::CheckECDSACurveIsAcceptable(
  EndEntityOrCA aEEOrCA, NamedCurve aCurve)
{
  return mCertDBTrustDomain.CheckECDSACurveIsAcceptable(aEEOrCA, aCurve);
}

Result
OCSPVerificationTrustDomain::VerifyECDSASignedDigest(
  const SignedDigest& aSignedDigest, Input aSubjectPublicKeyInfo)
{
  return mCertDBTrustDomain.VerifyECDSASignedDigest(aSignedDigest,
                                                    aSubjectPublicKeyInfo);
}

Result
OCSPVerificationTrustDomain::CheckValidityIsAcceptable(
  Time notBefore, Time notAfter, EndEntityOrCA endEntityOrCA,
  KeyPurposeId keyPurpose)
{
  return mCertDBTrustDomain.CheckValidityIsAcceptable(notBefore, notAfter,
                                                      endEntityOrCA,
                                                      keyPurpose);
}

Result
OCSPVerificationTrustDomain::NetscapeStepUpMatchesServerAuth(Time notBefore,
                                                     /*out*/ bool& matches)
{
  return mCertDBTrustDomain.NetscapeStepUpMatchesServerAuth(notBefore, matches);
}

void
OCSPVerificationTrustDomain::NoteAuxiliaryExtension(
  AuxiliaryExtension extension, Input extensionData)
{
  mCertDBTrustDomain.NoteAuxiliaryExtension(extension, extensionData);
}

Result
OCSPVerificationTrustDomain::DigestBuf(
  Input item, DigestAlgorithm digestAlg,
  /*out*/ uint8_t* digestBuf, size_t digestBufLen)
{
  return mCertDBTrustDomain.DigestBuf(item, digestAlg, digestBuf, digestBufLen);
}

} } // namespace mozilla::psm
