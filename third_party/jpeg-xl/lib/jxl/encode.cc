// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "jxl/encode.h"

#include <brotli/encode.h>

#include <algorithm>
#include <cstring>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/codec_in_out.h"
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
  StoreBE32(counter, jxl::Extend(output, 4));
}

void QueueFrame(
    const JxlEncoderOptions* options,
    jxl::MemoryManagerUniquePtr<jxl::JxlEncoderQueuedFrame>& frame) {
  if (options->values.lossless) {
    frame->option_values.cparams.SetLossless();
  }

  jxl::JxlEncoderQueuedInput queued_input(options->enc->memory_manager);
  queued_input.frame = std::move(frame);
  options->enc->input_queue.emplace_back(std::move(queued_input));
  options->enc->num_queued_frames++;
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
}  // namespace

JxlEncoderStatus JxlEncoderStruct::RefillOutputByteQueue() {
  jxl::PaddedBytes bytes;

  jxl::JxlEncoderQueuedInput& input = input_queue[0];

  // TODO(lode): split this into 3 functions: for adding the signature and other
  // initial headers (jbrd, ...), one for adding frame, and one for adding user
  // box.

  if (!wrote_bytes) {
    jxl::BitWriter writer;
    if (!WriteHeaders(&metadata, &writer, nullptr)) {
      return JXL_ENC_ERROR;
    }
    // Only send ICC (at least several hundred bytes) if fields aren't enough.
    if (metadata.m.color_encoding.WantICC()) {
      if (!jxl::WriteICC(metadata.m.color_encoding.ICC(), &writer,
                         jxl::kLayerHeader, nullptr)) {
        return JXL_ENC_ERROR;
      }
    }
    // TODO(lode): preview should be added here if a preview image is added

    writer.ZeroPadToByte();
    bytes = std::move(writer).TakeBytes();

    if (MustUseContainer()) {
      // Add "JXL " and ftyp box.
      output_byte_queue.insert(
          output_byte_queue.end(), jxl::kContainerHeader,
          jxl::kContainerHeader + sizeof(jxl::kContainerHeader));
      if (codestream_level != 5) {
        // Add jxll box.
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

      if (store_jpeg_metadata && jpeg_metadata.size() > 0) {
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
    if (!jxl::EncodeFrame(input_frame->option_values.cparams, jxl::FrameInfo{},
                          &metadata, input_frame->frame, &enc_state,
                          thread_pool.get(), &writer,
                          /*aux_out=*/nullptr)) {
      return JXL_ENC_ERROR;
    }

    // Possibly bytes already contains the codestream header: in case this is
    // the first frame, and the codestream header was not encoded as jxlp above.
    bytes.append(std::move(writer).TakeBytes());

    if (MustUseContainer()) {
      bool last_frame = frames_closed && !num_queued_frames;
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
      if (JXL_ENC_SUCCESS != BrotliCompress(9, box->contents.data(),
                                            box->contents.size(),
                                            &compressed)) {
        return JXL_ENC_ERROR;
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
  return JXL_ENC_SUCCESS;
}

void JxlEncoderInitBasicInfo(JxlBasicInfo* info) {
  info->have_container = JXL_FALSE;
  info->xsize = 0;
  info->ysize = 0;
  info->bits_per_sample = 8;
  info->exponent_bits_per_sample = 0;
  info->intensity_target = 255.f;
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
  info->animation.tps_numerator = 10;
  info->animation.tps_denominator = 1;
  info->animation.num_loops = 0;
  info->animation.have_timecodes = JXL_FALSE;
}

JxlEncoderStatus JxlEncoderSetBasicInfo(JxlEncoder* enc,
                                        const JxlBasicInfo* info) {
  if (!enc->metadata.size.Set(info->xsize, info->ysize)) {
    return JXL_ENC_ERROR;
  }
  if (!info->exponent_bits_per_sample) {
    if (info->bits_per_sample > 0 && info->bits_per_sample <= 24) {
      enc->metadata.m.SetUintSamples(info->bits_per_sample);
    } else {
      return JXL_ENC_ERROR;
    }
  } else if (info->bits_per_sample == 32 &&
             info->exponent_bits_per_sample == 8) {
    enc->metadata.m.SetFloat32Samples();
  } else if (info->bits_per_sample == 16 &&
             info->exponent_bits_per_sample == 5) {
    enc->metadata.m.SetFloat16Samples();
  } else {
    return JXL_ENC_NOT_SUPPORTED;
  }
  if (info->alpha_bits > 0 && info->alpha_exponent_bits > 0) {
    return JXL_ENC_NOT_SUPPORTED;
  }
  switch (info->alpha_bits) {
    case 0:
      break;
    case 32:
    case 16:
      enc->metadata.m.SetAlphaBits(16);
      break;
    case 8:
      enc->metadata.m.SetAlphaBits(info->alpha_bits);
      break;
    default:
      return JXL_ENC_ERROR;
  }
  enc->metadata.m.xyb_encoded = !info->uses_original_profile;
  if (info->orientation > 0 && info->orientation <= 8) {
    enc->metadata.m.orientation = info->orientation;
  } else {
    return JXL_API_ERROR("Invalid value for orientation field");
  }
  enc->metadata.m.SetIntensityTarget(info->intensity_target);
  enc->metadata.m.tone_mapping.min_nits = info->min_nits;
  enc->metadata.m.tone_mapping.relative_to_max_display =
      info->relative_to_max_display;
  enc->metadata.m.tone_mapping.linear_below = info->linear_below;
  enc->basic_info_set = true;
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
  jxl::ExtraChannelInfo& channel = enc->metadata.m.extra_channel_info[index];
  channel.type = static_cast<jxl::ExtraChannel>(info->type);
  channel.bit_depth.bits_per_sample = info->bits_per_sample;
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

JxlEncoderOptions* JxlEncoderOptionsCreate(JxlEncoder* enc,
                                           const JxlEncoderOptions* source) {
  auto opts =
      jxl::MemoryManagerMakeUnique<JxlEncoderOptions>(&enc->memory_manager);
  if (!opts) return nullptr;
  opts->enc = enc;
  if (source != nullptr) {
    opts->values = source->values;
  } else {
    opts->values.lossless = false;
  }
  JxlEncoderOptions* ret = opts.get();
  enc->encoder_options.emplace_back(std::move(opts));
  return ret;
}

JxlEncoderStatus JxlEncoderOptionsSetLossless(JxlEncoderOptions* options,
                                              const JXL_BOOL lossless) {
  options->values.lossless = lossless;
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderOptionsSetEffort(JxlEncoderOptions* options,
                                            const int effort) {
  return JxlEncoderOptionsSetInteger(options, JXL_ENC_OPTION_EFFORT, effort);
}

JxlEncoderStatus JxlEncoderOptionsSetDistance(JxlEncoderOptions* options,
                                              float distance) {
  if (distance < 0 || distance > 15) {
    return JXL_ENC_ERROR;
  }
  options->values.cparams.butteraugli_distance = distance;
  float jpeg_quality;
  // Formula to translate butteraugli distance roughly into JPEG 0-100 quality.
  // This is the inverse of the formula in cjxl.cc to translate JPEG quality
  // into butteraugli distance.
  if (distance < 6.56f) {
    jpeg_quality = -5.456783f * std::log(0.0256f * distance - 0.16384f);
  } else {
    jpeg_quality = -11.11111f * distance + 101.11111f;
  }
  // Translate JPEG quality into the quality_pair setting for modular encoding.
  // This is the formula also used in cjxl.cc to convert the command line JPEG
  // quality parameter to the quality_pair setting.
  // TODO(lode): combine the distance -> quality_pair conversion into a single
  // formula, possibly altering it to a more suitable heuristic.
  float quality;
  if (jpeg_quality < 7) {
    quality = std::min<float>(35 + (jpeg_quality - 7) * 3.0f, 100.0f);
  } else {
    quality = std::min<float>(35 + (jpeg_quality - 7) * 65.f / 93.f, 100.0f);
  }
  options->values.cparams.quality_pair.first =
      options->values.cparams.quality_pair.second = quality;
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderOptionsSetDecodingSpeed(JxlEncoderOptions* options,
                                                   int tier) {
  return JxlEncoderOptionsSetInteger(options, JXL_ENC_OPTION_DECODING_SPEED,
                                     tier);
}

JxlEncoderStatus JxlEncoderOptionsSetInteger(JxlEncoderOptions* options,
                                             JxlEncoderOptionId option,
                                             int32_t value) {
  switch (option) {
    case JXL_ENC_OPTION_EFFORT:
      if (value < 1 || value > 9) {
        return JXL_ENC_ERROR;
      }
      options->values.cparams.speed_tier =
          static_cast<jxl::SpeedTier>(10 - value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_DECODING_SPEED:
      if (value < 0 || value > 4) {
        return JXL_ENC_ERROR;
      }
      options->values.cparams.decoding_speed_tier = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_RESAMPLING:
      if (value != -1 && value != 1 && value != 2 && value != 4 && value != 8) {
        return JXL_ENC_ERROR;
      }
      options->values.cparams.resampling = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_EXTRA_CHANNEL_RESAMPLING:
      // TOOD(lode): the jxl codestream allows choosing a different resampling
      // factor for each extra channel, independently per frame. Move this
      // option to a JxlEncoderOptions-option that can be set per extra channel,
      // so needs its own function rather than JxlEncoderOptionsSetInteger due
      // to the extra channel index argument required.
      if (value != -1 && value != 1 && value != 2 && value != 4 && value != 8) {
        return JXL_ENC_ERROR;
      }
      // The implementation doesn't support the default choice between 1x1 and
      // 2x2 for extra channels, so 1x1 is set as the default.
      if (value == -1) value = 1;
      options->values.cparams.ec_resampling = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_ALREADY_DOWNSAMPLED:
      if (value < 0 || value > 1) {
        return JXL_ENC_ERROR;
      }
      options->values.cparams.already_downsampled = (value == 1);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_PHOTON_NOISE:
      if (value < 0) return JXL_ENC_ERROR;
      // TODO(lode): add encoder setting to set the 8 floating point values of
      // the noise synthesis parameters per frame for more fine grained control.
      options->values.cparams.photon_noise_iso = static_cast<float>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_NOISE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.noise = static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_DOTS:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.dots = static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_PATCHES:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.patches = static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_EPF:
      if (value < -1 || value > 3) return JXL_ENC_ERROR;
      options->values.cparams.epf = static_cast<int>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_GABORISH:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.gaborish = static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_MODULAR:
      if (value == 1) {
        options->values.cparams.modular_mode = true;
      } else if (value == -1 || value == 0) {
        options->values.cparams.modular_mode = false;
      } else {
        return JXL_ENC_ERROR;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_KEEP_INVISIBLE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.keep_invisible =
          static_cast<jxl::Override>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_GROUP_ORDER:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.centerfirst = (value == 1);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_GROUP_ORDER_CENTER_X:
      if (value < -1) return JXL_ENC_ERROR;
      options->values.cparams.center_x = static_cast<size_t>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_GROUP_ORDER_CENTER_Y:
      if (value < -1) return JXL_ENC_ERROR;
      options->values.cparams.center_y = static_cast<size_t>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_RESPONSIVE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.responsive = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_PROGRESSIVE_AC:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.progressive_mode = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_QPROGRESSIVE_AC:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      options->values.cparams.qprogressive_mode = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_PROGRESSIVE_DC:
      if (value < -1 || value > 2) return JXL_ENC_ERROR;
      options->values.cparams.progressive_dc = value;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_CHANNEL_COLORS_GLOBAL_PERCENT:
      if (value < -1 || value > 100) return JXL_ENC_ERROR;
      if (value == -1) {
        options->values.cparams.channel_colors_pre_transform_percent = 95.0f;
      } else {
        options->values.cparams.channel_colors_pre_transform_percent =
            static_cast<float>(value);
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_CHANNEL_COLORS_GROUP_PERCENT:
      if (value < -1 || value > 100) return JXL_ENC_ERROR;
      if (value == -1) {
        options->values.cparams.channel_colors_percent = 80.0f;
      } else {
        options->values.cparams.channel_colors_percent =
            static_cast<float>(value);
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_PALETTE_COLORS:
      if (value < -1 || value > 70913) return JXL_ENC_ERROR;
      if (value == -1) {
        options->values.cparams.palette_colors = 1 << 10;
      } else {
        options->values.cparams.palette_colors = value;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_LOSSY_PALETTE:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      // TODO(lode): the defaults of some palette settings depend on others.
      // See the logic in cjxl. Similar for other settings. This should be
      // handled in the encoder during JxlEncoderProcessOutput (or,
      // alternatively, in the cjxl binary like now)
      options->values.cparams.lossy_palette = (value == 1);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_COLOR_TRANSFORM:
      if (value < -1 || value > 2) return JXL_ENC_ERROR;
      if (value == -1) {
        options->values.cparams.color_transform = jxl::ColorTransform::kXYB;
      } else {
        options->values.cparams.color_transform =
            static_cast<jxl::ColorTransform>(value);
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_MODULAR_COLOR_SPACE:
      if (value < -1 || value > 35) return JXL_ENC_ERROR;
      options->values.cparams.colorspace = value + 2;
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_MODULAR_GROUP_SIZE:
      if (value < -1 || value > 3) return JXL_ENC_ERROR;
      // TODO(lode): the default behavior of this parameter for cjxl is
      // to choose 1 or 2 depending on the situation. This behavior needs to be
      // implemented either in the C++ library by allowing to set this to -1, or
      // kept in cjxl and set it to 1 or 2 using this API.
      if (value == -1) {
        options->values.cparams.modular_group_size_shift = 1;
      } else {
        options->values.cparams.modular_group_size_shift = value;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_MODULAR_PREDICTOR:
      if (value < -1 || value > 15) return JXL_ENC_ERROR;
      options->values.cparams.options.predictor =
          static_cast<jxl::Predictor>(value);
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_MODULAR_MA_TREE_LEARNING_PERCENT:
      if (value < -1) return JXL_ENC_ERROR;
      // This value is called "iterations" or "nb_repeats" in cjxl, but is in
      // fact a fraction in range 0.0-1.0, with the defautl value 0.5.
      // Convert from integer percentage to floating point fraction here.
      if (value == -1) {
        // TODO(lode): for this and many other settings, avoid duplicating the
        // default values here and in enc_params.h and options.h, have one
        // location where the defaults are specified.
        options->values.cparams.options.nb_repeats = 0.5f;
      } else {
        options->values.cparams.options.nb_repeats = value * 0.01f;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_MODULAR_NB_PREV_CHANNELS:
      // The max allowed value can in theory be higher. However, it depends on
      // the effort setting. 11 is the highest safe value that doesn't cause
      // tree_samples to be >= 64 in the encoder. The specification may allow
      // more than this. With more fine tuning higher values could be allowed.
      if (value < -1 || value > 11) return JXL_ENC_ERROR;
      if (value == -1) {
        options->values.cparams.options.max_properties = 0;
      } else {
        options->values.cparams.options.max_properties = value;
      }
      return JXL_ENC_SUCCESS;
    case JXL_ENC_OPTION_JPEG_RECON_CFL:
      if (value < -1 || value > 1) return JXL_ENC_ERROR;
      if (value == -1) {
        options->values.cparams.force_cfl_jpeg_recompression = true;
      } else {
        options->values.cparams.force_cfl_jpeg_recompression = value;
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
  enc->wrote_bytes = false;
  enc->jxlp_counter = 0;
  enc->metadata = jxl::CodecMetadata();
  enc->last_used_cparams = jxl::CompressParams();
  enc->frames_closed = false;
  enc->boxes_closed = false;
  enc->basic_info_set = false;
  enc->color_encoding_set = false;
  enc->use_container = false;
  enc->use_boxes = false;
  enc->codestream_level = 5;
}

void JxlEncoderDestroy(JxlEncoder* enc) {
  if (enc) {
    // Call destructor directly since custom free function is used.
    enc->~JxlEncoder();
    jxl::MemoryManagerFree(&enc->memory_manager, enc);
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

JxlEncoderStatus JxlEncoderAddJPEGFrame(const JxlEncoderOptions* options,
                                        const uint8_t* buffer, size_t size) {
  if (options->enc->frames_closed) {
    return JXL_ENC_ERROR;
  }

  jxl::CodecInOut io;
  if (!jxl::jpeg::DecodeImageJPG(jxl::Span<const uint8_t>(buffer, size), &io)) {
    return JXL_ENC_ERROR;
  }

  if (!options->enc->color_encoding_set) {
    if (!SetColorEncodingFromJpegData(
            *io.Main().jpeg_data, &options->enc->metadata.m.color_encoding)) {
      return JXL_ENC_ERROR;
    }
  }

  if (!options->enc->basic_info_set) {
    JxlBasicInfo basic_info;
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = io.Main().jpeg_data->width;
    basic_info.ysize = io.Main().jpeg_data->height;
    basic_info.uses_original_profile = true;
    if (JxlEncoderSetBasicInfo(options->enc, &basic_info) != JXL_ENC_SUCCESS) {
      return JXL_ENC_ERROR;
    }
  }

  if (options->enc->metadata.m.xyb_encoded) {
    // Can't XYB encode a lossless JPEG.
    return JXL_ENC_ERROR;
  }

  if (options->enc->store_jpeg_metadata) {
    jxl::jpeg::JPEGData data_in = *io.Main().jpeg_data;
    jxl::PaddedBytes jpeg_data;
    if (!EncodeJPEGData(data_in, &jpeg_data)) {
      return JXL_ENC_ERROR;
    }
    options->enc->jpeg_metadata = std::vector<uint8_t>(
        jpeg_data.data(), jpeg_data.data() + jpeg_data.size());
  }

  auto queued_frame = jxl::MemoryManagerMakeUnique<jxl::JxlEncoderQueuedFrame>(
      &options->enc->memory_manager,
      // JxlEncoderQueuedFrame is a struct with no constructors, so we use the
      // default move constructor there.
      jxl::JxlEncoderQueuedFrame{options->values,
                                 jxl::ImageBundle(&options->enc->metadata.m)});
  if (!queued_frame) {
    return JXL_ENC_ERROR;
  }
  queued_frame->frame.SetFromImage(std::move(*io.Main().color()),
                                   io.Main().c_current());
  queued_frame->frame.jpeg_data = std::move(io.Main().jpeg_data);
  queued_frame->frame.color_transform = io.Main().color_transform;
  queued_frame->frame.chroma_subsampling = io.Main().chroma_subsampling;

  QueueFrame(options, queued_frame);
  return JXL_ENC_SUCCESS;
}

JxlEncoderStatus JxlEncoderAddImageFrame(const JxlEncoderOptions* options,
                                         const JxlPixelFormat* pixel_format,
                                         const void* buffer, size_t size) {
  if (!options->enc->basic_info_set ||
      (!options->enc->color_encoding_set &&
       !options->enc->metadata.m.xyb_encoded)) {
    // Basic Info must be set, and color encoding must be set directly,
    // or set to XYB via JxlBasicInfo.uses_original_profile = JXL_FALSE
    // Otherwise, this is an API misuse.
    return JXL_ENC_ERROR;
  }

  if (options->enc->frames_closed) {
    return JXL_ENC_ERROR;
  }

  auto queued_frame = jxl::MemoryManagerMakeUnique<jxl::JxlEncoderQueuedFrame>(
      &options->enc->memory_manager,
      // JxlEncoderQueuedFrame is a struct with no constructors, so we use the
      // default move constructor there.
      jxl::JxlEncoderQueuedFrame{options->values,
                                 jxl::ImageBundle(&options->enc->metadata.m)});

  if (!queued_frame) {
    return JXL_ENC_ERROR;
  }

  jxl::ColorEncoding c_current;
  if (!options->enc->color_encoding_set) {
    if ((pixel_format->data_type == JXL_TYPE_FLOAT) ||
        (pixel_format->data_type == JXL_TYPE_FLOAT16)) {
      c_current =
          jxl::ColorEncoding::LinearSRGB(pixel_format->num_channels < 3);
    } else {
      c_current = jxl::ColorEncoding::SRGB(pixel_format->num_channels < 3);
    }
  } else {
    c_current = options->enc->metadata.m.color_encoding;
  }

  size_t xsize = options->enc->metadata.xsize();
  size_t ysize = options->enc->metadata.ysize();
  if (options->values.cparams.already_downsampled) {
    size_t factor = options->values.cparams.resampling;
    xsize = jxl::DivCeil(xsize, factor);
    ysize = jxl::DivCeil(ysize, factor);
  }

  if (!jxl::BufferToImageBundle(*pixel_format, xsize, ysize, buffer, size,
                                options->enc->thread_pool.get(), c_current,
                                &(queued_frame->frame))) {
    return JXL_ENC_ERROR;
  }

  QueueFrame(options, queued_frame);
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
      memcpy(static_cast<void*>(*next_out), enc->output_byte_queue.data(),
             to_copy);
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
