/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_IOS_OPEN_GLES20_H_
#define WEBRTC_MODULES_VIDEO_RENDER_IOS_OPEN_GLES20_H_

#include <OpenGLES/ES2/glext.h>

#include "webrtc/modules/video_render/include/video_render_defines.h"

/*
 * This OpenGles20 is the class of renderer for I420VideoFrame into a GLES 2.0
 * windows used in the VideoRenderIosView class.
 */
namespace webrtc {
class OpenGles20 {
 public:
  OpenGles20();
  ~OpenGles20();

  bool Setup(int32_t width, int32_t height);
  bool Render(const I420VideoFrame& frame);

  // SetCoordinates
  // Sets the coordinates where the stream shall be rendered.
  // Values must be between 0 and 1.
  bool SetCoordinates(const float z_order,
                      const float left,
                      const float top,
                      const float right,
                      const float bottom);

 private:
  // Compile and load the vertex and fragment shaders defined at the top of
  // open_gles20.mm
  GLuint LoadShader(GLenum shader_type, const char* shader_source);

  GLuint CreateProgram(const char* vertex_source, const char* fragment_source);

  // Initialize the textures by the frame width and height
  void SetupTextures(const I420VideoFrame& frame);

  // Update the textures by the YUV data from the frame
  void UpdateTextures(const I420VideoFrame& frame);

  GLuint texture_ids_[3];  // Texture id of Y,U and V texture.
  GLuint program_;
  GLsizei texture_width_;
  GLsizei texture_height_;

  GLfloat vertices_[20];
  static const char indices_[];
  static const char vertext_shader_[];
  static const char fragment_shader_[];
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_RENDER_IOS_OPEN_GLES20_H_
