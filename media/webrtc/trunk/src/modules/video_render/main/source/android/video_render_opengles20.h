/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_OPENGLES20_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_OPENGLES20_H_

#include "video_render_defines.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace webrtc
{

class VideoRenderOpenGles20 {
 public:
  VideoRenderOpenGles20(WebRtc_Word32 id);
  ~VideoRenderOpenGles20();

  WebRtc_Word32 Setup(WebRtc_Word32 widht, WebRtc_Word32 height);
  WebRtc_Word32 Render(const VideoFrame& frameToRender);
  WebRtc_Word32 SetCoordinates(WebRtc_Word32 zOrder,
                               const float left,
                               const float top,
                               const float right,
                               const float bottom);

 private:
  void printGLString(const char *name, GLenum s);
  void checkGlError(const char* op);
  GLuint loadShader(GLenum shaderType, const char* pSource);
  GLuint createProgram(const char* pVertexSource,
                       const char* pFragmentSource);
  void SetupTextures(const VideoFrame& frameToRender);
  void UpdateTextures(const VideoFrame& frameToRender);

  WebRtc_Word32 _id;
  GLuint _textureIds[3]; // Texture id of Y,U and V texture.
  GLuint _program;
  GLuint _vPositionHandle;
  GLsizei _textureWidth;
  GLsizei _textureHeight;

  GLfloat _vertices[20];
  static const char g_indices[];

  static const char g_vertextShader[];
  static const char g_fragmentShader[];

};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_ANDROID_VIDEO_RENDER_OPENGLES20_H_
