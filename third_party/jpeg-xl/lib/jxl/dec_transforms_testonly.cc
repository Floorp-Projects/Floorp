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

#include "lib/jxl/dec_transforms_testonly.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_transforms_testonly.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/dct_scales.h"
#include "lib/jxl/dec_transforms-inl.h"

namespace jxl {

#if HWY_ONCE
HWY_EXPORT(TransformToPixels);
void TransformToPixels(AcStrategy::Type strategy,
                       float* JXL_RESTRICT coefficients,
                       float* JXL_RESTRICT pixels, size_t pixels_stride,
                       float* scratch_space) {
  return HWY_DYNAMIC_DISPATCH(TransformToPixels)(strategy, coefficients, pixels,
                                                 pixels_stride, scratch_space);
}

HWY_EXPORT(LowestFrequenciesFromDC);
void LowestFrequenciesFromDC(const jxl::AcStrategy::Type strategy,
                             const float* dc, size_t dc_stride, float* llf) {
  return HWY_DYNAMIC_DISPATCH(LowestFrequenciesFromDC)(strategy, dc, dc_stride,
                                                       llf);
}

HWY_EXPORT(AFVIDCT4x4);
void AFVIDCT4x4(const float* JXL_RESTRICT coeffs, float* JXL_RESTRICT pixels) {
  return HWY_DYNAMIC_DISPATCH(AFVIDCT4x4)(coeffs, pixels);
}
#endif  // HWY_ONCE

}  // namespace jxl
