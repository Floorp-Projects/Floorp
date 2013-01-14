/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>

#include "video_render_opengles20.h"

//#define ANDROID_LOG

#ifdef ANDROID_LOG
#include <stdio.h>
#include <android/log.h>

#undef WEBRTC_TRACE
#define WEBRTC_TRACE(a,b,c,...)  __android_log_print(ANDROID_LOG_DEBUG, "*WEBRTCN*", __VA_ARGS__)
#else
#include "trace.h"
#endif

namespace webrtc {

const char VideoRenderOpenGles20::g_indices[] = { 0, 3, 2, 0, 2, 1 };

const char VideoRenderOpenGles20::g_vertextShader[] = {
  "attribute vec4 aPosition;\n"
  "attribute vec2 aTextureCoord;\n"
  "varying vec2 vTextureCoord;\n"
  "void main() {\n"
  "  gl_Position = aPosition;\n"
  "  vTextureCoord = aTextureCoord;\n"
  "}\n" };

// The fragment shader.
// Do YUV to RGB565 conversion.
const char VideoRenderOpenGles20::g_fragmentShader[] = {
  "precision mediump float;\n"
  "uniform sampler2D Ytex;\n"
  "uniform sampler2D Utex,Vtex;\n"
  "varying vec2 vTextureCoord;\n"
  "void main(void) {\n"
  "  float nx,ny,r,g,b,y,u,v;\n"
  "  mediump vec4 txl,ux,vx;"
  "  nx=vTextureCoord[0];\n"
  "  ny=vTextureCoord[1];\n"
  "  y=texture2D(Ytex,vec2(nx,ny)).r;\n"
  "  u=texture2D(Utex,vec2(nx,ny)).r;\n"
  "  v=texture2D(Vtex,vec2(nx,ny)).r;\n"

  //"  y = v;\n"+
  "  y=1.1643*(y-0.0625);\n"
  "  u=u-0.5;\n"
  "  v=v-0.5;\n"

  "  r=y+1.5958*v;\n"
  "  g=y-0.39173*u-0.81290*v;\n"
  "  b=y+2.017*u;\n"
  "  gl_FragColor=vec4(r,g,b,1.0);\n"
  "}\n" };

VideoRenderOpenGles20::VideoRenderOpenGles20(WebRtc_Word32 id) :
    _id(id),
    _textureWidth(-1),
    _textureHeight(-1) {
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s: id %d",
               __FUNCTION__, (int) _id);

  const GLfloat vertices[20] = {
    // X, Y, Z, U, V
    -1, -1, 0, 0, 1, // Bottom Left
    1, -1, 0, 1, 1, //Bottom Right
    1, 1, 0, 1, 0, //Top Right
    -1, 1, 0, 0, 0 }; //Top Left

  memcpy(_vertices, vertices, sizeof(_vertices));
}

VideoRenderOpenGles20::~VideoRenderOpenGles20() {
}

WebRtc_Word32 VideoRenderOpenGles20::Setup(WebRtc_Word32 width,
                                           WebRtc_Word32 height) {
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id,
               "%s: width %d, height %d", __FUNCTION__, (int) width,
               (int) height);

  printGLString("Version", GL_VERSION);
  printGLString("Vendor", GL_VENDOR);
  printGLString("Renderer", GL_RENDERER);
  printGLString("Extensions", GL_EXTENSIONS);

  int maxTextureImageUnits[2];
  int maxTextureSize[2];
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, maxTextureImageUnits);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, maxTextureSize);

  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id,
               "%s: number of textures %d, size %d", __FUNCTION__,
               (int) maxTextureImageUnits[0], (int) maxTextureSize[0]);

  _program = createProgram(g_vertextShader, g_fragmentShader);
  if (!_program) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: Could not create program", __FUNCTION__);
    return -1;
  }

  int positionHandle = glGetAttribLocation(_program, "aPosition");
  checkGlError("glGetAttribLocation aPosition");
  if (positionHandle == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: Could not get aPosition handle", __FUNCTION__);
    return -1;
  }

  int textureHandle = glGetAttribLocation(_program, "aTextureCoord");
  checkGlError("glGetAttribLocation aTextureCoord");
  if (textureHandle == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: Could not get aTextureCoord handle", __FUNCTION__);
    return -1;
  }

  // set the vertices array in the shader
  // _vertices contains 4 vertices with 5 coordinates.
  // 3 for (xyz) for the vertices and 2 for the texture
  glVertexAttribPointer(positionHandle, 3, GL_FLOAT, false,
                        5 * sizeof(GLfloat), _vertices);
  checkGlError("glVertexAttribPointer aPosition");

  glEnableVertexAttribArray(positionHandle);
  checkGlError("glEnableVertexAttribArray positionHandle");

  // set the texture coordinate array in the shader
  // _vertices contains 4 vertices with 5 coordinates.
  // 3 for (xyz) for the vertices and 2 for the texture
  glVertexAttribPointer(textureHandle, 2, GL_FLOAT, false, 5
                        * sizeof(GLfloat), &_vertices[3]);
  checkGlError("glVertexAttribPointer maTextureHandle");
  glEnableVertexAttribArray(textureHandle);
  checkGlError("glEnableVertexAttribArray textureHandle");

  glUseProgram(_program);
  int i = glGetUniformLocation(_program, "Ytex");
  checkGlError("glGetUniformLocation");
  glUniform1i(i, 0); /* Bind Ytex to texture unit 0 */
  checkGlError("glUniform1i Ytex");

  i = glGetUniformLocation(_program, "Utex");
  checkGlError("glGetUniformLocation Utex");
  glUniform1i(i, 1); /* Bind Utex to texture unit 1 */
  checkGlError("glUniform1i Utex");

  i = glGetUniformLocation(_program, "Vtex");
  checkGlError("glGetUniformLocation");
  glUniform1i(i, 2); /* Bind Vtex to texture unit 2 */
  checkGlError("glUniform1i");

  glViewport(0, 0, width, height);
  checkGlError("glViewport");
  return 0;
}

// SetCoordinates
// Sets the coordinates where the stream shall be rendered.
// Values must be between 0 and 1.
WebRtc_Word32 VideoRenderOpenGles20::SetCoordinates(WebRtc_Word32 zOrder,
                                                    const float left,
                                                    const float top,
                                                    const float right,
                                                    const float bottom) {
  if ((top > 1 || top < 0) || (right > 1 || right < 0) ||
      (bottom > 1 || bottom < 0) || (left > 1 || left < 0)) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "%s: Wrong coordinates", __FUNCTION__);
    return -1;
  }

  //  X, Y, Z, U, V
  // -1, -1, 0, 0, 1, // Bottom Left
  //  1, -1, 0, 1, 1, //Bottom Right
  //  1,  1, 0, 1, 0, //Top Right
  // -1,  1, 0, 0, 0  //Top Left

  // Bottom Left
  _vertices[0] = (left * 2) - 1;
  _vertices[1] = -1 * (2 * bottom) + 1;
  _vertices[2] = zOrder;

  //Bottom Right
  _vertices[5] = (right * 2) - 1;
  _vertices[6] = -1 * (2 * bottom) + 1;
  _vertices[7] = zOrder;

  //Top Right
  _vertices[10] = (right * 2) - 1;
  _vertices[11] = -1 * (2 * top) + 1;
  _vertices[12] = zOrder;

  //Top Left
  _vertices[15] = (left * 2) - 1;
  _vertices[16] = -1 * (2 * top) + 1;
  _vertices[17] = zOrder;

  return 0;
}

WebRtc_Word32 VideoRenderOpenGles20::Render(const I420VideoFrame&
                                            frameToRender) {

  if (frameToRender.IsZeroSize()) {
    return -1;
  }

  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s: id %d",
               __FUNCTION__, (int) _id);

  glUseProgram(_program);
  checkGlError("glUseProgram");

  if (_textureWidth != (GLsizei) frameToRender.width() ||
      _textureHeight != (GLsizei) frameToRender.height()) {
    SetupTextures(frameToRender);
  }
  else {
    UpdateTextures(frameToRender);
  }

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, g_indices);
  checkGlError("glDrawArrays");

  return 0;
}

GLuint VideoRenderOpenGles20::loadShader(GLenum shaderType,
                                         const char* pSource) {
  GLuint shader = glCreateShader(shaderType);
  if (shader) {
    glShaderSource(shader, 1, &pSource, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
      GLint infoLen = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
      if (infoLen) {
        char* buf = (char*) malloc(infoLen);
        if (buf) {
          glGetShaderInfoLog(shader, infoLen, NULL, buf);
          WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                       "%s: Could not compile shader %d: %s",
                       __FUNCTION__, shaderType, buf);
          free(buf);
        }
        glDeleteShader(shader);
        shader = 0;
      }
    }
  }
  return shader;
}

GLuint VideoRenderOpenGles20::createProgram(const char* pVertexSource,
                                            const char* pFragmentSource) {
  GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
  if (!vertexShader) {
    return 0;
  }

  GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
  if (!pixelShader) {
    return 0;
  }

  GLuint program = glCreateProgram();
  if (program) {
    glAttachShader(program, vertexShader);
    checkGlError("glAttachShader");
    glAttachShader(program, pixelShader);
    checkGlError("glAttachShader");
    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
      GLint bufLength = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
      if (bufLength) {
        char* buf = (char*) malloc(bufLength);
        if (buf) {
          glGetProgramInfoLog(program, bufLength, NULL, buf);
          WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                       "%s: Could not link program: %s",
                       __FUNCTION__, buf);
          free(buf);
        }
      }
      glDeleteProgram(program);
      program = 0;
    }
  }
  return program;
}

void VideoRenderOpenGles20::printGLString(const char *name, GLenum s) {
  const char *v = (const char *) glGetString(s);
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "GL %s = %s\n",
               name, v);
}

void VideoRenderOpenGles20::checkGlError(const char* op) {
#ifdef ANDROID_LOG
  for (GLint error = glGetError(); error; error = glGetError()) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "after %s() glError (0x%x)\n", op, error);
  }
#else
  return;
#endif
}

void VideoRenderOpenGles20::SetupTextures(const I420VideoFrame& frameToRender) {
  WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id,
               "%s: width %d, height %d", __FUNCTION__,
               frameToRender.width(), frameToRender.height());

  const GLsizei width = frameToRender.width();
  const GLsizei height = frameToRender.height();

  glGenTextures(3, _textureIds); //Generate  the Y, U and V texture
  GLuint currentTextureId = _textureIds[0]; // Y
  glActiveTexture( GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, currentTextureId);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE,
               (const GLvoid*) frameToRender.buffer(kYPlane));

  currentTextureId = _textureIds[1]; // U
  glActiveTexture( GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, currentTextureId);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  const WebRtc_UWord8* uComponent = frameToRender.buffer(kUPlane);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, (const GLvoid*) uComponent);

  currentTextureId = _textureIds[2]; // V
  glActiveTexture( GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, currentTextureId);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  const WebRtc_UWord8* vComponent = frameToRender.buffer(kVPlane);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, (const GLvoid*) vComponent);
  checkGlError("SetupTextures");

  _textureWidth = width;
  _textureHeight = height;
}

// Uploads a plane of pixel data, accounting for stride != width*bpp.
static void GlTexSubImage2D(GLsizei width, GLsizei height, int stride,
                            const uint8_t* plane) {
  if (stride == width) {
    // Yay!  We can upload the entire plane in a single GL call.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE,
                    static_cast<const GLvoid*>(plane));
  } else {
    // Boo!  Since GLES2 doesn't have GL_UNPACK_ROW_LENGTH and Android doesn't
    // have GL_EXT_unpack_subimage we have to upload a row at a time.  Ick.
    for (int row = 0; row < height; ++row) {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, width, 1, GL_LUMINANCE,
                      GL_UNSIGNED_BYTE,
                      static_cast<const GLvoid*>(plane + (row * stride)));
    }
  }
}

void VideoRenderOpenGles20::UpdateTextures(const
                                           I420VideoFrame& frameToRender) {
  const GLsizei width = frameToRender.width();
  const GLsizei height = frameToRender.height();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _textureIds[0]);
  GlTexSubImage2D(width, height, frameToRender.stride(kYPlane),
                  frameToRender.buffer(kYPlane));

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _textureIds[1]);
  GlTexSubImage2D(width / 2, height / 2, frameToRender.stride(kUPlane),
                  frameToRender.buffer(kUPlane));

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, _textureIds[2]);
  GlTexSubImage2D(width / 2, height / 2, frameToRender.stride(kVPlane),
                  frameToRender.buffer(kVPlane));

  checkGlError("UpdateTextures");
}

}  // namespace webrtc
