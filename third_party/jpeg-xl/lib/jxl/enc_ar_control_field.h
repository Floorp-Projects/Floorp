// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_AR_CONTROL_FIELD_H_
#define LIB_JXL_ENC_AR_CONTROL_FIELD_H_

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image.h"
#include "lib/jxl/quant_weights.h"

namespace jxl {

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

  void RunRect(const Rect& block_rect, const Image3F& opsin,
               PassesEncoderState* enc_state, size_t thread);

  std::vector<TempImages> temp_images;
  ImageB* epf_sharpness;
  ImageF* quant;
  bool all_default;
};

}  // namespace jxl

#endif  // LIB_JXL_AR_ENC_CONTROL_FIELD_H_
