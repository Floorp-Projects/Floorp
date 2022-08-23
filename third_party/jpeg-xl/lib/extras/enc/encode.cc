// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/encode.h"

#include <locale>

#if JPEGXL_ENABLE_APNG
#include "lib/extras/enc/apng.h"
#endif
#if JPEGXL_ENABLE_EXR
#include "lib/extras/enc/exr.h"
#endif
#if JPEGXL_ENABLE_JPEG
#include "lib/extras/enc/jpg.h"
#endif
#include "lib/extras/enc/npy.h"
#include "lib/extras/enc/pgx.h"
#include "lib/extras/enc/pnm.h"
#include "lib/jxl/base/printf_macros.h"

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

std::unique_ptr<Encoder> Encoder::FromExtension(std::string extension) {
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](char c) { return std::tolower(c, std::locale::classic()); });
#if JPEGXL_ENABLE_APNG
  if (extension == ".png" || extension == ".apng") return GetAPNGEncoder();
#endif

#if JPEGXL_ENABLE_JPEG
  if (extension == ".jpg") return GetJPEGEncoder();
  if (extension == ".jpeg") return GetJPEGEncoder();
#endif

  if (extension == ".npy") return GetNumPyEncoder();

  if (extension == ".pgx") return GetPGXEncoder();

  if (extension == ".pam") return GetPAMEncoder();
  if (extension == ".pgm") return GetPGMEncoder();
  if (extension == ".ppm") return GetPPMEncoder();
  if (extension == ".pfm") return GetPFMEncoder();

#if JPEGXL_ENABLE_EXR
  if (extension == ".exr") return GetEXREncoder();
#endif

  return nullptr;
}

}  // namespace extras
}  // namespace jxl
