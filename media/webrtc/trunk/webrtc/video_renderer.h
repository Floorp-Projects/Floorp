/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_RENDERER_H_

namespace webrtc {

class I420VideoFrame;

class VideoRenderer {
 public:
  // This function should return as soon as possible and not block until it's
  // time to render the frame.
  // TODO(mflodman) Remove time_to_render_ms when I420VideoFrame contains NTP.
  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) = 0;

  virtual bool IsTextureSupported() const = 0;

 protected:
  virtual ~VideoRenderer() {}
};
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_VIDEO_RENDERER_H_
