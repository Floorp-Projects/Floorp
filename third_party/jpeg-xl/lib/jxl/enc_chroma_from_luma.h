// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_CHROMA_FROM_LUMA_H_
#define LIB_JXL_ENC_CHROMA_FROM_LUMA_H_

// Chroma-from-luma, computed using heuristics to determine the best linear
// model for the X and B channels from the Y channel.

#include <cstddef>
#include <hwy/aligned_allocator.h>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/image.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/simd_util.h"

namespace jxl {

struct AuxOut;
class Quantizer;

void ColorCorrelationMapEncodeDC(const ColorCorrelationMap& map,
                                 BitWriter* writer, size_t layer,
                                 AuxOut* aux_out);

struct CfLHeuristics {
  Status Init(const Rect& rect);

  void PrepareForThreads(size_t num_threads) {
    mem = hwy::AllocateAligned<float>(num_threads * ItemsPerThread());
  }

  void ComputeTile(const Rect& r, const Image3F& opsin, const Rect& opsin_rect,
                   const DequantMatrices& dequant,
                   const AcStrategyImage* ac_strategy,
                   const ImageI* raw_quant_field, const Quantizer* quantizer,
                   bool fast, size_t thread, ColorCorrelationMap* cmap);

  ImageF dc_values;
  hwy::AlignedFreeUniquePtr<float[]> mem;

  // Working set is too large for stack; allocate dynamically.
  static size_t ItemsPerThread() {
    const size_t dct_scratch_size =
        3 * (MaxVectorSize() / sizeof(float)) * AcStrategy::kMaxBlockDim;
    return AcStrategy::kMaxCoeffArea * 3        // Blocks
           + kColorTileDim * kColorTileDim * 4  // AC coeff storage
           + AcStrategy::kMaxCoeffArea * 2      // Scratch space
           + dct_scratch_size;
  }
};

}  // namespace jxl

#endif  // LIB_JXL_ENC_CHROMA_FROM_LUMA_H_
