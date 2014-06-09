/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/frame_generator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {
namespace test {
namespace {

class ChromaGenerator : public FrameGenerator {
 public:
  ChromaGenerator(size_t width, size_t height)
      : angle_(0.0), width_(width), height_(height) {
    assert(width > 0);
    assert(height > 0);
  }

  virtual I420VideoFrame* NextFrame() OVERRIDE {
    frame_.CreateEmptyFrame(static_cast<int>(width_),
                            static_cast<int>(height_),
                            static_cast<int>(width_),
                            static_cast<int>((width_ + 1) / 2),
                            static_cast<int>((width_ + 1) / 2));
    angle_ += 30.0;
    uint8_t u = fabs(sin(angle_)) * 0xFF;
    uint8_t v = fabs(cos(angle_)) * 0xFF;

    memset(frame_.buffer(kYPlane), 0x80, frame_.allocated_size(kYPlane));
    memset(frame_.buffer(kUPlane), u, frame_.allocated_size(kUPlane));
    memset(frame_.buffer(kVPlane), v, frame_.allocated_size(kVPlane));
    return &frame_;
  }

 private:
  double angle_;
  size_t width_;
  size_t height_;
  I420VideoFrame frame_;
};

class YuvFileGenerator : public FrameGenerator {
 public:
  YuvFileGenerator(FILE* file, size_t width, size_t height)
      : file_(file), width_(width), height_(height) {
    assert(file);
    assert(width > 0);
    assert(height > 0);
    frame_size_ = CalcBufferSize(
        kI420, static_cast<int>(width_), static_cast<int>(height_));
    frame_buffer_ = new uint8_t[frame_size_];
  }

  virtual ~YuvFileGenerator() {
    fclose(file_);
    delete[] frame_buffer_;
  }

  virtual I420VideoFrame* NextFrame() OVERRIDE {
    size_t count = fread(frame_buffer_, 1, frame_size_, file_);
    if (count < frame_size_) {
      rewind(file_);
      return NextFrame();
    }

    frame_.CreateEmptyFrame(static_cast<int>(width_),
                            static_cast<int>(height_),
                            static_cast<int>(width_),
                            static_cast<int>((width_ + 1) / 2),
                            static_cast<int>((width_ + 1) / 2));

    ConvertToI420(kI420,
                  frame_buffer_,
                  0,
                  0,
                  static_cast<int>(width_),
                  static_cast<int>(height_),
                  0,
                  kRotateNone,
                  &frame_);
    return &frame_;
  }

 private:
  FILE* file_;
  size_t width_;
  size_t height_;
  size_t frame_size_;
  uint8_t* frame_buffer_;
  I420VideoFrame frame_;
};
}  // namespace

FrameGenerator* FrameGenerator::Create(size_t width, size_t height) {
  return new ChromaGenerator(width, height);
}

FrameGenerator* FrameGenerator::CreateFromYuvFile(const char* file,
                                                  size_t width,
                                                  size_t height) {
  FILE* file_handle = fopen(file, "rb");
  assert(file_handle);
  return new YuvFileGenerator(file_handle, width, height);
}

}  // namespace test
}  // namespace webrtc
