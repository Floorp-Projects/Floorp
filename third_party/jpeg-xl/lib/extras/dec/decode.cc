// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/decode.h"

#include <locale>

#if JPEGXL_ENABLE_APNG
#include "lib/extras/dec/apng.h"
#endif
#if JPEGXL_ENABLE_EXR
#include "lib/extras/dec/exr.h"
#endif
#if JPEGXL_ENABLE_GIF
#include "lib/extras/dec/gif.h"
#endif
#if JPEGXL_ENABLE_JPEG
#include "lib/extras/dec/jpg.h"
#endif
#include "lib/extras/dec/pgx.h"
#include "lib/extras/dec/pnm.h"

namespace jxl {
namespace extras {
namespace {

// Any valid encoding is larger (ensures codecs can read the first few bytes)
constexpr size_t kMinBytes = 9;

}  // namespace

std::vector<Codec> AvailableCodecs() {
  std::vector<Codec> out;
#if JPEGXL_ENABLE_APNG
  out.push_back(Codec::kPNG);
#endif
#if JPEGXL_ENABLE_EXR
  out.push_back(Codec::kEXR);
#endif
#if JPEGXL_ENABLE_GIF
  out.push_back(Codec::kGIF);
#endif
#if JPEGXL_ENABLE_JPEG
  out.push_back(Codec::kJPG);
#endif
  out.push_back(Codec::kPGX);
  out.push_back(Codec::kPNM);
  return out;
}

Codec CodecFromExtension(std::string extension,
                         size_t* JXL_RESTRICT bits_per_sample) {
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](char c) { return std::tolower(c, std::locale::classic()); });
  if (extension == ".png") return Codec::kPNG;

  if (extension == ".jpg") return Codec::kJPG;
  if (extension == ".jpeg") return Codec::kJPG;

  if (extension == ".pgx") return Codec::kPGX;

  if (extension == ".pam") return Codec::kPNM;
  if (extension == ".pnm") return Codec::kPNM;
  if (extension == ".pgm") return Codec::kPNM;
  if (extension == ".ppm") return Codec::kPNM;
  if (extension == ".pfm") {
    if (bits_per_sample != nullptr) *bits_per_sample = 32;
    return Codec::kPNM;
  }

  if (extension == ".gif") return Codec::kGIF;

  if (extension == ".exr") return Codec::kEXR;

  return Codec::kUnknown;
}

Status DecodeBytes(const Span<const uint8_t> bytes,
                   const ColorHints& color_hints,
                   const SizeConstraints& constraints,
                   extras::PackedPixelFile* ppf, Codec* orig_codec) {
  if (bytes.size() < kMinBytes) return JXL_FAILURE("Too few bytes");

  *ppf = extras::PackedPixelFile();

  // Default values when not set by decoders.
  ppf->info.uses_original_profile = true;
  ppf->info.orientation = JXL_ORIENT_IDENTITY;

  Codec codec;
#if JPEGXL_ENABLE_APNG
  if (DecodeImageAPNG(bytes, color_hints, constraints, ppf)) {
    codec = Codec::kPNG;
  } else
#endif
      if (DecodeImagePGX(bytes, color_hints, constraints, ppf)) {
    codec = Codec::kPGX;
  } else if (DecodeImagePNM(bytes, color_hints, constraints, ppf)) {
    codec = Codec::kPNM;
  }
#if JPEGXL_ENABLE_GIF
  else if (DecodeImageGIF(bytes, color_hints, constraints, ppf)) {
    codec = Codec::kGIF;
  }
#endif
#if JPEGXL_ENABLE_JPEG
  else if (DecodeImageJPG(bytes, color_hints, constraints, ppf)) {
    codec = Codec::kJPG;
  }
#endif
#if JPEGXL_ENABLE_EXR
  else if (DecodeImageEXR(bytes, color_hints, constraints, ppf)) {
    codec = Codec::kEXR;
  }
#endif
  else {
    return JXL_FAILURE("Codecs failed to decode");
  }
  if (orig_codec) *orig_codec = codec;

  return true;
}

}  // namespace extras
}  // namespace jxl
