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

#include "lib/jxl/enc_heuristics.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <numeric>
#include <string>

#include "lib/jxl/enc_ac_strategy.h"
#include "lib/jxl/enc_adaptive_quantization.h"
#include "lib/jxl/enc_ar_control_field.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_chroma_from_luma.h"
#include "lib/jxl/enc_modular.h"
#include "lib/jxl/enc_noise.h"
#include "lib/jxl/enc_patch_dictionary.h"
#include "lib/jxl/enc_quant_weights.h"
#include "lib/jxl/enc_splines.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/gaborish.h"

namespace jxl {
namespace {
void FindBestBlockEntropyModel(PassesEncoderState& enc_state) {
  if (enc_state.cparams.decoding_speed_tier >= 1) {
    static constexpr uint8_t kSimpleCtxMap[] = {
        // Cluster all blocks together
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  //
    };
    static_assert(
        3 * kNumOrders == sizeof(kSimpleCtxMap) / sizeof *kSimpleCtxMap,
        "Update simple context map");

    auto bcm = enc_state.shared.block_ctx_map;
    bcm.ctx_map.assign(std::begin(kSimpleCtxMap), std::end(kSimpleCtxMap));
    bcm.num_ctxs = 2;
    bcm.num_dc_ctxs = 1;
    return;
  }
  if (enc_state.cparams.speed_tier == SpeedTier::kFalcon) {
    return;
  }
  const ImageI& rqf = enc_state.shared.raw_quant_field;
  // No need to change context modeling for small images.
  size_t tot = rqf.xsize() * rqf.ysize();
  size_t size_for_ctx_model =
      (1 << 10) * enc_state.cparams.butteraugli_distance;
  if (tot < size_for_ctx_model) return;

  struct OccCounters {
    // count the occurrences of each qf value and each strategy type.
    OccCounters(const ImageI& rqf, const AcStrategyImage& ac_strategy) {
      for (size_t y = 0; y < rqf.ysize(); y++) {
        const int32_t* qf_row = rqf.Row(y);
        AcStrategyRow acs_row = ac_strategy.ConstRow(y);
        for (size_t x = 0; x < rqf.xsize(); x++) {
          int ord = kStrategyOrder[acs_row[x].RawStrategy()];
          int qf = qf_row[x] - 1;
          qf_counts[qf]++;
          qf_ord_counts[ord][qf]++;
          ord_counts[ord]++;
        }
      }
    }

    size_t qf_counts[256] = {};
    size_t qf_ord_counts[kNumOrders][256] = {};
    size_t ord_counts[kNumOrders] = {};
  };
  // The OccCounters struct is too big to allocate on the stack.
  std::unique_ptr<OccCounters> counters(
      new OccCounters(rqf, enc_state.shared.ac_strategy));

  // Splitting the context model according to the quantization field seems to
  // mostly benefit only large images.
  size_t size_for_qf_split = (1 << 13) * enc_state.cparams.butteraugli_distance;
  size_t num_qf_segments = tot < size_for_qf_split ? 1 : 2;
  std::vector<uint32_t>& qft = enc_state.shared.block_ctx_map.qf_thresholds;
  qft.clear();
  // Divide the quant field in up to num_qf_segments segments.
  size_t cumsum = 0;
  size_t next = 1;
  size_t last_cut = 256;
  size_t cut = tot * next / num_qf_segments;
  for (uint32_t j = 0; j < 256; j++) {
    cumsum += counters->qf_counts[j];
    if (cumsum > cut) {
      if (j != 0) {
        qft.push_back(j);
      }
      last_cut = j;
      while (cumsum > cut) {
        next++;
        cut = tot * next / num_qf_segments;
      }
    } else if (next > qft.size() + 1) {
      if (j - 1 == last_cut && j != 0) {
        qft.push_back(j);
      }
    }
  }

  // Count the occurrences of each segment.
  std::vector<size_t> counts(kNumOrders * (qft.size() + 1));
  size_t qft_pos = 0;
  for (size_t j = 0; j < 256; j++) {
    if (qft_pos < qft.size() && j == qft[qft_pos]) {
      qft_pos++;
    }
    for (size_t i = 0; i < kNumOrders; i++) {
      counts[qft_pos + i * (qft.size() + 1)] += counters->qf_ord_counts[i][j];
    }
  }

  // Repeatedly merge the lowest-count pair.
  std::vector<uint8_t> remap((qft.size() + 1) * kNumOrders);
  std::iota(remap.begin(), remap.end(), 0);
  std::vector<uint8_t> clusters(remap);
  // This is O(n^2 log n), but n <= 14.
  while (clusters.size() > 5) {
    std::sort(clusters.begin(), clusters.end(),
              [&](int a, int b) { return counts[a] > counts[b]; });
    counts[clusters[clusters.size() - 2]] += counts[clusters.back()];
    counts[clusters.back()] = 0;
    remap[clusters.back()] = clusters[clusters.size() - 2];
    clusters.pop_back();
  }
  for (size_t i = 0; i < remap.size(); i++) {
    while (remap[remap[i]] != remap[i]) {
      remap[i] = remap[remap[i]];
    }
  }
  // Relabel starting from 0.
  std::vector<uint8_t> remap_remap(remap.size(), remap.size());
  size_t num = 0;
  for (size_t i = 0; i < remap.size(); i++) {
    if (remap_remap[remap[i]] == remap.size()) {
      remap_remap[remap[i]] = num++;
    }
    remap[i] = remap_remap[remap[i]];
  }
  // Write the block context map.
  auto& ctx_map = enc_state.shared.block_ctx_map.ctx_map;
  ctx_map = remap;
  ctx_map.resize(remap.size() * 3);
  for (size_t i = remap.size(); i < remap.size() * 3; i++) {
    ctx_map[i] = remap[i % remap.size()] + num;
  }
  enc_state.shared.block_ctx_map.num_ctxs =
      *std::max_element(ctx_map.begin(), ctx_map.end()) + 1;
}

// Returns the target size based on whether bitrate or direct targetsize is
// given.
size_t TargetSize(const CompressParams& cparams,
                  const FrameDimensions& frame_dim) {
  if (cparams.target_size > 0) {
    return cparams.target_size;
  }
  if (cparams.target_bitrate > 0.0) {
    return 0.5 + cparams.target_bitrate * frame_dim.xsize * frame_dim.ysize /
                     kBitsPerByte;
  }
  return 0;
}
}  // namespace

void FindBestDequantMatrices(const CompressParams& cparams,
                             const Image3F& opsin,
                             ModularFrameEncoder* modular_frame_encoder,
                             DequantMatrices* dequant_matrices) {
  // TODO(veluca): quant matrices for no-gaborish.
  // TODO(veluca): heuristics for in-bitstream quant tables.
  *dequant_matrices = DequantMatrices();
  if (cparams.max_error_mode) {
    // Set numerators of all quantization matrices to constant values.
    float weights[3][1] = {{1.0f / cparams.max_error[0]},
                           {1.0f / cparams.max_error[1]},
                           {1.0f / cparams.max_error[2]}};
    DctQuantWeightParams dct_params(weights);
    std::vector<QuantEncoding> encodings(DequantMatrices::kNum,
                                         QuantEncoding::DCT(dct_params));
    DequantMatricesSetCustom(dequant_matrices, encodings,
                             modular_frame_encoder);
    float dc_weights[3] = {1.0f / cparams.max_error[0],
                           1.0f / cparams.max_error[1],
                           1.0f / cparams.max_error[2]};
    DequantMatricesSetCustomDC(dequant_matrices, dc_weights);
  }
}

bool DefaultEncoderHeuristics::HandlesColorConversion(
    const CompressParams& cparams, const ImageBundle& ib) {
  return cparams.noise != Override::kOn && cparams.patches != Override::kOn &&
         cparams.speed_tier >= SpeedTier::kWombat && cparams.resampling == 1 &&
         cparams.color_transform == ColorTransform::kXYB &&
         !cparams.modular_mode && !ib.HasAlpha();
}

Status DefaultEncoderHeuristics::LossyFrameHeuristics(
    PassesEncoderState* enc_state, ModularFrameEncoder* modular_frame_encoder,
    const ImageBundle* original_pixels, Image3F* opsin, ThreadPool* pool,
    AuxOut* aux_out) {
  PROFILER_ZONE("JxlLossyFrameHeuristics uninstrumented");

  CompressParams& cparams = enc_state->cparams;
  PassesSharedState& shared = enc_state->shared;

  // Compute parameters for noise synthesis.
  if (shared.frame_header.flags & FrameHeader::kNoise) {
    PROFILER_ZONE("enc GetNoiseParam");
    // Don't start at zero amplitude since adding noise is expensive -- it
    // significantly slows down decoding, and this is unlikely to
    // completely go away even with advanced optimizations. After the
    // kNoiseModelingRampUpDistanceRange we have reached the full level,
    // i.e. noise is no longer represented by the compressed image, so we
    // can add full noise by the noise modeling itself.
    static const float kNoiseModelingRampUpDistanceRange = 0.6;
    static const float kNoiseLevelAtStartOfRampUp = 0.25;
    static const float kNoiseRampupStart = 1.0;
    // TODO(user) test and properly select quality_coef with smooth
    // filter
    float quality_coef = 1.0f;
    const float rampup = (cparams.butteraugli_distance - kNoiseRampupStart) /
                         kNoiseModelingRampUpDistanceRange;
    if (rampup < 1.0f) {
      quality_coef = kNoiseLevelAtStartOfRampUp +
                     (1.0f - kNoiseLevelAtStartOfRampUp) * rampup;
    }
    if (rampup < 0.0f) {
      quality_coef = kNoiseRampupStart;
    }
    if (!GetNoiseParameter(*opsin, &shared.image_features.noise_params,
                           quality_coef)) {
      shared.frame_header.flags &= ~FrameHeader::kNoise;
    }
  }
  if (cparams.resampling != 1) {
    // In VarDCT mode, LossyFrameHeuristics takes care of running downsampling
    // after noise, if necessary.
    DownsampleImage(opsin, cparams.resampling);
    PadImageToBlockMultipleInPlace(opsin);
  }

  const FrameDimensions& frame_dim = enc_state->shared.frame_dim;
  size_t target_size = TargetSize(cparams, frame_dim);
  size_t opsin_target_size = target_size;
  if (cparams.target_size > 0 || cparams.target_bitrate > 0.0) {
    cparams.target_size = opsin_target_size;
  } else if (cparams.butteraugli_distance < 0) {
    return JXL_FAILURE("Expected non-negative distance");
  }

  // Find and subtract splines.
  if (cparams.speed_tier <= SpeedTier::kSquirrel) {
    shared.image_features.splines = FindSplines(*opsin);
    JXL_RETURN_IF_ERROR(
        shared.image_features.splines.SubtractFrom(opsin, shared.cmap));
  }

  // Find and subtract patches/dots.
  if (ApplyOverride(cparams.patches,
                    cparams.speed_tier <= SpeedTier::kSquirrel)) {
    FindBestPatchDictionary(*opsin, enc_state, pool, aux_out);
    PatchDictionaryEncoder::SubtractFrom(shared.image_features.patches, opsin);
  }

  static const float kAcQuant = 0.79f;
  const float quant_dc = InitialQuantDC(cparams.butteraugli_distance);
  Quantizer& quantizer = enc_state->shared.quantizer;
  // We don't know the quant field yet, but for computing the global scale
  // assuming that it will be the same as for Falcon mode is good enough.
  quantizer.ComputeGlobalScaleAndQuant(
      quant_dc, kAcQuant / cparams.butteraugli_distance, 0);

  // TODO(veluca): we can now run all the code from here to FindBestQuantizer
  // (excluded) one rect at a time. Do that.

  // Dependency graph:
  //
  // input: either XYB or input image
  //
  // input image -> XYB [optional]
  // XYB -> initial quant field
  // XYB -> Gaborished XYB
  // Gaborished XYB -> CfL1
  // initial quant field, Gaborished XYB, CfL1 -> ACS
  // initial quant field, ACS, Gaborished XYB -> EPF control field
  // initial quant field -> adjusted initial quant field
  // adjusted initial quant field, ACS -> raw quant field
  // raw quant field, ACS, Gaborished XYB -> CfL2
  //
  // output: Gaborished XYB, CfL, ACS, raw quant field, EPF control field.

  ArControlFieldHeuristics ar_heuristics;
  AcStrategyHeuristics acs_heuristics;
  CfLHeuristics cfl_heuristics;

  if (!opsin->xsize()) {
    JXL_ASSERT(HandlesColorConversion(cparams, *original_pixels));
    *opsin = Image3F(RoundUpToBlockDim(original_pixels->xsize()),
                     RoundUpToBlockDim(original_pixels->ysize()));
    opsin->ShrinkTo(original_pixels->xsize(), original_pixels->ysize());
    ToXYB(*original_pixels, pool, opsin, /*linear=*/nullptr);
    PadImageToBlockMultipleInPlace(opsin);
  }

  // Compute an initial estimate of the quantization field.
  // Call InitialQuantField only in Hare mode or slower. Otherwise, rely
  // on simple heuristics in FindBestAcStrategy, or set a constant for Falcon
  // mode.
  if (cparams.speed_tier > SpeedTier::kHare || cparams.uniform_quant > 0) {
    enc_state->initial_quant_field =
        ImageF(shared.frame_dim.xsize_blocks, shared.frame_dim.ysize_blocks);
    if (cparams.speed_tier == SpeedTier::kFalcon || cparams.uniform_quant > 0) {
      float q = cparams.uniform_quant > 0
                    ? cparams.uniform_quant
                    : kAcQuant / cparams.butteraugli_distance;
      FillImage(q, &enc_state->initial_quant_field);
    }
  } else {
    // Call this here, as it relies on pre-gaborish values.
    float butteraugli_distance_for_iqf = cparams.butteraugli_distance;
    if (!shared.frame_header.loop_filter.gab) {
      butteraugli_distance_for_iqf *= 0.73f;
    }
    enc_state->initial_quant_field = InitialQuantField(
        butteraugli_distance_for_iqf, *opsin, shared.frame_dim, pool, 1.0f,
        &enc_state->initial_quant_masking);
  }

  // TODO(veluca): do something about animations.

  // Apply inverse-gaborish.
  if (shared.frame_header.loop_filter.gab) {
    GaborishInverse(opsin, 0.9908511000000001f, pool);
  }

  cfl_heuristics.Init(*opsin);
  acs_heuristics.Init(*opsin, enc_state);

  auto process_tile = [&](size_t tid, size_t thread) {
    size_t n_enc_tiles =
        DivCeil(enc_state->shared.frame_dim.xsize_blocks, kEncTileDimInBlocks);
    size_t tx = tid % n_enc_tiles;
    size_t ty = tid / n_enc_tiles;
    size_t by0 = ty * kEncTileDimInBlocks;
    size_t by1 = std::min((ty + 1) * kEncTileDimInBlocks,
                          enc_state->shared.frame_dim.ysize_blocks);
    size_t bx0 = tx * kEncTileDimInBlocks;
    size_t bx1 = std::min((tx + 1) * kEncTileDimInBlocks,
                          enc_state->shared.frame_dim.xsize_blocks);
    Rect r(bx0, by0, bx1 - bx0, by1 - by0);

    // For speeds up to Wombat, we only compute the color correlation map
    // once we know the transform type and the quantization map.
    if (cparams.speed_tier <= SpeedTier::kSquirrel) {
      cfl_heuristics.ComputeTile(r, *opsin, enc_state->shared.matrices,
                                 /*ac_strategy=*/nullptr,
                                 /*quantizer=*/nullptr, /*fast=*/false, thread,
                                 &enc_state->shared.cmap);
    }

    // Choose block sizes.
    acs_heuristics.ProcessRect(r);

    // Choose amount of post-processing smoothing.
    // TODO(veluca): should this go *after* AdjustQuantField?
    ar_heuristics.RunRect(r, *opsin, enc_state, thread);

    // Always set the initial quant field, so we can compute the CfL map with
    // more accuracy. The initial quant field might change in slower modes, but
    // adjusting the quant field with butteraugli when all the other encoding
    // parameters are fixed is likely a more reliable choice anyway.
    AdjustQuantField(enc_state->shared.ac_strategy, r,
                     &enc_state->initial_quant_field);
    quantizer.SetQuantFieldRect(enc_state->initial_quant_field, r,
                                &enc_state->shared.raw_quant_field);

    // Compute a non-default CfL map if we are at Hare speed, or slower.
    if (cparams.speed_tier <= SpeedTier::kHare) {
      cfl_heuristics.ComputeTile(
          r, *opsin, enc_state->shared.matrices, &enc_state->shared.ac_strategy,
          &enc_state->shared.quantizer,
          /*fast=*/cparams.speed_tier >= SpeedTier::kWombat, thread,
          &enc_state->shared.cmap);
    }
  };
  RunOnPool(
      pool, 0,
      DivCeil(enc_state->shared.frame_dim.xsize_blocks, kEncTileDimInBlocks) *
          DivCeil(enc_state->shared.frame_dim.ysize_blocks,
                  kEncTileDimInBlocks),
      [&](const size_t num_threads) {
        ar_heuristics.PrepareForThreads(num_threads);
        cfl_heuristics.PrepareForThreads(num_threads);
        return true;
      },
      process_tile, "Enc Heuristics");

  acs_heuristics.Finalize(aux_out);
  if (cparams.speed_tier <= SpeedTier::kHare) {
    cfl_heuristics.ComputeDC(/*fast=*/cparams.speed_tier >= SpeedTier::kWombat,
                             &enc_state->shared.cmap);
  }

  FindBestDequantMatrices(cparams, *opsin, modular_frame_encoder,
                          &enc_state->shared.matrices);

  // Refine quantization levels.
  FindBestQuantizer(original_pixels, *opsin, enc_state, pool, aux_out);

  // Choose a context model that depends on the amount of quantization for AC.
  if (cparams.speed_tier != SpeedTier::kFalcon) {
    FindBestBlockEntropyModel(*enc_state);
  }
  return true;
}

}  // namespace jxl
