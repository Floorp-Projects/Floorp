/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BTVerifier_h
#define BTVerifier_h

#include "BTInclusionProof.h"
#include "pkix/Input.h"
#include "pkix/Result.h"

namespace mozilla { namespace ct {

// Decodes an Inclusion Proof (InclusionProofDataV2 as defined in RFC
// 6962-bis). This consumes the entirety of the input.
pkix::Result DecodeInclusionProof(pkix::Reader& input,
  InclusionProofDataV2& output);

} } // namespace mozilla::ct

#endif // BTVerifier_h
