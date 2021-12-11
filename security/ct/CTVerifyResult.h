/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTVerifyResult_h
#define CTVerifyResult_h

#include <vector>

#include "CTLog.h"
#include "SignedCertificateTimestamp.h"

namespace mozilla {
namespace ct {

// Holds a verified Signed Certificate Timestamp along with the verification
// status (e.g. valid/invalid) and additional information related to the
// verification.
struct VerifiedSCT {
  VerifiedSCT();

  // The original SCT.
  SignedCertificateTimestamp sct;

  enum class Status {
    None,
    // The SCT is from a known log, and the signature is valid.
    Valid,
    // The SCT is from a known disqualified log, and the signature is valid.
    // For the disqualification time of the log see |logDisqualificationTime|.
    ValidFromDisqualifiedLog,
    // The SCT is from an unknown log and can not be verified.
    UnknownLog,
    // The SCT is from a known log, but the signature is invalid.
    InvalidSignature,
    // The SCT signature is valid, but the timestamp is in the future.
    // Such SCTs are considered invalid (see RFC 6962, Section 5.2).
    InvalidTimestamp,
  };

  enum class Origin {
    Unknown,
    Embedded,
    TLSExtension,
    OCSPResponse,
  };

  Status status;
  Origin origin;
  CTLogOperatorId logOperatorId;
  uint64_t logDisqualificationTime;
};

typedef std::vector<VerifiedSCT> VerifiedSCTList;

// Holds Signed Certificate Timestamps verification results.
class CTVerifyResult {
 public:
  CTVerifyResult() { Reset(); }

  // SCTs that were processed during the verification along with their
  // verification results.
  VerifiedSCTList verifiedScts;

  // The verifier makes the best effort to extract the available SCTs
  // from the binary sources provided to it.
  // If some SCT cannot be extracted due to encoding errors, the verifier
  // proceeds to the next available one. In other words, decoding errors are
  // effectively ignored.
  // Note that a serialized SCT may fail to decode for a "legitimate" reason,
  // e.g. if the SCT is from a future version of the Certificate Transparency
  // standard.
  // |decodingErrors| field counts the errors of the above kind.
  size_t decodingErrors;

  void Reset();
};

}  // namespace ct
}  // namespace mozilla

#endif  // CTVerifyResult_h
