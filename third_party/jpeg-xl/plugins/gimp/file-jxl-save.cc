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

#include "plugins/gimp/file-jxl-save.h"

// Defined by both FUIF and glib.
#undef MAX
#undef MIN
#undef CLAMP

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/enc_file.h"
#include "plugins/gimp/common.h"

namespace jxl {

namespace {

template <bool has_alpha, size_t alpha_bits = 16>
Status ReadBuffer(const size_t xsize, const size_t ysize,
                  const std::vector<float>& pixel_data, PaddedBytes icc,
                  CodecInOut* const io) {
  constexpr float alpha_multiplier = has_alpha ? 1.f / 255 : 0.f;
  Image3F image(xsize, ysize);
  ImageF alpha;
  if (has_alpha) {
    alpha = ImageF(xsize, ysize);
  }
  const float* current_sample = pixel_data.data();
  for (size_t y = 0; y < ysize; ++y) {
    float* rows[3];
    for (size_t c = 0; c < 3; ++c) {
      rows[c] = image.PlaneRow(c, y);
    }
    float* const alpha_row = has_alpha ? alpha.Row(y) : nullptr;
    for (size_t x = 0; x < xsize; ++x) {
      for (float* const row : rows) {
        row[x] = BufferFormat<GIMP_PRECISION_FLOAT_GAMMA>::ToFloat(
            *current_sample++);
      }
      if (has_alpha) {
        alpha_row[x] = alpha_multiplier *
                       BufferFormat<GIMP_PRECISION_FLOAT_GAMMA>::ToFloat(
                           *current_sample++);
      }
    }
  }

  ColorEncoding color_encoding;
  JXL_RETURN_IF_ERROR(color_encoding.SetICC(std::move(icc)));
  io->metadata.m.color_encoding = color_encoding;
  io->SetFromImage(std::move(image), color_encoding);
  if (has_alpha) {
    io->metadata.m.SetAlphaBits(alpha_bits);
    io->Main().SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);
  }
  return true;
}

}  // namespace

Status SaveJpegXlImage(const gint32 image_id, const gint32 drawable_id,
                       const gint32 orig_image_id,
                       const gchar* const filename) {
  GimpColorProfile* profile =
      gimp_image_get_effective_color_profile(orig_image_id);
  gsize icc_size;
  const guint8* const icc_bytes =
      gimp_color_profile_get_icc_profile(profile, &icc_size);
  PaddedBytes icc;
  icc.assign(icc_bytes, icc_bytes + icc_size);
  g_clear_object(&profile);

  const Babl* format;
  Status (*read_buffer)(size_t, size_t, const std::vector<float>&, PaddedBytes,
                        CodecInOut*);
  const bool has_alpha = gimp_drawable_has_alpha(drawable_id);
  if (has_alpha) {
    format = babl_format("R'G'B'A float");
    read_buffer = &ReadBuffer</*has_alpha=*/true>;
  } else {
    format = babl_format("R'G'B' float");
    read_buffer = &ReadBuffer</*has_alpha=*/false>;
  }

  CodecInOut io;

  GeglBuffer* gegl_buffer = gimp_drawable_get_buffer(drawable_id);

  // TODO(lode): is there a way to query whether the data type if float or int
  // from gegl_buffer_get_format instead?
  GimpPrecision precision = gimp_image_get_precision(image_id);
  if (precision == GIMP_PRECISION_HALF_GAMMA) {
    io.metadata.m.bit_depth.bits_per_sample = 16;
    io.metadata.m.bit_depth.exponent_bits_per_sample = 5;
  } else if (precision == GIMP_PRECISION_FLOAT_GAMMA) {
    io.metadata.m.SetFloat32Samples();
  } else {  // unsigned integer
    // TODO(lode): handle GIMP_PRECISION_DOUBLE_GAMMA. 64-bit per channel is not
    // supported by io.metadata.m.
    const Babl* native_format = gegl_buffer_get_format(gegl_buffer);
    uint32_t bits_per_sample = 8 *
                               babl_format_get_bytes_per_pixel(native_format) /
                               babl_format_get_n_components(native_format);
    io.metadata.m.SetUintSamples(bits_per_sample);
  }

  const GeglRectangle rect = *gegl_buffer_get_extent(gegl_buffer);
  std::vector<float> pixel_data(rect.width * rect.height * (3 + has_alpha));
  gegl_buffer_get(gegl_buffer, &rect, 1., format, pixel_data.data(),
                  GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_clear_object(&gegl_buffer);

  JXL_RETURN_IF_ERROR(
      read_buffer(rect.width, rect.height, pixel_data, std::move(icc), &io));
  CompressParams params;
  PassesEncoderState encoder_state;
  PaddedBytes compressed;
  ThreadPoolInternal pool;
  params.butteraugli_distance = 1.f;
  JXL_RETURN_IF_ERROR(EncodeFile(params, &io, &encoder_state, &compressed,
                                 /*aux_out=*/nullptr, &pool));
  JXL_RETURN_IF_ERROR(WriteFile(compressed, filename));

  return true;
}

}  // namespace jxl
