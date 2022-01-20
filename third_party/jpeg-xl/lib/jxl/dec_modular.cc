// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_modular.h"

#include <stdint.h>

#include <vector>

#include "lib/jxl/frame_header.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_modular.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/compressed_dc.h"
#include "lib/jxl/epf.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/transform.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Rebind;

void MultiplySum(const size_t xsize,
                 const pixel_type* const JXL_RESTRICT row_in,
                 const pixel_type* const JXL_RESTRICT row_in_Y,
                 const float factor, float* const JXL_RESTRICT row_out) {
  const HWY_FULL(float) df;
  const Rebind<pixel_type, HWY_FULL(float)> di;  // assumes pixel_type <= float
  const auto factor_v = Set(df, factor);
  for (size_t x = 0; x < xsize; x += Lanes(di)) {
    const auto in = Load(di, row_in + x) + Load(di, row_in_Y + x);
    const auto out = ConvertTo(df, in) * factor_v;
    Store(out, df, row_out + x);
  }
}

void RgbFromSingle(const size_t xsize,
                   const pixel_type* const JXL_RESTRICT row_in,
                   const float factor, Image3F* decoded, size_t /*c*/, size_t y,
                   Rect& rect) {
  JXL_DASSERT(xsize <= rect.xsize());
  const HWY_FULL(float) df;
  const Rebind<pixel_type, HWY_FULL(float)> di;  // assumes pixel_type <= float

  float* const JXL_RESTRICT row_out_r = rect.PlaneRow(decoded, 0, y);
  float* const JXL_RESTRICT row_out_g = rect.PlaneRow(decoded, 1, y);
  float* const JXL_RESTRICT row_out_b = rect.PlaneRow(decoded, 2, y);

  const auto factor_v = Set(df, factor);
  for (size_t x = 0; x < xsize; x += Lanes(di)) {
    const auto in = Load(di, row_in + x);
    const auto out = ConvertTo(df, in) * factor_v;
    Store(out, df, row_out_r + x);
    Store(out, df, row_out_g + x);
    Store(out, df, row_out_b + x);
  }
}

// Same signature as RgbFromSingle so we can assign to the same pointer.
void SingleFromSingle(const size_t xsize,
                      const pixel_type* const JXL_RESTRICT row_in,
                      const float factor, Image3F* decoded, size_t c, size_t y,
                      Rect& rect) {
  JXL_DASSERT(xsize <= rect.xsize());
  const HWY_FULL(float) df;
  const Rebind<pixel_type, HWY_FULL(float)> di;  // assumes pixel_type <= float

  float* const JXL_RESTRICT row_out = rect.PlaneRow(decoded, c, y);

  const auto factor_v = Set(df, factor);
  for (size_t x = 0; x < xsize; x += Lanes(di)) {
    const auto in = Load(di, row_in + x);
    const auto out = ConvertTo(df, in) * factor_v;
    Store(out, df, row_out + x);
  }
}
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(MultiplySum);       // Local function
HWY_EXPORT(RgbFromSingle);     // Local function
HWY_EXPORT(SingleFromSingle);  // Local function

// convert custom [bits]-bit float (with [exp_bits] exponent bits) stored as int
// back to binary32 float
void int_to_float(const pixel_type* const JXL_RESTRICT row_in,
                  float* const JXL_RESTRICT row_out, const size_t xsize,
                  const int bits, const int exp_bits) {
  if (bits == 32) {
    JXL_ASSERT(sizeof(pixel_type) == sizeof(float));
    JXL_ASSERT(exp_bits == 8);
    memcpy(row_out, row_in, xsize * sizeof(float));
    return;
  }
  int exp_bias = (1 << (exp_bits - 1)) - 1;
  int sign_shift = bits - 1;
  int mant_bits = bits - exp_bits - 1;
  int mant_shift = 23 - mant_bits;
  for (size_t x = 0; x < xsize; ++x) {
    uint32_t f;
    memcpy(&f, &row_in[x], 4);
    int signbit = (f >> sign_shift);
    f &= (1 << sign_shift) - 1;
    if (f == 0) {
      row_out[x] = (signbit ? -0.f : 0.f);
      continue;
    }
    int exp = (f >> mant_bits);
    int mantissa = (f & ((1 << mant_bits) - 1));
    mantissa <<= mant_shift;
    // Try to normalize only if there is space for maneuver.
    if (exp == 0 && exp_bits < 8) {
      // subnormal number
      while ((mantissa & 0x800000) == 0) {
        mantissa <<= 1;
        exp--;
      }
      exp++;
      // remove leading 1 because it is implicit now
      mantissa &= 0x7fffff;
    }
    exp -= exp_bias;
    // broke up the arbitrary float into its parts, now reassemble into
    // binary32
    exp += 127;
    JXL_ASSERT(exp >= 0);
    f = (signbit ? 0x80000000 : 0);
    f |= (exp << 23);
    f |= mantissa;
    memcpy(&row_out[x], &f, 4);
  }
}

Status ModularFrameDecoder::DecodeGlobalInfo(BitReader* reader,
                                             const FrameHeader& frame_header,
                                             bool allow_truncated_group) {
  bool decode_color = frame_header.encoding == FrameEncoding::kModular;
  const auto& metadata = frame_header.nonserialized_metadata->m;
  bool is_gray = metadata.color_encoding.IsGray();
  size_t nb_chans = 3;
  if (is_gray && frame_header.color_transform == ColorTransform::kNone) {
    nb_chans = 1;
  }
  do_color = decode_color;
  if (!do_color) nb_chans = 0;
  size_t nb_extra = metadata.extra_channel_info.size();
  bool has_tree = reader->ReadBits(1);
  if (has_tree) {
    size_t tree_size_limit = std::min(
        static_cast<size_t>(1 << 22),
        1024 + frame_dim.xsize * frame_dim.ysize * (nb_chans + nb_extra) / 16);
    JXL_RETURN_IF_ERROR(DecodeTree(reader, &tree, tree_size_limit));
    JXL_RETURN_IF_ERROR(
        DecodeHistograms(reader, (tree.size() + 1) / 2, &code, &context_map));
  }

  bool fp = metadata.bit_depth.floating_point_sample;

  // bits_per_sample is just metadata for XYB images.
  if (metadata.bit_depth.bits_per_sample >= 32 && do_color &&
      frame_header.color_transform != ColorTransform::kXYB) {
    if (metadata.bit_depth.bits_per_sample == 32 && fp == false) {
      return JXL_FAILURE("uint32_t not supported in dec_modular");
    } else if (metadata.bit_depth.bits_per_sample > 32) {
      return JXL_FAILURE("bits_per_sample > 32 not supported");
    }
  }

  Image gi(frame_dim.xsize, frame_dim.ysize, metadata.bit_depth.bits_per_sample,
           nb_chans + nb_extra);

  all_same_shift = true;
  if (frame_header.color_transform == ColorTransform::kYCbCr) {
    for (size_t c = 0; c < nb_chans; c++) {
      gi.channel[c].hshift = frame_header.chroma_subsampling.HShift(c);
      gi.channel[c].vshift = frame_header.chroma_subsampling.VShift(c);
      size_t xsize_shifted =
          DivCeil(frame_dim.xsize, 1 << gi.channel[c].hshift);
      size_t ysize_shifted =
          DivCeil(frame_dim.ysize, 1 << gi.channel[c].vshift);
      gi.channel[c].shrink(xsize_shifted, ysize_shifted);
      if (gi.channel[c].hshift != gi.channel[0].hshift ||
          gi.channel[c].vshift != gi.channel[0].vshift)
        all_same_shift = false;
    }
  }

  for (size_t ec = 0, c = nb_chans; ec < nb_extra; ec++, c++) {
    size_t ecups = frame_header.extra_channel_upsampling[ec];
    gi.channel[c].shrink(DivCeil(frame_dim.xsize_upsampled, ecups),
                         DivCeil(frame_dim.ysize_upsampled, ecups));
    gi.channel[c].hshift = gi.channel[c].vshift =
        CeilLog2Nonzero(ecups) - CeilLog2Nonzero(frame_header.upsampling);
    if (gi.channel[c].hshift != gi.channel[0].hshift ||
        gi.channel[c].vshift != gi.channel[0].vshift)
      all_same_shift = false;
  }

  ModularOptions options;
  options.max_chan_size = frame_dim.group_dim;
  options.group_dim = frame_dim.group_dim;
  Status dec_status = ModularGenericDecompress(
      reader, gi, &global_header, ModularStreamId::Global().ID(frame_dim),
      &options,
      /*undo_transforms=*/false, &tree, &code, &context_map,
      allow_truncated_group);
  if (!allow_truncated_group) JXL_RETURN_IF_ERROR(dec_status);
  if (dec_status.IsFatalError()) {
    return JXL_FAILURE("Failed to decode global modular info");
  }

  // TODO(eustas): are we sure this can be done after partial decode?
  have_something = false;
  for (size_t c = 0; c < gi.channel.size(); c++) {
    Channel& gic = gi.channel[c];
    if (c >= gi.nb_meta_channels && gic.w <= frame_dim.group_dim &&
        gic.h <= frame_dim.group_dim)
      have_something = true;
  }
  // move global transforms to groups if possible
  if (!have_something && all_same_shift) {
    if (gi.transform.size() == 1 && gi.transform[0].id == TransformId::kRCT) {
      global_transform = gi.transform;
      gi.transform.clear();
      // TODO(jon): also move no-delta-palette out (trickier though)
    }
  }
  full_image = std::move(gi);
  return dec_status;
}

void ModularFrameDecoder::MaybeDropFullImage() {
  if (full_image.transform.empty() && !have_something && all_same_shift) {
    use_full_image = false;
    for (auto& ch : full_image.channel) {
      // keep metadata on channels around, but dealloc their planes
      ch.plane = Plane<pixel_type>();
    }
  }
}

Status ModularFrameDecoder::DecodeGroup(const Rect& rect, BitReader* reader,
                                        int minShift, int maxShift,
                                        const ModularStreamId& stream,
                                        bool zerofill,
                                        PassesDecoderState* dec_state,
                                        ImageBundle* output) {
  JXL_DASSERT(stream.kind == ModularStreamId::kModularDC ||
              stream.kind == ModularStreamId::kModularAC);
  const size_t xsize = rect.xsize();
  const size_t ysize = rect.ysize();
  Image gi(xsize, ysize, full_image.bitdepth, 0);
  // start at the first bigger-than-groupsize non-metachannel
  size_t c = full_image.nb_meta_channels;
  for (; c < full_image.channel.size(); c++) {
    Channel& fc = full_image.channel[c];
    if (fc.w > frame_dim.group_dim || fc.h > frame_dim.group_dim) break;
  }
  size_t beginc = c;
  for (; c < full_image.channel.size(); c++) {
    Channel& fc = full_image.channel[c];
    int shift = std::min(fc.hshift, fc.vshift);
    if (shift > maxShift) continue;
    if (shift < minShift) continue;
    Rect r(rect.x0() >> fc.hshift, rect.y0() >> fc.vshift,
           rect.xsize() >> fc.hshift, rect.ysize() >> fc.vshift, fc.w, fc.h);
    if (r.xsize() == 0 || r.ysize() == 0) continue;
    if (zerofill && use_full_image) {
      for (size_t y = 0; y < r.ysize(); ++y) {
        pixel_type* const JXL_RESTRICT row_out = r.Row(&fc.plane, y);
        memset(row_out, 0, r.xsize() * sizeof(*row_out));
      }
    } else {
      Channel gc(r.xsize(), r.ysize());
      if (zerofill) ZeroFillImage(&gc.plane);
      gc.hshift = fc.hshift;
      gc.vshift = fc.vshift;
      gi.channel.emplace_back(std::move(gc));
    }
  }
  if (zerofill && use_full_image) return true;
  // Return early if there's nothing to decode. Otherwise there might be
  // problems later (in ModularImageToDecodedRect).
  if (gi.channel.empty()) return true;
  ModularOptions options;
  if (!zerofill) {
    if (!ModularGenericDecompress(
            reader, gi, /*header=*/nullptr, stream.ID(frame_dim), &options,
            /*undo_transforms=*/true, &tree, &code, &context_map)) {
      return JXL_FAILURE("Failed to decode modular group");
    }
  }
  // Undo global transforms that have been pushed to the group level
  if (!use_full_image) {
    for (auto t : global_transform) {
      JXL_RETURN_IF_ERROR(t.Inverse(gi, global_header.wp_header));
    }
    JXL_RETURN_IF_ERROR(ModularImageToDecodedRect(
        gi, dec_state, nullptr, output, rect.Crop(dec_state->decoded)));
    return true;
  }
  int gic = 0;
  for (c = beginc; c < full_image.channel.size(); c++) {
    Channel& fc = full_image.channel[c];
    int shift = std::min(fc.hshift, fc.vshift);
    if (shift > maxShift) continue;
    if (shift < minShift) continue;
    Rect r(rect.x0() >> fc.hshift, rect.y0() >> fc.vshift,
           rect.xsize() >> fc.hshift, rect.ysize() >> fc.vshift, fc.w, fc.h);
    if (r.xsize() == 0 || r.ysize() == 0) continue;
    JXL_ASSERT(use_full_image);
    CopyImageTo(/*rect_from=*/Rect(0, 0, r.xsize(), r.ysize()),
                /*from=*/gi.channel[gic].plane,
                /*rect_to=*/r, /*to=*/&fc.plane);
    gic++;
  }
  return true;
}
Status ModularFrameDecoder::DecodeVarDCTDC(size_t group_id, BitReader* reader,
                                           PassesDecoderState* dec_state) {
  const Rect r = dec_state->shared->DCGroupRect(group_id);
  // TODO(eustas): investigate if we could reduce the impact of
  //               EvalRationalPolynomial; generally speaking, the limit is
  //               2**(128/(3*magic)), where 128 comes from IEEE 754 exponent,
  //               3 comes from XybToRgb that cubes the values, and "magic" is
  //               the sum of all other contributions. 2**18 is known to lead
  //               to NaN on input found by fuzzing (see commit message).
  Image image(r.xsize(), r.ysize(), full_image.bitdepth, 3);
  size_t stream_id = ModularStreamId::VarDCTDC(group_id).ID(frame_dim);
  reader->Refill();
  size_t extra_precision = reader->ReadFixedBits<2>();
  float mul = 1.0f / (1 << extra_precision);
  ModularOptions options;
  for (size_t c = 0; c < 3; c++) {
    Channel& ch = image.channel[c < 2 ? c ^ 1 : c];
    ch.w >>= dec_state->shared->frame_header.chroma_subsampling.HShift(c);
    ch.h >>= dec_state->shared->frame_header.chroma_subsampling.VShift(c);
    ch.shrink();
  }
  if (!ModularGenericDecompress(
          reader, image, /*header=*/nullptr, stream_id, &options,
          /*undo_transforms=*/true, &tree, &code, &context_map)) {
    return JXL_FAILURE("Failed to decode modular DC group");
  }
  DequantDC(r, &dec_state->shared_storage.dc_storage,
            &dec_state->shared_storage.quant_dc, image,
            dec_state->shared->quantizer.MulDC(), mul,
            dec_state->shared->cmap.DCFactors(),
            dec_state->shared->frame_header.chroma_subsampling,
            dec_state->shared->block_ctx_map);
  return true;
}

Status ModularFrameDecoder::DecodeAcMetadata(size_t group_id, BitReader* reader,
                                             PassesDecoderState* dec_state) {
  const Rect r = dec_state->shared->DCGroupRect(group_id);
  size_t upper_bound = r.xsize() * r.ysize();
  reader->Refill();
  size_t count = reader->ReadBits(CeilLog2Nonzero(upper_bound)) + 1;
  size_t stream_id = ModularStreamId::ACMetadata(group_id).ID(frame_dim);
  // YToX, YToB, ACS + QF, EPF
  Image image(r.xsize(), r.ysize(), full_image.bitdepth, 4);
  static_assert(kColorTileDimInBlocks == 8, "Color tile size changed");
  Rect cr(r.x0() >> 3, r.y0() >> 3, (r.xsize() + 7) >> 3, (r.ysize() + 7) >> 3);
  image.channel[0] = Channel(cr.xsize(), cr.ysize(), 3, 3);
  image.channel[1] = Channel(cr.xsize(), cr.ysize(), 3, 3);
  image.channel[2] = Channel(count, 2, 0, 0);
  ModularOptions options;
  if (!ModularGenericDecompress(
          reader, image, /*header=*/nullptr, stream_id, &options,
          /*undo_transforms=*/true, &tree, &code, &context_map)) {
    return JXL_FAILURE("Failed to decode AC metadata");
  }
  ConvertPlaneAndClamp(Rect(image.channel[0].plane), image.channel[0].plane, cr,
                       &dec_state->shared_storage.cmap.ytox_map);
  ConvertPlaneAndClamp(Rect(image.channel[1].plane), image.channel[1].plane, cr,
                       &dec_state->shared_storage.cmap.ytob_map);
  size_t num = 0;
  bool is444 = dec_state->shared->frame_header.chroma_subsampling.Is444();
  auto& ac_strategy = dec_state->shared_storage.ac_strategy;
  size_t xlim = std::min(ac_strategy.xsize(), r.x0() + r.xsize());
  size_t ylim = std::min(ac_strategy.ysize(), r.y0() + r.ysize());
  uint32_t local_used_acs = 0;
  for (size_t iy = 0; iy < r.ysize(); iy++) {
    size_t y = r.y0() + iy;
    int* row_qf = r.Row(&dec_state->shared_storage.raw_quant_field, iy);
    uint8_t* row_epf = r.Row(&dec_state->shared_storage.epf_sharpness, iy);
    int* row_in_1 = image.channel[2].plane.Row(0);
    int* row_in_2 = image.channel[2].plane.Row(1);
    int* row_in_3 = image.channel[3].plane.Row(iy);
    for (size_t ix = 0; ix < r.xsize(); ix++) {
      size_t x = r.x0() + ix;
      int sharpness = row_in_3[ix];
      if (sharpness < 0 || sharpness >= LoopFilter::kEpfSharpEntries) {
        return JXL_FAILURE("Corrupted sharpness field");
      }
      row_epf[ix] = sharpness;
      if (ac_strategy.IsValid(x, y)) {
        continue;
      }

      if (num >= count) return JXL_FAILURE("Corrupted stream");

      if (!AcStrategy::IsRawStrategyValid(row_in_1[num])) {
        return JXL_FAILURE("Invalid AC strategy");
      }
      local_used_acs |= 1u << row_in_1[num];
      AcStrategy acs = AcStrategy::FromRawStrategy(row_in_1[num]);
      if ((acs.covered_blocks_x() > 1 || acs.covered_blocks_y() > 1) &&
          !is444) {
        return JXL_FAILURE(
            "AC strategy not compatible with chroma subsampling");
      }
      // Ensure that blocks do not overflow *AC* groups.
      size_t next_x_ac_block = (x / kGroupDimInBlocks + 1) * kGroupDimInBlocks;
      size_t next_y_ac_block = (y / kGroupDimInBlocks + 1) * kGroupDimInBlocks;
      size_t next_x_dct_block = x + acs.covered_blocks_x();
      size_t next_y_dct_block = y + acs.covered_blocks_y();
      if (next_x_dct_block > next_x_ac_block || next_x_dct_block > xlim) {
        return JXL_FAILURE("Invalid AC strategy, x overflow");
      }
      if (next_y_dct_block > next_y_ac_block || next_y_dct_block > ylim) {
        return JXL_FAILURE("Invalid AC strategy, y overflow");
      }
      JXL_RETURN_IF_ERROR(
          ac_strategy.SetNoBoundsCheck(x, y, AcStrategy::Type(row_in_1[num])));
      row_qf[ix] =
          1 + std::max(0, std::min(Quantizer::kQuantMax - 1, row_in_2[num]));
      num++;
    }
  }
  dec_state->used_acs |= local_used_acs;
  if (dec_state->shared->frame_header.loop_filter.epf_iters > 0) {
    ComputeSigma(r, dec_state);
  }
  return true;
}

Status ModularFrameDecoder::ModularImageToDecodedRect(
    Image& gi, PassesDecoderState* dec_state, jxl::ThreadPool* pool,
    ImageBundle* output, Rect rect) {
  auto& decoded = dec_state->decoded;
  const auto& frame_header = dec_state->shared->frame_header;
  const auto* metadata = frame_header.nonserialized_metadata;
  size_t xsize = rect.xsize();
  size_t ysize = rect.ysize();
  if (!xsize || !ysize) {
    return true;
  }
  JXL_DASSERT(rect.IsInside(decoded));

  size_t c = 0;
  if (do_color) {
    const bool rgb_from_gray =
        metadata->m.color_encoding.IsGray() &&
        frame_header.color_transform == ColorTransform::kNone;
    const bool fp = metadata->m.bit_depth.floating_point_sample &&
                    frame_header.color_transform != ColorTransform::kXYB;
    for (; c < 3; c++) {
      float factor = full_image.bitdepth < 32
                         ? 1.f / ((1u << full_image.bitdepth) - 1)
                         : 0;
      size_t c_in = c;
      if (frame_header.color_transform == ColorTransform::kXYB) {
        factor = dec_state->shared->matrices.DCQuants()[c];
        // XYB is encoded as YX(B-Y)
        if (c < 2) c_in = 1 - c;
      } else if (rgb_from_gray) {
        c_in = 0;
      }
      JXL_ASSERT(c_in < gi.channel.size());
      Channel& ch_in = gi.channel[c_in];
      // TODO(eustas): could we detect it on earlier stage?
      if (ch_in.w == 0 || ch_in.h == 0) {
        return JXL_FAILURE("Empty image");
      }
      size_t xsize_shifted = DivCeil(xsize, 1 << ch_in.hshift);
      size_t ysize_shifted = DivCeil(ysize, 1 << ch_in.vshift);
      Rect r(rect.x0() >> ch_in.hshift, rect.y0() >> ch_in.vshift,
             rect.xsize() >> ch_in.hshift, rect.ysize() >> ch_in.vshift,
             DivCeil(decoded.xsize(), 1 << ch_in.hshift),
             DivCeil(decoded.ysize(), 1 << ch_in.vshift));
      if (r.ysize() != ch_in.h || r.xsize() != ch_in.w) {
        return JXL_FAILURE("Dimension mismatch: trying to fit a %" PRIuS
                           "x%" PRIuS
                           " modular channel into "
                           "a %" PRIuS "x%" PRIuS " rect",
                           ch_in.w, ch_in.h, r.xsize(), r.ysize());
      }
      if (frame_header.color_transform == ColorTransform::kXYB && c == 2) {
        JXL_ASSERT(!fp);
        RunOnPool(
            pool, 0, ysize_shifted, jxl::ThreadPool::SkipInit(),
            [&](const int task, const int thread) {
              const size_t y = task;
              const pixel_type* const JXL_RESTRICT row_in = ch_in.Row(y);
              const pixel_type* const JXL_RESTRICT row_in_Y =
                  gi.channel[0].Row(y);
              float* const JXL_RESTRICT row_out = r.PlaneRow(&decoded, c, y);
              HWY_DYNAMIC_DISPATCH(MultiplySum)
              (xsize_shifted, row_in, row_in_Y, factor, row_out);
            },
            "ModularIntToFloat");
      } else if (fp) {
        int bits = metadata->m.bit_depth.bits_per_sample;
        int exp_bits = metadata->m.bit_depth.exponent_bits_per_sample;
        RunOnPool(
            pool, 0, ysize_shifted, jxl::ThreadPool::SkipInit(),
            [&](const int task, const int thread) {
              const size_t y = task;
              const pixel_type* const JXL_RESTRICT row_in = ch_in.Row(y);
              float* const JXL_RESTRICT row_out = r.PlaneRow(&decoded, c, y);
              int_to_float(row_in, row_out, xsize_shifted, bits, exp_bits);
            },
            "ModularIntToFloat_losslessfloat");
      } else {
        RunOnPool(
            pool, 0, ysize_shifted, jxl::ThreadPool::SkipInit(),
            [&](const int task, const int thread) {
              const size_t y = task;
              const pixel_type* const JXL_RESTRICT row_in = ch_in.Row(y);
              if (rgb_from_gray) {
                HWY_DYNAMIC_DISPATCH(RgbFromSingle)
                (xsize_shifted, row_in, factor, &decoded, c, y, r);
              } else {
                HWY_DYNAMIC_DISPATCH(SingleFromSingle)
                (xsize_shifted, row_in, factor, &decoded, c, y, r);
              }
            },
            "ModularIntToFloat");
      }
      if (rgb_from_gray) {
        break;
      }
    }
    if (rgb_from_gray) {
      c = 1;
    }
  }
  for (size_t ec = 0; ec < dec_state->extra_channels.size(); ec++, c++) {
    const ExtraChannelInfo& eci = output->metadata()->extra_channel_info[ec];
    int bits = eci.bit_depth.bits_per_sample;
    int exp_bits = eci.bit_depth.exponent_bits_per_sample;
    bool fp = eci.bit_depth.floating_point_sample;
    JXL_ASSERT(fp || bits < 32);
    const float mul = fp ? 0 : (1.0f / ((1u << bits) - 1));
    size_t ecups = frame_header.extra_channel_upsampling[ec];
    const size_t ec_xsize = DivCeil(frame_dim.xsize_upsampled, ecups);
    const size_t ec_ysize = DivCeil(frame_dim.ysize_upsampled, ecups);
    JXL_ASSERT(c < gi.channel.size());
    Channel& ch_in = gi.channel[c];
    // For x0, y0 there's no need to do a DivCeil().
    JXL_DASSERT(rect.x0() % (1ul << ch_in.hshift) == 0);
    JXL_DASSERT(rect.y0() % (1ul << ch_in.vshift) == 0);
    Rect r(rect.x0() >> ch_in.hshift, rect.y0() >> ch_in.vshift,
           DivCeil(rect.xsize(), 1lu << ch_in.hshift),
           DivCeil(rect.ysize(), 1lu << ch_in.vshift), ec_xsize, ec_ysize);

    JXL_DASSERT(r.IsInside(dec_state->extra_channels[ec]));
    JXL_DASSERT(Rect(0, 0, r.xsize(), r.ysize()).IsInside(ch_in.plane));
    for (size_t y = 0; y < r.ysize(); ++y) {
      float* const JXL_RESTRICT row_out =
          r.Row(&dec_state->extra_channels[ec], y);
      const pixel_type* const JXL_RESTRICT row_in = ch_in.Row(y);
      if (fp) {
        int_to_float(row_in, row_out, r.xsize(), bits, exp_bits);
      } else {
        for (size_t x = 0; x < r.xsize(); ++x) {
          row_out[x] = row_in[x] * mul;
        }
      }
    }
    JXL_CHECK_IMAGE_INITIALIZED(dec_state->extra_channels[ec], r);
  }
  return true;
}

Status ModularFrameDecoder::FinalizeDecoding(PassesDecoderState* dec_state,
                                             jxl::ThreadPool* pool,
                                             ImageBundle* output,
                                             bool inplace) {
  if (!use_full_image) return true;
  Image gi = (inplace ? std::move(full_image) : full_image.clone());
  size_t xsize = gi.w;
  size_t ysize = gi.h;

  // Don't use threads if total image size is smaller than a group
  if (xsize * ysize < frame_dim.group_dim * frame_dim.group_dim) pool = nullptr;

  // Undo the global transforms
  gi.undo_transforms(global_header.wp_header, pool);
  for (auto t : global_transform) {
    JXL_RETURN_IF_ERROR(t.Inverse(gi, global_header.wp_header));
  }
  if (gi.error) return JXL_FAILURE("Undoing transforms failed");

  auto& decoded = dec_state->decoded;

  JXL_RETURN_IF_ERROR(
      ModularImageToDecodedRect(gi, dec_state, pool, output, Rect(decoded)));
  return true;
}

static constexpr const float kAlmostZero = 1e-8f;

Status ModularFrameDecoder::DecodeQuantTable(
    size_t required_size_x, size_t required_size_y, BitReader* br,
    QuantEncoding* encoding, size_t idx,
    ModularFrameDecoder* modular_frame_decoder) {
  JXL_RETURN_IF_ERROR(F16Coder::Read(br, &encoding->qraw.qtable_den));
  if (encoding->qraw.qtable_den < kAlmostZero) {
    // qtable[] values are already checked for <= 0 so the denominator may not
    // be negative.
    return JXL_FAILURE("Invalid qtable_den: value too small");
  }
  Image image(required_size_x, required_size_y, 8, 3);
  ModularOptions options;
  if (modular_frame_decoder) {
    JXL_RETURN_IF_ERROR(ModularGenericDecompress(
        br, image, /*header=*/nullptr,
        ModularStreamId::QuantTable(idx).ID(modular_frame_decoder->frame_dim),
        &options, /*undo_transforms=*/true, &modular_frame_decoder->tree,
        &modular_frame_decoder->code, &modular_frame_decoder->context_map));
  } else {
    JXL_RETURN_IF_ERROR(ModularGenericDecompress(br, image, /*header=*/nullptr,
                                                 0, &options,
                                                 /*undo_transforms=*/true));
  }
  if (!encoding->qraw.qtable) {
    encoding->qraw.qtable = new std::vector<int>();
  }
  encoding->qraw.qtable->resize(required_size_x * required_size_y * 3);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < required_size_y; y++) {
      int* JXL_RESTRICT row = image.channel[c].Row(y);
      for (size_t x = 0; x < required_size_x; x++) {
        (*encoding->qraw.qtable)[c * required_size_x * required_size_y +
                                 y * required_size_x + x] = row[x];
        if (row[x] <= 0) {
          return JXL_FAILURE("Invalid raw quantization table");
        }
      }
    }
  }
  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
