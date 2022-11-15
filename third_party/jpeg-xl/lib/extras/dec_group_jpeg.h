// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_GROUP_JPEG_H_
#define LIB_EXTRAS_DEC_GROUP_JPEG_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/compiler_specific.h"

namespace jxl {
namespace extras {

void GatherBlockStats(const int16_t* JXL_RESTRICT coeffs,
                      const size_t coeffs_size, int32_t* JXL_RESTRICT nonzeros,
                      int32_t* JXL_RESTRICT sumabs);

void DecodeJpegBlock(const int16_t* JXL_RESTRICT qblock,
                     const float* JXL_RESTRICT dequant_matrices,
                     const float* JXL_RESTRICT biases,
                     float* JXL_RESTRICT scratch_space,
                     float* JXL_RESTRICT output, size_t output_stride);

void Upsample2Horizontal(float* JXL_RESTRICT row_in,
                         float* JXL_RESTRICT row_out, size_t len_out);

void Upsample2Vertical(const float* JXL_RESTRICT row_top,
                       const float* JXL_RESTRICT row_mid,
                       const float* JXL_RESTRICT row_bot,
                       float* JXL_RESTRICT row_out0,
                       float* JXL_RESTRICT row_out1, size_t len);

void YCbCrToRGB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize);

void DecenterRow(float* row, size_t xsize);

void WriteToPackedImage(float* JXL_RESTRICT rows[3], size_t x0, size_t y0,
                        size_t len, uint8_t* JXL_RESTRICT scratch_space,
                        extras::PackedImage* image);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_GROUP_JPEG_H_
