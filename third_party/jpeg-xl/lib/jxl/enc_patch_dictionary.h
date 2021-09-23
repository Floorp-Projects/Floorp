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

// Friend class of PatchDictionary.
class PatchDictionaryEncoder {
 public:
  // Only call if HasAny().
  static void Encode(const PatchDictionary& pdic, BitWriter* writer,
                     size_t layer, AuxOut* aux_out);

  static void SetPositions(PatchDictionary* pdic,
                           std::vector<PatchPosition> positions) {
    pdic->positions_ = std::move(positions);
    pdic->ComputePatchCache();
  }

  static void SubtractFrom(const PatchDictionary& pdic, Image3F* opsin);
};

void FindBestPatchDictionary(const Image3F& opsin,
                             PassesEncoderState* JXL_RESTRICT state,
                             ThreadPool* pool, AuxOut* aux_out,
                             bool is_xyb = true);

}  // namespace jxl

#endif  // LIB_JXL_ENC_PATCH_DICTIONARY_H_
