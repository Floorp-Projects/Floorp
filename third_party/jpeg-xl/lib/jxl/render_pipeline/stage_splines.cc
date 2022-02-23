// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_splines.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_splines.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

class SplineStage : public RenderPipelineStage {
 public:
  explicit SplineStage(const Splines* splines)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        splines_(*splines) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    PROFILER_ZONE("RenderSplines");
    float* row_x = GetInputRow(input_rows, 0, 0);
    float* row_y = GetInputRow(input_rows, 1, 0);
    float* row_b = GetInputRow(input_rows, 2, 0);
    splines_.AddToRow(row_x, row_y, row_b, Rect(xpos, ypos, xsize, 1));
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 ? RenderPipelineChannelMode::kInPlace
                 : RenderPipelineChannelMode::kIgnored;
  }

 private:
  const Splines& splines_;
};

std::unique_ptr<RenderPipelineStage> GetSplineStage(const Splines* splines) {
  return jxl::make_unique<SplineStage>(splines);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(GetSplineStage);

std::unique_ptr<RenderPipelineStage> GetSplineStage(const Splines* splines) {
  return HWY_DYNAMIC_DISPATCH(GetSplineStage)(splines);
}

}  // namespace jxl
#endif
