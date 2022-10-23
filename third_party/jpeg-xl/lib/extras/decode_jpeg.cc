// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/decode_jpeg.h"

#include "lib/extras/dec_group_jpeg.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"
#include "lib/jxl/jpeg/enc_jpeg_data_reader.h"

namespace jxl {
namespace extras {

namespace {

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

// See the following article for the details:
// J. R. Price and M. Rabbani, "Dequantization bias for JPEG decompression"
// Proceedings International Conference on Information Technology: Coding and
// Computing (Cat. No.PR00540), 2000, pp. 30-35, doi: 10.1109/ITCC.2000.844179.
void ComputeOptimalLaplacianBiases(const int num_blocks, const int* nonzeros,
                                   const int* sumabs, float* biases) {
  for (size_t k = 1; k < kDCTBlockSize; ++k) {
    // Notation adapted from the article
    int N = num_blocks;
    int N1 = nonzeros[k];
    int N0 = num_blocks - N1;
    int S = sumabs[k];
    // Compute gamma from N0, N1, N, S (eq. 11), with A and B being just
    // temporary grouping of terms.
    float A = 4.0 * S + 2.0 * N;
    float B = 4.0 * S - 2.0 * N1;
    float gamma = (-1.0 * N0 + std::sqrt(N0 * N0 * 1.0 + A * B)) / A;
    float gamma2 = gamma * gamma;
    // The bias is computed from gamma with (eq. 5), where the quantization
    // multiplier Q can be factored out and thus the bias can be applied
    // directly on the quantized coefficient.
    biases[k] =
        0.5 * (((1.0 + gamma2) / (1.0 - gamma2)) + 1.0 / std::log(gamma));
  }
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

  int max_h_samp = 1;
  int max_v_samp = 1;
  for (size_t c = 0; c < nbcomp; ++c) {
    const auto& comp = jpeg_data.components[c];
    max_h_samp = std::max(comp.h_samp_factor, max_h_samp);
    max_v_samp = std::max(comp.v_samp_factor, max_v_samp);
  }
  JXL_RETURN_IF_ERROR(max_h_samp <= 2);
  JXL_RETURN_IF_ERROR(max_v_samp <= 2);

  const float kDequantScale = 1.0f / (8 * 255);
  std::vector<float> dequant(nbcomp * kDCTBlockSize);
  for (size_t c = 0; c < nbcomp; c++) {
    const auto& comp = jpeg_data.components[c];
    const int32_t* quant = jpeg_data.quant[comp.quant_idx].values.data();
    for (size_t k = 0; k < kDCTBlockSize; ++k) {
      dequant[c * kDCTBlockSize + k] = quant[k] * kDequantScale;
    }
  }

  JxlPixelFormat format = {nbcomp, output_data_type, JXL_LITTLE_ENDIAN, 0};
  ppf->frames.emplace_back(xsize, ysize, format);
  auto& frame = ppf->frames.back();

  // Padding for horizontal chroma upsampling.
  static constexpr size_t kPaddingLeft = CacheAligned::kAlignment;
  static constexpr size_t kPaddingRight = 1;

  size_t MCU_width = kBlockDim * max_h_samp;
  size_t MCU_height = kBlockDim * max_v_samp;
  size_t MCU_rows = DivCeil(ysize, MCU_height);
  size_t MCU_cols = DivCeil(xsize, MCU_width);
  size_t stride = MCU_cols * MCU_width + kPaddingLeft + kPaddingRight;
  Image3F MCU_row_buf(stride, MCU_height);
  size_t xsize_blocks = DivCeil(xsize, kBlockDim);

  // Temporary buffers for vertically upsampled chroma components. We keep a
  // ringbuffer of 3 * kBlockDim rows so that we have access for previous and
  // next rows.
  std::vector<ImageF> chroma;
  // In the rendering order, vertically upsampled chroma components come first.
  std::vector<size_t> component_order;
  for (size_t c = 0; c < nbcomp; ++c) {
    const auto& comp = jpeg_data.components[c];
    if (comp.v_samp_factor < max_v_samp) {
      component_order.emplace_back(c);
      chroma.emplace_back(ImageF(stride, 3 * kBlockDim));
    }
  }
  for (size_t c = 0; c < nbcomp; ++c) {
    const auto& comp = jpeg_data.components[c];
    if (comp.v_samp_factor == max_v_samp) {
      component_order.emplace_back(c);
    }
  }

  hwy::AlignedFreeUniquePtr<float[]> idct_scratch =
      hwy::AllocateAligned<float>(kDCTBlockSize * 2);
  hwy::AlignedFreeUniquePtr<float[]> upsample_scratch =
      hwy::AllocateAligned<float>(stride);

  constexpr size_t kTempOutputLen = 1024;
  size_t bytes_per_sample = ppf->info.bits_per_sample / 8;
  size_t bytes_per_pixel = nbcomp * bytes_per_sample;
  hwy::AlignedFreeUniquePtr<uint8_t[]> output_scratch =
      hwy::AllocateAligned<uint8_t>(bytes_per_pixel * kTempOutputLen);

  // Per channel and per frequency statistics about the number of nonzeros and
  // the sum of coefficient absolute values, used in dequantization bias
  // computation.
  std::vector<int> nonzeros(nbcomp * kDCTBlockSize);
  std::vector<int> sumabs(nbcomp * kDCTBlockSize);
  std::vector<size_t> num_processed_blocks(nbcomp);
  std::vector<float> biases(nbcomp * kDCTBlockSize);

  for (size_t mcu_y = 0; mcu_y <= MCU_rows; mcu_y++) {
    for (size_t ci = 0; ci < nbcomp; ++ci) {
      size_t c = component_order[ci];
      size_t k0 = c * kDCTBlockSize;
      auto& comp = jpeg_data.components[c];
      bool hups = comp.h_samp_factor < max_h_samp;
      bool vups = comp.v_samp_factor < max_v_samp;
      size_t nblocks_y = comp.v_samp_factor;
      ImageF* output = vups ? &chroma[ci] : &MCU_row_buf.Plane(c);
      size_t mcu_y0 = vups ? (mcu_y * kBlockDim) % output->ysize() : 0;
      if (ci == chroma.size() && mcu_y > 0) {
        // For the previous MCU row we have everything we need at this point,
        // including the chroma components for the current MCU row that was used
        // in upsampling, so we can do the color conversion and the interleaved
        // output.
        for (size_t y = 0; y < MCU_height; ++y) {
          float* rows[3];
          for (size_t c = 0; c < nbcomp; ++c) {
            rows[c] = MCU_row_buf.PlaneRow(c, y) + kPaddingLeft;
          }
          if (is_ycbcr && nbcomp == 3) {
            YCbCrToRGB(rows[0], rows[1], rows[2], xsize_blocks * kBlockDim);
          } else {
            for (size_t c = 0; c < nbcomp; ++c) {
              // Libjpeg encoder converts all unsigned input values to signed
              // ones, i.e. for 8 bit input from [0..255] to [-128..127]. For
              // YCbCr jpegs this is undone in the YCbCr -> RGB conversion above
              // by adding 128 to Y channel, but for grayscale and RGB jpegs we
              // need to undo it here channel by channel.
              DecenterRow(rows[c], xsize_blocks * kBlockDim);
            }
          }
          size_t y0 = (mcu_y - 1) * MCU_height + y;
          if (y0 >= ysize) continue;
          for (size_t x0 = 0; x0 < xsize; x0 += kTempOutputLen) {
            size_t len = std::min(xsize - x0, kTempOutputLen);
            WriteToPackedImage(rows, x0, y0, len, output_scratch.get(),
                               &frame.color);
          }
        }
      }
      if (mcu_y < MCU_rows) {
        if (!hups && !vups) {
          size_t num_coeffs = comp.width_in_blocks * kDCTBlockSize;
          size_t offset = mcu_y * comp.width_in_blocks * kDCTBlockSize;
          // Update statistics for this MCU row.
          GatherBlockStats(&comp.coeffs[offset], num_coeffs, &nonzeros[k0],
                           &sumabs[k0]);
          num_processed_blocks[c] += comp.width_in_blocks;
          if (mcu_y % 4 == 3) {
            // Re-compute optimal biases every few MCU-rows.
            ComputeOptimalLaplacianBiases(num_processed_blocks[c],
                                          &nonzeros[k0], &sumabs[k0],
                                          &biases[k0]);
          }
        }
        for (size_t iy = 0; iy < nblocks_y; ++iy) {
          size_t by = mcu_y * nblocks_y + iy;
          size_t y0 = mcu_y0 + iy * kBlockDim;
          int16_t* JXL_RESTRICT row_in =
              &comp.coeffs[by * comp.width_in_blocks * kDCTBlockSize];
          float* JXL_RESTRICT row_out = output->Row(y0) + kPaddingLeft;
          for (size_t bx = 0; bx < comp.width_in_blocks; ++bx) {
            DecodeJpegBlock(&row_in[bx * kDCTBlockSize], &dequant[k0],
                            &biases[k0], idct_scratch.get(),
                            &row_out[bx * kBlockDim], output->PixelsPerRow());
          }
          if (hups) {
            for (size_t y = 0; y < kBlockDim; ++y) {
              float* JXL_RESTRICT row = output->Row(y0 + y) + kPaddingLeft;
              Upsample2Horizontal(row, upsample_scratch.get(),
                                  xsize_blocks * kBlockDim);
              memcpy(row, upsample_scratch.get(),
                     xsize_blocks * kBlockDim * sizeof(row[0]));
            }
          }
        }
      }
      if (vups) {
        auto y_idx = [&](size_t mcu_y, ssize_t y) {
          return (output->ysize() + mcu_y * kBlockDim + y) % output->ysize();
        };
        if (mcu_y == 0) {
          // Copy the first row of the current MCU row to the last row of the
          // previous one.
          memcpy(output->Row(y_idx(mcu_y, -1)), output->Row(y_idx(mcu_y, 0)),
                 output->PixelsPerRow() * sizeof(output->Row(0)[0]));
        }
        if (mcu_y == MCU_rows) {
          // Copy the last row of the current MCU row to the  first row of the
          // next  one.
          memcpy(output->Row(y_idx(mcu_y + 1, 0)),
                 output->Row(y_idx(mcu_y, kBlockDim - 1)),
                 output->PixelsPerRow() * sizeof(output->Row(0)[0]));
        }
        if (mcu_y > 0) {
          for (size_t y = 0; y < kBlockDim; ++y) {
            size_t y_top = y_idx(mcu_y - 1, y - 1);
            size_t y_cur = y_idx(mcu_y - 1, y);
            size_t y_bot = y_idx(mcu_y - 1, y + 1);
            size_t y_out0 = 2 * y;
            size_t y_out1 = 2 * y + 1;
            Upsample2Vertical(output->Row(y_top) + kPaddingLeft,
                              output->Row(y_cur) + kPaddingLeft,
                              output->Row(y_bot) + kPaddingLeft,
                              MCU_row_buf.PlaneRow(c, y_out0) + kPaddingLeft,
                              MCU_row_buf.PlaneRow(c, y_out1) + kPaddingLeft,
                              xsize_blocks * kBlockDim);
          }
        }
      }
    }
  }
  return true;
}

}  // namespace extras
}  // namespace jxl
