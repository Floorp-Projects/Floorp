// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_ycbcr.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_ycbcr.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

class kYCbCrStage : public RenderPipelineStage {
 public:
  kYCbCrStage() : RenderPipelineStage(RenderPipelineStage::Settings()) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    PROFILER_ZONE("UndoYCbCr");

    const HWY_FULL(float) df;

    // Full-range BT.601 as defined by JFIF Clause 7:
    // https://www.itu.int/rec/T-REC-T.871-201105-I/en
    const auto c128 = Set(df, 128.0f / 255);
    const auto crcr = Set(df, 1.402f);
    const auto cgcb = Set(df, -0.114f * 1.772f / 0.587f);
    const auto cgcr = Set(df, -0.299f * 1.402f / 0.587f);
    const auto cbcb = Set(df, 1.772f);

    float* JXL_RESTRICT row0 = GetInputRow(input_rows, 0, 0);
    float* JXL_RESTRICT row1 = GetInputRow(input_rows, 1, 0);
    float* JXL_RESTRICT row2 = GetInputRow(input_rows, 2, 0);
    for (size_t x = 0; x < xsize; x += Lanes(df)) {
      const auto y_vec = Load(df, row1 + x) + c128;
      const auto cb_vec = Load(df, row0 + x);
      const auto cr_vec = Load(df, row2 + x);
      const auto r_vec = crcr * cr_vec + y_vec;
      const auto g_vec = cgcr * cr_vec + cgcb * cb_vec + y_vec;
      const auto b_vec = cbcb * cb_vec + y_vec;
      Store(r_vec, df, row0 + x);
      Store(g_vec, df, row1 + x);
      Store(b_vec, df, row2 + x);
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 ? RenderPipelineChannelMode::kInPlace
                 : RenderPipelineChannelMode::kIgnored;
  }
};

std::unique_ptr<RenderPipelineStage> GetYCbCrStage() {
  return jxl::make_unique<kYCbCrStage>();
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(GetYCbCrStage);

std::unique_ptr<RenderPipelineStage> GetYCbCrStage() {
  return HWY_DYNAMIC_DISPATCH(GetYCbCrStage)();
}

}  // namespace jxl
#endif
