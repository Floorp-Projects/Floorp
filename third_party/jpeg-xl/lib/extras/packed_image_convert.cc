// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/packed_image_convert.h"

#include "lib/jxl/enc_external_image.h"

namespace jxl {
namespace extras {

namespace {
size_t BitsPerSample(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_BOOLEAN:
      return 1;
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_UINT32:
      return 32;
    case JXL_TYPE_FLOAT:
      return 32;
    case JXL_TYPE_FLOAT16:
      return 16;
      // No default, give compiler error if new type not handled.
  }
  return 0;
}
}  // namespace

Status ConvertPackedPixelFileToCodecInOut(const PackedPixelFile& ppf,
                                          ThreadPool* pool, CodecInOut* io) {
  const bool has_alpha = ppf.info.alpha_bits != 0;
  JXL_ASSERT(!ppf.frames.empty());
  if (has_alpha) {
    JXL_ASSERT(ppf.info.alpha_bits == ppf.info.bits_per_sample);
    JXL_ASSERT(ppf.info.alpha_exponent_bits ==
               ppf.info.exponent_bits_per_sample);
  }

  const bool is_gray = ppf.info.num_color_channels == 1;
  JXL_ASSERT(ppf.info.num_color_channels == 1 ||
             ppf.info.num_color_channels == 3);

  // Convert the image metadata
  io->SetSize(ppf.info.xsize, ppf.info.ysize);
  io->metadata.m.bit_depth.bits_per_sample = ppf.info.bits_per_sample;
  io->metadata.m.bit_depth.exponent_bits_per_sample =
      ppf.info.exponent_bits_per_sample;
  io->metadata.m.bit_depth.floating_point_sample =
      ppf.info.exponent_bits_per_sample != 0;
  io->metadata.m.modular_16_bit_buffer_sufficient =
      ppf.info.exponent_bits_per_sample == 0 && ppf.info.bits_per_sample <= 12;

  io->metadata.m.SetAlphaBits(ppf.info.alpha_bits,
                              ppf.info.alpha_premultiplied);

  io->metadata.m.xyb_encoded = !ppf.info.uses_original_profile;
  JXL_ASSERT(ppf.info.orientation > 0 && ppf.info.orientation <= 8);
  io->metadata.m.orientation = ppf.info.orientation;

  // Convert animation metadata
  JXL_ASSERT(ppf.frames.size() == 1 || ppf.info.have_animation);
  io->metadata.m.have_animation = ppf.info.have_animation;
  io->metadata.m.animation.tps_numerator = ppf.info.animation.tps_numerator;
  io->metadata.m.animation.tps_denominator = ppf.info.animation.tps_denominator;
  io->metadata.m.animation.num_loops = ppf.info.animation.num_loops;

  // Convert the color encoding.
  if (!ppf.icc.empty()) {
    PaddedBytes icc;
    icc.append(ppf.icc);
    if (!io->metadata.m.color_encoding.SetICC(std::move(icc))) {
      fprintf(stderr, "Warning: error setting ICC profile, assuming SRGB");
      io->metadata.m.color_encoding = ColorEncoding::SRGB(is_gray);
    }
  } else {
    JXL_RETURN_IF_ERROR(ConvertExternalToInternalColorEncoding(
        ppf.color_encoding, &io->metadata.m.color_encoding));
  }

  // Convert the extra blobs
  io->blobs.exif.clear();
  io->blobs.exif.append(ppf.metadata.exif);
  io->blobs.iptc.clear();
  io->blobs.iptc.append(ppf.metadata.iptc);
  io->blobs.jumbf.clear();
  io->blobs.jumbf.append(ppf.metadata.jumbf);
  io->blobs.xmp.clear();
  io->blobs.xmp.append(ppf.metadata.xmp);

  // Convert the pixels
  io->dec_pixels = 0;
  io->frames.clear();
  for (const auto& frame : ppf.frames) {
    JXL_ASSERT(frame.color.pixels() != nullptr);
    size_t frame_bits_per_sample = BitsPerSample(frame.color.format.data_type);
    JXL_ASSERT(frame_bits_per_sample != 0);
    // It is ok for the frame.color.format.num_channels to not match the
    // number of channels on the image.
    JXL_ASSERT(1 <= frame.color.format.num_channels &&
               frame.color.format.num_channels <= 4);

    const Span<const uint8_t> span(
        static_cast<const uint8_t*>(frame.color.pixels()),
        frame.color.pixels_size);
    Rect frame_rect =
        Rect(frame.x0, frame.y0, frame.color.xsize, frame.color.ysize);
    JXL_ASSERT(frame_rect.IsInside(Rect(0, 0, ppf.info.xsize, ppf.info.ysize)));

    ImageBundle bundle(&io->metadata.m);
    if (ppf.info.have_animation) {
      bundle.duration = frame.frame_info.duration;
      bundle.blend = frame.blend;
      bundle.use_for_next_frame = frame.use_for_next_frame;
    }
    bundle.name = frame.name;  // frame.frame_info.name_length is ignored here.
    bundle.origin.x0 = frame.x0;
    bundle.origin.y0 = frame.y0;

    JXL_ASSERT(io->metadata.m.color_encoding.IsGray() ==
               (frame.color.format.num_channels <= 2));

    const bool float_in = frame.color.format.data_type == JXL_TYPE_FLOAT16 ||
                          frame.color.format.data_type == JXL_TYPE_FLOAT;
    JXL_RETURN_IF_ERROR(ConvertFromExternal(
        span, frame.color.xsize, frame.color.ysize,
        io->metadata.m.color_encoding,
        /*has_alpha=*/frame.color.format.num_channels == 2 ||
            frame.color.format.num_channels == 4,
        /*alpha_is_premultiplied=*/ppf.info.alpha_premultiplied,
        frame_bits_per_sample, frame.color.format.endianness,
        /*flipped_y=*/frame.color.flipped_y, pool, &bundle,
        /*float_in=*/float_in));

    // TODO(deymo): Convert the extra channels. FIXME!
    JXL_CHECK(frame.extra_channels.empty());

    io->frames.push_back(std::move(bundle));
    io->dec_pixels += frame.color.xsize * frame.color.ysize;
  }

  if (ppf.info.exponent_bits_per_sample == 0) {
    // uint case.
    io->metadata.m.bit_depth.bits_per_sample = io->Main().DetectRealBitdepth();
  }
  if (ppf.info.intensity_target != 0) {
    io->metadata.m.SetIntensityTarget(ppf.info.intensity_target);
  } else {
    SetIntensityTarget(io);
  }
  io->CheckMetadata();
  return true;
}

}  // namespace extras
}  // namespace jxl
