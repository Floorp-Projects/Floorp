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

#ifndef LIB_JXL_MODULAR_TRANSFORM_PALETTE_H_
#define LIB_JXL_MODULAR_TRANSFORM_PALETTE_H_

#include <set>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/modular_image.h"

namespace jxl {

namespace palette_internal {

static constexpr int kMaxPaletteLookupTableSize = 1 << 16;

static constexpr bool kEncodeToHighQualityImplicitPalette = true;

static constexpr int kCubePow = 3;

// 5x5x5 color cube for the larger cube.
static constexpr int kLargeCube = 5;

// Smaller interleaved color cube to fill the holes of the larger cube.
static constexpr int kSmallCube = kLargeCube - 1;
// kSmallCube ** kCubePow
static constexpr int kLargeCubeOffset = kSmallCube * kSmallCube * kSmallCube;

// Inclusive.
static constexpr int kMinImplicitPaletteIndex = -(2 * 72 - 1);

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
            {0, 0, 0},       {4, 4, 4},       {11, 0, 0},      {0, 0, -13},
            {0, -12, 0},     {-10, -10, -10}, {-18, -18, -18}, {-27, -27, -27},
            {-18, -18, 0},   {0, 0, -32},     {-32, 0, 0},     {-37, -37, -37},
            {0, -32, -32},   {24, 24, 45},    {50, 50, 50},    {-45, -24, -24},
            {-24, -45, -45}, {0, -24, -24},   {-34, -34, 0},   {-24, 0, -24},
            {-45, -45, -24}, {64, 64, 64},    {-32, 0, -32},   {0, -32, 0},
            {-32, 0, 32},    {-24, -45, -24}, {45, 24, 45},    {24, -24, -45},
            {-45, -24, 24},  {80, 80, 80},    {64, 0, 0},      {0, 0, -64},
            {0, -64, -64},   {-24, -24, 45},  {96, 96, 96},    {64, 64, 0},
            {45, -24, -24},  {34, -34, 0},    {112, 112, 112}, {24, -45, -45},
            {45, 45, -24},   {0, -32, 32},    {24, -24, 45},   {0, 96, 96},
            {45, -24, 24},   {24, -45, -24},  {-24, -45, 24},  {0, -64, 0},
            {96, 0, 0},      {128, 128, 128}, {64, 0, 64},     {144, 144, 144},
            {96, 96, 0},     {-36, -36, 36},  {45, -24, -45},  {45, -45, -24},
            {0, 0, -96},     {0, 128, 128},   {0, 96, 0},      {45, 24, -45},
            {-128, 0, 0},    {24, -45, 24},   {-45, 24, -45},  {64, 0, -64},
            {64, -64, -64},  {96, 0, 96},     {45, -45, 24},   {24, 45, -45},
            {64, 64, -64},   {128, 128, 0},   {0, 0, -128},    {-24, 45, -45},
        }};
    if (c >= kDeltaPalette[0].size()) {
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
    if (c >= kCubePow) return 0;
    index -= palette_size;
    if (c > 0) {
      int divisor = kSmallCube;
      for (size_t i = 1; i < c; ++i) {
        divisor *= kSmallCube;
      }
      index /= divisor;
    }
    index %= kSmallCube;
    return (index * ((1 << bit_depth) - 1)) / kSmallCube +
           (1 << (std::max(0, bit_depth - 3)));
  } else if (palette_size + kLargeCubeOffset <= index) {
    if (c >= kCubePow) return 0;
    index -= palette_size + kLargeCubeOffset;
    // TODO(eustas): should we take care of ambiguity created by
    //               index >= kLargeCube ** 3 ?
    if (c > 0) {
      int divisor = kLargeCube;
      for (size_t i = 1; i < c; ++i) {
        divisor *= kLargeCube;
      }
      index /= divisor;
    }
    index %= kLargeCube;
    return (index * ((1 << bit_depth) - 1)) / (kLargeCube - 1);
  }

  return palette[c * onerow + static_cast<size_t>(index)];
}

// Template so that it can take vectors of pixel_type or pixel_type_w
// indifferently.
template <typename T, typename U>
float ColorDistance(const T &JXL_RESTRICT a, const U &JXL_RESTRICT b) {
  JXL_ASSERT(a.size() == b.size());
  float distance = 0;
  float ave3 = 0;
  if (a.size() >= 3) {
    ave3 = (a[0] + b[0] + a[1] + b[1] + a[2] + b[2]) * (1.21f / 3.0f);
  }
  float sum_a = 0, sum_b = 0;
  for (size_t c = 0; c < a.size(); ++c) {
    const float difference =
        static_cast<float>(a[c]) - static_cast<float>(b[c]);
    float weight = c == 0 ? 3 : c == 1 ? 5 : 2;
    if (c < 3 && (a[c] + b[c] >= ave3)) {
      const float add_w[3] = {
          1.15,
          1.15,
          1.12,
      };
      weight += add_w[c];
      if (c == 2 && ((a[2] + b[2]) < 1.22 * ave3)) {
        weight -= 0.5;
      }
    }
    distance += difference * difference * weight * weight;
    const int sum_weight = c == 0 ? 3 : c == 1 ? 5 : 1;
    sum_a += a[c] * sum_weight;
    sum_b += b[c] * sum_weight;
  }
  distance *= 4;
  float sum_difference = sum_a - sum_b;
  distance += sum_difference * sum_difference;
  return distance;
}

static int QuantizeColorToImplicitPaletteIndex(
    const std::vector<pixel_type> &color, const int palette_size,
    const int bit_depth, bool high_quality) {
  int index = 0;
  if (high_quality) {
    int multiplier = 1;
    for (size_t c = 0; c < color.size(); c++) {
      int quantized = ((kLargeCube - 1) * color[c] + (1 << (bit_depth - 1))) /
                      ((1 << bit_depth) - 1);
      JXL_ASSERT((quantized % kLargeCube) == quantized);
      index += quantized * multiplier;
      multiplier *= kLargeCube;
    }
    return index + palette_size + kLargeCubeOffset;
  } else {
    int multiplier = 1;
    for (size_t c = 0; c < color.size(); c++) {
      int value = color[c];
      value -= 1 << (std::max(0, bit_depth - 3));
      value = std::max(0, value);
      int quantized = ((kLargeCube - 1) * value + (1 << (bit_depth - 1))) /
                      ((1 << bit_depth) - 1);
      JXL_ASSERT((quantized % kLargeCube) == quantized);
      if (quantized > kSmallCube - 1) {
        quantized = kSmallCube - 1;
      }
      index += quantized * multiplier;
      multiplier *= kSmallCube;
    }
    return index + palette_size;
  }
}

}  // namespace palette_internal

namespace {
// Returns the sum of a+b. If ever over- / underflow occurs it is reflected
// in "flags".
pixel_type CautiousAdd(pixel_type a, pixel_type b, pixel_type *flags) {
  // Avoid signed integer overflow.
  pixel_type sum = static_cast<pixel_type>(static_cast<uint32_t>(a) +
                                           static_cast<uint32_t>(b));
  // We care only about the highest bit. If sign is different, addition is safe.
  // If sign is the same, result sign should be the same as of the addends.
  *flags &= (a ^ b) | (a ^ ~sum);
  return sum;
}

bool IsHealthy(pixel_type flags) { return (flags >> 31); }
}  // namespace

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
  // might be false in case of lossy
  // JXL_DASSERT(input.channel[c0].minval == 0);
  // JXL_DASSERT(input.channel[c0].maxval == palette.w-1);
  if (nb < 1) return JXL_FAILURE("Corrupted transforms");
  for (int i = 1; i < nb; i++) {
    input.channel.insert(input.channel.begin() + c0 + 1, Channel(w, h));
  }
  const Channel &palette = input.channel[0];
  const pixel_type *JXL_RESTRICT p_palette = input.channel[0].Row(0);
  intptr_t onerow = input.channel[0].plane.PixelsPerRow();
  intptr_t onerow_image = input.channel[c0].plane.PixelsPerRow();
  const int bit_depth =
      CeilLog2Nonzero(static_cast<unsigned>(input.maxval - input.minval + 1));

  if (w == 0) {
    // Nothing to do.
    // Avoid touching "empty" channels with non-zero height.
  } else if (nb_deltas == 0 && predictor == Predictor::Zero) {
    if (nb == 1) {
      RunOnPool(
          pool, 0, h, ThreadPool::SkipInit(),
          [&](const int task, const int thread) {
            const size_t y = task;
            pixel_type *p = input.channel[c0].Row(y);
            for (size_t x = 0; x < w; x++) {
              const int index = Clamp1(p[x], 0, (pixel_type)palette.w - 1);
              p[x] = palette_internal::GetPaletteValue(
                  p_palette, index, /*c=*/0,
                  /*palette_size=*/palette.w,
                  /*onerow=*/onerow, /*bit_depth=*/bit_depth);
            }
          },
          "UndoChannelPalette");
    } else {
      RunOnPool(
          pool, 0, h, ThreadPool::SkipInit(),
          [&](const int task, const int thread) {
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
          "UndoPalette");
    }
  } else {
    // Parallelized per channel.
    ImageI indices = CopyImage(input.channel[c0].plane);
    if (predictor == Predictor::Weighted) {
      RunOnPool(
          pool, 0, nb, ThreadPool::SkipInit(),
          [&](size_t c, size_t _) {
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
          "UndoDeltaPaletteWP");
    } else if (predictor == Predictor::Gradient) {
      // Gradient is the most common predictor for now. This special case gives
      // about 20% extra speed.
      RunOnPool(
          pool, 0, nb, ThreadPool::SkipInit(),
          [&](size_t c, size_t _) {
            pixel_type flags = -1;
            Channel &channel = input.channel[c0 + c];
            for (size_t y = 0; y < channel.h; y++) {
              pixel_type *JXL_RESTRICT p = channel.Row(y);
              const pixel_type *JXL_RESTRICT idx = indices.Row(y);
              for (size_t x = 0; x < channel.w; x++) {
                int index = idx[x];
                pixel_type val = 0;
                const pixel_type palette_entry =
                    palette_internal::GetPaletteValue(
                        p_palette, index, /*c=*/c,
                        /*palette_size=*/palette.w,
                        /*onerow=*/onerow, /*bit_depth=*/bit_depth);
                if (index < static_cast<int32_t>(nb_deltas)) {
                  pixel_type left =
                      x ? p[x - 1] : (y ? *(p + x - onerow_image) : 0);
                  pixel_type top = y ? *(p + x - onerow_image) : left;
                  pixel_type topleft =
                      x && y ? *(p + x - 1 - onerow_image) : left;
                  val = CautiousAdd(ClampedGradient(left, top, topleft),
                                    palette_entry, &flags);
                } else {
                  val = palette_entry;
                }
                p[x] = val;
              }
            }
            if (!IsHealthy(flags)) {
              num_errors.fetch_add(1, std::memory_order_relaxed);
            }
          },
          "UndoDeltaPaletteGradient");
    } else {
      RunOnPool(
          pool, 0, nb, ThreadPool::SkipInit(),
          [&](size_t c, size_t _) {
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
          "UndoDeltaPaletteNoWP");
    }
  }
  input.nb_channels += nb - 1;
  input.nb_meta_channels--;
  input.channel.erase(input.channel.begin(), input.channel.begin() + 1);
  return num_errors.load(std::memory_order_relaxed) == 0;
}

static Status CheckPaletteParams(const Image &image, uint32_t begin_c,
                                 uint32_t end_c) {
  uint32_t c1 = begin_c;
  uint32_t c2 = end_c;
  // The range is including c1 and c2, so c2 may not be num_channels.
  if (c1 > image.channel.size() || c2 >= image.channel.size() || c2 < c1) {
    return JXL_FAILURE("Invalid channel range");
  }
  for (size_t c = begin_c + 1; c <= end_c; c++) {
    if (image.channel[c].w != image.channel[begin_c].w ||
        image.channel[c].h != image.channel[begin_c].h) {
      return false;
    }
  }

  return true;
}

static Status MetaPalette(Image &input, uint32_t begin_c, uint32_t end_c,
                          uint32_t nb_colors, uint32_t nb_deltas, bool lossy) {
  JXL_RETURN_IF_ERROR(CheckPaletteParams(input, begin_c, end_c));

  size_t nb = end_c - begin_c + 1;
  input.nb_meta_channels++;
  if (nb < 1 || input.nb_channels < nb) {
    return JXL_FAILURE("Corrupted transforms");
  }
  input.nb_channels -= nb - 1;
  input.channel.erase(input.channel.begin() + begin_c + 1,
                      input.channel.begin() + end_c + 1);
  Channel pch(nb_colors + nb_deltas, nb);
  pch.hshift = -1;
  input.channel.insert(input.channel.begin(), std::move(pch));
  return true;
}

static Status FwdPalette(Image &input, uint32_t begin_c, uint32_t end_c,
                         uint32_t &nb_colors, bool ordered, bool lossy,
                         Predictor &predictor,
                         const weighted::Header &wp_header) {
  JXL_QUIET_RETURN_IF_ERROR(CheckPaletteParams(input, begin_c, end_c));
  uint32_t nb = end_c - begin_c + 1;

  size_t w = input.channel[begin_c].w;
  size_t h = input.channel[begin_c].h;

  Image quantized_input;
  if (lossy) {
    quantized_input = Image(w, h, input.maxval, nb);
    for (size_t c = 0; c < nb; c++) {
      CopyImageTo(input.channel[begin_c + c].plane,
                  &quantized_input.channel[c].plane);
    }
  }

  JXL_DEBUG_V(
      7, "Trying to represent channels %i-%i using at most a %i-color palette.",
      begin_c, end_c, nb_colors);
  int nb_deltas = 0;
  bool delta_used = false;
  std::set<std::vector<pixel_type>>
      candidate_palette;  // ordered lexicographically
  std::vector<std::vector<pixel_type>> candidate_palette_imageorder;
  std::vector<pixel_type> color(nb);
  std::vector<float> color_with_error(nb);
  std::vector<const pixel_type *> p_in(nb);
  for (size_t y = 0; y < h; y++) {
    for (uint32_t c = 0; c < nb; c++) {
      p_in[c] = input.channel[begin_c + c].Row(y);
    }
    for (size_t x = 0; x < w; x++) {
      if (lossy && candidate_palette.size() >= nb_colors) break;
      for (uint32_t c = 0; c < nb; c++) {
        color[c] = p_in[c][x];
      }
      const bool new_color = candidate_palette.insert(color).second;
      if (new_color) {
        candidate_palette_imageorder.push_back(color);
      }
      if (candidate_palette.size() > nb_colors) {
        return false;  // too many colors
      }
    }
  }
  nb_colors = candidate_palette.size();
  JXL_DEBUG_V(6, "Channels %i-%i can be represented using a %i-color palette.",
              begin_c, end_c, nb_colors);

  Channel pch(nb_colors, nb);
  pch.hshift = -1;
  int x = 0;
  pixel_type *JXL_RESTRICT p_palette = pch.Row(0);
  intptr_t onerow = pch.plane.PixelsPerRow();
  intptr_t onerow_image = input.channel[begin_c].plane.PixelsPerRow();
  const int bit_depth =
      CeilLog2Nonzero(static_cast<unsigned>(input.maxval - input.minval + 1));
  std::vector<pixel_type> lookup;
  int minval, maxval;
  input.channel[begin_c].compute_minmax(&minval, &maxval);
  int lookup_table_size = maxval - minval + 1;
  if (lookup_table_size > palette_internal::kMaxPaletteLookupTableSize) {
    return false;  // too large lookup table
  }
  if (nb == 1) {
    lookup.resize(lookup_table_size);
  }
  if (ordered) {
    JXL_DEBUG_V(7, "Palette of %i colors, using lexicographic order",
                nb_colors);
    for (auto pcol : candidate_palette) {
      JXL_DEBUG_V(9, "  Color %i :  ", x);
      for (size_t i = 0; i < nb; i++) {
        p_palette[i * onerow + x] = pcol[i];
      }
      if (nb == 1) lookup[pcol[0] - minval] = x;
      for (size_t i = 0; i < nb; i++) {
        JXL_DEBUG_V(9, "%i ", pcol[i]);
      }
      x++;
    }
  } else {
    JXL_DEBUG_V(7, "Palette of %i colors, using image order", nb_colors);
    for (auto pcol : candidate_palette_imageorder) {
      JXL_DEBUG_V(9, "  Color %i :  ", x);
      for (size_t i = 0; i < nb; i++) p_palette[i * onerow + x] = pcol[i];
      if (nb == 1) lookup[pcol[0] - minval] = x;
      for (size_t i = 0; i < nb; i++) JXL_DEBUG_V(9, "%i ", pcol[i]);
      x++;
    }
  }
  std::vector<weighted::State> wp_states;
  for (size_t c = 0; c < nb; c++) {
    wp_states.emplace_back(wp_header, w, h);
  }
  std::vector<pixel_type *> p_quant(nb);
  // Three rows of error for dithering: y to y + 2.
  // Each row has two pixels of padding in the ends, which is
  // beneficial for both precision and encoding speed.
  std::vector<std::vector<float>> error_row[3];
  if (lossy) {
    for (int i = 0; i < 3; ++i) {
      error_row[i].resize(nb);
      for (size_t c = 0; c < nb; ++c) {
        error_row[i][c].resize(w + 4);
      }
    }
  }
  for (size_t y = 0; y < h; y++) {
    for (size_t c = 0; c < nb; c++) {
      p_in[c] = input.channel[begin_c + c].Row(y);
      if (lossy) p_quant[c] = quantized_input.channel[c].Row(y);
    }
    pixel_type *JXL_RESTRICT p = input.channel[begin_c].Row(y);
    if (nb == 1 && !lossy) {
      for (size_t x = 0; x < w; x++) p[x] = lookup[p[x] - minval];
    } else {
      for (size_t x = 0; x < w; x++) {
        int index;
        if (!lossy) {
          for (size_t c = 0; c < nb; c++) color[c] = p_in[c][x];
          // Exact search.
          for (index = 0; static_cast<uint32_t>(index) < nb_colors; index++) {
            bool found = true;
            for (size_t c = 0; c < nb; c++) {
              if (color[c] != p_palette[c * onerow + index]) {
                found = false;
                break;
              }
            }
            if (found) break;
          }
          if (index < nb_deltas) {
            delta_used = true;
          }
        } else {
          for (size_t c = 0; c < nb; c++) {
            color_with_error[c] = p_in[c][x] + error_row[0][c][x + 2];
            color[c] = std::min(
                input.maxval, std::max<pixel_type>(
                                  input.minval, lroundf(color_with_error[c])));
          }
          float best_distance = std::numeric_limits<float>::infinity();
          int best_index = 0;
          bool best_is_delta = false;
          std::vector<pixel_type> best_val(nb, 0);
          std::vector<pixel_type> quantized_val(nb);
          std::vector<pixel_type_w> predictions(nb);
          for (size_t c = 0; c < nb; ++c) {
            predictions[c] = PredictNoTreeWP(w, p_quant[c] + x, onerow_image, x,
                                             y, predictor, &wp_states[c])
                                 .guess;
          }
          const auto TryIndex = [&](const int index) {
            for (size_t c = 0; c < nb; c++) {
              quantized_val[c] = palette_internal::GetPaletteValue(
                  p_palette, index, /*c=*/c,
                  /*palette_size=*/nb_colors,
                  /*onerow=*/onerow, /*bit_depth=*/bit_depth);
              if (index < nb_deltas) {
                quantized_val[c] += predictions[c];
              }
            }
            const float color_distance =
                32 * palette_internal::ColorDistance(color_with_error,
                                                     quantized_val);
            float index_penalty = 0;
            if (index == -1) {
              index_penalty = -124;
            } else if (index < static_cast<int>(nb_colors)) {
              index_penalty = 2 * std::abs(index);
            } else if (index < static_cast<int>(nb_colors) +
                                   palette_internal::kLargeCubeOffset) {
              index_penalty = 70;
            } else {
              index_penalty = 256;
            }
            index_penalty *= 1LL << std::max(2 * (bit_depth - 8), 0);
            const float distance = color_distance + index_penalty;
            if (distance < best_distance) {
              best_distance = distance;
              best_index = index;
              best_is_delta = index < nb_deltas;
              best_val.swap(quantized_val);
            }
          };
          for (index = palette_internal::kMinImplicitPaletteIndex;
               index < static_cast<int32_t>(nb_colors); index++) {
            TryIndex(index);
          }
          TryIndex(palette_internal::QuantizeColorToImplicitPaletteIndex(
              color, nb_colors, bit_depth,
              /*high_quality=*/false));
          if (palette_internal::kEncodeToHighQualityImplicitPalette) {
            TryIndex(palette_internal::QuantizeColorToImplicitPaletteIndex(
                color, nb_colors, bit_depth,
                /*high_quality=*/true));
          }
          index = best_index;
          delta_used |= best_is_delta;
          for (size_t c = 0; c < nb; ++c) {
            wp_states[c].UpdateErrors(best_val[c], x, y, w);
            p_quant[c][x] = best_val[c];
          }
          float len_error = 0;
          for (size_t c = 0; c < nb; ++c) {
            float local_error = color_with_error[c] - best_val[c];
            len_error += local_error * local_error;
          }
          len_error = sqrt(len_error);
          float modulate = 1.0;
          int len_limit = 38 << std::max(0, bit_depth - 8);
          if (len_error > len_limit) {
            modulate *= len_limit / len_error;
          }
          for (size_t c = 0; c < nb; ++c) {
            float local_error = (color_with_error[c] - best_val[c]);
            float total_error = 0.65 * local_error;

            // If the neighboring pixels have some error in the opposite
            // direction of total_error, cancel some or all of it out before
            // spreading among them.
            constexpr int offsets[12][2] = {{1, 2}, {0, 3}, {0, 4}, {1, 1},
                                            {1, 3}, {2, 2}, {1, 0}, {1, 4},
                                            {2, 1}, {2, 3}, {2, 0}, {2, 4}};
            float total_available = 0;
            int n = 0;
            for (int i = 0; i < 11; ++i) {
              const int row = offsets[i][0];
              const int col = offsets[i][1];
              if (std::signbit(error_row[row][c][x + col]) !=
                  std::signbit(total_error)) {
                total_available += error_row[row][c][x + col];
                n++;
              }
            }
            float weight =
                std::abs(total_error) / (std::abs(total_available) + 1e-3);
            weight = std::min(weight, 1.0f);
            for (int i = 0; i < 11; ++i) {
              const int row = offsets[i][0];
              const int col = offsets[i][1];
              if (std::signbit(error_row[row][c][x + col]) !=
                  std::signbit(total_error)) {
                total_error += weight * error_row[row][c][x + col];
                error_row[row][c][x + col] *= (1 - weight);
              }
            }
            total_error *= modulate;
            const float remaining_error = (1.0f / 14.) * total_error;
            error_row[0][c][x + 3] += 2 * remaining_error;
            error_row[0][c][x + 4] += remaining_error;
            error_row[1][c][x + 0] += remaining_error;
            for (int i = 0; i < 5; ++i) {
              error_row[1][c][x + i] += remaining_error;
              error_row[2][c][x + i] += remaining_error;
            }
          }
        }
        p[x] = index;
      }
      if (lossy) {
        for (size_t c = 0; c < nb; ++c) {
          error_row[0][c].swap(error_row[1][c]);
          error_row[1][c].swap(error_row[2][c]);
          std::fill(error_row[2][c].begin(), error_row[2][c].end(), 0.f);
        }
      }
    }
  }
  if (!delta_used) {
    predictor = Predictor::Zero;
  }
  input.nb_meta_channels++;
  if (nb < 1 || input.nb_channels < nb) {
    return JXL_FAILURE("Corrupted transforms");
  }
  input.nb_channels -= nb - 1;
  input.channel.erase(input.channel.begin() + begin_c + 1,
                      input.channel.begin() + end_c + 1);
  input.channel.insert(input.channel.begin(), std::move(pch));
  return true;
}

}  // namespace jxl

#endif  // LIB_JXL_MODULAR_TRANSFORM_PALETTE_H_
