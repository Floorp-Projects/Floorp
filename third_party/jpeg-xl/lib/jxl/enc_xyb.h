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

#ifndef LIB_JXL_ENC_XYB_H_
#define LIB_JXL_ENC_XYB_H_

// Converts to XYB color space.

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Converts any color space to XYB. If `linear` is not null, returns `linear`
// after filling it with a linear sRGB copy of `in`. Otherwise, returns `&in`.
//
// NOTE this return value can avoid an extra color conversion if `in` would
// later be passed to JxlButteraugliComparator.
const ImageBundle* ToXYB(const ImageBundle& in, ThreadPool* pool,
                         Image3F* JXL_RESTRICT xyb,
                         ImageBundle* JXL_RESTRICT linear = nullptr);

// Bt.601 to match JPEG/JFIF. Outputs _signed_ YCbCr values suitable for DCT,
// see F.1.1.3 of T.81 (because our data type is float, there is no need to add
// a bias to make the values unsigned).
void RgbToYcbcr(const ImageF& r_plane, const ImageF& g_plane,
                const ImageF& b_plane, ImageF* y_plane, ImageF* cb_plane,
                ImageF* cr_plane, ThreadPool* pool);

// DEPRECATED, used by opsin_image_wrapper.
Image3F OpsinDynamicsImage(const Image3B& srgb8);

// For opsin_image_test.
void TestCubeRoot();

}  // namespace jxl

#endif  // LIB_JXL_ENC_XYB_H_
