// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_AR_CONTROL_FIELD_H_
#define LIB_JXL_ENC_AR_CONTROL_FIELD_H_

#include <stddef.h>

#include <vector>

#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"

namespace jxl {

struct PassesEncoderState;

struct ArControlFieldHeuristics {
  struct TempImages {
    void InitOnce() {
      if (laplacian_sqrsum.xsize() != 0) return;
      laplacian_sqrsum = ImageF(kEncTileDim + 4, kEncTileDim + 4);
      sqrsum_00 = ImageF(kEncTileDim / 4, kEncTileDim / 4);
      sqrsum_22 = ImageF(kEncTileDim / 4 + 1, kEncTileDim / 4 + 1);
    }

    ImageF laplacian_sqrsum;
    ImageF sqrsum_00;
    ImageF sqrsum_22;
  };

  void PrepareForThreads(size_t num_threads) {
    temp_images.resize(num_threads);
  }

  void RunRect(const CompressParams& cparams, const FrameHeader& frame_header,
               const Rect& block_rect, const Image3F& opsin,
               const Rect& opsin_rect, const ImageF& quant_field,
               const AcStrategyImage& ac_strategy, ImageB* epf_sharpness,
               size_t thread);

  std::vector<TempImages> temp_images;
};

}  // namespace jxl

#endif  // LIB_JXL_AR_ENC_CONTROL_FIELD_H_
