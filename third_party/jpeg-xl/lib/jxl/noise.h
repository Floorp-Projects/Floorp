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

#ifndef LIB_JXL_NOISE_H_
#define LIB_JXL_NOISE_H_

// Noise parameters shared by encoder/decoder.

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "lib/jxl/base/compiler_specific.h"

namespace jxl {

const float kNoisePrecision = 1 << 10;

struct NoiseParams {
  // LUT index is an intensity of pixel / mean intensity of patch
  static constexpr size_t kNumNoisePoints = 8;
  float lut[kNumNoisePoints];

  void Clear() {
    for (float& i : lut) i = 0;
  }
  bool HasAny() const {
    for (float i : lut) {
      if (std::abs(i) > 1e-3f) return true;
    }
    return false;
  }
};

static inline std::pair<int, float> IndexAndFrac(float x) {
  constexpr size_t kScaleNumerator = NoiseParams::kNumNoisePoints - 2;
  // TODO: instead of 1, this should be a proper Y range.
  constexpr float kScale = kScaleNumerator / 1;
  float scaled_x = std::max(0.f, x * kScale);
  float floor_x;
  float frac_x = std::modf(scaled_x, &floor_x);
  if (JXL_UNLIKELY(scaled_x > kScaleNumerator)) {
    floor_x = kScaleNumerator;
  }
  return std::make_pair(static_cast<size_t>(static_cast<int>(floor_x)), frac_x);
}

struct NoiseLevel {
  float noise_level;
  float intensity;
};

}  // namespace jxl

#endif  // LIB_JXL_NOISE_H_
