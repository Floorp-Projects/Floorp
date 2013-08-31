/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_GL_GL_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_GL_GL_RENDERER_H_

#ifdef WEBRTC_MAC
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "webrtc/video_engine/test/common/video_renderer.h"
#include "webrtc/typedefs.h"


namespace webrtc {
namespace test {

class GlRenderer : public VideoRenderer {
 public:
  virtual void RenderFrame(const webrtc::I420VideoFrame& frame,
                           int time_to_render_ms) OVERRIDE;

 protected:
  GlRenderer();

  void Init();
  void Destroy();

  void ResizeViewport(size_t width, size_t height);

 private:
  bool is_init_;
  uint8_t* buffer_;
  GLuint texture_;
  size_t width_, height_, buffer_size_;

  void ResizeVideo(size_t width, size_t height);
};
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_GL_GL_RENDERER_H_
