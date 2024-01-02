// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/common.h"

#include <jxl/codestream_header.h>
#include <jxl/types.h>

#include <cstddef>
#include <vector>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"

namespace jxl {
namespace extras {

Status SelectFormat(const std::vector<JxlPixelFormat>& accepted_formats,
                    const JxlBasicInfo& basic_info, JxlPixelFormat* format) {
  const size_t original_bit_depth = basic_info.bits_per_sample;
  size_t current_bit_depth = 0;
  size_t num_alpha_channels = (basic_info.alpha_bits != 0 ? 1 : 0);
  size_t num_channels = basic_info.num_color_channels + num_alpha_channels;
  for (;;) {
    for (const JxlPixelFormat& candidate : accepted_formats) {
      if (candidate.num_channels != num_channels) continue;
      const size_t candidate_bit_depth =
          PackedImage::BitsPerChannel(candidate.data_type);
      if (
          // Candidate bit depth is less than what we have and still enough
          (original_bit_depth <= candidate_bit_depth &&
           candidate_bit_depth < current_bit_depth) ||
          // Or larger than the too-small bit depth we currently have
          (current_bit_depth < candidate_bit_depth &&
           current_bit_depth < original_bit_depth)) {
        *format = candidate;
        current_bit_depth = candidate_bit_depth;
      }
    }
    if (current_bit_depth == 0) {
      if (num_channels > basic_info.num_color_channels) {
        // Try dropping the alpha channel.
        --num_channels;
        continue;
      }
      return JXL_FAILURE("no appropriate format found");
    }
    break;
  }
  if (current_bit_depth < original_bit_depth) {
    JXL_WARNING("encoding %" PRIuS "-bit original to %" PRIuS " bits",
                original_bit_depth, current_bit_depth);
  }
  return true;
}

}  // namespace extras
}  // namespace jxl
