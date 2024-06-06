// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_patch_dictionary.h"

#include <jxl/memory_manager.h>
#include <jxl/types.h>
#include <sys/types.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/dec_frame.h"
#include "lib/jxl/enc_ans.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_debug_image.h"
#include "lib/jxl/enc_dot_dictionary.h"
#include "lib/jxl/enc_frame.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/pack_signed.h"
#include "lib/jxl/patch_dictionary_internal.h"

namespace jxl {

static constexpr size_t kPatchFrameReferenceId = 3;

// static
void PatchDictionaryEncoder::Encode(const PatchDictionary& pdic,
                                    BitWriter* writer, size_t layer,
                                    AuxOut* aux_out) {
  JXL_ASSERT(pdic.HasAny());
  JxlMemoryManager* memory_manager = writer->memory_manager();
  std::vector<std::vector<Token>> tokens(1);

  auto add_num = [&](int context, size_t num) {
    tokens[0].emplace_back(context, num);
  };
  size_t num_ref_patch = 0;
  for (size_t i = 0; i < pdic.positions_.size();) {
    size_t ref_pos_idx = pdic.positions_[i].ref_pos_idx;
    while (i < pdic.positions_.size() &&
           pdic.positions_[i].ref_pos_idx == ref_pos_idx) {
      i++;
    }
    num_ref_patch++;
  }
  add_num(kNumRefPatchContext, num_ref_patch);
  size_t blend_pos = 0;
  size_t blending_stride = pdic.blendings_stride_;
  // blending_stride == num_ec + 1; num_ec > 1 =>
  bool choose_alpha = (blending_stride > 1 + 1);
  for (size_t i = 0; i < pdic.positions_.size();) {
    size_t i_start = i;
    size_t ref_pos_idx = pdic.positions_[i].ref_pos_idx;
    const auto& ref_pos = pdic.ref_positions_[ref_pos_idx];
    while (i < pdic.positions_.size() &&
           pdic.positions_[i].ref_pos_idx == ref_pos_idx) {
      i++;
    }
    size_t num = i - i_start;
    JXL_ASSERT(num > 0);
    add_num(kReferenceFrameContext, ref_pos.ref);
    add_num(kPatchReferencePositionContext, ref_pos.x0);
    add_num(kPatchReferencePositionContext, ref_pos.y0);
    add_num(kPatchSizeContext, ref_pos.xsize - 1);
    add_num(kPatchSizeContext, ref_pos.ysize - 1);
    add_num(kPatchCountContext, num - 1);
    for (size_t j = i_start; j < i; j++) {
      const PatchPosition& pos = pdic.positions_[j];
      if (j == i_start) {
        add_num(kPatchPositionContext, pos.x);
        add_num(kPatchPositionContext, pos.y);
      } else {
        add_num(kPatchOffsetContext,
                PackSigned(pos.x - pdic.positions_[j - 1].x));
        add_num(kPatchOffsetContext,
                PackSigned(pos.y - pdic.positions_[j - 1].y));
      }
      for (size_t j = 0; j < blending_stride; ++j, ++blend_pos) {
        const PatchBlending& info = pdic.blendings_[blend_pos];
        add_num(kPatchBlendModeContext, static_cast<uint32_t>(info.mode));
        if (UsesAlpha(info.mode) && choose_alpha) {
          add_num(kPatchAlphaChannelContext, info.alpha_channel);
        }
        if (UsesClamp(info.mode)) {
          add_num(kPatchClampContext, TO_JXL_BOOL(info.clamp));
        }
      }
    }
  }

  EntropyEncodingData codes;
  std::vector<uint8_t> context_map;
  BuildAndEncodeHistograms(memory_manager, HistogramParams(),
                           kNumPatchDictionaryContexts, tokens, &codes,
                           &context_map, writer, layer, aux_out);
  WriteTokens(tokens[0], codes, context_map, 0, writer, layer, aux_out);
}

// static
void PatchDictionaryEncoder::SubtractFrom(const PatchDictionary& pdic,
                                          Image3F* opsin) {
  // TODO(veluca): this can likely be optimized knowing it runs on full images.
  for (size_t y = 0; y < opsin->ysize(); y++) {
    float* JXL_RESTRICT rows[3] = {
        opsin->PlaneRow(0, y),
        opsin->PlaneRow(1, y),
        opsin->PlaneRow(2, y),
    };
    size_t blending_stride = pdic.blendings_stride_;
    for (size_t pos_idx : pdic.GetPatchesForRow(y)) {
      const size_t blending_idx = pos_idx * blending_stride;
      const PatchPosition& pos = pdic.positions_[pos_idx];
      const PatchReferencePosition& ref_pos =
          pdic.ref_positions_[pos.ref_pos_idx];
      const PatchBlendMode mode = pdic.blendings_[blending_idx].mode;
      size_t by = pos.y;
      size_t bx = pos.x;
      size_t xsize = ref_pos.xsize;
      JXL_DASSERT(y >= by);
      JXL_DASSERT(y < by + ref_pos.ysize);
      size_t iy = y - by;
      size_t ref = ref_pos.ref;
      const float* JXL_RESTRICT ref_rows[3] = {
          pdic.reference_frames_->at(ref).frame->color()->ConstPlaneRow(
              0, ref_pos.y0 + iy) +
              ref_pos.x0,
          pdic.reference_frames_->at(ref).frame->color()->ConstPlaneRow(
              1, ref_pos.y0 + iy) +
              ref_pos.x0,
          pdic.reference_frames_->at(ref).frame->color()->ConstPlaneRow(
              2, ref_pos.y0 + iy) +
              ref_pos.x0,
      };
      for (size_t ix = 0; ix < xsize; ix++) {
        for (size_t c = 0; c < 3; c++) {
          if (mode == PatchBlendMode::kAdd) {
            rows[c][bx + ix] -= ref_rows[c][ix];
          } else if (mode == PatchBlendMode::kReplace) {
            rows[c][bx + ix] = 0;
          } else if (mode == PatchBlendMode::kNone) {
            // Nothing to do.
          } else {
            JXL_UNREACHABLE("Blending mode %u not yet implemented",
                            static_cast<uint32_t>(mode));
          }
        }
      }
    }
  }
}

namespace {

struct PatchColorspaceInfo {
  float kChannelDequant[3];
  float kChannelWeights[3];

  explicit PatchColorspaceInfo(bool is_xyb) {
    if (is_xyb) {
      kChannelDequant[0] = 0.01615;
      kChannelDequant[1] = 0.08875;
      kChannelDequant[2] = 0.1922;
      kChannelWeights[0] = 30.0;
      kChannelWeights[1] = 3.0;
      kChannelWeights[2] = 1.0;
    } else {
      kChannelDequant[0] = 20.0f / 255;
      kChannelDequant[1] = 22.0f / 255;
      kChannelDequant[2] = 20.0f / 255;
      kChannelWeights[0] = 0.017 * 255;
      kChannelWeights[1] = 0.02 * 255;
      kChannelWeights[2] = 0.017 * 255;
    }
  }

  float ScaleForQuantization(float val, size_t c) {
    return val / kChannelDequant[c];
  }

  int Quantize(float val, size_t c) {
    return truncf(ScaleForQuantization(val, c));
  }

  bool is_similar_v(const float v1[3], const float v2[3], float threshold) {
    float distance = 0;
    for (size_t c = 0; c < 3; c++) {
      distance += std::fabs(v1[c] - v2[c]) * kChannelWeights[c];
    }
    return distance <= threshold;
  }
};

StatusOr<std::vector<PatchInfo>> FindTextLikePatches(
    const CompressParams& cparams, const Image3F& opsin,
    const PassesEncoderState* JXL_RESTRICT state, ThreadPool* pool,
    AuxOut* aux_out, bool is_xyb) {
  std::vector<PatchInfo> info;
  if (state->cparams.patches == Override::kOff) return info;
  const auto& frame_dim = state->shared.frame_dim;
  JxlMemoryManager* memory_manager = opsin.memory_manager();

  PatchColorspaceInfo pci(is_xyb);
  float kSimilarThreshold = 0.8f;

  auto is_similar_impl = [&pci](std::pair<uint32_t, uint32_t> p1,
                                std::pair<uint32_t, uint32_t> p2,
                                const float* JXL_RESTRICT rows[3],
                                size_t stride, float threshold) {
    float v1[3];
    float v2[3];
    for (size_t c = 0; c < 3; c++) {
      v1[c] = rows[c][p1.second * stride + p1.first];
      v2[c] = rows[c][p2.second * stride + p2.first];
    }
    return pci.is_similar_v(v1, v2, threshold);
  };

  std::atomic<bool> has_screenshot_areas{false};
  const size_t opsin_stride = opsin.PixelsPerRow();
  const float* JXL_RESTRICT opsin_rows[3] = {opsin.ConstPlaneRow(0, 0),
                                             opsin.ConstPlaneRow(1, 0),
                                             opsin.ConstPlaneRow(2, 0)};

  auto is_same = [&opsin_rows, opsin_stride](std::pair<uint32_t, uint32_t> p1,
                                             std::pair<uint32_t, uint32_t> p2) {
    for (auto& opsin_row : opsin_rows) {
      float v1 = opsin_row[p1.second * opsin_stride + p1.first];
      float v2 = opsin_row[p2.second * opsin_stride + p2.first];
      if (std::fabs(v1 - v2) > 1e-4) {
        return false;
      }
    }
    return true;
  };

  auto is_similar = [&](std::pair<uint32_t, uint32_t> p1,
                        std::pair<uint32_t, uint32_t> p2) {
    return is_similar_impl(p1, p2, opsin_rows, opsin_stride, kSimilarThreshold);
  };

  constexpr int64_t kPatchSide = 4;
  constexpr int64_t kExtraSide = 4;

  // Look for kPatchSide size squares, naturally aligned, that all have the same
  // pixel values.
  JXL_ASSIGN_OR_RETURN(
      ImageB is_screenshot_like,
      ImageB::Create(memory_manager, DivCeil(frame_dim.xsize, kPatchSide),
                     DivCeil(frame_dim.ysize, kPatchSide)));
  ZeroFillImage(&is_screenshot_like);
  uint8_t* JXL_RESTRICT screenshot_row = is_screenshot_like.Row(0);
  const size_t screenshot_stride = is_screenshot_like.PixelsPerRow();
  const auto process_row = [&](const uint32_t y, size_t /* thread */) {
    for (uint64_t x = 0; x < frame_dim.xsize / kPatchSide; x++) {
      bool all_same = true;
      for (size_t iy = 0; iy < static_cast<size_t>(kPatchSide); iy++) {
        for (size_t ix = 0; ix < static_cast<size_t>(kPatchSide); ix++) {
          size_t cx = x * kPatchSide + ix;
          size_t cy = y * kPatchSide + iy;
          if (!is_same({cx, cy}, {x * kPatchSide, y * kPatchSide})) {
            all_same = false;
            break;
          }
        }
      }
      if (!all_same) continue;
      size_t num = 0;
      size_t num_same = 0;
      for (int64_t iy = -kExtraSide; iy < kExtraSide + kPatchSide; iy++) {
        for (int64_t ix = -kExtraSide; ix < kExtraSide + kPatchSide; ix++) {
          int64_t cx = x * kPatchSide + ix;
          int64_t cy = y * kPatchSide + iy;
          if (cx < 0 || static_cast<uint64_t>(cx) >= frame_dim.xsize ||  //
              cy < 0 || static_cast<uint64_t>(cy) >= frame_dim.ysize) {
            continue;
          }
          num++;
          if (is_same({cx, cy}, {x * kPatchSide, y * kPatchSide})) num_same++;
        }
      }
      // Too few equal pixels nearby.
      if (num_same * 8 < num * 7) continue;
      screenshot_row[y * screenshot_stride + x] = 1;
      has_screenshot_areas = true;
    }
  };
  JXL_CHECK(RunOnPool(pool, 0, frame_dim.ysize / kPatchSide, ThreadPool::NoInit,
                      process_row, "IsScreenshotLike"));

  // TODO(veluca): also parallelize the rest of this function.
  if (WantDebugOutput(cparams)) {
    JXL_RETURN_IF_ERROR(
        DumpPlaneNormalized(cparams, "screenshot_like", is_screenshot_like));
  }

  constexpr int kSearchRadius = 1;

  if (!ApplyOverride(state->cparams.patches, has_screenshot_areas)) {
    return info;
  }

  // Search for "similar enough" pixels near the screenshot-like areas.
  JXL_ASSIGN_OR_RETURN(
      ImageB is_background,
      ImageB::Create(memory_manager, frame_dim.xsize, frame_dim.ysize));
  ZeroFillImage(&is_background);
  JXL_ASSIGN_OR_RETURN(
      Image3F background,
      Image3F::Create(memory_manager, frame_dim.xsize, frame_dim.ysize));
  ZeroFillImage(&background);
  constexpr size_t kDistanceLimit = 50;
  float* JXL_RESTRICT background_rows[3] = {
      background.PlaneRow(0, 0),
      background.PlaneRow(1, 0),
      background.PlaneRow(2, 0),
  };
  const size_t background_stride = background.PixelsPerRow();
  uint8_t* JXL_RESTRICT is_background_row = is_background.Row(0);
  const size_t is_background_stride = is_background.PixelsPerRow();
  std::vector<
      std::pair<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>>>
      queue;
  size_t queue_front = 0;
  for (size_t y = 0; y < frame_dim.ysize; y++) {
    for (size_t x = 0; x < frame_dim.xsize; x++) {
      if (!screenshot_row[screenshot_stride * (y / kPatchSide) +
                          (x / kPatchSide)])
        continue;
      queue.push_back({{x, y}, {x, y}});
    }
  }
  while (queue.size() != queue_front) {
    std::pair<uint32_t, uint32_t> cur = queue[queue_front].first;
    std::pair<uint32_t, uint32_t> src = queue[queue_front].second;
    queue_front++;
    if (is_background_row[cur.second * is_background_stride + cur.first])
      continue;
    is_background_row[cur.second * is_background_stride + cur.first] = 1;
    for (size_t c = 0; c < 3; c++) {
      background_rows[c][cur.second * background_stride + cur.first] =
          opsin_rows[c][src.second * opsin_stride + src.first];
    }
    for (int dx = -kSearchRadius; dx <= kSearchRadius; dx++) {
      for (int dy = -kSearchRadius; dy <= kSearchRadius; dy++) {
        if (dx == 0 && dy == 0) continue;
        int next_first = cur.first + dx;
        int next_second = cur.second + dy;
        if (next_first < 0 || next_second < 0 ||
            static_cast<uint32_t>(next_first) >= frame_dim.xsize ||
            static_cast<uint32_t>(next_second) >= frame_dim.ysize) {
          continue;
        }
        if (static_cast<uint32_t>(
                std::abs(next_first - static_cast<int>(src.first)) +
                std::abs(next_second - static_cast<int>(src.second))) >
            kDistanceLimit) {
          continue;
        }
        std::pair<uint32_t, uint32_t> next{next_first, next_second};
        if (is_similar(src, next)) {
          if (!screenshot_row[next.second / kPatchSide * screenshot_stride +
                              next.first / kPatchSide] ||
              is_same(src, next)) {
            if (!is_background_row[next.second * is_background_stride +
                                   next.first])
              queue.emplace_back(next, src);
          }
        }
      }
    }
  }
  queue.clear();

  ImageF ccs;
  Rng rng(0);
  bool paint_ccs = false;
  if (WantDebugOutput(cparams)) {
    JXL_RETURN_IF_ERROR(
        DumpPlaneNormalized(cparams, "is_background", is_background));
    if (is_xyb) {
      JXL_RETURN_IF_ERROR(DumpXybImage(cparams, "background", background));
    } else {
      JXL_RETURN_IF_ERROR(DumpImage(cparams, "background", background));
    }
    JXL_ASSIGN_OR_RETURN(
        ccs, ImageF::Create(memory_manager, frame_dim.xsize, frame_dim.ysize));
    ZeroFillImage(&ccs);
    paint_ccs = true;
  }

  constexpr float kVerySimilarThreshold = 0.03f;
  constexpr float kHasSimilarThreshold = 0.03f;

  const float* JXL_RESTRICT const_background_rows[3] = {
      background_rows[0], background_rows[1], background_rows[2]};
  auto is_similar_b = [&](std::pair<int, int> p1, std::pair<int, int> p2) {
    return is_similar_impl(p1, p2, const_background_rows, background_stride,
                           kVerySimilarThreshold);
  };

  constexpr int kMinPeak = 2;
  constexpr int kHasSimilarRadius = 2;

  // Find small CC outside the "similar enough" areas, compute bounding boxes,
  // and run heuristics to exclude some patches.
  JXL_ASSIGN_OR_RETURN(
      ImageB visited,
      ImageB::Create(memory_manager, frame_dim.xsize, frame_dim.ysize));
  ZeroFillImage(&visited);
  uint8_t* JXL_RESTRICT visited_row = visited.Row(0);
  const size_t visited_stride = visited.PixelsPerRow();
  std::vector<std::pair<uint32_t, uint32_t>> cc;
  std::vector<std::pair<uint32_t, uint32_t>> stack;
  for (size_t y = 0; y < frame_dim.ysize; y++) {
    for (size_t x = 0; x < frame_dim.xsize; x++) {
      if (is_background_row[y * is_background_stride + x]) continue;
      cc.clear();
      stack.clear();
      stack.emplace_back(x, y);
      size_t min_x = x;
      size_t max_x = x;
      size_t min_y = y;
      size_t max_y = y;
      std::pair<uint32_t, uint32_t> reference;
      bool found_border = false;
      bool all_similar = true;
      while (!stack.empty()) {
        std::pair<uint32_t, uint32_t> cur = stack.back();
        stack.pop_back();
        if (visited_row[cur.second * visited_stride + cur.first]) continue;
        visited_row[cur.second * visited_stride + cur.first] = 1;
        if (cur.first < min_x) min_x = cur.first;
        if (cur.first > max_x) max_x = cur.first;
        if (cur.second < min_y) min_y = cur.second;
        if (cur.second > max_y) max_y = cur.second;
        if (paint_ccs) {
          cc.push_back(cur);
        }
        for (int dx = -kSearchRadius; dx <= kSearchRadius; dx++) {
          for (int dy = -kSearchRadius; dy <= kSearchRadius; dy++) {
            if (dx == 0 && dy == 0) continue;
            int next_first = static_cast<int32_t>(cur.first) + dx;
            int next_second = static_cast<int32_t>(cur.second) + dy;
            if (next_first < 0 || next_second < 0 ||
                static_cast<uint32_t>(next_first) >= frame_dim.xsize ||
                static_cast<uint32_t>(next_second) >= frame_dim.ysize) {
              continue;
            }
            std::pair<uint32_t, uint32_t> next{next_first, next_second};
            if (!is_background_row[next.second * is_background_stride +
                                   next.first]) {
              stack.push_back(next);
            } else {
              if (!found_border) {
                reference = next;
                found_border = true;
              } else {
                if (!is_similar_b(next, reference)) all_similar = false;
              }
            }
          }
        }
      }
      if (!found_border || !all_similar || max_x - min_x >= kMaxPatchSize ||
          max_y - min_y >= kMaxPatchSize) {
        continue;
      }
      size_t bpos = background_stride * reference.second + reference.first;
      float ref[3] = {background_rows[0][bpos], background_rows[1][bpos],
                      background_rows[2][bpos]};
      bool has_similar = false;
      for (size_t iy = std::max<int>(
               static_cast<int32_t>(min_y) - kHasSimilarRadius, 0);
           iy < std::min(max_y + kHasSimilarRadius + 1, frame_dim.ysize);
           iy++) {
        for (size_t ix = std::max<int>(
                 static_cast<int32_t>(min_x) - kHasSimilarRadius, 0);
             ix < std::min(max_x + kHasSimilarRadius + 1, frame_dim.xsize);
             ix++) {
          size_t opos = opsin_stride * iy + ix;
          float px[3] = {opsin_rows[0][opos], opsin_rows[1][opos],
                         opsin_rows[2][opos]};
          if (pci.is_similar_v(ref, px, kHasSimilarThreshold)) {
            has_similar = true;
          }
        }
      }
      if (!has_similar) continue;
      info.emplace_back();
      info.back().second.emplace_back(min_x, min_y);
      QuantizedPatch& patch = info.back().first;
      patch.xsize = max_x - min_x + 1;
      patch.ysize = max_y - min_y + 1;
      int max_value = 0;
      for (size_t c : {1, 0, 2}) {
        for (size_t iy = min_y; iy <= max_y; iy++) {
          for (size_t ix = min_x; ix <= max_x; ix++) {
            size_t offset = (iy - min_y) * patch.xsize + ix - min_x;
            patch.fpixels[c][offset] =
                opsin_rows[c][iy * opsin_stride + ix] - ref[c];
            int val = pci.Quantize(patch.fpixels[c][offset], c);
            patch.pixels[c][offset] = val;
            if (std::abs(val) > max_value) max_value = std::abs(val);
          }
        }
      }
      if (max_value < kMinPeak) {
        info.pop_back();
        continue;
      }
      if (paint_ccs) {
        float cc_color = rng.UniformF(0.5, 1.0);
        for (std::pair<uint32_t, uint32_t> p : cc) {
          ccs.Row(p.second)[p.first] = cc_color;
        }
      }
    }
  }

  if (paint_ccs) {
    JXL_ASSERT(WantDebugOutput(cparams));
    JXL_RETURN_IF_ERROR(DumpPlaneNormalized(cparams, "ccs", ccs));
  }
  if (info.empty()) {
    return info;
  }

  // Remove duplicates.
  constexpr size_t kMinPatchOccurrences = 2;
  std::sort(info.begin(), info.end());
  size_t unique = 0;
  for (size_t i = 1; i < info.size(); i++) {
    if (info[i].first == info[unique].first) {
      info[unique].second.insert(info[unique].second.end(),
                                 info[i].second.begin(), info[i].second.end());
    } else {
      if (info[unique].second.size() >= kMinPatchOccurrences) {
        unique++;
      }
      info[unique] = info[i];
    }
  }
  if (info[unique].second.size() >= kMinPatchOccurrences) {
    unique++;
  }
  info.resize(unique);

  size_t max_patch_size = 0;

  for (const auto& patch : info) {
    size_t pixels = patch.first.xsize * patch.first.ysize;
    if (pixels > max_patch_size) max_patch_size = pixels;
  }

  // don't use patches if all patches are smaller than this
  constexpr size_t kMinMaxPatchSize = 20;
  if (max_patch_size < kMinMaxPatchSize) {
    info.clear();
  }

  return info;
}

}  // namespace

Status FindBestPatchDictionary(const Image3F& opsin,
                               PassesEncoderState* JXL_RESTRICT state,
                               const JxlCmsInterface& cms, ThreadPool* pool,
                               AuxOut* aux_out, bool is_xyb) {
  JXL_ASSIGN_OR_RETURN(
      std::vector<PatchInfo> info,
      FindTextLikePatches(state->cparams, opsin, state, pool, aux_out, is_xyb));
  JxlMemoryManager* memory_manager = opsin.memory_manager();

  // TODO(veluca): this doesn't work if both dots and patches are enabled.
  // For now, since dots and patches are not likely to occur in the same kind of
  // images, disable dots if some patches were found.
  if (info.empty() &&
      ApplyOverride(
          state->cparams.dots,
          state->cparams.speed_tier <= SpeedTier::kSquirrel &&
              state->cparams.butteraugli_distance >= kMinButteraugliForDots &&
              !state->cparams.disable_perceptual_optimizations)) {
    Rect rect(0, 0, state->shared.frame_dim.xsize,
              state->shared.frame_dim.ysize);
    JXL_ASSIGN_OR_RETURN(info,
                         FindDotDictionary(state->cparams, opsin, rect,
                                           state->shared.cmap.base(), pool));
  }

  if (info.empty()) return true;

  std::sort(
      info.begin(), info.end(), [&](const PatchInfo& a, const PatchInfo& b) {
        return a.first.xsize * a.first.ysize > b.first.xsize * b.first.ysize;
      });

  size_t max_x_size = 0;
  size_t max_y_size = 0;
  size_t total_pixels = 0;

  for (const auto& patch : info) {
    size_t pixels = patch.first.xsize * patch.first.ysize;
    if (max_x_size < patch.first.xsize) max_x_size = patch.first.xsize;
    if (max_y_size < patch.first.ysize) max_y_size = patch.first.ysize;
    total_pixels += pixels;
  }

  // Bin-packing & conversion of patches.
  constexpr float kBinPackingSlackness = 1.05f;
  size_t ref_xsize = std::max<float>(max_x_size, std::sqrt(total_pixels));
  size_t ref_ysize = std::max<float>(max_y_size, std::sqrt(total_pixels));
  std::vector<std::pair<size_t, size_t>> ref_positions(info.size());
  // TODO(veluca): allow partial overlaps of patches that have the same pixels.
  size_t max_y = 0;
  do {
    max_y = 0;
    // Increase packed image size.
    ref_xsize = ref_xsize * kBinPackingSlackness + 1;
    ref_ysize = ref_ysize * kBinPackingSlackness + 1;

    JXL_ASSIGN_OR_RETURN(ImageB occupied,
                         ImageB::Create(memory_manager, ref_xsize, ref_ysize));
    ZeroFillImage(&occupied);
    uint8_t* JXL_RESTRICT occupied_rows = occupied.Row(0);
    size_t occupied_stride = occupied.PixelsPerRow();

    bool success = true;
    // For every patch...
    for (size_t patch = 0; patch < info.size(); patch++) {
      size_t x0 = 0;
      size_t y0 = 0;
      size_t xsize = info[patch].first.xsize;
      size_t ysize = info[patch].first.ysize;
      bool found = false;
      // For every possible start position ...
      for (; y0 + ysize <= ref_ysize; y0++) {
        x0 = 0;
        for (; x0 + xsize <= ref_xsize; x0++) {
          bool has_occupied_pixel = false;
          size_t x = x0;
          // Check if it is possible to place the patch in this position in the
          // reference frame.
          for (size_t y = y0; y < y0 + ysize; y++) {
            x = x0;
            for (; x < x0 + xsize; x++) {
              if (occupied_rows[y * occupied_stride + x]) {
                has_occupied_pixel = true;
                break;
              }
            }
          }  // end of positioning check
          if (!has_occupied_pixel) {
            found = true;
            break;
          }
          x0 = x;  // Jump to next pixel after the occupied one.
        }
        if (found) break;
      }  // end of start position checking

      // We didn't find a possible position: repeat from the beginning with a
      // larger reference frame size.
      if (!found) {
        success = false;
        break;
      }

      // We found a position: mark the corresponding positions in the reference
      // image as used.
      ref_positions[patch] = {x0, y0};
      for (size_t y = y0; y < y0 + ysize; y++) {
        for (size_t x = x0; x < x0 + xsize; x++) {
          occupied_rows[y * occupied_stride + x] = JXL_TRUE;
        }
      }
      max_y = std::max(max_y, y0 + ysize);
    }

    if (success) break;
  } while (true);

  JXL_ASSERT(ref_ysize >= max_y);

  ref_ysize = max_y;

  JXL_ASSIGN_OR_RETURN(Image3F reference_frame,
                       Image3F::Create(memory_manager, ref_xsize, ref_ysize));
  // TODO(veluca): figure out a better way to fill the image.
  ZeroFillImage(&reference_frame);
  std::vector<PatchPosition> positions;
  std::vector<PatchReferencePosition> pref_positions;
  std::vector<PatchBlending> blendings;
  float* JXL_RESTRICT ref_rows[3] = {
      reference_frame.PlaneRow(0, 0),
      reference_frame.PlaneRow(1, 0),
      reference_frame.PlaneRow(2, 0),
  };
  size_t ref_stride = reference_frame.PixelsPerRow();
  size_t num_ec = state->shared.metadata->m.num_extra_channels;

  for (size_t i = 0; i < info.size(); i++) {
    PatchReferencePosition ref_pos;
    ref_pos.xsize = info[i].first.xsize;
    ref_pos.ysize = info[i].first.ysize;
    ref_pos.x0 = ref_positions[i].first;
    ref_pos.y0 = ref_positions[i].second;
    ref_pos.ref = kPatchFrameReferenceId;
    for (size_t y = 0; y < ref_pos.ysize; y++) {
      for (size_t x = 0; x < ref_pos.xsize; x++) {
        for (size_t c = 0; c < 3; c++) {
          ref_rows[c][(y + ref_pos.y0) * ref_stride + x + ref_pos.x0] =
              info[i].first.fpixels[c][y * ref_pos.xsize + x];
        }
      }
    }
    for (const auto& pos : info[i].second) {
      JXL_DEBUG_V(4, "Patch %" PRIuS "x%" PRIuS " at position %u,%u",
                  ref_pos.xsize, ref_pos.ysize, pos.first, pos.second);
      positions.emplace_back(
          PatchPosition{pos.first, pos.second, pref_positions.size()});
      // Add blending for color channels, ignore other channels.
      blendings.push_back({PatchBlendMode::kAdd, 0, false});
      for (size_t j = 0; j < num_ec; ++j) {
        blendings.push_back({PatchBlendMode::kNone, 0, false});
      }
    }
    pref_positions.emplace_back(ref_pos);
  }

  CompressParams cparams = state->cparams;
  // Recursive application of patches could create very weird issues.
  cparams.patches = Override::kOff;

  JXL_RETURN_IF_ERROR(RoundtripPatchFrame(&reference_frame, state,
                                          kPatchFrameReferenceId, cparams, cms,
                                          pool, aux_out, /*subtract=*/true));

  // TODO(veluca): this assumes that applying patches is commutative, which is
  // not true for all blending modes. This code only produces kAdd patches, so
  // this works out.
  PatchDictionaryEncoder::SetPositions(
      &state->shared.image_features.patches, std::move(positions),
      std::move(pref_positions), std::move(blendings), num_ec + 1);
  return true;
}

Status RoundtripPatchFrame(Image3F* reference_frame,
                           PassesEncoderState* JXL_RESTRICT state, int idx,
                           CompressParams& cparams, const JxlCmsInterface& cms,
                           ThreadPool* pool, AuxOut* aux_out, bool subtract) {
  JxlMemoryManager* memory_manager = state->memory_manager();
  FrameInfo patch_frame_info;
  cparams.resampling = 1;
  cparams.ec_resampling = 1;
  cparams.dots = Override::kOff;
  cparams.noise = Override::kOff;
  cparams.modular_mode = true;
  cparams.responsive = 0;
  cparams.progressive_dc = 0;
  cparams.progressive_mode = Override::kOff;
  cparams.qprogressive_mode = Override::kOff;
  // Use gradient predictor and not Predictor::Best.
  cparams.options.predictor = Predictor::Gradient;
  patch_frame_info.save_as_reference = idx;  // always saved.
  patch_frame_info.frame_type = FrameType::kReferenceOnly;
  patch_frame_info.save_before_color_transform = true;
  ImageBundle ib(memory_manager, &state->shared.metadata->m);
  // TODO(veluca): metadata.color_encoding is a lie: ib is in XYB, but there is
  // no simple way to express that yet.
  patch_frame_info.ib_needs_color_transform = false;
  ib.SetFromImage(std::move(*reference_frame),
                  state->shared.metadata->m.color_encoding);
  if (!ib.metadata()->extra_channel_info.empty()) {
    // Add placeholder extra channels to the patch image: patch encoding does
    // not yet support extra channels, but the codec expects that the amount of
    // extra channels in frames matches that in the metadata of the codestream.
    std::vector<ImageF> extra_channels;
    extra_channels.reserve(ib.metadata()->extra_channel_info.size());
    for (size_t i = 0; i < ib.metadata()->extra_channel_info.size(); i++) {
      JXL_ASSIGN_OR_RETURN(
          ImageF ch, ImageF::Create(memory_manager, ib.xsize(), ib.ysize()));
      extra_channels.emplace_back(std::move(ch));
      // Must initialize the image with data to not affect blending with
      // uninitialized memory.
      // TODO(lode): patches must copy and use the real extra channels instead.
      ZeroFillImage(&extra_channels.back());
    }
    ib.SetExtraChannels(std::move(extra_channels));
  }
  auto special_frame = jxl::make_unique<BitWriter>(memory_manager);
  AuxOut patch_aux_out;
  JXL_CHECK(EncodeFrame(
      memory_manager, cparams, patch_frame_info, state->shared.metadata, ib,
      cms, pool, special_frame.get(), aux_out ? &patch_aux_out : nullptr));
  if (aux_out) {
    for (const auto& l : patch_aux_out.layers) {
      aux_out->layers[kLayerDictionary].Assimilate(l);
    }
  }
  const Span<const uint8_t> encoded = special_frame->GetSpan();
  state->special_frames.emplace_back(std::move(special_frame));
  if (subtract) {
    ImageBundle decoded(memory_manager, &state->shared.metadata->m);
    PassesDecoderState dec_state(memory_manager);
    JXL_CHECK(dec_state.output_encoding_info.SetFromMetadata(
        *state->shared.metadata));
    const uint8_t* frame_start = encoded.data();
    size_t encoded_size = encoded.size();
    JXL_CHECK(DecodeFrame(&dec_state, pool, frame_start, encoded_size,
                          /*frame_header=*/nullptr, &decoded,
                          *state->shared.metadata));
    frame_start += decoded.decoded_bytes();
    encoded_size -= decoded.decoded_bytes();
    size_t ref_xsize =
        dec_state.shared_storage.reference_frames[idx].frame->color()->xsize();
    // if the frame itself uses patches, we need to decode another frame
    if (!ref_xsize) {
      JXL_CHECK(DecodeFrame(&dec_state, pool, frame_start, encoded_size,
                            /*frame_header=*/nullptr, &decoded,
                            *state->shared.metadata));
    }
    JXL_CHECK(encoded_size == 0);
    state->shared.reference_frames[idx] =
        std::move(dec_state.shared_storage.reference_frames[idx]);
  } else {
    *state->shared.reference_frames[idx].frame = std::move(ib);
  }
  return true;
}

}  // namespace jxl
