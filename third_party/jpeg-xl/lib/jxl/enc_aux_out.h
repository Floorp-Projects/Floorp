// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_AUX_OUT_H_
#define LIB_JXL_AUX_OUT_H_

// Optional output information for debugging and analyzing size usage.

#include <stddef.h>

#include <array>
#include <functional>
#include <string>

#include "lib/jxl/image.h"
#include "lib/jxl/jxl_inspection.h"

namespace jxl {

struct ColorEncoding;

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

const char* LayerName(size_t layer);

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
    void Print(size_t num_inputs) const;

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

  void Assimilate(const AuxOut& victim);

  void Print(size_t num_inputs) const;

  size_t TotalBits() const {
    size_t total = 0;
    for (const auto& layer : layers) {
      total += layer.total_bits;
    }
    return total;
  }

  template <typename T>
  void DumpImage(const char* label, const Image3<T>& image) const;

  void DumpXybImage(const char* label, const Image3F& image) const;

  template <typename T>
  void DumpPlaneNormalized(const char* label, const Plane<T>& image) const;

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

  std::function<Status(Image3F&&, const ColorEncoding&, const std::string&)>
      dump_image = nullptr;
};

extern template void AuxOut::DumpImage(const char* label,
                                       const Image3<float>& image) const;
extern template void AuxOut::DumpImage(const char* label,
                                       const Image3<uint8_t>& image) const;
extern template void AuxOut::DumpPlaneNormalized(
    const char* label, const Plane<float>& image) const;
extern template void AuxOut::DumpPlaneNormalized(
    const char* label, const Plane<uint8_t>& image) const;

// Used to skip image creation if they won't be written to debug directory.
static inline bool WantDebugOutput(const AuxOut* aux_out) {
  // Need valid pointer and filename.
  return aux_out != nullptr && !aux_out->debug_prefix.empty();
}

}  // namespace jxl

#endif  // LIB_JXL_AUX_OUT_H_
