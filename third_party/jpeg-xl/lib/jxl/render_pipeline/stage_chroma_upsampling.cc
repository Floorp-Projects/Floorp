// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_chroma_upsampling.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_chroma_upsampling.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/simd_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

class HorizontalChromaUpsamplingStage : public RenderPipelineStage {
 public:
  explicit HorizontalChromaUpsamplingStage(size_t channel)
      : RenderPipelineStage(RenderPipelineStage::Settings::ShiftX(
            /*shift=*/1, /*border=*/1)),
        c_(channel) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    PROFILER_ZONE("HorizontalChromaUpsampling");
    HWY_FULL(float) df;
    xextra = RoundUpTo(xextra, Lanes(df));
    auto threefour = Set(df, 0.75f);
    auto onefour = Set(df, 0.25f);
    const float* row_in = GetInputRow(input_rows, c_, 0);
    float* row_out = GetOutputRow(output_rows, c_, 0);
    for (ssize_t x = -xextra; x < static_cast<ssize_t>(xsize + xextra);
         x += Lanes(df)) {
      auto current = Load(df, row_in + x) * threefour;
      auto prev = LoadU(df, row_in + x - 1);
      auto next = LoadU(df, row_in + x + 1);
      auto left = MulAdd(onefour, prev, current);
      auto right = MulAdd(onefour, next, current);
      StoreInterleaved(df, left, right, row_out + x * 2);
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c == c_ ? RenderPipelineChannelMode::kInOut
                   : RenderPipelineChannelMode::kIgnored;
  }

 private:
  size_t c_;
};

class VerticalChromaUpsamplingStage : public RenderPipelineStage {
 public:
  explicit VerticalChromaUpsamplingStage(size_t channel)
      : RenderPipelineStage(RenderPipelineStage::Settings::ShiftY(
            /*shift=*/1, /*border=*/1)),
        c_(channel) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    PROFILER_ZONE("VerticalChromaUpsampling");
    HWY_FULL(float) df;
    xextra = RoundUpTo(xextra, Lanes(df));
    auto threefour = Set(df, 0.75f);
    auto onefour = Set(df, 0.25f);
    const float* row_top = GetInputRow(input_rows, c_, -1);
    const float* row_mid = GetInputRow(input_rows, c_, 0);
    const float* row_bot = GetInputRow(input_rows, c_, 1);
    float* row_out0 = GetOutputRow(output_rows, c_, 0);
    float* row_out1 = GetOutputRow(output_rows, c_, 1);
    for (ssize_t x = -xextra; x < static_cast<ssize_t>(xsize + xextra);
         x += Lanes(df)) {
      auto it = Load(df, row_top + x);
      auto im = Load(df, row_mid + x);
      auto ib = Load(df, row_bot + x);
      auto im_scaled = im * threefour;
      Store(MulAdd(it, onefour, im_scaled), df, row_out0 + x);
      Store(MulAdd(ib, onefour, im_scaled), df, row_out1 + x);
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c == c_ ? RenderPipelineChannelMode::kInOut
                   : RenderPipelineChannelMode::kIgnored;
  }

 private:
  size_t c_;
};

std::unique_ptr<RenderPipelineStage> GetChromaUpsamplingStage(size_t channel,
                                                              bool horizontal) {
  if (horizontal) {
    return jxl::make_unique<HorizontalChromaUpsamplingStage>(channel);
  } else {
    return jxl::make_unique<VerticalChromaUpsamplingStage>(channel);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(GetChromaUpsamplingStage);

std::unique_ptr<RenderPipelineStage> GetChromaUpsamplingStage(size_t channel,
                                                              bool horizontal) {
  return HWY_DYNAMIC_DISPATCH(GetChromaUpsamplingStage)(channel, horizontal);
}

}  // namespace jxl
#endif
