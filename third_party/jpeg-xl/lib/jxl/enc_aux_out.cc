// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_aux_out.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <numeric>  // accumulate
#include <sstream>

#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

const char* LayerName(size_t layer) {
  switch (layer) {
    case kLayerHeader:
      return "Headers";
    case kLayerTOC:
      return "TOC";
    case kLayerDictionary:
      return "Patches";
    case kLayerSplines:
      return "Splines";
    case kLayerNoise:
      return "Noise";
    case kLayerQuant:
      return "Quantizer";
    case kLayerModularTree:
      return "ModularTree";
    case kLayerModularGlobal:
      return "ModularGlobal";
    case kLayerDC:
      return "DC";
    case kLayerModularDcGroup:
      return "ModularDcGroup";
    case kLayerControlFields:
      return "ControlFields";
    case kLayerOrder:
      return "CoeffOrder";
    case kLayerAC:
      return "ACHistograms";
    case kLayerACTokens:
      return "ACTokens";
    case kLayerModularAcGroup:
      return "ModularAcGroup";
    default:
      JXL_ABORT("Invalid layer %d\n", static_cast<int>(layer));
  }
}

void AuxOut::LayerTotals::Print(size_t num_inputs) const {
  printf("%10" PRId64, static_cast<int64_t>(total_bits));
  if (histogram_bits != 0) {
    printf("   [c/i:%6.2f | hst:%8" PRId64 " | ex:%8" PRId64 " | h+c+e:%12.3f",
           num_clustered_histograms * 1.0 / num_inputs,
           static_cast<int64_t>(histogram_bits >> 3),
           static_cast<int64_t>(extra_bits >> 3),
           (histogram_bits + clustered_entropy + extra_bits) / 8.0);
    printf("]");
  }
  printf("\n");
}

void AuxOut::Assimilate(const AuxOut& victim) {
  for (size_t i = 0; i < layers.size(); ++i) {
    layers[i].Assimilate(victim.layers[i]);
  }
  num_blocks += victim.num_blocks;
  num_small_blocks += victim.num_small_blocks;
  num_dct4x8_blocks += victim.num_dct4x8_blocks;
  num_afv_blocks += victim.num_afv_blocks;
  num_dct8_blocks += victim.num_dct8_blocks;
  num_dct8x16_blocks += victim.num_dct8x16_blocks;
  num_dct8x32_blocks += victim.num_dct8x32_blocks;
  num_dct16_blocks += victim.num_dct16_blocks;
  num_dct16x32_blocks += victim.num_dct16x32_blocks;
  num_dct32_blocks += victim.num_dct32_blocks;
  num_dct32x64_blocks += victim.num_dct32x64_blocks;
  num_dct64_blocks += victim.num_dct64_blocks;
  num_butteraugli_iters += victim.num_butteraugli_iters;
  for (size_t i = 0; i < dc_pred_usage.size(); ++i) {
    dc_pred_usage[i] += victim.dc_pred_usage[i];
    dc_pred_usage_xb[i] += victim.dc_pred_usage_xb[i];
  }
  max_quant_rescale = std::max(max_quant_rescale, victim.max_quant_rescale);
  min_quant_rescale = std::min(min_quant_rescale, victim.min_quant_rescale);
  max_bitrate_error = std::max(max_bitrate_error, victim.max_bitrate_error);
  min_bitrate_error = std::min(min_bitrate_error, victim.min_bitrate_error);
}

void AuxOut::Print(size_t num_inputs) const {
  if (num_inputs == 0) return;

  LayerTotals all_layers;
  for (size_t i = 0; i < layers.size(); ++i) {
    all_layers.Assimilate(layers[i]);
  }

  printf("Average butteraugli iters: %10.2f\n",
         num_butteraugli_iters * 1.0 / num_inputs);
  if (min_quant_rescale != 1.0 || max_quant_rescale != 1.0) {
    printf("quant rescale range: %f .. %f\n", min_quant_rescale,
           max_quant_rescale);
    printf("bitrate error range: %.3f%% .. %.3f%%\n",
           100.0f * min_bitrate_error, 100.0f * max_bitrate_error);
  }

  for (size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].total_bits != 0) {
      printf("Total layer bits %-10s\t", LayerName(i));
      printf("%10f%%", 100.0 * layers[i].total_bits / all_layers.total_bits);
      layers[i].Print(num_inputs);
    }
  }
  printf("Total image size           ");
  all_layers.Print(num_inputs);

  const uint32_t dc_pred_total =
      std::accumulate(dc_pred_usage.begin(), dc_pred_usage.end(), 0u);
  const uint32_t dc_pred_total_xb =
      std::accumulate(dc_pred_usage_xb.begin(), dc_pred_usage_xb.end(), 0u);
  if (dc_pred_total + dc_pred_total_xb != 0) {
    printf("\nDC pred     Y                XB:\n");
    for (size_t i = 0; i < dc_pred_usage.size(); ++i) {
      printf("  %6u (%5.2f%%)    %6u (%5.2f%%)\n", dc_pred_usage[i],
             100.0 * dc_pred_usage[i] / dc_pred_total, dc_pred_usage_xb[i],
             100.0 * dc_pred_usage_xb[i] / dc_pred_total_xb);
    }
  }

  size_t total_blocks = 0;
  size_t total_positions = 0;
  if (total_blocks != 0 && total_positions != 0) {
    printf("\n\t\t  Blocks\t\tPositions\t\t\tBlocks/Position\n");
    printf(" Total:\t\t    %7" PRIuS "\t\t     %7" PRIuS " \t\t\t%10f%%\n\n",
           total_blocks, total_positions,
           100.0 * total_blocks / total_positions);
  }
}

template <typename T>
void AuxOut::DumpImage(const char* label, const Image3<T>& image) const {
  if (!dump_image) return;
  if (debug_prefix.empty()) return;
  std::ostringstream pathname;
  pathname << debug_prefix << label << ".png";
  (void)dump_image(ConvertToFloat(image), ColorEncoding::SRGB(),
                   pathname.str());
}
template void AuxOut::DumpImage(const char* label,
                                const Image3<float>& image) const;
template void AuxOut::DumpImage(const char* label,
                                const Image3<uint8_t>& image) const;

template <typename T>
void AuxOut::DumpPlaneNormalized(const char* label,
                                 const Plane<T>& image) const {
  T min;
  T max;
  ImageMinMax(image, &min, &max);
  Image3B normalized(image.xsize(), image.ysize());
  for (size_t c = 0; c < 3; ++c) {
    float mul = min == max ? 0 : (255.0f / (max - min));
    for (size_t y = 0; y < image.ysize(); ++y) {
      const T* JXL_RESTRICT row_in = image.ConstRow(y);
      uint8_t* JXL_RESTRICT row_out = normalized.PlaneRow(c, y);
      for (size_t x = 0; x < image.xsize(); ++x) {
        row_out[x] = static_cast<uint8_t>((row_in[x] - min) * mul);
      }
    }
  }
  DumpImage(label, normalized);
}
template void AuxOut::DumpPlaneNormalized(const char* label,
                                          const Plane<float>& image) const;
template void AuxOut::DumpPlaneNormalized(const char* label,
                                          const Plane<uint8_t>& image) const;

void AuxOut::DumpXybImage(const char* label, const Image3F& image) const {
  if (!dump_image) return;
  if (debug_prefix.empty()) return;
  std::ostringstream pathname;
  pathname << debug_prefix << label << ".png";

  Image3F linear(image.xsize(), image.ysize());
  OpsinParams opsin_params;
  opsin_params.Init(kDefaultIntensityTarget);
  OpsinToLinear(image, Rect(linear), nullptr, &linear, opsin_params);

  (void)dump_image(std::move(linear), ColorEncoding::LinearSRGB(),
                   pathname.str());
}

}  // namespace jxl
