// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_CODEC_IN_OUT_H_
#define LIB_JXL_CODEC_IN_OUT_H_

// Holds inputs/outputs for decoding/encoding images.

#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <utility>
#include <vector>

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/luminance.h"
#include "lib/jxl/size_constraints.h"

namespace jxl {

// Optional text/EXIF metadata.
struct Blobs {
  std::vector<uint8_t> exif;
  std::vector<uint8_t> iptc;
  std::vector<uint8_t> jumbf;
  std::vector<uint8_t> xmp;
};

// Holds a preview, a main image or one or more frames, plus the inputs/outputs
// to/from decoding/encoding.
class CodecInOut {
 public:
  CodecInOut() : preview_frame(&metadata.m) {
    frames.reserve(1);
    frames.emplace_back(&metadata.m);
  }

  // Move-only.
  CodecInOut(CodecInOut&&) = default;
  CodecInOut& operator=(CodecInOut&&) = default;

  size_t LastStillFrame() const {
    JXL_DASSERT(!frames.empty());
    size_t last = 0;
    for (size_t i = 0; i < frames.size(); i++) {
      last = i;
      if (frames[i].duration > 0) break;
    }
    return last;
  }

  ImageBundle& Main() { return frames[LastStillFrame()]; }
  const ImageBundle& Main() const { return frames[LastStillFrame()]; }

  // If c_current.IsGray(), all planes must be identical.
  void SetFromImage(Image3F&& color, const ColorEncoding& c_current) {
    Main().SetFromImage(std::move(color), c_current);
    SetIntensityTarget(&this->metadata.m);
    SetSize(Main().xsize(), Main().ysize());
  }

  void SetSize(size_t xsize, size_t ysize) {
    JXL_CHECK(metadata.size.Set(xsize, ysize));
  }

  void CheckMetadata() const {
    JXL_CHECK(metadata.m.bit_depth.bits_per_sample != 0);
    JXL_CHECK(!metadata.m.color_encoding.ICC().empty());

    if (preview_frame.xsize() != 0) preview_frame.VerifyMetadata();
    JXL_CHECK(preview_frame.metadata() == &metadata.m);

    for (const ImageBundle& ib : frames) {
      ib.VerifyMetadata();
      JXL_CHECK(ib.metadata() == &metadata.m);
    }
  }

  size_t xsize() const { return metadata.size.xsize(); }
  size_t ysize() const { return metadata.size.ysize(); }
  void ShrinkTo(size_t xsize, size_t ysize) {
    // preview is unaffected.
    for (ImageBundle& ib : frames) {
      ib.ShrinkTo(xsize, ysize);
    }
    SetSize(xsize, ysize);
  }

  // Performs "PremultiplyAlpha" for each ImageBundle (preview/frames).
  bool PremultiplyAlpha() {
    const auto doPremultiplyAlpha = [](ImageBundle& bundle) {
      if (!bundle.HasAlpha()) return;
      if (!bundle.HasColor()) return;
      auto* color = bundle.color();
      const auto* alpha = bundle.alpha();
      JXL_CHECK(color->ysize() == alpha->ysize());
      JXL_CHECK(color->xsize() == alpha->xsize());
      for (size_t y = 0; y < color->ysize(); y++) {
        ::jxl::PremultiplyAlpha(color->PlaneRow(0, y), color->PlaneRow(1, y),
                                color->PlaneRow(2, y), alpha->Row(y),
                                color->xsize());
      }
    };
    ExtraChannelInfo* eci = metadata.m.Find(ExtraChannel::kAlpha);
    if (eci == nullptr || eci->alpha_associated) return false;
    if (metadata.m.have_preview) {
      doPremultiplyAlpha(preview_frame);
    }
    for (ImageBundle& ib : frames) {
      doPremultiplyAlpha(ib);
    }
    eci->alpha_associated = true;
    return true;
  }

  bool UnpremultiplyAlpha() {
    const auto doUnpremultiplyAlpha = [](ImageBundle& bundle) {
      if (!bundle.HasAlpha()) return;
      if (!bundle.HasColor()) return;
      auto* color = bundle.color();
      const auto* alpha = bundle.alpha();
      JXL_CHECK(color->ysize() == alpha->ysize());
      JXL_CHECK(color->xsize() == alpha->xsize());
      for (size_t y = 0; y < color->ysize(); y++) {
        ::jxl::UnpremultiplyAlpha(color->PlaneRow(0, y), color->PlaneRow(1, y),
                                  color->PlaneRow(2, y), alpha->Row(y),
                                  color->xsize());
      }
    };
    ExtraChannelInfo* eci = metadata.m.Find(ExtraChannel::kAlpha);
    if (eci == nullptr || !eci->alpha_associated) return false;
    if (metadata.m.have_preview) {
      doUnpremultiplyAlpha(preview_frame);
    }
    for (ImageBundle& ib : frames) {
      doUnpremultiplyAlpha(ib);
    }
    eci->alpha_associated = false;
    return true;
  }

  // -- DECODER INPUT:

  SizeConstraints constraints;

  // -- DECODER OUTPUT:

  // Total number of pixels decoded (may differ from #frames * xsize * ysize
  // if frames are cropped)
  uint64_t dec_pixels = 0;

  // -- DECODER OUTPUT, ENCODER INPUT:

  // Metadata stored into / retrieved from bitstreams.

  Blobs blobs;

  CodecMetadata metadata;  // applies to preview and all frames

  // If metadata.have_preview:
  ImageBundle preview_frame;

  std::vector<ImageBundle> frames;  // size=1 if !metadata.have_animation

  // If the image should be written to a JPEG, use this quality for encoding.
  size_t jpeg_quality;
};

}  // namespace jxl

#endif  // LIB_JXL_CODEC_IN_OUT_H_
