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

#ifndef LIB_JXL_ENC_TRANSFORMS_H_
#define LIB_JXL_ENC_TRANSFORMS_H_

// Facade for (non-inlined) integral transforms.

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/compiler_specific.h"

namespace jxl {

void TransformFromPixels(const AcStrategy::Type strategy,
                         const float* JXL_RESTRICT pixels, size_t pixels_stride,
                         float* JXL_RESTRICT coefficients,
                         float* JXL_RESTRICT scratch_space);

// Equivalent of the above for DC image.
void DCFromLowestFrequencies(AcStrategy::Type strategy, const float* block,
                             float* dc, size_t dc_stride);

void AFVDCT4x4(const float* JXL_RESTRICT pixels, float* JXL_RESTRICT coeffs);

}  // namespace jxl

#endif  // LIB_JXL_ENC_TRANSFORMS_H_
