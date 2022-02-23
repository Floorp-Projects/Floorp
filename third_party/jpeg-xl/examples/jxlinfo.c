// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This example prints information from the main codestream header.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jxl/decode.h"

int PrintBasicInfo(FILE* file) {
  uint8_t* data = NULL;
  size_t data_size = 0;
  // In how large chunks to read from the file and try decoding the basic info.
  const size_t chunk_size = 64;

  JxlDecoder* dec = JxlDecoderCreate(NULL);
  if (!dec) {
    fprintf(stderr, "JxlDecoderCreate failed\n");
    return 0;
  }

  JxlDecoderSetKeepOrientation(dec, 1);

  if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(
                             dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING |
                                      JXL_DEC_FRAME | JXL_DEC_BOX)) {
    fprintf(stderr, "JxlDecoderSubscribeEvents failed\n");
    JxlDecoderDestroy(dec);
    return 0;
  }

  JxlBasicInfo info;
  int seen_basic_info = 0;
  JxlFrameHeader frame_header;

  for (;;) {
    // The first time, this will output JXL_DEC_NEED_MORE_INPUT because no
    // input is set yet, this is ok since the input is set when handling this
    // event.
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);

    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "Decoder error\n");
      break;
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      // The first time there is nothing to release and it returns 0, but that
      // is ok.
      size_t remaining = JxlDecoderReleaseInput(dec);
      // move any remaining bytes to the front if necessary
      if (remaining != 0) {
        memmove(data, data + data_size - remaining, remaining);
      }
      // resize the buffer to append one more chunk of data
      // TODO(lode): avoid unnecessary reallocations
      data = (uint8_t*)realloc(data, remaining + chunk_size);
      // append bytes read from the file behind the remaining bytes
      size_t read_size = fread(data + remaining, 1, chunk_size, file);
      if (read_size == 0 && feof(file)) {
        fprintf(stderr, "Unexpected EOF\n");
        break;
      }
      data_size = remaining + read_size;
      JxlDecoderSetInput(dec, data, data_size);
      if (feof(file)) JxlDecoderCloseInput(dec);
    } else if (status == JXL_DEC_SUCCESS) {
      // Finished all processing.
      break;
    } else if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info)) {
        fprintf(stderr, "JxlDecoderGetBasicInfo failed\n");
        break;
      }

      seen_basic_info = 1;

      printf("dimensions: %ux%u\n", info.xsize, info.ysize);
      printf("have_container: %d\n", info.have_container);
      printf("uses_original_profile: %d\n", info.uses_original_profile);
      printf("bits_per_sample: %d\n", info.bits_per_sample);
      if (info.exponent_bits_per_sample)
        printf("float, with exponent_bits_per_sample: %d\n",
               info.exponent_bits_per_sample);
      if (info.intensity_target != 255.f || info.min_nits != 0.f ||
          info.relative_to_max_display != 0 ||
          info.relative_to_max_display != 0.f) {
        printf("intensity_target: %f\n", info.intensity_target);
        printf("min_nits: %f\n", info.min_nits);
        printf("relative_to_max_display: %d\n", info.relative_to_max_display);
        printf("linear_below: %f\n", info.linear_below);
      }
      printf("have_preview: %d\n", info.have_preview);
      if (info.have_preview) {
        printf("preview xsize: %u\n", info.preview.xsize);
        printf("preview ysize: %u\n", info.preview.ysize);
      }
      printf("have_animation: %d\n", info.have_animation);
      if (info.have_animation) {
        printf("ticks per second (numerator / denominator): %u / %u\n",
               info.animation.tps_numerator, info.animation.tps_denominator);
        printf("num_loops: %u\n", info.animation.num_loops);
        printf("have_timecodes: %d\n", info.animation.have_timecodes);
      }
      const char* const orientation_string[8] = {
          "Normal",          "Flipped horizontally",
          "Upside down",     "Flipped vertically",
          "Transposed",      "90 degrees clockwise",
          "Anti-Transposed", "90 degrees counter-clockwise"};
      if (info.orientation > 0 && info.orientation < 9) {
        printf("orientation: %d (%s)\n", info.orientation,
               orientation_string[info.orientation - 1]);
      } else {
        fprintf(stderr, "Invalid orientation\n");
      }
      printf("num_color_channels: %d\n", info.num_color_channels);
      printf("num_extra_channels: %d\n", info.num_extra_channels);

      const char* const ec_type_names[7] = {"Alpha",       "Depth",
                                            "Spot color",  "Selection mask",
                                            "K (of CMYK)", "CFA (Bayer data)",
                                            "Thermal"};
      for (uint32_t i = 0; i < info.num_extra_channels; i++) {
        JxlExtraChannelInfo extra;
        if (JXL_DEC_SUCCESS != JxlDecoderGetExtraChannelInfo(dec, i, &extra)) {
          fprintf(stderr, "JxlDecoderGetExtraChannelInfo failed\n");
          break;
        }
        printf("extra channel %u:\n", i);
        printf("  type: %s\n",
               (extra.type < 7 ? ec_type_names[extra.type]
                               : (extra.type == JXL_CHANNEL_OPTIONAL
                                      ? "Unknown but can be ignored"
                                      : "Unknown, please update your libjxl")));
        printf("  bits_per_sample: %u\n", extra.bits_per_sample);
        if (extra.exponent_bits_per_sample > 0) {
          printf("  float, with exponent_bits_per_sample: %u\n",
                 extra.exponent_bits_per_sample);
        }
        if (extra.dim_shift > 0) {
          printf("  dim_shift: %u (upsampled %ux)\n", extra.dim_shift,
                 1 << extra.dim_shift);
        }
        if (extra.name_length) {
          char* name = malloc(extra.name_length + 1);
          if (JXL_DEC_SUCCESS != JxlDecoderGetExtraChannelName(
                                     dec, i, name, extra.name_length + 1)) {
            fprintf(stderr, "JxlDecoderGetExtraChannelName failed\n");
            free(name);
            break;
          }
          free(name);
          printf("  name: %s\n", name);
        }
        if (extra.type == JXL_CHANNEL_ALPHA)
          printf("  alpha_premultiplied: %d (%s)\n", extra.alpha_premultiplied,
                 extra.alpha_premultiplied ? "Premultiplied"
                                           : "Non-premultiplied");
        if (extra.type == JXL_CHANNEL_SPOT_COLOR) {
          printf("  spot_color: (%f, %f, %f) with opacity %f\n",
                 extra.spot_color[0], extra.spot_color[1], extra.spot_color[2],
                 extra.spot_color[3]);
        }
        if (extra.type == JXL_CHANNEL_CFA)
          printf("  cfa_channel: %u\n", extra.cfa_channel);
      }
    } else if (status == JXL_DEC_COLOR_ENCODING) {
      JxlPixelFormat format = {4, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
      printf("color profile:\n");

      JxlColorEncoding color_encoding;
      if (JXL_DEC_SUCCESS ==
          JxlDecoderGetColorAsEncodedProfile(dec, &format,
                                             JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                             &color_encoding)) {
        printf("  format: JPEG XL encoded color profile\n");
        const char* const cs_string[4] = {"RGB color", "Grayscale", "XYB",
                                          "Unknown"};
        const char* const wp_string[12] = {"", "D65", "Custom", "", "",  "",
                                           "", "",    "",       "", "E", "P3"};
        const char* const pr_string[12] = {
            "", "sRGB", "Custom", "", "", "", "", "", "", "Rec.2100", "", "P3"};
        const char* const tf_string[19] = {
            "", "709", "Unknown", "",     "", "", "",   "",    "Linear", "",
            "", "",    "",        "sRGB", "", "", "PQ", "DCI", "HLG"};
        const char* const ri_string[4] = {"Perceptual", "Relative",
                                          "Saturation", "Absolute"};
        printf("  color_space: %d (%s)\n", color_encoding.color_space,
               cs_string[color_encoding.color_space]);
        printf("  white_point: %d (%s)\n", color_encoding.white_point,
               wp_string[color_encoding.white_point]);
        if (color_encoding.white_point == JXL_WHITE_POINT_CUSTOM) {
          printf("  white_point XY: %f %f\n", color_encoding.white_point_xy[0],
                 color_encoding.white_point_xy[1]);
        }
        if (color_encoding.color_space == JXL_COLOR_SPACE_RGB ||
            color_encoding.color_space == JXL_COLOR_SPACE_UNKNOWN) {
          printf("  primaries: %d (%s)\n", color_encoding.primaries,
                 pr_string[color_encoding.primaries]);
          if (color_encoding.primaries == JXL_PRIMARIES_CUSTOM) {
            printf("  red primaries XY: %f %f\n",
                   color_encoding.primaries_red_xy[0],
                   color_encoding.primaries_red_xy[1]);
            printf("  green primaries XY: %f %f\n",
                   color_encoding.primaries_green_xy[0],
                   color_encoding.primaries_green_xy[1]);
            printf("  blue primaries XY: %f %f\n",
                   color_encoding.primaries_blue_xy[0],
                   color_encoding.primaries_blue_xy[1]);
          }
        }
        if (color_encoding.transfer_function == JXL_TRANSFER_FUNCTION_GAMMA) {
          printf("  transfer_function: gamma: %f\n", color_encoding.gamma);
        } else {
          printf("  transfer_function: %d (%s)\n",
                 color_encoding.transfer_function,
                 tf_string[color_encoding.transfer_function]);
        }
        printf("  rendering_intent: %d (%s)\n", color_encoding.rendering_intent,
               ri_string[color_encoding.rendering_intent]);

      } else {
        // The profile is not in JPEG XL encoded form, get as ICC profile
        // instead.
        printf("  format: ICC profile\n");
        size_t profile_size;
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetICCProfileSize(dec, &format,
                                        JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                        &profile_size)) {
          fprintf(stderr, "JxlDecoderGetICCProfileSize failed\n");
          continue;
        }
        printf("  ICC profile size: %" PRIu64 "\n", (uint64_t)profile_size);
        if (profile_size < 132) {
          fprintf(stderr, "ICC profile too small\n");
          continue;
        }
        uint8_t* profile = (uint8_t*)malloc(profile_size);
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetColorAsICCProfile(dec, &format,
                                           JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                           profile, profile_size)) {
          fprintf(stderr, "JxlDecoderGetColorAsICCProfile failed\n");
          free(profile);
          continue;
        }
        printf("  CMM type: \"%.4s\"\n", profile + 4);
        printf("  color space: \"%.4s\"\n", profile + 16);
        printf("  rendering intent: %d\n", (int)profile[67]);
        free(profile);
      }
    } else if (status == JXL_DEC_FRAME) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetFrameHeader(dec, &frame_header)) {
        fprintf(stderr, "JxlDecoderGetFrameHeader failed\n");
        break;
      }
      printf("frame:\n");
      if (frame_header.name_length) {
        char* name = malloc(frame_header.name_length + 1);
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetFrameName(dec, name, frame_header.name_length + 1)) {
          fprintf(stderr, "JxlDecoderGetFrameName failed\n");
          free(name);
          break;
        }
        free(name);
        printf("  name: %s\n", name);
      }
      float ms = frame_header.duration * 1000.f *
                 info.animation.tps_denominator / info.animation.tps_numerator;
      if (info.have_animation) {
        printf("  duration: %u ticks (%f ms)\n", frame_header.duration, ms);
        if (info.animation.have_timecodes) {
          printf("  time code: %X\n", frame_header.timecode);
        }
      }
      if (!frame_header.name_length && !info.have_animation) {
        printf("  still frame, unnamed\n");
      }
    } else if (status == JXL_DEC_BOX) {
      JxlBoxType type;
      uint64_t size;
      JxlDecoderGetBoxType(dec, type, JXL_FALSE);
      JxlDecoderGetBoxSizeRaw(dec, &size);
      printf("box: type: \"%c%c%c%c\" size: %" PRIu64 "\n", type[0], type[1],
             type[2], type[3], (uint64_t)size);
    } else {
      fprintf(stderr, "Unexpected decoder status\n");
      break;
    }
  }

  JxlDecoderDestroy(dec);
  free(data);

  return seen_basic_info;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr,
            "Usage: %s <jxl>\n"
            "Where:\n"
            "  jxl = input JPEG XL image filename\n",
            argv[0]);
    return 1;
  }

  const char* jxl_filename = argv[1];

  FILE* file = fopen(jxl_filename, "rb");
  if (!file) {
    fprintf(stderr, "Failed to read file %s\n", jxl_filename);
    return 1;
  }

  if (!PrintBasicInfo(file)) {
    fclose(file);
    fprintf(stderr, "Couldn't print basic info\n");
    return 1;
  }

  fclose(file);
  return 0;
}
