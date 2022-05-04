// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_xyb.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_xyb.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/dec_xyb-inl.h"
#include "lib/jxl/fast_math-inl.h"
#include "lib/jxl/sanitizers.h"
#include "lib/jxl/transfer_functions-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

template <typename Op>
struct PerChannelOp {
  explicit PerChannelOp(Op op) : op(op) {}
  template <typename D, typename T>
  void Transform(D d, T* r, T* g, T* b) const {
    *r = op.Transform(d, *r);
    *g = op.Transform(d, *g);
    *b = op.Transform(d, *b);
  }

  Op op;
};
template <typename Op>
PerChannelOp<Op> MakePerChannelOp(Op&& op) {
  return PerChannelOp<Op>(std::forward<Op>(op));
}

struct OpLinear {
  template <typename D, typename T>
  T Transform(D d, const T& linear) const {
    return linear;
  }
};

struct OpRgb {
  template <typename D, typename T>
  T Transform(D d, const T& linear) const {
#if JXL_HIGH_PRECISION
    return TF_SRGB().EncodedFromDisplay(d, linear);
#else
    return FastLinearToSRGB(d, linear);
#endif
  }
};

struct OpPq {
  template <typename D, typename T>
  T Transform(D d, const T& linear) const {
    return TF_PQ().EncodedFromDisplay(d, linear);
  }
};

struct OpHlg {
  explicit OpHlg(const float luminances[3], const float intensity_target)
      : luminances(luminances), exponent(1.0f) {
    if (295 <= intensity_target && intensity_target <= 305) {
      apply_inverse_ootf = false;
      return;
    }
    exponent =
        (1 / 1.2f) * std::pow(1.111f, -std::log2(intensity_target * 1e-3f)) - 1;
  }
  template <typename D, typename T>
  void Transform(D d, T* r, T* g, T* b) const {
    if (apply_inverse_ootf) {
      const T luminance = Set(d, luminances[0]) * *r +
                          Set(d, luminances[1]) * *g +
                          Set(d, luminances[2]) * *b;
      const T ratio =
          Min(FastPowf(d, luminance, Set(d, exponent)), Set(d, 1e9));
      *r *= ratio;
      *g *= ratio;
      *b *= ratio;
    }
    *r = TF_HLG().EncodedFromDisplay(d, *r);
    *g = TF_HLG().EncodedFromDisplay(d, *g);
    *b = TF_HLG().EncodedFromDisplay(d, *b);
  }
  bool apply_inverse_ootf = true;
  const float* luminances;
  float exponent;
};

struct Op709 {
  template <typename D, typename T>
  T Transform(D d, const T& linear) const {
    return TF_709().EncodedFromDisplay(d, linear);
  }
};

struct OpGamma {
  const float inverse_gamma;
  template <typename D, typename T>
  T Transform(D d, const T& linear) const {
    return IfThenZeroElse(linear <= Set(d, 1e-5f),
                          FastPowf(d, linear, Set(d, inverse_gamma)));
  }
};

template <typename Op>
class XYBStage : public RenderPipelineStage {
 public:
  XYBStage(OpsinParams opsin_params, Op op)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        opsin_params_(opsin_params),
        op_(op) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  size_t thread_id) const final {
    PROFILER_ZONE("UndoXYB");

    const HWY_FULL(float) d;
    const size_t xsize_v = RoundUpTo(xsize, Lanes(d));
    float* JXL_RESTRICT row0 = GetInputRow(input_rows, 0, 0);
    float* JXL_RESTRICT row1 = GetInputRow(input_rows, 1, 0);
    float* JXL_RESTRICT row2 = GetInputRow(input_rows, 2, 0);
    // All calculations are lane-wise, still some might require
    // value-dependent behaviour (e.g. NearestInt). Temporary unpoison last
    // vector tail.
    msan::UnpoisonMemory(row0 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::UnpoisonMemory(row1 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::UnpoisonMemory(row2 + xsize, sizeof(float) * (xsize_v - xsize));
    for (ssize_t x = -xextra; x < (ssize_t)(xsize + xextra); x += Lanes(d)) {
      const auto in_opsin_x = Load(d, row0 + x);
      const auto in_opsin_y = Load(d, row1 + x);
      const auto in_opsin_b = Load(d, row2 + x);
      auto r = Undefined(d);
      auto g = Undefined(d);
      auto b = Undefined(d);
      XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b, opsin_params_, &r, &g,
               &b);
      op_.Transform(d, &r, &g, &b);
      Store(r, d, row0 + x);
      Store(g, d, row1 + x);
      Store(b, d, row2 + x);
    }
    msan::PoisonMemory(row0 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::PoisonMemory(row1 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::PoisonMemory(row2 + xsize, sizeof(float) * (xsize_v - xsize));
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 ? RenderPipelineChannelMode::kInPlace
                 : RenderPipelineChannelMode::kIgnored;
  }

  const char* GetName() const override { return "XYB"; }

 private:
  OpsinParams opsin_params_;
  Op op_;
};

template <typename Op>
std::unique_ptr<XYBStage<Op>> MakeXYBStage(const OpsinParams& opsin_params,
                                           Op&& op) {
  return jxl::make_unique<XYBStage<Op>>(opsin_params, std::forward<Op>(op));
}

std::unique_ptr<RenderPipelineStage> GetXYBStage(
    const OutputEncodingInfo& output_encoding_info) {
  if (output_encoding_info.color_encoding.tf.IsLinear()) {
    return MakeXYBStage(output_encoding_info.opsin_params,
                        MakePerChannelOp(OpLinear()));
  } else if (output_encoding_info.color_encoding.tf.IsSRGB()) {
    return MakeXYBStage(output_encoding_info.opsin_params,
                        MakePerChannelOp(OpRgb()));
  } else if (output_encoding_info.color_encoding.tf.IsPQ()) {
    return MakeXYBStage(output_encoding_info.opsin_params,
                        MakePerChannelOp(OpPq()));
  } else if (output_encoding_info.color_encoding.tf.IsHLG()) {
    return MakeXYBStage(output_encoding_info.opsin_params,
                        OpHlg(output_encoding_info.luminances,
                              output_encoding_info.intensity_target));
  } else if (output_encoding_info.color_encoding.tf.Is709()) {
    return MakeXYBStage(output_encoding_info.opsin_params,
                        MakePerChannelOp(Op709()));
  } else if (output_encoding_info.color_encoding.tf.IsGamma() ||
             output_encoding_info.color_encoding.tf.IsDCI()) {
    return MakeXYBStage(
        output_encoding_info.opsin_params,
        MakePerChannelOp(OpGamma{output_encoding_info.inverse_gamma}));
  } else {
    // This is a programming error.
    JXL_ABORT("Invalid target encoding");
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(GetXYBStage);

std::unique_ptr<RenderPipelineStage> GetXYBStage(
    const OutputEncodingInfo& output_encoding_info) {
  return HWY_DYNAMIC_DISPATCH(GetXYBStage)(output_encoding_info);
}

namespace {
class FastXYBStage : public RenderPipelineStage {
 public:
  FastXYBStage(uint8_t* rgb, size_t stride, size_t width, size_t height,
               bool rgba, bool has_alpha, size_t alpha_c)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        rgb_(rgb),
        stride_(stride),
        width_(width),
        height_(height),
        rgba_(rgba),
        has_alpha_(has_alpha),
        alpha_c_(alpha_c) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  size_t thread_id) const final {
    if (ypos >= height_) return;
    JXL_ASSERT(xextra == 0);
    const float* xyba[4] = {
        GetInputRow(input_rows, 0, 0), GetInputRow(input_rows, 1, 0),
        GetInputRow(input_rows, 2, 0),
        has_alpha_ ? GetInputRow(input_rows, alpha_c_, 0) : nullptr};
    uint8_t* out_buf = rgb_ + stride_ * ypos + (rgba_ ? 4 : 3) * xpos;
    FastXYBTosRGB8(xyba, out_buf, rgba_,
                   xsize + xpos <= width_ ? xsize : width_ - xpos);
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 || (has_alpha_ && c == alpha_c_)
               ? RenderPipelineChannelMode::kInput
               : RenderPipelineChannelMode::kIgnored;
  }

  const char* GetName() const override { return "FastXYB"; }

 private:
  uint8_t* rgb_;
  size_t stride_;
  size_t width_;
  size_t height_;
  bool rgba_;
  bool has_alpha_;
  size_t alpha_c_;
  std::vector<float> opaque_alpha_;
};

}  // namespace

std::unique_ptr<RenderPipelineStage> GetFastXYBTosRGB8Stage(
    uint8_t* rgb, size_t stride, size_t width, size_t height, bool rgba,
    bool has_alpha, size_t alpha_c) {
  JXL_ASSERT(HasFastXYBTosRGB8());
  return make_unique<FastXYBStage>(rgb, stride, width, height, rgba, has_alpha,
                                   alpha_c);
}

}  // namespace jxl
#endif
