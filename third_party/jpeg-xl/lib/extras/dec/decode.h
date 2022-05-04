// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_DECODE_H_
#define LIB_EXTRAS_DEC_DECODE_H_

// Facade for image decoders (PNG, PNM, ...).

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "lib/extras/dec/color_hints.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/field_encodings.h"  // MakeBit

namespace jxl {
namespace extras {

// Codecs supported by CodecInOut::Encode.
enum class Codec : uint32_t {
  kUnknown,  // for CodecFromExtension
  kPNG,
  kPNM,
  kPGX,
  kJPG,
  kGIF,
  kEXR
};

static inline constexpr uint64_t EnumBits(Codec /*unused*/) {
  // Return only fully-supported codecs (kGIF is decode-only).
  return MakeBit(Codec::kPNM)
#if JPEGXL_ENABLE_APNG
         | MakeBit(Codec::kPNG)
#endif
#if JPEGXL_ENABLE_JPEG
         | MakeBit(Codec::kJPG)
#endif
#if JPEGXL_ENABLE_EXR
         | MakeBit(Codec::kEXR)
#endif
      ;
}

// If and only if extension is ".pfm", *bits_per_sample is updated to 32 so
// that Encode() would encode to PFM instead of PPM.
Codec CodecFromExtension(std::string extension,
                         size_t* JXL_RESTRICT bits_per_sample = nullptr);

// Decodes "bytes" and sets io->metadata.m.
// color_space_hint may specify the color space, otherwise, defaults to sRGB.
Status DecodeBytes(Span<const uint8_t> bytes, const ColorHints& color_hints,
                   const SizeConstraints& constraints,
                   extras::PackedPixelFile* ppf, Codec* orig_codec = nullptr);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_DECODE_H_
