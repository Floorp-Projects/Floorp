// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_reconstruct.h"

#include <atomic>
#include <utility>

#include "lib/jxl/filters.h"
#include "lib/jxl/image_ops.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_reconstruct.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/blending.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_noise.h"
#include "lib/jxl/dec_upsample.h"
#include "lib/jxl/dec_xyb-inl.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/epf.h"
#include "lib/jxl/fast_math-inl.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/passes_state.h"
#include "lib/jxl/sanitizers.h"
#include "lib/jxl/transfer_functions-inl.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

template <typename Op>
void DoUndoXYBInPlace(Image3F* idct, const Rect& rect, Op op,
                      const OutputEncodingInfo& output_encoding_info) {
  // TODO(eustas): should it still be capped?
  const HWY_CAPPED(float, GroupBorderAssigner::kPaddingXRound) d;
  const size_t xsize = rect.xsize();
  const size_t xsize_v = RoundUpTo(xsize, Lanes(d));
  // The size of `rect` might not be a multiple of Lanes(d), but is guaranteed
  // to be a multiple of kBlockDim or at the margin of the image.
  for (size_t y = 0; y < rect.ysize(); y++) {
    float* JXL_RESTRICT row0 = rect.PlaneRow(idct, 0, y);
    float* JXL_RESTRICT row1 = rect.PlaneRow(idct, 1, y);
    float* JXL_RESTRICT row2 = rect.PlaneRow(idct, 2, y);
    // All calculations are lane-wise, still some might require value-dependent
    // behaviour (e.g. NearestInt). Temporary unposion last vector tail.
    msan::UnpoisonMemory(row0 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::UnpoisonMemory(row1 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::UnpoisonMemory(row2 + xsize, sizeof(float) * (xsize_v - xsize));
    for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
      const auto in_opsin_x = Load(d, row0 + x);
      const auto in_opsin_y = Load(d, row1 + x);
      const auto in_opsin_b = Load(d, row2 + x);
      auto r = Undefined(d);
      auto g = Undefined(d);
      auto b = Undefined(d);
      XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b,
               output_encoding_info.opsin_params, &r, &g, &b);
      op.Transform(d, &r, &g, &b);
      Store(r, d, row0 + x);
      Store(g, d, row1 + x);
      Store(b, d, row2 + x);
    }
    msan::PoisonMemory(row0 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::PoisonMemory(row1 + xsize, sizeof(float) * (xsize_v - xsize));
    msan::PoisonMemory(row2 + xsize, sizeof(float) * (xsize_v - xsize));
  }
}

template <typename Op>
struct PerChannelOp {
  template <typename... Args>
  explicit PerChannelOp(Args&&... args) : op(std::forward<Args>(args)...) {}
  template <typename D, typename T>
  void Transform(D d, T* r, T* g, T* b) {
    *r = op.Transform(d, *r);
    *g = op.Transform(d, *g);
    *b = op.Transform(d, *b);
  }

  Op op;
};

struct OpLinear {
  template <typename D, typename T>
  T Transform(D d, const T& linear) {
    return linear;
  }
};

struct OpRgb {
  template <typename D, typename T>
  T Transform(D d, const T& linear) {
#if JXL_HIGH_PRECISION
    return TF_SRGB().EncodedFromDisplay(d, linear);
#else
    return FastLinearToSRGB(d, linear);
#endif
  }
};

struct OpPq {
  template <typename D, typename T>
  T Transform(D d, const T& linear) {
    return TF_PQ().EncodedFromDisplay(d, linear);
  }
};

struct OpHlg {
  explicit OpHlg(const float luminances[3], const float intensity_target)
      : luminances(luminances) {
    if (295 <= intensity_target && intensity_target <= 305) {
      apply_inverse_ootf = false;
      return;
    }
    exponent =
        (1 / 1.2f) * std::pow(1.111f, -std::log2(intensity_target * 1e-3f)) - 1;
  }
  template <typename D, typename T>
  void Transform(D d, T* r, T* g, T* b) {
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
  T Transform(D d, const T& linear) {
    return TF_709().EncodedFromDisplay(d, linear);
  }
};

struct OpGamma {
  explicit OpGamma(const float inverse_gamma) : inverse_gamma(inverse_gamma) {}
  const float inverse_gamma;
  template <typename D, typename T>
  T Transform(D d, const T& linear) {
    return IfThenZeroElse(linear <= Set(d, 1e-5f),
                          FastPowf(d, linear, Set(d, inverse_gamma)));
  }
};

Status UndoXYBInPlace(Image3F* idct, const Rect& rect,
                      const OutputEncodingInfo& output_encoding_info) {
  PROFILER_ZONE("UndoXYB");

  if (output_encoding_info.color_encoding.tf.IsLinear()) {
    DoUndoXYBInPlace(idct, rect, PerChannelOp<OpLinear>(),
                     output_encoding_info);
  } else if (output_encoding_info.color_encoding.tf.IsSRGB()) {
    DoUndoXYBInPlace(idct, rect, PerChannelOp<OpRgb>(), output_encoding_info);
  } else if (output_encoding_info.color_encoding.tf.IsPQ()) {
    DoUndoXYBInPlace(idct, rect, PerChannelOp<OpPq>(), output_encoding_info);
  } else if (output_encoding_info.color_encoding.tf.IsHLG()) {
    DoUndoXYBInPlace(idct, rect,
                     OpHlg(output_encoding_info.luminances,
                           output_encoding_info.intensity_target),
                     output_encoding_info);
  } else if (output_encoding_info.color_encoding.tf.Is709()) {
    DoUndoXYBInPlace(idct, rect, PerChannelOp<Op709>(), output_encoding_info);
  } else if (output_encoding_info.color_encoding.tf.IsGamma() ||
             output_encoding_info.color_encoding.tf.IsDCI()) {
    DoUndoXYBInPlace(idct, rect,
                     PerChannelOp<OpGamma>(output_encoding_info.inverse_gamma),
                     output_encoding_info);
  } else {
    // This is a programming error.
    JXL_ABORT("Invalid target encoding");
  }
  return true;
}

template <typename D, typename V>
void StoreRGBA(D d, V r, V g, V b, V a, bool alpha, size_t n, size_t extra,
               uint8_t* buf) {
#if HWY_TARGET == HWY_SCALAR
  buf[0] = r.raw;
  buf[1] = g.raw;
  buf[2] = b.raw;
  if (alpha) {
    buf[3] = a.raw;
  }
#elif HWY_TARGET == HWY_NEON
  if (alpha) {
    uint8x8x4_t data = {r.raw, g.raw, b.raw, a.raw};
    if (extra >= 8) {
      vst4_u8(buf, data);
    } else {
      uint8_t tmp[8 * 4];
      vst4_u8(tmp, data);
      memcpy(buf, tmp, n * 4);
    }
  } else {
    uint8x8x3_t data = {r.raw, g.raw, b.raw};
    if (extra >= 8) {
      vst3_u8(buf, data);
    } else {
      uint8_t tmp[8 * 3];
      vst3_u8(tmp, data);
      memcpy(buf, tmp, n * 3);
    }
  }
#else
  // TODO(veluca): implement this for x86.
  size_t mul = alpha ? 4 : 3;
  HWY_ALIGN uint8_t bytes[16];
  Store(r, d, bytes);
  for (size_t i = 0; i < n; i++) {
    buf[mul * i] = bytes[i];
  }
  Store(g, d, bytes);
  for (size_t i = 0; i < n; i++) {
    buf[mul * i + 1] = bytes[i];
  }
  Store(b, d, bytes);
  for (size_t i = 0; i < n; i++) {
    buf[mul * i + 2] = bytes[i];
  }
  if (alpha) {
    Store(a, d, bytes);
    for (size_t i = 0; i < n; i++) {
      buf[4 * i + 3] = bytes[i];
    }
  }
#endif
}

// Outputs floating point image to RGBA 8-bit buffer. Does not support alpha
// channel in the input, but outputs opaque alpha channel for the case where the
// output buffer to write to is in the 4-byte per pixel RGBA format.
void FloatToRGBA8(const Image3F& input, const Rect& input_rect, bool is_rgba,
                  const ImageF* alpha_in, const Rect& alpha_rect,
                  const Rect& output_buf_rect, uint8_t* JXL_RESTRICT output_buf,
                  size_t stride) {
  size_t bytes = is_rgba ? 4 : 3;
  for (size_t y = 0; y < output_buf_rect.ysize(); y++) {
    const float* JXL_RESTRICT row_in_r = input_rect.ConstPlaneRow(input, 0, y);
    const float* JXL_RESTRICT row_in_g = input_rect.ConstPlaneRow(input, 1, y);
    const float* JXL_RESTRICT row_in_b = input_rect.ConstPlaneRow(input, 2, y);
    const float* JXL_RESTRICT row_in_a =
        alpha_in ? alpha_rect.ConstRow(*alpha_in, y) : nullptr;
    size_t base_ptr =
        (y + output_buf_rect.y0()) * stride + bytes * output_buf_rect.x0();
    using D = HWY_CAPPED(float, 4);
    const D d;
    D::Rebind<uint32_t> du;
    auto zero = Zero(d);
    auto one = Set(d, 1.0f);
    auto mul = Set(d, 255.0f);

    // All calculations are lane-wise, still some might require value-dependent
    // behaviour (e.g. NearestInt). Temporary unposion last vector tail.
    size_t xsize = output_buf_rect.xsize();
    size_t xsize_v = RoundUpTo(xsize, Lanes(d));
    msan::UnpoisonMemory(row_in_r + xsize, sizeof(float) * (xsize_v - xsize));
    msan::UnpoisonMemory(row_in_g + xsize, sizeof(float) * (xsize_v - xsize));
    msan::UnpoisonMemory(row_in_b + xsize, sizeof(float) * (xsize_v - xsize));
    if (row_in_a)
      msan::UnpoisonMemory(row_in_a + xsize, sizeof(float) * (xsize_v - xsize));
    for (size_t x = 0; x < xsize; x += Lanes(d)) {
      auto rf = Clamp(zero, Load(d, row_in_r + x), one) * mul;
      auto gf = Clamp(zero, Load(d, row_in_g + x), one) * mul;
      auto bf = Clamp(zero, Load(d, row_in_b + x), one) * mul;
      auto af = row_in_a ? Clamp(zero, Load(d, row_in_a + x), one) * mul
                         : Set(d, 255.0f);
      auto r8 = U8FromU32(BitCast(du, NearestInt(rf)));
      auto g8 = U8FromU32(BitCast(du, NearestInt(gf)));
      auto b8 = U8FromU32(BitCast(du, NearestInt(bf)));
      auto a8 = U8FromU32(BitCast(du, NearestInt(af)));
      size_t n = output_buf_rect.xsize() - x;
      if (JXL_LIKELY(n >= Lanes(d))) {
        StoreRGBA(D::Rebind<uint8_t>(), r8, g8, b8, a8, is_rgba, Lanes(d), n,
                  output_buf + base_ptr + bytes * x);
      } else {
        StoreRGBA(D::Rebind<uint8_t>(), r8, g8, b8, a8, is_rgba, n, n,
                  output_buf + base_ptr + bytes * x);
      }
    }
    msan::PoisonMemory(row_in_r + xsize, sizeof(float) * (xsize_v - xsize));
    msan::PoisonMemory(row_in_g + xsize, sizeof(float) * (xsize_v - xsize));
    msan::PoisonMemory(row_in_b + xsize, sizeof(float) * (xsize_v - xsize));
    if (row_in_a)
      msan::PoisonMemory(row_in_a + xsize, sizeof(float) * (xsize_v - xsize));
  }
}

// Upsample in horizonal (if hs=1) and vertical (if vs=1) the plane_in image
// to the output plane_out image.
// The output region "rect" in plane_out and a border around it of lf.Padding()
// will be generated, as long as those pixels fall inside the image frame.
// Otherwise the border pixels that fall outside the image frame in plane_out
// are undefined.
// "rect" is an area inside the plane_out image which corresponds to the
// "frame_rect" area in the frame. plane_in and plane_out both are expected to
// have a padding of kGroupDataXBorder and kGroupDataYBorder on either side of
// X and Y coordinates. This means that when upsampling vertically the plane_out
// row `kGroupDataXBorder + N` will be generated from the plane_in row
// `kGroupDataXBorder + N / 2` (and a previous or next row).
void DoYCbCrUpsampling(size_t hs, size_t vs, ImageF* plane_in, const Rect& rect,
                       const Rect& frame_rect, const FrameDimensions& frame_dim,
                       ImageF* plane_out, const LoopFilter& lf, ImageF* temp) {
  JXL_DASSERT(SameSize(rect, frame_rect));
  JXL_DASSERT(hs <= 1 && vs <= 1);
  // The pixel in (xoff, yoff) is the origin of the downsampling coordinate
  // system.
  size_t xoff = PassesDecoderState::kGroupDataXBorder;
  size_t yoff = PassesDecoderState::kGroupDataYBorder;

  // This X,Y range is the intersection between the requested "rect" expanded
  // with a lf.Padding() all around and the image frame translated to the
  // coordinate system used by plane_out.
  // All the pixels in the [x0, x1) x [y0, y1) range must be defined in the
  // plane_out output at the end.
  const size_t y0 = rect.y0() - std::min<size_t>(lf.Padding(), frame_rect.y0());
  const size_t y1 = rect.y0() +
                    std::min(frame_rect.y0() + rect.ysize() + lf.Padding(),
                             frame_dim.ysize_padded) -
                    frame_rect.y0();

  const size_t x0 = rect.x0() - std::min<size_t>(lf.Padding(), frame_rect.x0());
  const size_t x1 = rect.x0() +
                    std::min(frame_rect.x0() + rect.xsize() + lf.Padding(),
                             frame_dim.xsize_padded) -
                    frame_rect.x0();

  if (hs == 0 && vs == 0) {
    Rect r(x0, y0, x1 - x0, y1 - y0);
    JXL_CHECK_IMAGE_INITIALIZED(*plane_in, r);
    CopyImageTo(r, *plane_in, r, plane_out);
    return;
  }
  // Prepare padding if we are on a border.
  // Copy the whole row/column here: it is likely similarly fast and ensures
  // that we don't forget some parts of padding.
  if (frame_rect.x0() == 0) {
    for (size_t y = 0; y < plane_in->ysize(); y++) {
      plane_in->Row(y)[rect.x0() - 1] = plane_in->Row(y)[rect.x0()];
    }
  }
  if (frame_rect.x0() + x1 - rect.x0() >= frame_dim.xsize_padded) {
    ssize_t borderx = static_cast<ssize_t>(x1 - xoff + hs) / (1 << hs) + xoff;
    for (size_t y = 0; y < plane_in->ysize(); y++) {
      plane_in->Row(y)[borderx] = plane_in->Row(y)[borderx - 1];
    }
  }
  if (frame_rect.y0() == 0) {
    memcpy(plane_in->Row(rect.y0() - 1), plane_in->Row(rect.y0()),
           plane_in->xsize() * sizeof(float));
  }
  if (frame_rect.y0() + y1 - rect.y0() >= frame_dim.ysize_padded) {
    ssize_t bordery = static_cast<ssize_t>(y1 - yoff + vs) / (1 << vs) + yoff;
    memcpy(plane_in->Row(bordery), plane_in->Row(bordery - 1),
           plane_in->xsize() * sizeof(float));
  }
  if (hs == 1) {
    // Limited to 4 for Interleave*.
    HWY_CAPPED(float, 4) d;
    auto threefour = Set(d, 0.75f);
    auto onefour = Set(d, 0.25f);
    size_t orig_y0 = y0;
    size_t orig_y1 = y1;
    if (vs != 0) {
      orig_y0 = (y0 >> 1) + (yoff >> 1) - 1;
      orig_y1 = (y1 >> 1) + (yoff >> 1) + 1;
    }
    for (size_t y = orig_y0; y < orig_y1; y++) {
      const float* in = plane_in->Row(y);
      float* out = temp->Row(y);
      for (size_t x = x0 / (2 * Lanes(d)) * 2 * Lanes(d);
           x < RoundUpTo(x1, 2 * Lanes(d)); x += 2 * Lanes(d)) {
        size_t ox = (x >> 1) + (xoff >> 1);
        auto current = Load(d, in + ox) * threefour;
        auto prev = LoadU(d, in + ox - 1);
        auto next = LoadU(d, in + ox + 1);
        auto left = MulAdd(onefour, prev, current);
        auto right = MulAdd(onefour, next, current);
#if HWY_TARGET == HWY_SCALAR
        Store(left, d, out + x);
        Store(right, d, out + x + 1);
#else
        Store(InterleaveLower(d, left, right), d, out + x);
        Store(InterleaveUpper(d, left, right), d, out + x + Lanes(d));
#endif
      }
    }
  } else {
    CopyImageTo(*plane_in, temp);
  }
  if (vs == 1) {
    HWY_FULL(float) d;
    auto threefour = Set(d, 0.75f);
    auto onefour = Set(d, 0.25f);
    for (size_t y = y0; y < y1; y++) {
      size_t oy1 = (y >> 1) + (yoff >> 1);
      if ((y & 1) == 1) oy1++;
      size_t oy0 = oy1 - 1;
      const float* in0 = temp->Row(oy0);
      const float* in1 = temp->Row(oy1);
      float* out = plane_out->Row(y);
      if ((y & 1) == 1) {
        for (size_t x = x0 / Lanes(d) * Lanes(d); x < RoundUpTo(x1, Lanes(d));
             x += Lanes(d)) {
          auto i0 = Load(d, in0 + x);
          auto i1 = Load(d, in1 + x);
          auto o = MulAdd(i0, threefour, i1 * onefour);
          Store(o, d, out + x);
        }
      } else {
        for (size_t x = x0 / Lanes(d) * Lanes(d); x < RoundUpTo(x1, Lanes(d));
             x += Lanes(d)) {
          auto i0 = Load(d, in0 + x);
          auto i1 = Load(d, in1 + x);
          auto o = MulAdd(i0, onefour, i1 * threefour);
          Store(o, d, out + x);
        }
      }
    }
  } else {
    CopyImageTo(*temp, plane_out);
  }

  // The output must be initialized including the lf.Padding() around the image
  // for all the pixels that fall inside the image frame.
  JXL_CHECK_IMAGE_INITIALIZED(*plane_out, Rect(x0, y0, x1 - x0, y1 - y0));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(UndoXYBInPlace);
HWY_EXPORT(FloatToRGBA8);
HWY_EXPORT(DoYCbCrUpsampling);

void UndoXYB(const Image3F& src, Image3F* dst,
             const OutputEncodingInfo& output_info, ThreadPool* pool) {
  CopyImageTo(src, dst);
  JXL_CHECK(RunOnPool(
      pool, 0, src.ysize(), ThreadPool::NoInit,
      [&](const uint32_t y, size_t /*thread*/) {
        JXL_CHECK(HWY_DYNAMIC_DISPATCH(UndoXYBInPlace)(dst, Rect(*dst).Line(y),
                                                       output_info));
      },
      "UndoXYB"));
}

namespace {
Rect ScaleRectForEC(Rect in, const FrameHeader& frame_header, size_t ec) {
  auto s = [&](size_t x) {
    return DivCeil(x * frame_header.upsampling,
                   frame_header.extra_channel_upsampling[ec]);
  };
  // For x0 and y0 the DivCeil is actually an exact division.
  return Rect(s(in.x0()), s(in.y0()), s(in.xsize()), s(in.ysize()));
}

// Implements EnsurePaddingInPlace, but allows processing data one row at a
// time.
class EnsurePaddingInPlaceRowByRow {
  void Init(const Rect& rect, const Rect& image_rect, size_t image_xsize,
            size_t image_ysize, size_t xpadding, size_t ypadding, ssize_t* y0,
            ssize_t* y1) {
    // coordinates relative to rect.
    JXL_ASSERT(SameSize(rect, image_rect));
    JXL_ASSERT(image_rect.x0() + image_rect.xsize() <= image_xsize);
    JXL_ASSERT(image_rect.y0() + image_rect.ysize() <= image_ysize);
    *y0 = -std::min(image_rect.y0(), ypadding);
    *y1 = rect.ysize() + std::min(ypadding, image_ysize - image_rect.ysize() -
                                                image_rect.y0());
    if (image_rect.x0() >= xpadding &&
        image_rect.x0() + image_rect.xsize() + xpadding <= image_xsize) {
      // Nothing to do.
      strategy_ = kSkip;
    } else if (image_xsize >= 2 * xpadding) {
      strategy_ = kFast;
    } else {
      strategy_ = kSlow;
    }
    y0_ = rect.y0();
    JXL_ASSERT(rect.x0() >= xpadding);
    x0_ = x1_ = rect.x0() - xpadding;
    // If close to the left border - do mirroring.
    if (image_rect.x0() < xpadding) x1_ = rect.x0() - image_rect.x0();
    x2_ = x3_ = rect.x0() + rect.xsize() + xpadding;
    // If close to the right border - do mirroring.
    if (image_rect.x0() + image_rect.xsize() + xpadding > image_xsize) {
      x2_ = rect.x0() + image_xsize - image_rect.x0();
    }
    JXL_ASSERT(x0_ <= x1_);
    JXL_ASSERT(x1_ <= x2_);
    JXL_ASSERT(x2_ <= x3_);
    JXL_ASSERT(image_xsize == (x2_ - x1_) ||
               (x1_ - x0_ <= x2_ - x1_ && x3_ - x2_ <= x2_ - x1_));
  }

 public:
  void Init(Image3F* img, const Rect& rect, const Rect& image_rect,
            size_t image_xsize, size_t image_ysize, size_t xpadding,
            size_t ypadding, ssize_t* y0, ssize_t* y1) {
    Init(rect, image_rect, image_xsize, image_ysize, xpadding, ypadding, y0,
         y1);
    img3_ = img;
    JXL_DASSERT(x3_ <= img->xsize());
  }
  void Init(ImageF* img, const Rect& rect, const Rect& image_rect,
            size_t image_xsize, size_t image_ysize, size_t xpadding,
            size_t ypadding, ssize_t* y0, ssize_t* y1) {
    Init(rect, image_rect, image_xsize, image_ysize, xpadding, ypadding, y0,
         y1);
    img_ = img;
    JXL_DASSERT(x3_ <= img->xsize());
  }
  // To be called when row `y` of the input is available, for all the values in
  // [*y0, *y1).
  void Process3(ssize_t y) {
    JXL_DASSERT(img3_);
    for (size_t c = 0; c < 3; c++) {
      img_ = &img3_->Plane(c);
      Process(y);
    }
  }
  void Process(ssize_t y) {
    JXL_DASSERT(img_);
    switch (strategy_) {
      case kSkip:
        break;
      case kFast: {
        // Image is wide enough that a single Mirror() step is sufficient.
        float* JXL_RESTRICT row = img_->Row(y + y0_);
        for (size_t x = x0_; x < x1_; x++) {
          row[x] = row[2 * x1_ - x - 1];
        }
        for (size_t x = x2_; x < x3_; x++) {
          row[x] = row[2 * x2_ - x - 1];
        }
        break;
      }
      case kSlow: {
        // Slow case for small images.
        float* JXL_RESTRICT row = img_->Row(y + y0_) + x1_;
        for (ssize_t x = x0_ - x1_; x < 0; x++) {
          *(row + x) = row[Mirror(x, x2_ - x1_)];
        }
        for (size_t x = x2_ - x1_; x < x3_ - x1_; x++) {
          *(row + x) = row[Mirror(x, x2_ - x1_)];
        }
        break;
      }
    }
  }

 private:
  // Initialized to silence spurious compiler warnings.
  Image3F* img3_ = nullptr;
  ImageF* img_ = nullptr;
  // Will fill [x0_, x1_) and [x2_, x3_) on every row.
  // The [x1_, x2_) range contains valid image pixels. We guarantee that either
  // x1_ - x0_ <= x2_ - x1_, (and similarly for x2_, x3_), or that the [x1_,
  // x2_) contains a full horizontal line of the original image.
  size_t x0_ = 0, x1_ = 0, x2_ = 0, x3_ = 0;
  size_t y0_ = 0;
  // kSlow: use calls to Mirror(), for the case where the border might be larger
  // than the image.
  // kFast: directly use the result of Mirror() when it can be computed in a
  // single iteration.
  // kSkip: do nothing.
  enum Strategy { kFast, kSlow, kSkip };
  Strategy strategy_ = kSkip;
};
}  // namespace

void EnsurePaddingInPlace(Image3F* img, const Rect& rect,
                          const Rect& image_rect, size_t image_xsize,
                          size_t image_ysize, size_t xpadding,
                          size_t ypadding) {
  ssize_t y0, y1;
  EnsurePaddingInPlaceRowByRow impl;
  impl.Init(img, rect, image_rect, image_xsize, image_ysize, xpadding, ypadding,
            &y0, &y1);
  for (ssize_t y = y0; y < y1; y++) {
    impl.Process3(y);
  }
}

void EnsurePaddingInPlace(ImageF* img, const Rect& rect, const Rect& image_rect,
                          size_t image_xsize, size_t image_ysize,
                          size_t xpadding, size_t ypadding) {
  ssize_t y0, y1;
  EnsurePaddingInPlaceRowByRow impl;
  impl.Init(img, rect, image_rect, image_xsize, image_ysize, xpadding, ypadding,
            &y0, &y1);
  for (ssize_t y = y0; y < y1; y++) {
    impl.Process(y);
  }
}

Status FinalizeImageRect(
    Image3F* input_image, const Rect& input_rect,
    const std::vector<std::pair<ImageF*, Rect>>& extra_channels,
    PassesDecoderState* dec_state, size_t thread,
    ImageBundle* JXL_RESTRICT output_image, const Rect& frame_rect) {
  // Do nothing if using the rendering pipeline.
  if (dec_state->render_pipeline) {
    return true;
  }
  const ImageFeatures& image_features = dec_state->shared->image_features;
  const FrameHeader& frame_header = dec_state->shared->frame_header;
  const ImageMetadata& metadata = frame_header.nonserialized_metadata->m;
  const LoopFilter& lf = frame_header.loop_filter;
  const FrameDimensions& frame_dim = dec_state->shared->frame_dim;
  JXL_DASSERT(frame_rect.xsize() <= kApplyImageFeaturesTileDim);
  JXL_DASSERT(frame_rect.ysize() <= kApplyImageFeaturesTileDim);
  JXL_DASSERT(input_rect.xsize() == frame_rect.xsize());
  JXL_DASSERT(input_rect.ysize() == frame_rect.ysize());
  JXL_DASSERT(frame_rect.x0() % GroupBorderAssigner::kPaddingXRound == 0);
  JXL_DASSERT(frame_rect.xsize() % GroupBorderAssigner::kPaddingXRound == 0 ||
              frame_rect.xsize() + frame_rect.x0() == frame_dim.xsize ||
              frame_rect.xsize() + frame_rect.x0() == frame_dim.xsize_padded);

  // +----------------------------- STEP 1 ------------------------------+
  // | Compute the rects on which patches and splines will be applied.   |
  // | In case we are applying upsampling, we need to apply patches on a |
  // | slightly larger image.                                            |
  // +-------------------------------------------------------------------+

  // If we are applying upsampling, we need 2 more pixels around the actual rect
  // for border. Thus, we also need to apply patches and splines to those
  // pixels. We compute here
  // - The portion of image that corresponds to the area we are applying IF.
  //   (rect_for_if)
  // - The rect where that pixel data is stored in upsampling_input_storage.
  //   (rect_for_if_storage)
  // - The rect where the pixel data that we need to upsample is stored.
  //   (rect_for_upsampling)
  // - The source rect for the pixel data in `input_image`. It is assumed that,
  //   if `frame_rect` is not on an image border, `input_image:input_rect` has
  //   enough border available. (rect_for_if_input)

  Image3F* output_color =
      dec_state->rgb_output == nullptr && dec_state->pixel_callback == nullptr
          ? output_image->color()
          : nullptr;

  Image3F* storage_for_if = output_color;
  Rect rect_for_if = frame_rect;
  Rect rect_for_if_storage = frame_rect;
  Rect rect_for_upsampling = frame_rect;
  Rect rect_for_if_input = input_rect;
  // The same as rect_for_if_input but in the frame coordinates.
  Rect frame_rect_for_ycbcr_upsampling = frame_rect;
  size_t extra_rows_t = 0;
  size_t extra_rows_b = 0;
  if (frame_header.upsampling != 1) {
    size_t ifbx0 = 0;
    size_t ifbx1 = 0;
    size_t ifby0 = 0;
    size_t ifby1 = 0;
    if (frame_rect.x0() >= 2) {
      JXL_DASSERT(input_rect.x0() >= 2);
      ifbx0 = 2;
    }
    if (frame_rect.y0() >= 2) {
      JXL_DASSERT(input_rect.y0() >= 2);
      extra_rows_t = ifby0 = 2;
    }
    for (size_t extra : {1, 2}) {
      if (frame_rect.x0() + frame_rect.xsize() + extra <=
          dec_state->shared->frame_dim.xsize_padded) {
        JXL_DASSERT(input_rect.x0() + input_rect.xsize() + extra <=
                    input_image->xsize());
        ifbx1 = extra;
      }
      if (frame_rect.y0() + frame_rect.ysize() + extra <=
          dec_state->shared->frame_dim.ysize_padded) {
        JXL_DASSERT(input_rect.y0() + input_rect.ysize() + extra <=
                    input_image->ysize());
        extra_rows_b = ifby1 = extra;
      }
    }
    rect_for_if = Rect(frame_rect.x0() - ifbx0, frame_rect.y0() - ifby0,
                       frame_rect.xsize() + ifbx0 + ifbx1,
                       frame_rect.ysize() + ifby0 + ifby1);
    // Storage for pixel data does not necessarily start at (0, 0) as we need to
    // have the left border of upsampling_rect aligned to a multiple of
    // GroupBorderAssigner::kPaddingXRound.
    rect_for_if_storage =
        Rect(kBlockDim + RoundUpTo(ifbx0, GroupBorderAssigner::kPaddingXRound) -
                 ifbx0,
             kBlockDim, rect_for_if.xsize(), rect_for_if.ysize());
    rect_for_upsampling =
        Rect(kBlockDim + RoundUpTo(ifbx0, GroupBorderAssigner::kPaddingXRound),
             kBlockDim + ifby0, frame_rect.xsize(), frame_rect.ysize());
    rect_for_if_input =
        Rect(input_rect.x0() - ifbx0, input_rect.y0() - ifby0,
             rect_for_if_storage.xsize(), rect_for_if_storage.ysize());
    frame_rect_for_ycbcr_upsampling =
        Rect(frame_rect.x0() - ifbx0, frame_rect.y0() - ifby0,
             rect_for_if_input.xsize(), rect_for_if_input.ysize());
    storage_for_if = &dec_state->upsampling_input_storage[thread];
  }

  // +--------------------------- STEP 1.5 ------------------------------+
  // | Perform YCbCr upsampling if needed.                               |
  // +-------------------------------------------------------------------+

  Image3F* input = input_image;
  if (!frame_header.chroma_subsampling.Is444()) {
    for (size_t c = 0; c < 3; c++) {
      size_t vs = frame_header.chroma_subsampling.VShift(c);
      size_t hs = frame_header.chroma_subsampling.HShift(c);
      // The per-thread output is used for the first time here. Poison the temp
      // image on this thread to prevent leaking initialized data from a
      // previous run in this thread in msan builds.
      msan::PoisonImage(dec_state->ycbcr_out_images[thread].Plane(c));
      HWY_DYNAMIC_DISPATCH(DoYCbCrUpsampling)
      (hs, vs, &input_image->Plane(c), rect_for_if_input,
       frame_rect_for_ycbcr_upsampling, frame_dim,
       &dec_state->ycbcr_out_images[thread].Plane(c), lf,
       &dec_state->ycbcr_temp_images[thread]);
    }
    input = &dec_state->ycbcr_out_images[thread];
  }

  // Variables for upsampling and filtering.
  Rect upsampled_frame_rect(frame_rect.x0() * frame_header.upsampling,
                            frame_rect.y0() * frame_header.upsampling,
                            frame_rect.xsize() * frame_header.upsampling,
                            frame_rect.ysize() * frame_header.upsampling);
  Rect full_frame_rect(0, 0, frame_dim.xsize_upsampled,
                       frame_dim.ysize_upsampled);
  upsampled_frame_rect = upsampled_frame_rect.Crop(full_frame_rect);
  EnsurePaddingInPlaceRowByRow ensure_padding_upsampling;
  ssize_t ensure_padding_upsampling_y0 = 0;
  ssize_t ensure_padding_upsampling_y1 = 0;

  EnsurePaddingInPlaceRowByRow ensure_padding_filter;
  FilterPipeline* fp = nullptr;
  ssize_t ensure_padding_filter_y0 = 0;
  ssize_t ensure_padding_filter_y1 = 0;
  if (lf.epf_iters != 0 || lf.gab) {
    fp = &dec_state->filter_pipelines[thread];
  }

  // +----------------------------- STEP 2 ------------------------------+
  // | Change rects and buffer to not use `output_image` if direct       |
  // | output to rgb8 is requested.                                      |
  // +-------------------------------------------------------------------+
  Image3F* output_pixel_data_storage = output_color;
  Rect upsampled_frame_rect_for_storage = upsampled_frame_rect;
  if (dec_state->rgb_output || dec_state->pixel_callback) {
    size_t log2_upsampling = CeilLog2Nonzero(frame_header.upsampling);
    if (storage_for_if == output_color) {
      storage_for_if =
          &dec_state->output_pixel_data_storage[log2_upsampling][thread];
      rect_for_if_storage =
          Rect(0, 0, rect_for_if_storage.xsize(), rect_for_if_storage.ysize());
    }
    output_pixel_data_storage =
        &dec_state->output_pixel_data_storage[log2_upsampling][thread];
    upsampled_frame_rect_for_storage =
        Rect(0, 0, upsampled_frame_rect.xsize(), upsampled_frame_rect.ysize());
    if (frame_header.upsampling == 1 && fp == nullptr) {
      upsampled_frame_rect_for_storage = rect_for_if_storage =
          rect_for_if_input;
      output_pixel_data_storage = storage_for_if = input;
    }
  }
  // Set up alpha channel.
  const size_t ec =
      metadata.Find(ExtraChannel::kAlpha) - metadata.extra_channel_info.data();
  const ImageF* alpha = nullptr;
  Rect alpha_rect = upsampled_frame_rect;
  if (ec < metadata.extra_channel_info.size()) {
    JXL_ASSERT(ec < extra_channels.size());
    if (frame_header.extra_channel_upsampling[ec] == 1) {
      alpha = extra_channels[ec].first;
      alpha_rect = extra_channels[ec].second;
    } else {
      alpha = &output_image->extra_channels()[ec];
      alpha_rect = upsampled_frame_rect;
    }
  }

  // +----------------------------- STEP 3 ------------------------------+
  // | Set up upsampling and upsample extra channels.                    |
  // +-------------------------------------------------------------------+
  Upsampler* color_upsampler = nullptr;
  if (frame_header.upsampling != 1) {
    color_upsampler =
        &dec_state->upsamplers[CeilLog2Nonzero(frame_header.upsampling) - 1];
    ensure_padding_upsampling.Init(
        storage_for_if, rect_for_upsampling, frame_rect, frame_dim.xsize_padded,
        frame_dim.ysize_padded, 2, 2, &ensure_padding_upsampling_y0,
        &ensure_padding_upsampling_y1);
  }

  std::vector<std::pair<ImageF*, Rect>> extra_channels_for_patches;
  std::vector<EnsurePaddingInPlaceRowByRow> ec_padding;

  bool late_ec_upsample = frame_header.upsampling != 1;
  for (auto ecups : frame_header.extra_channel_upsampling) {
    if (ecups != frame_header.upsampling) {
      // If patches are applied, either frame_header.upsampling == 1 or
      // late_ec_upsample is true.
      late_ec_upsample = false;
    }
  }

  ssize_t ensure_padding_upsampling_ec_y0 = 0;
  ssize_t ensure_padding_upsampling_ec_y1 = 0;

  // TODO(veluca) do not upsample extra channels to a full-image-sized buffer if
  // we are not outputting to an ImageBundle.
  if (!late_ec_upsample) {
    // Upsample extra channels first if not all channels have the same
    // upsampling factor.
    for (size_t ec = 0; ec < extra_channels.size(); ec++) {
      size_t ecups = frame_header.extra_channel_upsampling[ec];
      if (ecups == 1) {
        extra_channels_for_patches.push_back(extra_channels[ec]);
        continue;
      }
      ssize_t ensure_padding_y0, ensure_padding_y1;
      EnsurePaddingInPlaceRowByRow ensure_padding;
      // frame_rect can go up to frame_dim.xsize_padded, in VarDCT mode.
      Rect ec_image_rect = ScaleRectForEC(
          frame_rect.Crop(frame_dim.xsize, frame_dim.ysize), frame_header, ec);
      size_t ecxs = DivCeil(frame_dim.xsize_upsampled,
                            frame_header.extra_channel_upsampling[ec]);
      size_t ecys = DivCeil(frame_dim.ysize_upsampled,
                            frame_header.extra_channel_upsampling[ec]);
      ensure_padding.Init(extra_channels[ec].first, extra_channels[ec].second,
                          ec_image_rect, ecxs, ecys, 2, 2, &ensure_padding_y0,
                          &ensure_padding_y1);
      for (ssize_t y = ensure_padding_y0; y < ensure_padding_y1; y++) {
        ensure_padding.Process(y);
      }
      Upsampler& upsampler =
          dec_state->upsamplers[CeilLog2Nonzero(
                                    frame_header.extra_channel_upsampling[ec]) -
                                1];
      upsampler.UpsampleRect(
          *extra_channels[ec].first, extra_channels[ec].second,
          &output_image->extra_channels()[ec], upsampled_frame_rect,
          static_cast<ssize_t>(ec_image_rect.y0()) -
              static_cast<ssize_t>(extra_channels[ec].second.y0()),
          ecys, dec_state->upsampler_storage[thread].get());
      extra_channels_for_patches.emplace_back(
          &output_image->extra_channels()[ec], upsampled_frame_rect);
    }
  } else {
    // Upsample extra channels last if color channels are upsampled and all the
    // extra channels have the same upsampling as them.
    ec_padding.resize(extra_channels.size());
    for (size_t ec = 0; ec < extra_channels.size(); ec++) {
      // Add a border to the extra channel rect for when patches are applied.
      // This ensures that the correct row is accessed (y values for patches are
      // relative to rect_for_if, not to input_rect).
      // As the rect is extended by 0 or 2 pixels, and the patches input has,
      // accordingly, the same padding, this is safe.
      Rect r(extra_channels[ec].second.x0() + rect_for_upsampling.x0() -
                 rect_for_if_storage.x0(),
             extra_channels[ec].second.y0() + rect_for_upsampling.y0() -
                 rect_for_if_storage.y0(),
             extra_channels[ec].second.xsize() + rect_for_if_storage.xsize() -
                 rect_for_upsampling.xsize(),
             extra_channels[ec].second.ysize() + rect_for_if_storage.ysize() -
                 rect_for_upsampling.ysize());
      extra_channels_for_patches.emplace_back(extra_channels[ec].first, r);
      // frame_rect can go up to frame_dim.xsize_padded, in VarDCT mode.
      ec_padding[ec].Init(extra_channels[ec].first, extra_channels[ec].second,
                          frame_rect.Crop(frame_dim.xsize, frame_dim.ysize),
                          frame_dim.xsize, frame_dim.ysize, 2, 2,
                          &ensure_padding_upsampling_ec_y0,
                          &ensure_padding_upsampling_ec_y1);
    }
  }

  // Initialized to a valid non-null ptr to avoid UB if arithmetic is done with
  // the pointer value (which would then not be used).
  std::vector<float*> ec_ptrs_for_patches(extra_channels.size(),
                                          input->PlaneRow(0, 0));

  // +----------------------------- STEP 4 ------------------------------+
  // | Set up the filter pipeline.                                       |
  // +-------------------------------------------------------------------+
  if (fp) {
    ensure_padding_filter.Init(
        input, rect_for_if_input, rect_for_if, frame_dim.xsize_padded,
        frame_dim.ysize_padded, lf.Padding(), lf.Padding(),
        &ensure_padding_filter_y0, &ensure_padding_filter_y1);

    fp = PrepareFilterPipeline(dec_state, rect_for_if, *input,
                               rect_for_if_input, frame_dim.ysize_padded,
                               thread, storage_for_if, rect_for_if_storage);
  }

  // +----------------------------- STEP 5 ------------------------------+
  // | Run the prepared pipeline of operations.                          |
  // +-------------------------------------------------------------------+

  // y values are relative to rect_for_if.
  // Automatic mirroring in fp->ApplyFiltersRow() implies that we should ensure
  // that padding for the first lines of the image is already present before
  // calling ApplyFiltersRow() with "virtual" rows.
  // Here we rely on the fact that virtual rows at the beginning of the image
  // are only present if input_rect.y0() == 0.
  ssize_t first_ensure_padding_y = ensure_padding_filter_y0;
  if (frame_rect.y0() == 0) {
    JXL_DASSERT(ensure_padding_filter_y0 == 0);
    first_ensure_padding_y =
        std::min<ssize_t>(lf.Padding(), ensure_padding_filter_y1);
    for (ssize_t y = 0; y < first_ensure_padding_y; y++) {
      ensure_padding_filter.Process3(y);
    }
  }

  for (ssize_t y = -lf.Padding();
       y < static_cast<ssize_t>(lf.Padding() + rect_for_if.ysize()); y++) {
    if (fp) {
      if (y >= first_ensure_padding_y && y < ensure_padding_filter_y1) {
        ensure_padding_filter.Process3(y);
      }
      fp->ApplyFiltersRow(lf, dec_state->filter_weights, y);
    } else if (output_pixel_data_storage != input) {
      for (size_t c = 0; c < 3; c++) {
        memcpy(rect_for_if_storage.PlaneRow(storage_for_if, c, y),
               rect_for_if_input.ConstPlaneRow(*input, c, y),
               rect_for_if_input.xsize() * sizeof(float));
      }
    }
    if (y < static_cast<ssize_t>(lf.Padding())) continue;
    // At this point, row `y - lf.Padding()` of `rect_for_if` has been produced
    // by the filters.
    ssize_t available_y = y - lf.Padding();
    if (frame_header.upsampling == 1) {
      for (size_t i = 0; i < extra_channels.size(); i++) {
        ec_ptrs_for_patches[i] = extra_channels_for_patches[i].second.Row(
            extra_channels_for_patches[i].first, available_y);
      }
    }
    image_features.patches.AddTo(
        storage_for_if, rect_for_if_storage.Line(available_y),
        ec_ptrs_for_patches.data(), rect_for_if.Line(available_y));
    image_features.splines.AddTo(storage_for_if,
                                 rect_for_if_storage.Line(available_y),
                                 rect_for_if.Line(available_y));
    size_t num_ys = 1;
    if (frame_header.upsampling != 1) {
      // Upsampling `y` values are relative to `rect_for_upsampling`, not to
      // `rect_for_if`.
      ssize_t shifted_y = available_y - extra_rows_t;
      if (shifted_y >= ensure_padding_upsampling_y0 &&
          shifted_y < ensure_padding_upsampling_y1) {
        ensure_padding_upsampling.Process3(shifted_y);
      }
      if (late_ec_upsample && shifted_y >= ensure_padding_upsampling_ec_y0 &&
          shifted_y < ensure_padding_upsampling_ec_y1) {
        for (size_t ec = 0; ec < extra_channels.size(); ec++) {
          ec_padding[ec].Process(shifted_y);
        }
      }
      // Upsampling will access two rows of border, so the first upsampling
      // output will be available after shifted_y is at least 2, *unless* image
      // height is <= 2.
      if (shifted_y < 2 &&
          shifted_y + 1 != static_cast<ssize_t>(frame_rect.ysize())) {
        continue;
      }
      // Value relative to upsampled_frame_rect.
      size_t input_y = std::max<ssize_t>(shifted_y - 2, 0);
      size_t upsampled_available_y = frame_header.upsampling * input_y;
      size_t num_input_rows = 1;
      // If we are going to mirror the last output rows, then we already have 3
      // input lines ready. This happens iff we did not extend rect_for_if on
      // the bottom *and* we are at the last `y` value.
      if (extra_rows_b != 2 &&
          static_cast<size_t>(y) + 1 == lf.Padding() + rect_for_if.ysize()) {
        num_input_rows = 3;
      }
      num_input_rows = std::min(num_input_rows, frame_dim.ysize_padded);
      num_ys = num_input_rows * frame_header.upsampling;

      if (static_cast<size_t>(upsampled_available_y) >=
          upsampled_frame_rect.ysize()) {
        continue;
      }

      if (upsampled_available_y + num_ys >= upsampled_frame_rect.ysize()) {
        num_ys = upsampled_frame_rect.ysize() - upsampled_available_y;
      }

      // Upsampler takes care of mirroring, and checks "physical" boundaries.
      Rect upsample_input_rect = rect_for_upsampling.Lines(input_y, 1);
      color_upsampler->UpsampleRect(
          *storage_for_if, upsample_input_rect, output_pixel_data_storage,
          upsampled_frame_rect_for_storage.Lines(upsampled_available_y, num_ys),
          static_cast<ssize_t>(frame_rect.y0()) -
              static_cast<ssize_t>(rect_for_upsampling.y0()),
          frame_dim.ysize_padded, dec_state->upsampler_storage[thread].get());
      if (late_ec_upsample) {
        for (size_t ec = 0; ec < extra_channels.size(); ec++) {
          // Upsampler takes care of mirroring, and checks "physical"
          // boundaries.
          Rect upsample_ec_input_rect =
              extra_channels[ec].second.Lines(input_y, 1);
          color_upsampler->UpsampleRect(
              *extra_channels[ec].first, upsample_ec_input_rect,
              &output_image->extra_channels()[ec],
              upsampled_frame_rect.Lines(upsampled_available_y, num_ys),
              static_cast<ssize_t>(frame_rect.y0()) -
                  static_cast<ssize_t>(extra_channels[ec].second.y0()),
              frame_dim.ysize, dec_state->upsampler_storage[thread].get());
        }
      }
      available_y = upsampled_available_y;
    }

    if (static_cast<size_t>(available_y) >= upsampled_frame_rect.ysize()) {
      continue;
    }

    // The image data is now unconditionally in
    // `output_image_storage:upsampled_frame_rect_for_storage`.
    if (frame_header.flags & FrameHeader::kNoise) {
      PROFILER_ZONE("AddNoise");
      AddNoise(image_features.noise_params,
               upsampled_frame_rect.Lines(available_y, num_ys),
               dec_state->noise,
               upsampled_frame_rect_for_storage.Lines(available_y, num_ys),
               dec_state->shared_storage.cmap, output_pixel_data_storage);
    }

    if (dec_state->pre_color_transform_frame.xsize() != 0) {
      for (size_t c = 0; c < 3; c++) {
        for (size_t y = available_y; y < available_y + num_ys; y++) {
          float* JXL_RESTRICT row_out = upsampled_frame_rect.PlaneRow(
              &dec_state->pre_color_transform_frame, c, y);
          const float* JXL_RESTRICT row_in =
              upsampled_frame_rect_for_storage.ConstPlaneRow(
                  *output_pixel_data_storage, c, y);
          memcpy(row_out, row_in,
                 upsampled_frame_rect.xsize() * sizeof(*row_in));
        }
      }
    }

    // We skip the color transform entirely if save_before_color_transform and
    // the frame is not supposed to be displayed.

    if (dec_state->fast_xyb_srgb8_conversion) {
      FastXYBTosRGB8(
          *output_pixel_data_storage,
          upsampled_frame_rect_for_storage.Lines(available_y, num_ys),
          upsampled_frame_rect.Lines(available_y, num_ys)
              .Crop(Rect(0, 0, frame_dim.xsize_upsampled,
                         frame_dim.ysize_upsampled)),
          alpha, alpha_rect.Lines(available_y, num_ys),
          dec_state->rgb_output_is_rgba, dec_state->rgb_output, frame_dim.xsize,
          dec_state->rgb_stride);
    } else {
      if (frame_header.needs_color_transform()) {
        if (frame_header.color_transform == ColorTransform::kXYB) {
          JXL_RETURN_IF_ERROR(HWY_DYNAMIC_DISPATCH(UndoXYBInPlace)(
              output_pixel_data_storage,
              upsampled_frame_rect_for_storage.Lines(available_y, num_ys),
              dec_state->output_encoding_info));
        } else if (frame_header.color_transform == ColorTransform::kYCbCr) {
          YcbcrToRgb(
              *output_pixel_data_storage, output_pixel_data_storage,
              upsampled_frame_rect_for_storage.Lines(available_y, num_ys));
        }
      }

      // TODO(veluca): all blending should happen here.

      if (dec_state->rgb_output != nullptr) {
        HWY_DYNAMIC_DISPATCH(FloatToRGBA8)
        (*output_pixel_data_storage,
         upsampled_frame_rect_for_storage.Lines(available_y, num_ys),
         dec_state->rgb_output_is_rgba, alpha,
         alpha_rect.Lines(available_y, num_ys),
         upsampled_frame_rect.Lines(available_y, num_ys)
             .Crop(Rect(0, 0, frame_dim.xsize_upsampled,
                        frame_dim.ysize_upsampled)),
         dec_state->rgb_output, dec_state->rgb_stride);
      }
      if (dec_state->pixel_callback != nullptr) {
        Rect alpha_line_rect = alpha_rect.Lines(available_y, num_ys);
        Rect color_input_line_rect =
            upsampled_frame_rect_for_storage.Lines(available_y, num_ys);
        Rect image_line_rect = upsampled_frame_rect.Lines(available_y, num_ys)
                                   .Crop(Rect(0, 0, frame_dim.xsize_upsampled,
                                              frame_dim.ysize_upsampled));
        const float* line_buffers[4];
        for (size_t iy = 0; iy < image_line_rect.ysize(); iy++) {
          for (size_t c = 0; c < 3; c++) {
            line_buffers[c] = color_input_line_rect.ConstPlaneRow(
                *output_pixel_data_storage, c, iy);
          }
          if (alpha) {
            line_buffers[3] = alpha_line_rect.ConstRow(*alpha, iy);
          } else {
            line_buffers[3] = dec_state->opaque_alpha.data();
          }
          std::vector<float>& interleaved =
              dec_state->pixel_callback_rows[thread];
          size_t j = 0;
          for (size_t i = 0; i < image_line_rect.xsize(); i++) {
            interleaved[j++] = line_buffers[0][i];
            interleaved[j++] = line_buffers[1][i];
            interleaved[j++] = line_buffers[2][i];
            if (dec_state->rgb_output_is_rgba) {
              interleaved[j++] = line_buffers[3][i];
            }
          }
          dec_state->pixel_callback(interleaved.data(), image_line_rect.x0(),
                                    image_line_rect.y0() + iy,
                                    image_line_rect.xsize());
        }
      }
    }
  }

  return true;
}

Status FinalizeFrameDecoding(ImageBundle* decoded,
                             PassesDecoderState* dec_state, ThreadPool* pool,
                             bool force_fir, bool skip_blending, bool move_ec) {
  if (dec_state->render_pipeline) {
    return true;
  }
  const FrameHeader& frame_header = dec_state->shared->frame_header;
  const FrameDimensions& frame_dim = dec_state->shared->frame_dim;

  // FinalizeImageRect was not yet run, or we are forcing a run.
  if (!dec_state->EagerFinalizeImageRect() || force_fir) {
    std::vector<Rect> rects_to_process;
    for (size_t y = 0; y < frame_dim.ysize_padded; y += kGroupDim) {
      for (size_t x = 0; x < frame_dim.xsize_padded; x += kGroupDim) {
        Rect rect(x, y, kGroupDim, kGroupDim, frame_dim.xsize_padded,
                  frame_dim.ysize_padded);
        if (rect.xsize() == 0 || rect.ysize() == 0) continue;
        rects_to_process.push_back(rect);
      }
    }
    const auto allocate_storage = [&](const size_t num_threads) {
      dec_state->EnsureStorage(num_threads);
      return true;
    };

    {
      std::vector<ImageF> ecs;
      const ImageMetadata& metadata = frame_header.nonserialized_metadata->m;
      for (size_t i = 0; i < metadata.num_extra_channels; i++) {
        if (frame_header.extra_channel_upsampling[i] == 1) {
          if (move_ec) {
            ecs.push_back(std::move(dec_state->extra_channels[i]));
          } else {
            ecs.push_back(CopyImage(dec_state->extra_channels[i]));
          }
        } else {
          ecs.emplace_back(frame_dim.xsize_upsampled_padded,
                           frame_dim.ysize_upsampled_padded);
        }
      }
      decoded->SetExtraChannels(std::move(ecs));
    }

    std::atomic<bool> apply_features_ok{true};
    auto run_apply_features = [&](const uint32_t rect_id, size_t thread) {
      size_t xstart = PassesDecoderState::kGroupDataXBorder;
      size_t ystart = PassesDecoderState::kGroupDataYBorder;
      for (size_t c = 0; c < 3; c++) {
        Rect rh(rects_to_process[rect_id].x0() >>
                    frame_header.chroma_subsampling.HShift(c),
                rects_to_process[rect_id].y0() >>
                    frame_header.chroma_subsampling.VShift(c),
                rects_to_process[rect_id].xsize() >>
                    frame_header.chroma_subsampling.HShift(c),
                rects_to_process[rect_id].ysize() >>
                    frame_header.chroma_subsampling.VShift(c));
        Rect group_data_rect(xstart, ystart, rh.xsize(), rh.ysize());
        // Poison the image in this thread to prevent leaking initialized data
        // from a previous run in this thread in msan builds.
        msan::PoisonImage(dec_state->group_data[thread].Plane(c));
        CopyImageToWithPadding(
            rh, dec_state->decoded.Plane(c), dec_state->FinalizeRectPadding(),
            group_data_rect, &dec_state->group_data[thread].Plane(c));
      }
      Rect group_data_rect(xstart, ystart, rects_to_process[rect_id].xsize(),
                           rects_to_process[rect_id].ysize());
      std::vector<std::pair<ImageF*, Rect>> ec_rects;
      ec_rects.reserve(decoded->extra_channels().size());
      for (size_t i = 0; i < decoded->extra_channels().size(); i++) {
        Rect r = ScaleRectForEC(
            rects_to_process[rect_id].Crop(frame_dim.xsize, frame_dim.ysize),
            frame_header, i);
        if (frame_header.extra_channel_upsampling[i] != 1) {
          Rect ec_input_rect(kBlockDim, 2, r.xsize(), r.ysize());
          auto eti =
              &dec_state
                   ->ec_temp_images[thread * decoded->extra_channels().size() +
                                    i];
          // Poison the temp image on this thread to prevent leaking initialized
          // data from a previous run in this thread in msan builds.
          msan::PoisonImage(*eti);
          JXL_CHECK_IMAGE_INITIALIZED(dec_state->extra_channels[i], r);
          CopyImageToWithPadding(r, dec_state->extra_channels[i],
                                 /*padding=*/2, ec_input_rect, eti);
          ec_rects.emplace_back(eti, ec_input_rect);
        } else {
          JXL_CHECK_IMAGE_INITIALIZED(decoded->extra_channels()[i], r);
          ec_rects.emplace_back(&decoded->extra_channels()[i], r);
        }
      }
      if (!FinalizeImageRect(&dec_state->group_data[thread], group_data_rect,
                             ec_rects, dec_state, thread, decoded,
                             rects_to_process[rect_id])) {
        apply_features_ok = false;
      }
    };

    JXL_RETURN_IF_ERROR(RunOnPool(pool, 0, rects_to_process.size(),
                                  allocate_storage, run_apply_features,
                                  "ApplyFeatures"));

    if (!apply_features_ok) {
      return JXL_FAILURE("FinalizeImageRect failed");
    }
  }

  const size_t xsize = frame_dim.xsize_upsampled;
  const size_t ysize = frame_dim.ysize_upsampled;

  decoded->ShrinkTo(xsize, ysize);
  if (dec_state->pre_color_transform_frame.xsize() != 0) {
    dec_state->pre_color_transform_frame.ShrinkTo(xsize, ysize);
  }

  if (!skip_blending && ImageBlender::NeedsBlending(dec_state)) {
    if (dec_state->pre_color_transform_frame.xsize() != 0) {
      // Extra channels are going to be modified. Make a copy.
      dec_state->pre_color_transform_ec.clear();
      for (const auto& ec : decoded->extra_channels()) {
        dec_state->pre_color_transform_ec.emplace_back(CopyImage(ec));
      }
    }
    ImageBlender blender;
    ImageBundle foreground = std::move(*decoded);
    decoded->SetFromImage(Image3F(frame_header.nonserialized_metadata->xsize(),
                                  frame_header.nonserialized_metadata->ysize()),
                          foreground.c_current());
    std::vector<Rect> extra_channels_rects;
    decoded->extra_channels().reserve(foreground.extra_channels().size());
    extra_channels_rects.reserve(foreground.extra_channels().size());
    for (size_t i = 0; i < foreground.extra_channels().size(); ++i) {
      decoded->extra_channels().emplace_back(
          frame_header.nonserialized_metadata->xsize(),
          frame_header.nonserialized_metadata->ysize());
      extra_channels_rects.emplace_back(decoded->extra_channels().back());
    }
    JXL_RETURN_IF_ERROR(blender.PrepareBlending(
        dec_state, foreground.origin, foreground.xsize(), foreground.ysize(),
        &frame_header.nonserialized_metadata->m.extra_channel_info,
        foreground.c_current(), Rect(*decoded->color()),
        /*output=*/decoded->color(), Rect(*decoded->color()),
        &decoded->extra_channels(), std::move(extra_channels_rects)));

    std::vector<Rect> rects_to_process;
    for (size_t y = 0; y < frame_dim.ysize; y += kGroupDim) {
      for (size_t x = 0; x < frame_dim.xsize; x += kGroupDim) {
        Rect rect(x, y, kGroupDim, kGroupDim, frame_dim.xsize, frame_dim.ysize);
        if (rect.xsize() == 0 || rect.ysize() == 0) continue;
        rects_to_process.push_back(rect);
      }
    }

    std::atomic<bool> blending_ok{true};
    JXL_RETURN_IF_ERROR(RunOnPool(
        pool, 0, rects_to_process.size(), ThreadPool::NoInit,
        [&](const uint32_t i, size_t /*thread*/) {
          const Rect& rect = rects_to_process[i];
          auto rect_blender = blender.PrepareRect(
              rect, *foreground.color(), foreground.extra_channels(), rect);
          for (size_t y = 0; y < rect.ysize(); ++y) {
            if (!rect_blender.DoBlending(y)) {
              blending_ok = false;
              return;
            }
          }
        },
        "Blend"));
    JXL_RETURN_IF_ERROR(blending_ok.load());
  }

  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
