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

#ifndef LIB_JXL_COLOR_MANAGEMENT_H_
#define LIB_JXL_COLOR_MANAGEMENT_H_

// ICC profiles and color space conversions.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image.h"

namespace jxl {

enum class ExtraTF {
  kNone,
  kPQ,
  kHLG,
  kSRGB,
};

Status MaybeCreateProfile(const ColorEncoding& c,
                          PaddedBytes* JXL_RESTRICT icc);

Status CIEXYZFromWhiteCIExy(const CIExy& xy, float XYZ[3]);

}  // namespace jxl

#endif  // LIB_JXL_COLOR_MANAGEMENT_H_
