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

}  // namespace jxl

#endif  // LIB_JXL_PATCH_DICTIONARY_INTERNAL_H_
