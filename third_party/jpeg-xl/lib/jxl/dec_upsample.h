// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_DEC_UPSAMPLE_H_
#define LIB_JXL_DEC_UPSAMPLE_H_

#include "lib/jxl/image.h"
#include "lib/jxl/image_metadata.h"

namespace jxl {

struct Upsampler {
  void Init(size_t upsampling, const CustomTransformData& data);

  // Only 1x, 2x, 4x and 8x upsampling is supported.
  static constexpr size_t max_upsampling() { return 8; }

  // To produce N x N upsampled pixels the [-2..2]x[-2..2] neighborhood of
  // input pixel is taken and dot-multiplied with N x N corresponding "kernels".
  // Thus the "kernel" is a 5 x 5 matrix of weights.
  static constexpr size_t filter_radius() { return 2; }

  // Calculate multiple upsampled cells at the same time.
  // Kernels are transposed - several kernels are multiplied by input
  // at the same time.  In case of 2x upsampling there are only 4 kernels.
  // If current target supports SIMD vectors longer than 4 floats, to reduce
  // the wasted multiplications we increase the effective kernel count.
  static constexpr size_t max_x_repeat() { return 4; }

  // Get the size of "arena" required for UpsampleRect;
  // "arena" should be an aligned piece of memory with at least `GetArenaSize()`
  // float values accessible.
  static size_t GetArenaSize(size_t max_dst_xsize);

  // The caller must guarantee that `src:src_rect` has two pixels of padding
  // available on each side of the x dimension. `image_ysize` is the total
  // height of the frame that the source area belongs to (not the buffer);
  // `image_y_offset` is the difference between `src.y0()` and the corresponding
  // y value in the full frame.
  void UpsampleRect(const Image3F& src, const Rect& src_rect, Image3F* dst,
                    const Rect& dst_rect, ssize_t image_y_offset,
                    size_t image_ysize, float* arena) const;
  void UpsampleRect(const ImageF& src, const Rect& src_rect, ImageF* dst,
                    const Rect& dst_rect, ssize_t image_y_offset,
                    size_t image_ysize, float* arena) const;

 private:
  size_t upsampling_ = 1;
  size_t x_repeat_ = 1;
  CacheAlignedUniquePtr kernel_storage_ = {nullptr};
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_UPSAMPLE_H_
