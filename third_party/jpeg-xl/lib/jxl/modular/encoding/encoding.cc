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

#include "lib/jxl/modular/encoding/encoding.h"

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
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/options.h"
#include "lib/jxl/modular/transform/transform.h"
#include "lib/jxl/toc.h"

namespace jxl {

// Removes all nodes that use a static property (i.e. channel or group ID) from
// the tree and collapses each node on even levels with its two children to
// produce a flatter tree. Also computes whether the resulting tree requires
// using the weighted predictor.
FlatTree FilterTree(const Tree &global_tree,
                    std::array<pixel_type, kNumStaticProperties> &static_props,
                    size_t *num_props, bool *use_wp, bool *wp_only,
                    bool *gradient_only) {
  *num_props = 0;
  bool has_wp = false;
  bool has_non_wp = false;
  *gradient_only = true;
  const auto mark_property = [&](int32_t p) {
    if (p == kWPProp) {
      has_wp = true;
    } else if (p >= kNumStaticProperties) {
      has_non_wp = true;
    }
    if (p >= kNumStaticProperties && p != kGradientProp) {
      *gradient_only = false;
    }
  };
  FlatTree output;
  std::queue<size_t> nodes;
  nodes.push(0);
  // Produces a trimmed and flattened tree by doing a BFS visit of the original
  // tree, ignoring branches that are known to be false and proceeding two
  // levels at a time to collapse nodes in a flatter tree; if an inner parent
  // node has a leaf as a child, the leaf is duplicated and an implicit fake
  // node is added. This allows to reduce the number of branches when traversing
  // the resulting flat tree.
  while (!nodes.empty()) {
    size_t cur = nodes.front();
    nodes.pop();
    // Skip nodes that we can decide now, by jumping directly to their children.
    while (global_tree[cur].property < kNumStaticProperties &&
           global_tree[cur].property != -1) {
      if (static_props[global_tree[cur].property] > global_tree[cur].splitval) {
        cur = global_tree[cur].lchild;
      } else {
        cur = global_tree[cur].rchild;
      }
    }
    FlatDecisionNode flat;
    if (global_tree[cur].property == -1) {
      flat.property0 = -1;
      flat.childID = global_tree[cur].lchild;
      flat.predictor = global_tree[cur].predictor;
      flat.predictor_offset = global_tree[cur].predictor_offset;
      flat.multiplier = global_tree[cur].multiplier;
      *gradient_only &= flat.predictor == Predictor::Gradient;
      has_wp |= flat.predictor == Predictor::Weighted;
      has_non_wp |= flat.predictor != Predictor::Weighted;
      output.push_back(flat);
      continue;
    }
    flat.childID = output.size() + nodes.size() + 1;

    flat.property0 = global_tree[cur].property;
    *num_props = std::max<size_t>(flat.property0 + 1, *num_props);
    flat.splitval0 = global_tree[cur].splitval;

    for (size_t i = 0; i < 2; i++) {
      size_t cur_child =
          i == 0 ? global_tree[cur].lchild : global_tree[cur].rchild;
      // Skip nodes that we can decide now.
      while (global_tree[cur_child].property < kNumStaticProperties &&
             global_tree[cur_child].property != -1) {
        if (static_props[global_tree[cur_child].property] >
            global_tree[cur_child].splitval) {
          cur_child = global_tree[cur_child].lchild;
        } else {
          cur_child = global_tree[cur_child].rchild;
        }
      }
      // We ended up in a leaf, add a dummy decision and two copies of the leaf.
      if (global_tree[cur_child].property == -1) {
        flat.properties[i] = 0;
        flat.splitvals[i] = 0;
        nodes.push(cur_child);
        nodes.push(cur_child);
      } else {
        flat.properties[i] = global_tree[cur_child].property;
        flat.splitvals[i] = global_tree[cur_child].splitval;
        nodes.push(global_tree[cur_child].lchild);
        nodes.push(global_tree[cur_child].rchild);
        *num_props = std::max<size_t>(flat.properties[i] + 1, *num_props);
      }
    }

    for (size_t j = 0; j < 2; j++) mark_property(flat.properties[j]);
    mark_property(flat.property0);
    output.push_back(flat);
  }
  if (*num_props > kNumNonrefProperties) {
    *num_props =
        DivCeil(*num_props - kNumNonrefProperties, kExtraPropsPerChannel) *
            kExtraPropsPerChannel +
        kNumNonrefProperties;
  } else {
    *num_props = kNumNonrefProperties;
  }
  *use_wp = has_wp;
  *wp_only = has_wp && !has_non_wp;

  return output;
}

Status DecodeModularChannelMAANS(BitReader *br, ANSSymbolReader *reader,
                                 const std::vector<uint8_t> &context_map,
                                 const Tree &global_tree,
                                 const weighted::Header &wp_header,
                                 pixel_type chan, size_t group_id,
                                 Image *image) {
  Channel &channel = image->channel[chan];

  std::array<pixel_type, kNumStaticProperties> static_props = {chan,
                                                               (int)group_id};
  // TODO(veluca): filter the tree according to static_props.

  // zero pixel channel? could happen
  if (channel.w == 0 || channel.h == 0) return true;

  channel.resize(channel.w, channel.h);
  bool tree_has_wp_prop_or_pred = false;
  bool is_wp_only = false;
  bool is_gradient_only = false;
  size_t num_props;
  FlatTree tree =
      FilterTree(global_tree, static_props, &num_props,
                 &tree_has_wp_prop_or_pred, &is_wp_only, &is_gradient_only);

  // From here on, tree lookup returns a *clustered* context ID.
  // This avoids an extra memory lookup after tree traversal.
  for (size_t i = 0; i < tree.size(); i++) {
    if (tree[i].property0 == -1) {
      tree[i].childID = context_map[tree[i].childID];
    }
  }

  JXL_DEBUG_V(3, "Decoded MA tree with %zu nodes", tree.size());

  // MAANS decode

  // Check if this tree is a WP-only tree with a small enough property value
  // range.
  // Those contexts are *clustered* context ids. This reduces stack usages and
  // avoids an extra memory lookup.
  // Initialized to avoid clang-tidy complaining.
  uint8_t context_lookup[2 * kPropRangeFast] = {};
  int8_t multipliers[2 * kPropRangeFast] = {};
  int8_t offsets[2 * kPropRangeFast] = {};
  if (is_wp_only) {
    is_wp_only = TreeToLookupTable(tree, context_lookup, offsets, multipliers);
  }
  if (is_gradient_only) {
    is_gradient_only =
        TreeToLookupTable(tree, context_lookup, offsets, multipliers);
  }

  const auto make_pixel = [](uint64_t v, pixel_type multiplier,
                             pixel_type_w offset) -> pixel_type {
    JXL_DASSERT((v & 0xFFFFFFFF) == v);
    pixel_type_w val = UnpackSigned(v);
    return SaturatingAdd<pixel_type>(val * multiplier, offset);
  };

  if (is_gradient_only) {
    JXL_DEBUG_V(8, "Gradient fast track.");
    const intptr_t onerow = channel.plane.PixelsPerRow();
    for (size_t y = 0; y < channel.h; y++) {
      pixel_type *JXL_RESTRICT r = channel.Row(y);
      for (size_t x = 0; x < channel.w; x++) {
        pixel_type_w left = (x ? r[x - 1] : y ? *(r + x - onerow) : 0);
        pixel_type_w top = (y ? *(r + x - onerow) : left);
        pixel_type_w topleft = (x && y ? *(r + x - 1 - onerow) : left);
        int32_t guess = ClampedGradient(top, left, topleft);
        uint32_t pos =
            kPropRangeFast +
            std::min<pixel_type_w>(
                std::max<pixel_type_w>(-kPropRangeFast, top + left - topleft),
                kPropRangeFast - 1);
        uint32_t ctx_id = context_lookup[pos];
        uint64_t v = reader->ReadHybridUintClustered(ctx_id, br);
        r[x] = make_pixel(v, multipliers[pos],
                          static_cast<pixel_type_w>(offsets[pos]) + guess);
      }
    }
  } else if (is_wp_only) {
    JXL_DEBUG_V(8, "WP fast track.");
    const intptr_t onerow = channel.plane.PixelsPerRow();
    weighted::State wp_state(wp_header, channel.w, channel.h);
    Properties properties(1);
    for (size_t y = 0; y < channel.h; y++) {
      pixel_type *JXL_RESTRICT r = channel.Row(y);
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
        uint64_t v = reader->ReadHybridUintClustered(ctx_id, br);
        r[x] = make_pixel(v, multipliers[pos],
                          static_cast<pixel_type_w>(offsets[pos]) + guess);
        wp_state.UpdateErrors(r[x], x, y, channel.w);
      }
    }
  } else if (tree.size() == 1) {
    // special optimized case: no meta-adaptation, so no need
    // to compute properties.
    Predictor predictor = tree[0].predictor;
    int64_t offset = tree[0].predictor_offset;
    int32_t multiplier = tree[0].multiplier;
    size_t ctx_id = tree[0].childID;
    if (predictor == Predictor::Zero) {
      uint32_t value;
      if (reader->IsSingleValueAndAdvance(ctx_id, &value,
                                          channel.w * channel.h)) {
        // Special-case: histogram has a single symbol, with no extra bits, and
        // we use ANS mode.
        JXL_DEBUG_V(8, "Fastest track.");
        pixel_type v = make_pixel(value, multiplier, offset);
        for (size_t y = 0; y < channel.h; y++) {
          pixel_type *JXL_RESTRICT r = channel.Row(y);
          std::fill(r, r + channel.w, v);
        }

      } else {
        JXL_DEBUG_V(8, "Fast track.");
        for (size_t y = 0; y < channel.h; y++) {
          pixel_type *JXL_RESTRICT r = channel.Row(y);
          for (size_t x = 0; x < channel.w; x++) {
            uint32_t v = reader->ReadHybridUintClustered(ctx_id, br);
            r[x] = make_pixel(v, multiplier, offset);
          }
        }
      }
    } else if (predictor != Predictor::Weighted) {
      // special optimized case: no meta-adaptation, no wp, so no need to
      // compute properties
      JXL_DEBUG_V(8, "Quite fast track.");
      const intptr_t onerow = channel.plane.PixelsPerRow();
      for (size_t y = 0; y < channel.h; y++) {
        pixel_type *JXL_RESTRICT r = channel.Row(y);
        for (size_t x = 0; x < channel.w; x++) {
          PredictionResult pred =
              PredictNoTreeNoWP(channel.w, r + x, onerow, x, y, predictor);
          pixel_type_w g = pred.guess + offset;
          uint64_t v = reader->ReadHybridUintClustered(ctx_id, br);
          // NOTE: pred.multiplier is unset.
          r[x] = make_pixel(v, multiplier, g);
        }
      }
    } else {
      // special optimized case: no meta-adaptation, so no need to
      // compute properties
      JXL_DEBUG_V(8, "Somewhat fast track.");
      const intptr_t onerow = channel.plane.PixelsPerRow();
      weighted::State wp_state(wp_header, channel.w, channel.h);
      for (size_t y = 0; y < channel.h; y++) {
        pixel_type *JXL_RESTRICT r = channel.Row(y);
        for (size_t x = 0; x < channel.w; x++) {
          pixel_type_w g = PredictNoTreeWP(channel.w, r + x, onerow, x, y,
                                           predictor, &wp_state)
                               .guess +
                           offset;
          uint64_t v = reader->ReadHybridUintClustered(ctx_id, br);
          r[x] = make_pixel(v, multiplier, g);
          wp_state.UpdateErrors(r[x], x, y, channel.w);
        }
      }
    }
  } else if (!tree_has_wp_prop_or_pred) {
    // special optimized case: the weighted predictor and its properties are not
    // used, so no need to compute weights and properties.
    JXL_DEBUG_V(8, "Slow track.");
    MATreeLookup tree_lookup(tree);
    Properties properties = Properties(num_props);
    const intptr_t onerow = channel.plane.PixelsPerRow();
    Channel references(properties.size() - kNumNonrefProperties, channel.w);
    for (size_t y = 0; y < channel.h; y++) {
      pixel_type *JXL_RESTRICT p = channel.Row(y);
      PrecomputeReferences(channel, y, *image, chan, &references);
      InitPropsRow(&properties, static_props, y);
      for (size_t x = 0; x < channel.w; x++) {
        PredictionResult res =
            PredictTreeNoWP(&properties, channel.w, p + x, onerow, x, y,
                            tree_lookup, references);
        uint64_t v = reader->ReadHybridUintClustered(res.context, br);
        p[x] = make_pixel(v, res.multiplier, res.guess);
      }
    }
  } else {
    JXL_DEBUG_V(8, "Slowest track.");
    MATreeLookup tree_lookup(tree);
    Properties properties = Properties(num_props);
    const intptr_t onerow = channel.plane.PixelsPerRow();
    Channel references(properties.size() - kNumNonrefProperties, channel.w);
    weighted::State wp_state(wp_header, channel.w, channel.h);
    for (size_t y = 0; y < channel.h; y++) {
      pixel_type *JXL_RESTRICT p = channel.Row(y);
      InitPropsRow(&properties, static_props, y);
      PrecomputeReferences(channel, y, *image, chan, &references);
      for (size_t x = 0; x < channel.w; x++) {
        PredictionResult res =
            PredictTreeWP(&properties, channel.w, p + x, onerow, x, y,
                          tree_lookup, references, &wp_state);
        uint64_t v = reader->ReadHybridUintClustered(res.context, br);
        p[x] = make_pixel(v, res.multiplier, res.guess);
        wp_state.UpdateErrors(p[x], x, y, channel.w);
      }
    }
  }
  return true;
}

GroupHeader::GroupHeader() { Bundle::Init(this); }

Status ModularDecode(BitReader *br, Image &image, GroupHeader &header,
                     size_t group_id, ModularOptions *options,
                     const Tree *global_tree, const ANSCode *global_code,
                     const std::vector<uint8_t> *global_ctx_map,
                     bool allow_truncated_group) {
  if (image.nb_channels < 1) return true;

  // decode transforms
  JXL_RETURN_IF_ERROR(Bundle::Read(br, &header));
  JXL_DEBUG_V(4, "Global option: up to %i back-referencing MA properties.",
              options->max_properties);
  JXL_DEBUG_V(3, "Image data underwent %zu transformations: ",
              header.transforms.size());
  image.transform = header.transforms;
  for (Transform &transform : image.transform) {
    JXL_RETURN_IF_ERROR(transform.MetaApply(image));
  }
  if (image.error) {
    return JXL_FAILURE("Corrupt file. Aborting.");
  }

  size_t nb_channels = image.channel.size();

  size_t num_chans = 0;
  for (size_t i = 0; i < nb_channels; i++) {
    if (!image.channel[i].w || !image.channel[i].h) {
      continue;  // skip empty channels
    }
    if (i >= image.nb_meta_channels &&
        (image.channel[i].w > options->max_chan_size ||
         image.channel[i].h > options->max_chan_size)) {
      break;
    }
    num_chans++;
  }
  if (num_chans == 0) return true;

  // Read tree.
  Tree tree_storage;
  std::vector<uint8_t> context_map_storage;
  ANSCode code_storage;
  const Tree *tree = &tree_storage;
  const ANSCode *code = &code_storage;
  const std::vector<uint8_t> *context_map = &context_map_storage;
  if (!header.use_global_tree) {
    size_t tree_size_limit = 1024 + image.w * image.h * nb_channels;
    JXL_RETURN_IF_ERROR(DecodeTree(br, &tree_storage, tree_size_limit));
    JXL_RETURN_IF_ERROR(DecodeHistograms(br, (tree_storage.size() + 1) / 2,
                                         &code_storage, &context_map_storage));
  } else {
    if (!global_tree || !global_code || !global_ctx_map ||
        global_tree->empty()) {
      return JXL_FAILURE("No global tree available but one was requested");
    }
    tree = global_tree;
    code = global_code;
    context_map = global_ctx_map;
  }

  size_t distance_multiplier = 0;
  for (size_t i = 0; i < nb_channels; i++) {
    Channel &channel = image.channel[i];
    if (!channel.w || !channel.h) {
      continue;  // skip empty channels
    }
    if (i >= image.nb_meta_channels && (channel.w > options->max_chan_size ||
                                        channel.h > options->max_chan_size)) {
      break;
    }
    if (channel.w > distance_multiplier) {
      distance_multiplier = channel.w;
    }
  }
  // Read channels
  ANSSymbolReader reader(code, br, distance_multiplier);
  for (size_t i = 0; i < nb_channels; i++) {
    Channel &channel = image.channel[i];
    if (!channel.w || !channel.h) {
      continue;  // skip empty channels
    }
    if (i >= image.nb_meta_channels && (channel.w > options->max_chan_size ||
                                        channel.h > options->max_chan_size)) {
      break;
    }
    JXL_RETURN_IF_ERROR(DecodeModularChannelMAANS(br, &reader, *context_map,
                                                  *tree, header.wp_header, i,
                                                  group_id, &image));
    // Truncated group.
    if (!br->AllReadsWithinBounds()) {
      if (!allow_truncated_group) return JXL_FAILURE("Truncated input");
      ZeroFillImage(&channel.plane);
      return Status(StatusCode::kNotEnoughBytes);
    }
  }
  if (!reader.CheckANSFinalState()) {
    return JXL_FAILURE("ANS decode final state failed");
  }
  return true;
}

Status ModularGenericDecompress(BitReader *br, Image &image,
                                GroupHeader *header, size_t group_id,
                                ModularOptions *options, int undo_transforms,
                                const Tree *tree, const ANSCode *code,
                                const std::vector<uint8_t> *ctx_map,
                                bool allow_truncated_group) {
#ifdef JXL_ENABLE_ASSERT
  std::vector<std::pair<uint32_t, uint32_t>> req_sizes(image.channel.size());
  for (size_t c = 0; c < req_sizes.size(); c++) {
    req_sizes[c] = {image.channel[c].w, image.channel[c].h};
  }
#endif
  GroupHeader local_header;
  if (header == nullptr) header = &local_header;
  auto dec_status = ModularDecode(br, image, *header, group_id, options, tree,
                                  code, ctx_map, allow_truncated_group);
  if (!allow_truncated_group) JXL_RETURN_IF_ERROR(dec_status);
  if (dec_status.IsFatalError()) return dec_status;
  image.undo_transforms(header->wp_header, undo_transforms);
  if (image.error) return JXL_FAILURE("Corrupt file. Aborting.");
  size_t bit_pos = br->TotalBitsConsumed();
  JXL_DEBUG_V(4, "Modular-decoded a %zux%zu nbchans=%zu image from %zu bytes",
              image.w, image.h, image.real_nb_channels,
              (br->TotalBitsConsumed() - bit_pos) / 8);
  (void)bit_pos;
#ifdef JXL_ENABLE_ASSERT
  // Check that after applying all transforms we are back to the requested image
  // sizes, otherwise there's a programming error with the transformations.
  if (undo_transforms == -1 || undo_transforms == 0) {
    JXL_ASSERT(image.channel.size() == req_sizes.size());
    for (size_t c = 0; c < req_sizes.size(); c++) {
      JXL_ASSERT(req_sizes[c].first == image.channel[c].w);
      JXL_ASSERT(req_sizes[c].second == image.channel[c].h);
    }
  }
#endif
  return dec_status;
}

}  // namespace jxl
