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

#include "lib/jxl/enc_external_image.h"

#include <string.h>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include <vector>

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"

namespace jxl {
namespace {

// Loads a float in big endian
float LoadBEFloat(const uint8_t* p) {
  float value;
  const uint32_t u = LoadBE32(p);
  memcpy(&value, &u, 4);
  return value;
}

// Loads a float in little endian
float LoadLEFloat(const uint8_t* p) {
  float value;
  const uint32_t u = LoadLE32(p);
  memcpy(&value, &u, 4);
  return value;
}

typedef uint32_t(LoadFuncType)(const uint8_t* p);
template <LoadFuncType LoadFunc>
void JXL_INLINE LoadFloatRow(float* JXL_RESTRICT row_out, const uint8_t* in,
                             float mul, size_t xsize, size_t bytes_per_pixel) {
  size_t i = 0;
  for (size_t x = 0; x < xsize; ++x) {
    row_out[x] = mul * LoadFunc(in + i);
    i += bytes_per_pixel;
  }
}

uint32_t JXL_INLINE Load8(const uint8_t* p) { return *p; }

}  // namespace

Status ConvertFromExternal(Span<const uint8_t> bytes, size_t xsize,
                           size_t ysize, const ColorEncoding& c_current,
                           bool has_alpha, bool alpha_is_premultiplied,
                           size_t bits_per_sample, JxlEndianness endianness,
                           bool flipped_y, ThreadPool* pool, ImageBundle* ib) {
  if (bits_per_sample < 1 || bits_per_sample > 32) {
    return JXL_FAILURE("Invalid bits_per_sample value.");
  }
  // TODO(deymo): Implement 1-bit per sample as 8 samples per byte. In
  // any other case we use DivCeil(bits_per_sample, 8) bytes per pixel per
  // channel.
  if (bits_per_sample == 1) {
    return JXL_FAILURE("packed 1-bit per sample is not yet supported");
  }

  const size_t color_channels = c_current.Channels();
  const size_t channels = color_channels + has_alpha;

  // bytes_per_channel and bytes_per_pixel are only valid for
  // bits_per_sample > 1.
  const size_t bytes_per_channel = DivCeil(bits_per_sample, jxl::kBitsPerByte);
  const size_t bytes_per_pixel = channels * bytes_per_channel;

  const size_t row_size = xsize * bytes_per_pixel;
  if (ysize && bytes.size() / ysize < row_size) {
    return JXL_FAILURE("Buffer size is too small");
  }

  const bool little_endian =
      endianness == JXL_LITTLE_ENDIAN ||
      (endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());

  const uint8_t* const in = bytes.data();

  Image3F color(xsize, ysize);
  ImageF alpha;
  if (has_alpha) {
    alpha = ImageF(xsize, ysize);
  }

  // Matches the old behavior of PackedImage.
  // TODO(sboukortt): make this a parameter.
  const bool float_in = bits_per_sample == 32;

  const auto get_y = [flipped_y, ysize](const size_t y) {
    return flipped_y ? ysize - 1 - y : y;
  };

  if (float_in) {
    if (bits_per_sample != 32) {
      return JXL_FAILURE("non-32-bit float not supported");
    }
    for (size_t c = 0; c < color_channels; ++c) {
      RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
          [&](const int task, int /*thread*/) {
            const size_t y = get_y(task);
            size_t i =
                row_size * task + (c * bits_per_sample / jxl::kBitsPerByte);
            float* JXL_RESTRICT row_out = color.PlaneRow(c, y);
            if (little_endian) {
              for (size_t x = 0; x < xsize; ++x) {
                row_out[x] = LoadLEFloat(in + i);
                i += bytes_per_pixel;
              }
            } else {
              for (size_t x = 0; x < xsize; ++x) {
                row_out[x] = LoadBEFloat(in + i);
                i += bytes_per_pixel;
              }
            }
          },
          "ConvertRGBFloat");
    }
  } else {
    // Multiplier to convert from the integer range to floating point 0-1 range.
    float mul = 1. / ((1ull << bits_per_sample) - 1);
    for (size_t c = 0; c < color_channels; ++c) {
      RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
          [&](const int task, int /*thread*/) {
            const size_t y = get_y(task);
            size_t i = row_size * task + c * bytes_per_channel;
            float* JXL_RESTRICT row_out = color.PlaneRow(c, y);
            // TODO(deymo): add bits_per_sample == 1 case here. Also maybe
            // implement masking if bits_per_sample is not a multiple of 8.
            if (bits_per_sample <= 8) {
              LoadFloatRow<Load8>(row_out, in + i, mul, xsize, bytes_per_pixel);
            } else if (bits_per_sample <= 16) {
              if (little_endian) {
                LoadFloatRow<LoadLE16>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              } else {
                LoadFloatRow<LoadBE16>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              }
            } else if (bits_per_sample <= 24) {
              if (little_endian) {
                LoadFloatRow<LoadLE24>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              } else {
                LoadFloatRow<LoadBE24>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              }
            } else {
              if (little_endian) {
                LoadFloatRow<LoadLE32>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              } else {
                LoadFloatRow<LoadBE32>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              }
            }
          },
          "ConvertRGBUint");
    }
  }

  if (color_channels == 1) {
    CopyImageTo(color.Plane(0), &color.Plane(1));
    CopyImageTo(color.Plane(0), &color.Plane(2));
  }

  ib->SetFromImage(std::move(color), c_current);

  if (has_alpha) {
    if (float_in) {
      if (bits_per_sample != 32) {
        return JXL_FAILURE("non-32-bit float not supported");
      }
      RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
          [&](const int task, int /*thread*/) {
            const size_t y = get_y(task);
            size_t i = row_size * task +
                       (color_channels * bits_per_sample / jxl::kBitsPerByte);
            float* JXL_RESTRICT row_out = alpha.Row(y);
            if (little_endian) {
              for (size_t x = 0; x < xsize; ++x) {
                row_out[x] = LoadLEFloat(in + i);
                i += bytes_per_pixel;
              }
            } else {
              for (size_t x = 0; x < xsize; ++x) {
                row_out[x] = LoadBEFloat(in + i);
                i += bytes_per_pixel;
              }
            }
          },
          "ConvertAlphaFloat");
    } else {
      float mul = 1. / ((1ull << bits_per_sample) - 1);
      RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
          [&](const int task, int /*thread*/) {
            const size_t y = get_y(task);
            size_t i = row_size * task + color_channels * bytes_per_channel;
            float* JXL_RESTRICT row_out = alpha.Row(y);
            // TODO(deymo): add bits_per_sample == 1 case here. Also maybe
            // implement masking if bits_per_sample is not a multiple of 8.
            if (bits_per_sample <= 8) {
              LoadFloatRow<Load8>(row_out, in + i, mul, xsize, bytes_per_pixel);
            } else if (bits_per_sample <= 16) {
              if (little_endian) {
                LoadFloatRow<LoadLE16>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              } else {
                LoadFloatRow<LoadBE16>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              }
            } else if (bits_per_sample <= 24) {
              if (little_endian) {
                LoadFloatRow<LoadLE24>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              } else {
                LoadFloatRow<LoadBE24>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              }
            } else {
              if (little_endian) {
                LoadFloatRow<LoadLE32>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              } else {
                LoadFloatRow<LoadBE32>(row_out, in + i, mul, xsize,
                                       bytes_per_pixel);
              }
            }
          },
          "ConvertAlphaUint");
    }

    ib->SetAlpha(std::move(alpha), alpha_is_premultiplied);
  }

  return true;
}

Status BufferToImageBundle(const JxlPixelFormat& pixel_format, uint32_t xsize,
                           uint32_t ysize, const void* buffer, size_t size,
                           jxl::ThreadPool* pool,
                           const jxl::ColorEncoding& c_current,
                           jxl::ImageBundle* ib) {
  size_t bitdepth;

  // TODO(zond): Make this accept more than float and uint8/16.
  if (pixel_format.data_type == JXL_TYPE_FLOAT) {
    bitdepth = 32;
  } else if (pixel_format.data_type == JXL_TYPE_UINT8) {
    bitdepth = 8;
  } else if (pixel_format.data_type == JXL_TYPE_UINT16) {
    bitdepth = 16;
  } else {
    return JXL_FAILURE("unsupported bitdepth");
  }

  JXL_RETURN_IF_ERROR(ConvertFromExternal(
      jxl::Span<const uint8_t>(static_cast<uint8_t*>(const_cast<void*>(buffer)),
                               size),
      xsize, ysize, c_current,
      /*has_alpha=*/pixel_format.num_channels == 2 ||
          pixel_format.num_channels == 4,
      /*alpha_is_premultiplied=*/false, bitdepth, pixel_format.endianness,
      /*flipped_y=*/false, pool, ib));
  ib->VerifyMetadata();

  return true;
}

}  // namespace jxl
