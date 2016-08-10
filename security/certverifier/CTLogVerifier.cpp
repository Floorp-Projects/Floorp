/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTLogVerifier.h"

#include "CTSerialization.h"
#include "hasht.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "pkix/pkixnss.h"
#include "pkixutil.h"

namespace mozilla { namespace ct {

using namespace mozilla::pkix;

// A TrustDomain used to extract the SCT log signature parameters
// given its subjectPublicKeyInfo.
// Only RSASSA-PKCS1v15 with SHA-256 and ECDSA (using the NIST P-256 curve)
// with SHA-256 are allowed.
// RSA keys must be at least 2048 bits.
// See See RFC 6962, Section 2.1.4.
class SignatureParamsTrustDomain final : public TrustDomain
{
public:
  SignatureParamsTrustDomain()
    : mSignatureAlgorithm(DigitallySigned::SignatureAlgorithm::Anonymous)
  {
  }

  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&, Input,
                      TrustLevel&) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result FindIssuer(Input, IssuerChecker&, Time) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         const Input*, const Input*) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result IsChainValid(const DERArray&, Time) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result DigestBuf(Input, DigestAlgorithm, uint8_t*, size_t) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result CheckSignatureDigestAlgorithm(DigestAlgorithm, EndEntityOrCA,
                                       Time) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result CheckECDSACurveIsAcceptable(EndEntityOrCA, NamedCurve curve) override
  {
    MOZ_ASSERT(mSignatureAlgorithm ==
      DigitallySigned::SignatureAlgorithm::Anonymous);
    if (curve != NamedCurve::secp256r1) {
      return Result::ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
    }
    mSignatureAlgorithm = DigitallySigned::SignatureAlgorithm::ECDSA;
    return Success;
  }

  Result VerifyECDSASignedDigest(const SignedDigest&, Input) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result CheckRSAPublicKeyModulusSizeInBits(EndEntityOrCA,
                                            unsigned int modulusSizeInBits)
                                            override
  {
    MOZ_ASSERT(mSignatureAlgorithm ==
      DigitallySigned::SignatureAlgorithm::Anonymous);
    // Require RSA keys of at least 2048 bits. See RFC 6962, Section 2.1.4.
    if (modulusSizeInBits < 2048) {
      return Result::ERROR_INADEQUATE_KEY_SIZE;
    }
    mSignatureAlgorithm = DigitallySigned::SignatureAlgorithm::RSA;
    return Success;
  }

  Result VerifyRSAPKCS1SignedDigest(const SignedDigest&, Input) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result CheckValidityIsAcceptable(Time, Time, EndEntityOrCA,
                                   KeyPurposeId) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result NetscapeStepUpMatchesServerAuth(Time, bool&) override
  {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  void NoteAuxiliaryExtension(AuxiliaryExtension, Input) override
  {
  }

  DigitallySigned::SignatureAlgorithm mSignatureAlgorithm;
};


CTLogVerifier::CTLogVerifier()
  : mSignatureAlgorithm(DigitallySigned::SignatureAlgorithm::Anonymous)
{
}

Result
CTLogVerifier::Init(Input subjectPublicKeyInfo)
{
  SignatureParamsTrustDomain trustDomain;
  Result rv = CheckSubjectPublicKeyInfo(subjectPublicKeyInfo, trustDomain,
                                        EndEntityOrCA::MustBeEndEntity);
  if (rv != Success) {
    return rv;
  }
  mSignatureAlgorithm = trustDomain.mSignatureAlgorithm;

  rv = InputToBuffer(subjectPublicKeyInfo, mSubjectPublicKeyInfo);
  if (rv != Success) {
    return rv;
  }

  if (!mKeyId.resizeUninitialized(SHA256_LENGTH)) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  rv = DigestBufNSS(subjectPublicKeyInfo, DigestAlgorithm::sha256,
                    mKeyId.begin(), mKeyId.length());
  if (rv != Success) {
    return rv;
  }

  return Success;
}

Result
CTLogVerifier::Verify(const LogEntry& entry,
                      const SignedCertificateTimestamp& sct)
{
  if (mKeyId.empty() || sct.logId != mKeyId) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  if (!SignatureParametersMatch(sct.signature)) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Buffer serializedLogEntry;
  Result rv = EncodeLogEntry(entry, serializedLogEntry);
  if (rv != Success) {
    return rv;
  }

  Input logEntryInput;
  rv = BufferToInput(serializedLogEntry, logEntryInput);
  if (rv != Success) {
    return rv;
  }
  Input sctExtensionsInput;
  rv = BufferToInput(sct.extensions, sctExtensionsInput);
  if (rv != Success) {
    return rv;
  }

  Buffer serializedData;
  rv = EncodeV1SCTSignedData(sct.timestamp, logEntryInput, sctExtensionsInput,
                             serializedData);
  if (rv != Success) {
    return rv;
  }
  return VerifySignature(serializedData, sct.signature.signatureData);
}

Result
CTLogVerifier::VerifySignedTreeHead(const SignedTreeHead& sth)
{
  if (!SignatureParametersMatch(sth.signature)) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Buffer serializedData;
  Result rv = EncodeTreeHeadSignature(sth, serializedData);
  if (rv != Success) {
    return rv;
  }
  return VerifySignature(serializedData, sth.signature.signatureData);
}

bool
CTLogVerifier::SignatureParametersMatch(const DigitallySigned& signature)
{
  return signature.SignatureParametersMatch(
    DigitallySigned::HashAlgorithm::SHA256, mSignatureAlgorithm);
}

Result
CTLogVerifier::VerifySignature(Input data, Input signature)
{
  uint8_t digest[SHA256_LENGTH];
  Result rv = DigestBufNSS(data, DigestAlgorithm::sha256, digest,
                           ArrayLength(digest));
  if (rv != Success) {
    return rv;
  }

  SignedDigest signedDigest;
  signedDigest.digestAlgorithm = DigestAlgorithm::sha256;
  rv = signedDigest.digest.Init(digest, ArrayLength(digest));
  if (rv != Success) {
    return rv;
  }
  rv = signedDigest.signature.Init(signature);
  if (rv != Success) {
    return rv;
  }

  Input spki;
  rv = BufferToInput(mSubjectPublicKeyInfo, spki);
  if (rv != Success) {
    return rv;
  }

  switch (mSignatureAlgorithm) {
    case DigitallySigned::SignatureAlgorithm::RSA:
      rv = VerifyRSAPKCS1SignedDigestNSS(signedDigest, spki, nullptr);
      break;
    case DigitallySigned::SignatureAlgorithm::ECDSA:
      rv = VerifyECDSASignedDigestNSS(signedDigest, spki, nullptr);
      break;
    // We do not expect new values added to this enum any time soon,
    // so just listing all the available ones seems to be the easiest way
    // to suppress warning C4061 on MSVC (which expects all values of the
    // enum to be explicitly handled).
    case DigitallySigned::SignatureAlgorithm::Anonymous:
    case DigitallySigned::SignatureAlgorithm::DSA:
    default:
      MOZ_ASSERT_UNREACHABLE("RSA/ECDSA expected");
      return Result::FATAL_ERROR_INVALID_ARGS;
  }
  if (rv != Success) {
    if (IsFatalError(rv)) {
      return rv;
    }
    // If the error is non-fatal, we assume the signature was invalid.
    return Result::ERROR_BAD_SIGNATURE;
  }
  return Success;
}

Result
CTLogVerifier::VerifySignature(const Buffer& data, const Buffer& signature)
{
  Input dataInput;
  Result rv = BufferToInput(data, dataInput);
  if (rv != Success) {
    return rv;
  }
  Input signatureInput;
  rv = BufferToInput(signature, signatureInput);
  if (rv != Success) {
    return rv;
  }
  return VerifySignature(dataInput, signatureInput);
}

} } // namespace mozilla::ct
