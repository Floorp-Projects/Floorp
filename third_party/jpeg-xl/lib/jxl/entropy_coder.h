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

#ifndef LIB_JXL_ENTROPY_CODER_H_
#define LIB_JXL_ENTROPY_CODER_H_

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/ac_context.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/field_encodings.h"

// Entropy coding and context modeling of DC and AC coefficients, as well as AC
// strategy and quantization field.

namespace jxl {

static JXL_INLINE int32_t PredictFromTopAndLeft(
    const int32_t* const JXL_RESTRICT row_top,
    const int32_t* const JXL_RESTRICT row, size_t x, int32_t default_val) {
  if (x == 0) {
    return row_top == nullptr ? default_val : row_top[x];
  }
  if (row_top == nullptr) {
    return row[x - 1];
  }
  return (row_top[x] + row[x - 1] + 1) / 2;
}

static constexpr U32Enc kDCThresholdDist(Bits(4), BitsOffset(8, 16),
                                         BitsOffset(16, 272),
                                         BitsOffset(32, 65808));

static constexpr U32Enc kQFThresholdDist(Bits(2), BitsOffset(3, 4),
                                         BitsOffset(5, 12), BitsOffset(8, 44));

Status DecodeBlockCtxMap(BitReader* br, BlockCtxMap* block_ctx_map);

}  // namespace jxl

#endif  // LIB_JXL_ENTROPY_CODER_H_
