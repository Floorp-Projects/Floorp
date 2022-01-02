/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BTVerifier_h
#define BTVerifier_h

#include "BTTypes.h"
#include "mozpkix/Input.h"
#include "mozpkix/Result.h"
#include "mozpkix/pkixder.h"
#include "mozpkix/pkixtypes.h"

namespace mozilla {
namespace ct {

// Given an encoded subject public key info, a digest algorithm, a public key
// algorithm, and an encoded signed tree head (SignedTreeHeadV2 as defined in
// RFC 6962-bis), decodes and verifies the STH. Currently only ECDSA keys are
// supported. SHA-256 and SHA-512 are supported, but according to the
// specification only SHA-256 should be supported at this time.
pkix::Result DecodeAndVerifySignedTreeHead(
    pkix::Input signerSubjectPublicKeyInfo,
    pkix::DigestAlgorithm digestAlgorithm,
    pkix::der::PublicKeyAlgorithm publicKeyAlg, pkix::Input signedTreeHeadInput,
    SignedTreeHeadDataV2& signedTreeHead);

// Decodes an Inclusion Proof (InclusionProofDataV2 as defined in RFC
// 6962-bis). This consumes the entirety of the input.
pkix::Result DecodeInclusionProof(pkix::Input input,
                                  InclusionProofDataV2& output);
// Given a decoded Inclusion Proof (InclusionProofDataV2 as per RFC 6962-bis),
// the corresponding leaf data entry, and the expected root hash, returns
// Success if the proof verifies correctly.
// (https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-28#section-2.1.3.2)
pkix::Result VerifyInclusionProof(const InclusionProofDataV2& proof,
                                  pkix::Input leafEntry,
                                  pkix::Input expectedRootHash,
                                  pkix::DigestAlgorithm digestAlgorithm);

}  // namespace ct
}  // namespace mozilla

#endif  // BTVerifier_h
