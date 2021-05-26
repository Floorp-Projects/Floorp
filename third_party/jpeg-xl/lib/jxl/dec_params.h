// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
  // If true, render spot colors (otherwise only returned as extra channels)
  bool render_spotcolors = true;

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
           keep_dct == other.keep_dct &&
           render_spotcolors == other.render_spotcolors &&
           preview == other.preview && max_passes == other.max_passes &&
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
