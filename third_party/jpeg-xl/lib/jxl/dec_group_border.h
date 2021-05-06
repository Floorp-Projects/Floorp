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

#ifndef LIB_JXL_DEC_GROUP_BORDER_H_
#define LIB_JXL_DEC_GROUP_BORDER_H_

#include <stddef.h>

#include <atomic>

#include "lib/jxl/base/arch_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image.h"

namespace jxl {

class GroupBorderAssigner {
 public:
  // Prepare the GroupBorderAssigner to handle a given frame.
  void Init(const FrameDimensions& frame_dim);
  // Marks a group as done, and returns the (at most 3) rects to run
  // FinalizeImageRect on. `block_rect` must be the rect corresponding
  // to the given `group_id`, measured in blocks.
  void GroupDone(size_t group_id, size_t padding, Rect* rects_to_finalize,
                 size_t* num_to_finalize);
  // Marks a group as not-done, for running re-paints.
  void ClearDone(size_t group_id);

  static constexpr size_t kMaxToFinalize = 3;

  // Vectors on ARM NEON are never wider than 4 floats, so rounding to multiples
  // of 4 is enough.
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  static constexpr size_t kPaddingXRound = 4;
#else
  static constexpr size_t kPaddingXRound = kBlockDim;
#endif

  // Returns the necessary amount of padding for the X axis.
  size_t PaddingX(size_t padding) { return RoundUpTo(padding, kPaddingXRound); }

 private:
  FrameDimensions frame_dim_;
  std::unique_ptr<std::atomic<uint8_t>[]> counters_;

  // Constants to identify group positions relative to the corners.
  static constexpr uint8_t kTopLeft = 0x01;
  static constexpr uint8_t kTopRight = 0x02;
  static constexpr uint8_t kBottomRight = 0x04;
  static constexpr uint8_t kBottomLeft = 0x08;
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_GROUP_BORDER_H_
