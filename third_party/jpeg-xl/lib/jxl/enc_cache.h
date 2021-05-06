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

#ifndef LIB_JXL_ENC_CACHE_H_
#define LIB_JXL_ENC_CACHE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/coeff_order.h"
#include "lib/jxl/coeff_order_fwd.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dct_util.h"
#include "lib/jxl/enc_ans.h"
#include "lib/jxl/enc_heuristics.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/passes_state.h"
#include "lib/jxl/progressive_split.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer.h"

namespace jxl {

// Contains encoder state.
struct PassesEncoderState {
  PassesSharedState shared;

  ImageF initial_quant_field;    // Invalid in Falcon mode.
  ImageF initial_quant_masking;  // Invalid in Falcon mode.

  // Per-pass DCT coefficients for the image. One row per group.
  std::vector<std::unique_ptr<ACImage>> coeffs;

  // Raw data for special (reference+DC) frames.
  std::vector<std::unique_ptr<BitWriter>> special_frames;

  // For splitting into passes.
  ProgressiveSplitter progressive_splitter;

  CompressParams cparams;

  struct PassData {
    std::vector<std::vector<Token>> ac_tokens;
    std::vector<uint8_t> context_map;
    EntropyEncodingData codes;
  };

  std::vector<PassData> passes;
  std::vector<uint8_t> histogram_idx;

  // Coefficient orders that are non-default.
  std::vector<uint32_t> used_orders;

  // Multiplier to be applied to the quant matrices of the x channel.
  float x_qm_multiplier = 1.0f;
  float b_qm_multiplier = 1.0f;

  // Heuristics to be used by the encoder.
  std::unique_ptr<EncoderHeuristics> heuristics =
      make_unique<DefaultEncoderHeuristics>();
};

// Initialize per-frame information.
class ModularFrameEncoder;
void InitializePassesEncoder(const Image3F& opsin, ThreadPool* pool,
                             PassesEncoderState* passes_enc_state,
                             ModularFrameEncoder* modular_frame_encoder,
                             AuxOut* aux_out);

// Working area for ComputeCoefficients (per-group!)
struct EncCache {
  // Allocates memory when first called, shrinks images to current group size.
  void InitOnce();

  // TokenizeCoefficients
  Image3I num_nzeroes;
};

}  // namespace jxl

#endif  // LIB_JXL_ENC_CACHE_H_
