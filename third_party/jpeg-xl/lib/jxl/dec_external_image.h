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
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Converts ib to interleaved void* pixel buffer with the given format.
// bits_per_sample: must be 16 or 32 if float_out is true, and at most 16
// if it is false. No bit packing is done.
// num_channels: must be 1, 2, 3 or 4 for gray, gray+alpha, RGB, RGB+alpha.
// This supports the features needed for the C API and does not perform
// color space conversion.
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
                         size_t out_size, const PixelCallback& out_callback,
                         jxl::Orientation undo_orientation,
                         bool unpremul_alpha = false);

// Converts single-channel image to interleaved void* pixel buffer with the
// given format, with a single channel.
// This supports the features needed for the C API to get extra channels.
// Arguments are similar to the multi-channel function above.
Status ConvertToExternal(const jxl::ImageF& channel, size_t bits_per_sample,
                         bool float_out, JxlEndianness endianness,
                         size_t stride_out, jxl::ThreadPool* thread_pool,
                         void* out_image, size_t out_size,
                         const PixelCallback& out_callback,
                         jxl::Orientation undo_orientation);
}  // namespace jxl

#endif  // LIB_JXL_DEC_EXTERNAL_IMAGE_H_
