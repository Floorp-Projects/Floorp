// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_EXIF_H_
#define LIB_JXL_EXIF_H_

// Basic parsing of Exif (just enough for the render-impacting things
// like orientation)

#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/image_metadata.h"

namespace jxl {

// Parses the Exif data just enough to extract any render-impacting info.
// If the Exif data is invalid or could not be parsed, then it is treated
// as a no-op.
void InterpretExif(const std::vector<uint8_t>& exif, CodecMetadata* metadata);

// Sets the Exif orientation to the identity, to avoid repeated orientation
void ResetExifOrientation(std::vector<uint8_t>& exif);

}  // namespace jxl

#endif  // LIB_JXL_EXIF_H_
