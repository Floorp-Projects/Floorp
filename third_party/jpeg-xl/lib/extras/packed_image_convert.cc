// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/packed_image_convert.h"

#include <cstdint>

#include "jxl/color_encoding.h"
#include "jxl/types.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_image_bundle.h"

namespace jxl {
namespace extras {

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

  // Append all other extra channels.
  for (const PackedPixelFile::PackedExtraChannel& info :
       ppf.extra_channels_info) {
    ExtraChannelInfo out;
    out.type = static_cast<jxl::ExtraChannel>(info.ec_info.type);
    out.bit_depth.bits_per_sample = info.ec_info.bits_per_sample;
    out.bit_depth.exponent_bits_per_sample =
        info.ec_info.exponent_bits_per_sample;
    out.bit_depth.floating_point_sample =
        info.ec_info.exponent_bits_per_sample != 0;
    out.dim_shift = info.ec_info.dim_shift;
    out.name = info.name;
    out.alpha_associated = (info.ec_info.alpha_premultiplied != 0);
    out.spot_color[0] = info.ec_info.spot_color[0];
    out.spot_color[1] = info.ec_info.spot_color[1];
    out.spot_color[2] = info.ec_info.spot_color[2];
    out.spot_color[3] = info.ec_info.spot_color[3];
    io->metadata.m.extra_channel_info.push_back(std::move(out));
  }

  // Convert the pixels
  io->dec_pixels = 0;
  io->frames.clear();
  for (const auto& frame : ppf.frames) {
    JXL_ASSERT(frame.color.pixels() != nullptr);
    size_t frame_bits_per_sample =
        frame.color.BitsPerChannel(frame.color.format.data_type);
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
        /*float_in=*/float_in, /*align=*/0));

    for (const auto& ppf_ec : frame.extra_channels) {
      bundle.extra_channels().emplace_back(ppf_ec.xsize, ppf_ec.ysize);
      JXL_CHECK(BufferToImageF(ppf_ec.format, ppf_ec.xsize, ppf_ec.ysize,
                               ppf_ec.pixels(), ppf_ec.pixels_size, pool,
                               &bundle.extra_channels().back()));
    }

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

// Allows converting from internal CodecInOut to external PackedPixelFile
Status ConvertCodecInOutToPackedPixelFile(const CodecInOut& io,
                                          const JxlPixelFormat& pixel_format,
                                          const ColorEncoding& c_desired,
                                          ThreadPool* pool,
                                          PackedPixelFile* ppf) {
  const bool has_alpha = io.metadata.m.HasAlpha();
  bool alpha_premultiplied = false;
  JXL_ASSERT(!io.frames.empty());

  if (has_alpha) {
    JXL_ASSERT(io.metadata.m.GetAlphaBits() ==
               io.metadata.m.bit_depth.bits_per_sample);
    const auto* alpha_channel = io.metadata.m.Find(ExtraChannel::kAlpha);
    JXL_ASSERT(alpha_channel->bit_depth.exponent_bits_per_sample ==
               io.metadata.m.bit_depth.exponent_bits_per_sample);
    alpha_premultiplied = alpha_channel->alpha_associated;
  }

  // Convert the image metadata
  ppf->info.xsize = io.metadata.size.xsize();
  ppf->info.ysize = io.metadata.size.ysize();
  ppf->info.num_color_channels = io.metadata.m.color_encoding.Channels();
  ppf->info.bits_per_sample = io.metadata.m.bit_depth.bits_per_sample;
  ppf->info.exponent_bits_per_sample =
      io.metadata.m.bit_depth.exponent_bits_per_sample;

  ppf->info.alpha_bits = io.metadata.m.GetAlphaBits();
  ppf->info.alpha_premultiplied = alpha_premultiplied;

  ppf->info.uses_original_profile = !io.metadata.m.xyb_encoded;
  JXL_ASSERT(0 < io.metadata.m.orientation && io.metadata.m.orientation <= 8);
  ppf->info.orientation =
      static_cast<JxlOrientation>(io.metadata.m.orientation);
  ppf->info.num_color_channels = io.metadata.m.color_encoding.Channels();

  // Convert animation metadata
  JXL_ASSERT(io.frames.size() == 1 || io.metadata.m.have_animation);
  ppf->info.have_animation = io.metadata.m.have_animation;
  ppf->info.animation.tps_numerator = io.metadata.m.animation.tps_numerator;
  ppf->info.animation.tps_denominator = io.metadata.m.animation.tps_denominator;
  ppf->info.animation.num_loops = io.metadata.m.animation.num_loops;

  // Convert the color encoding
  ppf->icc.assign(c_desired.ICC().begin(), c_desired.ICC().end());
  if (ppf->icc.empty()) {
    ConvertInternalToExternalColorEncoding(c_desired, &ppf->color_encoding);
  }

  // Convert the extra blobs
  ppf->metadata.exif.assign(io.blobs.exif.begin(), io.blobs.exif.end());
  ppf->metadata.iptc.assign(io.blobs.iptc.begin(), io.blobs.iptc.end());
  ppf->metadata.jumbf.assign(io.blobs.jumbf.begin(), io.blobs.jumbf.end());
  ppf->metadata.xmp.assign(io.blobs.xmp.begin(), io.blobs.xmp.end());
  const bool float_out = pixel_format.data_type == JXL_TYPE_FLOAT ||
                         pixel_format.data_type == JXL_TYPE_FLOAT16;
  // Convert the pixels
  ppf->frames.clear();
  for (const auto& frame : io.frames) {
    size_t frame_bits_per_sample = frame.metadata()->bit_depth.bits_per_sample;
    JXL_ASSERT(frame_bits_per_sample != 0);
    // It is ok for the frame.color().kNumPlanes to not match the
    // number of channels on the image.
    const uint32_t num_channels =
        frame.metadata()->color_encoding.Channels() + has_alpha;
    JxlPixelFormat format{/*num_channels=*/num_channels,
                          /*data_type=*/pixel_format.data_type,
                          /*endianness=*/pixel_format.endianness,
                          /*align=*/pixel_format.align};

    PackedFrame packed_frame(frame.oriented_xsize(), frame.oriented_ysize(),
                             format);
    const size_t bits_per_sample =
        packed_frame.color.BitsPerChannel(pixel_format.data_type);
    packed_frame.name = frame.name;
    packed_frame.frame_info.name_length = frame.name.size();
    // Color transform
    ImageBundle ib = frame.Copy();
    const ImageBundle* to_color_transform = &ib;
    ImageMetadata metadata = io.metadata.m;
    ImageBundle store(&metadata);
    const ImageBundle* transformed;
    // TODO(firsching): handle the transform here.
    JXL_RETURN_IF_ERROR(TransformIfNeeded(*to_color_transform, c_desired,
                                          GetJxlCms(), pool, &store,
                                          &transformed));
    size_t stride = ib.oriented_xsize() *
                    (c_desired.Channels() * ppf->info.bits_per_sample) /
                    kBitsPerByte;
    PaddedBytes pixels(stride * ib.oriented_ysize());

    JXL_RETURN_IF_ERROR(ConvertToExternal(
        *transformed, bits_per_sample, float_out, format.num_channels,
        format.endianness,
        /* stride_out=*/packed_frame.color.stride, pool,
        packed_frame.color.pixels(), packed_frame.color.pixels_size,
        /*out_callback=*/nullptr, /*out_opaque=*/nullptr,
        frame.metadata()->GetOrientation()));

    // TODO(firsching): Convert the extra channels, beside one potential alpha
    // channel. FIXME!
    JXL_CHECK(frame.extra_channels().size() <= has_alpha);
    ppf->frames.push_back(std::move(packed_frame));
  }

  return true;
}
}  // namespace extras
}  // namespace jxl
