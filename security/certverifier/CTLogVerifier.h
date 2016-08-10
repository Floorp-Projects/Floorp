/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTLogVerifier_h
#define CTLogVerifier_h

#include "pkix/Input.h"
#include "pkix/pkix.h"
#include "SignedCertificateTimestamp.h"
#include "SignedTreeHead.h"

namespace mozilla { namespace ct {

// Verifies Signed Certificate Timestamps (SCTs) provided by a specific log
// using the public key of that log. Assumes the SCT being verified
// matches the log by log key ID and signature parameters (an error is returned
// otherwise).
// The verification functions return Success if the provided SCT has passed
// verification, ERROR_BAD_SIGNATURE if failed verification, or other result
// on error.
class CTLogVerifier
{
public:
  CTLogVerifier();

  // Initializes the verifier with log-specific information.
  // |subjectPublicKeyInfo| is a DER-encoded SubjectPublicKeyInfo.
  // An error is returned if |subjectPublicKeyInfo| refers to an unsupported
  // public key.
  pkix::Result Init(pkix::Input subjectPublicKeyInfo);

  // Returns the log's key ID, which is a SHA256 hash of its public key.
  // See RFC 6962, Section 3.2.
  const Buffer& keyId() const { return mKeyId; }

  // Verifies that |sct| contains a valid signature for |entry|.
  // |sct| must be signed by the verifier's log.
  pkix::Result Verify(const LogEntry& entry,
                      const SignedCertificateTimestamp& sct);

  // Verifies the signature in |sth|.
  // |sth| must be signed by the verifier's log.
  pkix::Result VerifySignedTreeHead(const SignedTreeHead& sth);

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

  Buffer mSubjectPublicKeyInfo;
  Buffer mKeyId;
  DigitallySigned::SignatureAlgorithm mSignatureAlgorithm;
};

} } // namespace mozilla::ct

#endif // CTLogVerifier_h
