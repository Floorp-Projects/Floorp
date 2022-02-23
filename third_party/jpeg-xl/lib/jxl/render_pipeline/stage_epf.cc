// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_epf.h"

#include "lib/jxl/epf.h"
#include "lib/jxl/sanitizers.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_epf.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
// TODO(veluca): In principle, vectors could be not capped, if we want to deal
// with having two different sigma values in a single vector.
using DF = HWY_CAPPED(float, 8);

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Vec;

JXL_INLINE Vec<DF> Weight(Vec<DF> sad, Vec<DF> inv_sigma, Vec<DF> thres) {
  auto v = MulAdd(sad, inv_sigma, Set(DF(), 1.0f));
  auto v2 = v * v;
  return IfThenZeroElse(v <= thres, v2);
}

// 5x5 plus-shaped kernel with 5 SADs per pixel (3x3 plus-shaped). So this makes
// this filter a 7x7 filter.
class EPF0Stage : public RenderPipelineStage {
 public:
  EPF0Stage(const LoopFilter& lf, const ImageF& sigma)
      : RenderPipelineStage(RenderPipelineStage::Settings::Symmetric(
            /*shift=*/0, /*border=*/3)),
        lf_(lf),
        sigma_(&sigma) {}

  template <bool aligned>
  JXL_INLINE void AddPixel(int row, const RowInfo& input_rows, ssize_t x,
                           Vec<DF> sad, Vec<DF> inv_sigma,
                           Vec<DF>* JXL_RESTRICT X, Vec<DF>* JXL_RESTRICT Y,
                           Vec<DF>* JXL_RESTRICT B,
                           Vec<DF>* JXL_RESTRICT w) const {
    auto cx = aligned ? Load(DF(), GetInputRow(input_rows, 0, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 0, row) + x);
    auto cy = aligned ? Load(DF(), GetInputRow(input_rows, 1, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 1, row) + x);
    auto cb = aligned ? Load(DF(), GetInputRow(input_rows, 2, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 2, row) + x);

    auto weight = Weight(sad, inv_sigma, Set(DF(), lf_.epf_pass1_zeroflush));
    *w += weight;
    *X = MulAdd(weight, cx, *X);
    *Y = MulAdd(weight, cy, *Y);
    *B = MulAdd(weight, cb, *B);
  }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    DF df;
    xextra = RoundUpTo(xextra, Lanes(df));
    const float* JXL_RESTRICT row_sigma =
        sigma_->Row(ypos / kBlockDim + kSigmaPadding);

    float sm = lf_.epf_pass0_sigma_scale;
    float bsm = sm * lf_.epf_border_sad_mul;

    HWY_ALIGN float sad_mul_center[kBlockDim] = {bsm, sm, sm, sm,
                                                 sm,  sm, sm, bsm};
    HWY_ALIGN float sad_mul_border[kBlockDim] = {bsm, bsm, bsm, bsm,
                                                 bsm, bsm, bsm, bsm};

    const float* sad_mul =
        (ypos % kBlockDim == 0 || ypos % kBlockDim == kBlockDim - 1)
            ? sad_mul_border
            : sad_mul_center;

    for (ssize_t x = -xextra; x < static_cast<ssize_t>(xsize + xextra);
         x += Lanes(df)) {
      size_t bx = (x + xpos + kSigmaPadding * kBlockDim) / kBlockDim;
      size_t ix = (x + xpos) % kBlockDim;

      if (row_sigma[bx] < kMinSigma) {
        for (size_t c = 0; c < 3; c++) {
          auto px = Load(df, GetInputRow(input_rows, c, 0) + x);
          Store(px, df, GetOutputRow(output_rows, c, 0) + x);
        }
        continue;
      }

      const auto sm = Load(df, sad_mul + ix);
      const auto inv_sigma = Set(df, row_sigma[bx]) * sm;

      decltype(Zero(df)) sads[12];
      for (size_t i = 0; i < 12; i++) sads[i] = Zero(df);
      constexpr std::array<int, 2> sads_off[12] = {
          {{-2, 0}}, {{-1, -1}}, {{-1, 0}}, {{-1, 1}}, {{0, -2}}, {{0, -1}},
          {{0, 1}},  {{0, 2}},   {{1, -1}}, {{1, 0}},  {{1, 1}},  {{2, 0}},
      };

      // compute sads
      // TODO(veluca): consider unrolling and optimizing this.
      for (size_t c = 0; c < 3; c++) {
        auto scale = Set(df, lf_.epf_channel_scale[c]);
        for (size_t i = 0; i < 12; i++) {
          auto sad = Zero(df);
          constexpr std::array<int, 2> plus_off[] = {
              {{0, 0}}, {{-1, 0}}, {{0, -1}}, {{1, 0}}, {{0, 1}}};
          for (size_t j = 0; j < 5; j++) {
            const auto r11 =
                LoadU(df, GetInputRow(input_rows, c, plus_off[j][0]) + x +
                              plus_off[j][1]);
            const auto c11 = LoadU(
                df,
                GetInputRow(input_rows, c, sads_off[i][0] + plus_off[j][0]) +
                    x + sads_off[i][1] + plus_off[j][1]);
            sad += AbsDiff(r11, c11);
          }
          sads[i] = MulAdd(sad, scale, sads[i]);
        }
      }
      const auto x_cc = Load(df, GetInputRow(input_rows, 0, 0) + x);
      const auto y_cc = Load(df, GetInputRow(input_rows, 1, 0) + x);
      const auto b_cc = Load(df, GetInputRow(input_rows, 2, 0) + x);

      auto w = Set(df, 1);
      auto X = x_cc;
      auto Y = y_cc;
      auto B = b_cc;

      for (size_t i = 0; i < 12; i++) {
        AddPixel</*aligned=*/false>(/*row=*/sads_off[i][0], input_rows,
                                    x + sads_off[i][1], sads[i], inv_sigma, &X,
                                    &Y, &B, &w);
      }
#if JXL_HIGH_PRECISION
      auto inv_w = Set(df, 1.0f) / w;
#else
      auto inv_w = ApproximateReciprocal(w);
#endif
      Store(X * inv_w, df, GetOutputRow(output_rows, 0, 0) + x);
      Store(Y * inv_w, df, GetOutputRow(output_rows, 1, 0) + x);
      Store(B * inv_w, df, GetOutputRow(output_rows, 2, 0) + x);
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 ? RenderPipelineChannelMode::kInOut
                 : RenderPipelineChannelMode::kIgnored;
  }

 private:
  LoopFilter lf_;
  const ImageF* sigma_;
};

// 3x3 plus-shaped kernel with 5 SADs per pixel (also 3x3 plus-shaped). So this
// makes this filter a 5x5 filter.
class EPF1Stage : public RenderPipelineStage {
 public:
  EPF1Stage(const LoopFilter& lf, const ImageF& sigma)
      : RenderPipelineStage(RenderPipelineStage::Settings::Symmetric(
            /*shift=*/0, /*border=*/2)),
        lf_(lf),
        sigma_(&sigma) {}

  template <bool aligned>
  JXL_INLINE void AddPixel(int row, const RowInfo& input_rows, ssize_t x,
                           Vec<DF> sad, Vec<DF> inv_sigma,
                           Vec<DF>* JXL_RESTRICT X, Vec<DF>* JXL_RESTRICT Y,
                           Vec<DF>* JXL_RESTRICT B,
                           Vec<DF>* JXL_RESTRICT w) const {
    auto cx = aligned ? Load(DF(), GetInputRow(input_rows, 0, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 0, row) + x);
    auto cy = aligned ? Load(DF(), GetInputRow(input_rows, 1, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 1, row) + x);
    auto cb = aligned ? Load(DF(), GetInputRow(input_rows, 2, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 2, row) + x);

    auto weight = Weight(sad, inv_sigma, Set(DF(), lf_.epf_pass1_zeroflush));
    *w += weight;
    *X = MulAdd(weight, cx, *X);
    *Y = MulAdd(weight, cy, *Y);
    *B = MulAdd(weight, cb, *B);
  }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    DF df;
    xextra = RoundUpTo(xextra, Lanes(df));
    const float* JXL_RESTRICT row_sigma =
        sigma_->Row(ypos / kBlockDim + kSigmaPadding);

    float sm = 1.0f;
    float bsm = sm * lf_.epf_border_sad_mul;

    HWY_ALIGN float sad_mul_center[kBlockDim] = {bsm, sm, sm, sm,
                                                 sm,  sm, sm, bsm};
    HWY_ALIGN float sad_mul_border[kBlockDim] = {bsm, bsm, bsm, bsm,
                                                 bsm, bsm, bsm, bsm};

    const float* sad_mul =
        (ypos % kBlockDim == 0 || ypos % kBlockDim == kBlockDim - 1)
            ? sad_mul_border
            : sad_mul_center;

    for (ssize_t x = -xextra; x < static_cast<ssize_t>(xsize + xextra);
         x += Lanes(df)) {
      size_t bx = (x + xpos + kSigmaPadding * kBlockDim) / kBlockDim;
      size_t ix = (x + xpos) % kBlockDim;

      if (row_sigma[bx] < kMinSigma) {
        for (size_t c = 0; c < 3; c++) {
          auto px = Load(df, GetInputRow(input_rows, c, 0) + x);
          Store(px, df, GetOutputRow(output_rows, c, 0) + x);
        }
        continue;
      }

      const auto sm = Load(df, sad_mul + ix);
      const auto inv_sigma = Set(df, row_sigma[bx]) * sm;
      auto sad0 = Zero(df);
      auto sad1 = Zero(df);
      auto sad2 = Zero(df);
      auto sad3 = Zero(df);

      // compute sads
      for (size_t c = 0; c < 3; c++) {
        // center px = 22, px above = 21
        auto t = Undefined(df);

        const auto p20 = Load(df, GetInputRow(input_rows, c, -2) + x);
        const auto p21 = Load(df, GetInputRow(input_rows, c, -1) + x);
        auto sad0c = AbsDiff(p20, p21);  // SAD 2, 1

        const auto p11 = LoadU(df, GetInputRow(input_rows, c, -1) + x - 1);
        auto sad1c = AbsDiff(p11, p21);  // SAD 1, 2

        const auto p31 = LoadU(df, GetInputRow(input_rows, c, -1) + x + 1);
        auto sad2c = AbsDiff(p31, p21);  // SAD 3, 2

        const auto p02 = LoadU(df, GetInputRow(input_rows, c, 0) + x - 2);
        const auto p12 = LoadU(df, GetInputRow(input_rows, c, 0) + x - 1);
        sad1c += AbsDiff(p02, p12);  // SAD 1, 2
        sad0c += AbsDiff(p11, p12);  // SAD 2, 1

        const auto p22 = LoadU(df, GetInputRow(input_rows, c, 0) + x);
        t = AbsDiff(p12, p22);
        sad1c += t;  // SAD 1, 2
        sad2c += t;  // SAD 3, 2
        t = AbsDiff(p22, p21);
        auto sad3c = t;  // SAD 2, 3
        sad0c += t;      // SAD 2, 1

        const auto p32 = LoadU(df, GetInputRow(input_rows, c, 0) + x + 1);
        sad0c += AbsDiff(p31, p32);  // SAD 2, 1
        t = AbsDiff(p22, p32);
        sad1c += t;  // SAD 1, 2
        sad2c += t;  // SAD 3, 2

        const auto p42 = LoadU(df, GetInputRow(input_rows, c, 0) + x + 2);
        sad2c += AbsDiff(p42, p32);  // SAD 3, 2

        const auto p13 = LoadU(df, GetInputRow(input_rows, c, 1) + x - 1);
        sad3c += AbsDiff(p13, p12);  // SAD 2, 3

        const auto p23 = Load(df, GetInputRow(input_rows, c, 1) + x);
        t = AbsDiff(p22, p23);
        sad0c += t;                  // SAD 2, 1
        sad3c += t;                  // SAD 2, 3
        sad1c += AbsDiff(p13, p23);  // SAD 1, 2

        const auto p33 = LoadU(df, GetInputRow(input_rows, c, 1) + x + 1);
        sad2c += AbsDiff(p33, p23);  // SAD 3, 2
        sad3c += AbsDiff(p33, p32);  // SAD 2, 3

        const auto p24 = Load(df, GetInputRow(input_rows, c, 2) + x);
        sad3c += AbsDiff(p24, p23);  // SAD 2, 3

        auto scale = Set(df, lf_.epf_channel_scale[c]);
        sad0 = MulAdd(sad0c, scale, sad0);
        sad1 = MulAdd(sad1c, scale, sad1);
        sad2 = MulAdd(sad2c, scale, sad2);
        sad3 = MulAdd(sad3c, scale, sad3);
      }
      const auto x_cc = Load(df, GetInputRow(input_rows, 0, 0) + x);
      const auto y_cc = Load(df, GetInputRow(input_rows, 1, 0) + x);
      const auto b_cc = Load(df, GetInputRow(input_rows, 2, 0) + x);

      auto w = Set(df, 1);
      auto X = x_cc;
      auto Y = y_cc;
      auto B = b_cc;

      // Top row
      AddPixel</*aligned=*/true>(/*row=*/-1, input_rows, x, sad0, inv_sigma, &X,
                                 &Y, &B, &w);
      // Center
      AddPixel</*aligned=*/false>(/*row=*/0, input_rows, x - 1, sad1, inv_sigma,
                                  &X, &Y, &B, &w);
      AddPixel</*aligned=*/false>(/*row=*/0, input_rows, x + 1, sad2, inv_sigma,
                                  &X, &Y, &B, &w);
      // Bottom
      AddPixel</*aligned=*/true>(/*row=*/1, input_rows, x, sad3, inv_sigma, &X,
                                 &Y, &B, &w);
#if JXL_HIGH_PRECISION
      auto inv_w = Set(df, 1.0f) / w;
#else
      auto inv_w = ApproximateReciprocal(w);
#endif
      Store(X * inv_w, df, GetOutputRow(output_rows, 0, 0) + x);
      Store(Y * inv_w, df, GetOutputRow(output_rows, 1, 0) + x);
      Store(B * inv_w, df, GetOutputRow(output_rows, 2, 0) + x);
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 ? RenderPipelineChannelMode::kInOut
                 : RenderPipelineChannelMode::kIgnored;
  }

 private:
  LoopFilter lf_;
  const ImageF* sigma_;
};

// 3x3 plus-shaped kernel with 1 SAD per pixel. So this makes this filter a 3x3
// filter.
class EPF2Stage : public RenderPipelineStage {
 public:
  EPF2Stage(const LoopFilter& lf, const ImageF& sigma)
      : RenderPipelineStage(RenderPipelineStage::Settings::Symmetric(
            /*shift=*/0, /*border=*/1)),
        lf_(lf),
        sigma_(&sigma) {}

  template <bool aligned>
  JXL_INLINE void AddPixel(int row, const RowInfo& input_rows, ssize_t x,
                           Vec<DF> rx, Vec<DF> ry, Vec<DF> rb,
                           Vec<DF> inv_sigma, Vec<DF>* JXL_RESTRICT X,
                           Vec<DF>* JXL_RESTRICT Y, Vec<DF>* JXL_RESTRICT B,
                           Vec<DF>* JXL_RESTRICT w) const {
    auto cx = aligned ? Load(DF(), GetInputRow(input_rows, 0, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 0, row) + x);
    auto cy = aligned ? Load(DF(), GetInputRow(input_rows, 1, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 1, row) + x);
    auto cb = aligned ? Load(DF(), GetInputRow(input_rows, 2, row) + x)
                      : LoadU(DF(), GetInputRow(input_rows, 2, row) + x);

    auto sad = AbsDiff(cx, rx) * Set(DF(), lf_.epf_channel_scale[0]);
    sad = MulAdd(AbsDiff(cy, ry), Set(DF(), lf_.epf_channel_scale[1]), sad);
    sad = MulAdd(AbsDiff(cb, rb), Set(DF(), lf_.epf_channel_scale[2]), sad);

    auto weight = Weight(sad, inv_sigma, Set(DF(), lf_.epf_pass2_zeroflush));

    *w += weight;
    *X = MulAdd(weight, cx, *X);
    *Y = MulAdd(weight, cy, *Y);
    *B = MulAdd(weight, cb, *B);
  }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    DF df;
    xextra = RoundUpTo(xextra, Lanes(df));
    const float* JXL_RESTRICT row_sigma =
        sigma_->Row(ypos / kBlockDim + kSigmaPadding);

    float sm = lf_.epf_pass2_sigma_scale;
    float bsm = sm * lf_.epf_border_sad_mul;

    HWY_ALIGN float sad_mul_center[kBlockDim] = {bsm, sm, sm, sm,
                                                 sm,  sm, sm, bsm};
    HWY_ALIGN float sad_mul_border[kBlockDim] = {bsm, bsm, bsm, bsm,
                                                 bsm, bsm, bsm, bsm};

    const float* sad_mul =
        (ypos % kBlockDim == 0 || ypos % kBlockDim == kBlockDim - 1)
            ? sad_mul_border
            : sad_mul_center;

    for (ssize_t x = -xextra; x < static_cast<ssize_t>(xsize + xextra);
         x += Lanes(df)) {
      size_t bx = (x + xpos + kSigmaPadding * kBlockDim) / kBlockDim;
      size_t ix = (x + xpos) % kBlockDim;

      if (row_sigma[bx] < kMinSigma) {
        for (size_t c = 0; c < 3; c++) {
          auto px = Load(df, GetInputRow(input_rows, c, 0) + x);
          Store(px, df, GetOutputRow(output_rows, c, 0) + x);
        }
        continue;
      }

      const auto sm = Load(df, sad_mul + ix);
      const auto inv_sigma = Set(df, row_sigma[bx]) * sm;

      const auto x_cc = Load(df, GetInputRow(input_rows, 0, 0) + x);
      const auto y_cc = Load(df, GetInputRow(input_rows, 1, 0) + x);
      const auto b_cc = Load(df, GetInputRow(input_rows, 2, 0) + x);

      auto w = Set(df, 1);
      auto X = x_cc;
      auto Y = y_cc;
      auto B = b_cc;

      // Top row
      AddPixel</*aligned=*/true>(/*row=*/-1, input_rows, x, x_cc, y_cc, b_cc,
                                 inv_sigma, &X, &Y, &B, &w);
      // Center
      AddPixel</*aligned=*/false>(/*row=*/0, input_rows, x - 1, x_cc, y_cc,
                                  b_cc, inv_sigma, &X, &Y, &B, &w);
      AddPixel</*aligned=*/false>(/*row=*/0, input_rows, x + 1, x_cc, y_cc,
                                  b_cc, inv_sigma, &X, &Y, &B, &w);
      // Bottom
      AddPixel</*aligned=*/true>(/*row=*/1, input_rows, x, x_cc, y_cc, b_cc,
                                 inv_sigma, &X, &Y, &B, &w);
#if JXL_HIGH_PRECISION
      auto inv_w = Set(df, 1.0f) / w;
#else
      auto inv_w = ApproximateReciprocal(w);
#endif
      Store(X * inv_w, df, GetOutputRow(output_rows, 0, 0) + x);
      Store(Y * inv_w, df, GetOutputRow(output_rows, 1, 0) + x);
      Store(B * inv_w, df, GetOutputRow(output_rows, 2, 0) + x);
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 ? RenderPipelineChannelMode::kInOut
                 : RenderPipelineChannelMode::kIgnored;
  }

 private:
  LoopFilter lf_;
  const ImageF* sigma_;
};

std::unique_ptr<RenderPipelineStage> GetEPFStage0(const LoopFilter& lf,
                                                  const ImageF& sigma) {
  return jxl::make_unique<EPF0Stage>(lf, sigma);
}

std::unique_ptr<RenderPipelineStage> GetEPFStage1(const LoopFilter& lf,
                                                  const ImageF& sigma) {
  return jxl::make_unique<EPF1Stage>(lf, sigma);
}

std::unique_ptr<RenderPipelineStage> GetEPFStage2(const LoopFilter& lf,
                                                  const ImageF& sigma) {
  return jxl::make_unique<EPF2Stage>(lf, sigma);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(GetEPFStage0);
HWY_EXPORT(GetEPFStage1);
HWY_EXPORT(GetEPFStage2);

std::unique_ptr<RenderPipelineStage> GetEPFStage(const LoopFilter& lf,
                                                 const ImageF& sigma,
                                                 size_t epf_stage) {
  JXL_ASSERT(lf.epf_iters != 0);
  switch (epf_stage) {
    case 0:
      return HWY_DYNAMIC_DISPATCH(GetEPFStage0)(lf, sigma);
    case 1:
      return HWY_DYNAMIC_DISPATCH(GetEPFStage1)(lf, sigma);
    case 2:
      return HWY_DYNAMIC_DISPATCH(GetEPFStage2)(lf, sigma);
    default:
      JXL_ABORT("Invalid EPF stage");
  }
  return nullptr;
}

}  // namespace jxl
#endif
