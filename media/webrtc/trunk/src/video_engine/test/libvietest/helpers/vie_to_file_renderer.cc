/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/libvietest/include/vie_to_file_renderer.h"

#include <assert.h>

ViEToFileRenderer::ViEToFileRenderer()
    : output_file_(NULL) {
}

ViEToFileRenderer::~ViEToFileRenderer() {
}

bool ViEToFileRenderer::PrepareForRendering(
    const std::string& output_path,
    const std::string& output_filename) {

  assert(output_file_ == NULL);

  output_file_ = std::fopen((output_path + output_filename).c_str(), "wb");
  if (output_file_ == NULL) {
    return false;
  }

  output_filename_ = output_filename;
  output_path_ = output_path;
  return true;
}

void ViEToFileRenderer::StopRendering() {
  assert(output_file_ != NULL);
  std::fclose(output_file_);
  output_file_ = NULL;
}

bool ViEToFileRenderer::SaveOutputFile(const std::string& prefix) {
  assert(output_file_ == NULL && output_filename_ != "");
  if (std::rename((output_path_ + output_filename_).c_str(),
                  (output_path_ + prefix + output_filename_).c_str()) != 0) {
    std::perror("Failed to rename output file");
    return false;
  }
  ForgetOutputFile();
  return true;
}

bool ViEToFileRenderer::DeleteOutputFile() {
  assert(output_file_ == NULL && output_filename_ != "");
  if (std::remove((output_path_ + output_filename_).c_str()) != 0) {
    std::perror("Failed to delete output file");
    return false;
  }
  ForgetOutputFile();
  return true;
}

const std::string ViEToFileRenderer::GetFullOutputPath() const {
  return output_path_ + output_filename_;
}

void ViEToFileRenderer::ForgetOutputFile() {
  output_filename_ = "";
  output_path_ = "";
}

int ViEToFileRenderer::DeliverFrame(unsigned char *buffer,
                                    int buffer_size,
                                    uint32_t time_stamp,
                                    int64_t render_time) {
  assert(output_file_ != NULL);

  int written = std::fwrite(buffer, sizeof(unsigned char),
                            buffer_size, output_file_);

  if (written == buffer_size) {
    return 0;
  } else {
    return -1;
  }
}

int ViEToFileRenderer::FrameSizeChange(unsigned int width,
                                       unsigned int height,
                                       unsigned int number_of_streams) {
  return 0;
}
