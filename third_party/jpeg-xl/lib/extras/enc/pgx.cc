// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/pgx.h"

#include <stdio.h>
#include <string.h>

#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/printf_macros.h"
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
                    char* header, int* JXL_RESTRICT chars_written) {
  if (ib.HasAlpha()) return JXL_FAILURE("PGX: can't store alpha");
  if (!ib.IsGray()) return JXL_FAILURE("PGX: must be grayscale");
  // TODO(lode): verify other bit depths: for other bit depths such as 1 or 4
  // bits, have a test case to verify it works correctly. For bits > 16, we may
  // need to change the way external_image works.
  if (bits_per_sample != 8 && bits_per_sample != 16) {
    return JXL_FAILURE("PGX: bits other than 8 or 16 not yet supported");
  }

  // Use ML (Big Endian), LM may not be well supported by all decoders.
  *chars_written = snprintf(header, kMaxHeaderSize,
                            "PG ML + %" PRIuS " %" PRIuS " %" PRIuS "\n",
                            bits_per_sample, ib.xsize(), ib.ysize());
  JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                      kMaxHeaderSize);
  return true;
}

}  // namespace

Status EncodeImagePGX(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes) {
  if (!Bundle::AllDefault(io->metadata.m)) {
    JXL_WARNING("PGX encoder ignoring metadata - use a different codec");
  }
  if (!c_desired.IsSRGB()) {
    JXL_WARNING(
        "PGX encoder cannot store custom ICC profile; decoder\n"
        "will need hint key=color_space to get the same values");
  }

  ImageBundle ib = io->Main().Copy();

  ImageMetadata metadata = io->metadata.m;
  ImageBundle store(&metadata);
  const ImageBundle* transformed;
  JXL_RETURN_IF_ERROR(TransformIfNeeded(ib, c_desired, GetJxlCms(), pool,
                                        &store, &transformed));
  PaddedBytes pixels(ib.xsize() * ib.ysize() *
                     (bits_per_sample / kBitsPerByte));
  size_t stride = ib.xsize() * (bits_per_sample / kBitsPerByte);
  JXL_RETURN_IF_ERROR(
      ConvertToExternal(*transformed, bits_per_sample,
                        /*float_out=*/false,
                        /*num_channels=*/1, JXL_BIG_ENDIAN, stride, pool,
                        pixels.data(), pixels.size(),
                        /*out_callback=*/{}, metadata.GetOrientation()));

  char header[kMaxHeaderSize];
  int header_size = 0;
  JXL_RETURN_IF_ERROR(EncodeHeader(ib, bits_per_sample, header, &header_size));

  bytes->resize(static_cast<size_t>(header_size) + pixels.size());
  memcpy(bytes->data(), header, static_cast<size_t>(header_size));
  memcpy(bytes->data() + header_size, pixels.data(), pixels.size());

  return true;
}

}  // namespace extras
}  // namespace jxl
