// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_COMMON_H_
#define LIB_JXL_COMMON_H_

// Shared constants.

#include <cstddef>

#ifndef JXL_HIGH_PRECISION
#define JXL_HIGH_PRECISION 1
#endif

// Macro that defines whether support for decoding JXL files to JPEG is enabled.
#ifndef JPEGXL_ENABLE_TRANSCODE_JPEG
#define JPEGXL_ENABLE_TRANSCODE_JPEG 1
#endif  // JPEGXL_ENABLE_TRANSCODE_JPEG

// Macro that defines whether support for decoding boxes is enabled.
#ifndef JPEGXL_ENABLE_BOXES
#define JPEGXL_ENABLE_BOXES 1
#endif  // JPEGXL_ENABLE_BOXES

namespace jxl {
// Some enums and typedefs used by more than one header file.

// Maximum number of passes in an image.
constexpr size_t kMaxNumPasses = 11;

// Maximum number of reference frames.
constexpr size_t kMaxNumReferenceFrames = 4;

}  // namespace jxl

#endif  // LIB_JXL_COMMON_H_
