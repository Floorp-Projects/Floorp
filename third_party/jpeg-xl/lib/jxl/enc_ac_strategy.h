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

#ifndef LIB_JXL_ENC_AC_STRATEGY_H_
#define LIB_JXL_ENC_AC_STRATEGY_H_

#include <stdint.h>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image.h"
#include "lib/jxl/quant_weights.h"

// `FindBestAcStrategy` uses heuristics to choose which AC strategy should be
// used in each block, as well as the initial quantization field.

namespace jxl {

// AC strategy selection: utility struct.

struct ACSConfig {
  const DequantMatrices* JXL_RESTRICT dequant;
  float info_loss_multiplier;
  float* JXL_RESTRICT quant_field_row;
  size_t quant_field_stride;
  float* JXL_RESTRICT masking_field_row;
  size_t masking_field_stride;
  const float* JXL_RESTRICT src_rows[3];
  size_t src_stride;
  // Cost for 1 (-1), 2 (-2) explicitly, cost for others computed with cost1 +
  // cost2 + sqrt(q) * cost_delta.
  float cost1;
  float cost2;
  float cost_delta;
  float base_entropy;
  float zeros_mul;
  const float& Pixel(size_t c, size_t x, size_t y) const {
    return src_rows[c][y * src_stride + x];
  }
  float Masking(size_t bx, size_t by) const {
    JXL_DASSERT(masking_field_row[by * masking_field_stride + bx] > 0);
    return masking_field_row[by * masking_field_stride + bx];
  }
  float Quant(size_t bx, size_t by) const {
    JXL_DASSERT(quant_field_row[by * quant_field_stride + bx] > 0);
    return quant_field_row[by * quant_field_stride + bx];
  }
  void SetQuant(size_t bx, size_t by, float value) const {
    JXL_DASSERT(value > 0);
    quant_field_row[by * quant_field_stride + bx] = value;
  }
};

struct AcStrategyHeuristics {
  void Init(const Image3F& src, PassesEncoderState* enc_state);
  void ProcessRect(const Rect& rect);
  void Finalize(AuxOut* aux_out);
  ACSConfig config;
  PassesEncoderState* enc_state;
  float entropy_adjust[2 * AcStrategy::kNumValidStrategies];
};

// Debug.
void DumpAcStrategy(const AcStrategyImage& ac_strategy, size_t xsize,
                    size_t ysize, const char* tag, AuxOut* aux_out);

}  // namespace jxl

#endif  // LIB_JXL_ENC_AC_STRATEGY_H_
