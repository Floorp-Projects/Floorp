/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/frame_reader.h"

#include <cassert>

#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

FrameReaderImpl::FrameReaderImpl(std::string input_filename,
                                 size_t frame_length_in_bytes)
    : input_filename_(input_filename),
      frame_length_in_bytes_(frame_length_in_bytes),
      input_file_(NULL) {
}

FrameReaderImpl::~FrameReaderImpl() {
  Close();
}

bool FrameReaderImpl::Init() {
  if (frame_length_in_bytes_ <= 0) {
    fprintf(stderr, "Frame length must be >0, was %zu\n",
            frame_length_in_bytes_);
    return false;
  }
  input_file_ = fopen(input_filename_.c_str(), "rb");
  if (input_file_ == NULL) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            input_filename_.c_str());
    return false;
  }
  // Calculate total number of frames.
  size_t source_file_size = GetFileSize(input_filename_);
  if (source_file_size <= 0u) {
    fprintf(stderr, "Found empty file: %s\n", input_filename_.c_str());
    return false;
  }
  number_of_frames_ = static_cast<int>(source_file_size /
                                       frame_length_in_bytes_);
  return true;
}

void FrameReaderImpl::Close() {
  if (input_file_ != NULL) {
    fclose(input_file_);
    input_file_ = NULL;
  }
}

bool FrameReaderImpl::ReadFrame(uint8_t* source_buffer) {
  assert(source_buffer);
  if (input_file_ == NULL) {
    fprintf(stderr, "FrameReader is not initialized (input file is NULL)\n");
    return false;
  }
  size_t nbr_read = fread(source_buffer, 1, frame_length_in_bytes_,
                          input_file_);
  if (nbr_read != static_cast<unsigned int>(frame_length_in_bytes_) &&
      ferror(input_file_)) {
    fprintf(stderr, "Error reading from input file: %s\n",
            input_filename_.c_str());
    return false;
  }
  if (feof(input_file_) != 0) {
    return false;  // No more frames to process.
  }
  return true;
}

}  // namespace test
}  // namespace webrtc
