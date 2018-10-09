/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BTVerifier_h
#define BTVerifier_h

#include "BTInclusionProof.h"
#include "mozpkix/Input.h"
#include "mozpkix/Result.h"
#include "mozpkix/pkixtypes.h"

namespace mozilla { namespace ct {

// Decodes an Inclusion Proof (InclusionProofDataV2 as defined in RFC
// 6962-bis). This consumes the entirety of the input.
pkix::Result DecodeInclusionProof(pkix::Reader& input,
  InclusionProofDataV2& output);
// Given a decoded Inclusion Proof (InclusionProofDataV2 as per RFC 6962-bis),
// the corresponding leaf data entry, and the expected root hash, returns
// Success if the proof verifies correctly.
// (https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-28#section-2.1.3.2)
pkix::Result VerifyInclusionProof(const InclusionProofDataV2& proof,
  pkix::Input leafEntry, pkix::Input expectedRootHash,
  pkix::DigestAlgorithm digestAlgorithm);

} } // namespace mozilla::ct

#endif // BTVerifier_h
