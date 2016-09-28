/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTVerifyResult_h
#define CTVerifyResult_h

#include "mozilla/Vector.h"
#include "SignedCertificateTimestamp.h"

namespace mozilla { namespace ct {

typedef Vector<SignedCertificateTimestamp> SCTList;

// Holds Signed Certificate Timestamps verification results.
class CTVerifyResult
{
public:
  // SCTs that were processed during the verification. For each SCT,
  // the verification result is stored in its |verificationStatus| field.
  SCTList scts;

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

} } // namespace mozilla::ct

#endif  // CTVerifyResult_h
