// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_AR_CONTROL_FIELD_H_
#define LIB_JXL_ENC_AR_CONTROL_FIELD_H_

#include <stddef.h>

#include <vector>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"

namespace jxl {

struct PassesEncoderState;

struct ArControlFieldHeuristics {
  struct TempImages {
    Status InitOnce() {
      if (laplacian_sqrsum.xsize() != 0) return true;
      JXL_ASSIGN_OR_RETURN(laplacian_sqrsum,
                           ImageF::Create(kEncTileDim + 4, kEncTileDim + 4));
      JXL_ASSIGN_OR_RETURN(sqrsum_00,
                           ImageF::Create(kEncTileDim / 4, kEncTileDim / 4));
      JXL_ASSIGN_OR_RETURN(
          sqrsum_22, ImageF::Create(kEncTileDim / 4 + 1, kEncTileDim / 4 + 1));
      return true;
    }

    ImageF laplacian_sqrsum;
    ImageF sqrsum_00;
    ImageF sqrsum_22;
  };

  void PrepareForThreads(size_t num_threads) {
    temp_images.resize(num_threads);
  }

  Status RunRect(const CompressParams& cparams, const FrameHeader& frame_header,
                 const Rect& block_rect, const Image3F& opsin,
                 const Rect& opsin_rect, const ImageF& quant_field,
                 const AcStrategyImage& ac_strategy, ImageB* epf_sharpness,
                 size_t thread);

  std::vector<TempImages> temp_images;
};

}  // namespace jxl

#endif  // LIB_JXL_AR_ENC_CONTROL_FIELD_H_
