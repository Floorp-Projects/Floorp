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

#ifndef LIB_EXTRAS_CODEC_H_
#define LIB_EXTRAS_CODEC_H_

// Facade for image encoders/decoders (PNG, PNM, ...).

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/field_encodings.h"  // MakeBit

namespace jxl {

// Codecs supported by CodecInOut::Encode.
enum class Codec : uint32_t {
  kUnknown,  // for CodecFromExtension
  kPNG,
  kPNM,
  kPGX,
  kJPG,
  kGIF,
  kEXR,
  kPSD
};

static inline constexpr uint64_t EnumBits(Codec /*unused*/) {
  // Return only fully-supported codecs (kGIF is decode-only).
  return MakeBit(Codec::kPNM) | MakeBit(Codec::kPNG)
#if JPEGXL_ENABLE_JPEG
         | MakeBit(Codec::kJPG)
#endif
#if JPEGXL_ENABLE_EXR
         | MakeBit(Codec::kEXR)
#endif
         | MakeBit(Codec::kPSD);
}

// Lower case ASCII including dot, e.g. ".png".
std::string ExtensionFromCodec(Codec codec, bool is_gray,
                               size_t bits_per_sample);

// If and only if extension is ".pfm", *bits_per_sample is updated to 32 so
// that Encode() would encode to PFM instead of PPM.
Codec CodecFromExtension(const std::string& extension,
                         size_t* JXL_RESTRICT bits_per_sample);

// Decodes "bytes" and sets io->metadata.m.
// dec_hints may specify the "color_space" (otherwise, defaults to sRGB).
Status SetFromBytes(const Span<const uint8_t> bytes, CodecInOut* io,
                    ThreadPool* pool = nullptr, Codec* orig_codec = nullptr);

// Reads from file and calls SetFromBytes.
Status SetFromFile(const std::string& pathname, CodecInOut* io,
                   ThreadPool* pool = nullptr, Codec* orig_codec = nullptr);

// Replaces "bytes" with an encoding of pixels transformed from c_current
// color space to c_desired.
Status Encode(const CodecInOut& io, Codec codec, const ColorEncoding& c_desired,
              size_t bits_per_sample, PaddedBytes* bytes,
              ThreadPool* pool = nullptr);

// Deduces codec, calls Encode and writes to file.
Status EncodeToFile(const CodecInOut& io, const ColorEncoding& c_desired,
                    size_t bits_per_sample, const std::string& pathname,
                    ThreadPool* pool = nullptr);
// Same, but defaults to metadata.original color_encoding and bits_per_sample.
Status EncodeToFile(const CodecInOut& io, const std::string& pathname,
                    ThreadPool* pool = nullptr);

}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_H_
