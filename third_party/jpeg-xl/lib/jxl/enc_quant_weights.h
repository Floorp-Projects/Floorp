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

#ifndef LIB_JXL_ENC_QUANT_WEIGHTS_H_
#define LIB_JXL_ENC_QUANT_WEIGHTS_H_

#include "lib/jxl/quant_weights.h"

namespace jxl {

Status DequantMatricesEncode(
    const DequantMatrices* matrices, BitWriter* writer, size_t layer,
    AuxOut* aux_out, ModularFrameEncoder* modular_frame_encoder = nullptr);
Status DequantMatricesEncodeDC(const DequantMatrices* matrices,
                               BitWriter* writer, size_t layer,
                               AuxOut* aux_out);
// For consistency with QuantEncoding, higher values correspond to more
// precision.
void DequantMatricesSetCustomDC(DequantMatrices* matrices, const float* dc);

void DequantMatricesSetCustom(DequantMatrices* matrices,
                              const std::vector<QuantEncoding>& encodings,
                              ModularFrameEncoder* encoder);

}  // namespace jxl

#endif  // LIB_JXL_ENC_QUANT_WEIGHTS_H_
