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

#ifndef LIB_JXL_PATCH_DICTIONARY_INTERNAL_H_
#define LIB_JXL_PATCH_DICTIONARY_INTERNAL_H_

#include "lib/jxl/dec_patch_dictionary.h"
#include "lib/jxl/passes_state.h"  // for PassesSharedState

namespace jxl {

// Context numbers as specified in Section C.4.5, Listing C.2:
enum Contexts {
  kNumRefPatchContext = 0,
  kReferenceFrameContext = 1,
  kPatchSizeContext = 2,
  kPatchReferencePositionContext = 3,
  kPatchPositionContext = 4,
  kPatchBlendModeContext = 5,
  kPatchOffsetContext = 6,
  kPatchCountContext = 7,
  kPatchAlphaChannelContext = 8,
  kPatchClampContext = 9,
  kNumPatchDictionaryContexts
};

template <bool add>
void PatchDictionary::Apply(Image3F* opsin, const Rect& opsin_rect,
                            const Rect& image_rect) const {
  JXL_CHECK(SameSize(opsin_rect, image_rect));
  size_t num = 0;
  for (size_t y = image_rect.y0(); y < image_rect.y0() + image_rect.ysize();
       y++) {
    if (y + 1 >= patch_starts_.size()) continue;
    float* JXL_RESTRICT rows[3] = {
        opsin_rect.PlaneRow(opsin, 0, y - image_rect.y0()),
        opsin_rect.PlaneRow(opsin, 1, y - image_rect.y0()),
        opsin_rect.PlaneRow(opsin, 2, y - image_rect.y0()),
    };
    for (size_t id = patch_starts_[y]; id < patch_starts_[y + 1]; id++) {
      num++;
      const PatchPosition& pos = positions_[sorted_patches_[id]];
      size_t by = pos.y;
      size_t bx = pos.x;
      size_t xsize = pos.ref_pos.xsize;
      JXL_DASSERT(y >= by);
      JXL_DASSERT(y < by + pos.ref_pos.ysize);
      size_t iy = y - by;
      size_t ref = pos.ref_pos.ref;
      if (bx >= image_rect.x0() + image_rect.xsize()) continue;
      if (bx + xsize < image_rect.x0()) continue;
      // TODO(veluca): check that the reference frame is in XYB.
      const float* JXL_RESTRICT ref_rows[3] = {
          shared_->reference_frames[ref].frame->color()->ConstPlaneRow(
              0, pos.ref_pos.y0 + iy) +
              pos.ref_pos.x0,
          shared_->reference_frames[ref].frame->color()->ConstPlaneRow(
              1, pos.ref_pos.y0 + iy) +
              pos.ref_pos.x0,
          shared_->reference_frames[ref].frame->color()->ConstPlaneRow(
              2, pos.ref_pos.y0 + iy) +
              pos.ref_pos.x0,
      };
      // TODO(veluca): use the same code as in dec_reconstruct.cc.
      for (size_t ix = 0; ix < xsize; ix++) {
        // TODO(veluca): hoist branches and checks.
        // TODO(veluca): implement for extra channels.
        if (bx + ix < image_rect.x0()) continue;
        if (bx + ix >= image_rect.x0() + image_rect.xsize()) continue;
        for (size_t c = 0; c < 3; c++) {
          if (add) {
            if (pos.blending[0].mode == PatchBlendMode::kAdd) {
              rows[c][bx + ix - image_rect.x0()] += ref_rows[c][ix];
            } else if (pos.blending[0].mode == PatchBlendMode::kReplace) {
              rows[c][bx + ix - image_rect.x0()] = ref_rows[c][ix];
            } else if (pos.blending[0].mode == PatchBlendMode::kNone) {
              // Nothing to do.
            } else {
              // Checked in decoding code.
              JXL_ABORT("Blending mode %u not yet implemented",
                        (uint32_t)pos.blending[0].mode);
            }
          } else {
            if (pos.blending[0].mode == PatchBlendMode::kAdd) {
              rows[c][bx + ix - image_rect.x0()] -= ref_rows[c][ix];
            } else if (pos.blending[0].mode == PatchBlendMode::kReplace) {
              rows[c][bx + ix - image_rect.x0()] = 0;
            } else if (pos.blending[0].mode == PatchBlendMode::kNone) {
              // Nothing to do.
            } else {
              // Checked in decoding code.
              JXL_ABORT("Blending mode %u not yet implemented",
                        (uint32_t)pos.blending[0].mode);
            }
          }
        }
      }
    }
  }
}

}  // namespace jxl

#endif  // LIB_JXL_PATCH_DICTIONARY_INTERNAL_H_
