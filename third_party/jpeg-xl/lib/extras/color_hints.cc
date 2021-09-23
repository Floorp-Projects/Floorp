// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/color_hints.h"

#include "lib/extras/color_description.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {
namespace extras {

Status ApplyColorHints(const ColorHints& color_hints,
                       const bool color_already_set, const bool is_gray,
                       CodecInOut* io) {
  if (color_already_set) {
    return color_hints.Foreach(
        [](const std::string& key, const std::string& /*value*/) {
          JXL_WARNING("Decoder ignoring %s hint", key.c_str());
          return true;
        });
  }

  bool got_color_space = false;

  JXL_RETURN_IF_ERROR(color_hints.Foreach(
      [is_gray, io, &got_color_space](const std::string& key,
                                      const std::string& value) -> Status {
        ColorEncoding* c_original = &io->metadata.m.color_encoding;
        if (key == "color_space") {
          JxlColorEncoding c_original_external;
          if (!ParseDescription(value, &c_original_external) ||
              !ConvertExternalToInternalColorEncoding(c_original_external,
                                                      c_original) ||
              !c_original->CreateICC()) {
            return JXL_FAILURE("Failed to apply color_space");
          }

          if (is_gray != io->metadata.m.color_encoding.IsGray()) {
            return JXL_FAILURE("mismatch between file and color_space hint");
          }

          got_color_space = true;
        } else if (key == "icc_pathname") {
          PaddedBytes icc;
          JXL_RETURN_IF_ERROR(ReadFile(value, &icc));
          JXL_RETURN_IF_ERROR(c_original->SetICC(std::move(icc)));
          got_color_space = true;
        } else {
          JXL_WARNING("Ignoring %s hint", key.c_str());
        }
        return true;
      }));

  if (!got_color_space) {
    JXL_WARNING("No color_space/icc_pathname given, assuming sRGB");
    JXL_RETURN_IF_ERROR(io->metadata.m.color_encoding.SetSRGB(
        is_gray ? ColorSpace::kGray : ColorSpace::kRGB));
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
