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

void ComputeDCTCoefficients(const Image3F& opsin, const bool xyb,
                            const ImageF& qf, const FrameDimensions& frame_dim,
                            const float* qm,
                            std::vector<jpeg::JPEGComponent>* components) {
  int max_samp_factor = 1;
  for (const auto& c : *components) {
    JXL_DASSERT(c.h_samp_factor == c.v_samp_factor);
    max_samp_factor = std::max(c.h_samp_factor, max_samp_factor);
  }
  float qfmin, qfmax;
  ImageMinMax(qf, &qfmin, &qfmax);
  HWY_ALIGN float scratch_space[2 * kDCTBlockSize];
  ImageF tmp;
  for (size_t c = 0; c < 3; c++) {
    auto& comp = (*components)[c];
    const size_t xsize_blocks = comp.width_in_blocks;
    const size_t ysize_blocks = comp.height_in_blocks;
    JXL_DASSERT(max_samp_factor % comp.h_samp_factor == 0);
    const int factor = max_samp_factor / comp.h_samp_factor;
    const ImageF* plane = &opsin.Plane(c);
    if (factor > 1) {
      tmp = CopyImage(*plane);
      DownsampleImage(&tmp, factor);
      plane = &tmp;
    }
    std::vector<jpeg::coeff_t>& coeffs = comp.coeffs;
    coeffs.resize(xsize_blocks * ysize_blocks * kDCTBlockSize);
    const float* qmc = &qm[c * kDCTBlockSize];
    for (size_t by = 0, bix = 0; by < ysize_blocks; by++) {
      for (size_t bx = 0; bx < xsize_blocks; bx++, bix++) {
        jpeg::coeff_t* block = &coeffs[bix * kDCTBlockSize];
        HWY_ALIGN float dct[kDCTBlockSize];
        TransformFromPixels(AcStrategy::Type::DCT, plane->Row(8 * by) + 8 * bx,
                            plane->PixelsPerRow(), dct, scratch_space);
        for (size_t iy = 0, i = 0; iy < 8; iy++) {
          for (size_t ix = 0; ix < 8; ix++, i++) {
            float coeff = 2040 * dct[i] * qmc[i];
            // Create more zeros in areas where jpeg xl would have used a lower
            // quantization multiplier.
            float zero_bias = 0.5f * qfmax / qf.Row(by * factor)[bx * factor];
            int cc = std::abs(coeff) < zero_bias ? 0 : std::round(coeff);
            // If the relative value of the adaptive quantization field is less
            // than 0.5, we drop the least significant bit.
            if (zero_bias > 1) {
              cc = cc / 2 * 2;
            }
            block[ix * 8 + iy] = cc;
          }
        }
        if (xyb) {
          // ToXYB does not create zero-centered sample values like RgbToYcbcr
          // does, so we apply an offset to the DC values instead.
          block[0] = std::round((2040 * dct[0] - 1024) * qmc[0]);
        }
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

std::vector<uint8_t> CreateICCAppMarker(const PaddedBytes& icc) {
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

std::vector<uint8_t> CreateXybICCAppMarker() {
  ColorEncoding c_xyb;
  c_xyb.SetColorSpace(ColorSpace::kXYB);
  c_xyb.rendering_intent = RenderingIntent::kPerceptual;
  JXL_CHECK(c_xyb.CreateICC());
  return CreateICCAppMarker(c_xyb.ICC());
}

static constexpr float kBaseQuantMatrixXYB[] = {
    // c = 0
    0.010745695802f,
    0.014724285860f,
    0.016765073259f,
    0.015352546818f,
    0.016849715608f,
    0.017505664513f,
    0.019171796023f,
    0.026983627671f,
    0.014724285860f,
    0.016005879113f,
    0.014807802023f,
    0.015257294568f,
    0.016239266522f,
    0.017754112611f,
    0.021007430943f,
    0.024258001854f,
    0.016765073259f,
    0.014807802023f,
    0.016266879484f,
    0.014202573480f,
    0.016155362246f,
    0.018324768181f,
    0.018883664957f,
    0.024261275157f,
    0.015352546818f,
    0.015257294568f,
    0.014202573480f,
    0.014974020066f,
    0.018844302744f,
    0.019286162437f,
    0.023009874591f,
    0.023277331489f,
    0.016849715608f,
    0.016239266522f,
    0.016155362246f,
    0.018844302744f,
    0.019491371738f,
    0.030153905190f,
    0.032131952026f,
    0.047015070993f,
    0.017505664513f,
    0.017754112611f,
    0.018324768181f,
    0.019286162437f,
    0.030153905190f,
    0.035875428738f,
    0.025324149774f,
    0.046037739693f,
    0.019171796023f,
    0.021007430943f,
    0.018883664957f,
    0.023009874591f,
    0.032131952026f,
    0.025324149774f,
    0.025619236945f,
    0.049740249957f,
    0.026983627671f,
    0.024258001854f,
    0.024261275157f,
    0.023277331489f,
    0.047015070993f,
    0.046037739693f,
    0.049740249957f,
    0.029683058303f,
    // c = 1
    0.002310547025f,
    0.002391506241f,
    0.002592377991f,
    0.002907631930f,
    0.003590107614f,
    0.003647418579f,
    0.003946607583f,
    0.004580867024f,
    0.002391506241f,
    0.002565945978f,
    0.002676532241f,
    0.003167916799f,
    0.003592423110f,
    0.003581864537f,
    0.004168190188f,
    0.004711190832f,
    0.002592377991f,
    0.002676532241f,
    0.002899922325f,
    0.003221508920f,
    0.003597377805f,
    0.004015001363f,
    0.004164168214f,
    0.004536180462f,
    0.002907631930f,
    0.003167916799f,
    0.003221508920f,
    0.003421333194f,
    0.003843692347f,
    0.004011729362f,
    0.004486022354f,
    0.005037524314f,
    0.003590107614f,
    0.003592423110f,
    0.003597377805f,
    0.003843692347f,
    0.003991982168f,
    0.004561113887f,
    0.005683994831f,
    0.005587879717f,
    0.003647418579f,
    0.003581864537f,
    0.004015001363f,
    0.004011729362f,
    0.004561113887f,
    0.004711190832f,
    0.005279489671f,
    0.005645298559f,
    0.003946607583f,
    0.004168190188f,
    0.004164168214f,
    0.004486022354f,
    0.005683994831f,
    0.005279489671f,
    0.005269099460f,
    0.005234577959f,
    0.004580867024f,
    0.004711190832f,
    0.004536180462f,
    0.005037524314f,
    0.005587879717f,
    0.005645298559f,
    0.005234577959f,
    0.005138602544f,
    // c = 2
    0.004694191704f,
    0.007478405841f,
    0.009119519544f,
    0.010846788859f,
    0.012040055008f,
    0.014283609506f,
    0.020805819128f,
    0.041346026531f,
    0.007478405841f,
    0.008473337032f,
    0.008457467755f,
    0.011507290737f,
    0.012282006381f,
    0.011077942494f,
    0.019589180487f,
    0.030348661601f,
    0.009119519544f,
    0.008457467755f,
    0.012692131754f,
    0.010360988009f,
    0.011883779193f,
    0.021216622915f,
    0.019468523508f,
    0.022375231013f,
    0.010846788859f,
    0.011507290737f,
    0.010360988009f,
    0.015688875916f,
    0.019428087454f,
    0.018982414995f,
    0.030218311113f,
    0.025108166811f,
    0.012040055008f,
    0.012282006381f,
    0.011883779193f,
    0.019428087454f,
    0.019908111501f,
    0.019428676375f,
    0.026540699320f,
    0.032446303017f,
    0.014283609506f,
    0.011077942494f,
    0.021216622915f,
    0.018982414995f,
    0.019428676375f,
    0.025654942665f,
    0.030689090332f,
    0.036234971093f,
    0.020805819128f,
    0.019589180487f,
    0.019468523508f,
    0.030218311113f,
    0.026540699320f,
    0.030689090332f,
    0.035378000966f,
    0.041109510150f,
    0.041346026531f,
    0.030348661601f,
    0.022375231013f,
    0.025108166811f,
    0.032446303017f,
    0.036234971093f,
    0.041109510150f,
    0.047241950370f,
};

// Y: mozjpeg q99; Cb, Cr: mozjpeg q95
static constexpr float kBaseQuantMatrixYCbCr[] = {
    // c = 0
    1, 1, 1, 1, 1, 1, 1, 2,  //
    1, 1, 1, 1, 1, 1, 1, 2,  //
    1, 1, 1, 1, 1, 1, 2, 3,  //
    1, 1, 1, 1, 1, 1, 2, 3,  //
    1, 1, 1, 1, 1, 2, 3, 4,  //
    1, 1, 1, 1, 2, 2, 3, 5,  //
    1, 1, 2, 2, 3, 3, 5, 6,  //
    2, 2, 3, 3, 4, 5, 6, 8,  //

    // c = 1
    2, 2, 2, 2, 3, 4, 6, 9,        //
    2, 2, 2, 3, 3, 4, 5, 8,        //
    2, 2, 2, 3, 4, 6, 9, 14,       //
    2, 3, 3, 4, 5, 7, 11, 16,      //
    3, 3, 4, 5, 7, 9, 13, 19,      //
    4, 4, 6, 7, 9, 12, 17, 24,     //
    6, 5, 9, 11, 13, 17, 23, 31,   //
    9, 8, 14, 16, 19, 24, 31, 42,  //

    // c = 2
    2, 2, 2, 2, 3, 4, 6, 9,        //
    2, 2, 2, 3, 3, 4, 5, 8,        //
    2, 2, 2, 3, 4, 6, 9, 14,       //
    2, 3, 3, 4, 5, 7, 11, 16,      //
    3, 3, 4, 5, 7, 9, 13, 19,      //
    4, 4, 6, 7, 9, 12, 17, 24,     //
    6, 5, 9, 11, 13, 17, 23, 31,   //
    9, 8, 14, 16, 19, 24, 31, 42,  //
};

void AddJpegQuantMatrices(const ImageF& qf, bool xyb, float dc_quant,
                          float global_scale,
                          std::vector<jpeg::JPEGQuantTable>* quant_tables,
                          float* qm) {
  const float* const base_quant_matrix =
      xyb ? kBaseQuantMatrixXYB : kBaseQuantMatrixYCbCr;
  // Scale the base quant matrix based on the scaled XYB scales and the quant
  // field.
  float qfmin, qfmax;
  ImageMinMax(qf, &qfmin, &qfmax);
  const float dc_scale = global_scale / dc_quant;
  const float ac_scale = global_scale / qfmax;
  for (size_t c = 0, ix = 0; c < 3; c++) {
    qm[ix] = dc_scale * base_quant_matrix[ix];
    ix++;
    for (size_t j = 1; j < kDCTBlockSize; j++, ix++) {
      qm[ix] = ac_scale * base_quant_matrix[ix];
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

float HistogramCost(const Histogram& histo) {
  std::vector<uint32_t> counts(jpeg::kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(jpeg::kJpegHuffmanAlphabetSize + 1);
  for (size_t i = 0; i < jpeg::kJpegHuffmanAlphabetSize; ++i) {
    counts[i] = histo.data_[i];
  }
  counts[jpeg::kJpegHuffmanAlphabetSize] = 1;
  CreateHuffmanTree(counts.data(), counts.size(),
                    jpeg::kJpegHuffmanMaxBitLength, &depths[0]);
  size_t header_bits = (1 + jpeg::kJpegHuffmanMaxBitLength) * 8;
  size_t data_bits = 0;
  for (size_t i = 0; i < jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      header_bits += 8;
      data_bits += counts[i] * depths[i];
    }
  }
  return header_bits + data_bits;
}

struct JpegClusteredHistograms {
  std::vector<Histogram> histograms;
  std::vector<uint32_t> histogram_indexes;
  std::vector<uint32_t> slot_ids;
};

void ClusterJpegHistograms(const std::vector<jpeg::HuffmanCodeTable>& jpeg_in,
                           JpegClusteredHistograms* clusters) {
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
  clusters->histogram_indexes.resize(histograms.size());
  std::vector<uint32_t> slot_histograms;
  std::vector<float> slot_costs;
  for (size_t i = 0; i < histograms.size(); ++i) {
    const Histogram& cur = histograms[i];
    if (cur.total_count_ == 0) {
      continue;
    }
    float best_cost = HistogramCost(cur);
    size_t best_slot = slot_histograms.size();
    for (size_t j = 0; j < slot_histograms.size(); ++j) {
      size_t prev_idx = slot_histograms[j];
      const Histogram& prev = clusters->histograms[prev_idx];
      Histogram combined;
      combined.AddHistogram(prev);
      combined.AddHistogram(cur);
      float combined_cost = HistogramCost(combined);
      float cost = combined_cost - slot_costs[j];
      if (cost < best_cost) {
        best_cost = cost;
        best_slot = j;
      }
    }
    if (best_slot == slot_histograms.size()) {
      // Create new histogram.
      size_t histogram_index = clusters->histograms.size();
      clusters->histograms.push_back(cur);
      clusters->histogram_indexes[i] = histogram_index;
      if (best_slot < 4) {
        // We have a free slot, so we put the new histogram there.
        slot_histograms.push_back(histogram_index);
        slot_costs.push_back(best_cost);
      } else {
        // TODO(szabadka) Find the best histogram to replce.
        best_slot = (clusters->slot_ids.back() + 1) % 4;
      }
      slot_histograms[best_slot] = histogram_index;
      slot_costs[best_slot] = best_cost;
      clusters->slot_ids.push_back(best_slot);
    } else {
      // Merge this histogram with a previous one.
      size_t histogram_index = slot_histograms[best_slot];
      clusters->histograms[histogram_index].AddHistogram(cur);
      clusters->histogram_indexes[i] = histogram_index;
      JXL_ASSERT(clusters->slot_ids[histogram_index] == best_slot);
      slot_costs[best_slot] += best_cost;
    }
  }
}

void BuildJpegHuffmanCode(const Histogram& histo, jpeg::JPEGHuffmanCode* huff) {
  std::vector<uint32_t> counts(jpeg::kJpegHuffmanAlphabetSize + 1);
  std::vector<uint8_t> depths(jpeg::kJpegHuffmanAlphabetSize + 1);
  for (size_t j = 0; j < jpeg::kJpegHuffmanAlphabetSize; ++j) {
    counts[j] = histo.data_[j];
  }
  counts[jpeg::kJpegHuffmanAlphabetSize] = 1;
  CreateHuffmanTree(counts.data(), counts.size(),
                    jpeg::kJpegHuffmanMaxBitLength, &depths[0]);
  std::fill(std::begin(huff->counts), std::end(huff->counts), 0);
  std::fill(std::begin(huff->values), std::end(huff->values), 0);
  for (size_t i = 0; i <= jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      ++huff->counts[depths[i]];
    }
  }
  int offset[jpeg::kJpegHuffmanMaxBitLength + 1] = {0};
  for (size_t i = 1; i <= jpeg::kJpegHuffmanMaxBitLength; ++i) {
    offset[i] = offset[i - 1] + huff->counts[i - 1];
  }
  for (size_t i = 0; i <= jpeg::kJpegHuffmanAlphabetSize; ++i) {
    if (depths[i] > 0) {
      huff->values[offset[depths[i]]++] = i;
    }
  }
  huff->is_last = false;
}

void AddJpegHuffmanCode(const Histogram& histogram, size_t slot_id,
                        std::vector<jpeg::JPEGHuffmanCode>* huff_codes) {
  jpeg::JPEGHuffmanCode huff_code;
  huff_code.slot_id = slot_id;
  BuildJpegHuffmanCode(histogram, &huff_code);
  huff_codes->emplace_back(std::move(huff_code));
}

void SetJpegHuffmanCode(const JpegClusteredHistograms& clusters,
                        size_t histogram_id, size_t slot_id_offset,
                        std::vector<uint32_t>& slot_histograms,
                        uint32_t* slot_id,
                        std::vector<jpeg::JPEGHuffmanCode>* huff_codes) {
  JXL_ASSERT(histogram_id < clusters.histogram_indexes.size());
  uint32_t histogram_index = clusters.histogram_indexes[histogram_id];
  *slot_id = clusters.slot_ids[histogram_index];
  if (slot_histograms[*slot_id] != histogram_index) {
    AddJpegHuffmanCode(clusters.histograms[histogram_index],
                       slot_id_offset + *slot_id, huff_codes);
    slot_histograms[*slot_id] = histogram_index;
  }
}

void FillJPEGData(const Image3F& opsin, const ImageF& qf, float dc_quant,
                  float global_scale, const bool xyb, const bool subsample_blue,
                  const PaddedBytes& icc, const FrameDimensions& frame_dim,
                  jpeg::JPEGData* out) {
  *out = jpeg::JPEGData();
  // ICC
  out->marker_order.push_back(0xe2);
  if (xyb) {
    out->app_data.push_back(CreateXybICCAppMarker());
  } else {
    out->app_data.push_back(CreateICCAppMarker(icc));
  }

  // DQT
  out->marker_order.emplace_back(0xdb);
  float qm[3 * kDCTBlockSize];
  AddJpegQuantMatrices(qf, xyb, dc_quant, global_scale, &out->quant, qm);

  // SOF
  out->marker_order.emplace_back(0xc2);
  out->components.resize(3);
  out->height = frame_dim.ysize;
  out->width = frame_dim.xsize;
  if (xyb) {
    out->components[0].id = 'R';
    out->components[1].id = 'G';
    out->components[2].id = 'B';
  } else {
    out->components[0].id = 1;
    out->components[1].id = 2;
    out->components[2].id = 3;
  }
  size_t max_samp_factor = subsample_blue ? 2 : 1;
  for (size_t c = 0; c < 3; ++c) {
    const size_t factor = (subsample_blue && c == 2) ? 2 : 1;
    out->components[c].h_samp_factor = max_samp_factor / factor;
    out->components[c].v_samp_factor = max_samp_factor / factor;
    JXL_ASSERT(frame_dim.xsize_blocks % factor == 0);
    JXL_ASSERT(frame_dim.ysize_blocks % factor == 0);
    out->components[c].width_in_blocks = frame_dim.xsize_blocks / factor;
    out->components[c].height_in_blocks = frame_dim.ysize_blocks / factor;
    out->components[c].quant_idx = c;
  }
  HWY_DYNAMIC_DISPATCH(ComputeDCTCoefficients)
  (opsin, xyb, qf, frame_dim, qm, &out->components);

  // DHT (the actual Huffman codes will be added later).
  out->marker_order.emplace_back(0xc4);

  // SOS
  std::vector<ProgressiveScan> progressive_mode = {
      // DC
      {0, 0, 0, 0, !subsample_blue}, {1, 2, 0, 0, false},  {3, 63, 0, 2, false},
      {3, 63, 2, 1, false},          {3, 63, 1, 0, false},
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

  // Cluster DC histograms.
  JpegClusteredHistograms dc_clusters;
  ClusterJpegHistograms(ss.dc_huff_table, &dc_clusters);

  // Cluster AC histograms.
  JpegClusteredHistograms ac_clusters;
  ClusterJpegHistograms(ss.ac_huff_table, &ac_clusters);

  // Rebuild marker_order because we may want to emit Huffman codes in between
  // scans.
  out->marker_order.resize(out->marker_order.size() - 2 -
                           out->scan_info.size());

  // Add the first 4 DC and AC histograms in the first DHT segment.
  out->marker_order.emplace_back(0xc4);  // DHT
  std::vector<uint32_t> dc_slot_histograms;
  std::vector<uint32_t> ac_slot_histograms;
  for (size_t i = 0; i < dc_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(dc_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(dc_clusters.histograms[i], i, &out->huffman_code);
    dc_slot_histograms.push_back(i);
  }
  for (size_t i = 0; i < ac_clusters.histograms.size(); ++i) {
    if (i >= 4) break;
    JXL_ASSERT(ac_clusters.slot_ids[i] == i);
    AddJpegHuffmanCode(ac_clusters.histograms[i], 0x10 + i, &out->huffman_code);
    ac_slot_histograms.push_back(i);
  }
  out->huffman_code.back().is_last = true;

  // Set the Huffman table indexes in the scan_infos and emit additional DHT
  // segments if necessary.
  size_t histogram_id = 0;
  for (auto& si : out->scan_info) {
    size_t num_huff_codes = out->huffman_code.size();
    for (size_t c = 0; c < si.num_components; ++c) {
      SetJpegHuffmanCode(dc_clusters, histogram_id, 0, dc_slot_histograms,
                         &si.components[c].dc_tbl_idx, &out->huffman_code);
      SetJpegHuffmanCode(ac_clusters, histogram_id, 0x10, ac_slot_histograms,
                         &si.components[c].ac_tbl_idx, &out->huffman_code);
      ++histogram_id;
    }
    // If we updated a slot histogram in this scan, create an additional DHT
    // segment.
    if (out->huffman_code.size() > num_huff_codes) {
      out->marker_order.emplace_back(0xc4);  // DHT
      out->huffman_code.back().is_last = true;
    }
    out->marker_order.emplace_back(0xda);  // SOS
  }
  out->marker_order.emplace_back(0xd9);  // EOI
}

size_t JpegSize(const jpeg::JPEGData& jpeg_data) {
  size_t total_size = 0;
  auto countsize = [&total_size](const uint8_t* buf, size_t len) {
    total_size += len;
    return len;
  };
  JXL_CHECK(jpeg::WriteJpeg(jpeg_data, countsize));
  return total_size;
}

}  // namespace

Status EncodeJpeg(const ImageBundle& input, const JpegSettings& jpeg_settings,
                  ThreadPool* pool, std::vector<uint8_t>* compressed) {
  const bool subsample_blue = jpeg_settings.xyb;
  const size_t max_shift = subsample_blue ? 1 : 0;
  FrameDimensions frame_dim;
  frame_dim.Set(input.xsize(), input.ysize(), 1, max_shift, max_shift, false,
                1);

  // Convert input to XYB colorspace.
  Image3F opsin(frame_dim.xsize_padded, frame_dim.ysize_padded);
  opsin.ShrinkTo(frame_dim.xsize, frame_dim.ysize);
  ToXYB(input, pool, &opsin, GetJxlCms());
  PadImageToBlockMultipleInPlace(&opsin, 8 << max_shift);

  // Compute adaptive quant field.
  ImageF mask;
  ImageF qf = InitialQuantField(jpeg_settings.distance, opsin, frame_dim, pool,
                                1.0, &mask);
  if (jpeg_settings.xyb) {
    ScaleXYB(&opsin);
  } else {
    opsin.ShrinkTo(input.xsize(), input.ysize());
    JXL_RETURN_IF_ERROR(RgbToYcbcr(
        input.color().Plane(0), input.color().Plane(1), input.color().Plane(2),
        &opsin.Plane(0), &opsin.Plane(1), &opsin.Plane(2), pool));
    PadImageToBlockMultipleInPlace(&opsin, 8 << max_shift);
  }

  // Create jpeg data and optimize Huffman codes.
  jpeg::JPEGData jpeg_data;
  float global_scale = 0.66f;
  if (!jpeg_settings.xyb) {
    global_scale /= 500;
    if (input.metadata()->color_encoding.tf.IsPQ()) {
      global_scale *= .4f;
    } else if (input.metadata()->color_encoding.tf.IsHLG()) {
      global_scale *= .5f;
    }
  }
  float dc_quant = InitialQuantDC(jpeg_settings.distance);
  FillJPEGData(opsin, qf, dc_quant, global_scale, jpeg_settings.xyb,
               subsample_blue, input.metadata()->color_encoding.ICC(),
               frame_dim, &jpeg_data);

  if (jpeg_settings.target_size != 0) {
    // Tweak the jpeg data so that the resulting compressed file is
    // approximately target_size long.
    size_t prev_size = 0;
    float best_error = 100.0f;
    float best_global_scale = global_scale;
    size_t iter = 0;
    for (;;) {
      size_t size = JpegSize(jpeg_data);
      float error = size * 1.0f / jpeg_settings.target_size - 1.0f;
      if (std::abs(error) < std::abs(best_error)) {
        best_error = error;
        best_global_scale = global_scale;
      }
      if (size == prev_size || std::abs(error) < 0.001f || iter >= 10) {
        break;
      }
      global_scale *= 1.0f + error;
      FillJPEGData(opsin, qf, dc_quant, global_scale, jpeg_settings.xyb,
                   subsample_blue, input.metadata()->color_encoding.ICC(),
                   frame_dim, &jpeg_data);
      prev_size = size;
      ++iter;
    }
    if (best_global_scale != global_scale) {
      FillJPEGData(opsin, qf, dc_quant, best_global_scale, jpeg_settings.xyb,
                   subsample_blue, input.metadata()->color_encoding.ICC(),
                   frame_dim, &jpeg_data);
    }
  }

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
