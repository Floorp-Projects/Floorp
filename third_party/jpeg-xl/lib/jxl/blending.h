// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_BLENDING_H_
#define LIB_JXL_BLENDING_H_
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/dec_patch_dictionary.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

void PerformBlending(const float* const* bg, const float* const* fg,
                     float* const* out, size_t x0, size_t xsize,
                     const PatchBlending& color_blending,
                     const PatchBlending* ec_blending,
                     const std::vector<ExtraChannelInfo>& extra_channel_info);

class ImageBlender {
 public:
  class RectBlender {
   public:
    // Does the blending for a given row of the rect passed to
    // ImageBlender::PrepareRect.
    Status DoBlending(size_t y);

    // If this returns true, then nothing needs to be done for this rect and
    // DoBlending can be skipped (but does not have to).
    bool done() const { return done_; }

   private:
    friend class ImageBlender;
    explicit RectBlender(bool done) : done_(done) {}

    bool done_;
    Rect current_overlap_;
    Rect current_cropbox_;
    const std::vector<ExtraChannelInfo>* extra_channel_info_;
    std::vector<const float*> fg_ptrs_;
    std::vector<size_t> fg_strides_;
    std::vector<const float*> bg_ptrs_;
    std::vector<size_t> bg_strides_;
    std::vector<float*> out_ptrs_;
    std::vector<size_t> out_strides_;
    std::vector<const float*> fg_row_ptrs_;
    std::vector<const float*> bg_row_ptrs_;
    std::vector<float*> out_row_ptrs_;
    std::vector<PatchBlending> blending_info_;
  };

  static bool NeedsBlending(PassesDecoderState* dec_state);

  Status PrepareBlending(
      PassesDecoderState* dec_state, FrameOrigin foreground_origin,
      size_t foreground_xsize, size_t foreground_ysize,
      const std::vector<ExtraChannelInfo>* extra_channel_info,
      const ColorEncoding& frame_color_encoding, const Rect& frame_rect,
      Image3F* output, const Rect& output_rect,
      std::vector<ImageF>* output_extra_channels,
      std::vector<Rect> output_extra_channels_rects);
  // rect is relative to the full decoded foreground.
  // But foreground here can be a subset of the full foreground, and input_rect
  // indicates where that rect is in that subset. For example, if rect =
  // Rect(10, 10, 20, 20), and foreground is subrect (7, 7, 30, 30) of the full
  // foreground, then input_rect should be (3, 3, 20, 20), because that is where
  // rect is relative to the foreground crop.
  ImageBlender::RectBlender PrepareRect(
      const Rect& rect, const Image3F& foreground,
      const std::vector<ImageF>& extra_channels, const Rect& input_rect) const;

  // If this returns true, then it is not necessary to call further methods on
  // this ImageBlender to achieve blending, although it is not forbidden either
  // (those methods will just return immediately in that case).
  bool done() const { return done_; }

 private:
  BlendingInfo info_;
  const std::vector<ExtraChannelInfo>* extra_channel_info_;
  Rect frame_rect_;
  // Destination, as well as background before DoBlending is called.
  Image3F* output_;
  ImageBundle* bg_;
  Rect output_rect_;
  std::vector<ImageF>* output_extra_channels_;
  std::vector<Rect> output_extra_channels_rects_;
  Rect cropbox_;
  Rect overlap_;
  bool done_ = false;
  const std::vector<BlendingInfo>* ec_info_;
  FrameOrigin o_{};
};

}  // namespace jxl

#endif  // LIB_JXL_BLENDING_H_
