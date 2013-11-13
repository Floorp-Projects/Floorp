/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This files is mostly copied from
// webrtc/modules/video_render/android/video_render_opengles20.h

// TODO(sjlee): unify this copy with the android one.
#include "webrtc/modules/video_render/ios/open_gles20.h"
#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;

const char OpenGles20::indices_[] = {0, 3, 2, 0, 2, 1};

const char OpenGles20::vertext_shader_[] = {
    "attribute vec4 aPosition;\n"
    "attribute vec2 aTextureCoord;\n"
    "varying vec2 vTextureCoord;\n"
    "void main() {\n"
    "  gl_Position = aPosition;\n"
    "  vTextureCoord = aTextureCoord;\n"
    "}\n"};

// The fragment shader.
// Do YUV to RGB565 conversion.
const char OpenGles20::fragment_shader_[] = {
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
    "  y=1.1643*(y-0.0625);\n"
    "  u=u-0.5;\n"
    "  v=v-0.5;\n"
    "  r=y+1.5958*v;\n"
    "  g=y-0.39173*u-0.81290*v;\n"
    "  b=y+2.017*u;\n"
    "  gl_FragColor=vec4(r,g,b,1.0);\n"
    "}\n"};

OpenGles20::OpenGles20() : texture_width_(-1), texture_height_(-1) {
  texture_ids_[0] = 0;
  texture_ids_[1] = 0;
  texture_ids_[2] = 0;

  program_ = 0;

  const GLfloat vertices[20] = {
      // X, Y, Z, U, V
      -1, -1, 0, 0, 1,   // Bottom Left
      1,  -1, 0, 1, 1,   // Bottom Right
      1,  1,  0, 1, 0,   // Top Right
      -1, 1,  0, 0, 0};  // Top Left

  memcpy(vertices_, vertices, sizeof(vertices_));
}

OpenGles20::~OpenGles20() {
  if (program_) {
    glDeleteTextures(3, texture_ids_);
    glDeleteProgram(program_);
  }
}

bool OpenGles20::Setup(int32_t width, int32_t height) {
  program_ = CreateProgram(vertext_shader_, fragment_shader_);
  if (!program_) {
    return false;
  }

  int position_handle = glGetAttribLocation(program_, "aPosition");
  int texture_handle = glGetAttribLocation(program_, "aTextureCoord");

  // set the vertices array in the shader
  // vertices_ contains 4 vertices with 5 coordinates.
  // 3 for (xyz) for the vertices and 2 for the texture
  glVertexAttribPointer(
      position_handle, 3, GL_FLOAT, false, 5 * sizeof(GLfloat), vertices_);

  glEnableVertexAttribArray(position_handle);

  // set the texture coordinate array in the shader
  // vertices_ contains 4 vertices with 5 coordinates.
  // 3 for (xyz) for the vertices and 2 for the texture
  glVertexAttribPointer(
      texture_handle, 2, GL_FLOAT, false, 5 * sizeof(GLfloat), &vertices_[3]);
  glEnableVertexAttribArray(texture_handle);

  glUseProgram(program_);
  int i = glGetUniformLocation(program_, "Ytex");
  glUniform1i(i, 0); /* Bind Ytex to texture unit 0 */

  i = glGetUniformLocation(program_, "Utex");
  glUniform1i(i, 1); /* Bind Utex to texture unit 1 */

  i = glGetUniformLocation(program_, "Vtex");
  glUniform1i(i, 2); /* Bind Vtex to texture unit 2 */

  glViewport(0, 0, width, height);
  return true;
}

bool OpenGles20::SetCoordinates(const float z_order,
                                const float left,
                                const float top,
                                const float right,
                                const float bottom) {
  if (top > 1 || top < 0 || right > 1 || right < 0 || bottom > 1 ||
      bottom < 0 || left > 1 || left < 0) {
    return false;
  }

  // Bottom Left
  vertices_[0] = (left * 2) - 1;
  vertices_[1] = -1 * (2 * bottom) + 1;
  vertices_[2] = z_order;

  // Bottom Right
  vertices_[5] = (right * 2) - 1;
  vertices_[6] = -1 * (2 * bottom) + 1;
  vertices_[7] = z_order;

  // Top Right
  vertices_[10] = (right * 2) - 1;
  vertices_[11] = -1 * (2 * top) + 1;
  vertices_[12] = z_order;

  // Top Left
  vertices_[15] = (left * 2) - 1;
  vertices_[16] = -1 * (2 * top) + 1;
  vertices_[17] = z_order;

  return true;
}

bool OpenGles20::Render(const I420VideoFrame& frame) {
  if (texture_width_ != (GLsizei)frame.width() ||
      texture_height_ != (GLsizei)frame.height()) {
    SetupTextures(frame);
  }
  UpdateTextures(frame);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices_);

  return true;
}

GLuint OpenGles20::LoadShader(GLenum shader_type, const char* shader_source) {
  GLuint shader = glCreateShader(shader_type);
  if (shader) {
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
      GLint info_len = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
      if (info_len) {
        char* buf = (char*)malloc(info_len);
        glGetShaderInfoLog(shader, info_len, NULL, buf);
        WEBRTC_TRACE(kTraceError,
                     kTraceVideoRenderer,
                     0,
                     "%s: Could not compile shader %d: %s",
                     __FUNCTION__,
                     shader_type,
                     buf);
        free(buf);
      }
      glDeleteShader(shader);
      shader = 0;
    }
  }
  return shader;
}

GLuint OpenGles20::CreateProgram(const char* vertex_source,
                                 const char* fragment_source) {
  GLuint vertex_shader = LoadShader(GL_VERTEX_SHADER, vertex_source);
  if (!vertex_shader) {
    return -1;
  }

  GLuint fragment_shader = LoadShader(GL_FRAGMENT_SHADER, fragment_source);
  if (!fragment_shader) {
    return -1;
  }

  GLuint program = glCreateProgram();
  if (program) {
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint link_status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE) {
      GLint info_len = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
      if (info_len) {
        char* buf = (char*)malloc(info_len);
        glGetProgramInfoLog(program, info_len, NULL, buf);
        WEBRTC_TRACE(kTraceError,
                     kTraceVideoRenderer,
                     0,
                     "%s: Could not link program: %s",
                     __FUNCTION__,
                     buf);
        free(buf);
      }
      glDeleteProgram(program);
      program = 0;
    }
  }

  if (vertex_shader) {
    glDeleteShader(vertex_shader);
  }

  if (fragment_shader) {
    glDeleteShader(fragment_shader);
  }

  return program;
}

static void InitializeTexture(int name, int id, int width, int height) {
  glActiveTexture(name);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_LUMINANCE,
               width,
               height,
               0,
               GL_LUMINANCE,
               GL_UNSIGNED_BYTE,
               NULL);
}

void OpenGles20::SetupTextures(const I420VideoFrame& frame) {
  const GLsizei width = frame.width();
  const GLsizei height = frame.height();

  if (!texture_ids_[0]) {
    glGenTextures(3, texture_ids_);  // Generate  the Y, U and V texture
  }

  InitializeTexture(GL_TEXTURE0, texture_ids_[0], width, height);
  InitializeTexture(GL_TEXTURE1, texture_ids_[1], width / 2, height / 2);
  InitializeTexture(GL_TEXTURE2, texture_ids_[2], width / 2, height / 2);

  texture_width_ = width;
  texture_height_ = height;
}

// Uploads a plane of pixel data, accounting for stride != width*bpp.
static void GlTexSubImage2D(GLsizei width,
                            GLsizei height,
                            int stride,
                            const uint8_t* plane) {
  if (stride == width) {
    // Yay!  We can upload the entire plane in a single GL call.
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    width,
                    height,
                    GL_LUMINANCE,
                    GL_UNSIGNED_BYTE,
                    static_cast<const GLvoid*>(plane));
  } else {
    // Boo!  Since GLES2 doesn't have GL_UNPACK_ROW_LENGTH and iOS doesn't
    // have GL_EXT_unpack_subimage we have to upload a row at a time.  Ick.
    for (int row = 0; row < height; ++row) {
      glTexSubImage2D(GL_TEXTURE_2D,
                      0,
                      0,
                      row,
                      width,
                      1,
                      GL_LUMINANCE,
                      GL_UNSIGNED_BYTE,
                      static_cast<const GLvoid*>(plane + (row * stride)));
    }
  }
}

void OpenGles20::UpdateTextures(const I420VideoFrame& frame) {
  const GLsizei width = frame.width();
  const GLsizei height = frame.height();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_ids_[0]);
  GlTexSubImage2D(width, height, frame.stride(kYPlane), frame.buffer(kYPlane));

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texture_ids_[1]);
  GlTexSubImage2D(
      width / 2, height / 2, frame.stride(kUPlane), frame.buffer(kUPlane));

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, texture_ids_[2]);
  GlTexSubImage2D(
      width / 2, height / 2, frame.stride(kVPlane), frame.buffer(kVPlane));
}
