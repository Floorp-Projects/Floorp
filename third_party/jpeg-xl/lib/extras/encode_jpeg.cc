// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/encode_jpeg.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/encode_jpeg.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_adaptive_quantization.h"
#include "lib/jxl/enc_cluster.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_quant_weights.h"
#include "lib/jxl/enc_transforms.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/huffman_tree.h"
#include "lib/jxl/image.h"
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"
#include "lib/jxl/jpeg/jpeg_data.h"
#include "lib/jxl/opsin_params.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace extras {
namespace HWY_NAMESPACE {

void ComputeDCTCoefficients(const Image3F& opsin, const ImageF& qf,
                            const FrameDimensions& frame_dim, const float* qm,
                            std::vector<jpeg::JPEGComponent>* components) {
  float qfmin, qfmax;
  ImageMinMax(qf, &qfmin, &qfmax);
  HWY_ALIGN float scratch_space[2 * kDCTBlockSize];
  for (size_t c = 0; c < 3; c++) {
    std::vector<jpeg::coeff_t>& coeffs = (*components)[c].coeffs;
    size_t num_blocks = frame_dim.xsize_blocks * frame_dim.ysize_blocks;
    coeffs.resize(num_blocks * kDCTBlockSize);
    const float* qmc = &qm[c * kDCTBlockSize];
    for (size_t by = 0, bix = 0; by < frame_dim.ysize_blocks; by++) {
      for (size_t bx = 0; bx < frame_dim.xsize_blocks; bx++, bix++) {
        HWY_ALIGN float dct[kDCTBlockSize];
        TransformFromPixels(AcStrategy::Type::DCT,
                            opsin.PlaneRow(c, 8 * by) + 8 * bx,
                            opsin.PixelsPerRow(), dct, scratch_space);
        jpeg::coeff_t* block = &coeffs[bix * kDCTBlockSize];
        for (size_t iy = 0, i = 0; iy < 8; iy++) {
          for (size_t ix = 0; ix < 8; ix++, i++) {
            float coeff = 2040 * dct[i] * qmc[i];
            // Create more zeros in areas where jpeg xl would have used a lower
            // quantization multiplier.
            float zero_bias = 0.5f * qfmax / qf.Row(by)[bx];
            int cc = std::abs(coeff) < zero_bias ? 0 : std::round(coeff);
            // If the relative value of the adaptive quantization field is less
            // than 0.5, we drop the least significant bit.
            if (zero_bias > 1) {
              cc = cc / 2 * 2;
            }
            block[ix * 8 + iy] = cc;
          }
        }
        block[0] = std::round((2040 * dct[0] - 1024) * qmc[0]);
      }
    }
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace extras
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
namespace extras {

HWY_EXPORT(ComputeDCTCoefficients);

namespace {

std::vector<uint8_t> CreateXybICCAppMarker() {
  ColorEncoding c_xyb;
  c_xyb.SetColorSpace(ColorSpace::kXYB);
  c_xyb.rendering_intent = RenderingIntent::kPerceptual;
  JXL_CHECK(c_xyb.CreateICC());
  const auto& icc = c_xyb.ICC();
  std::vector<uint8_t> icc_marker(17 + icc.size());
  // See the APP2 marker format for embedded ICC profile at
  // https://www.color.org/technotes/ICC-Technote-ProfileEmbedding.pdf
  icc_marker[0] = 0xe2;  // APP2 marker
  // ICC marker size (excluding the marker bytes).
  icc_marker[1] = (icc_marker.size() - 1) >> 8;
  icc_marker[2] = (icc_marker.size() - 1) & 0xFF;
  // Byte sequence identifying an APP2 marker containing an icc profile.
  memcpy(&icc_marker[3], "ICC_PROFILE", 12);
  icc_marker[15] = 1;  // Sequence number
  icc_marker[16] = 1;  // Number of chunks.
  memcpy(&icc_marker[17], icc.data(), icc.size());
  return icc_marker;
}

void AddJpegQuantMatrices(const ImageF& qf, float dc_quant,
                          std::vector<jpeg::JPEGQuantTable>* quant_tables,
                          float* qm) {
  // Create a custom JPEG XL dequant matrix. The quantization weight parameters
  // were determined with manual tweaking.
  DequantMatrices dequant;
  std::vector<QuantEncoding> encodings(DequantMatrices::kNum,
                                       QuantEncoding::Library(0));
  encodings[0] = QuantEncoding::DCT(
      DctQuantWeightParams({{{{2000.0, -0.5, -0.5, -0.5, -0.5, -0.5}},
                             {{500.0, -0.1, -0.1, -0.2, -0.2, -0.2}},
                             {{200.0, -1.0, -0.5, -0.5, -0.5, -0.5}}}},
                           6));
  dequant.SetEncodings(encodings);
  JXL_CHECK(dequant.EnsureComputed(1));
  memcpy(qm, dequant.Matrix(0, 0), 3 * kDCTBlockSize * sizeof(qm[0]));
  // Set custom DC quant weights.
  const float inv_dc_quant[3] = {3200.0f, 512.0f, 320.0f};
  for (size_t c = 0; c < 3; c++) {
    qm[c * kDCTBlockSize] = 1.0f / (inv_dc_quant[c] * dc_quant);
  }

  // Scale the quant matrix based on the scaled XYB scales and the quant field.
  float qfmin, qfmax;
  ImageMinMax(qf, &qfmin, &qfmax);
  const float global_scale = 0.66f;
  for (size_t c = 0; c < 3; c++) {
    const float scale = kScaledXYBScale[c] * global_scale;
    qm[c * kDCTBlockSize] *= scale;
    for (size_t j = 1; j < kDCTBlockSize; j++) {
      qm[c * kDCTBlockSize + j] *= scale / qfmax;
    }
  }

  // Save the quant matrix into the jpeg data and invert it.
  quant_tables->resize(3);
  for (size_t c = 0; c < 3; c++) {
    jpeg::JPEGQuantTable& quant = (*quant_tables)[c];
    quant.is_last = (c == 2);
    quant.index = c + 1;
    for (size_t j = 0; j < kDCTBlockSize; j++) {
      int qval = std::round(qm[c * kDCTBlockSize + j] * 255 * 8);
      quant.values[j] = std::max(1, std::min(qval, 255));
      qm[c * kDCTBlockSize + j] = 1.0f / quant.values[j];
    }
  }
}

struct ProgressiveScan {
  int Ss, Se, Ah, Al;
  bool interleaved;
};

void AddJpegScanInfos(const std::vector<ProgressiveScan>& scans,
                      std::vector<jpeg::JPEGScanInfo>* scan_infos) {
  for (const auto& scan : scans) {
    jpeg::JPEGScanInfo si;
    si.Ss = scan.Ss;
    si.Se = scan.Se;
    si.Ah = scan.Ah;
    si.Al = scan.Al;
    if (scan.interleaved) {
      si.num_components = 3;
      for (uint32_t c = 0; c < 3; ++c) {
        si.components[c].comp_idx = c;
      }
      scan_infos->push_back(si);
    } else {
      for (uint32_t c = 0; c < 3; ++c) {
        si.num_components = 1;
        si.components[0].comp_idx = c;
        scan_infos->push_back(si);
      }
    }
  }
}

void ClusterJpegHistograms(const std::vector<jpeg::HuffmanCodeTable>& jpeg_in,
                           size_t max_histograms, std::vector<Histogram>* out,
                           std::vector<uint32_t>* histogram_indexes) {
  std::vector<Histogram> histograms;
  for (const auto& t : jpeg_in) {
    Histogram histo;
    histo.data_.resize(jpeg::kJpegHuffmanAlphabetSize);
    for (size_t i = 0; i < histo.data_.size(); ++i) {
      histo.data_[i] = t.depth[i];
      histo.total_count_ += histo.data_[i];
    }
    histograms.push_back(histo);
  }
  // TODO(szabadka): Use a JPEG-specific version of the clustering algorithm.
  HistogramParams params;
  ClusterHistograms(params, histograms, max_histograms, out, histogram_indexes);
}

void BuildJpegHuffmanCode(const uint8_t* depth, jpeg::JPEGHuffmanCode* huff) {
  std::fill(std::begin(huff->counts), std::end(huff->counts), 0);
  std::fill(std::begin(huff->values), std::end(huff->values), 0);
  for (size_t i = 0; i <= jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depth[i] > 0) {
      ++huff->counts[depth[i]];
    }
  }
  int offset[jpeg::kJpegHuffmanMaxBitLength + 1] = {0};
  for (size_t i = 1; i <= jpeg::kJpegHuffmanMaxBitLength; ++i) {
    offset[i] = offset[i - 1] + huff->counts[i - 1];
  }
  for (size_t i = 0; i <= jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depth[i] > 0) {
      huff->values[offset[depth[i]]++] = i;
    }
  }
  huff->is_last = false;
}

void AddJpegHuffmanCodes(std::vector<Histogram>& histograms,
                         size_t slot_id_offset,
                         std::vector<jpeg::JPEGHuffmanCode>* huff_codes) {
  for (size_t i = 0; i < histograms.size(); ++i) {
    jpeg::JPEGHuffmanCode huff_code;
    huff_code.slot_id = slot_id_offset + i;
    std::vector<uint32_t> counts(jpeg::kJpegHuffmanAlphabetSize + 1);
    std::vector<uint8_t> depths(jpeg::kJpegHuffmanAlphabetSize + 1);
    for (size_t j = 0; j < jpeg::kJpegHuffmanAlphabetSize; ++j) {
      counts[j] = histograms[i].data_[j];
    }
    counts[jpeg::kJpegHuffmanAlphabetSize] = 1;
    CreateHuffmanTree(counts.data(), counts.size(),
                      jpeg::kJpegHuffmanMaxBitLength, &depths[0]);
    BuildJpegHuffmanCode(&depths[0], &huff_code);
    huff_codes->emplace_back(std::move(huff_code));
  }
}

void FillJPEGData(const Image3F& opsin, const ImageF& qf, float dc_quant,
                  const FrameDimensions& frame_dim, jpeg::JPEGData* out) {
  // ICC
  out->marker_order.push_back(0xe2);
  out->app_data.push_back(CreateXybICCAppMarker());

  // DQT
  out->marker_order.emplace_back(0xdb);
  float qm[3 * kDCTBlockSize];
  AddJpegQuantMatrices(qf, dc_quant, &out->quant, qm);

  // SOF
  out->marker_order.emplace_back(0xc2);
  out->components.resize(3);
  out->height = frame_dim.ysize;
  out->width = frame_dim.xsize;
  out->components[0].id = 'R';
  out->components[1].id = 'G';
  out->components[2].id = 'B';
  for (size_t c = 0; c < 3; ++c) {
    out->components[c].h_samp_factor = 1;
    out->components[c].v_samp_factor = 1;
    out->components[c].width_in_blocks = frame_dim.xsize_blocks;
    out->components[c].height_in_blocks = frame_dim.ysize_blocks;
    out->components[c].quant_idx = c;
  }
  HWY_DYNAMIC_DISPATCH(ComputeDCTCoefficients)
  (opsin, qf, frame_dim, qm, &out->components);

  // DHT (the actual Huffman codes will be added later).
  out->marker_order.emplace_back(0xc4);

  // SOS
  std::vector<ProgressiveScan> progressive_mode = {
      // DC
      {0, 0, 0, 0, true},
      // AC 1 - highest bits
      {1, 63, 0, 1, false},
      // AC 2 - lowest bit
      {1, 63, 1, 0, false},
  };
  AddJpegScanInfos(progressive_mode, &out->scan_info);
  for (size_t i = 0; i < out->scan_info.size(); i++) {
    out->marker_order.emplace_back(0xda);
  }

  // EOI
  out->marker_order.push_back(0xd9);

  // Gather histograms.
  jpeg::SerializationState ss;
  JXL_CHECK(jpeg::ProcessJpeg(*out, &ss));

  // Build DC Huffman codes and add them to DHT segment.
  std::vector<Histogram> dc_histo;
  std::vector<uint32_t> dc_tbl_indexes;
  ClusterJpegHistograms(ss.dc_huff_table, 4, &dc_histo, &dc_tbl_indexes);
  AddJpegHuffmanCodes(dc_histo, 0, &out->huffman_code);

  // Build AC Huffman codes and add them to DHT segment.
  std::vector<Histogram> ac_histo;
  std::vector<uint32_t> ac_tbl_indexes;
  ClusterJpegHistograms(ss.ac_huff_table, 4, &ac_histo, &ac_tbl_indexes);
  AddJpegHuffmanCodes(ac_histo, 0x10, &out->huffman_code);
  out->huffman_code.back().is_last = true;

  // Set the Huffman table indexes in the scan_infos.
  size_t histo_idx = 0;
  for (auto& si : out->scan_info) {
    for (size_t c = 0; c < si.num_components; ++c) {
      JXL_ASSERT(histo_idx < dc_tbl_indexes.size());
      JXL_ASSERT(histo_idx < ac_tbl_indexes.size());
      si.components[c].dc_tbl_idx = dc_tbl_indexes[histo_idx];
      si.components[c].ac_tbl_idx = ac_tbl_indexes[histo_idx];
      ++histo_idx;
    }
  }
}

}  // namespace

Status EncodeJpeg(const ImageBundle& input, float distance, ThreadPool* pool,
                  std::vector<uint8_t>* compressed) {
  FrameDimensions frame_dim;
  frame_dim.Set(input.xsize(), input.ysize(), 1, 0, 0, false, 1);

  // Convert input to XYB colorspace.
  Image3F opsin(frame_dim.xsize_padded, frame_dim.ysize_padded);
  opsin.ShrinkTo(frame_dim.xsize, frame_dim.ysize);
  ToXYB(input, pool, &opsin, GetJxlCms());
  PadImageToBlockMultipleInPlace(&opsin);

  // Compute adaptive quant field.
  ImageF mask;
  ImageF qf = InitialQuantField(distance, opsin, frame_dim, pool, 1.0, &mask);
  ScaleXYB(&opsin);

  // Create jpeg data and optimize Huffman codes.
  jpeg::JPEGData jpeg_data;
  FillJPEGData(opsin, qf, InitialQuantDC(distance), frame_dim, &jpeg_data);

  // Write jpeg data to compressed stream.
  auto write = [&compressed](const uint8_t* buf, size_t len) {
    compressed->insert(compressed->end(), buf, buf + len);
    return len;
  };
  return jpeg::WriteJpeg(jpeg_data, write);
}

}  // namespace extras
}  // namespace jxl
#endif  // HWY_ONCE
