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
#include "lib/extras/dec/jxl.h"
#include "lib/extras/dec/pgx.h"
#include "lib/extras/dec/pnm.h"

namespace jxl {
namespace extras {
namespace {

// Any valid encoding is larger (ensures codecs can read the first few bytes)
constexpr size_t kMinBytes = 9;

void BasenameAndExtension(std::string path, std::string* basename,
                          std::string* extension) {
  // Pattern: file.jxl
  size_t pos = path.find_last_of('.');
  if (pos < path.size()) {
    *basename = path.substr(0, pos);
    *extension = path.substr(pos);
    return;
  }
  // Pattern: jxl:-
  pos = path.find_first_of(':');
  if (pos < path.size()) {
    *basename = path.substr(pos + 1);
    *extension = "." + path.substr(0, pos);
    return;
  }
  // Extension not found
  *basename = path;
  *extension = "";
}

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

Codec CodecFromPath(std::string path, size_t* JXL_RESTRICT bits_per_sample,
                    std::string* basename, std::string* extension) {
  std::string base;
  std::string ext;
  BasenameAndExtension(path, &base, &ext);
  if (basename) *basename = base;
  if (extension) *extension = ext;

  std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
    return std::tolower(c, std::locale::classic());
  });
  if (ext == ".png") return Codec::kPNG;

  if (ext == ".jpg") return Codec::kJPG;
  if (ext == ".jpeg") return Codec::kJPG;

  if (ext == ".pgx") return Codec::kPGX;

  if (ext == ".pam") return Codec::kPNM;
  if (ext == ".pnm") return Codec::kPNM;
  if (ext == ".pgm") return Codec::kPNM;
  if (ext == ".ppm") return Codec::kPNM;
  if (ext == ".pfm") {
    if (bits_per_sample != nullptr) *bits_per_sample = 32;
    return Codec::kPNM;
  }

  if (ext == ".gif") return Codec::kGIF;

  if (ext == ".exr") return Codec::kEXR;

  return Codec::kUnknown;
}

Status DecodeBytes(const Span<const uint8_t> bytes,
                   const ColorHints& color_hints, extras::PackedPixelFile* ppf,
                   const SizeConstraints* constraints, Codec* orig_codec) {
  if (bytes.size() < kMinBytes) return JXL_FAILURE("Too few bytes");

  *ppf = extras::PackedPixelFile();

  // Default values when not set by decoders.
  ppf->info.uses_original_profile = true;
  ppf->info.orientation = JXL_ORIENT_IDENTITY;

  const auto choose_codec = [&]() -> Codec {
#if JPEGXL_ENABLE_APNG
    if (DecodeImageAPNG(bytes, color_hints, ppf, constraints)) {
      return Codec::kPNG;
    }
#endif
    if (DecodeImagePGX(bytes, color_hints, ppf, constraints)) {
      return Codec::kPGX;
    }
    if (DecodeImagePNM(bytes, color_hints, ppf, constraints)) {
      return Codec::kPNM;
    }
    JXLDecompressParams dparams = {};
    size_t decoded_bytes;
    if (DecodeImageJXL(bytes.data(), bytes.size(), dparams, &decoded_bytes,
                       ppf)) {
      return Codec::kJXL;
    }
#if JPEGXL_ENABLE_GIF
    if (DecodeImageGIF(bytes, color_hints, ppf, constraints)) {
      return Codec::kGIF;
    }
#endif
#if JPEGXL_ENABLE_JPEG
    if (DecodeImageJPG(bytes, color_hints, ppf, constraints)) {
      return Codec::kJPG;
    }
#endif
#if JPEGXL_ENABLE_EXR
    if (DecodeImageEXR(bytes, color_hints, ppf, constraints)) {
      return Codec::kEXR;
    }
#endif
    return Codec::kUnknown;
  };

  Codec codec = choose_codec();
  if (codec == Codec::kUnknown) {
    return JXL_FAILURE("Codecs failed to decode");
  }
  if (orig_codec) *orig_codec = codec;

  return true;
}

}  // namespace extras
}  // namespace jxl
