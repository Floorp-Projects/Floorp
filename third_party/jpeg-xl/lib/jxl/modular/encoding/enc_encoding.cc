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

#include <stdint.h>
#include <stdlib.h>

#include <cinttypes>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_ans.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/encoding/enc_ma.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/modular/encoding/ma_common.h"
#include "lib/jxl/modular/options.h"
#include "lib/jxl/modular/transform/transform.h"
#include "lib/jxl/toc.h"

namespace jxl {

namespace {
// Plot tree (if enabled) and predictor usage map.
constexpr bool kWantDebug = false;
}  // namespace

void GatherTreeData(const Image &image, pixel_type chan, size_t group_id,
                    const weighted::Header &wp_header,
                    const ModularOptions &options, TreeSamples &tree_samples,
                    size_t *total_pixels) {
  const Channel &channel = image.channel[chan];

  JXL_DEBUG_V(7, "Learning %zux%zu channel %d", channel.w, channel.h, chan);

  std::array<pixel_type, kNumStaticProperties> static_props = {chan,
                                                               (int)group_id};
  Properties properties(kNumNonrefProperties +
                        kExtraPropsPerChannel * options.max_properties);
  double pixel_fraction = std::min(1.0f, options.nb_repeats);
  // a fraction of 0 is used to disable learning entirely.
  if (pixel_fraction > 0) {
    pixel_fraction = std::max(pixel_fraction,
                              std::min(1.0, 1024.0 / (channel.w * channel.h)));
  }
  uint64_t threshold =
      (std::numeric_limits<uint64_t>::max() >> 32) * pixel_fraction;
  uint64_t s[2] = {0x94D049BB133111EBull, 0xBF58476D1CE4E5B9ull};
  // Xorshift128+ adapted from xorshift128+-inl.h
  auto use_sample = [&]() {
    auto s1 = s[0];
    const auto s0 = s[1];
    const auto bits = s1 + s0;  // b, c
    s[0] = s0;
    s1 ^= s1 << 23;
    s1 ^= s0 ^ (s1 >> 18) ^ (s0 >> 5);
    s[1] = s1;
    return (bits >> 32) <= threshold;
  };

  const intptr_t onerow = channel.plane.PixelsPerRow();
  Channel references(properties.size() - kNumNonrefProperties, channel.w);
  weighted::State wp_state(wp_header, channel.w, channel.h);
  tree_samples.PrepareForSamples(pixel_fraction * channel.h * channel.w + 64);
  for (size_t y = 0; y < channel.h; y++) {
    const pixel_type *JXL_RESTRICT p = channel.Row(y);
    PrecomputeReferences(channel, y, image, chan, &references);
    InitPropsRow(&properties, static_props, y);
    // TODO(veluca): avoid computing WP if we don't use its property or
    // predictions.
    for (size_t x = 0; x < channel.w; x++) {
      pixel_type_w pred[kNumModularPredictors];
      if (tree_samples.NumPredictors() != 1) {
        PredictLearnAll(&properties, channel.w, p + x, onerow, x, y, references,
                        &wp_state, pred);
      } else {
        pred[static_cast<int>(tree_samples.PredictorFromIndex(0))] =
            PredictLearn(&properties, channel.w, p + x, onerow, x, y,
                         tree_samples.PredictorFromIndex(0), references,
                         &wp_state)
                .guess;
      }
      (*total_pixels)++;
      if (use_sample()) {
        tree_samples.AddSample(p[x], properties, pred);
      }
      wp_state.UpdateErrors(p[x], x, y, channel.w);
    }
  }
}

Tree LearnTree(TreeSamples &&tree_samples, size_t total_pixels,
               const ModularOptions &options,
               const std::vector<ModularMultiplierInfo> &multiplier_info = {},
               StaticPropRange static_prop_range = {}) {
  for (size_t i = 0; i < kNumStaticProperties; i++) {
    if (static_prop_range[i][1] == 0) {
      static_prop_range[i][1] = std::numeric_limits<uint32_t>::max();
    }
  }
  if (!tree_samples.HasSamples()) {
    Tree tree;
    tree.emplace_back();
    tree.back().predictor = tree_samples.PredictorFromIndex(0);
    tree.back().property = -1;
    tree.back().predictor_offset = 0;
    tree.back().multiplier = 1;
    return tree;
  }
  float pixel_fraction = tree_samples.NumSamples() * 1.0f / total_pixels;
  float required_cost = pixel_fraction * 0.9 + 0.1;
  tree_samples.AllSamplesDone();
  Tree tree;
  ComputeBestTree(tree_samples,
                  options.splitting_heuristics_node_threshold * required_cost,
                  multiplier_info, static_prop_range,
                  options.fast_decode_multiplier, &tree);
  return tree;
}

constexpr bool kPrintTree = false;

void PrintTree(const Tree &tree, const std::string &path) {
  if (!kPrintTree) return;
  FILE *f = fopen((path + ".dot").c_str(), "w");
  fprintf(f, "graph{\n");
  for (size_t cur = 0; cur < tree.size(); cur++) {
    if (tree[cur].property < 0) {
      fprintf(f, "n%05zu [label=\"%s%+" PRId64 " (x%u)\"];\n", cur,
              PredictorName(tree[cur].predictor), tree[cur].predictor_offset,
              tree[cur].multiplier);
    } else {
      fprintf(f, "n%05zu [label=\"%s>%d\"];\n", cur,
              PropertyName(tree[cur].property).c_str(), tree[cur].splitval);
      fprintf(f, "n%05zu -- n%05d;\n", cur, tree[cur].lchild);
      fprintf(f, "n%05zu -- n%05d;\n", cur, tree[cur].rchild);
    }
  }
  fprintf(f, "}\n");
  fclose(f);
  JXL_ASSERT(
      system(("dot " + path + ".dot -T svg -o " + path + ".svg").c_str()) == 0);
}

Status EncodeModularChannelMAANS(const Image &image, pixel_type chan,
                                 const weighted::Header &wp_header,
                                 const Tree &global_tree,
                                 std::vector<Token> *tokens, AuxOut *aux_out,
                                 size_t group_id, bool skip_encoder_fast_path) {
  const Channel &channel = image.channel[chan];

  JXL_ASSERT(channel.w != 0 && channel.h != 0);

  Image3F predictor_img;
  if (kWantDebug) predictor_img = Image3F(channel.w, channel.h);

  JXL_DEBUG_V(6,
              "Encoding %zux%zu channel %d, "
              "(shift=%i,%i, cshift=%i,%i)",
              channel.w, channel.h, chan, channel.hshift, channel.vshift,
              channel.hcshift, channel.vcshift);

  std::array<pixel_type, kNumStaticProperties> static_props = {chan,
                                                               (int)group_id};
  bool use_wp, is_wp_only;
  bool is_gradient_only;
  size_t num_props;
  FlatTree tree = FilterTree(global_tree, static_props, &num_props, &use_wp,
                             &is_wp_only, &is_gradient_only);
  Properties properties(num_props);
  MATreeLookup tree_lookup(tree);
  JXL_DEBUG_V(3, "Encoding using a MA tree with %zu nodes", tree.size());

  // Check if this tree is a WP-only tree with a small enough property value
  // range.
  // Initialized to avoid clang-tidy complaining.
  uint16_t context_lookup[2 * kPropRangeFast] = {};
  int8_t offsets[2 * kPropRangeFast] = {};
  if (is_wp_only) {
    is_wp_only = TreeToLookupTable(tree, context_lookup, offsets);
  }

  tokens->reserve(tokens->size() + channel.w * channel.h);
  if (is_wp_only && !skip_encoder_fast_path) {
    for (size_t c = 0; c < 3; c++) {
      FillImage(static_cast<float>(PredictorColor(Predictor::Weighted)[c]),
                &predictor_img.Plane(c));
    }
    const intptr_t onerow = channel.plane.PixelsPerRow();
    weighted::State wp_state(wp_header, channel.w, channel.h);
    Properties properties(1);
    for (size_t y = 0; y < channel.h; y++) {
      const pixel_type *JXL_RESTRICT r = channel.Row(y);
      for (size_t x = 0; x < channel.w; x++) {
        size_t offset = 0;
        pixel_type_w left = (x ? r[x - 1] : y ? *(r + x - onerow) : 0);
        pixel_type_w top = (y ? *(r + x - onerow) : left);
        pixel_type_w topleft = (x && y ? *(r + x - 1 - onerow) : left);
        pixel_type_w topright =
            (x + 1 < channel.w && y ? *(r + x + 1 - onerow) : top);
        pixel_type_w toptop = (y > 1 ? *(r + x - onerow - onerow) : top);
        int32_t guess = wp_state.Predict</*compute_properties=*/true>(
            x, y, channel.w, top, left, topright, topleft, toptop, &properties,
            offset);
        uint32_t pos =
            kPropRangeFast + std::min(std::max(-kPropRangeFast, properties[0]),
                                      kPropRangeFast - 1);
        uint32_t ctx_id = context_lookup[pos];
        int32_t residual = r[x] - guess - offsets[pos];
        tokens->emplace_back(ctx_id, PackSigned(residual));
        wp_state.UpdateErrors(r[x], x, y, channel.w);
      }
    }
  } else if (tree.size() == 1 && tree[0].predictor == Predictor::Zero &&
             tree[0].multiplier == 1 && tree[0].predictor_offset == 0 &&
             !skip_encoder_fast_path) {
    for (size_t c = 0; c < 3; c++) {
      FillImage(static_cast<float>(PredictorColor(Predictor::Zero)[c]),
                &predictor_img.Plane(c));
    }
    for (size_t y = 0; y < channel.h; y++) {
      const pixel_type *JXL_RESTRICT p = channel.Row(y);
      for (size_t x = 0; x < channel.w; x++) {
        tokens->emplace_back(tree[0].childID, PackSigned(p[x]));
      }
    }
  } else if (tree.size() == 1 && tree[0].predictor != Predictor::Weighted &&
             (tree[0].multiplier & (tree[0].multiplier - 1)) == 0 &&
             tree[0].predictor_offset == 0 && !skip_encoder_fast_path) {
    // multiplier is a power of 2.
    for (size_t c = 0; c < 3; c++) {
      FillImage(static_cast<float>(PredictorColor(tree[0].predictor)[c]),
                &predictor_img.Plane(c));
    }
    uint32_t mul_shift = FloorLog2Nonzero((uint32_t)tree[0].multiplier);
    const intptr_t onerow = channel.plane.PixelsPerRow();
    for (size_t y = 0; y < channel.h; y++) {
      const pixel_type *JXL_RESTRICT r = channel.Row(y);
      for (size_t x = 0; x < channel.w; x++) {
        PredictionResult pred = PredictNoTreeNoWP(channel.w, r + x, onerow, x,
                                                  y, tree[0].predictor);
        pixel_type_w residual = r[x] - pred.guess;
        JXL_DASSERT((residual >> mul_shift) * tree[0].multiplier == residual);
        tokens->emplace_back(tree[0].childID,
                             PackSigned(residual >> mul_shift));
      }
    }

  } else if (!use_wp && !skip_encoder_fast_path) {
    const intptr_t onerow = channel.plane.PixelsPerRow();
    Channel references(properties.size() - kNumNonrefProperties, channel.w);
    for (size_t y = 0; y < channel.h; y++) {
      const pixel_type *JXL_RESTRICT p = channel.Row(y);
      PrecomputeReferences(channel, y, image, chan, &references);
      float *pred_img_row[3];
      if (kWantDebug) {
        for (size_t c = 0; c < 3; c++) {
          pred_img_row[c] = predictor_img.PlaneRow(c, y);
        }
      }
      InitPropsRow(&properties, static_props, y);
      for (size_t x = 0; x < channel.w; x++) {
        PredictionResult res =
            PredictTreeNoWP(&properties, channel.w, p + x, onerow, x, y,
                            tree_lookup, references);
        if (kWantDebug) {
          for (size_t i = 0; i < 3; i++) {
            pred_img_row[i][x] = PredictorColor(res.predictor)[i];
          }
        }
        pixel_type_w residual = p[x] - res.guess;
        JXL_ASSERT(residual % res.multiplier == 0);
        tokens->emplace_back(res.context,
                             PackSigned(residual / res.multiplier));
      }
    }
  } else {
    const intptr_t onerow = channel.plane.PixelsPerRow();
    Channel references(properties.size() - kNumNonrefProperties, channel.w);
    weighted::State wp_state(wp_header, channel.w, channel.h);
    for (size_t y = 0; y < channel.h; y++) {
      const pixel_type *JXL_RESTRICT p = channel.Row(y);
      PrecomputeReferences(channel, y, image, chan, &references);
      float *pred_img_row[3];
      if (kWantDebug) {
        for (size_t c = 0; c < 3; c++) {
          pred_img_row[c] = predictor_img.PlaneRow(c, y);
        }
      }
      InitPropsRow(&properties, static_props, y);
      for (size_t x = 0; x < channel.w; x++) {
        PredictionResult res =
            PredictTreeWP(&properties, channel.w, p + x, onerow, x, y,
                          tree_lookup, references, &wp_state);
        if (kWantDebug) {
          for (size_t i = 0; i < 3; i++) {
            pred_img_row[i][x] = PredictorColor(res.predictor)[i];
          }
        }
        pixel_type_w residual = p[x] - res.guess;
        JXL_ASSERT(residual % res.multiplier == 0);
        tokens->emplace_back(res.context,
                             PackSigned(residual / res.multiplier));
        wp_state.UpdateErrors(p[x], x, y, channel.w);
      }
    }
  }
  if (kWantDebug && WantDebugOutput(aux_out)) {
    aux_out->DumpImage(
        ("pred_" + ToString(group_id) + "_" + ToString(chan)).c_str(),
        predictor_img);
  }
  return true;
}

Status ModularEncode(const Image &image, const ModularOptions &options,
                     BitWriter *writer, AuxOut *aux_out, size_t layer,
                     size_t group_id, TreeSamples *tree_samples,
                     size_t *total_pixels, const Tree *tree,
                     GroupHeader *header, std::vector<Token> *tokens,
                     size_t *width) {
  if (image.error) return JXL_FAILURE("Invalid image");
  size_t nb_channels = image.channel.size();
  int bit_depth = 1, maxval = 1;
  while (maxval < image.maxval) {
    bit_depth++;
    maxval = maxval * 2 + 1;
  }
  JXL_DEBUG_V(2, "Encoding %zu-channel, %i-bit, %zux%zu image.", nb_channels,
              bit_depth, image.w, image.h);

  if (nb_channels < 1) {
    return true;  // is there any use for a zero-channel image?
  }

  // encode transforms
  GroupHeader header_storage;
  if (header == nullptr) header = &header_storage;
  Bundle::Init(header);
  if (options.predictor == Predictor::Weighted) {
    weighted::PredictorMode(options.wp_mode, &header->wp_header);
  }
  header->transforms = image.transform;
  // This doesn't actually work
  if (tree != nullptr) {
    header->use_global_tree = true;
  }
  if (tree_samples == nullptr && tree == nullptr) {
    JXL_RETURN_IF_ERROR(Bundle::Write(*header, writer, layer, aux_out));
  }

  TreeSamples tree_samples_storage;
  size_t total_pixels_storage = 0;
  if (!total_pixels) total_pixels = &total_pixels_storage;
  // If there's no tree, compute one (or gather data to).
  if (tree == nullptr) {
    bool gather_data = tree_samples != nullptr;
    if (tree_samples == nullptr) {
      JXL_RETURN_IF_ERROR(tree_samples_storage.SetPredictor(
          options.predictor, options.wp_tree_mode));
      JXL_RETURN_IF_ERROR(tree_samples_storage.SetProperties(
          options.splitting_heuristics_properties, options.wp_tree_mode));
      std::vector<pixel_type> pixel_samples;
      std::vector<pixel_type> diff_samples;
      std::vector<uint32_t> group_pixel_count;
      std::vector<uint32_t> channel_pixel_count;
      CollectPixelSamples(image, options, 0, group_pixel_count,
                          channel_pixel_count, pixel_samples, diff_samples);
      std::vector<ModularMultiplierInfo> dummy_multiplier_info;
      StaticPropRange range;
      tree_samples_storage.PreQuantizeProperties(
          range, dummy_multiplier_info, group_pixel_count, channel_pixel_count,
          pixel_samples, diff_samples, options.max_property_values);
    }
    for (size_t i = 0; i < nb_channels; i++) {
      if (!image.channel[i].w || !image.channel[i].h) {
        continue;  // skip empty channels
      }
      if (i >= image.nb_meta_channels &&
          (image.channel[i].w > options.max_chan_size ||
           image.channel[i].h > options.max_chan_size)) {
        break;
      }
      GatherTreeData(image, i, group_id, header->wp_header, options,
                     gather_data ? *tree_samples : tree_samples_storage,
                     total_pixels);
    }
    if (gather_data) return true;
  }

  JXL_ASSERT((tree == nullptr) == (tokens == nullptr));

  Tree tree_storage;
  std::vector<std::vector<Token>> tokens_storage(1);
  // Compute tree.
  if (tree == nullptr) {
    EntropyEncodingData code;
    std::vector<uint8_t> context_map;

    std::vector<std::vector<Token>> tree_tokens(1);
    tree_storage =
        LearnTree(std::move(tree_samples_storage), *total_pixels, options);
    tree = &tree_storage;
    tokens = &tokens_storage[0];

    Tree decoded_tree;
    TokenizeTree(*tree, &tree_tokens[0], &decoded_tree);
    JXL_ASSERT(tree->size() == decoded_tree.size());
    tree_storage = std::move(decoded_tree);

    if (kWantDebug && WantDebugOutput(aux_out)) {
      PrintTree(*tree, aux_out->debug_prefix + "/tree_" + ToString(group_id));
    }
    // Write tree
    BuildAndEncodeHistograms(HistogramParams(), kNumTreeContexts, tree_tokens,
                             &code, &context_map, writer, kLayerModularTree,
                             aux_out);
    WriteTokens(tree_tokens[0], code, context_map, writer, kLayerModularTree,
                aux_out);
  }

  size_t image_width = 0;
  for (size_t i = 0; i < nb_channels; i++) {
    if (!image.channel[i].w || !image.channel[i].h) {
      continue;  // skip empty channels
    }
    if (i >= image.nb_meta_channels &&
        (image.channel[i].w > options.max_chan_size ||
         image.channel[i].h > options.max_chan_size)) {
      break;
    }
    if (image.channel[i].w > image_width) image_width = image.channel[i].w;
    if (options.zero_tokens) {
      tokens->resize(tokens->size() + image.channel[i].w * image.channel[i].h,
                     {0, 0});
    } else {
      JXL_RETURN_IF_ERROR(EncodeModularChannelMAANS(
          image, i, header->wp_header, *tree, tokens, aux_out, group_id,
          options.skip_encoder_fast_path));
    }
  }

  // Write data if not using a global tree/ANS stream.
  if (!header->use_global_tree) {
    EntropyEncodingData code;
    std::vector<uint8_t> context_map;
    HistogramParams histo_params;
    histo_params.image_widths.push_back(image_width);
    BuildAndEncodeHistograms(histo_params, (tree->size() + 1) / 2,
                             tokens_storage, &code, &context_map, writer, layer,
                             aux_out);
    WriteTokens(tokens_storage[0], code, context_map, writer, layer, aux_out);
  } else {
    *width = image_width;
  }
  return true;
}

Status ModularGenericCompress(Image &image, const ModularOptions &opts,
                              BitWriter *writer, AuxOut *aux_out, size_t layer,
                              size_t group_id, TreeSamples *tree_samples,
                              size_t *total_pixels, const Tree *tree,
                              GroupHeader *header, std::vector<Token> *tokens,
                              size_t *width) {
  if (image.w == 0 || image.h == 0) return true;
  ModularOptions options = opts;  // Make a copy to modify it.

  if (options.predictor == static_cast<Predictor>(-1)) {
    options.predictor = Predictor::Gradient;
  }

  size_t bits = writer ? writer->BitsWritten() : 0;
  JXL_RETURN_IF_ERROR(ModularEncode(image, options, writer, aux_out, layer,
                                    group_id, tree_samples, total_pixels, tree,
                                    header, tokens, width));
  bits = writer ? writer->BitsWritten() - bits : 0;
  if (writer) {
    JXL_DEBUG_V(
        4, "Modular-encoded a %zux%zu maxval=%i nbchans=%zu image in %zu bytes",
        image.w, image.h, image.maxval, image.real_nb_channels, bits / 8);
  }
  (void)bits;
  return true;
}

}  // namespace jxl
