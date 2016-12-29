/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/video_renderer.h"

// TODO(pbos): Android renderer

#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

class NullRenderer : public VideoRenderer {
  void RenderFrame(const VideoFrame& video_frame,
                   int time_to_render_ms) override {}
  bool IsTextureSupported() const override { return false; }
};

VideoRenderer* VideoRenderer::Create(const char* window_title,
                                     size_t width,
                                     size_t height) {
  VideoRenderer* renderer = CreatePlatformRenderer(window_title, width, height);
  if (renderer != NULL) {
    // TODO(mflodman) Add a warning log.
    return renderer;
  }

  return new NullRenderer();
}
}  // namespace test
}  // namespace webrtc
