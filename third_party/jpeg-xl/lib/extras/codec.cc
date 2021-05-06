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

#include "lib/extras/codec.h"

#include "lib/jxl/base/file_io.h"
#if JPEGXL_ENABLE_APNG
#include "lib/extras/codec_apng.h"
#endif
#if JPEGXL_ENABLE_EXR
#include "lib/extras/codec_exr.h"
#endif
#if JPEGXL_ENABLE_GIF
#include "lib/extras/codec_gif.h"
#endif
#include "lib/extras/codec_jpg.h"
#include "lib/extras/codec_pgx.h"
#include "lib/extras/codec_png.h"
#include "lib/extras/codec_pnm.h"
#include "lib/extras/codec_psd.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {
namespace {

// Any valid encoding is larger (ensures codecs can read the first few bytes)
constexpr size_t kMinBytes = 9;

}  // namespace

std::string ExtensionFromCodec(Codec codec, const bool is_gray,
                               const size_t bits_per_sample) {
  switch (codec) {
    case Codec::kJPG:
      return ".jpg";
    case Codec::kPGX:
      return ".pgx";
    case Codec::kPNG:
      return ".png";
    case Codec::kPNM:
      if (is_gray) return ".pgm";
      return (bits_per_sample == 32) ? ".pfm" : ".ppm";
    case Codec::kGIF:
      return ".gif";
    case Codec::kEXR:
      return ".exr";
    case Codec::kPSD:
      return ".psd";
    case Codec::kUnknown:
      return std::string();
  }
  JXL_UNREACHABLE;
  return std::string();
}

Codec CodecFromExtension(const std::string& extension,
                         size_t* JXL_RESTRICT bits_per_sample) {
  if (extension == ".png") return Codec::kPNG;

  if (extension == ".jpg") return Codec::kJPG;
  if (extension == ".jpeg") return Codec::kJPG;

  if (extension == ".pgx") return Codec::kPGX;

  if (extension == ".pbm") {
    *bits_per_sample = 1;
    return Codec::kPNM;
  }
  if (extension == ".pgm") return Codec::kPNM;
  if (extension == ".ppm") return Codec::kPNM;
  if (extension == ".pfm") {
    *bits_per_sample = 32;
    return Codec::kPNM;
  }

  if (extension == ".gif") return Codec::kGIF;

  if (extension == ".exr") return Codec::kEXR;

  if (extension == ".psd") return Codec::kPSD;

  return Codec::kUnknown;
}

Status SetFromBytes(const Span<const uint8_t> bytes, CodecInOut* io,
                    ThreadPool* pool, Codec* orig_codec) {
  if (bytes.size() < kMinBytes) return JXL_FAILURE("Too few bytes");

  io->metadata.m.bit_depth.bits_per_sample = 0;  // (For is-set check below)

  Codec codec;
  if (DecodeImagePNG(bytes, pool, io)) {
    codec = Codec::kPNG;
  }
#if JPEGXL_ENABLE_APNG
  else if (DecodeImageAPNG(bytes, pool, io)) {
    codec = Codec::kPNG;
  }
#endif
  else if (DecodeImagePGX(bytes, pool, io)) {
    codec = Codec::kPGX;
  } else if (DecodeImagePNM(bytes, pool, io)) {
    codec = Codec::kPNM;
  }
#if JPEGXL_ENABLE_GIF
  else if (DecodeImageGIF(bytes, pool, io)) {
    codec = Codec::kGIF;
  }
#endif
  else if (DecodeImageJPG(bytes, pool, io)) {
    codec = Codec::kJPG;
  }
  else if (DecodeImagePSD(bytes, pool, io)) {
    codec = Codec::kPSD;
  }
#if JPEGXL_ENABLE_EXR
  else if (DecodeImageEXR(bytes, pool, io)) {
    codec = Codec::kEXR;
  }
#endif
  else {
    return JXL_FAILURE("Codecs failed to decode");
  }
  if (orig_codec) *orig_codec = codec;

  io->CheckMetadata();
  return true;
}

Status SetFromFile(const std::string& pathname, CodecInOut* io,
                   ThreadPool* pool, Codec* orig_codec) {
  PaddedBytes encoded;
  JXL_RETURN_IF_ERROR(ReadFile(pathname, &encoded));
  JXL_RETURN_IF_ERROR(
      SetFromBytes(Span<const uint8_t>(encoded), io, pool, orig_codec));
  return true;
}

Status Encode(const CodecInOut& io, const Codec codec,
              const ColorEncoding& c_desired, size_t bits_per_sample,
              PaddedBytes* bytes, ThreadPool* pool) {
  JXL_CHECK(!io.Main().c_current().ICC().empty());
  JXL_CHECK(!c_desired.ICC().empty());
  io.CheckMetadata();
  if (io.Main().IsJPEG() && codec != Codec::kJPG) {
    return JXL_FAILURE(
        "Output format has to be JPEG for losslessly recompressed JPEG "
        "reconstruction");
  }

  switch (codec) {
    case Codec::kPNG:
      return EncodeImagePNG(&io, c_desired, bits_per_sample, pool, bytes);
    case Codec::kJPG:
#if JPEGXL_ENABLE_JPEG
      return EncodeImageJPG(
          &io, io.use_sjpeg ? JpegEncoder::kSJpeg : JpegEncoder::kLibJpeg,
          io.jpeg_quality, YCbCrChromaSubsampling(), pool, bytes,
          io.Main().IsJPEG() ? DecodeTarget::kQuantizedCoeffs
                             : DecodeTarget::kPixels);
#else
      return JXL_FAILURE("JPEG XL was built without JPEG support");
#endif
    case Codec::kPNM:
      return EncodeImagePNM(&io, c_desired, bits_per_sample, pool, bytes);
    case Codec::kPGX:
      return EncodeImagePGX(&io, c_desired, bits_per_sample, pool, bytes);
    case Codec::kGIF:
      return JXL_FAILURE("Encoding to GIF is not implemented");
    case Codec::kPSD:
      return EncodeImagePSD(&io, c_desired, bits_per_sample, pool, bytes);
    case Codec::kEXR:
#if JPEGXL_ENABLE_EXR
      return EncodeImageEXR(&io, c_desired, pool, bytes);
#else
      return JXL_FAILURE("JPEG XL was built without OpenEXR support");
#endif
    case Codec::kUnknown:
      return JXL_FAILURE("Cannot encode using Codec::kUnknown");
  }

  return JXL_FAILURE("Invalid codec");
}

Status EncodeToFile(const CodecInOut& io, const ColorEncoding& c_desired,
                    size_t bits_per_sample, const std::string& pathname,
                    ThreadPool* pool) {
  const std::string extension = Extension(pathname);
  const Codec codec = CodecFromExtension(extension, &bits_per_sample);

  // Warn about incorrect usage of PBM/PGM/PGX/PPM - only the latter supports
  // color, but CodecFromExtension lumps them all together.
  if (codec == Codec::kPNM && extension != ".pfm") {
    if (!io.Main().IsGray() && extension != ".ppm") {
      JXL_WARNING("For color images, the filename should end with .ppm.\n");
    } else if (io.Main().IsGray() && extension == ".ppm") {
      JXL_WARNING(
          "For grayscale images, the filename should not end with .ppm.\n");
    }
    if (bits_per_sample > 16) {
      JXL_WARNING("PPM only supports up to 16 bits per sample");
      bits_per_sample = 16;
    }
  } else if (codec == Codec::kPGX && !io.Main().IsGray()) {
    JXL_WARNING("Storing color image to PGX - use .ppm extension instead.\n");
  }
  if (bits_per_sample > 16 && codec == Codec::kPNG) {
    JXL_WARNING("PNG only supports up to 16 bits per sample");
    bits_per_sample = 16;
  }

  PaddedBytes encoded;
  return Encode(io, codec, c_desired, bits_per_sample, &encoded, pool) &&
         WriteFile(encoded, pathname);
}

Status EncodeToFile(const CodecInOut& io, const std::string& pathname,
                    ThreadPool* pool) {
  // TODO(lode): need to take the floating_point_sample field into account
  return EncodeToFile(io, io.metadata.m.color_encoding,
                      io.metadata.m.bit_depth.bits_per_sample, pathname, pool);
}

}  // namespace jxl
