// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/modular/transform/enc_palette.h"

#include <array>
#include <map>
#include <set>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/enc_transform.h"
#include "lib/jxl/modular/transform/palette.h"

namespace jxl {

namespace palette_internal {

static constexpr bool kEncodeToHighQualityImplicitPalette = true;

// Inclusive.
static constexpr int kMinImplicitPaletteIndex = -(2 * 72 - 1);

float ColorDistance(const std::vector<float> &JXL_RESTRICT a,
                    const std::vector<pixel_type> &JXL_RESTRICT b) {
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

int RoundInt(int value, int div) {  // symmetric rounding around 0
  if (value < 0) return -RoundInt(-value, div);
  return (value + div / 2) / div;
}

struct PaletteIterationData {
  static constexpr int kMaxDeltas = 128;
  bool final_run = false;
  std::vector<pixel_type> deltas[3];
  std::vector<double> delta_distances;
  std::vector<pixel_type> frequent_deltas[3];

  // Populates `frequent_deltas` with items from `deltas` based on frequencies
  // and color distances.
  void FindFrequentColorDeltas(int num_pixels, int bitdepth) {
    using pixel_type_3d = std::array<pixel_type, 3>;
    std::map<pixel_type_3d, double> delta_frequency_map;
    pixel_type bucket_size = 3 << std::max(0, bitdepth - 8);
    // Store frequency weighted by delta distance from quantized value.
    for (size_t i = 0; i < deltas[0].size(); ++i) {
      pixel_type_3d delta = {
          {RoundInt(deltas[0][i], bucket_size),
           RoundInt(deltas[1][i], bucket_size),
           RoundInt(deltas[2][i], bucket_size)}};  // a basic form of clustering
      if (delta[0] == 0 && delta[1] == 0 && delta[2] == 0) continue;
      delta_frequency_map[delta] += sqrt(sqrt(delta_distances[i]));
    }

    const float delta_distance_multiplier = 1.0f / num_pixels;

    // Weigh frequencies by magnitude and normalize.
    for (auto &delta_frequency : delta_frequency_map) {
      std::vector<pixel_type> current_delta = {delta_frequency.first[0],
                                               delta_frequency.first[1],
                                               delta_frequency.first[2]};
      float delta_distance =
          sqrt(palette_internal::ColorDistance({0, 0, 0}, current_delta)) + 1;
      delta_frequency.second *= delta_distance * delta_distance_multiplier;
    }

    // Sort by weighted frequency.
    using pixel_type_3d_frequency = std::pair<pixel_type_3d, double>;
    std::vector<pixel_type_3d_frequency> sorted_delta_frequency_map(
        delta_frequency_map.begin(), delta_frequency_map.end());
    std::sort(
        sorted_delta_frequency_map.begin(), sorted_delta_frequency_map.end(),
        [](const pixel_type_3d_frequency &a, const pixel_type_3d_frequency &b) {
          return a.second > b.second;
        });

    // Store the top deltas.
    for (auto &delta_frequency : sorted_delta_frequency_map) {
      if (frequent_deltas[0].size() >= kMaxDeltas) break;
      // Number obtained by optimizing on jyrki31 corpus:
      if (delta_frequency.second < 17) break;
      for (int c = 0; c < 3; ++c) {
        frequent_deltas[c].push_back(delta_frequency.first[c] * bucket_size);
      }
    }
  }
};

Status FwdPaletteIteration(Image &input, uint32_t begin_c, uint32_t end_c,
                           uint32_t &nb_colors, uint32_t &nb_deltas,
                           bool ordered, bool lossy, Predictor &predictor,
                           const weighted::Header &wp_header,
                           PaletteIterationData &palette_iteration_data) {
  JXL_QUIET_RETURN_IF_ERROR(CheckEqualChannels(input, begin_c, end_c));
  JXL_ASSERT(begin_c >= input.nb_meta_channels);
  uint32_t nb = end_c - begin_c + 1;

  size_t w = input.channel[begin_c].w;
  size_t h = input.channel[begin_c].h;

  if (!lossy && nb == 1) {
    // Channel palette special case
    if (nb_colors == 0) return false;
    std::vector<pixel_type> lookup;
    pixel_type minval, maxval;
    compute_minmax(input.channel[begin_c], &minval, &maxval);
    size_t lookup_table_size =
        static_cast<int64_t>(maxval) - static_cast<int64_t>(minval) + 1;
    if (lookup_table_size > palette_internal::kMaxPaletteLookupTableSize) {
      return false;  // too large lookup table
    }
    lookup.resize(lookup_table_size, 0);
    pixel_type idx = 0;
    for (size_t y = 0; y < h; y++) {
      const pixel_type *p = input.channel[begin_c].Row(y);
      for (size_t x = 0; x < w; x++) {
        if (lookup[p[x] - minval] == 0) {
          lookup[p[x] - minval] = 1;
          idx++;
          if (idx > (int)nb_colors) return false;
        }
      }
    }
    JXL_DEBUG_V(6, "Channel %i uses only %i colors.", begin_c, idx);
    Channel pch(idx, 1);
    pch.hshift = -1;
    nb_colors = idx;
    idx = 0;
    pixel_type *JXL_RESTRICT p_palette = pch.Row(0);
    for (size_t i = 0; i < lookup_table_size; i++) {
      if (lookup[i]) {
        p_palette[idx] = i + minval;
        lookup[i] = idx;
        idx++;
      }
    }
    for (size_t y = 0; y < h; y++) {
      pixel_type *p = input.channel[begin_c].Row(y);
      for (size_t x = 0; x < w; x++) p[x] = lookup[p[x] - minval];
    }
    predictor = Predictor::Zero;
    input.nb_meta_channels++;
    input.channel.insert(input.channel.begin(), std::move(pch));
    return true;
  }

  Image quantized_input;
  if (lossy) {
    quantized_input = Image(w, h, input.bitdepth, nb);
    for (size_t c = 0; c < nb; c++) {
      CopyImageTo(input.channel[begin_c + c].plane,
                  &quantized_input.channel[c].plane);
    }
  }

  JXL_DEBUG_V(
      7, "Trying to represent channels %i-%i using at most a %i-color palette.",
      begin_c, end_c, nb_colors);
  nb_deltas = 0;
  bool delta_used = false;
  std::set<std::vector<pixel_type>>
      candidate_palette;  // ordered lexicographically
  std::vector<std::vector<pixel_type>> candidate_palette_imageorder;
  std::vector<pixel_type> color(nb);
  std::vector<float> color_with_error(nb);
  std::vector<const pixel_type *> p_in(nb);

  if (lossy) {
    palette_iteration_data.FindFrequentColorDeltas(w * h, input.bitdepth);
    nb_deltas = palette_iteration_data.frequent_deltas[0].size();

    // Count color frequency for colors that make a cross.
    std::map<std::vector<pixel_type>, size_t> color_freq_map;
    for (size_t y = 1; y + 1 < h; y++) {
      for (uint32_t c = 0; c < nb; c++) {
        p_in[c] = input.channel[begin_c + c].Row(y);
      }
      for (size_t x = 1; x + 1 < w; x++) {
        for (uint32_t c = 0; c < nb; c++) {
          color[c] = p_in[c][x];
        }
        int offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        bool makes_cross = true;
        for (int i = 0; i < 4 && makes_cross; ++i) {
          int dx = offsets[i][0];
          int dy = offsets[i][1];
          for (uint32_t c = 0; c < nb && makes_cross; c++) {
            if (input.channel[begin_c + c].Row(y + dy)[x + dx] != color[c]) {
              makes_cross = false;
            }
          }
        }
        if (makes_cross) color_freq_map[color] += 1;
      }
    }
    // Add colors satisfying frequency condition to the palette.
    constexpr float kImageFraction = 0.01f;
    size_t color_frequency_lower_bound = 5 + input.h * input.w * kImageFraction;
    for (const auto &color_freq : color_freq_map) {
      if (color_freq.second > color_frequency_lower_bound) {
        candidate_palette.insert(color_freq.first);
        candidate_palette_imageorder.push_back(color_freq.first);
      }
    }
  }

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

  nb_colors = nb_deltas + candidate_palette.size();
  JXL_DEBUG_V(6, "Channels %i-%i can be represented using a %i-color palette.",
              begin_c, end_c, nb_colors);

  Channel pch(nb_colors, nb);
  pch.hshift = -1;
  pixel_type *JXL_RESTRICT p_palette = pch.Row(0);
  intptr_t onerow = pch.plane.PixelsPerRow();
  intptr_t onerow_image = input.channel[begin_c].plane.PixelsPerRow();
  const int bit_depth = input.bitdepth;

  if (lossy) {
    for (uint32_t i = 0; i < nb_deltas; i++) {
      for (size_t c = 0; c < 3; c++) {
        p_palette[c * onerow + i] =
            palette_iteration_data.frequent_deltas[c][i];
      }
    }
  }

  int x = 0;
  if (ordered) {
    JXL_DEBUG_V(7, "Palette of %i colors, using lexicographic order",
                nb_colors);
    for (auto pcol : candidate_palette) {
      JXL_DEBUG_V(9, "  Color %i :  ", x);
      for (size_t i = 0; i < nb; i++) {
        p_palette[nb_deltas + i * onerow + x] = pcol[i];
      }
      for (size_t i = 0; i < nb; i++) {
        JXL_DEBUG_V(9, "%i ", pcol[i]);
      }
      x++;
    }
  } else {
    JXL_DEBUG_V(7, "Palette of %i colors, using image order", nb_colors);
    for (auto pcol : candidate_palette_imageorder) {
      JXL_DEBUG_V(9, "  Color %i :  ", x);
      for (size_t i = 0; i < nb; i++)
        p_palette[nb_deltas + i * onerow + x] = pcol[i];
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
        if (index < static_cast<int>(nb_deltas)) {
          delta_used = true;
        }
      } else {
        int best_index = 0;
        bool best_is_delta = false;
        float best_distance = std::numeric_limits<float>::infinity();
        std::vector<pixel_type> best_val(nb, 0);
        std::vector<pixel_type> ideal_residual(nb, 0);
        std::vector<pixel_type> quantized_val(nb);
        std::vector<pixel_type> predictions(nb);
        static const double kDiffusionMultiplier[] = {0.55, 0.75};
        for (int diffusion_index = 0; diffusion_index < 2; ++diffusion_index) {
          for (size_t c = 0; c < nb; c++) {
            color_with_error[c] =
                p_in[c][x] + palette_iteration_data.final_run *
                                 kDiffusionMultiplier[diffusion_index] *
                                 error_row[0][c][x + 2];
            color[c] = Clamp1(lroundf(color_with_error[c]), 0l,
                              (1l << input.bitdepth) - 1);
          }

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
              if (index < static_cast<int>(nb_deltas)) {
                quantized_val[c] += predictions[c];
              }
            }
            const float color_distance =
                32.0 / (1LL << std::max(0, 2 * (bit_depth - 8))) *
                palette_internal::ColorDistance(color_with_error,
                                                quantized_val);
            float index_penalty = 0;
            if (index == -1) {
              index_penalty = -124;
            } else if (index < 0) {
              index_penalty = -2 * index;
            } else if (index < static_cast<int>(nb_deltas)) {
              index_penalty = 250;
            } else if (index < static_cast<int>(nb_colors)) {
              index_penalty = 150;
            } else if (index < static_cast<int>(nb_colors) +
                                   palette_internal::kLargeCubeOffset) {
              index_penalty = 70;
            } else {
              index_penalty = 256;
            }
            const float distance = color_distance + index_penalty;
            if (distance < best_distance) {
              best_distance = distance;
              best_index = index;
              best_is_delta = index < static_cast<int>(nb_deltas);
              best_val.swap(quantized_val);
              for (size_t c = 0; c < nb; ++c) {
                ideal_residual[c] = color_with_error[c] - predictions[c];
              }
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
        }
        index = best_index;
        delta_used |= best_is_delta;
        if (!palette_iteration_data.final_run) {
          for (size_t c = 0; c < 3; ++c) {
            palette_iteration_data.deltas[c].push_back(ideal_residual[c]);
          }
          palette_iteration_data.delta_distances.push_back(best_distance);
        }

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
          float total_error = (color_with_error[c] - best_val[c]);

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
      if (palette_iteration_data.final_run) p[x] = index;
    }
    if (lossy) {
      for (size_t c = 0; c < nb; ++c) {
        error_row[0][c].swap(error_row[1][c]);
        error_row[1][c].swap(error_row[2][c]);
        std::fill(error_row[2][c].begin(), error_row[2][c].end(), 0.f);
      }
    }
  }
  if (!delta_used) {
    predictor = Predictor::Zero;
  }
  if (palette_iteration_data.final_run) {
    input.nb_meta_channels++;
    input.channel.erase(input.channel.begin() + begin_c + 1,
                        input.channel.begin() + end_c + 1);
    input.channel.insert(input.channel.begin(), std::move(pch));
  }
  nb_colors -= nb_deltas;
  return true;
}

Status FwdPalette(Image &input, uint32_t begin_c, uint32_t end_c,
                  uint32_t &nb_colors, uint32_t &nb_deltas, bool ordered,
                  bool lossy, Predictor &predictor,
                  const weighted::Header &wp_header) {
  PaletteIterationData palette_iteration_data;
  uint32_t nb = end_c - begin_c + 1;
  uint32_t nb_colors_orig = nb_colors;
  uint32_t nb_deltas_orig = nb_deltas;
  bool status;
  if ((lossy || nb != 1) &&
      input.bitdepth >= 8) {  // if no channel palette special case
    status = FwdPaletteIteration(input, begin_c, end_c, nb_colors, nb_deltas,
                                 ordered, lossy, predictor, wp_header,
                                 palette_iteration_data);
  }
  palette_iteration_data.final_run = true;
  nb_colors = nb_colors_orig;
  nb_deltas = nb_deltas_orig;
  status =
      FwdPaletteIteration(input, begin_c, end_c, nb_colors, nb_deltas, ordered,
                          lossy, predictor, wp_header, palette_iteration_data);
  return status;
}

}  // namespace jxl
