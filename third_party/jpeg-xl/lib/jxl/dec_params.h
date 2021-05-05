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

#ifndef LIB_JXL_DEC_PARAMS_H_
#define LIB_JXL_DEC_PARAMS_H_

// Parameters and flags that govern JXL decompression.

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "lib/jxl/base/override.h"

namespace jxl {

struct DecompressParams {
  // If true, checks at the end of decoding that all of the compressed data
  // was consumed by the decoder.
  bool check_decompressed_size = true;

  // If true, skip dequant and iDCT and decode to JPEG (only if possible)
  bool keep_dct = false;

  // These cannot be kOn because they need encoder support.
  Override preview = Override::kDefault;

  // How many passes to decode at most. By default, decode everything.
  uint32_t max_passes = std::numeric_limits<uint32_t>::max();
  // Alternatively, one can specify the maximum tolerable downscaling factor
  // with respect to the full size of the image. By default, nothing less than
  // the full size is requested.
  size_t max_downsampling = 1;

  // Try to decode as much as possible of a truncated codestream, but only whole
  // sections at a time.
  bool allow_partial_files = false;
  // Allow even more progression.
  bool allow_more_progressive_steps = false;

  bool operator==(const DecompressParams other) const {
    return check_decompressed_size == other.check_decompressed_size &&
           keep_dct == other.keep_dct && preview == other.preview &&
           max_passes == other.max_passes &&
           max_downsampling == other.max_downsampling &&
           allow_partial_files == other.allow_partial_files &&
           allow_more_progressive_steps == other.allow_more_progressive_steps;
  }
  bool operator!=(const DecompressParams& other) const {
    return !(*this == other);
  }
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_PARAMS_H_
