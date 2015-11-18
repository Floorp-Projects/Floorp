/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_VIDEO_RENDER_FRAMES_H_  // NOLINT
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_VIDEO_RENDER_FRAMES_H_  // NOLINT

#include <list>

#include "webrtc/modules/video_render/include/video_render.h"

namespace webrtc {

// Class definitions
class VideoRenderFrames {
 public:
  VideoRenderFrames();

  // Add a frame to the render queue
  int32_t AddFrame(const I420VideoFrame& new_frame);

  // Get a frame for rendering, or a zero-size frame if it's not time to render.
  I420VideoFrame FrameToRender();

  // Releases all frames
  int32_t ReleaseAllFrames();

  // Returns the number of ms to next frame to render
  uint32_t TimeToNextFrameRelease();

  // Sets estimates delay in renderer
  int32_t SetRenderDelay(const uint32_t render_delay);

 private:
  // 10 seconds for 30 fps.
  enum { KMaxNumberOfFrames = 300 };
  // Don't render frames with timestamp older than 500ms from now.
  enum { KOldRenderTimestampMS = 500 };
  // Don't render frames with timestamp more than 10s into the future.
  enum { KFutureRenderTimestampMS = 10000 };

  // Sorted list with framed to be rendered, oldest first.
  std::list<I420VideoFrame> incoming_frames_;

  // Estimated delay from a frame is released until it's rendered.
  uint32_t render_delay_ms_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_VIDEO_RENDER_FRAMES_H_  // NOLINT
