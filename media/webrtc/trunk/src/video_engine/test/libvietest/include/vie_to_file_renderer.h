/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_VIE_TO_FILE_RENDERER_H_
#define SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_VIE_TO_FILE_RENDERER_H_

#include <cstdio>
#include <string>

#include "video_engine/include/vie_render.h"

class ViEToFileRenderer: public webrtc::ExternalRenderer {
 public:
  ViEToFileRenderer();
  virtual ~ViEToFileRenderer();

  // Returns false if we fail opening the output filename for writing.
  bool PrepareForRendering(const std::string& output_path,
                           const std::string& output_filename);

  // Closes the output file.
  void StopRendering();

  // Deletes the closed output file from the file system. This is one option
  // after calling StopRendering, the other being KeepOutputFile. This file
  // renderer will forget about the file after this call and can be used again.
  bool DeleteOutputFile();

  // Renames the closed output file to its previous name with the provided
  // prefix prepended. This file renderer will forget about the file after this
  // call and can be used again.
  bool SaveOutputFile(const std::string& prefix);

  // Implementation of ExternalRenderer:
  int FrameSizeChange(unsigned int width, unsigned int height,
                      unsigned int number_of_streams);

  int DeliverFrame(unsigned char* buffer, int buffer_size,
                   uint32_t time_stamp,
                   int64_t render_time);

  const std::string GetFullOutputPath() const;

 private:
  void ForgetOutputFile();

  std::FILE* output_file_;
  std::string output_path_;
  std::string output_filename_;
};

#endif  // SRC_VIDEO_ENGINE_TEST_AUTO_TEST_HELPERS_VIE_TO_FILE_RENDERER_H_
