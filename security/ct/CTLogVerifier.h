/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTLogVerifier_h
#define CTLogVerifier_h

#include <memory>

#include "CTLog.h"
#include "CTUtils.h"
#include "SignedCertificateTimestamp.h"
#include "mozpkix/Input.h"
#include "mozpkix/Result.h"
#include "mozpkix/pkix.h"

namespace mozilla {
namespace ct {

// Verifies Signed Certificate Timestamps (SCTs) provided by a specific log
// using the public key of that log. Assumes the SCT being verified
// matches the log by log key ID and signature parameters (an error is returned
// otherwise).
// The verification functions return Success if the provided SCT has passed
// verification, ERROR_BAD_SIGNATURE if failed verification, or other result
// on error.
class CTLogVerifier {
 public:
  CTLogVerifier();

  // Initializes the verifier with log-specific information. Only the public
  // key is used for verification, other parameters are purely informational.
  // |subjectPublicKeyInfo| is a DER-encoded SubjectPublicKeyInfo.
  // |operatorId| The numeric ID of the log operator as assigned at
  // https://www.certificate-transparency.org/known-logs .
  // |logStatus| Either "Included" or "Disqualified".
  // |disqualificationTime| Disqualification timestamp (for disqualified logs).
  // An error is returned if |subjectPublicKeyInfo| refers to an unsupported
  // public key.
  pkix::Result Init(pkix::Input subjectPublicKeyInfo,
                    CTLogOperatorId operatorId, CTLogStatus logStatus,
                    uint64_t disqualificationTime);

  // Returns the log's key ID, which is a SHA256 hash of its public key.
  // See RFC 6962, Section 3.2.
  const Buffer& keyId() const { return mKeyId; }

  CTLogOperatorId operatorId() const { return mOperatorId; }
  bool isDisqualified() const { return mDisqualified; }
  uint64_t disqualificationTime() const { return mDisqualificationTime; }

  // Verifies that |sct| contains a valid signature for |entry|.
  // |sct| must be signed by the verifier's log.
  pkix::Result Verify(const LogEntry& entry,
                      const SignedCertificateTimestamp& sct);

  // Returns true if the signature and hash algorithms in |signature|
  // match those of the log.
  bool SignatureParametersMatch(const DigitallySigned& signature);

 private:
  // Performs the underlying verification using the log's public key. Note
  // that |signature| contains the raw signature data (i.e. without any
  // DigitallySigned struct encoding).
  // Returns Success if passed verification, ERROR_BAD_SIGNATURE if failed
  // verification, or other result on error.
  pkix::Result VerifySignature(pkix::Input data, pkix::Input signature);
  pkix::Result VerifySignature(const Buffer& data, const Buffer& signature);

  // mPublicECKey works around an architectural deficiency in NSS. In the case
  // of EC, if we don't create, import, and cache this key, NSS will import and
  // verify it every signature verification, which is slow. For RSA, this is
  // unused and will be null.
  UniqueSECKEYPublicKey mPublicECKey;
  Buffer mSubjectPublicKeyInfo;
  Buffer mKeyId;
  DigitallySigned::SignatureAlgorithm mSignatureAlgorithm;
  CTLogOperatorId mOperatorId;
  bool mDisqualified;
  uint64_t mDisqualificationTime;
};

}  // namespace ct
}  // namespace mozilla

#endif  // CTLogVerifier_h
