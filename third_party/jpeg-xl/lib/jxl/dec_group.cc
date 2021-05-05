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

#include "lib/jxl/dec_group.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "lib/jxl/frame_header.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_group.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/ac_context.h"
#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/coeff_order.h"
#include "lib/jxl/common.h"
#include "lib/jxl/convolve.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/dec_reconstruct.h"
#include "lib/jxl/dec_transforms-inl.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/epf.h"
#include "lib/jxl/opsin_params.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer-inl.h"
#include "lib/jxl/quantizer.h"

#ifndef LIB_JXL_DEC_GROUP_CC
#define LIB_JXL_DEC_GROUP_CC
namespace jxl {

// Interface for reading groups for DecodeGroupImpl.
class GetBlock {
 public:
  virtual void StartRow(size_t by) = 0;
  virtual Status LoadBlock(size_t bx, size_t by, const AcStrategy& acs,
                           size_t size, size_t log2_covered_blocks,
                           ACPtr block[3], ACType ac_type) = 0;
  virtual ~GetBlock() {}
};

// Controls whether DecodeGroupImpl renders to pixels or not.
enum DrawMode {
  // Render to pixels.
  kDraw = 0,
  // Don't render to pixels.
  kDontDraw = 1,
  // Don't do IDCT or dequantization, but just postprocessing. Used for
  // progressive DC.
  kOnlyImageFeatures = 2,
};

}  // namespace jxl
#endif  // LIB_JXL_DEC_GROUP_CC

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::ShiftRight;

using D = HWY_FULL(float);
using DU = HWY_FULL(uint32_t);
using DI = HWY_FULL(int32_t);
using DI16 = Rebind<int16_t, DI>;
constexpr D d;
constexpr DI di;
constexpr DI16 di16;

// TODO(veluca): consider SIMDfying.
void Transpose8x8InPlace(int32_t* JXL_RESTRICT block) {
  for (size_t x = 0; x < 8; x++) {
    for (size_t y = x + 1; y < 8; y++) {
      std::swap(block[y * 8 + x], block[x * 8 + y]);
    }
  }
}

template <ACType ac_type>
void DequantLane(Vec<D> scaled_dequant_x, Vec<D> scaled_dequant_y,
                 Vec<D> scaled_dequant_b,
                 const float* JXL_RESTRICT dequant_matrices, size_t dq_ofs,
                 size_t size, size_t k, Vec<D> x_cc_mul, Vec<D> b_cc_mul,
                 const float* JXL_RESTRICT biases, ACPtr qblock[3],
                 float* JXL_RESTRICT block) {
  const auto x_mul = Load(d, dequant_matrices + dq_ofs + k) * scaled_dequant_x;
  const auto y_mul =
      Load(d, dequant_matrices + dq_ofs + size + k) * scaled_dequant_y;
  const auto b_mul =
      Load(d, dequant_matrices + dq_ofs + 2 * size + k) * scaled_dequant_b;

  Vec<DI> quantized_x_int;
  Vec<DI> quantized_y_int;
  Vec<DI> quantized_b_int;
  if (ac_type == ACType::k16) {
    Rebind<int16_t, DI> di16;
    quantized_x_int = PromoteTo(di, Load(di16, qblock[0].ptr16 + k));
    quantized_y_int = PromoteTo(di, Load(di16, qblock[1].ptr16 + k));
    quantized_b_int = PromoteTo(di, Load(di16, qblock[2].ptr16 + k));
  } else {
    quantized_x_int = Load(di, qblock[0].ptr32 + k);
    quantized_y_int = Load(di, qblock[1].ptr32 + k);
    quantized_b_int = Load(di, qblock[2].ptr32 + k);
  }

  const auto dequant_x_cc =
      AdjustQuantBias(di, 0, quantized_x_int, biases) * x_mul;
  const auto dequant_y =
      AdjustQuantBias(di, 1, quantized_y_int, biases) * y_mul;
  const auto dequant_b_cc =
      AdjustQuantBias(di, 2, quantized_b_int, biases) * b_mul;

  const auto dequant_x = MulAdd(x_cc_mul, dequant_y, dequant_x_cc);
  const auto dequant_b = MulAdd(b_cc_mul, dequant_y, dequant_b_cc);
  Store(dequant_x, d, block + k);
  Store(dequant_y, d, block + size + k);
  Store(dequant_b, d, block + 2 * size + k);
}

template <ACType ac_type>
void DequantBlock(const AcStrategy& acs, float inv_global_scale, int quant,
                  float x_dm_multiplier, float b_dm_multiplier, Vec<D> x_cc_mul,
                  Vec<D> b_cc_mul, size_t kind, size_t size,
                  const Quantizer& quantizer,
                  const float* JXL_RESTRICT dequant_matrices,
                  size_t covered_blocks, const size_t* sbx,
                  const float* JXL_RESTRICT* JXL_RESTRICT dc_row,
                  size_t dc_stride, const float* JXL_RESTRICT biases,
                  ACPtr qblock[3], float* JXL_RESTRICT block) {
  PROFILER_FUNC;

  const auto scaled_dequant_s = inv_global_scale / quant;

  const auto scaled_dequant_x = Set(d, scaled_dequant_s * x_dm_multiplier);
  const auto scaled_dequant_y = Set(d, scaled_dequant_s);
  const auto scaled_dequant_b = Set(d, scaled_dequant_s * b_dm_multiplier);

  const size_t dq_ofs = quantizer.DequantMatrixOffset(kind, 0);

  for (size_t k = 0; k < covered_blocks * kDCTBlockSize; k += Lanes(d)) {
    DequantLane<ac_type>(scaled_dequant_x, scaled_dequant_y, scaled_dequant_b,
                         dequant_matrices, dq_ofs, size, k, x_cc_mul, b_cc_mul,
                         biases, qblock, block);
  }
  for (size_t c = 0; c < 3; c++) {
    LowestFrequenciesFromDC(acs.Strategy(), dc_row[c] + sbx[c], dc_stride,
                            block + c * size);
  }
}

Status DecodeGroupImpl(GetBlock* JXL_RESTRICT get_block,
                       GroupDecCache* JXL_RESTRICT group_dec_cache,
                       PassesDecoderState* JXL_RESTRICT dec_state,
                       size_t thread, size_t group_idx, ImageBundle* decoded,
                       DrawMode draw) {
  // TODO(veluca): investigate cache usage in this function.
  PROFILER_FUNC;
  constexpr size_t kGroupDataXBorder = PassesDecoderState::kGroupDataXBorder;
  constexpr size_t kGroupDataYBorder = PassesDecoderState::kGroupDataYBorder;

  const Rect block_rect = dec_state->shared->BlockGroupRect(group_idx);
  const AcStrategyImage& ac_strategy = dec_state->shared->ac_strategy;

  const size_t xsize_blocks = block_rect.xsize();
  const size_t ysize_blocks = block_rect.ysize();

  const size_t dc_stride = dec_state->shared->dc->PixelsPerRow();

  const float inv_global_scale = dec_state->shared->quantizer.InvGlobalScale();
  const float* JXL_RESTRICT dequant_matrices =
      dec_state->shared->quantizer.DequantMatrix(0, 0);

  const YCbCrChromaSubsampling& cs =
      dec_state->shared->frame_header.chroma_subsampling;

  const size_t idct_stride = dec_state->EagerFinalizeImageRect()
                                 ? dec_state->group_data[thread].PixelsPerRow()
                                 : dec_state->decoded.PixelsPerRow();

  HWY_ALIGN int32_t scaled_qtable[64 * 3];

  ACType ac_type = dec_state->coefficients->Type();
  auto dequant_block = (ac_type == ACType::k16 ? DequantBlock<ACType::k16>
                                               : DequantBlock<ACType::k32>);
  // Whether or not coefficients should be stored for future usage, and/or read
  // from past usage.
  bool accumulate = !dec_state->coefficients->IsEmpty();
  // Offset of the current block in the group.
  size_t offset = 0;

  std::array<int, 3> jpeg_c_map;
  std::array<int, 3> dcoff = {};

  // TODO(veluca): all of this should be done only once per image.
  if (decoded->IsJPEG()) {
    if (!dec_state->shared->cmap.IsJPEGCompatible()) {
      return JXL_FAILURE("The CfL map is not JPEG-compatible");
    }
    jpeg_c_map = JpegOrder(dec_state->shared->frame_header.color_transform,
                           decoded->jpeg_data->components.size() == 1);
    const std::vector<QuantEncoding>& qe =
        dec_state->shared->matrices.encodings();
    if (qe.empty() || qe[0].mode != QuantEncoding::Mode::kQuantModeRAW ||
        std::abs(qe[0].qraw.qtable_den - 1.f / (8 * 255)) > 1e-8f) {
      return JXL_FAILURE(
          "Quantization table is not a JPEG quantization table.");
    }
    for (size_t c = 0; c < 3; c++) {
      if (dec_state->shared->frame_header.color_transform ==
          ColorTransform::kNone) {
        dcoff[c] = 1024 / (*qe[0].qraw.qtable)[64 * c];
      }
      for (size_t i = 0; i < 64; i++) {
        // Transpose the matrix, as it will be used on the transposed block.
        int n = qe[0].qraw.qtable->at(64 + i);
        int d = qe[0].qraw.qtable->at(64 * c + i);
        if (n <= 0 || d <= 0 || n >= 65536 || d >= 65536) {
          return JXL_FAILURE("Invalid JPEG quantization table");
        }
        scaled_qtable[64 * c + (i % 8) * 8 + (i / 8)] =
            (1 << kCFLFixedPointPrecision) * n / d;
      }
    }
  }

  size_t hshift[3] = {cs.HShift(0), cs.HShift(1), cs.HShift(2)};
  size_t vshift[3] = {cs.VShift(0), cs.VShift(1), cs.VShift(2)};
  Rect r[3];
  for (size_t i = 0; i < 3; i++) {
    r[i] =
        Rect(block_rect.x0() >> hshift[i], block_rect.y0() >> vshift[i],
             block_rect.xsize() >> hshift[i], block_rect.ysize() >> vshift[i]);
  }

  for (size_t by = 0; by < ysize_blocks; ++by) {
    if (draw == kOnlyImageFeatures) break;
    get_block->StartRow(by);
    size_t sby[3] = {by >> vshift[0], by >> vshift[1], by >> vshift[2]};

    const int32_t* JXL_RESTRICT row_quant =
        block_rect.ConstRow(dec_state->shared->raw_quant_field, by);

    const float* JXL_RESTRICT dc_rows[3] = {
        r[0].ConstPlaneRow(*dec_state->shared->dc, 0, sby[0]),
        r[1].ConstPlaneRow(*dec_state->shared->dc, 1, sby[1]),
        r[2].ConstPlaneRow(*dec_state->shared->dc, 2, sby[2]),
    };

    const size_t ty = (block_rect.y0() + by) / kColorTileDimInBlocks;
    AcStrategyRow acs_row = ac_strategy.ConstRow(block_rect, by);

    const int8_t* JXL_RESTRICT row_cmap[3] = {
        dec_state->shared->cmap.ytox_map.ConstRow(ty),
        nullptr,
        dec_state->shared->cmap.ytob_map.ConstRow(ty),
    };

    float* JXL_RESTRICT idct_row[3];
    int16_t* JXL_RESTRICT jpeg_row[3];
    for (size_t c = 0; c < 3; c++) {
      if (dec_state->EagerFinalizeImageRect()) {
        idct_row[c] = dec_state->group_data[thread].PlaneRow(
                          c, sby[c] * kBlockDim + kGroupDataYBorder) +
                      kGroupDataXBorder;
      } else {
        idct_row[c] =
            dec_state->decoded.PlaneRow(c, (r[c].y0() + sby[c]) * kBlockDim) +
            r[c].x0() * kBlockDim;
      }
      if (decoded->IsJPEG()) {
        auto& component = decoded->jpeg_data->components[jpeg_c_map[c]];
        jpeg_row[c] =
            component.coeffs.data() +
            (component.width_in_blocks * (r[c].y0() + sby[c]) + r[c].x0()) *
                kDCTBlockSize;
      }
    }

    size_t bx = 0;
    for (size_t tx = 0; tx < DivCeil(xsize_blocks, kColorTileDimInBlocks);
         tx++) {
      size_t abs_tx = tx + block_rect.x0() / kColorTileDimInBlocks;
      auto x_cc_mul =
          Set(d, dec_state->shared->cmap.YtoXRatio(row_cmap[0][abs_tx]));
      auto b_cc_mul =
          Set(d, dec_state->shared->cmap.YtoBRatio(row_cmap[2][abs_tx]));
      // Increment bx by llf_x because those iterations would otherwise
      // immediately continue (!IsFirstBlock). Reduces mispredictions.
      for (; bx < xsize_blocks && bx < (tx + 1) * kColorTileDimInBlocks;) {
        size_t sbx[3] = {bx >> hshift[0], bx >> hshift[1], bx >> hshift[2]};
        AcStrategy acs = acs_row[bx];
        const size_t llf_x = acs.covered_blocks_x();

        // Can only happen in the second or lower rows of a varblock.
        if (JXL_UNLIKELY(!acs.IsFirstBlock())) {
          bx += llf_x;
          continue;
        }
        PROFILER_ZONE("DecodeGroupImpl inner");
        const size_t log2_covered_blocks = acs.log2_covered_blocks();

        const size_t covered_blocks = 1 << log2_covered_blocks;
        const size_t size = covered_blocks * kDCTBlockSize;

        ACPtr qblock[3];
        if (accumulate) {
          for (size_t c = 0; c < 3; c++) {
            qblock[c] = dec_state->coefficients->PlaneRow(c, group_idx, offset);
          }
        } else {
          // No point in reading from bitstream without accumulating and not
          // drawing.
          JXL_ASSERT(draw == kDraw);
          if (ac_type == ACType::k16) {
            memset(group_dec_cache->dec_group_qblock16, 0,
                   size * 3 * sizeof(int16_t));
            for (size_t c = 0; c < 3; c++) {
              qblock[c].ptr16 = group_dec_cache->dec_group_qblock16 + c * size;
            }
          } else {
            memset(group_dec_cache->dec_group_qblock, 0,
                   size * 3 * sizeof(int32_t));
            for (size_t c = 0; c < 3; c++) {
              qblock[c].ptr32 = group_dec_cache->dec_group_qblock + c * size;
            }
          }
        }
        JXL_RETURN_IF_ERROR(get_block->LoadBlock(
            bx, by, acs, size, log2_covered_blocks, qblock, ac_type));
        offset += size;
        if (draw == kDontDraw) {
          bx += llf_x;
          continue;
        }

        if (JXL_UNLIKELY(decoded->IsJPEG())) {
          if (acs.Strategy() != AcStrategy::Type::DCT) {
            return JXL_FAILURE(
                "Can only decode to JPEG if only DCT-8 is used.");
          }

          HWY_ALIGN int32_t transposed_dct_y[64];
          for (size_t c : {1, 0, 2}) {
            if (decoded->jpeg_data->components.size() == 1 && c != 1) {
              continue;
            }
            if ((sbx[c] << hshift[c] != bx) || (sby[c] << vshift[c] != by)) {
              continue;
            }
            int16_t* JXL_RESTRICT jpeg_pos =
                jpeg_row[c] + sbx[c] * kDCTBlockSize;
            // JPEG XL is transposed, JPEG is not.
            auto transposed_dct = qblock[c].ptr32;
            Transpose8x8InPlace(transposed_dct);
            // No CfL - no need to store the y block converted to integers.
            if (!cs.Is444() ||
                (row_cmap[0][abs_tx] == 0 && row_cmap[2][abs_tx] == 0)) {
              for (size_t i = 0; i < 64; i += Lanes(d)) {
                const auto ini = Load(di, transposed_dct + i);
                const auto ini16 = DemoteTo(di16, ini);
                StoreU(ini16, di16, jpeg_pos + i);
              }
            } else if (c == 1) {
              // Y channel: save for restoring X/B, but nothing else to do.
              for (size_t i = 0; i < 64; i += Lanes(d)) {
                const auto ini = Load(di, transposed_dct + i);
                Store(ini, di, transposed_dct_y + i);
                const auto ini16 = DemoteTo(di16, ini);
                StoreU(ini16, di16, jpeg_pos + i);
              }
            } else {
              // transposed_dct_y contains the y channel block, transposed.
              const auto scale = Set(
                  di, dec_state->shared->cmap.RatioJPEG(row_cmap[c][abs_tx]));
              const auto round = Set(di, 1 << (kCFLFixedPointPrecision - 1));
              for (int i = 0; i < 64; i += Lanes(d)) {
                auto in = Load(di, transposed_dct + i);
                auto in_y = Load(di, transposed_dct_y + i);
                auto qt = Load(di, scaled_qtable + c * size + i);
                auto coeff_scale =
                    ShiftRight<kCFLFixedPointPrecision>(qt * scale + round);
                auto cfl_factor = ShiftRight<kCFLFixedPointPrecision>(
                    in_y * coeff_scale + round);
                StoreU(DemoteTo(di16, in + cfl_factor), di16, jpeg_pos + i);
              }
            }
            jpeg_pos[0] =
                Clamp1<float>(dc_rows[c][sbx[c]] - dcoff[c], -2047, 2047);
          }
        } else {
          HWY_ALIGN float* const block = group_dec_cache->dec_group_block;
          // Dequantize and add predictions.
          dequant_block(
              acs, inv_global_scale, row_quant[bx], dec_state->x_dm_multiplier,
              dec_state->b_dm_multiplier, x_cc_mul, b_cc_mul, acs.RawStrategy(),
              size, dec_state->shared->quantizer, dequant_matrices,
              acs.covered_blocks_y() * acs.covered_blocks_x(), sbx, dc_rows,
              dc_stride,
              dec_state->output_encoding_info.opsin_params.quant_biases, qblock,
              block);

          for (size_t c : {1, 0, 2}) {
            if ((sbx[c] << hshift[c] != bx) || (sby[c] << vshift[c] != by)) {
              continue;
            }
            // IDCT
            float* JXL_RESTRICT idct_pos = idct_row[c] + sbx[c] * kBlockDim;
            TransformToPixels(acs.Strategy(), block + c * size, idct_pos,
                              idct_stride, group_dec_cache->scratch_space);
          }
        }
        bx += llf_x;
      }
    }
  }
  if (draw == kDontDraw) {
    return true;
  }
  // No ApplyImageFeatures in JPEG mode, or if using chroma subsampling. It will
  // be done after decoding the whole image (this allows it to work on the
  // chroma channels too).
  if (dec_state->EagerFinalizeImageRect() && !decoded->IsJPEG()) {
    // Copy the group borders to the border storage.
    size_t xsize_groups = dec_state->shared->frame_dim.xsize_groups;
    size_t xsize_padded = dec_state->shared->frame_dim.xsize_padded;
    size_t ysize_padded = dec_state->shared->frame_dim.ysize_padded;
    size_t gx = group_idx % xsize_groups;
    size_t gy = group_idx / xsize_groups;
    Image3F* group_data = &dec_state->group_data[thread];
    size_t x0 = block_rect.x0() * kBlockDim;
    size_t x1 = (block_rect.x0() + block_rect.xsize()) * kBlockDim;
    size_t y0 = block_rect.y0() * kBlockDim;
    size_t y1 = (block_rect.y0() + block_rect.ysize()) * kBlockDim;
    size_t padding = dec_state->FinalizeRectPadding();
    size_t borderx = dec_state->group_border_assigner.PaddingX(padding);
    size_t bordery = padding;
    size_t borderx_write = padding + borderx;
    size_t bordery_write = padding + bordery;
    CopyImageTo(
        Rect(kGroupDataXBorder, kGroupDataYBorder, x1 - x0, bordery_write),
        *group_data, Rect(x0, (gy * 2) * bordery_write, x1 - x0, bordery_write),
        &dec_state->borders_horizontal);
    CopyImageTo(
        Rect(kGroupDataXBorder, kGroupDataYBorder + y1 - y0 - bordery_write,
             x1 - x0, bordery_write),
        *group_data,
        Rect(x0, (gy * 2 + 1) * bordery_write, x1 - x0, bordery_write),
        &dec_state->borders_horizontal);
    CopyImageTo(
        Rect(kGroupDataXBorder, kGroupDataYBorder, borderx_write, y1 - y0),
        *group_data, Rect((gx * 2) * borderx_write, y0, borderx_write, y1 - y0),
        &dec_state->borders_vertical);
    CopyImageTo(Rect(kGroupDataXBorder + x1 - x0 - borderx_write,
                     kGroupDataYBorder, borderx_write, y1 - y0),
                *group_data,
                Rect((gx * 2 + 1) * borderx_write, y0, borderx_write, y1 - y0),
                &dec_state->borders_vertical);
    Rect fir_rects[GroupBorderAssigner::kMaxToFinalize];
    size_t num_fir_rects = 0;
    dec_state->group_border_assigner.GroupDone(
        group_idx, dec_state->FinalizeRectPadding(), fir_rects, &num_fir_rects);
    for (size_t i = 0; i < num_fir_rects; i++) {
      const Rect& r = fir_rects[i];
      // Limits of the area to copy from, in image coordinates.
      JXL_DASSERT(r.x0() == 0 || r.x0() >= borderx);
      size_t x0src = r.x0() == 0 ? r.x0() : r.x0() - borderx;
      size_t x1src = r.x0() + r.xsize() +
                     (r.x0() + r.xsize() == xsize_padded ? 0 : borderx);
      JXL_DASSERT(r.y0() == 0 || r.y0() >= bordery);
      size_t y0src = r.y0() == 0 ? r.y0() : r.y0() - bordery;
      size_t y1src = r.y0() + r.ysize() +
                     (r.y0() + r.ysize() == ysize_padded ? 0 : bordery);
      // Copy other groups' borders from the border storage.
      if (y0src < y0) {
        CopyImageTo(Rect(x0src, (gy * 2 - 1) * bordery_write, x1src - x0src,
                         bordery_write),
                    dec_state->borders_horizontal,
                    Rect(kGroupDataXBorder + x0src - x0,
                         kGroupDataYBorder - bordery_write, x1src - x0src,
                         bordery_write),
                    group_data);
      }
      if (y1src > y1) {
        CopyImageTo(
            Rect(x0src, (gy * 2 + 2) * bordery_write, x1src - x0src,
                 bordery_write),
            dec_state->borders_horizontal,
            Rect(kGroupDataXBorder + x0src - x0, kGroupDataYBorder + y1 - y0,
                 x1src - x0src, bordery_write),
            group_data);
      }
      if (x0src < x0) {
        CopyImageTo(
            Rect((gx * 2 - 1) * borderx_write, y0src, borderx_write,
                 y1src - y0src),
            dec_state->borders_vertical,
            Rect(kGroupDataXBorder - borderx_write,
                 kGroupDataYBorder + y0src - y0, borderx_write, y1src - y0src),
            group_data);
      }
      if (x1src > x1) {
        CopyImageTo(
            Rect((gx * 2 + 2) * borderx_write, y0src, borderx_write,
                 y1src - y0src),
            dec_state->borders_vertical,
            Rect(kGroupDataXBorder + x1 - x0, kGroupDataYBorder + y0src - y0,
                 borderx_write, y1src - y0src),
            group_data);
      }
      Rect group_data_rect(kGroupDataXBorder + r.x0() - x0,
                           kGroupDataYBorder + r.y0() - y0, r.xsize(),
                           r.ysize());
      JXL_RETURN_IF_ERROR(FinalizeImageRect(group_data, group_data_rect, {},
                                            dec_state, thread, decoded, r));
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
// Decode quantized AC coefficients of DCT blocks.
// LLF components in the output block will not be modified.
template <ACType ac_type>
Status DecodeACVarBlock(size_t ctx_offset, size_t log2_covered_blocks,
                        int32_t* JXL_RESTRICT row_nzeros,
                        const int32_t* JXL_RESTRICT row_nzeros_top,
                        size_t nzeros_stride, size_t c, size_t bx, size_t by,
                        size_t lbx, AcStrategy acs,
                        const coeff_order_t* JXL_RESTRICT coeff_order,
                        BitReader* JXL_RESTRICT br,
                        ANSSymbolReader* JXL_RESTRICT decoder,
                        const std::vector<uint8_t>& context_map,
                        const uint8_t* qdc_row, const int32_t* qf_row,
                        const BlockCtxMap& block_ctx_map, ACPtr block,
                        size_t shift = 0) {
  PROFILER_FUNC;
  // Equal to number of LLF coefficients.
  const size_t covered_blocks = 1 << log2_covered_blocks;
  const size_t size = covered_blocks * kDCTBlockSize;
  int32_t predicted_nzeros =
      PredictFromTopAndLeft(row_nzeros_top, row_nzeros, bx, 32);

  size_t ord = kStrategyOrder[acs.RawStrategy()];
  const coeff_order_t* JXL_RESTRICT order =
      &coeff_order[CoeffOrderOffset(ord, c)];

  size_t block_ctx = block_ctx_map.Context(qdc_row[lbx], qf_row[bx], ord, c);
  const int32_t nzero_ctx =
      block_ctx_map.NonZeroContext(predicted_nzeros, block_ctx) + ctx_offset;

  size_t nzeros = decoder->ReadHybridUint(nzero_ctx, br, context_map);
  if (nzeros + covered_blocks > size) {
    return JXL_FAILURE("Invalid AC: nzeros too large");
  }
  for (size_t y = 0; y < acs.covered_blocks_y(); y++) {
    for (size_t x = 0; x < acs.covered_blocks_x(); x++) {
      row_nzeros[bx + x + y * nzeros_stride] =
          (nzeros + covered_blocks - 1) >> log2_covered_blocks;
    }
  }

  const size_t histo_offset =
      ctx_offset + block_ctx_map.ZeroDensityContextsOffset(block_ctx);

  // Skip LLF
  {
    PROFILER_ZONE("AcDecSkipLLF, reader");
    size_t prev = (nzeros > size / 16 ? 0 : 1);
    for (size_t k = covered_blocks; k < size && nzeros != 0; ++k) {
      const size_t ctx =
          histo_offset + ZeroDensityContext(nzeros, k, covered_blocks,
                                            log2_covered_blocks, prev);
      const size_t u_coeff = decoder->ReadHybridUint(ctx, br, context_map);
      // Hand-rolled version of UnpackSigned, shifting before the conversion to
      // signed integer to avoid undefined behavior of shifting negative
      // numbers.
      const size_t magnitude = u_coeff >> 1;
      const size_t neg_sign = (~u_coeff) & 1;
      const intptr_t coeff =
          static_cast<intptr_t>((magnitude ^ (neg_sign - 1)) << shift);
      if (ac_type == ACType::k16) {
        block.ptr16[order[k]] += coeff;
      } else {
        block.ptr32[order[k]] += coeff;
      }
      prev = static_cast<size_t>(u_coeff != 0);
      nzeros -= prev;
    }
    if (JXL_UNLIKELY(nzeros != 0)) {
      return JXL_FAILURE(
          "Invalid AC: nzeros not 0. Block (%zu, %zu), channel %zu", bx, by, c);
    }
  }
  return true;
}

// Structs used by DecodeGroupImpl to get a quantized block.
// GetBlockFromBitstream uses ANS decoding (and thus keeps track of row
// pointers in row_nzeros), GetBlockFromEncoder simply reads the coefficient
// image provided by the encoder.

struct GetBlockFromBitstream : public GetBlock {
  void StartRow(size_t by) override {
    qf_row = rect.ConstRow(*qf, by);
    for (size_t c = 0; c < 3; c++) {
      size_t sby = by >> vshift[c];
      quant_dc_row = quant_dc->ConstRow(rect.y0() + by) + rect.x0();
      for (size_t i = 0; i < num_passes; i++) {
        row_nzeros[i][c] = group_dec_cache->num_nzeroes[i].PlaneRow(c, sby);
        row_nzeros_top[i][c] =
            sby == 0
                ? nullptr
                : group_dec_cache->num_nzeroes[i].ConstPlaneRow(c, sby - 1);
      }
    }
  }

  Status LoadBlock(size_t bx, size_t by, const AcStrategy& acs, size_t size,
                   size_t log2_covered_blocks, ACPtr block[3],
                   ACType ac_type) override {
    auto decode_ac_varblock = ac_type == ACType::k16
                                  ? DecodeACVarBlock<ACType::k16>
                                  : DecodeACVarBlock<ACType::k32>;
    for (size_t c : {1, 0, 2}) {
      size_t sbx = bx >> hshift[c];
      size_t sby = by >> vshift[c];
      if (JXL_UNLIKELY((sbx << hshift[c] != bx) || (sby << vshift[c] != by))) {
        continue;
      }

      for (size_t pass = 0; JXL_UNLIKELY(pass < num_passes); pass++) {
        JXL_RETURN_IF_ERROR(decode_ac_varblock(
            ctx_offset[pass], log2_covered_blocks, row_nzeros[pass][c],
            row_nzeros_top[pass][c], nzeros_stride, c, sbx, sby, bx, acs,
            &coeff_orders[pass * coeff_order_size], readers[pass],
            &decoders[pass], context_map[pass], quant_dc_row, qf_row,
            *block_ctx_map, block[c], shift_for_pass[pass]));
      }
    }
    return true;
  }

  Status Init(BitReader* JXL_RESTRICT* JXL_RESTRICT readers, size_t num_passes,
              size_t group_idx, size_t histo_selector_bits, const Rect& rect,
              GroupDecCache* JXL_RESTRICT group_dec_cache,
              PassesDecoderState* dec_state, size_t first_pass) {
    for (size_t i = 0; i < 3; i++) {
      hshift[i] = dec_state->shared->frame_header.chroma_subsampling.HShift(i);
      vshift[i] = dec_state->shared->frame_header.chroma_subsampling.VShift(i);
    }
    this->coeff_order_size = dec_state->shared->coeff_order_size;
    this->coeff_orders =
        dec_state->shared->coeff_orders.data() + first_pass * coeff_order_size;
    this->context_map = dec_state->context_map.data() + first_pass;
    this->readers = readers;
    this->num_passes = num_passes;
    this->shift_for_pass =
        dec_state->shared->frame_header.passes.shift + first_pass;
    this->group_dec_cache = group_dec_cache;
    this->rect = rect;
    block_ctx_map = &dec_state->shared->block_ctx_map;
    qf = &dec_state->shared->raw_quant_field;
    quant_dc = &dec_state->shared->quant_dc;

    for (size_t pass = 0; pass < num_passes; pass++) {
      // Select which histogram set to use among those of the current pass.
      size_t cur_histogram = 0;
      if (histo_selector_bits != 0) {
        cur_histogram = readers[pass]->ReadBits(histo_selector_bits);
      }
      if (cur_histogram >= dec_state->shared->num_histograms) {
        return JXL_FAILURE("Invalid histogram selector");
      }
      ctx_offset[pass] = cur_histogram * block_ctx_map->NumACContexts();

      decoders[pass] =
          ANSSymbolReader(&dec_state->code[pass + first_pass], readers[pass]);
    }
    nzeros_stride = group_dec_cache->num_nzeroes[0].PixelsPerRow();
    for (size_t i = 0; i < num_passes; i++) {
      JXL_ASSERT(
          nzeros_stride ==
          static_cast<size_t>(group_dec_cache->num_nzeroes[i].PixelsPerRow()));
    }
    return true;
  }

  const uint32_t* shift_for_pass = nullptr;  // not owned
  const coeff_order_t* JXL_RESTRICT coeff_orders;
  size_t coeff_order_size;
  const std::vector<uint8_t>* JXL_RESTRICT context_map;
  ANSSymbolReader decoders[kMaxNumPasses];
  BitReader* JXL_RESTRICT* JXL_RESTRICT readers;
  size_t num_passes;
  size_t ctx_offset[kMaxNumPasses];
  size_t nzeros_stride;
  int32_t* JXL_RESTRICT row_nzeros[kMaxNumPasses][3];
  const int32_t* JXL_RESTRICT row_nzeros_top[kMaxNumPasses][3];
  GroupDecCache* JXL_RESTRICT group_dec_cache;
  const BlockCtxMap* block_ctx_map;
  const ImageI* qf;
  const ImageB* quant_dc;
  const int32_t* qf_row;
  const uint8_t* quant_dc_row;
  Rect rect;
  size_t hshift[3], vshift[3];
};

struct GetBlockFromEncoder : public GetBlock {
  void StartRow(size_t by) override {}

  Status LoadBlock(size_t bx, size_t by, const AcStrategy& acs, size_t size,
                   size_t log2_covered_blocks, ACPtr block[3],
                   ACType ac_type) override {
    JXL_DASSERT(ac_type == ACType::k32);
    for (size_t c = 0; c < 3; c++) {
      // for each pass
      for (size_t i = 0; i < quantized_ac->size(); i++) {
        for (size_t k = 0; k < size; k++) {
          // TODO(veluca): SIMD.
          block[c].ptr32[k] += rows[i][c][offset + k];
        }
      }
    }
    offset += size;
    return true;
  }

  GetBlockFromEncoder(const std::vector<std::unique_ptr<ACImage>>& ac,
                      size_t group_idx)
      : quantized_ac(&ac) {
    // TODO(veluca): not supported with chroma subsampling.
    for (size_t i = 0; i < quantized_ac->size(); i++) {
      JXL_CHECK((*quantized_ac)[i]->Type() == ACType::k32);
      for (size_t c = 0; c < 3; c++) {
        rows[i][c] = (*quantized_ac)[i]->PlaneRow(c, group_idx, 0).ptr32;
      }
    }
  }

  const std::vector<std::unique_ptr<ACImage>>* JXL_RESTRICT quantized_ac;
  size_t offset = 0;
  const int32_t* JXL_RESTRICT rows[kMaxNumPasses][3];
};

HWY_EXPORT(DecodeGroupImpl);

}  // namespace

Status DecodeGroup(BitReader* JXL_RESTRICT* JXL_RESTRICT readers,
                   size_t num_passes, size_t group_idx,
                   PassesDecoderState* JXL_RESTRICT dec_state,
                   GroupDecCache* JXL_RESTRICT group_dec_cache, size_t thread,
                   ImageBundle* JXL_RESTRICT decoded, size_t first_pass,
                   bool force_draw, bool dc_only) {
  PROFILER_FUNC;

  DrawMode draw = (num_passes + first_pass ==
                   dec_state->shared->frame_header.passes.num_passes) ||
                          force_draw
                      ? kDraw
                      : kDontDraw;

  if (draw == kDraw && num_passes == 0 && first_pass == 0) {
    // We reuse filter_input_storage here as it is not currently in use.
    const Rect src_rect = dec_state->shared->BlockGroupRect(group_idx);
    const Rect copy_rect(kBlockDim, 2, src_rect.xsize(), src_rect.ysize());
    CopyImageToWithPadding(src_rect, *dec_state->shared->dc, 2, copy_rect,
                           &dec_state->filter_input_storage[thread]);
    EnsurePaddingInPlace(&dec_state->filter_input_storage[thread], copy_rect,
                         src_rect, dec_state->shared->frame_dim.xsize_blocks,
                         dec_state->shared->frame_dim.ysize_blocks, 2, 2);
    Image3F* upsampling_dst = &dec_state->decoded;
    Rect dst_rect(src_rect.x0() * 8, src_rect.y0() * 8, src_rect.xsize() * 8,
                  src_rect.ysize() * 8);
    if (dec_state->EagerFinalizeImageRect()) {
      upsampling_dst = &dec_state->group_data[thread];
      dst_rect = Rect(PassesDecoderState::kGroupDataXBorder,
                      PassesDecoderState::kGroupDataYBorder, dst_rect.xsize(),
                      dst_rect.ysize());
    }
    dec_state->dc_upsampler.UpsampleRect(
        dec_state->filter_input_storage[thread], copy_rect, upsampling_dst,
        dst_rect,
        static_cast<ssize_t>(src_rect.y0()) -
            static_cast<ssize_t>(copy_rect.y0()),
        dec_state->shared->frame_dim.ysize_blocks);
    draw = kOnlyImageFeatures;
  }

  size_t histo_selector_bits = 0;
  if (dc_only) {
    JXL_ASSERT(num_passes == 0);
  } else {
    JXL_ASSERT(dec_state->shared->num_histograms > 0);
    histo_selector_bits = CeilLog2Nonzero(dec_state->shared->num_histograms);
  }

  GetBlockFromBitstream get_block;
  JXL_RETURN_IF_ERROR(
      get_block.Init(readers, num_passes, group_idx, histo_selector_bits,
                     dec_state->shared->BlockGroupRect(group_idx),
                     group_dec_cache, dec_state, first_pass));

  JXL_RETURN_IF_ERROR(HWY_DYNAMIC_DISPATCH(DecodeGroupImpl)(
      &get_block, group_dec_cache, dec_state, thread, group_idx, decoded,
      draw));

  for (size_t pass = 0; pass < num_passes; pass++) {
    if (!get_block.decoders[pass].CheckANSFinalState()) {
      return JXL_FAILURE("ANS checksum failure.");
    }
  }
  return true;
}

Status DecodeGroupForRoundtrip(const std::vector<std::unique_ptr<ACImage>>& ac,
                               size_t group_idx,
                               PassesDecoderState* JXL_RESTRICT dec_state,
                               GroupDecCache* JXL_RESTRICT group_dec_cache,
                               size_t thread, ImageBundle* JXL_RESTRICT decoded,
                               AuxOut* aux_out) {
  PROFILER_FUNC;

  GetBlockFromEncoder get_block(ac, group_idx);
  group_dec_cache->InitOnce(
      /*num_passes=*/0,
      /*used_acs=*/(1u << AcStrategy::kNumValidStrategies) - 1);

  return HWY_DYNAMIC_DISPATCH(DecodeGroupImpl)(&get_block, group_dec_cache,
                                               dec_state, thread, group_idx,
                                               decoded, kDraw);
}

}  // namespace jxl
#endif  // HWY_ONCE
