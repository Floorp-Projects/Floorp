/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_CHANNEL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_CHANNEL_H_

#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/modules/video_render/ios/video_render_ios_view.h"

namespace webrtc {

class VideoRenderIosGles20;

class VideoRenderIosChannel : public VideoRenderCallback {
 public:
  explicit VideoRenderIosChannel(VideoRenderIosView* view);
  virtual ~VideoRenderIosChannel();

  // Implementation of VideoRenderCallback.
  virtual int32_t RenderFrame(const uint32_t stream_id,
                              I420VideoFrame& video_frame) OVERRIDE;

  int SetStreamSettings(const float z_order,
                        const float left,
                        const float top,
                        const float right,
                        const float bottom);
  bool IsUpdated();
  bool RenderOffScreenBuffer();

 private:
  VideoRenderIosView* view_;
  I420VideoFrame* current_frame_;
  bool buffer_is_updated_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_CHANNEL_H_
