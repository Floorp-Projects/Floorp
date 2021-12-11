// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_PASSES_STATE_H_
#define LIB_JXL_PASSES_STATE_H_

#include "lib/jxl/ac_context.h"
#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_patch_dictionary.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/noise.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer.h"
#include "lib/jxl/splines.h"

// Structures that hold the (en/de)coder state for a JPEG XL kVarDCT
// (en/de)coder.

namespace jxl {

struct ImageFeatures {
  NoiseParams noise_params;
  PatchDictionary patches;
  Splines splines;
};

// State common to both encoder and decoder.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct PassesSharedState {
  PassesSharedState() : frame_header(nullptr) {}

  // Headers and metadata.
  const CodecMetadata* metadata;
  FrameHeader frame_header;

  FrameDimensions frame_dim;

  // Control fields and parameters.
  AcStrategyImage ac_strategy;

  // Dequant matrices + quantizer.
  DequantMatrices matrices;
  Quantizer quantizer{&matrices};
  ImageI raw_quant_field;

  // Per-block side information for EPF detail preservation.
  ImageB epf_sharpness;

  ColorCorrelationMap cmap;

  ImageFeatures image_features;

  // Memory area for storing coefficient orders.
  // `coeff_order_size` is the size used by *one* set of coefficient orders (at
  // most kMaxCoeffOrderSize). A set of coefficient orders is present for each
  // pass.
  size_t coeff_order_size = 0;
  std::vector<coeff_order_t> coeff_orders;

  // Decoder-side DC and quantized DC.
  ImageB quant_dc;
  Image3F dc_storage;
  const Image3F* JXL_RESTRICT dc = &dc_storage;

  BlockCtxMap block_ctx_map;

  Image3F dc_frames[4];

  struct {
    ImageBundle storage;
    // Can either point to `storage`, if this is a frame that is not stored in
    // the CodecInOut, or can point to an existing ImageBundle.
    // TODO(veluca): pointing to ImageBundles in CodecInOut is not possible for
    // now, as they are stored in a vector and thus may be moved. Fix this.
    ImageBundle* JXL_RESTRICT frame = &storage;
    // ImageBundle doesn't yet have a simple way to state it is in XYB.
    bool ib_is_in_xyb = false;
  } reference_frames[4] = {};

  // Number of pre-clustered set of histograms (with the same ctx map), per
  // pass. Encoded as num_histograms_ - 1.
  size_t num_histograms = 0;

  bool IsGrayscale() const { return metadata->m.color_encoding.IsGray(); }

  Rect GroupRect(size_t group_index) const {
    const size_t gx = group_index % frame_dim.xsize_groups;
    const size_t gy = group_index / frame_dim.xsize_groups;
    const Rect rect(gx * frame_dim.group_dim, gy * frame_dim.group_dim,
                    frame_dim.group_dim, frame_dim.group_dim, frame_dim.xsize,
                    frame_dim.ysize);
    return rect;
  }

  Rect PaddedGroupRect(size_t group_index) const {
    const size_t gx = group_index % frame_dim.xsize_groups;
    const size_t gy = group_index / frame_dim.xsize_groups;
    const Rect rect(gx * frame_dim.group_dim, gy * frame_dim.group_dim,
                    frame_dim.group_dim, frame_dim.group_dim,
                    frame_dim.xsize_padded, frame_dim.ysize_padded);
    return rect;
  }

  Rect BlockGroupRect(size_t group_index) const {
    const size_t gx = group_index % frame_dim.xsize_groups;
    const size_t gy = group_index / frame_dim.xsize_groups;
    const Rect rect(gx * (frame_dim.group_dim >> 3),
                    gy * (frame_dim.group_dim >> 3), frame_dim.group_dim >> 3,
                    frame_dim.group_dim >> 3, frame_dim.xsize_blocks,
                    frame_dim.ysize_blocks);
    return rect;
  }

  Rect DCGroupRect(size_t group_index) const {
    const size_t gx = group_index % frame_dim.xsize_dc_groups;
    const size_t gy = group_index / frame_dim.xsize_dc_groups;
    const Rect rect(gx * frame_dim.group_dim, gy * frame_dim.group_dim,
                    frame_dim.group_dim, frame_dim.group_dim,
                    frame_dim.xsize_blocks, frame_dim.ysize_blocks);
    return rect;
  }
};

// Initialized the state information that is shared between encoder and decoder.
Status InitializePassesSharedState(const FrameHeader& frame_header,
                                   PassesSharedState* JXL_RESTRICT shared,
                                   bool encoder = false);

}  // namespace jxl

#endif  // LIB_JXL_PASSES_STATE_H_
