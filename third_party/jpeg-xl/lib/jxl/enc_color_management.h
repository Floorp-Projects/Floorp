// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_COLOR_MANAGEMENT_H_
#define LIB_JXL_ENC_COLOR_MANAGEMENT_H_

// ICC profiles and color space conversions.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image.h"

namespace jxl {

// Run is thread-safe.
class ColorSpaceTransform {
 public:
  ColorSpaceTransform();
  ~ColorSpaceTransform();

  // Cannot copy (transforms_ holds pointers).
  ColorSpaceTransform(const ColorSpaceTransform&) = delete;
  ColorSpaceTransform& operator=(const ColorSpaceTransform&) = delete;

  // "Constructor"; allocates for up to `num_threads`, or returns false.
  // `intensity_target` is used for conversion to and from PQ, which is absolute
  // (1 always represents 10000 cd/m²) and thus needs scaling in linear space if
  // 1 is to represent another luminance level instead.
  Status Init(const ColorEncoding& c_src, const ColorEncoding& c_dst,
              float intensity_target, size_t xsize, size_t num_threads);

  float* BufSrc(const size_t thread) { return buf_src_.Row(thread); }

  float* BufDst(const size_t thread) { return buf_dst_.Row(thread); }

#if JPEGXL_ENABLE_SKCMS
  struct SkcmsICC;
  std::unique_ptr<SkcmsICC> skcms_icc_;
#else
  void* lcms_transform_;
#endif

  ImageF buf_src_;
  ImageF buf_dst_;
  float intensity_target_;
  size_t xsize_;
  bool skip_lcms_ = false;
  ExtraTF preprocess_ = ExtraTF::kNone;
  ExtraTF postprocess_ = ExtraTF::kNone;
};

// buf_X can either be from BufX() or caller-allocated, interleaved storage.
// `thread` must be less than the `num_threads` passed to Init.
// `t` is non-const because buf_* may be modified.
void DoColorSpaceTransform(ColorSpaceTransform* t, size_t thread,
                           const float* buf_src, float* buf_dst);

}  // namespace jxl

#endif  // LIB_JXL_ENC_COLOR_MANAGEMENT_H_
