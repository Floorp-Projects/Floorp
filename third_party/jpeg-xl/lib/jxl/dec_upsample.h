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

  // The caller must guarantee that `src:src_rect` has two pixels of padding
  // available on each side of the x dimension. `image_ysize` is the total
  // height of the frame that the source area belongs to (not the buffer);
  // `image_y_offset` is the difference between `src.y0()` and the corresponding
  // y value in the full frame.
  void UpsampleRect(const Image3F& src, const Rect& src_rect, Image3F* dst,
                    const Rect& dst_rect, ssize_t image_y_offset,
                    size_t image_ysize) const;
  void UpsampleRect(const ImageF& src, const Rect& src_rect, ImageF* dst,
                    const Rect& dst_rect, ssize_t image_y_offset,
                    size_t image_ysize) const;

 private:
  size_t upsampling_ = 1;
  float kernel_[4][4][5][5];
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_UPSAMPLE_H_
