// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/codec.h"

#include <jxl/decode.h>
#include <jxl/types.h>

#include "lib/extras/dec/decode.h"
#include "lib/extras/enc/apng.h"
#include "lib/extras/enc/exr.h"
#include "lib/extras/enc/jpg.h"
#include "lib/extras/enc/pgx.h"
#include "lib/extras/enc/pnm.h"
#include "lib/extras/packed_image.h"
#include "lib/extras/packed_image_convert.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {
namespace {

// Any valid encoding is larger (ensures codecs can read the first few bytes)
constexpr size_t kMinBytes = 9;

}  // namespace

Status SetFromBytes(const Span<const uint8_t> bytes,
                    const extras::ColorHints& color_hints, CodecInOut* io,
                    ThreadPool* pool, const SizeConstraints* constraints,
                    extras::Codec* orig_codec) {
  if (bytes.size() < kMinBytes) return JXL_FAILURE("Too few bytes");

  extras::PackedPixelFile ppf;
  if (extras::DecodeBytes(bytes, color_hints, &ppf, constraints, orig_codec)) {
    return ConvertPackedPixelFileToCodecInOut(ppf, pool, io);
  }
  return JXL_FAILURE("Codecs failed to decode");
}

Status Encode(const extras::PackedPixelFile& ppf, const extras::Codec codec,
              std::vector<uint8_t>* bytes, ThreadPool* pool) {
  bytes->clear();
  std::unique_ptr<extras::Encoder> encoder;
  switch (codec) {
    case extras::Codec::kPNG:
      encoder = extras::GetAPNGEncoder();
      if (encoder) {
        break;
      } else {
        return JXL_FAILURE("JPEG XL was built without (A)PNG support");
      }
    case extras::Codec::kJPG:
      encoder = extras::GetJPEGEncoder();
      if (encoder) {
        break;
      } else {
        return JXL_FAILURE("JPEG XL was built without JPEG support");
      }
    case extras::Codec::kPNM:
      if (ppf.info.alpha_bits > 0) {
        encoder = extras::GetPAMEncoder();
      } else if (ppf.info.num_color_channels == 1) {
        encoder = extras::GetPGMEncoder();
      } else if (ppf.info.bits_per_sample <= 16) {
        encoder = extras::GetPPMEncoder();
      } else {
        encoder = extras::GetPFMEncoder();
      }
      break;
    case extras::Codec::kPGX:
      encoder = extras::GetPGXEncoder();
      break;
    case extras::Codec::kGIF:
      return JXL_FAILURE("Encoding to GIF is not implemented");
    case extras::Codec::kEXR:
      encoder = extras::GetEXREncoder();
      if (encoder) {
        break;
      } else {
        return JXL_FAILURE("JPEG XL was built without OpenEXR support");
      }
    case extras::Codec::kJXL:
      // TODO(user): implement
      return JXL_FAILURE("Codec::kJXL is not supported yet");

    case extras::Codec::kUnknown:
      return JXL_FAILURE("Cannot encode using Codec::kUnknown");
  }

  if (!encoder) {
    return JXL_FAILURE("Invalid codec.");
  }
  extras::EncodedImage encoded_image;
  JXL_RETURN_IF_ERROR(encoder->Encode(ppf, &encoded_image, pool));
  JXL_ASSERT(encoded_image.bitstreams.size() == 1);
  *bytes = encoded_image.bitstreams[0];

  return true;
}

Status Encode(const extras::PackedPixelFile& ppf, const std::string& pathname,
              std::vector<uint8_t>* bytes, ThreadPool* pool) {
  std::string extension;
  const extras::Codec codec =
      extras::CodecFromPath(pathname, nullptr, &extension);
  return Encode(ppf, codec, bytes, pool);
}

}  // namespace jxl
