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

#include "lib/jxl/enc_modular.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/compressed_dc.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_cluster.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/enc_patch_dictionary.h"
#include "lib/jxl/enc_quant_weights.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/gaborish.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/encoding/enc_encoding.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/modular/encoding/ma_common.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/options.h"
#include "lib/jxl/modular/transform/transform.h"
#include "lib/jxl/toc.h"

namespace jxl {

namespace {
// Squeeze default quantization factors
// these quantization factors are for -Q 50  (other qualities simply scale the
// factors; things are rounded down and obviously cannot get below 1)
static const float squeeze_quality_factor =
    0.35;  // for easy tweaking of the quality range (decrease this number for
           // higher quality)
static const float squeeze_luma_factor =
    1.1;  // for easy tweaking of the balance between luma (or anything
          // non-chroma) and chroma (decrease this number for higher quality
          // luma)
static const float squeeze_quality_factor_xyb = 2.4f;
static const float squeeze_xyb_qtable[3][16] = {
    {163.84, 81.92, 40.96, 20.48, 10.24, 5.12, 2.56, 1.28, 0.64, 0.32, 0.16,
     0.08, 0.04, 0.02, 0.01, 0.005},  // Y
    {1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0.5, 0.5, 0.5, 0.5,
     0.5},  // X
    {2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0.5, 0.5, 0.5,
     0.5},  // B-Y
};

static const float squeeze_luma_qtable[16] = {
    163.84, 81.92, 40.96, 20.48, 10.24, 5.12, 2.56, 1.28,
    0.64,   0.32,  0.16,  0.08,  0.04,  0.02, 0.01, 0.005};
// for 8-bit input, the range of YCoCg chroma is -255..255 so basically this
// does 4:2:0 subsampling (two most fine grained layers get quantized away)
static const float squeeze_chroma_qtable[16] = {
    1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0.5, 0.5, 0.5, 0.5, 0.5};

// `cutoffs` must be sorted.
Tree MakeFixedTree(int property, const std::vector<int32_t>& cutoffs,
                   Predictor pred, size_t num_pixels) {
  size_t log_px = CeilLog2Nonzero(num_pixels);
  size_t min_gap = 0;
  // Reduce fixed tree height when encoding small images.
  if (log_px < 14) {
    min_gap = 8 * (14 - log_px);
  }
  Tree tree;
  struct NodeInfo {
    size_t begin, end, pos;
  };
  std::queue<NodeInfo> q;
  // Leaf IDs will be set by roundtrip decoding the tree.
  tree.push_back(PropertyDecisionNode::Leaf(pred));
  q.push(NodeInfo{0, cutoffs.size(), 0});
  while (!q.empty()) {
    NodeInfo info = q.front();
    q.pop();
    if (info.begin + min_gap >= info.end) continue;
    uint32_t split = (info.begin + info.end) / 2;
    tree[info.pos] =
        PropertyDecisionNode::Split(property, cutoffs[split], tree.size());
    q.push(NodeInfo{split + 1, info.end, tree.size()});
    tree.push_back(PropertyDecisionNode::Leaf(pred));
    q.push(NodeInfo{info.begin, split, tree.size()});
    tree.push_back(PropertyDecisionNode::Leaf(pred));
  }
  return tree;
}

Tree PredefinedTree(ModularOptions::TreeKind tree_kind, size_t total_pixels) {
  if (tree_kind == ModularOptions::TreeKind::kJpegTranscodeACMeta) {
    // All the data is 0, so no need for a fancy tree.
    return {PropertyDecisionNode::Leaf(Predictor::Zero)};
  }
  if (tree_kind == ModularOptions::TreeKind::kFalconACMeta) {
    // All the data is 0 except the quant field. TODO(veluca): make that 0 too.
    return {PropertyDecisionNode::Leaf(Predictor::Left)};
  }
  if (tree_kind == ModularOptions::TreeKind::kACMeta) {
    // Small image.
    if (total_pixels < 1024) {
      return {PropertyDecisionNode::Leaf(Predictor::Left)};
    }
    Tree tree;
    // 0: c > 1
    tree.push_back(PropertyDecisionNode::Split(0, 1, 1));
    // 1: c > 2
    tree.push_back(PropertyDecisionNode::Split(0, 2, 3));
    // 2: c > 0
    tree.push_back(PropertyDecisionNode::Split(0, 0, 5));
    // 3: EPF control field (all 0 or 4), top > 0
    tree.push_back(PropertyDecisionNode::Split(6, 0, 21));
    // 4: ACS+QF, y > 0
    tree.push_back(PropertyDecisionNode::Split(2, 0, 7));
    // 5: CfL x
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Gradient));
    // 6: CfL b
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Gradient));
    // 7: QF: split according to the left quant value.
    tree.push_back(PropertyDecisionNode::Split(7, 5, 9));
    // 8: ACS: split in 4 segments (8x8 from 0 to 3, large square 4-5, large
    // rectangular 6-11, 8x8 12+), according to previous ACS value.
    tree.push_back(PropertyDecisionNode::Split(7, 5, 15));
    // QF
    tree.push_back(PropertyDecisionNode::Split(7, 11, 11));
    tree.push_back(PropertyDecisionNode::Split(7, 3, 13));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Left));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Left));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Left));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Left));
    // ACS
    tree.push_back(PropertyDecisionNode::Split(7, 11, 17));
    tree.push_back(PropertyDecisionNode::Split(7, 3, 19));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    // EPF, left > 0
    tree.push_back(PropertyDecisionNode::Split(7, 0, 23));
    tree.push_back(PropertyDecisionNode::Split(7, 0, 25));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    tree.push_back(PropertyDecisionNode::Leaf(Predictor::Zero));
    return tree;
  }
  if (tree_kind == ModularOptions::TreeKind::kWPFixedDC) {
    std::vector<int32_t> cutoffs = {
        -500, -392, -255, -191, -127, -95, -63, -47, -31, -23, -15,
        -11,  -7,   -4,   -3,   -1,   0,   1,   3,   5,   7,   11,
        15,   23,   31,   47,   63,   95,  127, 191, 255, 392, 500};
    return MakeFixedTree(kNumNonrefProperties - weighted::kNumProperties,
                         cutoffs, Predictor::Weighted, total_pixels);
  }
  if (tree_kind == ModularOptions::TreeKind::kGradientFixedDC) {
    std::vector<int32_t> cutoffs = {
        -500, -392, -255, -191, -127, -95, -63, -47, -31, -23, -15,
        -11,  -7,   -4,   -3,   -1,   0,   1,   3,   5,   7,   11,
        15,   23,   31,   47,   63,   95,  127, 191, 255, 392, 500};
    return MakeFixedTree(kGradientProp, cutoffs, Predictor::Gradient,
                         total_pixels);
  }
  JXL_ABORT("Unreachable");
  return {};
}

// Merges the trees in `trees` using nodes that decide on stream_id, as defined
// by `tree_splits`.
void MergeTrees(const std::vector<Tree>& trees,
                const std::vector<size_t>& tree_splits, size_t begin,
                size_t end, Tree* tree) {
  JXL_ASSERT(trees.size() + 1 == tree_splits.size());
  JXL_ASSERT(end > begin);
  JXL_ASSERT(end <= trees.size());
  if (end == begin + 1) {
    // Insert the tree, adding the opportune offset to all child nodes.
    // This will make the leaf IDs wrong, but subsequent roundtripping will fix
    // them.
    size_t sz = tree->size();
    tree->insert(tree->end(), trees[begin].begin(), trees[begin].end());
    for (size_t i = sz; i < tree->size(); i++) {
      (*tree)[i].lchild += sz;
      (*tree)[i].rchild += sz;
    }
    return;
  }
  size_t mid = (begin + end) / 2;
  size_t splitval = tree_splits[mid] - 1;
  size_t cur = tree->size();
  tree->emplace_back(1 /*stream_id*/, splitval, 0, 0, Predictor::Zero, 0, 1);
  (*tree)[cur].lchild = tree->size();
  MergeTrees(trees, tree_splits, mid, end, tree);
  (*tree)[cur].rchild = tree->size();
  MergeTrees(trees, tree_splits, begin, mid, tree);
}

void QuantizeChannel(Channel& ch, const int q) {
  if (q == 1) return;
  for (size_t y = 0; y < ch.plane.ysize(); y++) {
    pixel_type* row = ch.plane.Row(y);
    for (size_t x = 0; x < ch.plane.xsize(); x++) {
      if (row[x] < 0) {
        row[x] = -((-row[x] + q / 2) / q) * q;
      } else {
        row[x] = ((row[x] + q / 2) / q) * q;
      }
    }
  }
}

// convert binary32 float that corresponds to custom [bits]-bit float (with
// [exp_bits] exponent bits) to a [bits]-bit integer representation that should
// fit in pixel_type
Status float_to_int(const float* const row_in, pixel_type* const row_out,
                    size_t xsize, unsigned int bits, unsigned int exp_bits,
                    bool fp, float factor) {
  JXL_ASSERT(sizeof(pixel_type) * 8 >= bits);
  if (!fp) {
    for (size_t x = 0; x < xsize; ++x) {
      row_out[x] = row_in[x] * factor + 0.5f;
    }
    return true;
  }
  if (bits == 32 && fp) {
    JXL_ASSERT(exp_bits == 8);
    memcpy((void*)row_out, (const void*)row_in, 4 * xsize);
    return true;
  }

  int exp_bias = (1 << (exp_bits - 1)) - 1;
  int max_exp = (1 << exp_bits) - 1;
  uint32_t sign = (1u << (bits - 1));
  int mant_bits = bits - exp_bits - 1;
  int mant_shift = 23 - mant_bits;
  for (size_t x = 0; x < xsize; ++x) {
    uint32_t f;
    memcpy(&f, &row_in[x], 4);
    int signbit = (f >> 31);
    f &= 0x7fffffff;
    if (f == 0) {
      row_out[x] = (signbit ? sign : 0);
      continue;
    }
    int exp = (f >> 23) - 127;
    if (exp == 128) return JXL_FAILURE("Inf/NaN not allowed");
    int mantissa = (f & 0x007fffff);
    // broke up the binary32 into its parts, now reassemble into
    // arbitrary float
    exp += exp_bias;
    if (exp < 0) {  // will become a subnormal number
      // add implicit leading 1 to mantissa
      mantissa |= 0x00800000;
      if (exp < -mant_bits) {
        return JXL_FAILURE(
            "Invalid float number: %g cannot be represented with %i "
            "exp_bits and %i mant_bits (exp %i)",
            row_in[x], exp_bits, mant_bits, exp);
      }
      mantissa >>= 1 - exp;
      exp = 0;
    }
    // exp should be representable in exp_bits, otherwise input was
    // invalid
    if (exp > max_exp) return JXL_FAILURE("Invalid float exponent");
    if (mantissa & ((1 << mant_shift) - 1)) {
      return JXL_FAILURE("%g is losing precision (mant: %x)", row_in[x],
                         mantissa);
    }
    mantissa >>= mant_shift;
    f = (signbit ? sign : 0);
    f |= (exp << mant_bits);
    f |= mantissa;
    row_out[x] = (pixel_type)f;
  }
  return true;
}
}  // namespace

ModularFrameEncoder::ModularFrameEncoder(const FrameHeader& frame_header,
                                         const CompressParams& cparams_orig)
    : frame_dim(frame_header.ToFrameDimensions()), cparams(cparams_orig) {
  size_t num_streams =
      ModularStreamId::Num(frame_dim, frame_header.passes.num_passes);
  stream_images.resize(num_streams);
  if (cquality > 100) cquality = quality;

  // use a sensible default if nothing explicit is specified:
  // Squeeze for lossy, no squeeze for lossless
  if (cparams.responsive < 0) {
    if (quality == 100) {
      cparams.responsive = 0;
    } else {
      cparams.responsive = 1;
    }
  }

  if (cparams.speed_tier > SpeedTier::kWombat) {
    cparams.options.splitting_heuristics_node_threshold = 192;
  } else {
    cparams.options.splitting_heuristics_node_threshold = 96;
  }
  {
    // Set properties.
    std::vector<uint32_t> prop_order;
    if (cparams.responsive) {
      // Properties in order of their likelyhood of being useful for Squeeze
      // residuals.
      prop_order = {0, 1, 4, 5, 6, 7, 8, 15, 9, 10, 11, 12, 13, 14, 2, 3};
    } else {
      // Same, but for the non-Squeeze case.
      prop_order = {0, 1, 15, 9, 10, 11, 12, 13, 14, 2, 3, 4, 5, 6, 7, 8};
    }
    switch (cparams.speed_tier) {
      case SpeedTier::kSquirrel:
        cparams.options.splitting_heuristics_properties.assign(
            prop_order.begin(), prop_order.begin() + 8);
        cparams.options.max_property_values = 32;
        break;
      case SpeedTier::kKitten:
        cparams.options.splitting_heuristics_properties.assign(
            prop_order.begin(), prop_order.begin() + 10);
        cparams.options.max_property_values = 64;
        break;
      case SpeedTier::kTortoise:
        cparams.options.splitting_heuristics_properties = prop_order;
        cparams.options.max_property_values = 256;
        break;
      default:
        cparams.options.splitting_heuristics_properties.assign(
            prop_order.begin(), prop_order.begin() + 6);
        cparams.options.max_property_values = 16;
        break;
    }
    if (cparams.speed_tier > SpeedTier::kTortoise) {
      // Gradient in previous channels.
      for (int i = 0; i < cparams.options.max_properties; i++) {
        cparams.options.splitting_heuristics_properties.push_back(
            kNumNonrefProperties + i * 4 + 3);
      }
    } else {
      // All the extra properties in Tortoise mode.
      for (int i = 0; i < cparams.options.max_properties * 4; i++) {
        cparams.options.splitting_heuristics_properties.push_back(
            kNumNonrefProperties + i);
      }
    }
  }

  if (cparams.options.predictor == static_cast<Predictor>(-1)) {
    // no explicit predictor(s) given, set a good default
    if ((cparams.speed_tier <= SpeedTier::kTortoise ||
         cparams.modular_mode == false) &&
        quality == 100 && cparams.near_lossless == false &&
        cparams.responsive == false) {
      // TODO(veluca): allow all predictors that don't break residual
      // multipliers in lossy mode.
      cparams.options.predictor = Predictor::Variable;
    } else if (cparams.near_lossless) {
      // weighted predictor for near_lossless
      cparams.options.predictor = Predictor::Weighted;
    } else if (cparams.responsive) {
      // zero predictor for Squeeze residues
      cparams.options.predictor = Predictor::Zero;
    } else if (quality < 100) {
      // If not responsive and lossy. TODO(veluca): use near_lossless instead?
      cparams.options.predictor = Predictor::Gradient;
    } else if (cparams.speed_tier < SpeedTier::kFalcon) {
      // try median and weighted predictor for anything else
      cparams.options.predictor = Predictor::Best;
    } else {
      // just weighted predictor in fastest mode
      cparams.options.predictor = Predictor::Weighted;
    }
  }
  tree_splits.push_back(0);
  if (cparams.modular_mode == false) {
    cparams.options.fast_decode_multiplier = 1.0f;
    tree_splits.push_back(ModularStreamId::VarDCTDC(0).ID(frame_dim));
    tree_splits.push_back(ModularStreamId::ModularDC(0).ID(frame_dim));
    tree_splits.push_back(ModularStreamId::ACMetadata(0).ID(frame_dim));
    tree_splits.push_back(ModularStreamId::QuantTable(0).ID(frame_dim));
    tree_splits.push_back(ModularStreamId::ModularAC(0, 0).ID(frame_dim));
    ac_metadata_size.resize(frame_dim.num_dc_groups);
    extra_dc_precision.resize(frame_dim.num_dc_groups);
  }
  tree_splits.push_back(num_streams);
  cparams.options.max_chan_size = frame_dim.group_dim;

  // TODO(veluca): figure out how to use different predictor sets per channel.
  stream_options.resize(num_streams, cparams.options);
}

Status ModularFrameEncoder::ComputeEncodingData(
    const FrameHeader& frame_header, const ImageBundle& ib,
    Image3F* JXL_RESTRICT color, PassesEncoderState* JXL_RESTRICT enc_state,
    ThreadPool* pool, AuxOut* aux_out, bool do_color) {
  const FrameDimensions& frame_dim = enc_state->shared.frame_dim;

  if (do_color && frame_header.loop_filter.gab) {
    GaborishInverse(color, 0.9908511000000001f, pool);
  }

  if (do_color && cparams.speed_tier < SpeedTier::kCheetah) {
    FindBestPatchDictionary(*color, enc_state, nullptr, nullptr,
                            cparams.color_transform == ColorTransform::kXYB);
    PatchDictionaryEncoder::SubtractFrom(
        enc_state->shared.image_features.patches, color);
  }

  // Convert ImageBundle to modular Image object
  const size_t xsize = std::min(color->xsize(), ib.xsize());
  const size_t ysize = std::min(color->ysize(), ib.ysize());

  int nb_chans = 3;
  if (ib.IsGray() && cparams.color_transform == ColorTransform::kNone) {
    nb_chans = 1;
  }
  if (!do_color) nb_chans = 0;

  if (ib.HasExtraChannels()) {
    nb_chans += ib.extra_channels().size();
  }

  bool fp = ib.metadata()->bit_depth.floating_point_sample;

  // bits_per_sample is just metadata for XYB images.
  if (ib.metadata()->bit_depth.bits_per_sample >= 32 && do_color &&
      cparams.color_transform != ColorTransform::kXYB) {
    if (ib.metadata()->bit_depth.bits_per_sample == 32 && fp == false) {
      return JXL_FAILURE("uint32_t not supported in enc_modular");
    } else if (ib.metadata()->bit_depth.bits_per_sample > 32) {
      return JXL_FAILURE("bits_per_sample > 32 not supported");
    }
  }

  int maxval = (fp ? 1
                   : (1u << static_cast<uint32_t>(
                          ib.metadata()->bit_depth.bits_per_sample)) -
                         1);

  Image& gi = stream_images[0];
  gi = Image(xsize, ysize, maxval, nb_chans);
  int c = 0;
  if (cparams.color_transform == ColorTransform::kXYB &&
      cparams.modular_mode == true) {
    static const float enc_factors[3] = {32768.0f, 2048.0f, 2048.0f};
    DequantMatricesSetCustomDC(&enc_state->shared.matrices, enc_factors);
  }
  if (do_color) {
    for (; c < 3; c++) {
      if (ib.IsGray() && cparams.color_transform == ColorTransform::kNone &&
          c != (cparams.color_transform == ColorTransform::kXYB ? 1 : 0))
        continue;
      int c_out = c;
      // XYB is encoded as YX(B-Y)
      if (cparams.color_transform == ColorTransform::kXYB && c < 2)
        c_out = 1 - c_out;
      float factor = maxval;
      if (cparams.color_transform == ColorTransform::kXYB)
        factor = enc_state->shared.matrices.InvDCQuant(c);
      if (c == 2 && cparams.color_transform == ColorTransform::kXYB) {
        JXL_ASSERT(!fp);
        for (size_t y = 0; y < ysize; ++y) {
          const float* const JXL_RESTRICT row_in = color->PlaneRow(c, y);
          pixel_type* const JXL_RESTRICT row_out = gi.channel[c_out].Row(y);
          pixel_type* const JXL_RESTRICT row_Y = gi.channel[0].Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            row_out[x] = row_in[x] * factor + 0.5f;
            row_out[x] -= row_Y[x];
          }
        }
      } else {
        int bits = ib.metadata()->bit_depth.bits_per_sample;
        int exp_bits = ib.metadata()->bit_depth.exponent_bits_per_sample;
        for (size_t y = 0; y < ysize; ++y) {
          const float* const JXL_RESTRICT row_in = color->PlaneRow(c, y);
          pixel_type* const JXL_RESTRICT row_out = gi.channel[c_out].Row(y);
          JXL_RETURN_IF_ERROR(
              float_to_int(row_in, row_out, xsize, bits, exp_bits, fp, factor));
        }
      }
    }
    if (ib.IsGray() && cparams.color_transform == ColorTransform::kNone) c = 1;
  }
  if (ib.HasExtraChannels()) {
    for (size_t ec = 0; ec < ib.extra_channels().size(); ec++, c++) {
      const ExtraChannelInfo& eci = ib.metadata()->extra_channel_info[ec];
      gi.channel[c].resize(eci.Size(ib.xsize()), eci.Size(ib.ysize()));
      gi.channel[c].hshift = gi.channel[c].vshift = eci.dim_shift;

      int bits = eci.bit_depth.bits_per_sample;
      int exp_bits = eci.bit_depth.exponent_bits_per_sample;
      bool fp = eci.bit_depth.floating_point_sample;
      float factor = (fp ? 1 : ((1u << eci.bit_depth.bits_per_sample) - 1));
      for (size_t y = 0; y < ysize; ++y) {
        const float* const JXL_RESTRICT row_in = ib.extra_channels()[ec].Row(y);
        pixel_type* const JXL_RESTRICT row_out = gi.channel[c].Row(y);
        JXL_RETURN_IF_ERROR(
            float_to_int(row_in, row_out, xsize, bits, exp_bits, fp, factor));
      }
    }
  }
  JXL_ASSERT(c == nb_chans);

  // Set options and apply transformations

  if (quality < 100 || cparams.near_lossless) {
    if (cparams.palette_colors != 0) {
      JXL_DEBUG_V(3, "Lossy encode, not doing palette transforms");
    }
    if (cparams.color_transform == ColorTransform::kXYB) {
      cparams.channel_colors_pre_transform_percent = 0;
    }
    cparams.channel_colors_percent = 0;
    cparams.palette_colors = 0;
  }

  // if few colors, do all-channel palette before trying channel palette
  // Logic is as follows:
  // - if you can make a palette with few colors (arbitrary threshold: 200),
  //   then you can also make channel palettes, but they will just be extra
  //   signaling cost for almost no benefit
  // - if the palette needs more colors, then channel palette might help to
  //   reduce palette signaling cost
  if (cparams.palette_colors != 0 && cparams.speed_tier < SpeedTier::kFalcon) {
    // all-channel palette (e.g. RGBA)
    if (gi.nb_channels > 1) {
      Transform maybe_palette(TransformId::kPalette);
      maybe_palette.begin_c = gi.nb_meta_channels;
      maybe_palette.num_c = gi.nb_channels;
      maybe_palette.nb_colors =
          std::min(std::min(200, (int)(xsize * ysize / 8)),
                   std::abs(cparams.palette_colors) / 16);
      maybe_palette.ordered_palette = cparams.palette_colors >= 0;
      maybe_palette.lossy_palette = false;
      gi.do_transform(maybe_palette, weighted::Header());
    }
  }

  // Global channel palette
  if (cparams.channel_colors_pre_transform_percent > 0 &&
      !cparams.lossy_palette) {
    // single channel palette (like FLIF's ChannelCompact)
    for (size_t i = 0; i < gi.nb_channels; i++) {
      int min, max;
      gi.channel[gi.nb_meta_channels + i].compute_minmax(&min, &max);
      int64_t colors = max - min + 1;
      JXL_DEBUG_V(10, "Channel %zu: range=%i..%i", i, min, max);
      Transform maybe_palette_1(TransformId::kPalette);
      maybe_palette_1.begin_c = i + gi.nb_meta_channels;
      maybe_palette_1.num_c = 1;
      // simple heuristic: if less than X percent of the values in the range
      // actually occur, it is probably worth it to do a compaction
      // (but only if the channel palette is less than 6% the size of the
      // image itself)
      maybe_palette_1.nb_colors = std::min(
          (int)(xsize * ysize / 16),
          (int)(cparams.channel_colors_pre_transform_percent / 100. * colors));
      if (gi.do_transform(maybe_palette_1, weighted::Header())) {
        // effective bit depth is lower, adjust quantization accordingly
        gi.channel[gi.nb_meta_channels + i].compute_minmax(&min, &max);
        if (max < maxval) maxval = max;
      }
    }
  }

  // Global palette
  if ((cparams.palette_colors != 0 || cparams.lossy_palette) &&
      cparams.speed_tier < SpeedTier::kFalcon) {
    // all-channel palette (e.g. RGBA)
    if (gi.nb_channels > 1) {
      Transform maybe_palette(TransformId::kPalette);
      maybe_palette.begin_c = gi.nb_meta_channels;
      maybe_palette.num_c = gi.nb_channels;
      maybe_palette.nb_colors =
          std::min((int)(xsize * ysize / 8), std::abs(cparams.palette_colors));
      maybe_palette.ordered_palette = cparams.palette_colors >= 0;
      maybe_palette.lossy_palette =
          (cparams.lossy_palette && gi.nb_channels == 3);
      if (maybe_palette.lossy_palette) {
        maybe_palette.predictor = Predictor::Average4;
      }
      // TODO(veluca): use a custom weighted header if using the weighted
      // predictor.
      gi.do_transform(maybe_palette, weighted::Header());
    }
    // all-minus-one-channel palette (RGB with separate alpha, or CMY with
    // separate K)
    if (gi.nb_channels > 3) {
      Transform maybe_palette_3(TransformId::kPalette);
      maybe_palette_3.begin_c = gi.nb_meta_channels;
      maybe_palette_3.num_c = gi.nb_channels - 1;
      maybe_palette_3.nb_colors =
          std::min((int)(xsize * ysize / 8), std::abs(cparams.palette_colors));
      maybe_palette_3.ordered_palette = cparams.palette_colors >= 0;
      maybe_palette_3.lossy_palette = cparams.lossy_palette;
      if (maybe_palette_3.lossy_palette) {
        maybe_palette_3.predictor = Predictor::Average4;
      }
      gi.do_transform(maybe_palette_3, weighted::Header());
    }
  }

  if (cparams.color_transform == ColorTransform::kNone && do_color && !fp) {
    if (cparams.colorspace == 1 ||
        (cparams.colorspace < 0 && (quality < 100 || cparams.near_lossless ||
                                    cparams.speed_tier > SpeedTier::kHare))) {
      Transform ycocg{TransformId::kRCT};
      ycocg.rct_type = 6;
      ycocg.begin_c = gi.nb_meta_channels;
      gi.do_transform(ycocg, weighted::Header());
    } else if (cparams.colorspace >= 2) {
      Transform sg(TransformId::kRCT);
      sg.begin_c = gi.nb_meta_channels;
      sg.rct_type = cparams.colorspace - 2;
      gi.do_transform(sg, weighted::Header());
    }
  }

  if (cparams.responsive && gi.nb_channels != 0) {
    gi.do_transform(Transform(TransformId::kSqueeze),
                    weighted::Header());  // use default squeezing
  }

  std::vector<uint32_t> quants;

  if (quality < 100 || cquality < 100) {
    quants.resize(gi.channel.size(), 1);
    JXL_DEBUG_V(
        2,
        "Adding quantization constants corresponding to luma quality %.2f "
        "and chroma quality %.2f",
        quality, cquality);
    if (!cparams.responsive) {
      JXL_DEBUG_V(1,
                  "Warning: lossy compression without Squeeze "
                  "transform is just color quantization.");
      quality = (400 + quality) / 5;
      cquality = (400 + cquality) / 5;
    }

    // convert 'quality' to quantization scaling factor
    if (quality > 50) {
      quality = 200.0 - quality * 2.0;
    } else {
      quality = 900.0 - quality * 16.0;
    }
    if (cquality > 50) {
      cquality = 200.0 - cquality * 2.0;
    } else {
      cquality = 900.0 - cquality * 16.0;
    }
    if (cparams.color_transform != ColorTransform::kXYB) {
      quality *= 0.01f * maxval / 255.f;
      cquality *= 0.01f * maxval / 255.f;
    } else {
      quality *= 0.01f;
      cquality *= 0.01f;
    }

    if (cparams.options.nb_repeats == 0) {
      return JXL_FAILURE("nb_repeats = 0 not supported with modular lossy!");
    }
    for (uint32_t i = gi.nb_meta_channels; i < gi.channel.size(); i++) {
      Channel& ch = gi.channel[i];
      int shift = ch.hcshift + ch.vcshift;  // number of pixel halvings
      if (shift > 15) shift = 15;
      int q;
      // assuming default Squeeze here
      int component = ((i - gi.nb_meta_channels) % gi.real_nb_channels);
      // last 4 channels are final chroma residuals
      if (gi.real_nb_channels > 2 && i >= gi.channel.size() - 4) {
        component = 1;
      }

      if (cparams.color_transform == ColorTransform::kXYB && component < 3) {
        q = (component == 0 ? quality : cquality) * squeeze_quality_factor_xyb *
            squeeze_xyb_qtable[component][shift];
      } else {
        if (cparams.colorspace != 0 && component > 0 && component < 3) {
          q = cquality * squeeze_quality_factor * squeeze_chroma_qtable[shift];
        } else {
          q = quality * squeeze_quality_factor * squeeze_luma_factor *
              squeeze_luma_qtable[shift];
        }
      }
      if (q < 1) q = 1;
      QuantizeChannel(gi.channel[i], q);
      quants[i] = q;
    }
  }

  // Fill other groups.
  struct GroupParams {
    Rect rect;
    int minShift;
    int maxShift;
    ModularStreamId id;
  };
  std::vector<GroupParams> stream_params;

  stream_options[0] = cparams.options;

  // DC
  for (size_t group_id = 0; group_id < frame_dim.num_dc_groups; group_id++) {
    const size_t gx = group_id % frame_dim.xsize_dc_groups;
    const size_t gy = group_id / frame_dim.xsize_dc_groups;
    const Rect rect(gx * frame_dim.group_dim << 3,
                    gy * frame_dim.group_dim << 3, frame_dim.group_dim << 3,
                    frame_dim.group_dim << 3);
    // minShift==3 because kDcGroupDim>>3 == frame_dim.group_dim
    // maxShift==1000 is infinity
    stream_params.push_back(
        GroupParams{rect, 3, 1000, ModularStreamId::ModularDC(group_id)});
  }
  // AC global -> nothing.
  // AC
  for (size_t group_id = 0; group_id < frame_dim.num_groups; group_id++) {
    const size_t gx = group_id % frame_dim.xsize_groups;
    const size_t gy = group_id / frame_dim.xsize_groups;
    const Rect mrect(gx * frame_dim.group_dim, gy * frame_dim.group_dim,
                     frame_dim.group_dim, frame_dim.group_dim);
    for (size_t i = 0; i < enc_state->progressive_splitter.GetNumPasses();
         i++) {
      int maxShift, minShift;
      frame_header.passes.GetDownsamplingBracket(i, minShift, maxShift);
      stream_params.push_back(GroupParams{
          mrect, minShift, maxShift, ModularStreamId::ModularAC(group_id, i)});
    }
  }
  gi_channel.resize(stream_images.size());

  RunOnPool(
      pool, 0, stream_params.size(), ThreadPool::SkipInit(),
      [&](size_t i, size_t _) {
        stream_options[stream_params[i].id.ID(frame_dim)] = cparams.options;
        JXL_CHECK(PrepareStreamParams(
            stream_params[i].rect, cparams, stream_params[i].minShift,
            stream_params[i].maxShift, stream_params[i].id, do_color));
      },
      "ChooseParams");
  {
    // Clear out channels that have been copied to groups.
    Image& full_image = stream_images[0];
    size_t c = full_image.nb_meta_channels;
    for (; c < full_image.channel.size(); c++) {
      Channel& fc = full_image.channel[c];
      if (fc.w > frame_dim.group_dim || fc.h > frame_dim.group_dim) break;
    }
    for (; c < full_image.channel.size(); c++) {
      full_image.channel[c].plane = ImageI();
    }
  }

  if (!quants.empty()) {
    for (uint32_t stream_id = 0; stream_id < stream_images.size();
         stream_id++) {
      // skip non-modular stream_ids
      if (stream_id > 0 && gi_channel[stream_id].empty()) continue;
      Image& image = stream_images[stream_id];
      const ModularOptions& options = stream_options[stream_id];
      for (uint32_t i = image.nb_meta_channels; i < image.channel.size(); i++) {
        if (i >= image.nb_meta_channels &&
            (image.channel[i].w > options.max_chan_size ||
             image.channel[i].h > options.max_chan_size)) {
          continue;
        }
        if (stream_id > 0 && gi_channel[stream_id].empty()) continue;
        size_t ch_id = stream_id == 0
                           ? i
                           : gi_channel[stream_id][i - image.nb_meta_channels];
        uint32_t q = quants[ch_id];
        // Inform the tree splitting heuristics that each channel in each group
        // used this quantization factor. This will produce a tree with the
        // given multipliers.
        if (multiplier_info.empty() ||
            multiplier_info.back().range[1][0] != stream_id ||
            multiplier_info.back().multiplier != q) {
          StaticPropRange range;
          range[0] = {i, i + 1};
          range[1] = {stream_id, stream_id + 1};
          multiplier_info.push_back({range, (uint32_t)q});
        } else {
          // Previous channel in the same group had the same quantization
          // factor. Don't provide two different ranges, as that creates
          // unnecessary nodes.
          multiplier_info.back().range[0][1] = i + 1;
        }
      }
    }
    // Merge group+channel settings that have the same channels and quantization
    // factors, to avoid unnecessary nodes.
    std::sort(multiplier_info.begin(), multiplier_info.end(),
              [](ModularMultiplierInfo a, ModularMultiplierInfo b) {
                return std::make_tuple(a.range, a.multiplier) <
                       std::make_tuple(b.range, b.multiplier);
              });
    size_t new_num = 1;
    for (size_t i = 1; i < multiplier_info.size(); i++) {
      ModularMultiplierInfo& prev = multiplier_info[new_num - 1];
      ModularMultiplierInfo& cur = multiplier_info[i];
      if (prev.range[0] == cur.range[0] && prev.multiplier == cur.multiplier &&
          prev.range[1][1] == cur.range[1][0]) {
        prev.range[1][1] = cur.range[1][1];
      } else {
        multiplier_info[new_num++] = multiplier_info[i];
      }
    }
    multiplier_info.resize(new_num);
  }

  return PrepareEncoding(pool, enc_state->shared.frame_dim,
                         enc_state->heuristics.get(), aux_out);
}

Status ModularFrameEncoder::PrepareEncoding(ThreadPool* pool,
                                            const FrameDimensions& frame_dim,
                                            EncoderHeuristics* heuristics,
                                            AuxOut* aux_out) {
  if (!tree.empty()) return true;

  // Compute tree.
  size_t num_streams = stream_images.size();
  stream_headers.resize(num_streams);
  tokens.resize(num_streams);

  if (heuristics->CustomFixedTreeLossless(frame_dim, &tree)) {
    // Using a fixed tree.
  } else if (cparams.speed_tier != SpeedTier::kFalcon || quality != 100 ||
             !cparams.modular_mode) {
    // Avoid creating a tree with leaves that don't correspond to any pixels.
    std::vector<size_t> useful_splits;
    useful_splits.reserve(tree_splits.size());
    for (size_t chunk = 0; chunk < tree_splits.size() - 1; chunk++) {
      bool has_pixels = false;
      size_t start = tree_splits[chunk];
      size_t stop = tree_splits[chunk + 1];
      for (size_t i = start; i < stop; i++) {
        for (const Channel& c : stream_images[i].channel) {
          if (c.w && c.h) has_pixels = true;
        }
      }
      if (has_pixels) {
        useful_splits.push_back(tree_splits[chunk]);
      }
    }
    // Don't do anything if modular mode does not have any pixels in this image
    if (useful_splits.empty()) return true;
    useful_splits.push_back(tree_splits.back());

    std::atomic_flag invalid_force_wp = ATOMIC_FLAG_INIT;

    std::vector<Tree> trees(useful_splits.size() - 1);
    RunOnPool(
        pool, 0, useful_splits.size() - 1, ThreadPool::SkipInit(),
        [&](size_t chunk, size_t _) {
          // TODO(veluca): parallelize more.
          size_t total_pixels = 0;
          uint32_t start = useful_splits[chunk];
          uint32_t stop = useful_splits[chunk + 1];
          uint32_t max_c = 0;
          if (stream_options[start].tree_kind !=
              ModularOptions::TreeKind::kLearn) {
            for (size_t i = start; i < stop; i++) {
              for (const Channel& ch : stream_images[i].channel) {
                total_pixels += ch.w * ch.h;
              }
            }
            trees[chunk] =
                PredefinedTree(stream_options[start].tree_kind, total_pixels);
            return;
          }
          TreeSamples tree_samples;
          if (!tree_samples.SetPredictor(stream_options[start].predictor,
                                         stream_options[start].wp_tree_mode)) {
            invalid_force_wp.test_and_set(std::memory_order_acq_rel);
            return;
          }
          if (!tree_samples.SetProperties(
                  stream_options[start].splitting_heuristics_properties,
                  stream_options[start].wp_tree_mode)) {
            invalid_force_wp.test_and_set(std::memory_order_acq_rel);
            return;
          }
          std::vector<pixel_type> pixel_samples;
          std::vector<pixel_type> diff_samples;
          std::vector<uint32_t> group_pixel_count;
          std::vector<uint32_t> channel_pixel_count;
          for (size_t i = start; i < stop; i++) {
            max_c = std::max<uint32_t>(stream_images[i].channel.size(), max_c);
            CollectPixelSamples(stream_images[i], stream_options[i], i,
                                group_pixel_count, channel_pixel_count,
                                pixel_samples, diff_samples);
          }
          StaticPropRange range;
          range[0] = {0, max_c};
          range[1] = {start, stop};
          auto local_multiplier_info = multiplier_info;

          tree_samples.PreQuantizeProperties(
              range, local_multiplier_info, group_pixel_count,
              channel_pixel_count, pixel_samples, diff_samples,
              stream_options[start].max_property_values);
          for (size_t i = start; i < stop; i++) {
            JXL_CHECK(ModularGenericCompress(
                stream_images[i], stream_options[i], /*writer=*/nullptr,
                /*aux_out=*/nullptr, 0, i, &tree_samples, &total_pixels));
          }

          // TODO(veluca): parallelize more.
          trees[chunk] =
              LearnTree(std::move(tree_samples), total_pixels,
                        stream_options[start], local_multiplier_info, range);
        },
        "LearnTrees");
    if (invalid_force_wp.test_and_set(std::memory_order_acq_rel)) {
      return JXL_FAILURE("PrepareEncoding: force_no_wp with {Weighted}");
    }
    tree.clear();
    MergeTrees(trees, useful_splits, 0, useful_splits.size() - 1, &tree);
  } else {
    // Fixed tree.
    // TODO(veluca): determine cutoffs?
    std::vector<int32_t> cutoffs = {-255, -191, -127, -95, -63, -47, -31, -23,
                                    -15,  -11,  -7,   -5,  -3,  -1,  0,   1,
                                    3,    5,    7,    11,  15,  23,  31,  47,
                                    63,   95,   127,  191, 255};
    size_t total_pixels = 0;
    for (const Image& img : stream_images) {
      for (const Channel& ch : img.channel) {
        total_pixels += ch.w * ch.h;
      }
    }
    tree = MakeFixedTree(kNumNonrefProperties - weighted::kNumProperties,
                         cutoffs, Predictor::Weighted, total_pixels);
  }
  // TODO(veluca): do this somewhere else.
  if (cparams.near_lossless) {
    for (size_t i = 0; i < tree.size(); i++) {
      tree[i].predictor_offset = 0;
    }
  }
  tree_tokens.resize(1);
  tree_tokens[0].clear();
  Tree decoded_tree;
  TokenizeTree(tree, &tree_tokens[0], &decoded_tree);
  JXL_ASSERT(tree.size() == decoded_tree.size());
  tree = std::move(decoded_tree);

  if (WantDebugOutput(aux_out)) {
    PrintTree(tree, aux_out->debug_prefix + "/global_tree");
  }

  image_widths.resize(num_streams);
  RunOnPool(
      pool, 0, num_streams, ThreadPool::SkipInit(),
      [&](size_t stream_id, size_t _) {
        AuxOut my_aux_out;
        if (aux_out) {
          my_aux_out.dump_image = aux_out->dump_image;
          my_aux_out.debug_prefix = aux_out->debug_prefix;
        }
        tokens[stream_id].clear();
        JXL_CHECK(ModularGenericCompress(
            stream_images[stream_id], stream_options[stream_id],
            /*writer=*/nullptr, &my_aux_out, 0, stream_id,
            /*tree_samples=*/nullptr,
            /*total_pixels=*/nullptr,
            /*tree=*/&tree, /*header=*/&stream_headers[stream_id],
            /*tokens=*/&tokens[stream_id],
            /*widths=*/&image_widths[stream_id]));
      },
      "ComputeTokens");
  return true;
}

Status ModularFrameEncoder::EncodeGlobalInfo(BitWriter* writer,
                                             AuxOut* aux_out) {
  BitWriter::Allotment allotment(writer, 1);
  // If we are using brotli, or not using modular mode.
  if (tree_tokens.empty() || tree_tokens[0].empty()) {
    writer->Write(1, 0);
    ReclaimAndCharge(writer, &allotment, kLayerModularTree, aux_out);
    return true;
  }
  writer->Write(1, 1);
  ReclaimAndCharge(writer, &allotment, kLayerModularTree, aux_out);

  // Write tree
  HistogramParams params;
  if (cparams.speed_tier > SpeedTier::kKitten) {
    params.clustering = HistogramParams::ClusteringType::kFast;
    params.ans_histogram_strategy =
        HistogramParams::ANSHistogramStrategy::kApproximate;
    params.lz77_method = HistogramParams::LZ77Method::kNone;
    // Near-lossless DC, as well as modular mode, require choosing hybrid uint
    // more carefully.
    if ((!extra_dc_precision.empty() && extra_dc_precision[0] != 0) ||
        (cparams.modular_mode && cparams.speed_tier < SpeedTier::kCheetah)) {
      params.uint_method = HistogramParams::HybridUintMethod::kFast;
    } else {
      params.uint_method = HistogramParams::HybridUintMethod::kNone;
    }
  } else if (cparams.speed_tier <= SpeedTier::kTortoise) {
    params.lz77_method = HistogramParams::LZ77Method::kOptimal;
  } else {
    params.lz77_method = HistogramParams::LZ77Method::kLZ77;
  }
  if (cparams.decoding_speed_tier >= 1) {
    params.max_histograms = 12;
  }
  BuildAndEncodeHistograms(params, kNumTreeContexts, tree_tokens, &code,
                           &context_map, writer, kLayerModularTree, aux_out);
  WriteTokens(tree_tokens[0], code, context_map, writer, kLayerModularTree,
              aux_out);
  params.image_widths = image_widths;
  // Write histograms.
  BuildAndEncodeHistograms(params, (tree.size() + 1) / 2, tokens, &code,
                           &context_map, writer, kLayerModularGlobal, aux_out);
  return true;
}

Status ModularFrameEncoder::EncodeStream(BitWriter* writer, AuxOut* aux_out,
                                         size_t layer,
                                         const ModularStreamId& stream) {
  size_t stream_id = stream.ID(frame_dim);
  if (stream_images[stream_id].real_nb_channels < 1) {
    return true;  // Image with no channels, header never gets decoded.
  }
  JXL_RETURN_IF_ERROR(
      Bundle::Write(stream_headers[stream_id], writer, layer, aux_out));
  WriteTokens(tokens[stream_id], code, context_map, writer, layer, aux_out);
  return true;
}

namespace {
float EstimateWPCost(const Image& img, size_t i) {
  size_t extra_bits = 0;
  float histo_cost = 0;
  HybridUintConfig config;
  int32_t cutoffs[] = {-500, -392, -255, -191, -127, -95, -63, -47, -31,
                       -23,  -15,  -11,  -7,   -4,   -3,  -1,  0,   1,
                       3,    5,    7,    11,   15,   23,  31,  47,  63,
                       95,   127,  191,  255,  392,  500};
  constexpr size_t nc = sizeof(cutoffs) / sizeof(*cutoffs) + 1;
  Histogram histo[nc] = {};
  weighted::Header wp_header;
  PredictorMode(i, &wp_header);
  for (const Channel& ch : img.channel) {
    const intptr_t onerow = ch.plane.PixelsPerRow();
    weighted::State wp_state(wp_header, ch.w, ch.h);
    Properties properties(1);
    for (size_t y = 0; y < ch.h; y++) {
      const pixel_type* JXL_RESTRICT r = ch.Row(y);
      for (size_t x = 0; x < ch.w; x++) {
        size_t offset = 0;
        pixel_type_w left = (x ? r[x - 1] : y ? *(r + x - onerow) : 0);
        pixel_type_w top = (y ? *(r + x - onerow) : left);
        pixel_type_w topleft = (x && y ? *(r + x - 1 - onerow) : left);
        pixel_type_w topright =
            (x + 1 < ch.w && y ? *(r + x + 1 - onerow) : top);
        pixel_type_w toptop = (y > 1 ? *(r + x - onerow - onerow) : top);
        pixel_type guess = wp_state.Predict</*compute_properties=*/true>(
            x, y, ch.w, top, left, topright, topleft, toptop, &properties,
            offset);
        size_t ctx = 0;
        for (int c : cutoffs) {
          ctx += c >= properties[0];
        }
        pixel_type res = r[x] - guess;
        uint32_t token, nbits, bits;
        config.Encode(PackSigned(res), &token, &nbits, &bits);
        histo[ctx].Add(token);
        extra_bits += nbits;
        wp_state.UpdateErrors(r[x], x, y, ch.w);
      }
    }
    for (size_t h = 0; h < nc; h++) {
      histo_cost += histo[h].ShannonEntropy();
      histo[h].Clear();
    }
  }
  return histo_cost + extra_bits;
}

float EstimateCost(const Image& img) {
  // TODO(veluca): consider SIMDfication of this code.
  size_t extra_bits = 0;
  float histo_cost = 0;
  HybridUintConfig config;
  uint32_t cutoffs[] = {0,  1,  3,  5,   7,   11,  15,  23, 31,
                        47, 63, 95, 127, 191, 255, 392, 500};
  constexpr size_t nc = sizeof(cutoffs) / sizeof(*cutoffs) + 1;
  Histogram histo[nc] = {};
  for (const Channel& ch : img.channel) {
    const intptr_t onerow = ch.plane.PixelsPerRow();
    for (size_t y = 0; y < ch.h; y++) {
      const pixel_type* JXL_RESTRICT r = ch.Row(y);
      for (size_t x = 0; x < ch.w; x++) {
        pixel_type_w left = (x ? r[x - 1] : y ? *(r + x - onerow) : 0);
        pixel_type_w top = (y ? *(r + x - onerow) : left);
        pixel_type_w topleft = (x && y ? *(r + x - 1 - onerow) : left);
        size_t maxdiff = std::max(std::max(left, top), topleft) -
                         std::min(std::min(left, top), topleft);
        size_t ctx = 0;
        for (uint32_t c : cutoffs) {
          ctx += c > maxdiff;
        }
        pixel_type res = r[x] - ClampedGradient(top, left, topleft);
        uint32_t token, nbits, bits;
        config.Encode(PackSigned(res), &token, &nbits, &bits);
        histo[ctx].Add(token);
        extra_bits += nbits;
      }
    }
    for (size_t h = 0; h < nc; h++) {
      histo_cost += histo[h].ShannonEntropy();
      histo[h].Clear();
    }
  }
  return histo_cost + extra_bits;
}

}  // namespace

Status ModularFrameEncoder::PrepareStreamParams(const Rect& rect,
                                                const CompressParams& cparams,
                                                int minShift, int maxShift,
                                                const ModularStreamId& stream,
                                                bool do_color) {
  size_t stream_id = stream.ID(frame_dim);
  JXL_ASSERT(stream_id != 0);
  Image& full_image = stream_images[0];
  const size_t xsize = rect.xsize();
  const size_t ysize = rect.ysize();
  int maxval = full_image.maxval;
  Image& gi = stream_images[stream_id];
  gi = Image(xsize, ysize, maxval, 0);
  // start at the first bigger-than-frame_dim.group_dim non-metachannel
  size_t c = full_image.nb_meta_channels;
  for (; c < full_image.channel.size(); c++) {
    Channel& fc = full_image.channel[c];
    if (fc.w > frame_dim.group_dim || fc.h > frame_dim.group_dim) break;
  }
  for (; c < full_image.channel.size(); c++) {
    Channel& fc = full_image.channel[c];
    int shift = std::min(fc.hshift, fc.vshift);
    if (shift > maxShift) continue;
    if (shift < minShift) continue;
    Rect r(rect.x0() >> fc.hshift, rect.y0() >> fc.vshift,
           rect.xsize() >> fc.hshift, rect.ysize() >> fc.vshift, fc.w, fc.h);
    if (r.xsize() == 0 || r.ysize() == 0) continue;
    gi_channel[stream_id].push_back(c);
    Channel gc(r.xsize(), r.ysize());
    gc.hshift = fc.hshift;
    gc.vshift = fc.vshift;
    for (size_t y = 0; y < r.ysize(); ++y) {
      const pixel_type* const JXL_RESTRICT row_in = r.ConstRow(fc.plane, y);
      pixel_type* const JXL_RESTRICT row_out = gc.Row(y);
      for (size_t x = 0; x < r.xsize(); ++x) {
        row_out[x] = row_in[x];
      }
    }
    gi.channel.emplace_back(std::move(gc));
  }
  gi.nb_channels = gi.channel.size();
  gi.real_nb_channels = gi.nb_channels;

  // Do some per-group transforms

  float quality = cparams.quality_pair.first;

  // Local palette
  // TODO(veluca): make this work with quantize-after-prediction in lossy mode.
  if (quality == 100 && cparams.palette_colors != 0 &&
      cparams.speed_tier < SpeedTier::kCheetah) {
    // all-channel palette (e.g. RGBA)
    if (gi.nb_channels > 1) {
      Transform maybe_palette(TransformId::kPalette);
      maybe_palette.begin_c = gi.nb_meta_channels;
      maybe_palette.num_c = gi.nb_channels;
      maybe_palette.nb_colors = std::abs(cparams.palette_colors);
      maybe_palette.ordered_palette = cparams.palette_colors >= 0;
      gi.do_transform(maybe_palette, weighted::Header());
    }
    // all-minus-one-channel palette (RGB with separate alpha, or CMY with
    // separate K)
    if (gi.nb_channels > 3) {
      Transform maybe_palette_3(TransformId::kPalette);
      maybe_palette_3.begin_c = gi.nb_meta_channels;
      maybe_palette_3.num_c = gi.nb_channels - 1;
      maybe_palette_3.nb_colors = std::abs(cparams.palette_colors);
      maybe_palette_3.ordered_palette = cparams.palette_colors >= 0;
      maybe_palette_3.lossy_palette = cparams.lossy_palette;
      if (maybe_palette_3.lossy_palette) {
        maybe_palette_3.predictor = Predictor::Weighted;
      }
      gi.do_transform(maybe_palette_3, weighted::Header());
    }
  }

  // Local channel palette
  if (cparams.channel_colors_percent > 0 && quality == 100 &&
      !cparams.lossy_palette && cparams.speed_tier < SpeedTier::kCheetah) {
    // single channel palette (like FLIF's ChannelCompact)
    for (size_t i = 0; i < gi.nb_channels; i++) {
      int min, max;
      gi.channel[gi.nb_meta_channels + i].compute_minmax(&min, &max);
      int colors = max - min + 1;
      JXL_DEBUG_V(10, "Channel %zu: range=%i..%i", i, min, max);
      Transform maybe_palette_1(TransformId::kPalette);
      maybe_palette_1.begin_c = i + gi.nb_meta_channels;
      maybe_palette_1.num_c = 1;
      // simple heuristic: if less than X percent of the values in the range
      // actually occur, it is probably worth it to do a compaction
      // (but only if the channel palette is less than 80% the size of the
      // image itself)
      maybe_palette_1.nb_colors =
          std::min((int)(xsize * ysize * 0.8),
                   (int)(cparams.channel_colors_percent / 100. * colors));
      gi.do_transform(maybe_palette_1, weighted::Header());
    }
  }
  if (cparams.near_lossless > 0 && gi.nb_channels != 0) {
    Transform nl(TransformId::kNearLossless);
    nl.predictor = cparams.options.predictor;
    JXL_RETURN_IF_ERROR(nl.predictor != Predictor::Best);
    JXL_RETURN_IF_ERROR(nl.predictor != Predictor::Variable);
    nl.begin_c = gi.nb_meta_channels;
    if (cparams.colorspace == 0) {
      nl.num_c = gi.nb_channels;
      nl.max_delta_error = cparams.near_lossless;
      gi.do_transform(nl, weighted::Header());
    } else {
      nl.num_c = 1;
      nl.max_delta_error = cparams.near_lossless;
      gi.do_transform(nl, weighted::Header());
      nl.begin_c += 1;
      nl.num_c = gi.nb_channels - 1;
      nl.max_delta_error++;  // more loss for chroma
      gi.do_transform(nl, weighted::Header());
    }
  }

  // lossless and no specific color transform specified: try Nothing, YCoCg,
  // and 17 RCTs
  if (cparams.color_transform == ColorTransform::kNone && quality == 100 &&
      cparams.colorspace < 0 && gi.nb_channels > 2 && !cparams.near_lossless &&
      cparams.responsive == false && do_color &&
      cparams.speed_tier <= SpeedTier::kHare) {
    Transform sg(TransformId::kRCT);
    sg.begin_c = gi.nb_meta_channels;

    size_t nb_rcts_to_try = 0;
    switch (cparams.speed_tier) {
      case SpeedTier::kFalcon:
        nb_rcts_to_try = 0;  // Just do global YCoCg
        break;
      case SpeedTier::kCheetah:
        nb_rcts_to_try = 0;  // Just do global YCoCg
        break;
      case SpeedTier::kHare:
        nb_rcts_to_try = 4;
        break;
      case SpeedTier::kWombat:
        nb_rcts_to_try = 5;
        break;
      case SpeedTier::kSquirrel:
        nb_rcts_to_try = 7;
        break;
      case SpeedTier::kKitten:
        nb_rcts_to_try = 9;
        break;
      case SpeedTier::kTortoise:
        nb_rcts_to_try = 19;
        break;
    }
    float best_cost = std::numeric_limits<float>::max();
    size_t best_rct = 0;
    // These should be 19 actually different transforms; the remaining ones
    // are equivalent to one of these (note that the first two are do-nothing
    // and YCoCg) modulo channel reordering (which only matters in the case of
    // MA-with-prev-channels-properties) and/or sign (e.g. RmG vs GmR)
    for (int i : {0 * 7 + 0, 0 * 7 + 6, 0 * 7 + 5, 1 * 7 + 3, 3 * 7 + 5,
                  5 * 7 + 5, 1 * 7 + 5, 2 * 7 + 5, 1 * 7 + 1, 0 * 7 + 4,
                  1 * 7 + 2, 2 * 7 + 1, 2 * 7 + 2, 2 * 7 + 3, 4 * 7 + 4,
                  4 * 7 + 5, 0 * 7 + 2, 0 * 7 + 1, 0 * 7 + 3}) {
      if (nb_rcts_to_try == 0) break;
      int num_transforms_to_keep = gi.transform.size();
      sg.rct_type = i;
      gi.do_transform(sg, weighted::Header());
      float cost = EstimateCost(gi);
      if (cost < best_cost) {
        best_rct = i;
        best_cost = cost;
      }
      nb_rcts_to_try--;
      // Ensure we do not clamp channels to their supposed range, as this
      // otherwise breaks in the presence of patches.
      gi.undo_transforms(weighted::Header(), num_transforms_to_keep == 0
                                                 ? -1
                                                 : num_transforms_to_keep);
    }
    // Apply the best RCT to the image for future encoding.
    sg.rct_type = best_rct;
    gi.do_transform(sg, weighted::Header());
  } else {
    // No need to try anything, just use the default options.
  }
  size_t nb_wp_modes = 0;
  switch (cparams.speed_tier) {
    case SpeedTier::kFalcon:
      nb_wp_modes = 1;
      break;
    case SpeedTier::kCheetah:
      nb_wp_modes = 1;
      break;
    case SpeedTier::kHare:
      nb_wp_modes = 1;
      break;
    case SpeedTier::kWombat:
      nb_wp_modes = 1;
      break;
    case SpeedTier::kSquirrel:
      nb_wp_modes = 1;
      break;
    case SpeedTier::kKitten:
      nb_wp_modes = 2;
      break;
    case SpeedTier::kTortoise:
      nb_wp_modes = 5;
      break;
  }
  if (nb_wp_modes > 1 &&
      (stream_options[stream_id].predictor == Predictor::Weighted ||
       stream_options[stream_id].predictor == Predictor::Best ||
       stream_options[stream_id].predictor == Predictor::Variable)) {
    float best_cost = std::numeric_limits<float>::max();
    stream_options[stream_id].wp_mode = 0;
    for (size_t i = 0; i < nb_wp_modes; i++) {
      float cost = EstimateWPCost(gi, i);
      if (cost < best_cost) {
        best_cost = cost;
        stream_options[stream_id].wp_mode = i;
      }
    }
  }
  return true;
}

int QuantizeWP(const int32_t* qrow, size_t onerow, size_t c, size_t x, size_t y,
               size_t w, weighted::State* wp_state, float value,
               float inv_factor) {
  float svalue = value * inv_factor;
  PredictionResult pred =
      PredictNoTreeWP(w, qrow + x, onerow, x, y, Predictor::Weighted, wp_state);
  svalue -= pred.guess;
  int residual = roundf(svalue);
  if (residual > 2 || residual < -2) residual = roundf(svalue * 0.5) * 2;
  return residual + pred.guess;
}

int QuantizeGradient(const int32_t* qrow, size_t onerow, size_t c, size_t x,
                     size_t y, size_t w, float value, float inv_factor) {
  float svalue = value * inv_factor;
  PredictionResult pred =
      PredictNoTreeNoWP(w, qrow + x, onerow, x, y, Predictor::Gradient);
  svalue -= pred.guess;
  int residual = roundf(svalue);
  if (residual > 2 || residual < -2) residual = roundf(svalue * 0.5) * 2;
  return residual + pred.guess;
}

void ModularFrameEncoder::AddVarDCTDC(const Image3F& dc, size_t group_index,
                                      bool nl_dc,
                                      PassesEncoderState* enc_state) {
  const Rect r = enc_state->shared.DCGroupRect(group_index);
  extra_dc_precision[group_index] = nl_dc ? 1 : 0;
  float mul = 1 << extra_dc_precision[group_index];

  size_t stream_id = ModularStreamId::VarDCTDC(group_index).ID(frame_dim);
  stream_options[stream_id].max_chan_size = 0xFFFFFF;
  stream_options[stream_id].predictor = Predictor::Weighted;
  stream_options[stream_id].wp_tree_mode = ModularOptions::WPTreeMode::kWPOnly;
  if (cparams.speed_tier >= SpeedTier::kSquirrel) {
    stream_options[stream_id].tree_kind = ModularOptions::TreeKind::kWPFixedDC;
  }
  if (cparams.decoding_speed_tier >= 1) {
    stream_options[stream_id].tree_kind =
        ModularOptions::TreeKind::kGradientFixedDC;
  }

  stream_images[stream_id] = Image(r.xsize(), r.ysize(), 255, 3);
  if (nl_dc && stream_options[stream_id].tree_kind ==
                   ModularOptions::TreeKind::kGradientFixedDC) {
    JXL_ASSERT(enc_state->shared.frame_header.chroma_subsampling.Is444());
    for (size_t c : {1, 0, 2}) {
      float inv_factor = enc_state->shared.quantizer.GetInvDcStep(c) * mul;
      float y_factor = enc_state->shared.quantizer.GetDcStep(1) / mul;
      float cfl_factor = enc_state->shared.cmap.DCFactors()[c];
      for (size_t y = 0; y < r.ysize(); y++) {
        int32_t* quant_row =
            stream_images[stream_id].channel[c < 2 ? c ^ 1 : c].plane.Row(y);
        size_t stride = stream_images[stream_id]
                            .channel[c < 2 ? c ^ 1 : c]
                            .plane.PixelsPerRow();
        const float* row = r.ConstPlaneRow(dc, c, y);
        if (c == 1) {
          for (size_t x = 0; x < r.xsize(); x++) {
            quant_row[x] = QuantizeGradient(quant_row, stride, c, x, y,
                                            r.xsize(), row[x], inv_factor);
          }
        } else {
          int32_t* quant_row_y =
              stream_images[stream_id].channel[0].plane.Row(y);
          for (size_t x = 0; x < r.xsize(); x++) {
            quant_row[x] = QuantizeGradient(
                quant_row, stride, c, x, y, r.xsize(),
                row[x] - quant_row_y[x] * (y_factor * cfl_factor), inv_factor);
          }
        }
      }
    }
  } else if (nl_dc) {
    JXL_ASSERT(enc_state->shared.frame_header.chroma_subsampling.Is444());
    for (size_t c : {1, 0, 2}) {
      float inv_factor = enc_state->shared.quantizer.GetInvDcStep(c) * mul;
      float y_factor = enc_state->shared.quantizer.GetDcStep(1) / mul;
      float cfl_factor = enc_state->shared.cmap.DCFactors()[c];
      weighted::Header header;
      weighted::State wp_state(header, r.xsize(), r.ysize());
      for (size_t y = 0; y < r.ysize(); y++) {
        int32_t* quant_row =
            stream_images[stream_id].channel[c < 2 ? c ^ 1 : c].plane.Row(y);
        size_t stride = stream_images[stream_id]
                            .channel[c < 2 ? c ^ 1 : c]
                            .plane.PixelsPerRow();
        const float* row = r.ConstPlaneRow(dc, c, y);
        if (c == 1) {
          for (size_t x = 0; x < r.xsize(); x++) {
            quant_row[x] = QuantizeWP(quant_row, stride, c, x, y, r.xsize(),
                                      &wp_state, row[x], inv_factor);
            wp_state.UpdateErrors(quant_row[x], x, y, r.xsize());
          }
        } else {
          int32_t* quant_row_y =
              stream_images[stream_id].channel[0].plane.Row(y);
          for (size_t x = 0; x < r.xsize(); x++) {
            quant_row[x] = QuantizeWP(
                quant_row, stride, c, x, y, r.xsize(), &wp_state,
                row[x] - quant_row_y[x] * (y_factor * cfl_factor), inv_factor);
            wp_state.UpdateErrors(quant_row[x], x, y, r.xsize());
          }
        }
      }
    }
  } else if (enc_state->shared.frame_header.chroma_subsampling.Is444()) {
    for (size_t c : {1, 0, 2}) {
      float inv_factor = enc_state->shared.quantizer.GetInvDcStep(c) * mul;
      float y_factor = enc_state->shared.quantizer.GetDcStep(1) / mul;
      float cfl_factor = enc_state->shared.cmap.DCFactors()[c];
      for (size_t y = 0; y < r.ysize(); y++) {
        int32_t* quant_row =
            stream_images[stream_id].channel[c < 2 ? c ^ 1 : c].plane.Row(y);
        const float* row = r.ConstPlaneRow(dc, c, y);
        if (c == 1) {
          for (size_t x = 0; x < r.xsize(); x++) {
            quant_row[x] = roundf(row[x] * inv_factor);
          }
        } else {
          int32_t* quant_row_y =
              stream_images[stream_id].channel[0].plane.Row(y);
          for (size_t x = 0; x < r.xsize(); x++) {
            quant_row[x] =
                roundf((row[x] - quant_row_y[x] * (y_factor * cfl_factor)) *
                       inv_factor);
          }
        }
      }
    }
  } else {
    for (size_t c : {1, 0, 2}) {
      Rect rect(
          r.x0() >> enc_state->shared.frame_header.chroma_subsampling.HShift(c),
          r.y0() >> enc_state->shared.frame_header.chroma_subsampling.VShift(c),
          r.xsize() >>
              enc_state->shared.frame_header.chroma_subsampling.HShift(c),
          r.ysize() >>
              enc_state->shared.frame_header.chroma_subsampling.VShift(c));
      float inv_factor = enc_state->shared.quantizer.GetInvDcStep(c) * mul;
      size_t ys = rect.ysize();
      size_t xs = rect.xsize();
      Channel& ch = stream_images[stream_id].channel[c < 2 ? c ^ 1 : c];
      ch.w = xs;
      ch.h = ys;
      ch.resize();
      for (size_t y = 0; y < ys; y++) {
        int32_t* quant_row = ch.plane.Row(y);
        const float* row = rect.ConstPlaneRow(dc, c, y);
        for (size_t x = 0; x < xs; x++) {
          quant_row[x] = roundf(row[x] * inv_factor);
        }
      }
    }
  }

  DequantDC(r, &enc_state->shared.dc_storage, &enc_state->shared.quant_dc,
            stream_images[stream_id], enc_state->shared.quantizer.MulDC(),
            1.0 / mul, enc_state->shared.cmap.DCFactors(),
            enc_state->shared.frame_header.chroma_subsampling,
            enc_state->shared.block_ctx_map);
}

void ModularFrameEncoder::AddACMetadata(size_t group_index, bool jpeg_transcode,
                                        PassesEncoderState* enc_state) {
  const Rect r = enc_state->shared.DCGroupRect(group_index);
  size_t stream_id = ModularStreamId::ACMetadata(group_index).ID(frame_dim);
  stream_options[stream_id].max_chan_size = 0xFFFFFF;
  stream_options[stream_id].wp_tree_mode = ModularOptions::WPTreeMode::kNoWP;
  if (jpeg_transcode) {
    stream_options[stream_id].tree_kind =
        ModularOptions::TreeKind::kJpegTranscodeACMeta;
  } else if (cparams.speed_tier == SpeedTier::kFalcon) {
    stream_options[stream_id].tree_kind =
        ModularOptions::TreeKind::kFalconACMeta;
  } else if (cparams.speed_tier > SpeedTier::kKitten) {
    stream_options[stream_id].tree_kind = ModularOptions::TreeKind::kACMeta;
  }
  // If we are using a non-constant CfL field, and are in a slow enough mode,
  // re-enable tree computation for it.
  if (cparams.speed_tier < SpeedTier::kSquirrel &&
      cparams.force_cfl_jpeg_recompression) {
    stream_options[stream_id].tree_kind = ModularOptions::TreeKind::kLearn;
  }
  // YToX, YToB, ACS + QF, EPF
  Image& image = stream_images[stream_id];
  image = Image(r.xsize(), r.ysize(), 255, 4);
  static_assert(kColorTileDimInBlocks == 8, "Color tile size changed");
  Rect cr(r.x0() >> 3, r.y0() >> 3, (r.xsize() + 7) >> 3, (r.ysize() + 7) >> 3);
  image.channel[0] = Channel(cr.xsize(), cr.ysize(), 3, 3);
  image.channel[1] = Channel(cr.xsize(), cr.ysize(), 3, 3);
  image.channel[2] = Channel(r.xsize() * r.ysize(), 2, 0, 0);
  ConvertPlaneAndClamp(cr, enc_state->shared.cmap.ytox_map,
                       Rect(image.channel[0].plane), &image.channel[0].plane);
  ConvertPlaneAndClamp(cr, enc_state->shared.cmap.ytob_map,
                       Rect(image.channel[1].plane), &image.channel[1].plane);
  size_t num = 0;
  for (size_t y = 0; y < r.ysize(); y++) {
    AcStrategyRow row_acs = enc_state->shared.ac_strategy.ConstRow(r, y);
    const int* row_qf = r.ConstRow(enc_state->shared.raw_quant_field, y);
    const uint8_t* row_epf = r.ConstRow(enc_state->shared.epf_sharpness, y);
    int* out_acs = image.channel[2].plane.Row(0);
    int* out_qf = image.channel[2].plane.Row(1);
    int* row_out_epf = image.channel[3].plane.Row(y);
    for (size_t x = 0; x < r.xsize(); x++) {
      row_out_epf[x] = row_epf[x];
      if (!row_acs[x].IsFirstBlock()) continue;
      out_acs[num] = row_acs[x].RawStrategy();
      out_qf[num] = row_qf[x] - 1;
      num++;
    }
  }
  image.channel[2].w = num;
  image.channel[2].resize();
  ac_metadata_size[group_index] = num;
}

void ModularFrameEncoder::EncodeQuantTable(
    size_t size_x, size_t size_y, BitWriter* writer,
    const QuantEncoding& encoding, size_t idx,
    ModularFrameEncoder* modular_frame_encoder) {
  JXL_ASSERT(encoding.qraw.qtable != nullptr);
  JXL_ASSERT(size_x * size_y * 3 == encoding.qraw.qtable->size());
  JXL_CHECK(F16Coder::Write(encoding.qraw.qtable_den, writer));
  if (modular_frame_encoder) {
    JXL_CHECK(modular_frame_encoder->EncodeStream(
        writer, nullptr, 0, ModularStreamId::QuantTable(idx)));
    return;
  }
  Image image(size_x, size_y, 255, 3);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < size_y; y++) {
      int* JXL_RESTRICT row = image.channel[c].Row(y);
      for (size_t x = 0; x < size_x; x++) {
        row[x] = (*encoding.qraw.qtable)[c * size_x * size_y + y * size_x + x];
      }
    }
  }
  ModularOptions cfopts;
  JXL_CHECK(ModularGenericCompress(image, cfopts, writer));
}

void ModularFrameEncoder::AddQuantTable(size_t size_x, size_t size_y,
                                        const QuantEncoding& encoding,
                                        size_t idx) {
  size_t stream_id = ModularStreamId::QuantTable(idx).ID(frame_dim);
  JXL_ASSERT(encoding.qraw.qtable != nullptr);
  JXL_ASSERT(size_x * size_y * 3 == encoding.qraw.qtable->size());
  Image& image = stream_images[stream_id];
  image = Image(size_x, size_y, 255, 3);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < size_y; y++) {
      int* JXL_RESTRICT row = image.channel[c].Row(y);
      for (size_t x = 0; x < size_x; x++) {
        row[x] = (*encoding.qraw.qtable)[c * size_x * size_y + y * size_x + x];
      }
    }
  }
}
}  // namespace jxl
