// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_JXL_INSPECTION_H_
#define LIB_JXL_JXL_INSPECTION_H_

#include <functional>

#include "lib/jxl/image.h"

namespace jxl {
// Type of the inspection-callback which, if enabled, will be called on various
// intermediate data during image processing, allowing inspection access.
//
// Returns false if processing can be stopped at that point, true otherwise.
// This is only advisory - it is always OK to just continue processing.
using InspectorImage3F = std::function<bool(const char*, const Image3F&)>;
}  // namespace jxl

#endif  // LIB_JXL_JXL_INSPECTION_H_
