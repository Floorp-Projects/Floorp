// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "plugins/gimp/file-jxl-load.h"

#define _PROFILE_ORIGIN_ JXL_COLOR_PROFILE_TARGET_ORIGINAL
#define _PROFILE_TARGET_ JXL_COLOR_PROFILE_TARGET_DATA
#define LOAD_PROC "file-jxl-load"

namespace jxl {

bool LoadJpegXlImage(const gchar *const filename, gint32 *const image_id) {
  std::vector<uint8_t> icc_profile;
  GimpColorProfile *profile_icc = nullptr;
  GimpColorProfile *profile_int = nullptr;
  bool is_linear = false;

  gint32 layer;

  gpointer pixels_buffer_1 = nullptr;
  gpointer pixels_buffer_2 = nullptr;
  size_t buffer_size = 0;

  GimpImageBaseType image_type = GIMP_RGB;
  GimpImageType layer_type = GIMP_RGB_IMAGE;
  GimpPrecision precision = GIMP_PRECISION_U16_GAMMA;
  JxlBasicInfo info = {};
  JxlPixelFormat format = {};

  format.num_channels = 4;
  format.data_type = JXL_TYPE_FLOAT;
  format.endianness = JXL_NATIVE_ENDIAN;
  format.align = 0;

  bool is_gray = false;

  JpegXlGimpProgress gimp_load_progress(
      ("Opening JPEG XL file:" + std::string(filename)).c_str());
  gimp_load_progress.update();

  // read file
  std::ifstream instream(filename, std::ios::in | std::ios::binary);
  std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(instream)),
                                  std::istreambuf_iterator<char>());
  instream.close();

  gimp_load_progress.update();

  // multi-threaded parallel runner.
  auto runner = JxlResizableParallelRunnerMake(nullptr);

  auto dec = JxlDecoderMake(nullptr);
  if (JXL_DEC_SUCCESS !=
      JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO |
                                               JXL_DEC_COLOR_ENCODING |
                                               JXL_DEC_FULL_IMAGE)) {
    g_printerr(LOAD_PROC " Error: JxlDecoderSubscribeEvents failed\n");
    return false;
  }

  if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(),
                                                     JxlResizableParallelRunner,
                                                     runner.get())) {
    g_printerr(LOAD_PROC " Error: JxlDecoderSetParallelRunner failed\n");
    return false;
  }

  // grand decode loop...
  JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());

  while (true) {
    gimp_load_progress.update();

    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

    if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
        g_printerr(LOAD_PROC " Error: JxlDecoderGetBasicInfo failed\n");
        return false;
      }

      JxlResizableParallelRunnerSetThreads(
          runner.get(),
          JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
    } else if (status == JXL_DEC_COLOR_ENCODING) {
      // check for ICC profile
      size_t icc_size = 0;
      JxlColorEncoding color_encoding;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderGetColorAsEncodedProfile(
              dec.get(), &format, _PROFILE_ORIGIN_, &color_encoding)) {
        // Attempt to load ICC profile when no internal color encoding
        if (JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize(dec.get(), &format,
                                                           _PROFILE_ORIGIN_,
                                                           &icc_size)) {
          g_printerr(LOAD_PROC
                     " Warning: JxlDecoderGetICCProfileSize failed\n");
        }

        if (icc_size > 0) {
          icc_profile.resize(icc_size);
          if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                     dec.get(), &format, _PROFILE_ORIGIN_,
                                     icc_profile.data(), icc_profile.size())) {
            g_printerr(LOAD_PROC
                       " Warning: JxlDecoderGetColorAsICCProfile failed\n");
          }

          profile_icc = gimp_color_profile_new_from_icc_profile(
              icc_profile.data(), icc_profile.size(), nullptr);

          if (profile_icc) {
            is_linear = gimp_color_profile_is_linear(profile_icc);
            g_printerr(LOAD_PROC " Info: Color profile is_linear = %d\n",
                       is_linear);
          } else {
            g_printerr(LOAD_PROC " Warning: Failed to read ICC profile.\n");
          }
        } else {
          g_printerr(LOAD_PROC " Warning: Empty ICC data.\n");
        }
      }

      // Internal color profile detection...
      if (JXL_DEC_SUCCESS ==
          JxlDecoderGetColorAsEncodedProfile(
              dec.get(), &format, _PROFILE_TARGET_, &color_encoding)) {
        g_printerr(LOAD_PROC " Info: Internal color encoding detected.\n");

        // figure out linearity of internal profile
        switch (color_encoding.transfer_function) {
          case JXL_TRANSFER_FUNCTION_LINEAR:
            is_linear = true;
            break;

          case JXL_TRANSFER_FUNCTION_709:
          case JXL_TRANSFER_FUNCTION_PQ:
          case JXL_TRANSFER_FUNCTION_HLG:
          case JXL_TRANSFER_FUNCTION_GAMMA:
          case JXL_TRANSFER_FUNCTION_DCI:
          case JXL_TRANSFER_FUNCTION_SRGB:
            is_linear = false;
            break;

          case JXL_TRANSFER_FUNCTION_UNKNOWN:
          default:
            if (profile_icc) {
              g_printerr(LOAD_PROC
                         " Info: Unknown transfer function.  "
                         "ICC profile is present.");
            } else {
              g_printerr(LOAD_PROC
                         " Info: Unknown transfer function.  "
                         "No ICC profile present.");
            }
            break;
        }

        switch (color_encoding.color_space) {
          case JXL_COLOR_SPACE_RGB:
            if (color_encoding.white_point == JXL_WHITE_POINT_D65 &&
                color_encoding.primaries == JXL_PRIMARIES_SRGB) {
              if (is_linear) {
                profile_int = gimp_color_profile_new_rgb_srgb_linear();
              } else {
                profile_int = gimp_color_profile_new_rgb_srgb();
              }
            } else if (!is_linear &&
                       color_encoding.white_point == JXL_WHITE_POINT_D65 &&
                       (color_encoding.primaries_green_xy[0] == 0.2100 ||
                        color_encoding.primaries_green_xy[1] == 0.7100)) {
              // Probably Adobe RGB
              profile_int = gimp_color_profile_new_rgb_adobe();
            } else if (profile_icc) {
              g_printerr(LOAD_PROC
                         " Info: Unknown RGB colorspace.  "
                         "Using ICC profile.\n");
            } else {
              g_printerr(LOAD_PROC
                         " Info: Unknown RGB colorspace.  "
                         "Treating as sRGB.\n");
              if (is_linear) {
                profile_int = gimp_color_profile_new_rgb_srgb_linear();
              } else {
                profile_int = gimp_color_profile_new_rgb_srgb();
              }
            }
            break;

          case JXL_COLOR_SPACE_GRAY:
            is_gray = true;
            if (!profile_icc ||
                color_encoding.white_point == JXL_WHITE_POINT_D65) {
              if (is_linear) {
                profile_int = gimp_color_profile_new_d65_gray_linear();
              } else {
                profile_int = gimp_color_profile_new_d65_gray_srgb_trc();
              }
            }
            break;
          case JXL_COLOR_SPACE_XYB:
          case JXL_COLOR_SPACE_UNKNOWN:
          default:
            if (profile_icc) {
              g_printerr(LOAD_PROC
                         " Info: Unknown colorspace.  Using ICC profile.\n");
            } else {
              g_error(
                  LOAD_PROC
                  " Warning: Unknown colorspace. Treating as sRGB profile.\n");

              if (is_linear) {
                profile_int = gimp_color_profile_new_rgb_srgb_linear();
              } else {
                profile_int = gimp_color_profile_new_rgb_srgb();
              }
            }
            break;
        }
      }

      // set pixel format
      if (info.num_color_channels > 1) {
        if (info.alpha_bits == 0) {
          image_type = GIMP_RGB;
          layer_type = GIMP_RGB_IMAGE;
          format.num_channels = info.num_color_channels;
        } else {
          image_type = GIMP_RGB;
          layer_type = GIMP_RGBA_IMAGE;
          format.num_channels = info.num_color_channels + 1;
        }
      } else if (info.num_color_channels == 1) {
        if (info.alpha_bits == 0) {
          image_type = GIMP_GRAY;
          layer_type = GIMP_GRAY_IMAGE;
          format.num_channels = info.num_color_channels;
        } else {
          image_type = GIMP_GRAY;
          layer_type = GIMP_GRAYA_IMAGE;
          format.num_channels = info.num_color_channels + 1;
        }
      }

      // Set image bit depth and linearity
      if (info.bits_per_sample <= 8) {
        if (is_linear) {
          precision = GIMP_PRECISION_U8_LINEAR;
        } else {
          precision = GIMP_PRECISION_U8_GAMMA;
        }
      } else if (info.bits_per_sample <= 16) {
        if (info.exponent_bits_per_sample > 0) {
          if (is_linear) {
            precision = GIMP_PRECISION_HALF_LINEAR;
          } else {
            precision = GIMP_PRECISION_HALF_GAMMA;
          }
        } else if (is_linear) {
          precision = GIMP_PRECISION_U16_LINEAR;
        } else {
          precision = GIMP_PRECISION_U16_GAMMA;
        }
      } else {
        if (info.exponent_bits_per_sample > 0) {
          if (is_linear) {
            precision = GIMP_PRECISION_FLOAT_LINEAR;
          } else {
            precision = GIMP_PRECISION_FLOAT_GAMMA;
          }
        } else if (is_linear) {
          precision = GIMP_PRECISION_U32_LINEAR;
        } else {
          precision = GIMP_PRECISION_U32_GAMMA;
        }
      }

      // create new image
      if (is_linear) {
        *image_id = gimp_image_new_with_precision(
            info.xsize, info.ysize, image_type, GIMP_PRECISION_FLOAT_LINEAR);
      } else {
        *image_id = gimp_image_new_with_precision(
            info.xsize, info.ysize, image_type, GIMP_PRECISION_FLOAT_GAMMA);
      }

      if (profile_int) {
        gimp_image_set_color_profile(*image_id, profile_int);
      } else if (!profile_icc) {
        g_printerr(LOAD_PROC " Warning: No color profile.\n");
      }
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      // get image from decoder in FLOAT
      format.data_type = JXL_TYPE_FLOAT;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)) {
        g_printerr(LOAD_PROC " Error: JxlDecoderImageOutBufferSize failed\n");
        return false;
      }
      pixels_buffer_1 = g_malloc(buffer_size);
      if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format,
                                                         pixels_buffer_1,
                                                         buffer_size)) {
        g_printerr(LOAD_PROC " Error: JxlDecoderSetImageOutBuffer failed\n");
        return false;
      }
    } else if (status == JXL_DEC_FULL_IMAGE || status == JXL_DEC_FRAME) {
      // create and insert layer
      layer = gimp_layer_new(*image_id, "Background", info.xsize, info.ysize,
                             layer_type, /*opacity=*/100,
                             gimp_image_get_default_new_layer_mode(*image_id));

      gimp_image_insert_layer(*image_id, layer, /*parent_id=*/-1,
                              /*position=*/0);

      pixels_buffer_2 = g_malloc(buffer_size);
      GeglBuffer *buffer = gimp_drawable_get_buffer(layer);
      const Babl *destination_format = gegl_buffer_set_format(buffer, nullptr);

      std::string babl_format_str = "";
      if (is_gray) {
        babl_format_str += "Y'";
      } else {
        babl_format_str += "R'G'B'";
      }
      if (info.alpha_bits > 0) {
        babl_format_str += "A";
      }
      babl_format_str += " float";

      const Babl *source_format = babl_format(babl_format_str.c_str());

      babl_process(babl_fish(source_format, destination_format),
                   pixels_buffer_1, pixels_buffer_2, info.xsize * info.ysize);

      gegl_buffer_set(buffer, GEGL_RECTANGLE(0, 0, info.xsize, info.ysize), 0,
                      nullptr, pixels_buffer_2, GEGL_AUTO_ROWSTRIDE);

      g_clear_object(&buffer);
    } else if (status == JXL_DEC_SUCCESS) {
      // All decoding successfully finished.
      // It's not required to call JxlDecoderReleaseInput(dec.get())
      // since the decoder will be destroyed.
      break;
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      g_printerr(LOAD_PROC " Error: Already provided all input\n");
      return false;
    } else if (status == JXL_DEC_ERROR) {
      g_printerr(LOAD_PROC " Error: Decoder error\n");
      return false;
    } else {
      g_printerr(LOAD_PROC " Error: Unknown decoder status\n");
      return false;
    }
  }  // end grand decode loop

  gimp_load_progress.update();

  if (profile_icc) {
    gimp_image_set_color_profile(*image_id, profile_icc);
  }

  gimp_load_progress.update();

  // TODO(xiota): Add option to keep image as float
  if (info.bits_per_sample < 32) {
    gimp_image_convert_precision(*image_id, precision);
  }

  gimp_image_set_filename(*image_id, filename);

  gimp_load_progress.finished();
  return true;
}

}  // namespace jxl
