// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_external_image.h"

#include <string.h>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_external_image.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/cache_aligned.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/sanitizers.h"
#include "lib/jxl/transfer_functions-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

void FloatToU32(const float* in, uint32_t* out, size_t num, float mul,
                size_t bits_per_sample) {
  // TODO(eustas): investigate 24..31 bpp cases.
  if (bits_per_sample == 32) {
    // Conversion to real 32-bit *unsigned* integers requires more intermediate
    // precision that what is given by the usual f32 -> i32 conversion
    // instructions, so we run the non-SIMD path for those.
    const uint32_t cap = (1ull << bits_per_sample) - 1;
    for (size_t x = 0; x < num; x++) {
      float v = in[x];
      if (v >= 1.0f) {
        out[x] = cap;
      } else if (v >= 0.0f) {  // Inverted condition => NaN -> 0.
        out[x] = static_cast<uint32_t>(v * mul + 0.5f);
      } else {
        out[x] = 0;
      }
    }
    return;
  }

  // General SIMD case for less than 32 bits output.
  const HWY_FULL(float) d;
  const hwy::HWY_NAMESPACE::Rebind<uint32_t, decltype(d)> du;

  // Unpoison accessing partially-uninitialized vectors with memory sanitizer.
  // This is because we run NearestInt() on the vector, which triggers msan even
  // it it safe to do so since the values are not mixed between lanes.
  const size_t num_round_up = RoundUpTo(num, Lanes(d));
  msan::UnpoisonMemory(in + num, sizeof(in[0]) * (num_round_up - num));

  const auto one = Set(d, 1.0f);
  const auto scale = Set(d, mul);
  for (size_t x = 0; x < num; x += Lanes(d)) {
    auto v = Load(d, in + x);
    // Clamp turns NaN to 'min'.
    v = Clamp(v, Zero(d), one);
    auto i = NearestInt(v * scale);
    Store(BitCast(du, i), du, out + x);
  }

  // Poison back the output.
  msan::PoisonMemory(out + num, sizeof(out[0]) * (num_round_up - num));
}

void FloatToF16(const float* in, hwy::float16_t* out, size_t num) {
  const HWY_FULL(float) d;
  const hwy::HWY_NAMESPACE::Rebind<hwy::float16_t, decltype(d)> du;

  // Unpoison accessing partially-uninitialized vectors with memory sanitizer.
  // This is because we run DemoteTo() on the vector which triggers msan.
  const size_t num_round_up = RoundUpTo(num, Lanes(d));
  msan::UnpoisonMemory(in + num, sizeof(in[0]) * (num_round_up - num));

  for (size_t x = 0; x < num; x += Lanes(d)) {
    auto v = Load(d, in + x);
    auto v16 = DemoteTo(du, v);
    Store(v16, du, out + x);
  }

  // Poison back the output.
  msan::PoisonMemory(out + num, sizeof(out[0]) * (num_round_up - num));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace jxl {
namespace {

// Stores a float in big endian
void StoreBEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreBE32(u, p);
}

// Stores a float in little endian
void StoreLEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreLE32(u, p);
}

// The orientation may not be identity.
// TODO(lode): SIMDify where possible
template <typename T>
void UndoOrientation(jxl::Orientation undo_orientation, const Plane<T>& image,
                     Plane<T>& out, jxl::ThreadPool* pool) {
  const size_t xsize = image.xsize();
  const size_t ysize = image.ysize();

  if (undo_orientation == Orientation::kFlipHorizontal) {
    out = Plane<T>(xsize, ysize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          T* JXL_RESTRICT row_out = out.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            row_out[xsize - x - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kRotate180) {
    out = Plane<T>(xsize, ysize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          T* JXL_RESTRICT row_out = out.Row(ysize - y - 1);
          for (size_t x = 0; x < xsize; ++x) {
            row_out[xsize - x - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kFlipVertical) {
    out = Plane<T>(xsize, ysize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          T* JXL_RESTRICT row_out = out.Row(ysize - y - 1);
          for (size_t x = 0; x < xsize; ++x) {
            row_out[x] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kTranspose) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(x)[y] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kRotate90) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(x)[ysize - y - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kAntiTranspose) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(xsize - x - 1)[ysize - y - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kRotate270) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(xsize - x - 1)[y] = row_in[x];
          }
        },
        "UndoOrientation");
  }
}
}  // namespace

HWY_EXPORT(FloatToU32);
HWY_EXPORT(FloatToF16);

namespace {

using StoreFuncType = void(uint32_t value, uint8_t* dest);
template <StoreFuncType StoreFunc>
void StoreUintRow(uint32_t* JXL_RESTRICT* rows_u32, size_t num_channels,
                  size_t xsize, size_t bytes_per_sample,
                  uint8_t* JXL_RESTRICT out) {
  for (size_t x = 0; x < xsize; ++x) {
    for (size_t c = 0; c < num_channels; c++) {
      StoreFunc(rows_u32[c][x],
                out + (num_channels * x + c) * bytes_per_sample);
    }
  }
}

template <void(StoreFunc)(float, uint8_t*)>
void StoreFloatRow(const float* JXL_RESTRICT* rows_in, size_t num_channels,
                   size_t xsize, uint8_t* JXL_RESTRICT out) {
  for (size_t x = 0; x < xsize; ++x) {
    for (size_t c = 0; c < num_channels; c++) {
      StoreFunc(rows_in[c][x], out + (num_channels * x + c) * sizeof(float));
    }
  }
}

void JXL_INLINE Store8(uint32_t value, uint8_t* dest) { *dest = value & 0xff; }

// Maximum number of channels for the ConvertChannelsToExternal function.
const size_t kConvertMaxChannels = 4;

// Converts a list of channels to an interleaved image, applying transformations
// when needed.
// The input channels are given as a (non-const!) array of channel pointers and
// interleaved in that order.
//
// Note: if a pointer in channels[] is nullptr, a 1.0 value will be used
// instead. This is useful for handling when a user requests an alpha channel
// from an image that doesn't have one. The first channel in the list may not
// be nullptr, since it is used to determine the image size.
Status ConvertChannelsToExternal(const ImageF* channels[], size_t num_channels,
                                 size_t bits_per_sample, bool float_out,
                                 JxlEndianness endianness, size_t stride,
                                 jxl::ThreadPool* pool, void* out_image,
                                 size_t out_size,
                                 JxlImageOutCallback out_callback,
                                 void* out_opaque,
                                 jxl::Orientation undo_orientation) {
  JXL_DASSERT(num_channels != 0 && num_channels <= kConvertMaxChannels);
  JXL_DASSERT(channels[0] != nullptr);

  if (bits_per_sample < 1 || bits_per_sample > 32) {
    return JXL_FAILURE("Invalid bits_per_sample value.");
  }
  if (!!out_image == !!out_callback) {
    return JXL_FAILURE(
        "Must provide either an out_image or an out_callback, but not both.");
  }
  // TODO(deymo): Implement 1-bit per pixel packed in 8 samples per byte.
  if (bits_per_sample == 1) {
    return JXL_FAILURE("packed 1-bit per sample is not yet supported");
  }
  if (bits_per_sample > 16 && bits_per_sample < 32) {
    return JXL_FAILURE("not supported, try bits_per_sample=32");
  }

  // bytes_per_channel and is only valid for bits_per_sample > 1.
  const size_t bytes_per_channel = DivCeil(bits_per_sample, jxl::kBitsPerByte);
  const size_t bytes_per_pixel = num_channels * bytes_per_channel;

  std::vector<std::vector<uint8_t>> row_out_callback;
  auto InitOutCallback = [&](size_t num_threads) {
    if (out_callback) {
      row_out_callback.resize(num_threads);
      for (size_t i = 0; i < num_threads; ++i) {
        row_out_callback[i].resize(stride);
      }
    }
  };

  // Channels used to store the transformed original channels if needed.
  ImageF temp_channels[kConvertMaxChannels];
  if (undo_orientation != Orientation::kIdentity) {
    for (size_t c = 0; c < num_channels; ++c) {
      if (channels[c]) {
        UndoOrientation(undo_orientation, *channels[c], temp_channels[c], pool);
        channels[c] = &(temp_channels[c]);
      }
    }
  }

  // First channel may not be nullptr.
  size_t xsize = channels[0]->xsize();
  size_t ysize = channels[0]->ysize();

  if (stride < bytes_per_pixel * xsize) {
    return JXL_FAILURE("stride is smaller than scanline width in bytes: %" PRIuS
                       " vs %" PRIuS,
                       stride, bytes_per_pixel * xsize);
  }

  const bool little_endian =
      endianness == JXL_LITTLE_ENDIAN ||
      (endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());

  // Handle the case where a channel is nullptr by creating a single row with
  // ones to use instead.
  ImageF ones;
  for (size_t c = 0; c < num_channels; ++c) {
    if (!channels[c]) {
      ones = ImageF(xsize, 1);
      FillImage(1.0f, &ones);
      break;
    }
  }

  if (float_out) {
    if (bits_per_sample == 16) {
      bool swap_endianness = little_endian != IsLittleEndian();
      Plane<hwy::float16_t> f16_cache;
      RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize),
          [&](size_t num_threads) {
            f16_cache =
                Plane<hwy::float16_t>(xsize, num_channels * num_threads);
            InitOutCallback(num_threads);
            return true;
          },
          [&](const int task, int thread) {
            const int64_t y = task;
            const float* JXL_RESTRICT row_in[kConvertMaxChannels];
            for (size_t c = 0; c < num_channels; c++) {
              row_in[c] = channels[c] ? channels[c]->Row(y) : ones.Row(0);
            }
            hwy::float16_t* JXL_RESTRICT row_f16[kConvertMaxChannels];
            for (size_t c = 0; c < num_channels; c++) {
              row_f16[c] = f16_cache.Row(c + thread * num_channels);
              HWY_DYNAMIC_DISPATCH(FloatToF16)
              (row_in[c], row_f16[c], xsize);
            }
            uint8_t* row_out =
                out_callback
                    ? row_out_callback[thread].data()
                    : &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
            // interleave the one scanline
            hwy::float16_t* row_f16_out =
                reinterpret_cast<hwy::float16_t*>(row_out);
            for (size_t x = 0; x < xsize; x++) {
              for (size_t c = 0; c < num_channels; c++) {
                row_f16_out[x * num_channels + c] = row_f16[c][x];
              }
            }
            if (swap_endianness) {
              size_t size = xsize * num_channels * 2;
              for (size_t i = 0; i < size; i += 2) {
                std::swap(row_out[i + 0], row_out[i + 1]);
              }
            }
            if (out_callback) {
              (*out_callback)(out_opaque, 0, y, xsize, row_out);
            }
          },
          "ConvertF16");
    } else if (bits_per_sample == 32) {
      RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize),
          [&](size_t num_threads) {
            InitOutCallback(num_threads);
            return true;
          },
          [&](const int task, int thread) {
            const int64_t y = task;
            uint8_t* row_out =
                out_callback
                    ? row_out_callback[thread].data()
                    : &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
            const float* JXL_RESTRICT row_in[kConvertMaxChannels];
            for (size_t c = 0; c < num_channels; c++) {
              row_in[c] = channels[c] ? channels[c]->Row(y) : ones.Row(0);
            }
            if (little_endian) {
              StoreFloatRow<StoreLEFloat>(row_in, num_channels, xsize, row_out);
            } else {
              StoreFloatRow<StoreBEFloat>(row_in, num_channels, xsize, row_out);
            }
            if (out_callback) {
              (*out_callback)(out_opaque, 0, y, xsize, row_out);
            }
          },
          "ConvertFloat");
    } else {
      return JXL_FAILURE("float other than 16-bit and 32-bit not supported");
    }
  } else {
    // Multiplier to convert from floating point 0-1 range to the integer
    // range.
    float mul = (1ull << bits_per_sample) - 1;
    Plane<uint32_t> u32_cache;
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize),
        [&](size_t num_threads) {
          u32_cache = Plane<uint32_t>(xsize, num_channels * num_threads);
          InitOutCallback(num_threads);
          return true;
        },
        [&](const int task, int thread) {
          const int64_t y = task;
          uint8_t* row_out =
              out_callback
                  ? row_out_callback[thread].data()
                  : &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
          const float* JXL_RESTRICT row_in[kConvertMaxChannels];
          for (size_t c = 0; c < num_channels; c++) {
            row_in[c] = channels[c] ? channels[c]->Row(y) : ones.Row(0);
          }
          uint32_t* JXL_RESTRICT row_u32[kConvertMaxChannels];
          for (size_t c = 0; c < num_channels; c++) {
            row_u32[c] = u32_cache.Row(c + thread * num_channels);
            // row_u32[] is a per-thread temporary row storage, this isn't
            // intended to be initialized on a previous run.
            msan::PoisonMemory(row_u32[c], xsize * sizeof(row_u32[c][0]));
            HWY_DYNAMIC_DISPATCH(FloatToU32)
            (row_in[c], row_u32[c], xsize, mul, bits_per_sample);
          }
          // TODO(deymo): add bits_per_sample == 1 case here.
          if (bits_per_sample <= 8) {
            StoreUintRow<Store8>(row_u32, num_channels, xsize, 1, row_out);
          } else if (bits_per_sample <= 16) {
            if (little_endian) {
              StoreUintRow<StoreLE16>(row_u32, num_channels, xsize, 2, row_out);
            } else {
              StoreUintRow<StoreBE16>(row_u32, num_channels, xsize, 2, row_out);
            }
          } else {
            if (little_endian) {
              StoreUintRow<StoreLE32>(row_u32, num_channels, xsize, 4, row_out);
            } else {
              StoreUintRow<StoreBE32>(row_u32, num_channels, xsize, 4, row_out);
            }
          }
          if (out_callback) {
            (*out_callback)(out_opaque, 0, y, xsize, row_out);
          }
        },
        "ConvertUint");
  }
  return true;
}

}  // namespace

Status ConvertToExternal(const jxl::ImageBundle& ib, size_t bits_per_sample,
                         bool float_out, size_t num_channels,
                         JxlEndianness endianness, size_t stride,
                         jxl::ThreadPool* pool, void* out_image,
                         size_t out_size, JxlImageOutCallback out_callback,
                         void* out_opaque, jxl::Orientation undo_orientation) {
  bool want_alpha = num_channels == 2 || num_channels == 4;
  size_t color_channels = num_channels <= 2 ? 1 : 3;

  const Image3F* color = &ib.color();
  // Undo premultiplied alpha.
  Image3F unpremul;
  if (ib.AlphaIsPremultiplied() && ib.HasAlpha()) {
    unpremul = Image3F(color->xsize(), color->ysize());
    CopyImageTo(*color, &unpremul);
    for (size_t y = 0; y < unpremul.ysize(); y++) {
      UnpremultiplyAlpha(unpremul.PlaneRow(0, y), unpremul.PlaneRow(1, y),
                         unpremul.PlaneRow(2, y), ib.alpha().Row(y),
                         unpremul.xsize());
    }
    color = &unpremul;
  }

  const ImageF* channels[kConvertMaxChannels];
  size_t c = 0;
  for (; c < color_channels; c++) {
    channels[c] = &color->Plane(c);
  }
  if (want_alpha) {
    channels[c++] = ib.HasAlpha() ? &ib.alpha() : nullptr;
  }
  JXL_ASSERT(num_channels == c);

  return ConvertChannelsToExternal(
      channels, num_channels, bits_per_sample, float_out, endianness, stride,
      pool, out_image, out_size, out_callback, out_opaque, undo_orientation);
}

Status ConvertToExternal(const jxl::ImageF& channel, size_t bits_per_sample,
                         bool float_out, JxlEndianness endianness,
                         size_t stride, jxl::ThreadPool* pool, void* out_image,
                         size_t out_size, JxlImageOutCallback out_callback,
                         void* out_opaque, jxl::Orientation undo_orientation) {
  const ImageF* channels[1];
  channels[0] = &channel;
  return ConvertChannelsToExternal(
      channels, 1, bits_per_sample, float_out, endianness, stride, pool,
      out_image, out_size, out_callback, out_opaque, undo_orientation);
}

}  // namespace jxl
#endif  // HWY_ONCE
