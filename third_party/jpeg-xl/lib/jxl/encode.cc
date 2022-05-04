// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "jxl/encode.h"

#include <brotli/encode.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "jxl/codestream_header.h"
#include "jxl/types.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_icc_codec.h"
#include "lib/jxl/encode_internal.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"
#include "lib/jxl/sanitizers.h"

// Debug-printing failure macro similar to JXL_FAILURE, but for the status code
// JXL_ENC_ERROR
#ifdef JXL_CRASH_ON_ERROR
#define JXL_API_ERROR(format, ...)                                           \
  (::jxl::Debug(("%s:%d: " format "\n"), __FILE__, __LINE__, ##__VA_ARGS__), \
   ::jxl::Abort(), JXL_ENC_ERROR)
#else  // JXL_CRASH_ON_ERROR
#define JXL_API_ERROR(format, ...)                                             \
  (((JXL_DEBUG_ON_ERROR) &&                                                    \
    ::jxl::Debug(("%s:%d: " format "\n"), __FILE__, __LINE__, ##__VA_ARGS__)), \
   JXL_ENC_ERROR)
#endif  // JXL_CRASH_ON_ERROR

namespace jxl {}  // namespace jxl

uint32_t JxlEncoderVersion(void) {
  return JPEGXL_MAJOR_VERSION * 1000000 + JPEGXL_MINOR_VERSION * 1000 +
         JPEGXL_PATCH_VERSION;
}

namespace {
template <typename T>
void AppendJxlpBoxCounter(uint32_t counter, bool last, T* output) {
  if (last) counter |= 0x80000000;
  for (size_t i = 0; i < 4; i++) {
    output->push_back(counter >> (8 * (3 - i)) & 0xff);
  }
}

void QueueFrame(
    const JxlEncoderFrameSettings* frame_settings,
    jxl::MemoryManagerUniquePtr<jxl::JxlEncoderQueuedFrame>& frame) {
  if (frame_settings->values.lossless) {
    frame->option_values.cparams.SetLossless();
  }

  jxl::JxlEncoderQueuedInput queued_input(frame_settings->enc->memory_manager);
  queued_input.frame = std::move(frame);
  frame_settings->enc->input_queue.emplace_back(std::move(queued_input));
  frame_settings->enc->num_queued_frames++;
}

void QueueBox(JxlEncoder* enc,
              jxl::MemoryManagerUniquePtr<jxl::JxlEncoderQueuedBox>& box) {
  jxl::JxlEncoderQueuedInput queued_input(enc->memory_manager);
  queued_input.box = std::move(box);
  enc->input_queue.emplace_back(std::move(queued_input));
  enc->num_queued_boxes++;
}

// TODO(lode): share this code and the Brotli compression code in enc_jpeg_data
JxlEncoderStatus BrotliCompress(int quality, const uint8_t* in, size_t in_size,
                                jxl::PaddedBytes* out) {
  std::unique_ptr<BrotliEncoderState, decltype(BrotliEncoderDestroyInstance)*>
      enc(BrotliEncoderCreateInstance(nullptr, nullptr, nullptr),
          BrotliEncoderDestroyInstance);
  if (!enc) return JXL_API_ERROR("BrotliEncoderCreateInstance failed");

  BrotliEncoderSetParameter(enc.get(), BROTLI_PARAM_QUALITY, quality);
  BrotliEncoderSetParameter(enc.get(), BROTLI_PARAM_SIZE_HINT, in_size);

  constexpr size_t kBufferSize = 128 * 1024;
  jxl::PaddedBytes temp_buffer(kBufferSize);

  size_t avail_in = in_size;
  const uint8_t* next_in = in;

  size_t total_out = 0;

  for (;;) {
    size_t avail_out = kBufferSize;
    uint8_t* next_out = temp_buffer.data();
    jxl::msan::MemoryIsInitialized(next_in, avail_in);
    if (!BrotliEncoderCompressStream(enc.get(), BROTLI_OPERATION_FINISH,
                                     &avail_in, &next_in, &avail_out, &next_out,
                                     &total_out)) {
      return JXL_API_ERROR("Brotli compression failed");
    }
    size_t out_size = next_out - temp_buffer.data();
    jxl::msan::UnpoisonMemory(next_out - out_size, out_size);
    out->resize(out->size() + out_size);
    memcpy(out->data() + out->size() - out_size, temp_buffer.data(), out_size);
    if (BrotliEncoderIsFinished(enc.get())) break;
  }

  return JXL_ENC_SUCCESS;
}

// The JXL codestream can have level 5 or level 10. Levels have certain
// restrictions such as max allowed image dimensions. This function checks the
// level required to support the current encoder settings. The debug_string is
// intended to be used for developer API error messages, and may be set to
// nullptr.
int VerifyLevelSettings(const JxlEncoder* enc, std::string* debug_string) {
  const auto& m = enc->metadata.m;

  uint64_t xsize = enc->metadata.size.xsize();
  uint64_t ysize = enc->metadata.size.ysize();
  // The uncompressed ICC size, if it is used.
  size_t icc_size = 0;
  if (m.color_encoding.WantICC()) {
    icc_size = m.color_encoding.ICC().size();
  }

  // Level 10 checks

  if (xsize > (1ull << 30ull) || ysize > (1ull << 30ull) ||
      xsize * ysize > (1ull << 40ull)) {
    if (debug_string) *debug_string = "Too large image dimensions";
    return -1;
  }
  if (icc_size > (1ull << 28)) {
    if (debug_string) *debug_string = "Too large ICC profile size";
    return -1;
  }
  if (m.num_extra_channels > 256) {
    if (debug_string) *debug_string = "Too many extra channels";
    return -1;
  }

  // Level 5 checks

  if (!m.modular_16_bit_buffer_sufficient) {
    if (debug_string) *debug_string = "Too high modular bit depth";
    return 10;
  }
  if (xsize > (1ull << 18ull) || ysize > (1ull << 18ull) ||
      xsize * ysize > (1ull << 28ull)) {
    if (debug_string) *debug_string = "Too large image dimensions";
    return 10;
  }
  if (icc_size > (1ull << 22)) {
    if (debug_string) *debug_string = "Too large ICC profile";
    return 10;
  }
  if (m.num_extra_channels > 4) {
    if (debug_string) *debug_string = "Too many extra channels";
    return 10;
  }
  for (size_t i = 0; i < m.extra_channel_info.size(); ++i) {
    if (m.extra_channel_info[i].type == jxl::ExtraChannel::kBlack) {
      if (debug_string) *debug_string = "CMYK channel not allowed";
      return 10;
    }
  }

  // TODO(lode): also need to check if consecutive composite-still frames total
  // pixel amount doesn't exceed 2**28 in the case of level 5. This should be
  // done when adding frame and requires ability to add composite still frames
  // to be added first.

  // TODO(lode): also need to check animation duration of a frame. This should
  // be done when adding frame, but first requires implementing setting the
  // JxlFrameHeader for a frame.

  // TODO(lode): also need to check properties such as num_splines, num_patches,
  // modular_16bit_buffers and multiple properties of modular trees. However
  // these are not user-set properties so cannot be checked here, but decisions
  // the C++ encoder should be able to make based on the level.

  // All level 5 checks passes, so can return the more compatible level 5
  return 5;
}
JxlEncoderStatus CheckValidBitdepth(uint32_t bits_per_sample,
                                    uint32_t exponent_bits_per_sample) {
  if (!exponent_bits_per_sample) {
    // The spec allows up to 31 for bits_per_sample here, but
    // the code does not (yet) support it.
    if (!(bits_per_sample > 0 && bits_per_sample <= 24)) {
      return JXL_API_ERROR("Invalid value for bits_per_sample");
    }
  } else if ((exponent_bits_per_sample > 8) ||
             (bits_per_sample > 24 + exponent_bits_per_sample) ||
             (bits_per_sample < 3 + exponent_bits_per_sample)) {
    return JXL_API_ERROR("Invalid float description");
  }
  return JXL_ENC_SUCCESS;
}

}  // namespace

JxlEncoderStatus JxlEncoderStruct::RefillOutputByteQueue() {
  jxl::PaddedBytes bytes;

  jxl::JxlEncoderQueuedInput& input = input_queue[0];

  // TODO(lode): split this into 3 functions: for adding the signature and other
  // initial headers (jbrd, ...), one for adding frame, and one for adding user
  // box.

  if (!wrote_bytes) {
    // First time encoding any data, verify the level 5 vs level 10 settings
    std::string level_message;
    int required_level = VerifyLevelSettings(this, &level_message);
    // Only level 5 and 10 are defined, and the function can return -1 to
    // indicate full incompatibility.
    JXL_ASSERT(required_level == -1 || required_level == 5 ||
               required_level == 10);
    if (codestream_level == 5 && required_level != 5) {
      // If the required level is 10, return error rather than automatically
      // setting the level to 10, to avoid inadvertently creating a level 10
      // JXL file while intending to target a level 5 decoder.
      return JXL_API_ERROR(
          "%s",
          ("Codestream level verification for level 5 failed: " + level_message)
              .c_str());
    }
    if (codestream_level == 10 && required_level == -1) {
      return JXL_API_ERROR(
          "%s", ("Codestream level verification for level 10 failed: " +
                 level_message)
                    .c_str());
    }

    jxl::BitWriter writer;
    if (!WriteHeaders(&metadata, &writer, nullptr)) {
      return JXL_API_ERROR("Failed to write codestream header");
    }
    // Only send ICC (at least several hundred bytes) if fields aren't enough.
    if (metadata.m.color_encoding.WantICC()) {
      if (!jxl::WriteICC(metadata.m.color_encoding.ICC(), &writer,
                         jxl::kLayerHeader, nullptr)) {
        return JXL_API_ERROR("Failed to write ICC profile");
      }
    }
    // TODO(lode): preview should be added here if a preview image is added

    writer.ZeroPadToByte();

    // Not actually the end of frame, but the end of metadata/ICC, but helps
    // the next frame to start here for indexing purposes.
    codestream_bytes_written_end_of_frame +=
        jxl::DivCeil(writer.BitsWritten(), 8);

    bytes = std::move(writer).TakeBytes();

    if (MustUseContainer()) {
      // Add "JXL " and ftyp box.
      output_byte_queue.insert(
          output_byte_queue.end(), jxl::kContainerHeader,
          jxl::kContainerHeader + sizeof(jxl::kContainerHeader));
      if (codestream_level != 5) {
        // Add jxll box directly after the ftyp box to indicate the codestream
        // level.
        output_byte_queue.insert(
            output_byte_queue.end(), jxl::kLevelBoxHeader,
            jxl::kLevelBoxHeader + sizeof(jxl::kLevelBoxHeader));
        output_byte_queue.push_back(codestream_level);
      }

      // Whether to write the basic info and color profile header of the
      // codestream into an early separate jxlp box, so that it comes before
      // metadata or jpeg reconstruction boxes. In theory this could simply
      // always be done, but there's no reason to add an extra box with box
      // header overhead if the codestream will already come immediately after
      // the signature and level boxes.
      bool partial_header = store_jpeg_metadata || (use_boxes && !input.frame);

      if (partial_header) {
        jxl::AppendBoxHeader(jxl::MakeBoxType("jxlp"), bytes.size() + 4,
                             /*unbounded=*/false, &output_byte_queue);
        AppendJxlpBoxCounter(jxlp_counter++, /*last=*/false,
                             &output_byte_queue);
        output_byte_queue.insert(output_byte_queue.end(), bytes.data(),
                                 bytes.data() + bytes.size());
        bytes.clear();
      }

      if (store_jpeg_metadata && !jpeg_metadata.empty()) {
        jxl::AppendBoxHeader(jxl::MakeBoxType("jbrd"), jpeg_metadata.size(),
                             false, &output_byte_queue);
        output_byte_queue.insert(output_byte_queue.end(), jpeg_metadata.begin(),
                                 jpeg_metadata.end());
      }
    }
    wrote_bytes = true;
  }

  // Choose frame or box processing: exactly one of the two unique pointers (box
  // or frame) in the input queue item is non-null.
  if (input.frame) {
    jxl::MemoryManagerUniquePtr<jxl::JxlEncoderQueuedFrame> input_frame =
        std::move(input.frame);
    input_queue.erase(input_queue.begin());
    num_queued_frames--;
    for (unsigned idx = 0; idx < input_frame->ec_initialized.size(); idx++) {
      if (!input_frame->ec_initialized[idx]) {
        return JXL_API_ERROR("Extra channel %u is not initialized", idx);
      }
    }

    // TODO(zond): If the input queue is empty and the frames_closed is true,
    // then mark this frame as the last.

    // TODO(zond): Handle progressive mode like EncodeFile does it.
    // TODO(zond): Handle animation like EncodeFile does it, by checking if
    //             JxlEncoderCloseFrames has been called and if the frame queue
    //             is empty (to see if it's the last animation frame).

    if (metadata.m.xyb_encoded) {
      input_frame->option_values.cparams.color_transform =
          jxl::ColorTransform::kXYB;
    } else {
      // TODO(zond): Figure out when to use kYCbCr instead.
      input_frame->option_values.cparams.color_transform =
          jxl::ColorTransform::kNone;
    }

    jxl::BitWriter writer;
    jxl::PassesEncoderState enc_state;

    // EncodeFrame creates jxl::FrameHeader object internally based on the
    // FrameInfo, imagebundle, cparams and metadata. Copy the information to
    // these.
    jxl::ImageBundle& ib = input_frame->frame;
    ib.name = input_frame->option_values.frame_name;
    if (metadata.m.have_animation) {
      ib.duration = input_frame->option_values.header.duration;
      ib.timecode = input_frame->option_values.header.timecode;
    } else {
      // If have_animation is false, the encoder should ignore the duration and
      // timecode values. However, assigning them to ib will cause the encoder
      // to write an invalid frame header that can't be decoded so ensure
      // they're the default value of 0 here.
      ib.duration = 0;
      ib.timecode = 0;
    }
    ib.blendmode = static_cast<jxl::BlendMode>(
        input_frame->option_values.header.layer_info.blend_info.blendmode);
    ib.blend =
        input_frame->option_values.header.layer_info.blend_info.blendmode !=
        JXL_BLEND_REPLACE;

    size_t save_as_reference =
        input_frame->option_values.header.layer_info.save_as_reference;
    ib.use_for_next_frame = !!save_as_reference;

    jxl::FrameInfo frame_info;
    bool last_frame = frames_closed && !num_queued_frames;
    frame_info.is_last = last_frame;
    frame_info.save_as_reference = save_as_reference;
    frame_info.source =
        input_frame->option_values.header.layer_info.blend_info.source;
    frame_info.clamp =
        input_frame->option_values.header.layer_info.blend_info.clamp;
    frame_info.alpha_channel =
        input_frame->option_values.header.layer_info.blend_info.alpha;
    frame_info.extra_channel_blending_info.resize(
        metadata.m.num_extra_channels);
    // If extra channel blend info has not been set, use the blend mode from the
    // layer_info.
    JxlBlendInfo default_blend_info =
        input_frame->option_values.header.layer_info.blend_info;
    for (size_t i = 0; i < metadata.m.num_extra_channels; ++i) {
      auto& to = frame_info.extra_channel_blending_info[i];
      const auto& from =
          i < input_frame->option_values.extra_channel_blend_info.size()
              ? input_frame->option_values.extra_channel_blend_info[i]
              : default_blend_info;
      to.mode = static_cast<jxl::BlendMode>(from.blendmode);
      to.source = from.source;
      to.alpha_channel = from.alpha;
      to.clamp = (from.clamp != 0);
    }

    if (input_frame->option_values.header.layer_info.have_crop) {
      ib.origin.x0 = input_frame->option_values.header.layer_info.crop_x0;
      ib.origin.y0 = input_frame->option_values.header.layer_info.crop_y0;
    }
    JXL_ASSERT(writer.BitsWritten() == 0);
    if (!jxl::EncodeFrame(input_frame->option_values.cparams, frame_info,
                          &metadata, input_frame->frame, &enc_state, cms,
                          thread_pool.get(), &writer,
                          /*aux_out=*/nullptr)) {
      return JXL_API_ERROR("Failed to encode frame");
    }
    codestream_bytes_written_beginning_of_frame =
        codestream_bytes_written_end_of_frame;
    codestream_bytes_written_end_of_frame +=
        jxl::DivCeil(writer.BitsWritten(), 8);

    // Possibly bytes already contains the codestream header: in case this is
    // the first frame, and the codestream header was not encoded as jxlp above.
    bytes.append(std::move(writer).TakeBytes());
    if (MustUseContainer()) {
      if (last_frame && jxlp_counter == 0) {
        // If this is the last frame and no jxlp boxes were used yet, it's
        // slighly more efficient to write a jxlc box since it has 4 bytes less
        // overhead.
        jxl::AppendBoxHeader(jxl::MakeBoxType("jxlc"), bytes.size(),
                             /*unbounded=*/false, &output_byte_queue);
      } else {
        jxl::AppendBoxHeader(jxl::MakeBoxType("jxlp"), bytes.size() + 4,
                             /*unbounded=*/false, &output_byte_queue);
        AppendJxlpBoxCounter(jxlp_counter++, last_frame, &output_byte_queue);
      }
    }

    output_byte_queue.insert(output_byte_queue.end(), bytes.data(),
                             bytes.data() + bytes.size());

    last_used_cparams = input_frame->option_values.cparams;
  } else {
    // Not a frame, so is a box instead
    jxl::MemoryManagerUniquePtr<jxl::JxlEncoderQueuedBox> box =
        std::move(input.box);
    input_queue.erase(input_queue.begin());
    num_queued_boxes--;

    if (box->compress_box) {
      jxl::PaddedBytes compressed(4);
      // Prepend the original box type in the brob box contents
      for (size_t i = 0; i < 4; i++) {
        compressed[i] = static_cast<uint8_t>(box->type[i]);
      }
      if (JXL_ENC_SUCCESS !=
          BrotliCompress((brotli_effort >= 0 ? brotli_effort : 4),
                         box->contents.data(), box->contents.size(),
                         &compressed)) {
        return JXL_API_ERROR("Brotli compression for brob box failed");
      }
      jxl::AppendBoxHeader(jxl::MakeBoxType("brob"), compressed.size(), false,
                           &output_byte_queue);
      output_byte_queue.insert(output_byte_queue.end(), compressed.data(),
                               compressed.data() + compressed.size());
    } else {
      jxl::AppendBoxHeader(box->type, box->contents.size(), false,
                           &output_byte_queue);
      output_byte_queue.insert(output_byte_queue.end(), box->contents.data(),
                               box->contents.data() + box->contents.size());
    }
  }

  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderSetColorEncoding(JxlEncoder* enc,
                                            const JxlColorEncoding* color) {
  if (enc->color_encoding_set) {
    // Already set
    return JXL_ENC_ERROR;
  }
  if (!jxl::ConvertExternalToInternalColorEncoding(
          *color, &enc->metadata.m.color_encoding)) {
    return JXL_ENC_ERROR;
  }
  enc->color_encoding_set = true;
  if (!enc->intensity_target_set) {
    jxl::SetIntensityTarget(&enc->metadata.m);
  }
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderSetICCProfile(JxlEncoder* enc,
                                         const uint8_t* icc_profile,
                                         size_t size) {
  if (enc->color_encoding_set) {
    // Already set
    return JXL_ENC_ERROR;
  }
  jxl::PaddedBytes icc;
  icc.assign(icc_profile, icc_profile + size);
  if (!enc->metadata.m.color_encoding.SetICC(std::move(icc))) {
    return JXL_ENC_ERROR;
  }
  enc->color_encoding_set = true;
  if (!enc->intensity_target_set) {
    jxl::SetIntensityTarget(&enc->metadata.m);
  }
  return JXL_ENC_SUCCESS;
}

void JxlEncoderInitBasicInfo(JxlBasicInfo* info) {
  info->have_container = JXL_FALSE;
  info->xsize = 0;
  info->ysize = 0;
  info->bits_per_sample = 8;
  info->exponent_bits_per_sample = 0;
  info->intensity_target = 0.f;
  info->min_nits = 0.f;
  info->relative_to_max_display = JXL_FALSE;
  info->linear_below = 0.f;
  info->uses_original_profile = JXL_FALSE;
  info->have_preview = JXL_FALSE;
  info->have_animation = JXL_FALSE;
  info->orientation = JXL_ORIENT_IDENTITY;
  info->num_color_channels = 3;
  info->num_extra_channels = 0;
  info->alpha_bits = 0;
  info->alpha_exponent_bits = 0;
  info->alpha_premultiplied = JXL_FALSE;
  info->preview.xsize = 0;
  info->preview.ysize = 0;
  info->intrinsic_xsize = 0;
  info->intrinsic_ysize = 0;
  info->animation.tps_numerator = 10;
  info->animation.tps_denominator = 1;
  info->animation.num_loops = 0;
  info->animation.have_timecodes = JXL_FALSE;
}

void JxlEncoderInitFrameHeader(JxlFrameHeader* frame_header) {
  // For each field, the default value of the specification is used. Depending
  // on wheter an animation frame, or a composite still blending frame, is used,
  // different fields have to be set up by the user after initing the frame
  // header.
  frame_header->duration = 0;
  frame_header->timecode = 0;
  frame_header->name_length = 0;
  // In the specification, the default value of is_last is !frame_type, and the
  // default frame_type is kRegularFrame which has value 0, so is_last is true
  // by default. However, the encoder does not use this value (the field exists
  // for the decoder to set) since last frame is determined by usage of
  // JxlEncoderCloseFrames instead.
  frame_header->is_last = JXL_TRUE;
  frame_header->layer_info.have_crop = JXL_FALSE;
  frame_header->layer_info.crop_x0 = 0;
  frame_header->layer_info.crop_y0 = 0;
  // These must be set if have_crop is enabled, but the default value has
  // have_crop false, and these dimensions 0. The user must set these to the
  // desired size after enabling have_crop (which is not yet implemented).
  frame_header->layer_info.xsize = 0;
  frame_header->layer_info.ysize = 0;
  JxlEncoderInitBlendInfo(&frame_header->layer_info.blend_info);
  frame_header->layer_info.save_as_reference = 0;
}

void JxlEncoderInitBlendInfo(JxlBlendInfo* blend_info) {
  // Default blend mode in the specification is 0. Note that combining
  // blend mode of replace with a duration is not useful, but the user has to
  // manually set duration in case of animation, or manually change the blend
  // mode in case of composite stills, so initing to a combination that is not
  // useful on its own is not an issue.
  blend_info->blendmode = JXL_BLEND_REPLACE;
  blend_info->source = 0;
  blend_info->alpha = 0;
  blend_info->clamp = 0;
}

JxlEncoderStatus JxlEncoderSetBasicInfo(JxlEncoder* enc,
                                        const JxlBasicInfo* info) {
  if (!enc->metadata.size.Set(info->xsize, info->ysize)) {
    return JXL_ENC_ERROR;
  }
  if (JXL_ENC_SUCCESS != CheckValidBitdepth(info->bits_per_sample,
                                            info->exponent_bits_per_sample)) {
    return JXL_ENC_ERROR;
  }
  enc->metadata.m.bit_depth.bits_per_sample = info->bits_per_sample;
  enc->metadata.m.bit_depth.exponent_bits_per_sample =
      info->exponent_bits_per_sample;
  enc->metadata.m.bit_depth.floating_point_sample =
      (info->exponent_bits_per_sample != 0u);
  enc->metadata.m.modular_16_bit_buffer_sufficient =
      (!info->uses_original_profile || info->bits_per_sample <= 12) &&
      info->alpha_bits <= 12;

  // The number of extra channels includes the alpha channel, so for example and
  // RGBA with no other extra channels, has exactly num_extra_channels == 1
  enc->metadata.m.num_extra_channels = info->num_extra_channels;
  enc->metadata.m.extra_channel_info.resize(enc->metadata.m.num_extra_channels);
  if (info->num_extra_channels == 0 && info->alpha_bits) {
    return JXL_API_ERROR(
        "when alpha_bits is non-zero, the number of channels must be at least "
        "1");
  }
  // If the user provides non-zero alpha_bits, we make the channel info at index
  // zero the appropriate alpha channel.
  if (info->alpha_bits) {
    JxlExtraChannelInfo channel_info;
    JxlEncoderInitExtraChannelInfo(JXL_CHANNEL_ALPHA, &channel_info);
    channel_info.bits_per_sample = info->alpha_bits;
    channel_info.exponent_bits_per_sample = info->alpha_exponent_bits;
    if (JxlEncoderSetExtraChannelInfo(enc, 0, &channel_info)) {
      return JXL_ENC_ERROR;
    }
  }

  enc->metadata.m.xyb_encoded = !info->uses_original_profile;
  if (info->orientation > 0 && info->orientation <= 8) {
    enc->metadata.m.orientation = info->orientation;
  } else {
    return JXL_API_ERROR("Invalid value for orientation field");
  }
  if (info->intensity_target != 0) {
    enc->metadata.m.SetIntensityTarget(info->intensity_target);
    enc->intensity_target_set = true;
  } else if (enc->color_encoding_set || enc->metadata.m.xyb_encoded) {
    // If both conditions are false, JxlEncoderSetColorEncoding will be called
    // later and we will get one more chance to call jxl::SetIntensityTarget,
    // after the color encoding is indeed set.
    jxl::SetIntensityTarget(&enc->metadata.m);
    enc->intensity_target_set = true;
  }
  enc->metadata.m.tone_mapping.min_nits = info->min_nits;
  enc->metadata.m.tone_mapping.relative_to_max_display =
      info->relative_to_max_display;
  enc->metadata.m.tone_mapping.linear_below = info->linear_below;
  enc->basic_info_set = true;

  enc->metadata.m.have_animation = info->have_animation;
  if (info->have_animation) {
    if (info->animation.tps_denominator < 1) {
      return JXL_API_ERROR(
          "If animation is used, tps_denominator must be >= 1");
    }
    if (info->animation.tps_numerator < 1) {
      return JXL_API_ERROR("If animation is used, tps_numerator must be >= 1");
    }
    enc->metadata.m.animation.tps_numerator = info->animation.tps_numerator;
    enc->metadata.m.animation.tps_denominator = info->animation.tps_denominator;
    enc->metadata.m.animation.num_loops = info->animation.num_loops;
    enc->metadata.m.animation.have_timecodes = info->animation.have_timecodes;
  }
  std::string level_message;
  int required_level = VerifyLevelSettings(enc, &level_message);
  if (required_level == -1 ||
      static_cast<int>(enc->codestream_level) < required_level) {
    return JXL_API_ERROR("%s", ("Codestream level verification for level " +
                                std::to_string(enc->codestream_level) +
                                " failed: " + level_message)
                                   .c_str());
  }
  return JXL_ENC_SUCCESS;
}

void JxlEncoderInitExtraChannelInfo(JxlExtraChannelType type,
                                    JxlExtraChannelInfo* info) {
  info->type = type;
  info->bits_per_sample = 8;
  info->exponent_bits_per_sample = 0;
  info->dim_shift = 0;
  info->name_length = 0;
  info->alpha_premultiplied = JXL_FALSE;
  info->spot_color[0] = 0;
  info->spot_color[1] = 0;
  info->spot_color[2] = 0;
  info->spot_color[3] = 0;
  info->cfa_channel = 0;
}

JXL_EXPORT JxlEncoderStatus JxlEncoderSetExtraChannelInfo(
    JxlEncoder* enc, size_t index, const JxlExtraChannelInfo* info) {
  if (index >= enc->metadata.m.num_extra_channels) {
    return JXL_API_ERROR("Invalid value for the index of extra channel");
  }
  if (JXL_ENC_SUCCESS != CheckValidBitdepth(info->bits_per_sample,
                                            info->exponent_bits_per_sample)) {
    return JXL_ENC_ERROR;
  }

  jxl::ExtraChannelInfo& channel = enc->metadata.m.extra_channel_info[index];
  channel.type = static_cast<jxl::ExtraChannel>(info->type);
  channel.bit_depth.bits_per_sample = info->bits_per_sample;
  enc->metadata.m.modular_16_bit_buffer_sufficient &=
      info->bits_per_sample <= 12;
  channel.bit_depth.exponent_bits_per_sample = info->exponent_bits_per_sample;
  channel.bit_depth.floating_point_sample = info->exponent_bits_per_sample != 0;
  channel.dim_shift = info->dim_shift;
  channel.name = "";
  channel.alpha_associated = (info->alpha_premultiplied != 0);
  channel.cfa_channel = info->cfa_channel;
  channel.spot_color[0] = info->spot_color[0];
  channel.spot_color[1] = info->spot_color[1];
  channel.spot_color[2] = info->spot_color[2];
  channel.spot_color[3] = info->spot_color[3];
  std::string level_message;
  int required_level = VerifyLevelSettings(enc, &level_message);
  if (required_level == -1 ||
      static_cast<int>(enc->codestream_level) < required_level) {
    return JXL_API_ERROR("%s", ("Codestream level verification for level " +
                                std::to_string(enc->codestream_level) +
                                " failed: " + level_message)
                                   .c_str());
  }
  return JXL_ENC_SUCCESS;
}

JXL_EXPORT JxlEncoderStatus JxlEncoderSetExtraChannelName(JxlEncoder* enc,
                                                          size_t index,
                                                          const char* name,
                                                          size_t size) {
  if (index >= enc->metadata.m.num_extra_channels) {
    return JXL_API_ERROR("Invalid value for the index of extra channel");
  }
  enc->metadata.m.extra_channel_info[index].name =
      std::string(name, name + size);
  return JXL_ENC_SUCCESS;
}

JxlEncoderFrameSettings* JxlEncoderFrameSettingsCreate(
    JxlEncoder* enc, const JxlEncoderFrameSettings* source) {
  auto opts = jxl::MemoryManagerMakeUnique<JxlEncoderFrameSettings>(
      &enc->memory_manager);
  if (!opts) return nullptr;
  opts->enc = enc;
  if (source != nullptr) {
    opts->values = source->values;
  } else {
    opts->values.lossless = false;
  }
  opts->values.cparams.level = enc->codestream_level;
  JxlEncoderFrameSettings* ret = opts.get();
  enc->encoder_options.emplace_back(std::move(opts));
  return ret;
}

JxlEncoderFrameSettings* JxlEncoderOptionsCreate(
    JxlEncoder* enc, const JxlEncoderFrameSettings* source) {
  // Deprecated function name, call the non-deprecated function
  return JxlEncoderFrameSettingsCreate(enc, source);
}

JxlEncoderStatus JxlEncoderSetFrameLossless(
    JxlEncoderFrameSettings* frame_settings, const JXL_BOOL lossless) {
  if (lossless && frame_settings->enc->basic_info_set &&
      frame_settings->enc->metadata.m.xyb_encoded) {
    return JXL_API_ERROR("Set use_original_profile=true for lossless encoding");
  }
  frame_settings->values.lossless = lossless;
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderOptionsSetLossless(
    JxlEncoderFrameSettings* frame_settings, JXL_BOOL lossless) {
  // Deprecated function name, call the non-deprecated function
  return JxlEncoderSetFrameLossless(frame_settings, lossless);
}

JxlEncoderStatus JxlEncoderOptionsSetEffort(
    JxlEncoderFrameSettings* frame_settings, const int effort) {
  return JxlEncoderFrameSettingsSetOption(frame_settings,
                                          JXL_ENC_FRAME_SETTING_EFFORT, effort);
}

JxlEncoderStatus JxlEncoderSetFrameDistance(
    JxlEncoderFrameSettings* frame_settings, float distance) {
  if (distance < 0.f || distance > 25.f) {
    return JXL_ENC_ERROR;
  }
  if (distance > 0.f && distance < 0.01f) {
    distance = 0.01f;
  }
  frame_settings->values.cparams.butteraugli_distance = distance;
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderOptionsSetDistance(
    JxlEncoderFrameSettings* frame_settings, float distance) {
  // Deprecated function name, call the non-deprecated function
  return JxlEncoderSetFrameDistance(frame_settings, distance);
}

JxlEncoderStatus JxlEncoderOptionsSetDecodingSpeed(
    JxlEncoderFrameSettings* frame_settings, int tier) {
  return JxlEncoderFrameSettingsSetOption(
      frame_settings, JXL_ENC_FRAME_SETTING_DECODING_SPEED, tier);
}

JxlEncoderStatus JxlEncoderFrameSettingsSetOption(
    JxlEncoderFrameSettings* frame_settings, JxlEncoderFrameSettingId option,
    int32_t value) {
  switch (option) {
    case JXL_ENC_FRAME_SETTING_EFFORT:
      if (value < 1 || value > 9) {
        return JXL_ENC_ERROR;
      }
      frame_settings->values.cparams.speed_tier =
          static_cast<jxl::SpeedTier>(10 - value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_BROTLI_EFFORT:
      if (value < -1 || value > 11) {
        return JXL_ENC_ERROR;
      }
      // set cparams for brotli use in JPEG frames
      frame_settings->values.cparams.brotli_effort = value;
      // set enc option for brotli use in brob boxes
      frame_settings->enc->brotli_effort = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_DECODING_SPEED:
      if (value < 0 || value > 4) {
        return JXL_ENC_ERROR;
      }
      frame_settings->values.cparams.decoding_speed_tier = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_RESAMPLING:
      if (value != -1 && value != 1 && value != 2 && value != 4 && value != 8) {
        return JXL_ENC_ERROR;
      }
      frame_settings->values.cparams.resampling = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_EXTRA_CHANNEL_RESAMPLING:
      // TOOD(lode): the jxl codestream allows choosing a different resampling
      // factor for each extra channel, independently per frame. Move this
      // option to a JxlEncoderFrameSettings-option that can be set per extra
      // channel, so needs its own function rather than
      // JxlEncoderFrameSettingsSetOption due to the extra channel index
      // argument required.
      if (value != -1 && value != 1 && value != 2 && value != 4 && value != 8) {
        return JXL_ENC_ERROR;
      }
      frame_settings->values.cparams.ec_resampling = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_ALREADY_DOWNSAMPLED:
      if (value < 0 || value > 1) {
        return JXL_ENC_ERROR;
      }
      frame_settings->values.cparams.already_downsampled = (value == 1);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_PHOTON_NOISE:
      if (value < 0) return JXL_ENC_ERROR;
      // TODO(lode): add encoder setting to set the 8 floating point values of
      // the noise synthesis parameters per frame for more fine grained control.
      frame_settings->values.cparams.photon_noise_iso =
          static_cast<float>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_NOISE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.noise = static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_DOTS:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.dots = static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_PATCHES:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.patches =
          static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_EPF:
      if (value < -1 || value > 3) return JXL_ENC_ERROR;
      frame_settings->values.cparams.epf = static_cast<int>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_GABORISH:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.gaborish =
          static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_MODULAR:
      if (value == 1) {
        frame_settings->values.cparams.modular_mode = true;
      } else if (value == -1 || value == 0) {
        frame_settings->values.cparams.modular_mode = false;
      } else {
        return JXL_ENC_ERROR;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_KEEP_INVISIBLE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.keep_invisible =
          static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_GROUP_ORDER:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.centerfirst = (value == 1);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_GROUP_ORDER_CENTER_X:
      if (value < -1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.center_x = static_cast<size_t>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_GROUP_ORDER_CENTER_Y:
      if (value < -1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.center_y = static_cast<size_t>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_RESPONSIVE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.responsive = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_PROGRESSIVE_AC:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.progressive_mode = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_QPROGRESSIVE_AC:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      frame_settings->values.cparams.qprogressive_mode = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_PROGRESSIVE_DC:
      if (value < -1 || value > 2) return JXL_ENC_ERROR;
      frame_settings->values.cparams.progressive_dc = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GLOBAL_PERCENT:
      if (value < -1 || value > 100) return JXL_ENC_ERROR;
      if (value == -1) {
        frame_settings->values.cparams.channel_colors_pre_transform_percent =
            95.0f;
      } else {
        frame_settings->values.cparams.channel_colors_pre_transform_percent =
            static_cast<float>(value);
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GROUP_PERCENT:
      if (value < -1 || value > 100) return JXL_ENC_ERROR;
      if (value == -1) {
        frame_settings->values.cparams.channel_colors_percent = 80.0f;
      } else {
        frame_settings->values.cparams.channel_colors_percent =
            static_cast<float>(value);
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_PALETTE_COLORS:
      if (value < -1 || value > 70913) return JXL_ENC_ERROR;
      if (value == -1) {
        frame_settings->values.cparams.palette_colors = 1 << 10;
      } else {
        frame_settings->values.cparams.palette_colors = value;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_LOSSY_PALETTE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      // TODO(lode): the defaults of some palette settings depend on others.
      // See the logic in cjxl. Similar for other settings. This should be
      // handled in the encoder during JxlEncoderProcessOutput (or,
      // alternatively, in the cjxl binary like now)
      frame_settings->values.cparams.lossy_palette = (value == 1);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_COLOR_TRANSFORM:
      if (value < -1 || value > 2) return JXL_ENC_ERROR;
      if (value == -1) {
        frame_settings->values.cparams.color_transform =
            jxl::ColorTransform::kXYB;
      } else {
        frame_settings->values.cparams.color_transform =
            static_cast<jxl::ColorTransform>(value);
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_MODULAR_COLOR_SPACE:
      if (value < -1 || value > 35) return JXL_ENC_ERROR;
      frame_settings->values.cparams.colorspace = value + 2;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_MODULAR_GROUP_SIZE:
      if (value < -1 || value > 3) return JXL_ENC_ERROR;
      // TODO(lode): the default behavior of this parameter for cjxl is
      // to choose 1 or 2 depending on the situation. This behavior needs to be
      // implemented either in the C++ library by allowing to set this to -1, or
      // kept in cjxl and set it to 1 or 2 using this API.
      if (value == -1) {
        frame_settings->values.cparams.modular_group_size_shift = 1;
      } else {
        frame_settings->values.cparams.modular_group_size_shift = value;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_MODULAR_PREDICTOR:
      if (value < -1 || value > 15) return JXL_ENC_ERROR;
      frame_settings->values.cparams.options.predictor =
          static_cast<jxl::Predictor>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_MODULAR_MA_TREE_LEARNING_PERCENT:
      if (value < -1) return JXL_ENC_ERROR;
      // This value is called "iterations" or "nb_repeats" in cjxl, but is in
      // fact a fraction in range 0.0-1.0, with the defautl value 0.5.
      // Convert from integer percentage to floating point fraction here.
      if (value == -1) {
        // TODO(lode): for this and many other settings, avoid duplicating the
        // default values here and in enc_params.h and options.h, have one
        // location where the defaults are specified.
        frame_settings->values.cparams.options.nb_repeats = 0.5f;
      } else {
        frame_settings->values.cparams.options.nb_repeats = value * 0.01f;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_MODULAR_NB_PREV_CHANNELS:
      // The max allowed value can in theory be higher. However, it depends on
      // the effort setting. 11 is the highest safe value that doesn't cause
      // tree_samples to be >= 64 in the encoder. The specification may allow
      // more than this. With more fine tuning higher values could be allowed.
      if (value < -1 || value > 11) return JXL_ENC_ERROR;
      if (value == -1) {
        frame_settings->values.cparams.options.max_properties = 0;
      } else {
        frame_settings->values.cparams.options.max_properties = value;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_FRAME_SETTING_JPEG_RECON_CFL:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      if (value == -1) {
        frame_settings->values.cparams.force_cfl_jpeg_recompression = true;
      } else {
        frame_settings->values.cparams.force_cfl_jpeg_recompression = value;
      }
      return JXL_ENC_SUCCESS;
    default:
      return JXL_ENC_ERROR;
  }
}

JxlEncoder* JxlEncoderCreate(const JxlMemoryManager* memory_manager) {
  JxlMemoryManager local_memory_manager;
  if (!jxl::MemoryManagerInit(&local_memory_manager, memory_manager)) {
    return nullptr;
  }

  void* alloc =
      jxl::MemoryManagerAlloc(&local_memory_manager, sizeof(JxlEncoder));
  if (!alloc) return nullptr;
  JxlEncoder* enc = new (alloc) JxlEncoder();
  enc->memory_manager = local_memory_manager;
  // TODO(sboukortt): add an API function to set this.
  enc->cms = jxl::GetJxlCms();

  // Initialize all the field values.
  JxlEncoderReset(enc);

  return enc;
}

void JxlEncoderReset(JxlEncoder* enc) {
  enc->thread_pool.reset();
  enc->input_queue.clear();
  enc->num_queued_frames = 0;
  enc->num_queued_boxes = 0;
  enc->encoder_options.clear();
  enc->output_byte_queue.clear();
  enc->codestream_bytes_written_beginning_of_frame = 0;
  enc->codestream_bytes_written_end_of_frame = 0;
  enc->wrote_bytes = false;
  enc->jxlp_counter = 0;
  enc->metadata = jxl::CodecMetadata();
  enc->last_used_cparams = jxl::CompressParams();
  enc->frames_closed = false;
  enc->boxes_closed = false;
  enc->basic_info_set = false;
  enc->color_encoding_set = false;
  enc->intensity_target_set = false;
  enc->use_container = false;
  enc->use_boxes = false;
  enc->codestream_level = 5;
}

void JxlEncoderDestroy(JxlEncoder* enc) {
  if (enc) {
    JxlMemoryManager local_memory_manager = enc->memory_manager;
    // Call destructor directly since custom free function is used.
    enc->~JxlEncoder();
    jxl::MemoryManagerFree(&local_memory_manager, enc);
  }
}

JxlEncoderStatus JxlEncoderUseContainer(JxlEncoder* enc,
                                        JXL_BOOL use_container) {
  if (enc->wrote_bytes) {
    return JXL_API_ERROR("this setting can only be set at the beginning");
  }
  enc->use_container = static_cast<bool>(use_container);
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderStoreJPEGMetadata(JxlEncoder* enc,
                                             JXL_BOOL store_jpeg_metadata) {
  if (enc->wrote_bytes) {
    return JXL_API_ERROR("this setting can only be set at the beginning");
  }
  enc->store_jpeg_metadata = static_cast<bool>(store_jpeg_metadata);
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderSetCodestreamLevel(JxlEncoder* enc, int level) {
  if (level != 5 && level != 10) return JXL_API_ERROR("invalid level");
  if (enc->wrote_bytes) {
    return JXL_API_ERROR("this setting can only be set at the beginning");
  }
  enc->codestream_level = level;
  return JXL_ENC_SUCCESS;
}

int JxlEncoderGetRequiredCodestreamLevel(const JxlEncoder* enc) {
  return VerifyLevelSettings(enc, nullptr);
}

void JxlEncoderSetCms(JxlEncoder* enc, JxlCmsInterface cms) {
  jxl::msan::MemoryIsInitialized(&cms, sizeof(cms));
  enc->cms = cms;
}

JxlEncoderStatus JxlEncoderSetParallelRunner(JxlEncoder* enc,
                                             JxlParallelRunner parallel_runner,
                                             void* parallel_runner_opaque) {
  if (enc->thread_pool) return JXL_API_ERROR("parallel runner already set");
  enc->thread_pool = jxl::MemoryManagerMakeUnique<jxl::ThreadPool>(
      &enc->memory_manager, parallel_runner, parallel_runner_opaque);
  if (!enc->thread_pool) {
    return JXL_ENC_ERROR;
  }
  return JXL_ENC_SUCCESS;
}

namespace {
JxlEncoderStatus GetCurrentDimensions(
    const JxlEncoderFrameSettings* frame_settings, size_t& xsize,
    size_t& ysize) {
  xsize = frame_settings->enc->metadata.xsize();
  ysize = frame_settings->enc->metadata.ysize();
  if (frame_settings->values.header.layer_info.have_crop) {
    xsize = frame_settings->values.header.layer_info.xsize;
    ysize = frame_settings->values.header.layer_info.ysize;
  }
  if (frame_settings->values.cparams.already_downsampled) {
    size_t factor = frame_settings->values.cparams.resampling;
    xsize = jxl::DivCeil(xsize, factor);
    ysize = jxl::DivCeil(ysize, factor);
  }
  if (xsize == 0 || ysize == 0) {
    return JXL_API_ERROR("zero-sized frame is not allowed");
  }
  return JXL_ENC_SUCCESS;
}
}  // namespace

JxlEncoderStatus JxlEncoderAddJPEGFrame(
    const JxlEncoderFrameSettings* frame_settings, const uint8_t* buffer,
    size_t size) {
  if (frame_settings->enc->frames_closed) {
    return JXL_ENC_ERROR;
  }

  jxl::CodecInOut io;
  if (!jxl::jpeg::DecodeImageJPG(jxl::Span<const uint8_t>(buffer, size), &io)) {
    return JXL_ENC_ERROR;
  }

  if (!frame_settings->enc->color_encoding_set) {
    if (!SetColorEncodingFromJpegData(
            *io.Main().jpeg_data,
            &frame_settings->enc->metadata.m.color_encoding)) {
      return JXL_ENC_ERROR;
    }
  }

  if (!frame_settings->enc->basic_info_set) {
    JxlBasicInfo basic_info;
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = io.Main().jpeg_data->width;
    basic_info.ysize = io.Main().jpeg_data->height;
    basic_info.uses_original_profile = true;
    if (JxlEncoderSetBasicInfo(frame_settings->enc, &basic_info) !=
        JXL_ENC_SUCCESS) {
      return JXL_ENC_ERROR;
    }
  }

  if (frame_settings->enc->metadata.m.xyb_encoded) {
    // Can't XYB encode a lossless JPEG.
    return JXL_ENC_ERROR;
  }

  if (frame_settings->enc->store_jpeg_metadata) {
    jxl::jpeg::JPEGData data_in = *io.Main().jpeg_data;
    jxl::PaddedBytes jpeg_data;
    if (!jxl::jpeg::EncodeJPEGData(data_in, &jpeg_data,
                                   frame_settings->values.cparams)) {
      return JXL_ENC_ERROR;
    }
    frame_settings->enc->jpeg_metadata = std::vector<uint8_t>(
        jpeg_data.data(), jpeg_data.data() + jpeg_data.size());
  }

  auto queued_frame = jxl::MemoryManagerMakeUnique<jxl::JxlEncoderQueuedFrame>(
      &frame_settings->enc->memory_manager,
      // JxlEncoderQueuedFrame is a struct with no constructors, so we use the
      // default move constructor there.
      jxl::JxlEncoderQueuedFrame{
          frame_settings->values,
          jxl::ImageBundle(&frame_settings->enc->metadata.m),
          {}});
  if (!queued_frame) {
    return JXL_ENC_ERROR;
  }
  queued_frame->frame.SetFromImage(std::move(*io.Main().color()),
                                   io.Main().c_current());
  size_t xsize, ysize;
  if (GetCurrentDimensions(frame_settings, xsize, ysize) != JXL_ENC_SUCCESS) {
    return JXL_API_ERROR("bad dimensions");
  }
  if (xsize != static_cast<size_t>(io.Main().jpeg_data->width) ||
      ysize != static_cast<size_t>(io.Main().jpeg_data->height)) {
    return JXL_API_ERROR("JPEG dimensions don't match frame dimensions");
  }
  std::vector<jxl::ImageF> extra_channels(
      frame_settings->enc->metadata.m.num_extra_channels);
  for (auto& extra_channel : extra_channels) {
    extra_channel = jxl::ImageF(xsize, ysize);
    queued_frame->ec_initialized.push_back(0);
  }
  queued_frame->frame.SetExtraChannels(std::move(extra_channels));
  queued_frame->frame.jpeg_data = std::move(io.Main().jpeg_data);
  queued_frame->frame.color_transform = io.Main().color_transform;
  queued_frame->frame.chroma_subsampling = io.Main().chroma_subsampling;

  QueueFrame(frame_settings, queued_frame);
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderAddImageFrame(
    const JxlEncoderFrameSettings* frame_settings,
    const JxlPixelFormat* pixel_format, const void* buffer, size_t size) {
  if (!frame_settings->enc->basic_info_set ||
      (!frame_settings->enc->color_encoding_set &&
       !frame_settings->enc->metadata.m.xyb_encoded)) {
    // Basic Info must be set, and color encoding must be set directly,
    // or set to XYB via JxlBasicInfo.uses_original_profile = JXL_FALSE
    // Otherwise, this is an API misuse.
    return JXL_ENC_ERROR;
  }

  if (frame_settings->enc->frames_closed) {
    return JXL_ENC_ERROR;
  }

  auto queued_frame = jxl::MemoryManagerMakeUnique<jxl::JxlEncoderQueuedFrame>(
      &frame_settings->enc->memory_manager,
      // JxlEncoderQueuedFrame is a struct with no constructors, so we use the
      // default move constructor there.
      jxl::JxlEncoderQueuedFrame{
          frame_settings->values,
          jxl::ImageBundle(&frame_settings->enc->metadata.m),
          {}});

  if (!queued_frame) {
    return JXL_ENC_ERROR;
  }

  jxl::ColorEncoding c_current;
  if (!frame_settings->enc->color_encoding_set) {
    if ((pixel_format->data_type == JXL_TYPE_FLOAT) ||
        (pixel_format->data_type == JXL_TYPE_FLOAT16)) {
      c_current =
          jxl::ColorEncoding::LinearSRGB(pixel_format->num_channels < 3);
    } else {
      c_current = jxl::ColorEncoding::SRGB(pixel_format->num_channels < 3);
    }
  } else {
    c_current = frame_settings->enc->metadata.m.color_encoding;
  }
  uint32_t num_channels = pixel_format->num_channels;
  size_t has_interleaved_alpha =
      static_cast<size_t>(num_channels == 2 || num_channels == 4);
  if (has_interleaved_alpha >
      frame_settings->enc->metadata.m.num_extra_channels) {
    return JXL_API_ERROR("number of extra channels mismatch");
  }
  size_t xsize, ysize;
  if (GetCurrentDimensions(frame_settings, xsize, ysize) != JXL_ENC_SUCCESS) {
    return JXL_API_ERROR("bad dimensions");
  }
  std::vector<jxl::ImageF> extra_channels(
      frame_settings->enc->metadata.m.num_extra_channels);
  for (auto& extra_channel : extra_channels) {
    extra_channel = jxl::ImageF(xsize, ysize);
  }
  queued_frame->frame.SetExtraChannels(std::move(extra_channels));
  for (auto& ec_info : frame_settings->enc->metadata.m.extra_channel_info) {
    if (has_interleaved_alpha && ec_info.type == jxl::ExtraChannel::kAlpha) {
      queued_frame->ec_initialized.push_back(1);
      has_interleaved_alpha = 0;  // only first Alpha is initialized
    } else {
      queued_frame->ec_initialized.push_back(0);
    }
  }
  queued_frame->frame.origin.x0 =
      frame_settings->values.header.layer_info.crop_x0;
  queued_frame->frame.origin.y0 =
      frame_settings->values.header.layer_info.crop_y0;
  queued_frame->frame.use_for_next_frame =
      (frame_settings->values.header.layer_info.save_as_reference != 0u);
  queued_frame->frame.blendmode =
      frame_settings->values.header.layer_info.blend_info.blendmode ==
              JXL_BLEND_REPLACE
          ? jxl::BlendMode::kReplace
          : jxl::BlendMode::kBlend;
  queued_frame->frame.blend =
      frame_settings->values.header.layer_info.blend_info.source > 0;

  if (!jxl::BufferToImageBundle(*pixel_format, xsize, ysize, buffer, size,
                                frame_settings->enc->thread_pool.get(),
                                c_current, &(queued_frame->frame))) {
    return JXL_ENC_ERROR;
  }
  if (frame_settings->values.lossless &&
      frame_settings->enc->metadata.m.xyb_encoded) {
    return JXL_API_ERROR("Set use_original_profile=true for lossless encoding");
  }
  queued_frame->option_values.cparams.level =
      frame_settings->enc->codestream_level;

  QueueFrame(frame_settings, queued_frame);
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderUseBoxes(JxlEncoder* enc) {
  if (enc->wrote_bytes) {
    return JXL_API_ERROR("this setting can only be set at the beginning");
  }
  enc->use_boxes = true;
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderAddBox(JxlEncoder* enc, const JxlBoxType type,
                                  const uint8_t* contents, size_t size,
                                  JXL_BOOL compress_box) {
  if (!enc->use_boxes) {
    return JXL_API_ERROR(
        "must set JxlEncoderUseBoxes at the beginning to add boxes");
  }
  if (compress_box) {
    if (memcmp("jxl", type, 3) == 0) {
      return JXL_API_ERROR(
          "brob box may not contain a type starting with \"jxl\"");
    }
    if (memcmp("jbrd", type, 4) == 0) {
      return JXL_API_ERROR("jbrd box may not be brob compressed");
    }
    if (memcmp("brob", type, 4) == 0) {
      // The compress_box will compress an existing non-brob box into a brob
      // box. If already giving a valid brotli-compressed brob box, set
      // compress_box to false since it is already compressed.
      return JXL_API_ERROR("a brob box cannot contain another brob box");
    }
  }

  auto box = jxl::MemoryManagerMakeUnique<jxl::JxlEncoderQueuedBox>(
      &enc->memory_manager);

  box->type = jxl::MakeBoxType(type);
  box->contents.assign(contents, contents + size);
  box->compress_box = !!compress_box;
  QueueBox(enc, box);
  return JXL_ENC_SUCCESS;
}

JXL_EXPORT JxlEncoderStatus JxlEncoderSetExtraChannelBuffer(
    const JxlEncoderOptions* frame_settings, const JxlPixelFormat* pixel_format,
    const void* buffer, size_t size, uint32_t index) {
  if (index >= frame_settings->enc->metadata.m.num_extra_channels) {
    return JXL_API_ERROR("Invalid value for the index of extra channel");
  }
  if (!frame_settings->enc->basic_info_set ||
      !frame_settings->enc->color_encoding_set) {
    return JXL_ENC_ERROR;
  }
  if (frame_settings->enc->input_queue.empty()) {
    return JXL_ENC_ERROR;
  }
  if (frame_settings->enc->frames_closed) {
    return JXL_ENC_ERROR;
  }
  size_t xsize, ysize;
  if (GetCurrentDimensions(frame_settings, xsize, ysize) != JXL_ENC_SUCCESS) {
    return JXL_API_ERROR("bad dimensions");
  }
  if (!jxl::BufferToImageF(*pixel_format, xsize, ysize, buffer, size,
                           frame_settings->enc->thread_pool.get(),
                           &frame_settings->enc->input_queue.back()
                                .frame->frame.extra_channels()[index])) {
    return JXL_API_ERROR("Failed to set buffer for extra channel");
  }
  frame_settings->enc->input_queue.back().frame->ec_initialized[index] = 1;

  return JXL_ENC_SUCCESS;
}

void JxlEncoderCloseFrames(JxlEncoder* enc) { enc->frames_closed = true; }

void JxlEncoderCloseBoxes(JxlEncoder* enc) { enc->boxes_closed = true; }

void JxlEncoderCloseInput(JxlEncoder* enc) {
  JxlEncoderCloseFrames(enc);
  JxlEncoderCloseBoxes(enc);
}
JxlEncoderStatus JxlEncoderProcessOutput(JxlEncoder* enc, uint8_t** next_out,
                                         size_t* avail_out) {
  while (*avail_out > 0 &&
         (!enc->output_byte_queue.empty() || !enc->input_queue.empty())) {
    if (!enc->output_byte_queue.empty()) {
      size_t to_copy = std::min(*avail_out, enc->output_byte_queue.size());
      std::copy_n(enc->output_byte_queue.begin(), to_copy, *next_out);
      *next_out += to_copy;
      *avail_out -= to_copy;
      enc->output_byte_queue.erase(enc->output_byte_queue.begin(),
                                   enc->output_byte_queue.begin() + to_copy);
    } else if (!enc->input_queue.empty()) {
      if (enc->RefillOutputByteQueue() != JXL_ENC_SUCCESS) {
        return JXL_ENC_ERROR;
      }
    }
  }

  if (!enc->output_byte_queue.empty() || !enc->input_queue.empty()) {
    return JXL_ENC_NEED_MORE_OUTPUT;
  }
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderSetFrameHeader(JxlEncoderOptions* frame_settings,
                                          const JxlFrameHeader* frame_header) {
  if (frame_header->layer_info.blend_info.source > 3) {
    return JXL_API_ERROR("invalid blending source index");
  }
  // If there are no extra channels, it's ok for the value to be 0.
  if (frame_header->layer_info.blend_info.alpha != 0 &&
      frame_header->layer_info.blend_info.alpha >=
          frame_settings->enc->metadata.m.extra_channel_info.size()) {
    return JXL_API_ERROR("alpha blend channel index out of bounds");
  }

  frame_settings->values.header = *frame_header;
  // Setting the frame header resets the frame name, it must be set again with
  // JxlEncoderSetFrameName if desired.
  frame_settings->values.frame_name = "";

  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderSetExtraChannelBlendInfo(
    JxlEncoderOptions* frame_settings, size_t index,
    const JxlBlendInfo* blend_info) {
  if (index >= frame_settings->enc->metadata.m.num_extra_channels) {
    return JXL_API_ERROR("Invalid value for the index of extra channel");
  }

  if (frame_settings->values.extra_channel_blend_info.size() !=
      frame_settings->enc->metadata.m.num_extra_channels) {
    JxlBlendInfo default_blend_info;
    JxlEncoderInitBlendInfo(&default_blend_info);
    frame_settings->values.extra_channel_blend_info.resize(
        frame_settings->enc->metadata.m.num_extra_channels, default_blend_info);
  }
  frame_settings->values.extra_channel_blend_info[index] = *blend_info;
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderSetFrameName(JxlEncoderFrameSettings* frame_settings,
                                        const char* frame_name) {
  std::string str = frame_name ? frame_name : "";
  if (str.size() > 1071) {
    return JXL_API_ERROR("frame name can be max 1071 bytes long");
  }
  frame_settings->values.frame_name = str;
  frame_settings->values.header.name_length = str.size();
  return JXL_ENC_SUCCESS;
}

void JxlColorEncodingSetToSRGB(JxlColorEncoding* color_encoding,
                               JXL_BOOL is_gray) {
  ConvertInternalToExternalColorEncoding(jxl::ColorEncoding::SRGB(is_gray),
                                         color_encoding);
}

void JxlColorEncodingSetToLinearSRGB(JxlColorEncoding* color_encoding,
                                     JXL_BOOL is_gray) {
  ConvertInternalToExternalColorEncoding(
      jxl::ColorEncoding::LinearSRGB(is_gray), color_encoding);
}
