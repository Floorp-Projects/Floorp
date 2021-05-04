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

#ifndef LIB_JXL_ENC_EXTERNAL_IMAGE_H_
#define LIB_JXL_ENC_EXTERNAL_IMAGE_H_

// Interleaved image for color transforms and Codec.

#include <stddef.h>
#include <stdint.h>

#include "jxl/types.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Return the size in bytes of a given xsize, channels and bits_per_sample
// interleaved image.
constexpr size_t RowSize(size_t xsize, size_t channels,
                         size_t bits_per_sample) {
  return bits_per_sample == 1
             ? DivCeil(xsize, kBitsPerByte)
             : xsize * channels * DivCeil(bits_per_sample, kBitsPerByte);
}

// Convert an interleaved pixel buffer to the internal ImageBundle
// representation. This is the opposite of ConvertToExternal().
Status ConvertFromExternal(Span<const uint8_t> bytes, size_t xsize,
                           size_t ysize, const ColorEncoding& c_current,
                           bool has_alpha, bool alpha_is_premultiplied,
                           size_t bits_per_sample, JxlEndianness endianness,
                           bool flipped_y, ThreadPool* pool, ImageBundle* ib);

Status BufferToImageBundle(const JxlPixelFormat& pixel_format, uint32_t xsize,
                           uint32_t ysize, const void* buffer, size_t size,
                           jxl::ThreadPool* pool,
                           const jxl::ColorEncoding& c_current,
                           jxl::ImageBundle* ib);

}  // namespace jxl

#endif  // LIB_JXL_ENC_EXTERNAL_IMAGE_H_
