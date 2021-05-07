// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_DEC_XYB_H_
#define LIB_JXL_DEC_XYB_H_

// XYB -> linear sRGB.

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_metadata.h"
#include "lib/jxl/opsin_params.h"

namespace jxl {

// Parameters for XYB->sRGB conversion.
struct OpsinParams {
  float inverse_opsin_matrix[9 * 4];
  float opsin_biases[4];
  float opsin_biases_cbrt[4];
  float quant_biases[4];
  void Init(float intensity_target);
};

struct OutputEncodingInfo {
  ColorEncoding color_encoding;
  // Used for Gamma and DCI transfer functions.
  float inverse_gamma;
  // Contains an opsin matrix that converts to the primaries of the output
  // encoding.
  OpsinParams opsin_params;
  Status Set(const ImageMetadata& metadata);
  bool all_default_opsin = true;
  bool color_encoding_is_original = false;
};

// Converts `inout` (not padded) from opsin to linear sRGB in-place. Called from
// per-pass postprocessing, hence parallelized.
void OpsinToLinearInplace(Image3F* JXL_RESTRICT inout, ThreadPool* pool,
                          const OpsinParams& opsin_params);

// Converts `opsin:rect` (opsin may be padded, rect.x0 must be vector-aligned)
// to linear sRGB. Called from whole-frame encoder, hence parallelized.
void OpsinToLinear(const Image3F& opsin, const Rect& rect, ThreadPool* pool,
                   Image3F* JXL_RESTRICT linear,
                   const OpsinParams& opsin_params);

// Bt.601 to match JPEG/JFIF. Inputs are _signed_ YCbCr values suitable for DCT,
// see F.1.1.3 of T.81 (because our data type is float, there is no need to add
// a bias to make the values unsigned).
void YcbcrToRgb(const Image3F& ycbcr, Image3F* rgb, const Rect& rect);

ImageF UpsampleV2(const ImageF& src, ThreadPool* pool);

// WARNING: this uses unaligned accesses, so the caller must first call
// src.InitializePaddingForUnalignedAccesses() to avoid msan crashes.
ImageF UpsampleH2(const ImageF& src, ThreadPool* pool);

bool HasFastXYBTosRGB8();
void FastXYBTosRGB8(const Image3F& input, const Rect& input_rect,
                    const Rect& output_buf_rect,
                    uint8_t* JXL_RESTRICT output_buf, size_t xsize);

}  // namespace jxl

#endif  // LIB_JXL_DEC_XYB_H_
