// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/pnm.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/fields.h"  // AllDefault
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {
namespace extras {
namespace {

constexpr size_t kMaxHeaderSize = 200;

Status EncodeHeader(const PackedPixelFile& ppf, const size_t bits_per_sample,
                    const bool little_endian, char* header,
                    int* JXL_RESTRICT chars_written) {
  bool is_gray = ppf.info.num_color_channels <= 2;
  size_t oriented_xsize =
      ppf.info.orientation <= 4 ? ppf.info.xsize : ppf.info.ysize;
  size_t oriented_ysize =
      ppf.info.orientation <= 4 ? ppf.info.ysize : ppf.info.xsize;
  if (ppf.info.alpha_bits > 0) {  // PAM
    if (bits_per_sample > 16) return JXL_FAILURE("PNM cannot have > 16 bits");
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    *chars_written =
        snprintf(header, kMaxHeaderSize,
                 "P7\nWIDTH %" PRIuS "\nHEIGHT %" PRIuS
                 "\nDEPTH %u\nMAXVAL %u\nTUPLTYPE %s\nENDHDR\n",
                 oriented_xsize, oriented_ysize, is_gray ? 2 : 4, max_val,
                 is_gray ? "GRAYSCALE_ALPHA" : "RGB_ALPHA");
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else if (bits_per_sample == 32) {  // PFM
    const char type = is_gray ? 'f' : 'F';
    const double scale = little_endian ? -1.0 : 1.0;
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%.1f\n",
                 type, oriented_xsize, oriented_ysize, scale);
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else {  // PGM/PPM
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    if (max_val >= 65536) return JXL_FAILURE("PNM cannot have > 16 bits");
    const char type = is_gray ? '5' : '6';
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%u\n",
                 type, oriented_xsize, oriented_ysize, max_val);
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  }
  return true;
}

Span<const uint8_t> MakeSpan(const char* str) {
  return Span<const uint8_t>(reinterpret_cast<const uint8_t*>(str),
                             strlen(str));
}

// Flip the image vertically for loading/saving PFM files which have the
// scanlines inverted.
void VerticallyFlipImage(float* const float_image, const size_t xsize,
                         const size_t ysize, const size_t num_channels) {
  for (size_t y = 0; y < ysize / 2; y++) {
    float* first_row = &float_image[y * num_channels * xsize];
    float* other_row = &float_image[(ysize - y - 1) * num_channels * xsize];
    for (size_t c = 0; c < num_channels; c++) {
      for (size_t x = 0; x < xsize; ++x) {
        float tmp = first_row[x * num_channels + c];
        first_row[x * num_channels + c] = other_row[x * num_channels + c];
        other_row[x * num_channels + c] = tmp;
      }
    }
  }
}

}  // namespace

Status EncodeImagePNM(const PackedPixelFile& ppf, size_t bits_per_sample,
                      ThreadPool* pool, size_t frame_index,
                      std::vector<uint8_t>* bytes) {
  const bool floating_point = bits_per_sample > 16;
  // Choose native for PFM; PGM/PPM require big-endian
  const JxlEndianness endianness =
      floating_point ? JXL_NATIVE_ENDIAN : JXL_BIG_ENDIAN;
  if (!ppf.metadata.exif.empty() || !ppf.metadata.iptc.empty() ||
      !ppf.metadata.jumbf.empty() || !ppf.metadata.xmp.empty()) {
    JXL_WARNING("PNM encoder ignoring metadata - use a different codec");
  }

  char header[kMaxHeaderSize];
  int header_size = 0;
  bool is_little_endian = endianness == JXL_LITTLE_ENDIAN ||
                          (endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());
  JXL_RETURN_IF_ERROR(EncodeHeader(ppf, bits_per_sample, is_little_endian,
                                   header, &header_size));
  bytes->resize(static_cast<size_t>(header_size) +
                ppf.frames[frame_index].color.pixels_size);
  memcpy(bytes->data(), header, static_cast<size_t>(header_size));
  memcpy(bytes->data() + header_size, ppf.frames[frame_index].color.pixels(),
         ppf.frames[frame_index].color.pixels_size);
  if (floating_point) {
    VerticallyFlipImage(reinterpret_cast<float*>(bytes->data() + header_size),
                        ppf.frames[frame_index].color.xsize,
                        ppf.frames[frame_index].color.ysize,
                        ppf.info.num_color_channels);
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
