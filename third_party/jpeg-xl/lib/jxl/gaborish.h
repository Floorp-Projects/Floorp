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

#ifndef LIB_JXL_GABORISH_H_
#define LIB_JXL_GABORISH_H_

// Linear smoothing (3x3 convolution) for deblocking without too much blur.

#include <stdint.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/image.h"

namespace jxl {

// Used in encoder to reduce the impact of the decoder's smoothing.
// This is not exact. Works in-place to reduce memory use.
// The input is typically in XYB space.
void GaborishInverse(Image3F* in_out, float mul, ThreadPool* pool);

}  // namespace jxl

#endif  // LIB_JXL_GABORISH_H_
