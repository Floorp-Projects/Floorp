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

#ifndef LIB_JXL_BLENDING_H_
#define LIB_JXL_BLENDING_H_
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

class ImageBlender {
 public:
  class RectBlender {
   public:
    // Does the blending for a given row of the rect passed to
    // ImageBlender::PrepareRect. It is safe to have parallel calls to
    // DoBlending.
    Status DoBlending(size_t y) const;

    // If this returns true, then nothing needs to be done for this rect and
    // DoBlending can be skipped (but does not have to).
    bool done() const { return done_; }

   private:
    friend class ImageBlender;
    explicit RectBlender(bool done) : done_(done) {}

    bool done_;
    Rect current_overlap_;
    Rect current_cropbox_;
    ImageBundle foreground_;
    BlendingInfo info_;
    ImageBundle* dest_;
    const std::vector<BlendingInfo>* ec_info_;
    size_t first_alpha_;
  };

  Status PrepareBlending(PassesDecoderState* dec_state, ImageBundle* foreground,
                         ImageBundle* output);
  // rect is relative to foreground.
  RectBlender PrepareRect(const Rect& rect,
                          const ImageBundle& foreground) const;

  // If this returns true, then it is not necessary to call further methods on
  // this ImageBlender to achieve blending, although it is not forbidden either
  // (those methods will just return immediately in that case).
  bool done() const { return done_; }

 private:
  BlendingInfo info_;
  // Destination, as well as background before DoBlending is called.
  ImageBundle* dest_;
  Rect cropbox_;
  Rect overlap_;
  bool done_ = false;
  const std::vector<BlendingInfo>* ec_info_;
  size_t first_alpha_;
  FrameOrigin o_{};
};

}  // namespace jxl

#endif  // LIB_JXL_BLENDING_H_
