// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_AUX_OUT_H_
#define LIB_JXL_AUX_OUT_H_

// Optional output information for debugging and analyzing size usage.

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <array>
#include <functional>
#include <sstream>
#include <string>
#include <utility>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/jxl_inspection.h"

namespace jxl {

// For LayerName and AuxOut::layers[] index. Order does not matter.
enum {
  kLayerHeader = 0,
  kLayerTOC,
  kLayerDictionary,
  kLayerSplines,
  kLayerNoise,
  kLayerQuant,
  kLayerModularTree,
  kLayerModularGlobal,
  kLayerDC,
  kLayerModularDcGroup,
  kLayerControlFields,
  kLayerOrder,
  kLayerAC,
  kLayerACTokens,
  kLayerModularAcGroup,
  kNumImageLayers
};

static inline const char* LayerName(size_t layer) {
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

// Statistics gathered during compression or decompression.
struct AuxOut {
 private:
  struct LayerTotals {
    void Assimilate(const LayerTotals& victim) {
      num_clustered_histograms += victim.num_clustered_histograms;
      histogram_bits += victim.histogram_bits;
      extra_bits += victim.extra_bits;
      total_bits += victim.total_bits;
      clustered_entropy += victim.clustered_entropy;
    }
    void Print(size_t num_inputs) const {
      printf("%10" PRId64, static_cast<int64_t>(total_bits));
      if (histogram_bits != 0) {
        printf("   [c/i:%6.2f | hst:%8" PRId64 " | ex:%8" PRId64
               " | h+c+e:%12.3f",
               num_clustered_histograms * 1.0 / num_inputs,
               static_cast<int64_t>(histogram_bits >> 3),
               static_cast<int64_t>(extra_bits >> 3),
               (histogram_bits + clustered_entropy + extra_bits) / 8.0);
        printf("]");
      }
      printf("\n");
    }
    size_t num_clustered_histograms = 0;
    size_t extra_bits = 0;

    // Set via BitsWritten below
    size_t histogram_bits = 0;
    size_t total_bits = 0;

    double clustered_entropy = 0.0;
  };

 public:
  AuxOut() = default;
  AuxOut(const AuxOut&) = default;

  void Assimilate(const AuxOut& victim) {
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

  void Print(size_t num_inputs) const;

  size_t TotalBits() const {
    size_t total = 0;
    for (const auto& layer : layers) {
      total += layer.total_bits;
    }
    return total;
  }

  template <typename T>
  void DumpImage(const char* label, const Image3<T>& image) const {
    if (!dump_image) return;
    if (debug_prefix.empty()) return;
    std::ostringstream pathname;
    pathname << debug_prefix << label << ".png";
    CodecInOut io;
    // Always save to 16-bit png.
    io.metadata.m.SetUintSamples(16);
    io.metadata.m.color_encoding = ColorEncoding::SRGB();
    io.SetFromImage(ConvertToFloat(image), io.metadata.m.color_encoding);
    (void)dump_image(io, pathname.str());
  }
  template <typename T>
  void DumpImage(const char* label, const Plane<T>& image) {
    DumpImage(label,
              Image3<T>(CopyImage(image), CopyImage(image), CopyImage(image)));
  }

  template <typename T>
  void DumpXybImage(const char* label, const Image3<T>& image) const {
    if (!dump_image) return;
    if (debug_prefix.empty()) return;
    std::ostringstream pathname;
    pathname << debug_prefix << label << ".png";

    Image3F linear(image.xsize(), image.ysize());
    OpsinParams opsin_params;
    opsin_params.Init(kDefaultIntensityTarget);
    OpsinToLinear(image, Rect(linear), nullptr, &linear, opsin_params);

    CodecInOut io;
    io.metadata.m.SetUintSamples(16);
    io.metadata.m.color_encoding = ColorEncoding::LinearSRGB();
    io.SetFromImage(std::move(linear), io.metadata.m.color_encoding);

    (void)dump_image(io, pathname.str());
  }

  // Normalizes all the channels to range 0-1, creating a false-color image
  // which allows seeing the information from non-RGB channels in an RGB debug
  // image.
  template <typename T>
  void DumpImageNormalized(const char* label, const Image3<T>& image) const {
    std::array<T, 3> min;
    std::array<T, 3> max;
    Image3MinMax(image, &min, &max);
    Image3B normalized(image.xsize(), image.ysize());
    for (size_t c = 0; c < 3; ++c) {
      float mul = min[c] == max[c] ? 0 : (255.0f / (max[c] - min[c]));
      for (size_t y = 0; y < image.ysize(); ++y) {
        const T* JXL_RESTRICT row_in = image.ConstPlaneRow(c, y);
        uint8_t* JXL_RESTRICT row_out = normalized.PlaneRow(c, y);
        for (size_t x = 0; x < image.xsize(); ++x) {
          row_out[x] = static_cast<uint8_t>((row_in[x] - min[c]) * mul);
        }
      }
    }
    DumpImage(label, normalized);
  }

  template <typename T>
  void DumpPlaneNormalized(const char* label, const Plane<T>& image) const {
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

  void SetInspectorImage3F(const jxl::InspectorImage3F& inspector) {
    inspector_image3f_ = inspector;
  }

  // Allows hooking intermediate data inspection into various places of the
  // processing pipeline. Returns true iff processing should proceed.
  bool InspectImage3F(const char* label, const Image3F& image) {
    if (inspector_image3f_ != nullptr) {
      return inspector_image3f_(label, image);
    }
    return true;
  }

  std::array<LayerTotals, kNumImageLayers> layers;
  size_t num_blocks = 0;

  // Number of blocks that use larger DCT (set by ac_strategy).
  size_t num_small_blocks = 0;
  size_t num_dct4x8_blocks = 0;
  size_t num_afv_blocks = 0;
  size_t num_dct8_blocks = 0;
  size_t num_dct8x16_blocks = 0;
  size_t num_dct8x32_blocks = 0;
  size_t num_dct16_blocks = 0;
  size_t num_dct16x32_blocks = 0;
  size_t num_dct32_blocks = 0;
  size_t num_dct32x64_blocks = 0;
  size_t num_dct64_blocks = 0;

  std::array<uint32_t, 8> dc_pred_usage = {{0}};
  std::array<uint32_t, 8> dc_pred_usage_xb = {{0}};

  int num_butteraugli_iters = 0;

  float max_quant_rescale = 1.0f;
  float min_quant_rescale = 1.0f;
  float min_bitrate_error = 0.0f;
  float max_bitrate_error = 0.0f;

  // If not empty, additional debugging information (e.g. debug images) is
  // saved in files with this prefix.
  std::string debug_prefix;

  // By how much the decoded image was downsampled relative to the encoded
  // image.
  size_t downsampling = 1;

  jxl::InspectorImage3F inspector_image3f_;

  std::function<Status(const CodecInOut&, const std::string&)> dump_image =
      nullptr;
};

// Used to skip image creation if they won't be written to debug directory.
static inline bool WantDebugOutput(const AuxOut* aux_out) {
  // Need valid pointer and filename.
  return aux_out != nullptr && !aux_out->debug_prefix.empty();
}

}  // namespace jxl

#endif  // LIB_JXL_AUX_OUT_H_
