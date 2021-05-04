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

// This example prints information from the main codestream header.

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

  if (JXL_DEC_SUCCESS !=
      JxlDecoderSubscribeEvents(dec,
                                JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING)) {
    fprintf(stderr, "JxlDecoderSubscribeEvents failed\n");
    JxlDecoderDestroy(dec);
    return 0;
  }

  JxlBasicInfo info;
  int seen_basic_info = 0;

  for (;;) {
    // The firs time, this will output JXL_DEC_NEED_MORE_INPUT because no
    // input is set yet, this is ok since the input is set when handling this
    // event.
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);

    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "Decoder error\n");
      break;
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      // The firstt time there is nothing to release and it returns 0, but that
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
      data_size = remaining + read_size;
      JxlDecoderSetInput(dec, data, data_size);
    } else if (status == JXL_DEC_SUCCESS) {
      // Finished all processing.
      break;
    } else if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info)) {
        fprintf(stderr, "JxlDecoderGetBasicInfo failed\n");
        break;
      }

      seen_basic_info = 1;

      printf("xsize: %u\n", info.xsize);
      printf("ysize:  %u\n", info.ysize);
      printf("have_container: %d\n", info.have_container);
      printf("uses_original_profile: %d\n", info.uses_original_profile);
      printf("bits_per_sample: %d\n", info.bits_per_sample);
      printf("exponent_bits_per_sample: %d\n", info.exponent_bits_per_sample);
      printf("intensity_target: %f\n", info.intensity_target);
      printf("min_nits: %f\n", info.min_nits);
      printf("relative_to_max_display: %d\n", info.relative_to_max_display);
      printf("linear_below: %f\n", info.linear_below);
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
      printf("orientation: %d\n", info.orientation);
      printf("num_extra_channels: %d\n", info.num_extra_channels);
      printf("alpha_bits: %d\n", info.alpha_bits);
      printf("alpha_exponent_bits: %d\n", info.alpha_exponent_bits);
      printf("alpha_premultiplied: %d\n", info.alpha_premultiplied);

      for (uint32_t i = 0; i < info.num_extra_channels; i++) {
        JxlExtraChannelInfo extra;
        if (JXL_DEC_SUCCESS != JxlDecoderGetExtraChannelInfo(dec, i, &extra)) {
          fprintf(stderr, "JxlDecoderGetExtraChannelInfo failed\n");
          break;
        }
        printf("extra channel: %u info:\n", i);
        printf("  type: %d\n", extra.type);
        printf("  bits_per_sample: %u\n", extra.bits_per_sample);
        printf("  exponent_bits_per_sample: %u\n",
               extra.exponent_bits_per_sample);
        printf("  dim_shift: %u\n", extra.dim_shift);
        printf("  name_length: %u\n", extra.name_length);
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
        printf("  alpha_associated: %d\n", extra.alpha_associated);
        printf("  spot_color: %f %f %f %f\n", extra.spot_color[0],
               extra.spot_color[1], extra.spot_color[2], extra.spot_color[3]);
        printf("  cfa_channel: %u\n", extra.cfa_channel);
      }
    } else if (status == JXL_DEC_COLOR_ENCODING) {
      JxlPixelFormat format = {4, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
      JxlColorProfileTarget targets[2] = {JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                          JXL_COLOR_PROFILE_TARGET_DATA};
      for (size_t i = 0; i < 2; i++) {
        JxlColorProfileTarget target = targets[i];
        if (info.uses_original_profile) {
          if (target != JXL_COLOR_PROFILE_TARGET_ORIGINAL) continue;
          printf("color profile:\n");
        } else {
          printf(target == JXL_COLOR_PROFILE_TARGET_ORIGINAL
                     ? "original color profile:\n"
                     : "data color profile:\n");
        }

        JxlColorEncoding color_encoding;
        if (JXL_DEC_SUCCESS == JxlDecoderGetColorAsEncodedProfile(
                                   dec, &format, target, &color_encoding)) {
          printf("  format: JPEG XL encoded color profile\n");
          printf("  color_space: %d\n", color_encoding.color_space);
          printf("  white_point: %d\n", color_encoding.white_point);
          printf("  white_point XY: %f %f\n", color_encoding.white_point_xy[0],
                 color_encoding.white_point_xy[1]);
          if (color_encoding.color_space == JXL_COLOR_SPACE_RGB ||
              color_encoding.color_space == JXL_COLOR_SPACE_UNKNOWN) {
            printf("  primaries: %d\n", color_encoding.primaries);
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
          printf("  transfer_function: %d\n", color_encoding.transfer_function);
          if (color_encoding.transfer_function == JXL_TRANSFER_FUNCTION_GAMMA) {
            printf("  transfer_function gamma: %f\n", color_encoding.gamma);
          }
          printf("  rendering_intent: %d\n", color_encoding.rendering_intent);
        } else {
          // The profile is not in JPEG XL encoded form, get as ICC profile
          // instead.
          printf("  format: ICC profile\n");
          size_t profile_size;
          if (JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize(
                                     dec, &format, target, &profile_size)) {
            fprintf(stderr, "JxlDecoderGetICCProfileSize failed\n");
            continue;
          }
          printf("  ICC profile size: %zu\n", profile_size);
          if (profile_size < 132) {
            fprintf(stderr, "ICC profile too small\n");
            continue;
          }
          uint8_t* profile = (uint8_t*)malloc(profile_size);
          if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(dec, &format,
                                                                target, profile,
                                                                profile_size)) {
            fprintf(stderr, "JxlDecoderGetColorAsICCProfile failed\n");
            free(profile);
            continue;
          }
          printf("  CMM type: \"%.4s\"\n", profile + 4);
          printf("  color space: \"%.4s\"\n", profile + 16);
          printf("  rendering intent: %d\n", (int)profile[67]);
          free(profile);
        }
      }
      // This is the last expected event, no need to read the rest of the file.
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
    fprintf(stderr, "Couldn't print basic info\n");
    return 1;
  }

  return 0;
}
