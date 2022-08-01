// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_MODULAR_TRANSFORM_PALETTE_H_
#define LIB_JXL_MODULAR_TRANSFORM_PALETTE_H_

#include <atomic>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/transform.h"  // CheckEqualChannels

namespace jxl {

namespace palette_internal {

static constexpr int kMaxPaletteLookupTableSize = 1 << 16;

static constexpr int kRgbChannels = 3;

// 5x5x5 color cube for the larger cube.
static constexpr int kLargeCube = 5;

// Smaller interleaved color cube to fill the holes of the larger cube.
static constexpr int kSmallCube = 4;
static constexpr int kSmallCubeBits = 2;
// kSmallCube ** 3
static constexpr int kLargeCubeOffset = kSmallCube * kSmallCube * kSmallCube;

static inline pixel_type Scale(uint64_t value, uint64_t bit_depth,
                               uint64_t denom) {
  // return (value * ((static_cast<pixel_type_w>(1) << bit_depth) - 1)) / denom;
  // We only call this function with kSmallCube or kLargeCube - 1 as denom,
  // allowing us to avoid a division here.
  JXL_ASSERT(denom == 4);
  return (value * ((static_cast<uint64_t>(1) << bit_depth) - 1)) >> 2;
}

// The purpose of this function is solely to extend the interpretation of
// palette indices to implicit values. If index < nb_deltas, indicating that the
// result is a delta palette entry, it is the responsibility of the caller to
// treat it as such.
static pixel_type GetPaletteValue(const pixel_type *const palette, int index,
                                  const size_t c, const int palette_size,
                                  const int onerow, const int bit_depth) {
  if (index < 0) {
    static constexpr std::array<std::array<pixel_type, 3>, 72> kDeltaPalette = {
        {
            {{0, 0, 0}},       {{4, 4, 4}},       {{11, 0, 0}},
            {{0, 0, -13}},     {{0, -12, 0}},     {{-10, -10, -10}},
            {{-18, -18, -18}}, {{-27, -27, -27}}, {{-18, -18, 0}},
            {{0, 0, -32}},     {{-32, 0, 0}},     {{-37, -37, -37}},
            {{0, -32, -32}},   {{24, 24, 45}},    {{50, 50, 50}},
            {{-45, -24, -24}}, {{-24, -45, -45}}, {{0, -24, -24}},
            {{-34, -34, 0}},   {{-24, 0, -24}},   {{-45, -45, -24}},
            {{64, 64, 64}},    {{-32, 0, -32}},   {{0, -32, 0}},
            {{-32, 0, 32}},    {{-24, -45, -24}}, {{45, 24, 45}},
            {{24, -24, -45}},  {{-45, -24, 24}},  {{80, 80, 80}},
            {{64, 0, 0}},      {{0, 0, -64}},     {{0, -64, -64}},
            {{-24, -24, 45}},  {{96, 96, 96}},    {{64, 64, 0}},
            {{45, -24, -24}},  {{34, -34, 0}},    {{112, 112, 112}},
            {{24, -45, -45}},  {{45, 45, -24}},   {{0, -32, 32}},
            {{24, -24, 45}},   {{0, 96, 96}},     {{45, -24, 24}},
            {{24, -45, -24}},  {{-24, -45, 24}},  {{0, -64, 0}},
            {{96, 0, 0}},      {{128, 128, 128}}, {{64, 0, 64}},
            {{144, 144, 144}}, {{96, 96, 0}},     {{-36, -36, 36}},
            {{45, -24, -45}},  {{45, -45, -24}},  {{0, 0, -96}},
            {{0, 128, 128}},   {{0, 96, 0}},      {{45, 24, -45}},
            {{-128, 0, 0}},    {{24, -45, 24}},   {{-45, 24, -45}},
            {{64, 0, -64}},    {{64, -64, -64}},  {{96, 0, 96}},
            {{45, -45, 24}},   {{24, 45, -45}},   {{64, 64, -64}},
            {{128, 128, 0}},   {{0, 0, -128}},    {{-24, 45, -45}},
        }};
    if (c >= kRgbChannels) {
      return 0;
    }
    // Do not open the brackets, otherwise INT32_MIN negation could overflow.
    index = -(index + 1);
    index %= 1 + 2 * (kDeltaPalette.size() - 1);
    static constexpr int kMultiplier[] = {-1, 1};
    pixel_type result =
        kDeltaPalette[((index + 1) >> 1)][c] * kMultiplier[index & 1];
    if (bit_depth > 8) {
      result *= static_cast<pixel_type>(1) << (bit_depth - 8);
    }
    return result;
  } else if (palette_size <= index && index < palette_size + kLargeCubeOffset) {
    if (c >= kRgbChannels) return 0;
    index -= palette_size;
    index >>= c * kSmallCubeBits;
    return Scale(index % kSmallCube, bit_depth, kSmallCube) +
           (1 << (std::max(0, bit_depth - 3)));
  } else if (palette_size + kLargeCubeOffset <= index) {
    if (c >= kRgbChannels) return 0;
    index -= palette_size + kLargeCubeOffset;
    // TODO(eustas): should we take care of ambiguity created by
    //               index >= kLargeCube ** 3 ?
    switch (c) {
      case 0:
        break;
      case 1:
        index /= kLargeCube;
        break;
      case 2:
        index /= kLargeCube * kLargeCube;
        break;
    }
    return Scale(index % kLargeCube, bit_depth, kLargeCube - 1);
  }
  return palette[c * onerow + static_cast<size_t>(index)];
}

}  // namespace palette_internal

static Status InvPalette(Image &input, uint32_t begin_c, uint32_t nb_colors,
                         uint32_t nb_deltas, Predictor predictor,
                         const weighted::Header &wp_header, ThreadPool *pool) {
  if (input.nb_meta_channels < 1) {
    return JXL_FAILURE("Error: Palette transform without palette.");
  }
  std::atomic<int> num_errors{0};
  int nb = input.channel[0].h;
  uint32_t c0 = begin_c + 1;
  if (c0 >= input.channel.size()) {
    return JXL_FAILURE("Channel is out of range.");
  }
  size_t w = input.channel[c0].w;
  size_t h = input.channel[c0].h;
  if (nb < 1) return JXL_FAILURE("Corrupted transforms");
  for (int i = 1; i < nb; i++) {
    input.channel.insert(
        input.channel.begin() + c0 + 1,
        Channel(w, h, input.channel[c0].hshift, input.channel[c0].vshift));
  }
  const Channel &palette = input.channel[0];
  const pixel_type *JXL_RESTRICT p_palette = input.channel[0].Row(0);
  intptr_t onerow = input.channel[0].plane.PixelsPerRow();
  intptr_t onerow_image = input.channel[c0].plane.PixelsPerRow();
  const int bit_depth = std::min(input.bitdepth, 24);

  if (w == 0) {
    // Nothing to do.
    // Avoid touching "empty" channels with non-zero height.
  } else if (nb_deltas == 0 && predictor == Predictor::Zero) {
    if (nb == 1) {
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, 0, h, ThreadPool::NoInit,
          [&](const uint32_t task, size_t /* thread */) {
            const size_t y = task;
            pixel_type *p = input.channel[c0].Row(y);
            for (size_t x = 0; x < w; x++) {
              const int index = Clamp1<int>(p[x], 0, (pixel_type)palette.w - 1);
              p[x] = palette_internal::GetPaletteValue(
                  p_palette, index, /*c=*/0,
                  /*palette_size=*/palette.w,
                  /*onerow=*/onerow, /*bit_depth=*/bit_depth);
            }
          },
          "UndoChannelPalette"));
    } else {
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, 0, h, ThreadPool::NoInit,
          [&](const uint32_t task, size_t /* thread */) {
            const size_t y = task;
            std::vector<pixel_type *> p_out(nb);
            const pixel_type *p_index = input.channel[c0].Row(y);
            for (int c = 0; c < nb; c++)
              p_out[c] = input.channel[c0 + c].Row(y);
            for (size_t x = 0; x < w; x++) {
              const int index = p_index[x];
              for (int c = 0; c < nb; c++) {
                p_out[c][x] = palette_internal::GetPaletteValue(
                    p_palette, index, /*c=*/c,
                    /*palette_size=*/palette.w,
                    /*onerow=*/onerow, /*bit_depth=*/bit_depth);
              }
            }
          },
          "UndoPalette"));
    }
  } else {
    // Parallelized per channel.
    ImageI indices = CopyImage(input.channel[c0].plane);
    if (predictor == Predictor::Weighted) {
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, 0, nb, ThreadPool::NoInit,
          [&](const uint32_t c, size_t /* thread */) {
            Channel &channel = input.channel[c0 + c];
            weighted::State wp_state(wp_header, channel.w, channel.h);
            for (size_t y = 0; y < channel.h; y++) {
              pixel_type *JXL_RESTRICT p = channel.Row(y);
              const pixel_type *JXL_RESTRICT idx = indices.Row(y);
              for (size_t x = 0; x < channel.w; x++) {
                int index = idx[x];
                pixel_type_w val = 0;
                const pixel_type palette_entry =
                    palette_internal::GetPaletteValue(
                        p_palette, index, /*c=*/c,
                        /*palette_size=*/palette.w, /*onerow=*/onerow,
                        /*bit_depth=*/bit_depth);
                if (index < static_cast<int32_t>(nb_deltas)) {
                  PredictionResult pred =
                      PredictNoTreeWP(channel.w, p + x, onerow_image, x, y,
                                      predictor, &wp_state);
                  val = pred.guess + palette_entry;
                } else {
                  val = palette_entry;
                }
                p[x] = val;
                wp_state.UpdateErrors(p[x], x, y, channel.w);
              }
            }
          },
          "UndoDeltaPaletteWP"));
    } else {
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, 0, nb, ThreadPool::NoInit,
          [&](const uint32_t c, size_t /* thread */) {
            Channel &channel = input.channel[c0 + c];
            for (size_t y = 0; y < channel.h; y++) {
              pixel_type *JXL_RESTRICT p = channel.Row(y);
              const pixel_type *JXL_RESTRICT idx = indices.Row(y);
              for (size_t x = 0; x < channel.w; x++) {
                int index = idx[x];
                pixel_type_w val = 0;
                const pixel_type palette_entry =
                    palette_internal::GetPaletteValue(
                        p_palette, index, /*c=*/c,
                        /*palette_size=*/palette.w,
                        /*onerow=*/onerow, /*bit_depth=*/bit_depth);
                if (index < static_cast<int32_t>(nb_deltas)) {
                  PredictionResult pred = PredictNoTreeNoWP(
                      channel.w, p + x, onerow_image, x, y, predictor);
                  val = pred.guess + palette_entry;
                } else {
                  val = palette_entry;
                }
                p[x] = val;
              }
            }
          },
          "UndoDeltaPaletteNoWP"));
    }
  }
  if (c0 >= input.nb_meta_channels) {
    // Palette was done on normal channels
    input.nb_meta_channels--;
  } else {
    // Palette was done on metachannels
    JXL_ASSERT(static_cast<int>(input.nb_meta_channels) >= 2 - nb);
    input.nb_meta_channels -= 2 - nb;
    JXL_ASSERT(begin_c + nb - 1 < input.nb_meta_channels);
  }
  input.channel.erase(input.channel.begin(), input.channel.begin() + 1);
  return num_errors.load(std::memory_order_relaxed) == 0;
}

static Status MetaPalette(Image &input, uint32_t begin_c, uint32_t end_c,
                          uint32_t nb_colors, uint32_t nb_deltas, bool lossy) {
  JXL_RETURN_IF_ERROR(CheckEqualChannels(input, begin_c, end_c));

  size_t nb = end_c - begin_c + 1;
  if (begin_c >= input.nb_meta_channels) {
    // Palette was done on normal channels
    input.nb_meta_channels++;
  } else {
    // Palette was done on metachannels
    JXL_ASSERT(end_c < input.nb_meta_channels);
    // we remove nb-1 metachannels and add one
    input.nb_meta_channels += 2 - nb;
  }
  input.channel.erase(input.channel.begin() + begin_c + 1,
                      input.channel.begin() + end_c + 1);
  Channel pch(nb_colors + nb_deltas, nb);
  pch.hshift = -1;
  input.channel.insert(input.channel.begin(), std::move(pch));
  return true;
}

}  // namespace jxl

#endif  // LIB_JXL_MODULAR_TRANSFORM_PALETTE_H_
