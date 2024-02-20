// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/chroma_from_luma.h"

#include "lib/jxl/image_ops.h"

namespace jxl {

StatusOr<ColorCorrelationMap> ColorCorrelationMap::Create(size_t xsize,
                                                          size_t ysize,
                                                          bool XYB) {
  ColorCorrelationMap result;
  size_t xblocks = DivCeil(xsize, kColorTileDim);
  size_t yblocks = DivCeil(ysize, kColorTileDim);
  JXL_ASSIGN_OR_RETURN(result.ytox_map, ImageSB::Create(xblocks, yblocks));
  JXL_ASSIGN_OR_RETURN(result.ytob_map, ImageSB::Create(xblocks, yblocks));
  ZeroFillImage(&result.ytox_map);
  ZeroFillImage(&result.ytob_map);
  if (!XYB) {
    result.base_correlation_b_ = 0;
  }
  result.RecomputeDCFactors();
  return result;
}

}  // namespace jxl
