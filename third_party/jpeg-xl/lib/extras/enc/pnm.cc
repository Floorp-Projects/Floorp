// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/pnm.h"

#include <stdio.h>
#include <string.h>

#include <string>

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

Status EncodeHeader(const ImageBundle& ib, const size_t bits_per_sample,
                    const bool little_endian, char* header,
                    int* JXL_RESTRICT chars_written) {
  if (ib.HasAlpha()) return JXL_FAILURE("PNM: can't store alpha");

  if (bits_per_sample == 32) {  // PFM
    const char type = ib.IsGray() ? 'f' : 'F';
    const double scale = little_endian ? -1.0 : 1.0;
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%.1f\n",
                 type, ib.oriented_xsize(), ib.oriented_ysize(), scale);
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else if (bits_per_sample == 1) {  // PBM
    if (!ib.IsGray()) {
      return JXL_FAILURE("Cannot encode color as PBM");
    }
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P4\n%" PRIuS " %" PRIuS "\n",
                 ib.oriented_xsize(), ib.oriented_ysize());
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else {  // PGM/PPM
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    if (max_val >= 65536) return JXL_FAILURE("PNM cannot have > 16 bits");
    const char type = ib.IsGray() ? '5' : '6';
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%u\n",
                 type, ib.oriented_xsize(), ib.oriented_ysize(), max_val);
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
void VerticallyFlipImage(Image3F* const image) {
  for (int c = 0; c < 3; c++) {
    for (size_t y = 0; y < image->ysize() / 2; y++) {
      float* first_row = image->PlaneRow(c, y);
      float* other_row = image->PlaneRow(c, image->ysize() - y - 1);
      for (size_t x = 0; x < image->xsize(); ++x) {
        float tmp = first_row[x];
        first_row[x] = other_row[x];
        other_row[x] = tmp;
      }
    }
  }
}

}  // namespace

Status EncodeImagePNM(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes) {
  const bool floating_point = bits_per_sample > 16;
  // Choose native for PFM; PGM/PPM require big-endian (N/A for PBM)
  const JxlEndianness endianness =
      floating_point ? JXL_NATIVE_ENDIAN : JXL_BIG_ENDIAN;

  ImageMetadata metadata_copy = io->metadata.m;
  // AllDefault sets all_default, which can cause a race condition.
  if (!Bundle::AllDefault(metadata_copy)) {
    JXL_WARNING("PNM encoder ignoring metadata - use a different codec");
  }
  if (!c_desired.IsSRGB()) {
    JXL_WARNING(
        "PNM encoder cannot store custom ICC profile; decoder\n"
        "will need hint key=color_space to get the same values");
  }

  ImageBundle ib = io->Main().Copy();
  // In case of PFM the image must be flipped upside down since that format
  // is designed that way.
  const ImageBundle* to_color_transform = &ib;
  ImageBundle flipped;
  if (floating_point) {
    flipped = ib.Copy();
    VerticallyFlipImage(flipped.color());
    to_color_transform = &flipped;
  }
  ImageMetadata metadata = io->metadata.m;
  ImageBundle store(&metadata);
  const ImageBundle* transformed;
  JXL_RETURN_IF_ERROR(TransformIfNeeded(
      *to_color_transform, c_desired, GetJxlCms(), pool, &store, &transformed));
  size_t stride = ib.oriented_xsize() *
                  (c_desired.Channels() * bits_per_sample) / kBitsPerByte;
  PaddedBytes pixels(stride * ib.oriented_ysize());
  JXL_RETURN_IF_ERROR(ConvertToExternal(
      *transformed, bits_per_sample, floating_point, c_desired.Channels(),
      endianness, stride, pool, pixels.data(), pixels.size(),
      /*out_callback=*/nullptr, /*out_opaque=*/nullptr,
      metadata.GetOrientation()));

  char header[kMaxHeaderSize];
  int header_size = 0;
  bool is_little_endian = endianness == JXL_LITTLE_ENDIAN ||
                          (endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());
  JXL_RETURN_IF_ERROR(EncodeHeader(*transformed, bits_per_sample,
                                   is_little_endian, header, &header_size));

  bytes->resize(static_cast<size_t>(header_size) + pixels.size());
  memcpy(bytes->data(), header, static_cast<size_t>(header_size));
  memcpy(bytes->data() + header_size, pixels.data(), pixels.size());

  return true;
}

}  // namespace extras
}  // namespace jxl
