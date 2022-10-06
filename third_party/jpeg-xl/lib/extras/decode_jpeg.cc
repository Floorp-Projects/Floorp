// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/decode_jpeg.h"

#include "lib/extras/dec_group_jpeg.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"
#include "lib/jxl/jpeg/enc_jpeg_data_reader.h"
#include "lib/jxl/render_pipeline/render_pipeline.h"
#include "lib/jxl/render_pipeline/stage_chroma_upsampling.h"
#include "lib/jxl/render_pipeline/stage_write.h"
#include "lib/jxl/render_pipeline/stage_ycbcr.h"

namespace jxl {
namespace extras {

namespace {

Rect BlockGroupRect(const FrameDimensions& frame_dim, size_t group_index) {
  const size_t gx = group_index % frame_dim.xsize_groups;
  const size_t gy = group_index / frame_dim.xsize_groups;
  const Rect rect(gx * (frame_dim.group_dim >> 3),
                  gy * (frame_dim.group_dim >> 3), frame_dim.group_dim >> 3,
                  frame_dim.group_dim >> 3, frame_dim.xsize_blocks,
                  frame_dim.ysize_blocks);
  return rect;
}

Rect DCGroupRect(const FrameDimensions& frame_dim, size_t group_index) {
  const size_t gx = group_index % frame_dim.xsize_dc_groups;
  const size_t gy = group_index / frame_dim.xsize_dc_groups;
  const Rect rect(gx * frame_dim.group_dim, gy * frame_dim.group_dim,
                  frame_dim.group_dim, frame_dim.group_dim,
                  frame_dim.xsize_blocks, frame_dim.ysize_blocks);
  return rect;
}

Status SetChromaSubsamplingFromJpegData(const jpeg::JPEGData& jpeg_data,
                                        YCbCrChromaSubsampling* cs) {
  size_t nbcomp = jpeg_data.components.size();
  if (nbcomp == 3) {
    uint8_t hsample[3], vsample[3];
    for (size_t i = 0; i < nbcomp; i++) {
      hsample[i] = jpeg_data.components[i].h_samp_factor;
      vsample[i] = jpeg_data.components[i].v_samp_factor;
    }
    JXL_RETURN_IF_ERROR(cs->Set(hsample, vsample));
  } else if (nbcomp == 1) {
    uint8_t hsample[3], vsample[3];
    for (size_t i = 0; i < 3; i++) {
      hsample[i] = jpeg_data.components[0].h_samp_factor;
      vsample[i] = jpeg_data.components[0].v_samp_factor;
    }
    JXL_RETURN_IF_ERROR(cs->Set(hsample, vsample));
  }
  return true;
}

bool IsYCbCrJpeg(const jpeg::JPEGData& jpeg_data) {
  size_t nbcomp = jpeg_data.components.size();
  bool is_rgb = false;
  const auto& markers = jpeg_data.marker_order;
  // If there is a JFIF marker, this is YCbCr. Otherwise...
  if (std::find(markers.begin(), markers.end(), 0xE0) == markers.end()) {
    // Try to find an 'Adobe' marker.
    size_t app_markers = 0;
    size_t i = 0;
    for (; i < markers.size(); i++) {
      // This is an APP marker.
      if ((markers[i] & 0xF0) == 0xE0) {
        JXL_CHECK(app_markers < jpeg_data.app_data.size());
        // APP14 marker
        if (markers[i] == 0xEE) {
          const auto& data = jpeg_data.app_data[app_markers];
          if (data.size() == 15 && data[3] == 'A' && data[4] == 'd' &&
              data[5] == 'o' && data[6] == 'b' && data[7] == 'e') {
            // 'Adobe' marker.
            is_rgb = data[14] == 0;
            break;
          }
        }
        app_markers++;
      }
    }

    if (i == markers.size()) {
      // No 'Adobe' marker, guess from component IDs.
      is_rgb = nbcomp == 3 && jpeg_data.components[0].id == 'R' &&
               jpeg_data.components[1].id == 'G' &&
               jpeg_data.components[2].id == 'B';
    }
  }
  return (!is_rgb || nbcomp == 1);
}

inline std::array<int, 3> JpegOrder(bool is_ycbcr, bool is_gray) {
  if (is_gray) {
    return {{0, 0, 0}};
  } else if (is_ycbcr) {
    return {{1, 0, 2}};
  } else {
    return {{0, 1, 2}};
  }
}

void SetDequantWeightsFromJpegData(const jpeg::JPEGData& jpeg_data,
                                   const bool is_ycbcr, float* dequant) {
  auto jpeg_c_map = JpegOrder(is_ycbcr, jpeg_data.components.size() == 1);
  const float kDequantScale = 1.0f / (8 * 255);
  for (size_t c = 0; c < 3; c++) {
    size_t jpeg_c = jpeg_c_map[c];
    const int32_t* quant =
        jpeg_data.quant[jpeg_data.components[jpeg_c].quant_idx].values.data();
    for (size_t k = 0; k < kDCTBlockSize; ++k) {
      dequant[c * kDCTBlockSize + k] = quant[k] * kDequantScale;
    }
  }
}

void SetCoefficientsFromJpegData(const jpeg::JPEGData& jpeg_data,
                                 const FrameDimensions& frame_dim,
                                 const YCbCrChromaSubsampling& cs,
                                 const bool is_ycbcr, Image3S* coeffs) {
  auto jpeg_c_map = JpegOrder(is_ycbcr, jpeg_data.components.size() == 1);
  *coeffs = Image3S(kGroupDim * kGroupDim, frame_dim.num_groups);
  for (size_t c = 0; c < 3; ++c) {
    if (jpeg_data.components.size() == 1 && c != 1) {
      ZeroFillImage(&coeffs->Plane(c));
      continue;
    }
    const auto& comp = jpeg_data.components[jpeg_c_map[c]];
    size_t hshift = cs.HShift(c);
    size_t vshift = cs.VShift(c);
    int dcquant = jpeg_data.quant[comp.quant_idx].values.data()[0];
    int16_t dc_level = 1024 / dcquant;
    size_t jpeg_stride = comp.width_in_blocks * kDCTBlockSize;
    for (size_t group_index = 0; group_index < frame_dim.num_groups;
         group_index++) {
      Rect block_rect = BlockGroupRect(frame_dim, group_index);
      size_t xsize_blocks = DivCeil(block_rect.xsize(), 1 << hshift);
      size_t ysize_blocks = DivCeil(block_rect.ysize(), 1 << vshift);
      size_t group_xsize = xsize_blocks * kDCTBlockSize;
      size_t bx0 = block_rect.x0() >> hshift;
      size_t by0 = block_rect.y0() >> vshift;
      size_t jpeg_offset = by0 * jpeg_stride + bx0 * kDCTBlockSize;
      const int16_t* JXL_RESTRICT jpeg_coeffs =
          comp.coeffs.data() + jpeg_offset;
      int16_t* JXL_RESTRICT coeff_row = coeffs->PlaneRow(c, group_index);
      for (size_t by = 0; by < ysize_blocks; ++by) {
        memcpy(&coeff_row[by * group_xsize], &jpeg_coeffs[by * jpeg_stride],
               group_xsize * sizeof(coeff_row[0]));
      }
      if (!is_ycbcr) {
        for (size_t offset = 0; offset < coeffs->xsize();
             offset += kDCTBlockSize) {
          coeff_row[offset] += dc_level;
        }
      }
    }
  }
}

std::unique_ptr<RenderPipeline> PreparePipeline(
    const YCbCrChromaSubsampling& cs, const bool is_ycbcr,
    const FrameDimensions& frame_dim, PackedImage* output) {
  RenderPipeline::Builder builder(3);
  if (!cs.Is444()) {
    for (size_t c = 0; c < 3; c++) {
      if (cs.HShift(c) != 0) {
        builder.AddStage(GetChromaUpsamplingStage(c, /*horizontal=*/true));
      }
      if (cs.VShift(c) != 0) {
        builder.AddStage(GetChromaUpsamplingStage(c, /*horizontal=*/false));
      }
    }
  }
  if (is_ycbcr) {
    builder.AddStage(GetYCbCrStage());
  }
  ImageOutput main_output;
  main_output.format = output->format;
  main_output.bits_per_sample =
      PackedImage::BitsPerChannel(output->format.data_type);
  main_output.buffer = reinterpret_cast<uint8_t*>(output->pixels());
  main_output.buffer_size = output->pixels_size;
  main_output.stride = output->stride;
  std::vector<ImageOutput> extra_output;
  builder.AddStage(GetWriteToOutputStage(
      main_output, output->xsize, output->ysize,
      /*has_alpha=*/false,
      /*unpremul_alpha=*/false,
      /*alpha_c=*/0, Orientation::kIdentity, extra_output));
  return std::move(builder).Finalize(frame_dim);
}

}  // namespace

Status DecodeJpeg(const std::vector<uint8_t>& compressed,
                  JxlDataType output_data_type, ThreadPool* pool,
                  PackedPixelFile* ppf) {
  jpeg::JPEGData jpeg_data;
  JXL_RETURN_IF_ERROR(jpeg::ReadJpeg(compressed.data(), compressed.size(),
                                     jpeg::JpegReadMode::kReadAll, &jpeg_data));
  const size_t xsize = jpeg_data.width;
  const size_t ysize = jpeg_data.height;
  const uint32_t nbcomp = jpeg_data.components.size();
  const bool is_ycbcr = IsYCbCrJpeg(jpeg_data);

  ppf->info.xsize = xsize;
  ppf->info.ysize = ysize;
  ppf->info.num_color_channels = nbcomp;
  ppf->info.bits_per_sample = PackedImage::BitsPerChannel(output_data_type);

  ColorEncoding color_encoding;
  JXL_RETURN_IF_ERROR(SetColorEncodingFromJpegData(jpeg_data, &color_encoding));
  PaddedBytes icc = color_encoding.ICC();
  ppf->icc.assign(icc.data(), icc.data() + icc.size());
  ConvertInternalToExternalColorEncoding(color_encoding, &ppf->color_encoding);

  YCbCrChromaSubsampling cs;
  JXL_RETURN_IF_ERROR(SetChromaSubsamplingFromJpegData(jpeg_data, &cs));

  FrameDimensions frame_dim;
  frame_dim.Set(xsize, ysize, /*group_size_shift=*/1, cs.MaxHShift(),
                cs.MaxVShift(),
                /*modular_mode=*/false, /*upsampling=*/1);

  std::vector<float> dequant(3 * kDCTBlockSize);
  SetDequantWeightsFromJpegData(jpeg_data, is_ycbcr, &dequant[0]);

  Image3S coeffs;
  SetCoefficientsFromJpegData(jpeg_data, frame_dim, cs, is_ycbcr, &coeffs);

  JxlPixelFormat format = {nbcomp, output_data_type, JXL_LITTLE_ENDIAN, 0};
  ppf->frames.emplace_back(xsize, ysize, format);
  auto& frame = ppf->frames.back();

  std::unique_ptr<RenderPipeline> render_pipeline =
      PreparePipeline(cs, is_ycbcr, frame_dim, &frame.color);
  JXL_RETURN_IF_ERROR(render_pipeline->IsInitialized());

  hwy::AlignedFreeUniquePtr<float[]> float_memory;
  const auto allocate_storage = [&](const size_t num_threads) -> Status {
    JXL_RETURN_IF_ERROR(
        render_pipeline->PrepareForThreads(num_threads,
                                           /*use_group_ids=*/false));
    float_memory = hwy::AllocateAligned<float>(kDCTBlockSize * 2 * num_threads);
    return true;
  };
  const auto process_group = [&](const uint32_t group_index,
                                 const size_t thread) {
    RenderPipelineInput input =
        render_pipeline->GetInputBuffers(group_index, thread);
    float* group_dec_cache = float_memory.get() + thread * kDCTBlockSize * 2;
    const Rect block_rect = BlockGroupRect(frame_dim, group_index);
    JXL_CHECK(DecodeGroupJpeg(coeffs, group_index, block_rect, cs, &dequant[0],
                              group_dec_cache, thread, input));
    input.Done();
  };
  JXL_CHECK(RunOnPool(pool, 0, frame_dim.num_groups, allocate_storage,
                      process_group, "Decode Groups"));

  return true;
}

}  // namespace extras
}  // namespace jxl
