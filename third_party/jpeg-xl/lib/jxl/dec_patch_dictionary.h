// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_DEC_PATCH_DICTIONARY_H_
#define LIB_JXL_DEC_PATCH_DICTIONARY_H_

// Chooses reference patches, and avoids encoding them once per occurrence.

#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include <tuple>
#include <vector>

#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/image.h"
#include "lib/jxl/opsin_params.h"

namespace jxl {

enum class PatchBlendMode : uint8_t {
  // The new values are the old ones. Useful to skip some channels.
  kNone = 0,
  // The new values (in the crop) replace the old ones: sample = new
  kReplace = 1,
  // The new values (in the crop) get added to the old ones: sample = old + new
  kAdd = 2,
  // The new values (in the crop) get multiplied by the old ones:
  // sample = old * new
  // This blend mode is only supported if BlendColorSpace is kEncoded. The
  // range of the new value matters for multiplication purposes, and its
  // nominal range of 0..1 is computed the same way as this is done for the
  // alpha values in kBlend and kAlphaWeightedAdd.
  kMul = 3,
  // The new values (in the crop) replace the old ones if alpha>0:
  // For first alpha channel:
  // alpha = old + new * (1 - old)
  // For other channels if !alpha_associated:
  // sample = ((1 - new_alpha) * old * old_alpha + new_alpha * new) / alpha
  // For other channels if alpha_associated:
  // sample = (1 - new_alpha) * old + new
  // The alpha formula applies to the alpha used for the division in the other
  // channels formula, and applies to the alpha channel itself if its
  // blend_channel value matches itself.
  // If using kBlendAbove, new is the patch and old is the original image; if
  // using kBlendBelow, the meaning is inverted.
  kBlendAbove = 4,
  kBlendBelow = 5,
  // The new values (in the crop) are added to the old ones if alpha>0:
  // For first alpha channel: sample = sample = old + new * (1 - old)
  // For other channels: sample = old + alpha * new
  kAlphaWeightedAddAbove = 6,
  kAlphaWeightedAddBelow = 7,
  kNumBlendModes,
};

inline bool UsesAlpha(PatchBlendMode mode) {
  return mode == PatchBlendMode::kBlendAbove ||
         mode == PatchBlendMode::kBlendBelow ||
         mode == PatchBlendMode::kAlphaWeightedAddAbove ||
         mode == PatchBlendMode::kAlphaWeightedAddBelow;
}
inline bool UsesClamp(PatchBlendMode mode) {
  return UsesAlpha(mode) || mode == PatchBlendMode::kMul;
}

struct PatchBlending {
  PatchBlendMode mode;
  uint32_t alpha_channel;
  bool clamp;
};

// Position and size of the patch in the reference frame.
struct PatchReferencePosition {
  size_t ref, x0, y0, xsize, ysize;
  bool operator<(const PatchReferencePosition& oth) const {
    return std::make_tuple(ref, x0, y0, xsize, ysize) <
           std::make_tuple(oth.ref, oth.x0, oth.y0, oth.xsize, oth.ysize);
  }
  bool operator==(const PatchReferencePosition& oth) const {
    return !(*this < oth) && !(oth < *this);
  }
};

struct PatchPosition {
  // Position of top-left corner of the patch in the image.
  size_t x, y;
  // Different blend mode for color and extra channels.
  std::vector<PatchBlending> blending;
  PatchReferencePosition ref_pos;
  bool operator<(const PatchPosition& oth) const {
    return std::make_tuple(ref_pos, x, y) <
           std::make_tuple(oth.ref_pos, oth.x, oth.y);
  }
};

struct PassesSharedState;

// Encoder-side helper class to encode the PatchesDictionary.
class PatchDictionaryEncoder;

class PatchDictionary {
 public:
  PatchDictionary() = default;

  void SetPassesSharedState(const PassesSharedState* shared) {
    shared_ = shared;
  }

  bool HasAny() const { return !positions_.empty(); }

  Status Decode(BitReader* br, size_t xsize, size_t ysize,
                bool* uses_extra_channels);

  void Clear() {
    positions_.clear();
    ComputePatchCache();
  }

  // Only adds patches that belong to the `image_rect` area of the decoded
  // image, writing them to the `opsin_rect` area of `opsin`.
  Status AddTo(Image3F* opsin, const Rect& opsin_rect,
               float* const* extra_channels, const Rect& image_rect) const;

  // Returns dependencies of this patch dictionary on reference frame ids as a
  // bit mask: bits 0-3 indicate reference frame 0-3.
  int GetReferences() const;

 private:
  friend class PatchDictionaryEncoder;

  const PassesSharedState* shared_;
  std::vector<PatchPosition> positions_;

  // Patch occurrences sorted by y.
  std::vector<size_t> sorted_patches_;
  // Index of the first patch for each y value.
  std::vector<size_t> patch_starts_;

  // Patch IDs in position [patch_starts_[y], patch_start_[y+1]) of
  // sorted_patches_ are all the patches that intersect the horizontal line at
  // y.
  // The relative order of patches that affect the same pixels is the same -
  // important when applying patches is noncommutative.

  // Compute patches_by_y_ after updating positions_.
  void ComputePatchCache();
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_PATCH_DICTIONARY_H_
