// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_patch_dictionary.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/blending.h"
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

Status PatchDictionary::Decode(BitReader* br, size_t xsize, size_t ysize,
                               bool* uses_extra_channels) {
  positions_.clear();
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
  // Limit max memory usage of patches to about 66 bytes per pixel (assuming 8
  // bytes per size_t)
  const size_t num_pixels = xsize * ysize;
  const size_t max_ref_patches = 1024 + num_pixels / 4;
  const size_t max_patches = max_ref_patches * 4;
  const size_t max_blending_infos = max_patches * 4;
  if (num_ref_patch > max_ref_patches) {
    return JXL_FAILURE("Too many patches in dictionary");
  }
  size_t num_ec = shared_->metadata->m.num_extra_channels;

  size_t total_patches = 0;
  size_t next_size = 1;

  for (size_t id = 0; id < num_ref_patch; id++) {
    PatchReferencePosition ref_pos;
    ref_pos.ref = read_num(kReferenceFrameContext);
    if (ref_pos.ref >= kMaxNumReferenceFrames ||
        shared_->reference_frames[ref_pos.ref].frame->xsize() == 0) {
      return JXL_FAILURE("Invalid reference frame ID");
    }
    if (!shared_->reference_frames[ref_pos.ref].ib_is_in_xyb) {
      return JXL_FAILURE(
          "Patches cannot use frames saved post color transforms");
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
    total_patches += id_count;
    if (total_patches > max_patches) {
      return JXL_FAILURE("Too many patches in dictionary");
    }
    if (next_size < total_patches) {
      next_size *= 2;
      next_size = std::min<size_t>(next_size, max_patches);
    }
    if (next_size * (num_ec + 1) > max_blending_infos) {
      return JXL_FAILURE("Too many patches in dictionary");
    }
    positions_.reserve(next_size);
    blendings_.reserve(next_size * (num_ec + 1));
    for (size_t i = 0; i < id_count; i++) {
      PatchPosition pos;
      pos.ref_pos_idx = ref_positions_.size();
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
        return JXL_FAILURE("Invalid patch x: at %" PRIuS " + %" PRIuS
                           " > %" PRIuS,
                           pos.x, ref_pos.xsize, xsize);
      }
      if (pos.y + ref_pos.ysize > ysize) {
        return JXL_FAILURE("Invalid patch y: at %" PRIuS " + %" PRIuS
                           " > %" PRIuS,
                           pos.y, ref_pos.ysize, ysize);
      }
      for (size_t j = 0; j < num_ec + 1; j++) {
        uint32_t blend_mode = read_num(kPatchBlendModeContext);
        if (blend_mode >= uint32_t(PatchBlendMode::kNumBlendModes)) {
          return JXL_FAILURE("Invalid patch blend mode: %u", blend_mode);
        }
        PatchBlending info;
        info.mode = static_cast<PatchBlendMode>(blend_mode);
        if (UsesAlpha(info.mode)) {
          *uses_extra_channels = true;
        }
        if (info.mode != PatchBlendMode::kNone && j > 0) {
          *uses_extra_channels = true;
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
        } else {
          info.alpha_channel = 0;
        }
        if (UsesClamp(info.mode)) {
          info.clamp = read_num(kPatchClampContext);
        } else {
          info.clamp = false;
        }
        blendings_.push_back(info);
      }
      positions_.push_back(std::move(pos));
    }
    ref_positions_.emplace_back(std::move(ref_pos));
  }
  positions_.shrink_to_fit();

  if (!decoder.CheckANSFinalState()) {
    return JXL_FAILURE("ANS checksum failure.");
  }

  ComputePatchCache();
  return true;
}

int PatchDictionary::GetReferences() const {
  int result = 0;
  for (size_t i = 0; i < ref_positions_.size(); ++i) {
    result |= (1 << static_cast<int>(ref_positions_[i].ref));
  }
  return result;
}

void PatchDictionary::ComputePatchCache() {
  patch_starts_.clear();
  sorted_patches_.clear();
  if (positions_.empty()) return;
  std::vector<std::pair<size_t, size_t>> sorted_patches_y;
  for (size_t i = 0; i < positions_.size(); i++) {
    const PatchPosition& pos = positions_[i];
    const PatchReferencePosition& ref_pos = ref_positions_[pos.ref_pos_idx];
    for (size_t y = pos.y; y < pos.y + ref_pos.ysize; y++) {
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

// Adds patches to a segment of `xsize` pixels, starting at `inout`, assumed
// to be located at position (x0, y) in the frame.
void PatchDictionary::AddOneRow(float* const* inout, size_t y, size_t x0,
                                size_t xsize) const {
  if (patch_starts_.empty()) return;
  size_t num_ec = shared_->metadata->m.num_extra_channels;
  std::vector<const float*> fg_ptrs(3 + num_ec);
  if (y + 1 >= patch_starts_.size()) return;
  for (size_t id = patch_starts_[y]; id < patch_starts_[y + 1]; id++) {
    const size_t pos_idx = sorted_patches_[id];
    const size_t blending_idx = pos_idx * (num_ec + 1);
    const PatchPosition& pos = positions_[pos_idx];
    const PatchReferencePosition& ref_pos = ref_positions_[pos.ref_pos_idx];
    size_t by = pos.y;
    size_t bx = pos.x;
    size_t patch_xsize = ref_pos.xsize;
    JXL_DASSERT(y >= by);
    JXL_DASSERT(y < by + ref_pos.ysize);
    size_t iy = y - by;
    size_t ref = ref_pos.ref;
    if (bx >= x0 + xsize) continue;
    if (bx + patch_xsize < x0) continue;
    size_t patch_x0 = std::max(bx, x0);
    size_t patch_x1 = std::min(bx + patch_xsize, x0 + xsize);
    for (size_t c = 0; c < 3; c++) {
      fg_ptrs[c] = shared_->reference_frames[ref].frame->color()->ConstPlaneRow(
                       c, ref_pos.y0 + iy) +
                   ref_pos.x0 + x0 - bx;
    }
    for (size_t i = 0; i < num_ec; i++) {
      fg_ptrs[3 + i] =
          shared_->reference_frames[ref].frame->extra_channels()[i].ConstRow(
              ref_pos.y0 + iy) +
          ref_pos.x0 + x0 - bx;
    }
    PerformBlending(inout, fg_ptrs.data(), inout, patch_x0 - x0,
                    patch_x1 - patch_x0, blendings_[blending_idx],
                    &blendings_[blending_idx + 1],
                    shared_->metadata->m.extra_channel_info);
  }
}
}  // namespace jxl
