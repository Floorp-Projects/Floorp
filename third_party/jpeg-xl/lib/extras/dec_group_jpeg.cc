// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec_group_jpeg.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/dec_group_jpeg.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/status.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/dec_transforms-inl.h"
#include "lib/jxl/render_pipeline/render_pipeline.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::And;
using hwy::HWY_NAMESPACE::AndNot;
using hwy::HWY_NAMESPACE::ApproximateReciprocal;
using hwy::HWY_NAMESPACE::Gt;
using hwy::HWY_NAMESPACE::IfThenElse;
using hwy::HWY_NAMESPACE::IfThenElseZero;
using hwy::HWY_NAMESPACE::Lt;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Xor;

using D = HWY_FULL(float);
using DI = HWY_FULL(int32_t);
constexpr D d;
constexpr DI di;

template <class DI>
HWY_INLINE HWY_MAYBE_UNUSED Vec<Rebind<float, DI>> AdjustQuantBias(
    DI di, const size_t c, const Vec<DI> quant_i,
    const float* HWY_RESTRICT biases) {
  const Rebind<float, DI> df;

  const auto quant = ConvertTo(df, quant_i);

  // Compare |quant|, keep sign bit for negating result.
  const auto kSign = BitCast(df, Set(di, INT32_MIN));
  const auto sign = And(quant, kSign);  // TODO(janwas): = abs ^ orig
  const auto abs_quant = AndNot(kSign, quant);

  // If |x| is 1, kZeroBias creates a different bias for each channel.
  // We're implementing the following:
  // if (quant == 0) return 0;
  // if (quant == 1) return biases[c];
  // if (quant == -1) return -biases[c];
  // return quant - biases[3] / quant;

  // Integer comparison is not helpful because Clang incurs bypass penalties
  // from unnecessarily mixing integer and float.
  const auto is_01 = Lt(abs_quant, Set(df, 1.125f));
  const auto not_0 = Gt(abs_quant, Zero(df));

  // Bitwise logic is faster than quant * biases[c].
  const auto one_bias = IfThenElseZero(not_0, Xor(Set(df, biases[c]), sign));

  // About 2E-5 worse than ReciprocalNR or division.
  const auto bias =
      NegMulAdd(Set(df, biases[3]), ApproximateReciprocal(quant), quant);

  return IfThenElse(is_01, one_bias, bias);
}

void DequantBlock(const int16_t* JXL_RESTRICT qblock, size_t c,
                  const float* JXL_RESTRICT dequant_matrices,
                  const float* JXL_RESTRICT biases, float* JXL_RESTRICT block) {
  for (size_t k = 0; k < kDCTBlockSize; k += Lanes(d)) {
    const auto mul = Load(d, dequant_matrices + c * kDCTBlockSize + k);
    Rebind<int16_t, DI> di16;
    Vec<DI> quantized = PromoteTo(di, Load(di16, qblock + k));
    const auto dequant = Mul(AdjustQuantBias(di, c, quantized, biases), mul);
    Store(dequant, d, block + k);
  }
}

Status DecodeGroupJpeg(const Image3S& coeffs, size_t group_idx,
                       const Rect block_rect, const YCbCrChromaSubsampling& cs,
                       const float* dequant_matrices,
                       float* JXL_RESTRICT group_dec_cache, size_t thread,
                       RenderPipelineInput& render_pipeline_input) {
  HWY_ALIGN float* const block = group_dec_cache;
  HWY_ALIGN float* const scratch_space = block + kDCTBlockSize;

  size_t hshift[3] = {cs.HShift(0), cs.HShift(1), cs.HShift(2)};
  size_t vshift[3] = {cs.VShift(0), cs.VShift(1), cs.VShift(2)};

  static constexpr float kDefaultQuantBias[4] = {
      1.0f - 0.05465007330715401f,
      1.0f - 0.07005449891748593f,
      1.0f - 0.049935103337343655f,
      0.145f,
  };

  for (size_t c = 0; c < 3; ++c) {
    ImageF* rpbuffer = render_pipeline_input.GetBuffer(c).first;
    Rect rect = render_pipeline_input.GetBuffer(c).second;
    size_t xsize_blocks = DivCeil(block_rect.xsize(), 1 << hshift[c]);
    size_t ysize_blocks = DivCeil(block_rect.ysize(), 1 << vshift[c]);
    size_t offset = 0;
    for (size_t by = 0; by < ysize_blocks; ++by) {
      float* JXL_RESTRICT idct_row = rect.Row(rpbuffer, by * kBlockDim);
      size_t idct_stride = rpbuffer->PixelsPerRow();
      for (size_t bx = 0; bx < xsize_blocks; ++bx) {
        const int16_t* qblock = &coeffs.PlaneRow(c, group_idx)[offset];
        offset += kDCTBlockSize;
        DequantBlock(qblock, c, dequant_matrices, kDefaultQuantBias, block);
        // IDCT
        float* JXL_RESTRICT idct_pos = idct_row + bx * kBlockDim;
        // JPEG XL transposes the DCT, JPEG doesn't.
        Transpose<8, 8>::Run(DCTFrom(block, 8), DCTTo(scratch_space, 8));
        TransformToPixels(AcStrategy::DCT, scratch_space, idct_pos, idct_stride,
                          block);
      }
    }
  }
  return true;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
namespace {
HWY_EXPORT(DecodeGroupJpeg);
}  // namespace

namespace extras {
Status DecodeGroupJpeg(const Image3S& coeffs, size_t group_idx,
                       const Rect block_rect, const YCbCrChromaSubsampling& cs,
                       const float* dequant_matrices,
                       float* JXL_RESTRICT group_dec_cache, size_t thread,
                       RenderPipelineInput& render_pipeline_input) {
  return HWY_DYNAMIC_DISPATCH(DecodeGroupJpeg)(
      coeffs, group_idx, block_rect, cs, dequant_matrices, group_dec_cache,
      thread, render_pipeline_input);
}

}  // namespace extras
}  // namespace jxl
#endif  // HWY_ONCE
