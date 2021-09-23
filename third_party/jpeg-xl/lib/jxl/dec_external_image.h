// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_DEC_EXTERNAL_IMAGE_H_
#define LIB_JXL_DEC_EXTERNAL_IMAGE_H_

// Interleaved image for color transforms and Codec.

#include <stddef.h>
#include <stdint.h>

#include "jxl/decode.h"
#include "jxl/types.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Converts ib to interleaved void* pixel buffer with the given format.
// bits_per_sample: must be 8, 16 or 32, and must be 32 if float_out
// is true. 1 and 32 int are not yet implemented.
// num_channels: must be 1, 2, 3 or 4 for gray, gray+alpha, RGB, RGB+alpha.
// This supports the features needed for the C API and does not perform
// color space conversion.
// TODO(lode): support 1-bit output (bits_per_sample == 1)
// TODO(lode): support rectangle crop.
// stride_out is output scanline size in bytes, must be >=
// output_xsize * output_bytes_per_pixel.
// undo_orientation is an EXIF orientation to undo. Depending on the
// orientation, the output xsize and ysize are swapped compared to input
// xsize and ysize.
Status ConvertToExternal(const jxl::ImageBundle& ib, size_t bits_per_sample,
                         bool float_out, size_t num_channels,
                         JxlEndianness endianness, size_t stride_out,
                         jxl::ThreadPool* thread_pool, void* out_image,
                         size_t out_size, JxlImageOutCallback out_callback,
                         void* out_opaque, jxl::Orientation undo_orientation);

}  // namespace jxl

#endif  // LIB_JXL_DEC_EXTERNAL_IMAGE_H_
