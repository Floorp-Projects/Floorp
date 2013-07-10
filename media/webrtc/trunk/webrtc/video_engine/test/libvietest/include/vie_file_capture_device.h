/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_FILE_CAPTURE_DEVICE_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_FILE_CAPTURE_DEVICE_H_

#include <cstdio>

#include <string>

#include "typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ViEExternalCapture;
}

// This class opens a i420 file and feeds it into a ExternalCapture instance,
// thereby acting as a faked capture device with deterministic input.
class ViEFileCaptureDevice {
 public:
  // The input sink is where to send the I420 video frames.
  explicit ViEFileCaptureDevice(webrtc::ViEExternalCapture* input_sink);
  virtual ~ViEFileCaptureDevice();

  // Opens the provided I420 file and interprets it according to the provided
  // width and height. Returns false if the file doesn't exist.
  bool OpenI420File(const std::string& path, int width, int height);

  // Reads the previously opened file for at most time_slice_ms milliseconds,
  // after which it will return. It will make sure to sleep accordingly so we
  // do not send more than max_fps cap (we may send less, though).
  void ReadFileFor(uint64_t time_slice_ms, uint32_t max_fps);

  // Closes the opened input file.
  void CloseFile();

 private:
  webrtc::ViEExternalCapture* input_sink_;

  std::FILE* input_file_;
  webrtc::CriticalSectionWrapper* mutex_;

  uint32_t frame_length_;
  uint32_t width_;
  uint32_t height_;
};

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_FILE_CAPTURE_DEVICE_H_
