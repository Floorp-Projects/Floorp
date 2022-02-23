// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
#include "lib/extras/packed_image_convert.h"
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

Status SetFromBytes(const Span<const uint8_t> bytes,
                    const ColorHints& color_hints, CodecInOut* io,
                    ThreadPool* pool, Codec* orig_codec) {
  if (bytes.size() < kMinBytes) return JXL_FAILURE("Too few bytes");

  extras::PackedPixelFile ppf;
  // Default values when not set by decoders.
  ppf.info.uses_original_profile = true;
  ppf.info.orientation = JXL_ORIENT_IDENTITY;

  Codec codec;
  bool skip_ppf_conversion = false;
  if (extras::DecodeImagePNG(bytes, color_hints, io->constraints, &ppf)) {
    codec = Codec::kPNG;
  }
#if JPEGXL_ENABLE_APNG
  else if (extras::DecodeImageAPNG(bytes, color_hints, io->constraints, &ppf)) {
    codec = Codec::kPNG;
  }
#endif
  else if (extras::DecodeImagePGX(bytes, color_hints, io->constraints, &ppf)) {
    codec = Codec::kPGX;
  } else if (extras::DecodeImagePNM(bytes, color_hints, io->constraints,
                                    &ppf)) {
    codec = Codec::kPNM;
  }
#if JPEGXL_ENABLE_GIF
  else if (extras::DecodeImageGIF(bytes, color_hints, io->constraints, &ppf)) {
    codec = Codec::kGIF;
  }
#endif
  else if (io->dec_target == DecodeTarget::kQuantizedCoeffs &&
           extras::DecodeImageJPGCoefficients(bytes, io)) {
    // TODO(deymo): In this case the tools should use a different API to
    // transcode the input JPEG to JXL instead of expressing it as a
    // PackedPixelFile.
    codec = Codec::kJPG;
    skip_ppf_conversion = true;
  } else if (io->dec_target == DecodeTarget::kPixels &&
             extras::DecodeImageJPG(bytes, color_hints, io->constraints,
                                    &ppf)) {
    codec = Codec::kJPG;
  } else if (extras::DecodeImagePSD(bytes, color_hints, pool, io)) {
    // TODO(deymo): Migrate PSD codec too.
    codec = Codec::kPSD;
    skip_ppf_conversion = true;
  }
#if JPEGXL_ENABLE_EXR
  else if (extras::DecodeImageEXR(bytes, color_hints, io->constraints,
                                  io->target_nits, pool, &ppf)) {
    codec = Codec::kEXR;
  }
#endif
  else {
    return JXL_FAILURE("Codecs failed to decode");
  }
  if (orig_codec) *orig_codec = codec;

  if (!skip_ppf_conversion) {
    JXL_RETURN_IF_ERROR(ConvertPackedPixelFileToCodecInOut(ppf, pool, io));
  }

  return true;
}

Status SetFromFile(const std::string& pathname, const ColorHints& color_hints,
                   CodecInOut* io, ThreadPool* pool, Codec* orig_codec) {
  PaddedBytes encoded;
  JXL_RETURN_IF_ERROR(ReadFile(pathname, &encoded));
  JXL_RETURN_IF_ERROR(SetFromBytes(Span<const uint8_t>(encoded), color_hints,
                                   io, pool, orig_codec));
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
      return extras::EncodeImagePNG(&io, c_desired, bits_per_sample, pool,
                                    bytes);
    case Codec::kJPG:
      if (io.Main().IsJPEG()) {
        return extras::EncodeImageJPGCoefficients(&io, bytes);
      } else {
#if JPEGXL_ENABLE_JPEG
        return EncodeImageJPG(&io,
                              io.use_sjpeg ? extras::JpegEncoder::kSJpeg
                                           : extras::JpegEncoder::kLibJpeg,
                              io.jpeg_quality, YCbCrChromaSubsampling(), pool,
                              bytes);
#else
        return JXL_FAILURE("JPEG XL was built without JPEG support");
#endif
      }
    case Codec::kPNM:
      return extras::EncodeImagePNM(&io, c_desired, bits_per_sample, pool,
                                    bytes);
    case Codec::kPGX:
      return extras::EncodeImagePGX(&io, c_desired, bits_per_sample, pool,
                                    bytes);
    case Codec::kGIF:
      return JXL_FAILURE("Encoding to GIF is not implemented");
    case Codec::kPSD:
      return extras::EncodeImagePSD(&io, c_desired, bits_per_sample, pool,
                                    bytes);
    case Codec::kEXR:
#if JPEGXL_ENABLE_EXR
      return extras::EncodeImageEXR(&io, c_desired, pool, bytes);
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
