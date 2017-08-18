/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BTInclusionProof_h
#define BTInclusionProof_h

#include "Buffer.h"
#include "mozilla/Vector.h"

namespace mozilla { namespace ct {

// Represents a Merkle inclusion proof for purposes of serialization,
// deserialization, and verification of the proof.  The format for inclusion
// proofs in RFC 6962-bis is as follows:
//
//    opaque LogID<2..127>;
//    opaque NodeHash<32..2^8-1>;
//
//     struct {
//         LogID log_id;
//         uint64 tree_size;
//         uint64 leaf_index;
//         NodeHash inclusion_path<1..2^16-1>;
//     } InclusionProofDataV2;

const uint64_t kInitialPathLengthCapacity = 32;

struct InclusionProofDataV2
{
  Buffer logId;
  uint64_t treeSize;
  uint64_t leafIndex;
  Vector<Buffer, kInitialPathLengthCapacity> inclusionPath;
};

} } // namespace mozilla:ct

#endif // BTInclusionProof_h
