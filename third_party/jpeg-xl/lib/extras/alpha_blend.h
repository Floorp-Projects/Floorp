// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_ALPHA_BLEND_H_
#define LIB_EXTRAS_ALPHA_BLEND_H_

#include "lib/extras/packed_image.h"

namespace jxl {
namespace extras {

void AlphaBlend(PackedPixelFile* ppf, float background[3]);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_ALPHA_BLEND_H_
