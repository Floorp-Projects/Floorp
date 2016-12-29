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
#include "webrtc/system_wrappers/include/clock.h"

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

  VideoFrame* NextFrame() override {
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
  VideoFrame frame_;
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

  VideoFrame* NextFrame() override {
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
  VideoFrame last_read_frame_;
  VideoFrame temp_frame_copy_;
};

class ScrollingImageFrameGenerator : public FrameGenerator {
 public:
  ScrollingImageFrameGenerator(Clock* clock,
                               const std::vector<FILE*>& files,
                               size_t source_width,
                               size_t source_height,
                               size_t target_width,
                               size_t target_height,
                               int64_t scroll_time_ms,
                               int64_t pause_time_ms)
      : clock_(clock),
        start_time_(clock->TimeInMilliseconds()),
        scroll_time_(scroll_time_ms),
        pause_time_(pause_time_ms),
        num_frames_(files.size()),
        current_frame_num_(num_frames_ - 1),
        current_source_frame_(nullptr),
        file_generator_(files, source_width, source_height, 1) {
    RTC_DCHECK(clock_ != nullptr);
    RTC_DCHECK_GT(num_frames_, 0u);
    RTC_DCHECK_GE(source_height, target_height);
    RTC_DCHECK_GE(source_width, target_width);
    RTC_DCHECK_GE(scroll_time_ms, 0);
    RTC_DCHECK_GE(pause_time_ms, 0);
    RTC_DCHECK_GT(scroll_time_ms + pause_time_ms, 0);
    current_frame_.CreateEmptyFrame(static_cast<int>(target_width),
                                    static_cast<int>(target_height),
                                    static_cast<int>(target_width),
                                    static_cast<int>((target_width + 1) / 2),
                                    static_cast<int>((target_width + 1) / 2));
  }

  virtual ~ScrollingImageFrameGenerator() {}

  VideoFrame* NextFrame() override {
    const int64_t kFrameDisplayTime = scroll_time_ + pause_time_;
    const int64_t now = clock_->TimeInMilliseconds();
    int64_t ms_since_start = now - start_time_;

    size_t frame_num = (ms_since_start / kFrameDisplayTime) % num_frames_;
    UpdateSourceFrame(frame_num);

    double scroll_factor;
    int64_t time_into_frame = ms_since_start % kFrameDisplayTime;
    if (time_into_frame < scroll_time_) {
      scroll_factor = static_cast<double>(time_into_frame) / scroll_time_;
    } else {
      scroll_factor = 1.0;
    }
    CropSourceToScrolledImage(scroll_factor);

    return &current_frame_;
  }

  void UpdateSourceFrame(size_t frame_num) {
    while (current_frame_num_ != frame_num) {
      current_source_frame_ = file_generator_.NextFrame();
      current_frame_num_ = (current_frame_num_ + 1) % num_frames_;
    }
    RTC_DCHECK(current_source_frame_ != nullptr);
  }

  void CropSourceToScrolledImage(double scroll_factor) {
    const int kTargetWidth = current_frame_.width();
    const int kTargetHeight = current_frame_.height();
    int scroll_margin_x = current_source_frame_->width() - kTargetWidth;
    int pixels_scrolled_x =
        static_cast<int>(scroll_margin_x * scroll_factor + 0.5);
    int scroll_margin_y = current_source_frame_->height() - kTargetHeight;
    int pixels_scrolled_y =
        static_cast<int>(scroll_margin_y * scroll_factor + 0.5);

    int offset_y = (current_source_frame_->stride(PlaneType::kYPlane) *
                    pixels_scrolled_y) +
                   pixels_scrolled_x;
    int offset_u = (current_source_frame_->stride(PlaneType::kUPlane) *
                    (pixels_scrolled_y / 2)) +
                   (pixels_scrolled_x / 2);
    int offset_v = (current_source_frame_->stride(PlaneType::kVPlane) *
                    (pixels_scrolled_y / 2)) +
                   (pixels_scrolled_x / 2);

    current_frame_.CreateFrame(
        &current_source_frame_->buffer(PlaneType::kYPlane)[offset_y],
        &current_source_frame_->buffer(PlaneType::kUPlane)[offset_u],
        &current_source_frame_->buffer(PlaneType::kVPlane)[offset_v],
        kTargetWidth, kTargetHeight,
        current_source_frame_->stride(PlaneType::kYPlane),
        current_source_frame_->stride(PlaneType::kUPlane),
        current_source_frame_->stride(PlaneType::kVPlane));
  }

  Clock* const clock_;
  const int64_t start_time_;
  const int64_t scroll_time_;
  const int64_t pause_time_;
  const size_t num_frames_;
  size_t current_frame_num_;
  VideoFrame* current_source_frame_;
  VideoFrame current_frame_;
  YuvFileGenerator file_generator_;
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
    RTC_DCHECK(file != nullptr);
    files.push_back(file);
  }

  return new YuvFileGenerator(files, width, height, frame_repeat_count);
}

FrameGenerator* FrameGenerator::CreateScrollingInputFromYuvFiles(
    Clock* clock,
    std::vector<std::string> filenames,
    size_t source_width,
    size_t source_height,
    size_t target_width,
    size_t target_height,
    int64_t scroll_time_ms,
    int64_t pause_time_ms) {
  assert(!filenames.empty());
  std::vector<FILE*> files;
  for (const std::string& filename : filenames) {
    FILE* file = fopen(filename.c_str(), "rb");
    RTC_DCHECK(file != nullptr);
    files.push_back(file);
  }

  return new ScrollingImageFrameGenerator(
      clock, files, source_width, source_height, target_width, target_height,
      scroll_time_ms, pause_time_ms);
}

}  // namespace test
}  // namespace webrtc
