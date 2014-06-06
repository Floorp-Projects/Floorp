/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pkixder.h"
#include "pkix/bind.h"
#include "cert.h"

namespace mozilla { namespace pkix { namespace der {

// not inline
Result
Fail(PRErrorCode errorCode)
{
  PR_SetError(errorCode, 0);
  return Failure;
}

namespace internal {

// Too complicated to be inline
Result
ExpectTagAndGetLength(Input& input, uint8_t expectedTag, uint16_t& length)
{
  PR_ASSERT((expectedTag & 0x1F) != 0x1F); // high tag number form not allowed

  uint8_t tag;
  if (input.Read(tag) != Success) {
    return Failure;
  }

  if (tag != expectedTag) {
    return Fail(SEC_ERROR_BAD_DER);
  }

  // The short form of length is a single byte with the high order bit set
  // to zero. The long form of length is one byte with the high order bit
  // set, followed by N bytes, where N is encoded in the lowest 7 bits of
  // the first byte.
  uint8_t length1;
  if (input.Read(length1) != Success) {
    return Failure;
  }
  if (!(length1 & 0x80)) {
    length = length1;
  } else if (length1 == 0x81) {
    uint8_t length2;
    if (input.Read(length2) != Success) {
      return Failure;
    }
    if (length2 < 128) {
      // Not shortest possible encoding
      return Fail(SEC_ERROR_BAD_DER);
    }
    length = length2;
  } else if (length1 == 0x82) {
    if (input.Read(length) != Success) {
      return Failure;
    }
    if (length < 256) {
      // Not shortest possible encoding
      return Fail(SEC_ERROR_BAD_DER);
    }
  } else {
    // We don't support lengths larger than 2^16 - 1.
    return Fail(SEC_ERROR_BAD_DER);
  }

  // Ensure the input is long enough for the length it says it has.
  return input.EnsureLength(length);
}

} // namespace internal

Result
SignedData(Input& input, /*out*/ Input& tbs, /*out*/ CERTSignedData& signedData)
{
  Input::Mark mark(input.GetMark());

  if (ExpectTagAndGetValue(input, SEQUENCE, tbs) != Success) {
    return Failure;
  }

  if (input.GetSECItem(siBuffer, mark, signedData.data) != Success) {
    return Failure;
  }

  if (Nested(input, SEQUENCE,
             bind(AlgorithmIdentifier, _1, ref(signedData.signatureAlgorithm)))
        != Success) {
    return Failure;
  }

  if (ExpectTagAndGetValue(input, BIT_STRING, signedData.signature)
        != Success) {
    return Failure;
  }
  if (signedData.signature.len == 0) {
    return Fail(SEC_ERROR_BAD_SIGNATURE);
  }
  unsigned int unusedBitsAtEnd = signedData.signature.data[0];
  // XXX: Really the constraint should be that unusedBitsAtEnd must be less
  // than 7. But, we suspect there are no real-world OCSP responses or X.509
  // certificates with non-zero unused bits. It seems like NSS assumes this in
  // various places, so we enforce it too in order to simplify this code. If we
  // find compatibility issues, we'll know we're wrong and we'll have to figure
  // out how to shift the bits around.
  if (unusedBitsAtEnd != 0) {
    return Fail(SEC_ERROR_BAD_SIGNATURE);
  }
  ++signedData.signature.data;
  --signedData.signature.len;
  signedData.signature.len = (signedData.signature.len << 3); // Bytes to bits

  return Success;
}

} } } // namespace mozilla::pkix::der
