/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <string>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {
namespace {

// Size of header: "YUV4MPEG2 WXXXXXX HXXXXXX FXXX:1 C420\n"
// We allocate up to 6 digits for width and height and up to 3 digits for FPS.
const size_t kFileHeaderMaxSize = 38;
// Size of header: "YUV4MPEG2 WX HX FX:1 C420\n"
const size_t kFileHeaderMinSize = 26;
// Size of header: "FRAME\n"
const size_t kFrameHeaderSize = 6;

std::string GetExpectedHeaderPrefix(int width, int height) {
  rtc::StringBuilder out;
  out << "YUV4MPEG2 W" << width << " H" << height << " F";
  return out.str();
}

}  // namespace

Y4mFrameReaderImpl::Y4mFrameReaderImpl(std::string input_filename,
                                       int width,
                                       int height)
    : YuvFrameReaderImpl(input_filename, width, height) {
  frame_length_in_bytes_ += kFrameHeaderSize;
  buffer_ = new char[kFileHeaderMaxSize];
}
Y4mFrameReaderImpl::~Y4mFrameReaderImpl() {
  delete[] buffer_;
}

bool Y4mFrameReaderImpl::Init() {
  if (input_width_ <= 0 || input_height_ <= 0) {
    RTC_LOG(LS_ERROR) << "Frame width and height must be positive. Was: "
                      << input_width_ << "x" << input_height_;
    return false;
  }
  input_file_ = fopen(input_filename_.c_str(), "rb");
  if (input_file_ == nullptr) {
    RTC_LOG(LS_ERROR) << "Couldn't open input file: " << input_filename_;
    return false;
  }
  size_t source_file_size = GetFileSize(input_filename_);
  if (source_file_size <= 0u) {
    RTC_LOG(LS_ERROR) << "Input file " << input_filename_ << " is empty.";
    return false;
  }
  char* c_file_header = fgets(buffer_, kFileHeaderMaxSize, input_file_);
  std::string file_header(c_file_header);
  if (file_header.size() < kFileHeaderMinSize) {
    RTC_LOG(LS_ERROR) << "Couldn't read Y4M header from input file: "
                      << input_filename_;
    return false;
  }
  if (file_header.find(GetExpectedHeaderPrefix(input_width_, input_height_)) !=
      0) {
    RTC_LOG(LS_ERROR) << "Couldn't read Y4M file: " << input_filename_
                      << ". Input file has different resolution, expected: "
                      << GetExpectedHeaderPrefix(input_width_, input_height_)
                      << "[0-9]?:1 C420; got: " << file_header;
    return false;
  }

  number_of_frames_ = static_cast<int>((source_file_size - file_header.size()) /
                                       frame_length_in_bytes_);

  if (number_of_frames_ == 0) {
    RTC_LOG(LS_ERROR) << "Input file " << input_filename_ << " is too small.";
  }
  return true;
}

rtc::scoped_refptr<I420Buffer> Y4mFrameReaderImpl::ReadFrame() {
  if (input_file_ == nullptr) {
    RTC_LOG(LS_ERROR) << "Y4mFrameReaderImpl is not initialized.";
    return nullptr;
  }
  if (fread(buffer_, 1, kFrameHeaderSize, input_file_) < kFrameHeaderSize &&
      ferror(input_file_)) {
    RTC_LOG(LS_ERROR) << "Couldn't read frame header from input file: "
                      << input_filename_;
    return nullptr;
  }
  return YuvFrameReaderImpl::ReadFrame();
}

}  // namespace test
}  // namespace webrtc
