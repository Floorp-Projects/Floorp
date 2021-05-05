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

#include "lib/jxl/dec_patch_dictionary.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <algorithm>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/dec_frame.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/patch_dictionary_internal.h"

namespace jxl {

constexpr int kMaxPatches = 1 << 24;

Status PatchDictionary::Decode(BitReader* br, size_t xsize, size_t ysize) {
  std::vector<uint8_t> context_map;
  ANSCode code;
  JXL_RETURN_IF_ERROR(
      DecodeHistograms(br, kNumPatchDictionaryContexts, &code, &context_map));
  ANSSymbolReader decoder(&code, br);

  auto read_num = [&](size_t context) {
    size_t r = decoder.ReadHybridUint(context, br, context_map);
    return r;
  };

  size_t num_ref_patch = read_num(kNumRefPatchContext);
  // TODO(veluca): does this make sense?
  if (num_ref_patch > kMaxPatches) {
    return JXL_FAILURE("Too many patches in dictionary");
  }

  for (size_t id = 0; id < num_ref_patch; id++) {
    PatchReferencePosition ref_pos;
    ref_pos.ref = read_num(kReferenceFrameContext);
    if (ref_pos.ref >= kMaxNumReferenceFrames ||
        shared_->reference_frames[ref_pos.ref].frame->xsize() == 0) {
      return JXL_FAILURE("Invalid reference frame ID");
    }
    const ImageBundle& ib = *shared_->reference_frames[ref_pos.ref].frame;
    ref_pos.x0 = read_num(kPatchReferencePositionContext);
    ref_pos.y0 = read_num(kPatchReferencePositionContext);
    ref_pos.xsize = read_num(kPatchSizeContext) + 1;
    ref_pos.ysize = read_num(kPatchSizeContext) + 1;
    if (ref_pos.x0 + ref_pos.xsize > ib.xsize()) {
      return JXL_FAILURE("Invalid position specified in reference frame");
    }
    if (ref_pos.y0 + ref_pos.ysize > ib.ysize()) {
      return JXL_FAILURE("Invalid position specified in reference frame");
    }
    size_t id_count = read_num(kPatchCountContext) + 1;
    if (id_count > kMaxPatches) {
      return JXL_FAILURE("Too many patches in dictionary");
    }
    positions_.reserve(positions_.size() + id_count);
    for (size_t i = 0; i < id_count; i++) {
      PatchPosition pos;
      pos.ref_pos = ref_pos;
      if (i == 0) {
        pos.x = read_num(kPatchPositionContext);
        pos.y = read_num(kPatchPositionContext);
      } else {
        pos.x =
            positions_.back().x + UnpackSigned(read_num(kPatchOffsetContext));
        pos.y =
            positions_.back().y + UnpackSigned(read_num(kPatchOffsetContext));
      }
      if (pos.x + ref_pos.xsize > xsize) {
        return JXL_FAILURE("Invalid patch x: at %zu + %zu > %zu", pos.x,
                           ref_pos.xsize, xsize);
      }
      if (pos.y + ref_pos.ysize > ysize) {
        return JXL_FAILURE("Invalid patch y: at %zu + %zu > %zu", pos.y,
                           ref_pos.ysize, ysize);
      }
      for (size_t i = 0; i < shared_->metadata->m.extra_channel_info.size() + 1;
           i++) {
        uint32_t blend_mode = read_num(kPatchBlendModeContext);
        if (blend_mode >= uint32_t(PatchBlendMode::kNumBlendModes)) {
          return JXL_FAILURE("Invalid patch blend mode: %u", blend_mode);
        }
        PatchBlending info;
        info.mode = static_cast<PatchBlendMode>(blend_mode);
        if (i != 0 && info.mode != PatchBlendMode::kNone) {
          return JXL_FAILURE(
              "Blending of extra channels with patches is not supported yet");
        }
        if (info.mode != PatchBlendMode::kAdd &&
            info.mode != PatchBlendMode::kNone &&
            info.mode != PatchBlendMode::kReplace) {
          return JXL_FAILURE("Blending mode not supported yet: %u", blend_mode);
        }
        if (UsesAlpha(info.mode) &&
            shared_->metadata->m.extra_channel_info.size() > 1) {
          info.alpha_channel = read_num(kPatchAlphaChannelContext);
          if (info.alpha_channel >=
              shared_->metadata->m.extra_channel_info.size()) {
            return JXL_FAILURE(
                "Invalid alpha channel for blending: %u out of %u\n",
                info.alpha_channel,
                (uint32_t)shared_->metadata->m.extra_channel_info.size());
          }
        }
        if (UsesClamp(info.mode)) {
          info.clamp = read_num(kPatchClampContext);
        }
        pos.blending.push_back(info);
      }
      positions_.push_back(std::move(pos));
    }
  }

  if (!decoder.CheckANSFinalState()) {
    return JXL_FAILURE("ANS checksum failure.");
  }
  if (!HasAny()) {
    return JXL_FAILURE("Decoded patch dictionary but got none");
  }

  ComputePatchCache();
  return true;
}

void PatchDictionary::ComputePatchCache() {
  if (positions_.empty()) return;
  std::vector<std::pair<size_t, size_t>> sorted_patches_y;
  for (size_t i = 0; i < positions_.size(); i++) {
    const PatchPosition& pos = positions_[i];
    for (size_t y = pos.y; y < pos.y + pos.ref_pos.ysize; y++) {
      sorted_patches_y.emplace_back(y, i);
    }
  }
  // The relative order of patches that affect the same pixels is preserved.
  // This is important for patches that have a blend mode different from kAdd.
  std::sort(sorted_patches_y.begin(), sorted_patches_y.end());
  patch_starts_.resize(sorted_patches_y.back().first + 2,
                       sorted_patches_y.size());
  sorted_patches_.resize(sorted_patches_y.size());
  for (size_t i = 0; i < sorted_patches_y.size(); i++) {
    sorted_patches_[i] = sorted_patches_y[i].second;
    patch_starts_[sorted_patches_y[i].first] =
        std::min(patch_starts_[sorted_patches_y[i].first], i);
  }
  for (size_t i = patch_starts_.size() - 1; i > 0; i--) {
    patch_starts_[i - 1] = std::min(patch_starts_[i], patch_starts_[i - 1]);
  }
}

void PatchDictionary::AddTo(Image3F* opsin, const Rect& opsin_rect,
                            const Rect& image_rect) const {
  Apply</*add=*/true>(opsin, opsin_rect, image_rect);
}

}  // namespace jxl
