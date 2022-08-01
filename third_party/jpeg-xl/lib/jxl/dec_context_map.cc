// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_context_map.h"

#include <algorithm>
#include <vector>

#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/entropy_coder.h"

namespace jxl {

namespace {

void MoveToFront(uint8_t* v, uint8_t index) {
  uint8_t value = v[index];
  uint8_t i = index;
  for (; i; --i) v[i] = v[i - 1];
  v[0] = value;
}

void InverseMoveToFrontTransform(uint8_t* v, int v_len) {
  uint8_t mtf[256];
  int i;
  for (i = 0; i < 256; ++i) {
    mtf[i] = static_cast<uint8_t>(i);
  }
  for (i = 0; i < v_len; ++i) {
    uint8_t index = v[i];
    v[i] = mtf[index];
    if (index) MoveToFront(mtf, index);
  }
}

Status VerifyContextMap(const std::vector<uint8_t>& context_map,
                        const size_t num_htrees) {
  std::vector<bool> have_htree(num_htrees);
  size_t num_found = 0;
  for (const uint8_t htree : context_map) {
    if (htree >= num_htrees) {
      return JXL_FAILURE("Invalid histogram index in context map.");
    }
    if (!have_htree[htree]) {
      have_htree[htree] = true;
      ++num_found;
    }
  }
  if (num_found != num_htrees) {
    return JXL_FAILURE("Incomplete context map.");
  }
  return true;
}

}  // namespace

Status DecodeContextMap(std::vector<uint8_t>* context_map, size_t* num_htrees,
                        BitReader* input) {
  bool is_simple = input->ReadFixedBits<1>();
  if (is_simple) {
    int bits_per_entry = input->ReadFixedBits<2>();
    if (bits_per_entry != 0) {
      for (size_t i = 0; i < context_map->size(); i++) {
        (*context_map)[i] = input->ReadBits(bits_per_entry);
      }
    } else {
      std::fill(context_map->begin(), context_map->end(), 0);
    }
  } else {
    bool use_mtf = input->ReadFixedBits<1>();
    ANSCode code;
    std::vector<uint8_t> dummy_ctx_map;
    // Usage of LZ77 is disallowed if decoding only two symbols. This doesn't
    // make sense in non-malicious bitstreams, and could cause a stack overflow
    // in malicious bitstreams by making every context map require its own
    // context map.
    JXL_RETURN_IF_ERROR(
        DecodeHistograms(input, 1, &code, &dummy_ctx_map,
                         /*disallow_lz77=*/context_map->size() <= 2));
    ANSSymbolReader reader(&code, input);
    size_t i = 0;
    while (i < context_map->size()) {
      uint32_t sym = reader.ReadHybridUint(0, input, dummy_ctx_map);
      if (sym >= kMaxClusters) {
        return JXL_FAILURE("Invalid cluster ID");
      }
      (*context_map)[i] = sym;
      i++;
    }
    if (!reader.CheckANSFinalState()) {
      return JXL_FAILURE("Invalid context map");
    }
    if (use_mtf) {
      InverseMoveToFrontTransform(context_map->data(), context_map->size());
    }
  }
  *num_htrees = *std::max_element(context_map->begin(), context_map->end()) + 1;
  return VerifyContextMap(*context_map, *num_htrees);
}

}  // namespace jxl
