/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BTVerifier.h"
#include "CTUtils.h"
#include "SignedCertificateTimestamp.h"

#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "mozilla/TypeTraits.h"

namespace mozilla { namespace ct {

using namespace mozilla::pkix;

typedef mozilla::pkix::Result Result;

// Members of a Inclusion Proof struct
static const size_t kLogIdPrefixLengthBytes = 1;
static const size_t kProofTreeSizeLength = 8;
static const size_t kLeafIndexLength = 8;
static const size_t kInclusionPathLengthBytes = 2;
static const size_t kNodeHashPrefixLengthBytes = 1;

Result
DecodeInclusionProof(pkix::Reader& reader, InclusionProofDataV2& output)
{
  InclusionProofDataV2 result;

  Input logId;
  Result rv = ReadVariableBytes<kLogIdPrefixLengthBytes>(reader, logId);
  if (rv != Success) {
    return rv;
  }

  rv = ReadUint<kProofTreeSizeLength>(reader, result.treeSize);
  if (rv != Success) {
    return rv;
  }

  if (result.treeSize < 1) {
    return pkix::Result::ERROR_BAD_DER;
  }

  rv = ReadUint<kLeafIndexLength>(reader, result.leafIndex);
  if (rv != Success) {
    return rv;
  }

  if (result.leafIndex >= result.treeSize) {
    return pkix::Result::ERROR_BAD_DER;
  }

  Input pathInput;
  rv = ReadVariableBytes<kInclusionPathLengthBytes>(reader, pathInput);
  if (rv != Success) {
    return rv;
  }

  if (pathInput.GetLength() < 1) {
    return pkix::Result::ERROR_BAD_DER;
  }

  Reader pathReader(pathInput);
  Vector<Buffer, kInitialPathLengthCapacity> inclusionPath;

  while (!pathReader.AtEnd()) {
    Input hash;
    rv = ReadVariableBytes<kNodeHashPrefixLengthBytes>(pathReader, hash);
    if (rv != Success) {
      return rv;
    }

    Buffer hashBuffer;
    rv = InputToBuffer(hash, hashBuffer);
    if (rv != Success) {
      return rv;
    }

    if (!inclusionPath.append(std::move(hashBuffer))) {
      return pkix::Result::FATAL_ERROR_NO_MEMORY;
    }
  }

  if (!reader.AtEnd()){
    return pkix::Result::ERROR_BAD_DER;
  }

  rv = InputToBuffer(logId, result.logId);
  if (rv != Success) {
    return rv;
  }

  result.inclusionPath = std::move(inclusionPath);

  output = std::move(result);
  return Success;
}
} } //namespace mozilla::ct
