// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/color_hints.h"

#include "jxl/encode.h"
#include "lib/extras/dec/color_description.h"
#include "lib/jxl/base/file_io.h"

namespace jxl {
namespace extras {

Status ApplyColorHints(const ColorHints& color_hints,
                       const bool color_already_set, const bool is_gray,
                       PackedPixelFile* ppf) {
  if (color_already_set) {
    return color_hints.Foreach(
        [](const std::string& key, const std::string& /*value*/) {
          JXL_WARNING("Decoder ignoring %s hint", key.c_str());
          return true;
        });
  }

  bool got_color_space = false;

  JXL_RETURN_IF_ERROR(color_hints.Foreach(
      [is_gray, ppf, &got_color_space](const std::string& key,
                                       const std::string& value) -> Status {
        if (key == "color_space") {
          JxlColorEncoding c_original_external;
          if (!ParseDescription(value, &c_original_external)) {
            return JXL_FAILURE("Failed to apply color_space");
          }
          ppf->color_encoding = c_original_external;

          if (is_gray !=
              (ppf->color_encoding.color_space == JXL_COLOR_SPACE_GRAY)) {
            return JXL_FAILURE("mismatch between file and color_space hint");
          }

          got_color_space = true;
        } else if (key == "icc_pathname") {
          JXL_RETURN_IF_ERROR(ReadFile(value, &ppf->icc));
          got_color_space = true;
        } else {
          JXL_WARNING("Ignoring %s hint", key.c_str());
        }
        return true;
      }));

  if (!got_color_space) {
    JXL_WARNING("No color_space/icc_pathname given, assuming sRGB");
    ppf->color_encoding.color_space =
        is_gray ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB;
    ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
    ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
    ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
