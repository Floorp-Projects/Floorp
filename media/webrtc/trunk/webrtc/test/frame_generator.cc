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

#include "webrtc/base/checks.h"
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

  I420VideoFrame* NextFrame() override {
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
  YuvFileGenerator(std::vector<FILE*> files,
                   size_t width,
                   size_t height,
                   int frame_repeat_count)
      : file_index_(0),
        files_(files),
        width_(width),
        height_(height),
        frame_size_(CalcBufferSize(kI420,
                                   static_cast<int>(width_),
                                   static_cast<int>(height_))),
        frame_buffer_(new uint8_t[frame_size_]),
        frame_display_count_(frame_repeat_count),
        current_display_count_(0) {
    assert(width > 0);
    assert(height > 0);
    assert(frame_repeat_count > 0);
  }

  virtual ~YuvFileGenerator() {
    for (FILE* file : files_)
      fclose(file);
  }

  I420VideoFrame* NextFrame() override {
    if (current_display_count_ == 0)
      ReadNextFrame();
    if (++current_display_count_ >= frame_display_count_)
      current_display_count_ = 0;

    // If this is the last repeatition of this frame, it's OK to use the
    // original instance, otherwise use a copy.
    if (current_display_count_ == frame_display_count_)
      return &last_read_frame_;

    temp_frame_copy_.CopyFrame(last_read_frame_);
    return &temp_frame_copy_;
  }

  void ReadNextFrame() {
    size_t bytes_read =
        fread(frame_buffer_.get(), 1, frame_size_, files_[file_index_]);
    if (bytes_read < frame_size_) {
      // No more frames to read in this file, rewind and move to next file.
      rewind(files_[file_index_]);
      file_index_ = (file_index_ + 1) % files_.size();
      bytes_read = fread(frame_buffer_.get(), 1, frame_size_,
          files_[file_index_]);
      assert(bytes_read >= frame_size_);
    }

    last_read_frame_.CreateEmptyFrame(
        static_cast<int>(width_), static_cast<int>(height_),
        static_cast<int>(width_), static_cast<int>((width_ + 1) / 2),
        static_cast<int>((width_ + 1) / 2));

    ConvertToI420(kI420, frame_buffer_.get(), 0, 0, static_cast<int>(width_),
                  static_cast<int>(height_), 0, kVideoRotation_0,
                  &last_read_frame_);
  }

 private:
  size_t file_index_;
  const std::vector<FILE*> files_;
  const size_t width_;
  const size_t height_;
  const size_t frame_size_;
  const rtc::scoped_ptr<uint8_t[]> frame_buffer_;
  const int frame_display_count_;
  int current_display_count_;
  I420VideoFrame last_read_frame_;
  I420VideoFrame temp_frame_copy_;
};
}  // namespace

FrameGenerator* FrameGenerator::CreateChromaGenerator(size_t width,
                                                      size_t height) {
  return new ChromaGenerator(width, height);
}

FrameGenerator* FrameGenerator::CreateFromYuvFile(
    std::vector<std::string> filenames,
    size_t width,
    size_t height,
    int frame_repeat_count) {
  assert(!filenames.empty());
  std::vector<FILE*> files;
  for (const std::string& filename : filenames) {
    FILE* file = fopen(filename.c_str(), "rb");
    DCHECK(file != nullptr);
    files.push_back(file);
  }

  return new YuvFileGenerator(files, width, height, frame_repeat_count);
}

}  // namespace test
}  // namespace webrtc
