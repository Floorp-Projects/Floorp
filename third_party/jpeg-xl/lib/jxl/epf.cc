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

// Edge-preserving smoothing: weighted average based on L1 patch similarity.

#include "lib/jxl/epf.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <numeric>  // std::accumulate
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/epf.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/convolve.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/filters.h"
#include "lib/jxl/filters_internal.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Vec;

// The EPF logic treats 8x8 blocks as one unit, each with their own sigma.
// It should be possible to do two blocks at a time in AVX3 vectors, at some
// increase in complexity (broadcasting sigma0/1 to lanes 0..7 and 8..15).
using DF = HWY_CAPPED(float, GroupBorderAssigner::kPaddingXRound);
using DU = HWY_CAPPED(uint32_t, GroupBorderAssigner::kPaddingXRound);

// kInvSigmaNum / 0.3
constexpr float kMinSigma = -3.90524291751269967465540850526868f;

DF df;

JXL_INLINE Vec<DF> Weight(Vec<DF> sad, Vec<DF> inv_sigma, Vec<DF> thres) {
  auto v = MulAdd(sad, inv_sigma, Set(DF(), 1.0f));
  auto v2 = v * v;
  return IfThenZeroElse(v <= thres, v2);
}

template <bool aligned>
JXL_INLINE void AddPixelStep1(int row, const FilterRows& rows, size_t x,
                              Vec<DF> sad, Vec<DF> inv_sigma,
                              const LoopFilter& lf, Vec<DF>* JXL_RESTRICT X,
                              Vec<DF>* JXL_RESTRICT Y, Vec<DF>* JXL_RESTRICT B,
                              Vec<DF>* JXL_RESTRICT w) {
  auto cx = aligned ? Load(DF(), rows.GetInputRow(row, 0) + x)
                    : LoadU(DF(), rows.GetInputRow(row, 0) + x);
  auto cy = aligned ? Load(DF(), rows.GetInputRow(row, 1) + x)
                    : LoadU(DF(), rows.GetInputRow(row, 1) + x);
  auto cb = aligned ? Load(DF(), rows.GetInputRow(row, 2) + x)
                    : LoadU(DF(), rows.GetInputRow(row, 2) + x);

  auto weight = Weight(sad, inv_sigma, Set(df, lf.epf_pass1_zeroflush));
  *w += weight;
  *X = MulAdd(weight, cx, *X);
  *Y = MulAdd(weight, cy, *Y);
  *B = MulAdd(weight, cb, *B);
}

template <bool aligned>
JXL_INLINE void AddPixelStep2(int row, const FilterRows& rows, size_t x,
                              Vec<DF> rx, Vec<DF> ry, Vec<DF> rb,
                              Vec<DF> inv_sigma, const LoopFilter& lf,
                              Vec<DF>* JXL_RESTRICT X, Vec<DF>* JXL_RESTRICT Y,
                              Vec<DF>* JXL_RESTRICT B,
                              Vec<DF>* JXL_RESTRICT w) {
  auto cx = aligned ? Load(DF(), rows.GetInputRow(row, 0) + x)
                    : LoadU(DF(), rows.GetInputRow(row, 0) + x);
  auto cy = aligned ? Load(DF(), rows.GetInputRow(row, 1) + x)
                    : LoadU(DF(), rows.GetInputRow(row, 1) + x);
  auto cb = aligned ? Load(DF(), rows.GetInputRow(row, 2) + x)
                    : LoadU(DF(), rows.GetInputRow(row, 2) + x);

  auto sad = AbsDiff(cx, rx) * Set(df, lf.epf_channel_scale[0]);
  sad = MulAdd(AbsDiff(cy, ry), Set(df, lf.epf_channel_scale[1]), sad);
  sad = MulAdd(AbsDiff(cb, rb), Set(df, lf.epf_channel_scale[2]), sad);

  auto weight = Weight(sad, inv_sigma, Set(df, lf.epf_pass2_zeroflush));

  *w += weight;
  *X = MulAdd(weight, cx, *X);
  *Y = MulAdd(weight, cy, *Y);
  *B = MulAdd(weight, cb, *B);
}

template <class D, class V>
void GaborishVector(const D df, const float* JXL_RESTRICT row_t,
                    const float* JXL_RESTRICT row_m,
                    const float* JXL_RESTRICT row_b, const V w0, const V w1,
                    const V w2, float* JXL_RESTRICT row_out) {
// Filter x0 is only aligned to blocks (8 floats = 32 bytes). For larger
// vectors, treat loads as unaligned (we manually align the Store).
#undef LoadMaybeU
#if HWY_CAP_GE512
#define LoadMaybeU LoadU
#else
#define LoadMaybeU Load
#endif

  const auto t = LoadMaybeU(df, row_t);
  const auto tl = LoadU(df, row_t - 1);
  const auto tr = LoadU(df, row_t + 1);
  const auto m = LoadMaybeU(df, row_m);
  const auto l = LoadU(df, row_m - 1);
  const auto r = LoadU(df, row_m + 1);
  const auto b = LoadMaybeU(df, row_b);
  const auto bl = LoadU(df, row_b - 1);
  const auto br = LoadU(df, row_b + 1);
  const auto sum0 = m;
  const auto sum1 = (l + r) + (t + b);
  const auto sum2 = (tl + tr) + (bl + br);
  auto pixels = MulAdd(sum2, w2, MulAdd(sum1, w1, sum0 * w0));
  Store(pixels, df, row_out);
}

void GaborishRow(const FilterRows& rows, const LoopFilter& /* lf */,
                 const FilterWeights& filter_weights, size_t x0, size_t x1,
                 size_t /*image_x_mod_8*/, size_t /* image_y_mod_8 */) {
  JXL_DASSERT(x0 % Lanes(df) == 0);

  const float* JXL_RESTRICT gab_weights = filter_weights.gab_weights;
  for (size_t c = 0; c < 3; c++) {
    const float* JXL_RESTRICT row_t = rows.GetInputRow(-1, c);
    const float* JXL_RESTRICT row_m = rows.GetInputRow(0, c);
    const float* JXL_RESTRICT row_b = rows.GetInputRow(1, c);
    float* JXL_RESTRICT row_out = rows.GetOutputRow(c);

    size_t ix = x0;

#if HWY_CAP_GE512
    const HWY_FULL(float) dfull;  // Gaborish is not block-dependent.

    // For AVX3, x0 might only be aligned to 8, not 16; if so, do a capped
    // vector first to ensure full (Store-only!) alignment, then full vectors.
    const uintptr_t addr = reinterpret_cast<uintptr_t>(row_out + ix);
    if ((addr % 64) != 0 && ix < x1) {
      const auto w0 = Set(df, gab_weights[3 * c + 0]);
      const auto w1 = Set(df, gab_weights[3 * c + 1]);
      const auto w2 = Set(df, gab_weights[3 * c + 2]);
      GaborishVector(df, row_t + ix, row_m + ix, row_b + ix, w0, w1, w2,
                     row_out + ix);
      ix += Lanes(df);
    }

    const auto wfull0 = Set(dfull, gab_weights[3 * c + 0]);
    const auto wfull1 = Set(dfull, gab_weights[3 * c + 1]);
    const auto wfull2 = Set(dfull, gab_weights[3 * c + 2]);
    for (; ix + Lanes(dfull) <= x1; ix += Lanes(dfull)) {
      GaborishVector(dfull, row_t + ix, row_m + ix, row_b + ix, wfull0, wfull1,
                     wfull2, row_out + ix);
    }
#endif

    // Non-AVX3 loop, or last capped vector for AVX3, if necessary
    const auto w0 = Set(df, gab_weights[3 * c + 0]);
    const auto w1 = Set(df, gab_weights[3 * c + 1]);
    const auto w2 = Set(df, gab_weights[3 * c + 2]);
    for (; ix < x1; ix += Lanes(df)) {
      GaborishVector(df, row_t + ix, row_m + ix, row_b + ix, w0, w1, w2,
                     row_out + ix);
    }
  }
}

// Step 0: 5x5 plus-shaped kernel with 5 SADs per pixel (3x3
// plus-shaped). So this makes this filter a 7x7 filter.
void Epf0Row(const FilterRows& rows, const LoopFilter& lf,
             const FilterWeights& filter_weights, size_t x0, size_t x1,
             size_t image_x_mod_8, size_t image_y_mod_8) {
  JXL_DASSERT(x0 % Lanes(df) == 0);
  const float* JXL_RESTRICT row_sigma = rows.GetSigmaRow();

  float sm = lf.epf_pass0_sigma_scale;
  float bsm = sm * lf.epf_border_sad_mul;

  HWY_ALIGN float sad_mul[kBlockDim] = {bsm, sm, sm, sm, sm, sm, sm, bsm};

  if (image_y_mod_8 == 0 || image_y_mod_8 == kBlockDim - 1) {
    for (size_t i = 0; i < kBlockDim; i += Lanes(df)) {
      Store(Set(df, bsm), df, sad_mul + i);
    }
  }

  for (size_t x = x0; x < x1; x += Lanes(df)) {
    size_t bx = (x + image_x_mod_8) / kBlockDim;
    size_t ix = (x + image_x_mod_8) % kBlockDim;
    if (row_sigma[bx] < kMinSigma) {
      for (size_t c = 0; c < 3; c++) {
        auto px = Load(df, rows.GetInputRow(0, c) + x);
        Store(px, df, rows.GetOutputRow(c) + x);
      }
      continue;
    }

    const auto sm = Load(df, sad_mul + ix);
    const auto inv_sigma = Set(DF(), row_sigma[bx]) * sm;

    decltype(Zero(df)) sads[12];
    for (size_t i = 0; i < 12; i++) sads[i] = Zero(df);
    constexpr std::array<int, 2> sads_off[12] = {
        {-2, 0}, {-1, -1}, {-1, 0}, {-1, 1}, {0, -2}, {0, -1},
        {0, 1},  {0, 2},   {1, -1}, {1, 0},  {1, 1},  {2, 0},
    };

    // compute sads
    // TODO(veluca): consider unrolling and optimizing this.
    for (size_t c = 0; c < 3; c++) {
      auto scale = Set(df, lf.epf_channel_scale[c]);
      for (size_t i = 0; i < 12; i++) {
        auto sad = Zero(df);
        constexpr std::array<int, 2> plus_off[] = {
            {0, 0}, {-1, 0}, {0, -1}, {1, 0}, {0, 1}};
        for (size_t j = 0; j < 5; j++) {
          const auto r11 = LoadU(
              df, rows.GetInputRow(plus_off[j][0], c) + x + plus_off[j][1]);
          const auto c11 =
              LoadU(df, rows.GetInputRow(sads_off[i][0] + plus_off[j][0], c) +
                            x + sads_off[i][1] + plus_off[j][1]);
          sad += AbsDiff(r11, c11);
        }
        sads[i] = MulAdd(sad, scale, sads[i]);
      }
    }
    const auto x_cc = LoadU(df, rows.GetInputRow(0, 0) + x);
    const auto y_cc = LoadU(df, rows.GetInputRow(0, 1) + x);
    const auto b_cc = LoadU(df, rows.GetInputRow(0, 2) + x);

    auto w = Set(df, 1);
    auto X = x_cc;
    auto Y = y_cc;
    auto B = b_cc;

    for (size_t i = 0; i < 12; i++) {
      AddPixelStep1</*aligned=*/false>(/*row=*/sads_off[i][0], rows,
                                       x + sads_off[i][1], sads[i], inv_sigma,
                                       lf, &X, &Y, &B, &w);
    }

#if JXL_HIGH_PRECISION
    auto inv_w = Set(df, 1.0f) / w;
#else
    auto inv_w = ApproximateReciprocal(w);
#endif
    Store(X * inv_w, df, rows.GetOutputRow(0) + x);
    Store(Y * inv_w, df, rows.GetOutputRow(1) + x);
    Store(B * inv_w, df, rows.GetOutputRow(2) + x);
  }
}

// Step 1: 3x3 plus-shaped kernel with 5 SADs per pixel (also 3x3
// plus-shaped). So this makes this filter a 5x5 filter.
void Epf1Row(const FilterRows& rows, const LoopFilter& lf,
             const FilterWeights& filter_weights, size_t x0, size_t x1,
             size_t image_x_mod_8, size_t image_y_mod_8) {
  JXL_DASSERT(x0 % Lanes(df) == 0);
  const float* JXL_RESTRICT row_sigma = rows.GetSigmaRow();

  float sm = 1.0f;
  float bsm = sm * lf.epf_border_sad_mul;

  HWY_ALIGN float sad_mul[kBlockDim] = {bsm, sm, sm, sm, sm, sm, sm, bsm};

  if (image_y_mod_8 == 0 || image_y_mod_8 == kBlockDim - 1) {
    for (size_t i = 0; i < kBlockDim; i += Lanes(df)) {
      Store(Set(df, bsm), df, sad_mul + i);
    }
  }

  for (size_t x = x0; x < x1; x += Lanes(df)) {
    size_t bx = (x + image_x_mod_8) / kBlockDim;
    size_t ix = (x + image_x_mod_8) % kBlockDim;
    if (row_sigma[bx] < kMinSigma) {
      for (size_t c = 0; c < 3; c++) {
        auto px = Load(df, rows.GetInputRow(0, c) + x);
        Store(px, df, rows.GetOutputRow(c) + x);
      }
      continue;
    }

    const auto sm = Load(df, sad_mul + ix);
    const auto inv_sigma = Set(DF(), row_sigma[bx]) * sm;
    auto sad0 = Zero(df);
    auto sad1 = Zero(df);
    auto sad2 = Zero(df);
    auto sad3 = Zero(df);

    // compute sads
    for (size_t c = 0; c < 3; c++) {
      // center px = 22, px above = 21
      auto t = Undefined(df);

      const auto p20 = Load(df, rows.GetInputRow(-2, c) + x);
      const auto p21 = Load(df, rows.GetInputRow(-1, c) + x);
      auto sad0c = AbsDiff(p20, p21);  // SAD 2, 1

      const auto p11 = LoadU(df, rows.GetInputRow(-1, c) + x - 1);
      auto sad1c = AbsDiff(p11, p21);  // SAD 1, 2

      const auto p31 = LoadU(df, rows.GetInputRow(-1, c) + x + 1);
      auto sad2c = AbsDiff(p31, p21);  // SAD 3, 2

      const auto p02 = LoadU(df, rows.GetInputRow(0, c) + x - 2);
      const auto p12 = LoadU(df, rows.GetInputRow(0, c) + x - 1);
      sad1c += AbsDiff(p02, p12);  // SAD 1, 2
      sad0c += AbsDiff(p11, p12);  // SAD 2, 1

      const auto p22 = LoadU(df, rows.GetInputRow(0, c) + x);
      t = AbsDiff(p12, p22);
      sad1c += t;  // SAD 1, 2
      sad2c += t;  // SAD 3, 2
      t = AbsDiff(p22, p21);
      auto sad3c = t;  // SAD 2, 3
      sad0c += t;      // SAD 2, 1

      const auto p32 = LoadU(df, rows.GetInputRow(0, c) + x + 1);
      sad0c += AbsDiff(p31, p32);  // SAD 2, 1
      t = AbsDiff(p22, p32);
      sad1c += t;  // SAD 1, 2
      sad2c += t;  // SAD 3, 2

      const auto p42 = LoadU(df, rows.GetInputRow(0, c) + x + 2);
      sad2c += AbsDiff(p42, p32);  // SAD 3, 2

      const auto p13 = LoadU(df, rows.GetInputRow(1, c) + x - 1);
      sad3c += AbsDiff(p13, p12);  // SAD 2, 3

      const auto p23 = Load(df, rows.GetInputRow(1, c) + x);
      t = AbsDiff(p22, p23);
      sad0c += t;                  // SAD 2, 1
      sad3c += t;                  // SAD 2, 3
      sad1c += AbsDiff(p13, p23);  // SAD 1, 2

      const auto p33 = LoadU(df, rows.GetInputRow(1, c) + x + 1);
      sad2c += AbsDiff(p33, p23);  // SAD 3, 2
      sad3c += AbsDiff(p33, p32);  // SAD 2, 3

      const auto p24 = Load(df, rows.GetInputRow(2, c) + x);
      sad3c += AbsDiff(p24, p23);  // SAD 2, 3

      auto scale = Set(df, lf.epf_channel_scale[c]);
      sad0 = MulAdd(sad0c, scale, sad0);
      sad1 = MulAdd(sad1c, scale, sad1);
      sad2 = MulAdd(sad2c, scale, sad2);
      sad3 = MulAdd(sad3c, scale, sad3);
    }
    const auto x_cc = Load(df, rows.GetInputRow(0, 0) + x);
    const auto y_cc = Load(df, rows.GetInputRow(0, 1) + x);
    const auto b_cc = Load(df, rows.GetInputRow(0, 2) + x);

    auto w = Set(df, 1);
    auto X = x_cc;
    auto Y = y_cc;
    auto B = b_cc;

    // Top row
    AddPixelStep1</*aligned=*/true>(/*row=*/-1, rows, x, sad0, inv_sigma, lf,
                                    &X, &Y, &B, &w);
    // Center
    AddPixelStep1</*aligned=*/false>(/*row=*/0, rows, x - 1, sad1, inv_sigma,
                                     lf, &X, &Y, &B, &w);
    AddPixelStep1</*aligned=*/false>(/*row=*/0, rows, x + 1, sad2, inv_sigma,
                                     lf, &X, &Y, &B, &w);
    // Bottom
    AddPixelStep1</*aligned=*/true>(/*row=*/1, rows, x, sad3, inv_sigma, lf, &X,
                                    &Y, &B, &w);
#if JXL_HIGH_PRECISION
    auto inv_w = Set(df, 1.0f) / w;
#else
    auto inv_w = ApproximateReciprocal(w);
#endif
    Store(X * inv_w, df, rows.GetOutputRow(0) + x);
    Store(Y * inv_w, df, rows.GetOutputRow(1) + x);
    Store(B * inv_w, df, rows.GetOutputRow(2) + x);
  }
}

// Step 2: 3x3 plus-shaped kernel with a single reference pixel, ran on
// the output of the previous step.
void Epf2Row(const FilterRows& rows, const LoopFilter& lf,
             const FilterWeights& filter_weights, size_t x0, size_t x1,
             size_t image_x_mod_8, size_t image_y_mod_8) {
  JXL_DASSERT(x0 % Lanes(df) == 0);
  const float* JXL_RESTRICT row_sigma = rows.GetSigmaRow();

  float sm = lf.epf_pass2_sigma_scale;
  float bsm = sm * lf.epf_border_sad_mul;

  HWY_ALIGN float sad_mul[kBlockDim] = {bsm, sm, sm, sm, sm, sm, sm, bsm};

  if (image_y_mod_8 == 0 || image_y_mod_8 == kBlockDim - 1) {
    for (size_t i = 0; i < kBlockDim; i += Lanes(df)) {
      Store(Set(df, bsm), df, sad_mul + i);
    }
  }

  for (size_t x = x0; x < x1; x += Lanes(df)) {
    size_t bx = (x + image_x_mod_8) / kBlockDim;
    size_t ix = (x + image_x_mod_8) % kBlockDim;

    if (row_sigma[bx] < kMinSigma) {
      for (size_t c = 0; c < 3; c++) {
        auto px = Load(df, rows.GetInputRow(0, c) + x);
        Store(px, df, rows.GetOutputRow(c) + x);
      }
      continue;
    }

    const auto sm = Load(df, sad_mul + ix);
    const auto inv_sigma = Set(DF(), row_sigma[bx]) * sm;

    const auto x_cc = Load(df, rows.GetInputRow(0, 0) + x);
    const auto y_cc = Load(df, rows.GetInputRow(0, 1) + x);
    const auto b_cc = Load(df, rows.GetInputRow(0, 2) + x);

    auto w = Set(df, 1);
    auto X = x_cc;
    auto Y = y_cc;
    auto B = b_cc;

    // Top row
    AddPixelStep2</*aligned=*/true>(/*row=*/-1, rows, x, x_cc, y_cc, b_cc,
                                    inv_sigma, lf, &X, &Y, &B, &w);
    // Center
    AddPixelStep2</*aligned=*/false>(/*row=*/0, rows, x - 1, x_cc, y_cc, b_cc,
                                     inv_sigma, lf, &X, &Y, &B, &w);
    AddPixelStep2</*aligned=*/false>(/*row=*/0, rows, x + 1, x_cc, y_cc, b_cc,
                                     inv_sigma, lf, &X, &Y, &B, &w);
    // Bottom
    AddPixelStep2</*aligned=*/true>(/*row=*/1, rows, x, x_cc, y_cc, b_cc,
                                    inv_sigma, lf, &X, &Y, &B, &w);

#if JXL_HIGH_PRECISION
    auto inv_w = Set(df, 1.0f) / w;
#else
    auto inv_w = ApproximateReciprocal(w);
#endif
    Store(X * inv_w, df, rows.GetOutputRow(0) + x);
    Store(Y * inv_w, df, rows.GetOutputRow(1) + x);
    Store(B * inv_w, df, rows.GetOutputRow(2) + x);
  }
}

constexpr FilterDefinition kGaborishFilter{&GaborishRow, 1};
constexpr FilterDefinition kEpf0Filter{&Epf0Row, 3};
constexpr FilterDefinition kEpf1Filter{&Epf1Row, 2};
constexpr FilterDefinition kEpf2Filter{&Epf2Row, 1};

void FilterPipelineInit(FilterPipeline* fp, const LoopFilter& lf,
                        const Image3F& in, const Rect& in_rect,
                        const Rect& image_rect, size_t image_ysize,
                        Image3F* out, const Rect& out_rect) {
  JXL_DASSERT(lf.gab || lf.epf_iters > 0);
  // All EPF filters use sigma so we need to compute it.
  fp->compute_sigma = lf.epf_iters > 0;

  fp->num_filters = 0;
  fp->storage_rows_used = 0;
  // First filter always uses the input image.
  fp->filters[0].SetInput(&in, in_rect, image_rect, image_ysize);

  if (lf.gab) {
    fp->AddStep<kGaborishFilter.border>(kGaborishFilter);
  }

  if (lf.epf_iters == 1) {
    fp->AddStep<kEpf1Filter.border>(kEpf1Filter);
  } else if (lf.epf_iters == 2) {
    fp->AddStep<kEpf1Filter.border>(kEpf1Filter);
    fp->AddStep<kEpf2Filter.border>(kEpf2Filter);
  } else if (lf.epf_iters == 3) {
    fp->AddStep<kEpf0Filter.border>(kEpf0Filter);
    fp->AddStep<kEpf1Filter.border>(kEpf1Filter);
    fp->AddStep<kEpf2Filter.border>(kEpf2Filter);
  }

  // At least one of the filters was enabled so "num_filters" must be non-zero.
  JXL_DASSERT(fp->num_filters > 0);

  // Set the output of the last filter as the output image.
  fp->filters[fp->num_filters - 1].SetOutput(out, out_rect);

  // Walk the list of filters backwards to compute how many rows are needed.
  size_t col_border = 0;
  for (int i = fp->num_filters - 1; i >= 0; i--) {
    // The extra border needed for future filtering should be a multiple of
    // Lanes(df). Rounding up in each step but not storing the rounded up
    // value in col_border means that in a 3-step filter the first two filters
    // may have the same output_col_border value but the second one would use
    // uninitialized values from the previous one. It is fine to have this
    // situation for pixels outside the col_border but inside the rounded up
    // col_border.
    fp->filters[i].output_col_border = RoundUpTo(col_border, Lanes(df));
    col_border += fp->filters[i].filter_def.border;
  }
  fp->total_border = col_border;
  JXL_ASSERT(fp->total_border == lf.Padding());
  JXL_ASSERT(fp->total_border <= kMaxFilterBorder);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(FilterPipelineInit);  // Local function

// Mirror n floats starting at *p and store them before p.
JXL_INLINE void LeftMirror(float* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    *(p - 1 - i) = p[i];
  }
}

// Mirror n floats starting at *(p - n) and store them at *p.
JXL_INLINE void RightMirror(float* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    p[i] = *(p - 1 - i);
  }
}

void ComputeSigma(const Rect& block_rect, PassesDecoderState* state) {
  const LoopFilter& lf = state->shared->frame_header.loop_filter;
  JXL_CHECK(lf.epf_iters > 0);
  const AcStrategyImage& ac_strategy = state->shared->ac_strategy;
  const float quant_scale = state->shared->quantizer.Scale();

  const size_t sigma_stride = state->filter_weights.sigma.PixelsPerRow();
  const size_t sharpness_stride = state->shared->epf_sharpness.PixelsPerRow();

  for (size_t by = 0; by < block_rect.ysize(); ++by) {
    float* JXL_RESTRICT sigma_row =
        block_rect.Row(&state->filter_weights.sigma, by);
    const uint8_t* JXL_RESTRICT sharpness_row =
        block_rect.ConstRow(state->shared->epf_sharpness, by);
    AcStrategyRow acs_row = ac_strategy.ConstRow(block_rect, by);
    const int* const JXL_RESTRICT row_quant =
        block_rect.ConstRow(state->shared->raw_quant_field, by);

    for (size_t bx = 0; bx < block_rect.xsize(); bx++) {
      AcStrategy acs = acs_row[bx];
      size_t llf_x = acs.covered_blocks_x();
      if (!acs.IsFirstBlock()) continue;
      // quant_scale is smaller for low quality.
      // quant_scale is roughly 0.08 / butteraugli score.
      //
      // row_quant is smaller for low quality.
      // row_quant is a quantization multiplier of form 1.0 /
      // row_quant[bx]
      //
      // lf.epf_quant_mul is a parameter in the format
      // kInvSigmaNum is a constant
      float sigma_quant =
          lf.epf_quant_mul / (quant_scale * row_quant[bx] * kInvSigmaNum);
      for (size_t iy = 0; iy < acs.covered_blocks_y(); iy++) {
        for (size_t ix = 0; ix < acs.covered_blocks_x(); ix++) {
          float sigma =
              sigma_quant *
              lf.epf_sharp_lut[sharpness_row[bx + ix + iy * sharpness_stride]];
          // Avoid infinities.
          sigma = std::min(-1e-4f, sigma);  // TODO(veluca): remove this.
          sigma_row[bx + ix + kSigmaPadding +
                    (iy + kSigmaPadding) * sigma_stride] = 1.0f / sigma;
        }
      }
      // TODO(veluca): remove this padding.
      // Left padding with mirroring.
      if (bx + block_rect.x0() == 0) {
        for (size_t iy = 0; iy < acs.covered_blocks_y(); iy++) {
          LeftMirror(
              sigma_row + kSigmaPadding + (iy + kSigmaPadding) * sigma_stride,
              kSigmaBorder);
        }
      }
      // Right padding with mirroring.
      if (bx + block_rect.x0() + llf_x ==
          state->shared->frame_dim.xsize_blocks) {
        for (size_t iy = 0; iy < acs.covered_blocks_y(); iy++) {
          RightMirror(sigma_row + kSigmaPadding + bx + llf_x +
                          (iy + kSigmaPadding) * sigma_stride,
                      kSigmaBorder);
        }
      }
      // Offsets for row copying, in blocks.
      size_t offset_before = bx + block_rect.x0() == 0 ? 1 : bx + kSigmaPadding;
      size_t offset_after =
          bx + block_rect.x0() + llf_x == state->shared->frame_dim.xsize_blocks
              ? kSigmaPadding + llf_x + bx + kSigmaBorder
              : kSigmaPadding + llf_x + bx;
      size_t num = offset_after - offset_before;
      // Above
      if (by + block_rect.y0() == 0) {
        for (size_t iy = 0; iy < kSigmaBorder; iy++) {
          memcpy(
              sigma_row + offset_before +
                  (kSigmaPadding - 1 - iy) * sigma_stride,
              sigma_row + offset_before + (kSigmaPadding + iy) * sigma_stride,
              num * sizeof(*sigma_row));
        }
      }
      // Below
      if (by + block_rect.y0() + acs.covered_blocks_y() ==
          state->shared->frame_dim.ysize_blocks) {
        for (size_t iy = 0; iy < kSigmaBorder; iy++) {
          memcpy(
              sigma_row + offset_before +
                  sigma_stride * (acs.covered_blocks_y() + kSigmaPadding + iy),
              sigma_row + offset_before +
                  sigma_stride *
                      (acs.covered_blocks_y() + kSigmaPadding - 1 - iy),
              num * sizeof(*sigma_row));
        }
      }
    }
  }
}

FilterPipeline* PrepareFilterPipeline(
    PassesDecoderState* dec_state, const Rect& image_rect, const Image3F& input,
    const Rect& input_rect, size_t image_ysize, size_t thread,
    Image3F* JXL_RESTRICT out, const Rect& output_rect) {
  const LoopFilter& lf = dec_state->shared->frame_header.loop_filter;
  JXL_DASSERT(image_rect.x0() % GroupBorderAssigner::kPaddingXRound == 0);
  JXL_DASSERT(input_rect.x0() % GroupBorderAssigner::kPaddingXRound == 0);
  JXL_DASSERT(output_rect.x0() % GroupBorderAssigner::kPaddingXRound == 0);
  JXL_DASSERT(input_rect.x0() >= lf.Padding());
  JXL_DASSERT(image_rect.xsize() == input_rect.xsize());
  JXL_DASSERT(image_rect.xsize() == output_rect.xsize());
  FilterPipeline* fp = &(dec_state->filter_pipelines[thread]);
  HWY_DYNAMIC_DISPATCH(FilterPipelineInit)
  (fp, lf, input, input_rect, image_rect, image_ysize, out, output_rect);
  return fp;
}

void ApplyFilters(PassesDecoderState* dec_state, const Rect& image_rect,
                  const Image3F& input, const Rect& input_rect, size_t thread,
                  Image3F* JXL_RESTRICT out, const Rect& output_rect) {
  auto fp = PrepareFilterPipeline(dec_state, image_rect, input, input_rect,
                                  input_rect.ysize(), thread, out, output_rect);
  const LoopFilter& lf = dec_state->shared->frame_header.loop_filter;
  for (ssize_t y = -lf.Padding();
       y < static_cast<ssize_t>(lf.Padding() + image_rect.ysize()); y++) {
    fp->ApplyFiltersRow(lf, dec_state->filter_weights, image_rect, y);
  }
}

}  // namespace jxl
#endif  // HWY_ONCE
