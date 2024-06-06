// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/packed_image_convert.h"

#include <jxl/cms.h>
#include <jxl/color_encoding.h>
#include <jxl/memory_manager.h>
#include <jxl/types.h>

#include <cstdint>

#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/luminance.h"

namespace jxl {
namespace extras {

Status ConvertPackedFrameToImageBundle(const JxlBasicInfo& info,
                                       const JxlBitDepth& input_bitdepth,
                                       const PackedFrame& frame,
                                       const CodecInOut& io, ThreadPool* pool,
                                       ImageBundle* bundle) {
  JxlMemoryManager* memory_manager = io.memory_manager;
  JXL_ASSERT(frame.color.pixels() != nullptr);
  size_t frame_bits_per_sample =
      input_bitdepth.type == JXL_BIT_DEPTH_FROM_PIXEL_FORMAT
          ? PackedImage::BitsPerChannel(frame.color.format.data_type)
          : info.bits_per_sample;
  JXL_ASSERT(frame_bits_per_sample != 0);
  // It is ok for the frame.color.format.num_channels to not match the
  // number of channels on the image.
  JXL_ASSERT(1 <= frame.color.format.num_channels &&
             frame.color.format.num_channels <= 4);

  const Span<const uint8_t> span(
      static_cast<const uint8_t*>(frame.color.pixels()),
      frame.color.pixels_size);
  JXL_ASSERT(Rect(frame.frame_info.layer_info.crop_x0,
                  frame.frame_info.layer_info.crop_y0,
                  frame.frame_info.layer_info.xsize,
                  frame.frame_info.layer_info.ysize)
                 .IsInside(Rect(0, 0, info.xsize, info.ysize)));
  if (info.have_animation) {
    bundle->duration = frame.frame_info.duration;
    bundle->blend = frame.frame_info.layer_info.blend_info.blendmode > 0;
    bundle->use_for_next_frame =
        frame.frame_info.layer_info.save_as_reference > 0;
    bundle->origin.x0 = frame.frame_info.layer_info.crop_x0;
    bundle->origin.y0 = frame.frame_info.layer_info.crop_y0;
  }
  bundle->name = frame.name;  // frame.frame_info.name_length is ignored here.
  JXL_ASSERT(io.metadata.m.color_encoding.IsGray() ==
             (frame.color.format.num_channels <= 2));

  JXL_RETURN_IF_ERROR(ConvertFromExternal(
      span, frame.color.xsize, frame.color.ysize, io.metadata.m.color_encoding,
      frame_bits_per_sample, frame.color.format, pool, bundle));

  bundle->extra_channels().resize(io.metadata.m.extra_channel_info.size());
  for (size_t i = 0; i < frame.extra_channels.size(); i++) {
    const auto& ppf_ec = frame.extra_channels[i];
    JXL_ASSIGN_OR_RETURN(
        bundle->extra_channels()[i],
        ImageF::Create(memory_manager, ppf_ec.xsize, ppf_ec.ysize));
    JXL_CHECK(BufferToImageF(ppf_ec.format, ppf_ec.xsize, ppf_ec.ysize,
                             ppf_ec.pixels(), ppf_ec.pixels_size, pool,
                             &bundle->extra_channels()[i]));
  }
  return true;
}

Status ConvertPackedPixelFileToCodecInOut(const PackedPixelFile& ppf,
                                          ThreadPool* pool, CodecInOut* io) {
  JxlMemoryManager* memory_manager = io->memory_manager;
  const bool has_alpha = ppf.info.alpha_bits != 0;
  JXL_ASSERT(!ppf.frames.empty());
  if (has_alpha) {
    JXL_ASSERT(ppf.info.alpha_bits == ppf.info.bits_per_sample);
    JXL_ASSERT(ppf.info.alpha_exponent_bits ==
               ppf.info.exponent_bits_per_sample);
  }

  const bool is_gray = (ppf.info.num_color_channels == 1);
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
                              FROM_JXL_BOOL(ppf.info.alpha_premultiplied));
  ExtraChannelInfo* alpha = io->metadata.m.Find(ExtraChannel::kAlpha);
  if (alpha) alpha->bit_depth = io->metadata.m.bit_depth;

  io->metadata.m.xyb_encoded = !FROM_JXL_BOOL(ppf.info.uses_original_profile);
  JXL_ASSERT(ppf.info.orientation > 0 && ppf.info.orientation <= 8);
  io->metadata.m.orientation = ppf.info.orientation;

  // Convert animation metadata
  JXL_ASSERT(ppf.frames.size() == 1 || ppf.info.have_animation);
  io->metadata.m.have_animation = FROM_JXL_BOOL(ppf.info.have_animation);
  io->metadata.m.animation.tps_numerator = ppf.info.animation.tps_numerator;
  io->metadata.m.animation.tps_denominator = ppf.info.animation.tps_denominator;
  io->metadata.m.animation.num_loops = ppf.info.animation.num_loops;

  // Convert the color encoding.
  if (ppf.primary_color_representation == PackedPixelFile::kIccIsPrimary) {
    IccBytes icc = ppf.icc;
    if (!io->metadata.m.color_encoding.SetICC(std::move(icc),
                                              JxlGetDefaultCms())) {
      fprintf(stderr, "Warning: error setting ICC profile, assuming SRGB\n");
      io->metadata.m.color_encoding = ColorEncoding::SRGB(is_gray);
    } else {
      if (io->metadata.m.color_encoding.IsCMYK()) {
        // We expect gray or tri-color.
        return JXL_FAILURE("Embedded ICC is CMYK");
      }
      if (io->metadata.m.color_encoding.IsGray() != is_gray) {
        // E.g. JPG image has 3 channels, but gray ICC.
        return JXL_FAILURE("Embedded ICC does not match image color type");
      }
    }
  } else {
    JXL_RETURN_IF_ERROR(
        io->metadata.m.color_encoding.FromExternal(ppf.color_encoding));
    if (io->metadata.m.color_encoding.ICC().empty()) {
      return JXL_FAILURE("Failed to serialize ICC");
    }
  }

  // Convert the extra blobs
  io->blobs.exif = ppf.metadata.exif;
  io->blobs.iptc = ppf.metadata.iptc;
  io->blobs.jhgm = ppf.metadata.jhgm;
  io->blobs.jumbf = ppf.metadata.jumbf;
  io->blobs.xmp = ppf.metadata.xmp;

  // Append all other extra channels.
  for (const auto& info : ppf.extra_channels_info) {
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

  // Convert the preview
  if (ppf.preview_frame) {
    size_t preview_xsize = ppf.preview_frame->color.xsize;
    size_t preview_ysize = ppf.preview_frame->color.ysize;
    io->metadata.m.have_preview = true;
    JXL_RETURN_IF_ERROR(
        io->metadata.m.preview_size.Set(preview_xsize, preview_ysize));
    JXL_RETURN_IF_ERROR(ConvertPackedFrameToImageBundle(
        ppf.info, ppf.input_bitdepth, *ppf.preview_frame, *io, pool,
        &io->preview_frame));
  }

  // Convert the pixels
  io->frames.clear();
  for (const auto& frame : ppf.frames) {
    ImageBundle bundle(memory_manager, &io->metadata.m);
    JXL_RETURN_IF_ERROR(ConvertPackedFrameToImageBundle(
        ppf.info, ppf.input_bitdepth, frame, *io, pool, &bundle));
    io->frames.push_back(std::move(bundle));
  }

  if (ppf.info.exponent_bits_per_sample == 0) {
    // uint case.
    io->metadata.m.bit_depth.bits_per_sample = io->Main().DetectRealBitdepth();
  }
  if (ppf.info.intensity_target != 0) {
    io->metadata.m.SetIntensityTarget(ppf.info.intensity_target);
  } else {
    SetIntensityTarget(&io->metadata.m);
  }
  io->CheckMetadata();
  return true;
}

PackedPixelFile ConvertImage3FToPackedPixelFile(const Image3F& image,
                                                const ColorEncoding& c_enc,
                                                JxlPixelFormat format,
                                                ThreadPool* pool) {
  PackedPixelFile ppf;
  ppf.info.xsize = image.xsize();
  ppf.info.ysize = image.ysize();
  ppf.info.num_color_channels = 3;
  ppf.info.bits_per_sample = PackedImage::BitsPerChannel(format.data_type);
  ppf.info.exponent_bits_per_sample = format.data_type == JXL_TYPE_FLOAT ? 8
                                      : format.data_type == JXL_TYPE_FLOAT16
                                          ? 5
                                          : 0;
  ppf.color_encoding = c_enc.ToExternal();
  ppf.frames.clear();
  JXL_ASSIGN_OR_DIE(PackedFrame frame,
                    PackedFrame::Create(image.xsize(), image.ysize(), format));
  const ImageF* channels[3];
  for (int c = 0; c < 3; ++c) {
    channels[c] = &image.Plane(c);
  }
  bool float_samples = ppf.info.exponent_bits_per_sample > 0;
  JXL_CHECK(ConvertChannelsToExternal(
      channels, 3, ppf.info.bits_per_sample, float_samples, format.endianness,
      frame.color.stride, pool, frame.color.pixels(0, 0, 0),
      frame.color.pixels_size, PixelCallback(), Orientation::kIdentity));
  ppf.frames.emplace_back(std::move(frame));
  return ppf;
}

// Allows converting from internal CodecInOut to external PackedPixelFile
Status ConvertCodecInOutToPackedPixelFile(const CodecInOut& io,
                                          const JxlPixelFormat& pixel_format,
                                          const ColorEncoding& c_desired,
                                          ThreadPool* pool,
                                          PackedPixelFile* ppf) {
  JxlMemoryManager* memory_manager = io.memory_manager;
  const bool has_alpha = io.metadata.m.HasAlpha();
  JXL_ASSERT(!io.frames.empty());

  if (has_alpha) {
    JXL_ASSERT(io.metadata.m.GetAlphaBits() ==
               io.metadata.m.bit_depth.bits_per_sample);
    const auto* alpha_channel = io.metadata.m.Find(ExtraChannel::kAlpha);
    JXL_ASSERT(alpha_channel->bit_depth.exponent_bits_per_sample ==
               io.metadata.m.bit_depth.exponent_bits_per_sample);
    ppf->info.alpha_bits = alpha_channel->bit_depth.bits_per_sample;
    ppf->info.alpha_exponent_bits =
        alpha_channel->bit_depth.exponent_bits_per_sample;
    ppf->info.alpha_premultiplied =
        TO_JXL_BOOL(alpha_channel->alpha_associated);
  }

  // Convert the image metadata
  ppf->info.xsize = io.metadata.size.xsize();
  ppf->info.ysize = io.metadata.size.ysize();
  ppf->info.num_color_channels = io.metadata.m.color_encoding.Channels();
  ppf->info.bits_per_sample = io.metadata.m.bit_depth.bits_per_sample;
  ppf->info.exponent_bits_per_sample =
      io.metadata.m.bit_depth.exponent_bits_per_sample;

  ppf->info.intensity_target = io.metadata.m.tone_mapping.intensity_target;
  ppf->info.linear_below = io.metadata.m.tone_mapping.linear_below;
  ppf->info.min_nits = io.metadata.m.tone_mapping.min_nits;
  ppf->info.relative_to_max_display =
      TO_JXL_BOOL(io.metadata.m.tone_mapping.relative_to_max_display);

  ppf->info.uses_original_profile = TO_JXL_BOOL(!io.metadata.m.xyb_encoded);
  JXL_ASSERT(0 < io.metadata.m.orientation && io.metadata.m.orientation <= 8);
  ppf->info.orientation =
      static_cast<JxlOrientation>(io.metadata.m.orientation);
  ppf->info.num_color_channels = io.metadata.m.color_encoding.Channels();

  // Convert animation metadata
  JXL_ASSERT(io.frames.size() == 1 || io.metadata.m.have_animation);
  ppf->info.have_animation = TO_JXL_BOOL(io.metadata.m.have_animation);
  ppf->info.animation.tps_numerator = io.metadata.m.animation.tps_numerator;
  ppf->info.animation.tps_denominator = io.metadata.m.animation.tps_denominator;
  ppf->info.animation.num_loops = io.metadata.m.animation.num_loops;

  // Convert the color encoding
  ppf->icc.assign(c_desired.ICC().begin(), c_desired.ICC().end());
  ppf->primary_color_representation =
      c_desired.WantICC() ? PackedPixelFile::kIccIsPrimary
                          : PackedPixelFile::kColorEncodingIsPrimary;
  ppf->color_encoding = c_desired.ToExternal();

  // Convert the extra blobs
  ppf->metadata.exif = io.blobs.exif;
  ppf->metadata.iptc = io.blobs.iptc;
  ppf->metadata.jhgm = io.blobs.jhgm;
  ppf->metadata.jumbf = io.blobs.jumbf;
  ppf->metadata.xmp = io.blobs.xmp;
  const bool float_out = pixel_format.data_type == JXL_TYPE_FLOAT ||
                         pixel_format.data_type == JXL_TYPE_FLOAT16;
  // Convert the pixels
  ppf->frames.clear();
  for (const auto& frame : io.frames) {
    JXL_ASSERT(frame.metadata()->bit_depth.bits_per_sample != 0);
    // It is ok for the frame.color().kNumPlanes to not match the
    // number of channels on the image.
    const uint32_t alpha_channels = has_alpha ? 1 : 0;
    const uint32_t num_channels =
        frame.metadata()->color_encoding.Channels() + alpha_channels;
    JxlPixelFormat format{/*num_channels=*/num_channels,
                          /*data_type=*/pixel_format.data_type,
                          /*endianness=*/pixel_format.endianness,
                          /*align=*/pixel_format.align};

    JXL_ASSIGN_OR_RETURN(PackedFrame packed_frame,
                         PackedFrame::Create(frame.oriented_xsize(),
                                             frame.oriented_ysize(), format));
    const size_t bits_per_sample =
        float_out ? packed_frame.color.BitsPerChannel(pixel_format.data_type)
                  : ppf->info.bits_per_sample;
    packed_frame.name = frame.name;
    packed_frame.frame_info.name_length = frame.name.size();
    // Color transform
    JXL_ASSIGN_OR_RETURN(ImageBundle ib, frame.Copy());
    const ImageBundle* to_color_transform = &ib;
    ImageMetadata metadata = io.metadata.m;
    ImageBundle store(memory_manager, &metadata);
    const ImageBundle* transformed;
    // TODO(firsching): handle the transform here.
    JXL_RETURN_IF_ERROR(TransformIfNeeded(*to_color_transform, c_desired,
                                          *JxlGetDefaultCms(), pool, &store,
                                          &transformed));

    JXL_RETURN_IF_ERROR(ConvertToExternal(
        *transformed, bits_per_sample, float_out, format.num_channels,
        format.endianness,
        /* stride_out=*/packed_frame.color.stride, pool,
        packed_frame.color.pixels(), packed_frame.color.pixels_size,
        /*out_callback=*/{}, frame.metadata()->GetOrientation()));

    // TODO(firsching): Convert the extra channels, beside one potential alpha
    // channel. FIXME!
    JXL_CHECK(frame.extra_channels().size() <= (has_alpha ? 1 : 0));
    ppf->frames.push_back(std::move(packed_frame));
  }

  return true;
}
}  // namespace extras
}  // namespace jxl
