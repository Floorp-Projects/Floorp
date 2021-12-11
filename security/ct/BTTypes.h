/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BTTypes_h
#define BTTypes_h

#include <vector>

#include "Buffer.h"

namespace mozilla {
namespace ct {

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

struct InclusionProofDataV2 {
  Buffer logId;
  uint64_t treeSize;
  uint64_t leafIndex;
  std::vector<Buffer> inclusionPath;
};

// Represents a Signed Tree Head as per RFC 6962-bis. All extensions are
// ignored. The signature field covers the data in the tree_head field.
//
//     struct {
//         LogID log_id;
//         TreeHeadDataV2 tree_head;
//         opaque signature<0..2^16-1>;
//     } SignedTreeHeadDataV2;
//
//     struct {
//         uint64 timestamp;
//         uint64 tree_size;
//         NodeHash root_hash;
//         Extension sth_extensions<0..2^16-1>;
//     } TreeHeadDataV2;

struct SignedTreeHeadDataV2 {
  Buffer logId;
  uint64_t timestamp;
  uint64_t treeSize;
  Buffer rootHash;
};

}  // namespace ct
}  // namespace mozilla

#endif  // BTTypes_h
