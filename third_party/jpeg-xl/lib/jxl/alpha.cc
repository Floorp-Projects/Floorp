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

#include "lib/jxl/alpha.h"

#include <algorithm>

namespace jxl {

void PerformAlphaBlending(const AlphaBlendingInputLayer& bg,
                          const AlphaBlendingInputLayer& fg,
                          const AlphaBlendingOutput& out, size_t num_pixels,
                          bool alpha_is_premultiplied) {
  if (alpha_is_premultiplied) {
    for (size_t x = 0; x < num_pixels; ++x) {
      out.r[x] = (fg.r[x] + bg.r[x] * (1.f - fg.a[x]));
      out.g[x] = (fg.g[x] + bg.g[x] * (1.f - fg.a[x]));
      out.b[x] = (fg.b[x] + bg.b[x] * (1.f - fg.a[x]));
      out.a[x] = (1.f - (1.f - fg.a[x]) * (1.f - bg.a[x]));
    }
  } else {
    for (size_t x = 0; x < num_pixels; ++x) {
      const float new_a = 1.f - (1.f - fg.a[x]) * (1.f - bg.a[x]);
      const float rnew_a = (new_a > 0 ? 1.f / new_a : 0.f);
      out.r[x] =
          (fg.r[x] * fg.a[x] + bg.r[x] * bg.a[x] * (1.f - fg.a[x])) * rnew_a;
      out.g[x] =
          (fg.g[x] * fg.a[x] + bg.g[x] * bg.a[x] * (1.f - fg.a[x])) * rnew_a;
      out.b[x] =
          (fg.b[x] * fg.a[x] + bg.b[x] * bg.a[x] * (1.f - fg.a[x])) * rnew_a;
      out.a[x] = new_a;
    }
  }
}
void PerformAlphaBlending(const float* bg, const float* bga, const float* fg,
                          const float* fga, float* out, size_t num_pixels,
                          bool alpha_is_premultiplied) {
  if (alpha_is_premultiplied) {
    for (size_t x = 0; x < num_pixels; ++x) {
      out[x] = (fg[x] + bg[x] * (1.f - fga[x]));
    }
  } else {
    for (size_t x = 0; x < num_pixels; ++x) {
      const float new_a = 1.f - (1.f - fga[x]) * (1.f - bga[x]);
      const float rnew_a = (new_a > 0 ? 1.f / new_a : 0.f);
      out[x] = (fg[x] * fga[x] + bg[x] * bga[x] * (1.f - fga[x])) * rnew_a;
    }
  }
}

void PerformAlphaWeightedAdd(const AlphaBlendingInputLayer& bg,
                             const AlphaBlendingInputLayer& fg,
                             const AlphaBlendingOutput& out,
                             size_t num_pixels) {
  for (size_t x = 0; x < num_pixels; ++x) {
    out.r[x] = bg.r[x] + fg.r[x] * fg.a[x];
    out.g[x] = bg.g[x] + fg.g[x] * fg.a[x];
    out.b[x] = bg.b[x] + fg.b[x] * fg.a[x];
    out.a[x] = bg.a[x];
  }
}
void PerformAlphaWeightedAdd(const float* bg, const float* fg, const float* fga,
                             float* out, size_t num_pixels) {
  for (size_t x = 0; x < num_pixels; ++x) {
    out[x] = bg[x] + fg[x] * fga[x];
  }
}

void PerformMulBlending(const float* bg, const float* fg, float* out,
                        size_t num_pixels) {
  for (size_t x = 0; x < num_pixels; ++x) {
    out[x] = bg[x] * fg[x];
  }
}

void PremultiplyAlpha(float* JXL_RESTRICT r, float* JXL_RESTRICT g,
                      float* JXL_RESTRICT b, const float* JXL_RESTRICT a,
                      size_t num_pixels) {
  for (size_t x = 0; x < num_pixels; ++x) {
    const float multiplier = std::max(kSmallAlpha, a[x]);
    r[x] *= multiplier;
    g[x] *= multiplier;
    b[x] *= multiplier;
  }
}

void UnpremultiplyAlpha(float* JXL_RESTRICT r, float* JXL_RESTRICT g,
                        float* JXL_RESTRICT b, const float* JXL_RESTRICT a,
                        size_t num_pixels) {
  for (size_t x = 0; x < num_pixels; ++x) {
    const float multiplier = 1.f / std::max(kSmallAlpha, a[x]);
    r[x] *= multiplier;
    g[x] *= multiplier;
    b[x] *= multiplier;
  }
}

}  // namespace jxl
