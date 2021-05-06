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

#include "lib/jxl/enc_butteraugli_pnorm.h"

#include <math.h>
#include <stdlib.h>

#include <atomic>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_butteraugli_pnorm.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Rebind;

double ComputeDistanceP(const ImageF& distmap, const ButteraugliParams& params,
                        double p) {
  PROFILER_FUNC;
  // In approximate-border mode, skip pixels on the border likely to be affected
  // by FastGauss' zero-valued-boundary behavior. The border is less than half
  // the largest-diameter kernel (37x37 pixels), and 0 if the image is tiny.
  // NOTE: chosen such that it is vector-aligned.
  size_t border = (params.approximate_border) ? 8 : 0;
  if (distmap.xsize() <= 2 * border || distmap.ysize() <= 2 * border) {
    border = 0;
  }

  const double onePerPixels = 1.0 / (distmap.ysize() * distmap.xsize());
  if (std::abs(p - 3.0) < 1E-6) {
    double sum1[3] = {0.0};

// Prefer double if possible, but otherwise use float rather than scalar.
#if HWY_CAP_FLOAT64
    using T = double;
    const Rebind<float, HWY_FULL(double)> df;
#else
    using T = float;
#endif
    const HWY_FULL(T) d;
    constexpr size_t N = MaxLanes(HWY_FULL(T)());
    // Manually aligned storage to avoid asan crash on clang-7 due to
    // unaligned spill.
    HWY_ALIGN T sum_totals0[N] = {0};
    HWY_ALIGN T sum_totals1[N] = {0};
    HWY_ALIGN T sum_totals2[N] = {0};

    for (size_t y = border; y < distmap.ysize() - border; ++y) {
      const float* JXL_RESTRICT row = distmap.ConstRow(y);

      auto sums0 = Zero(d);
      auto sums1 = Zero(d);
      auto sums2 = Zero(d);

      size_t x = border;
      for (; x + Lanes(d) <= distmap.xsize() - border; x += Lanes(d)) {
#if HWY_CAP_FLOAT64
        const auto d1 = PromoteTo(d, Load(df, row + x));
#else
        const auto d1 = Load(d, row + x);
#endif
        const auto d2 = d1 * d1 * d1;
        sums0 += d2;
        const auto d3 = d2 * d2;
        sums1 += d3;
        const auto d4 = d3 * d3;
        sums2 += d4;
      }

      Store(sums0 + Load(d, sum_totals0), d, sum_totals0);
      Store(sums1 + Load(d, sum_totals1), d, sum_totals1);
      Store(sums2 + Load(d, sum_totals2), d, sum_totals2);

      for (; x < distmap.xsize() - border; ++x) {
        const double d1 = row[x];
        double d2 = d1 * d1 * d1;
        sum1[0] += d2;
        d2 *= d2;
        sum1[1] += d2;
        d2 *= d2;
        sum1[2] += d2;
      }
    }
    double v = 0;
    v += pow(
        onePerPixels * (sum1[0] + GetLane(SumOfLanes(Load(d, sum_totals0)))),
        1.0 / (p * 1.0));
    v += pow(
        onePerPixels * (sum1[1] + GetLane(SumOfLanes(Load(d, sum_totals1)))),
        1.0 / (p * 2.0));
    v += pow(
        onePerPixels * (sum1[2] + GetLane(SumOfLanes(Load(d, sum_totals2)))),
        1.0 / (p * 4.0));
    v /= 3.0;
    return v;
  } else {
    static std::atomic<int> once{0};
    if (once.fetch_add(1, std::memory_order_relaxed) == 0) {
      JXL_WARNING("WARNING: using slow ComputeDistanceP");
    }
    double sum1[3] = {0.0};
    for (size_t y = border; y < distmap.ysize() - border; ++y) {
      const float* JXL_RESTRICT row = distmap.ConstRow(y);
      for (size_t x = border; x < distmap.xsize() - border; ++x) {
        double d2 = std::pow(row[x], p);
        sum1[0] += d2;
        d2 *= d2;
        sum1[1] += d2;
        d2 *= d2;
        sum1[2] += d2;
      }
    }
    double v = 0;
    for (int i = 0; i < 3; ++i) {
      v += pow(onePerPixels * (sum1[i]), 1.0 / (p * (1 << i)));
    }
    v /= 3.0;
    return v;
  }
}

// TODO(lode): take alpha into account when needed
double ComputeDistance2(const ImageBundle& ib1, const ImageBundle& ib2) {
  PROFILER_FUNC;
  // Convert to sRGB - closer to perception than linear.
  const Image3F* srgb1 = &ib1.color();
  Image3F copy1;
  if (!ib1.IsSRGB()) {
    JXL_CHECK(ib1.CopyTo(Rect(ib1), ColorEncoding::SRGB(ib1.IsGray()), &copy1));
    srgb1 = &copy1;
  }
  const Image3F* srgb2 = &ib2.color();
  Image3F copy2;
  if (!ib2.IsSRGB()) {
    JXL_CHECK(ib2.CopyTo(Rect(ib2), ColorEncoding::SRGB(ib2.IsGray()), &copy2));
    srgb2 = &copy2;
  }

  JXL_CHECK(SameSize(*srgb1, *srgb2));

  // TODO(veluca): SIMD.
  float yuvmatrix[3][3] = {{0.299, 0.587, 0.114},
                           {-0.14713, -0.28886, 0.436},
                           {0.615, -0.51499, -0.10001}};
  double sum_of_squares[3] = {};
  for (size_t y = 0; y < srgb1->ysize(); ++y) {
    const float* JXL_RESTRICT row1[3];
    const float* JXL_RESTRICT row2[3];
    for (size_t j = 0; j < 3; j++) {
      row1[j] = srgb1->ConstPlaneRow(j, y);
      row2[j] = srgb2->ConstPlaneRow(j, y);
    }
    for (size_t x = 0; x < srgb1->xsize(); ++x) {
      float cdiff[3] = {};
      // YUV conversion is linear, so we can run it on the difference.
      for (size_t j = 0; j < 3; j++) {
        cdiff[j] = row1[j][x] - row2[j][x];
      }
      float yuvdiff[3] = {};
      for (size_t j = 0; j < 3; j++) {
        for (size_t k = 0; k < 3; k++) {
          yuvdiff[j] += yuvmatrix[j][k] * cdiff[k];
        }
      }
      for (size_t j = 0; j < 3; j++) {
        sum_of_squares[j] += yuvdiff[j] * yuvdiff[j];
      }
    }
  }
  // Weighted PSNR as in JPEG-XL: chroma counts 1/8.
  const float weights[3] = {6.0f / 8, 1.0f / 8, 1.0f / 8};
  // Avoid squaring the weight - 1/64 is too extreme.
  double norm = 0;
  for (size_t i = 0; i < 3; i++) {
    norm += std::sqrt(sum_of_squares[i]) * weights[i];
  }
  // This function returns distance *squared*.
  return norm * norm;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(ComputeDistanceP);
double ComputeDistanceP(const ImageF& distmap, const ButteraugliParams& params,
                        double p) {
  return HWY_DYNAMIC_DISPATCH(ComputeDistanceP)(distmap, params, p);
}

HWY_EXPORT(ComputeDistance2);
double ComputeDistance2(const ImageBundle& ib1, const ImageBundle& ib2) {
  return HWY_DYNAMIC_DISPATCH(ComputeDistance2)(ib1, ib2);
}

}  // namespace jxl
#endif
