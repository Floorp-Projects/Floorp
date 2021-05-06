// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
