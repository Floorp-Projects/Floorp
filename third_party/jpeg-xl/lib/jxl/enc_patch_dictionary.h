// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_PATCH_DICTIONARY_H_
#define LIB_JXL_ENC_PATCH_DICTIONARY_H_

// Chooses reference patches, and avoids encoding them once per occurrence.

#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include <tuple>
#include <vector>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/dec_patch_dictionary.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image.h"
#include "lib/jxl/opsin_params.h"

namespace jxl {

constexpr size_t kMaxPatchSize = 32;

struct QuantizedPatch {
  size_t xsize;
  size_t ysize;
  QuantizedPatch() {
    for (size_t i = 0; i < 3; i++) {
      pixels[i].resize(kMaxPatchSize * kMaxPatchSize);
      fpixels[i].resize(kMaxPatchSize * kMaxPatchSize);
    }
  }
  std::vector<int8_t> pixels[3] = {};
  // Not compared. Used only to retrieve original pixels to construct the
  // reference image.
  std::vector<float> fpixels[3] = {};
  bool operator==(const QuantizedPatch& other) const {
    if (xsize != other.xsize) return false;
    if (ysize != other.ysize) return false;
    for (size_t c = 0; c < 3; c++) {
      if (memcmp(pixels[c].data(), other.pixels[c].data(),
                 sizeof(int8_t) * xsize * ysize) != 0)
        return false;
    }
    return true;
  }

  bool operator<(const QuantizedPatch& other) const {
    if (xsize != other.xsize) return xsize < other.xsize;
    if (ysize != other.ysize) return ysize < other.ysize;
    for (size_t c = 0; c < 3; c++) {
      int cmp = memcmp(pixels[c].data(), other.pixels[c].data(),
                       sizeof(int8_t) * xsize * ysize);
      if (cmp > 0) return false;
      if (cmp < 0) return true;
    }
    return false;
  }
};

// Pair (patch, vector of occurrences).
using PatchInfo =
    std::pair<QuantizedPatch, std::vector<std::pair<uint32_t, uint32_t>>>;

// Friend class of PatchDictionary.
class PatchDictionaryEncoder {
 public:
  // Only call if HasAny().
  static void Encode(const PatchDictionary& pdic, BitWriter* writer,
                     size_t layer, AuxOut* aux_out);

  static void SetPositions(PatchDictionary* pdic,
                           std::vector<PatchPosition> positions) {
    if (pdic->positions_.empty()) {
      pdic->positions_ = std::move(positions);
    } else {
      pdic->positions_.insert(pdic->positions_.end(), positions.begin(),
                              positions.end());
    }
    pdic->ComputePatchCache();
  }

  static void SubtractFrom(const PatchDictionary& pdic, Image3F* opsin);
};

void FindBestPatchDictionary(const Image3F& opsin,
                             PassesEncoderState* JXL_RESTRICT state,
                             ThreadPool* pool, AuxOut* aux_out,
                             bool is_xyb = true);

void RoundtripPatchFrame(Image3F* reference_frame,
                         PassesEncoderState* JXL_RESTRICT state, int idx,
                         CompressParams& cparams, ThreadPool* pool,
                         bool subtract);

}  // namespace jxl

#endif  // LIB_JXL_ENC_PATCH_DICTIONARY_H_
