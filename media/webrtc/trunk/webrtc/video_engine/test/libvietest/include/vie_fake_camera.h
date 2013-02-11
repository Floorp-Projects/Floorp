/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_FAKE_CAMERA_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_FAKE_CAMERA_H_

#include <string>

namespace webrtc {
class ViECapture;
class ThreadWrapper;
}

class ViEFileCaptureDevice;

// Registers an external capture device with the provided capture interface
// and starts running a fake camera by reading frames from a file. The frame-
// reading code runs in a separate thread which makes it possible to run tests
// while the fake camera feeds data into the system. This class is not thread-
// safe in itself (but handles its own thread in a safe manner).
class ViEFakeCamera {
 public:
  // The argument is the capture interface to register with.
  explicit ViEFakeCamera(webrtc::ViECapture* capture_interface);
  virtual ~ViEFakeCamera();

  // Runs the scenario in the class comments.
  bool StartCameraInNewThread(const std::string& i420_test_video_path,
                              int width,
                              int height);
  // Stops the camera and cleans up everything allocated by the start method.
  bool StopCamera();

  int capture_id() const { return capture_id_; }

 private:
  webrtc::ViECapture* capture_interface_;

  int capture_id_;
  webrtc::ThreadWrapper* camera_thread_;
  ViEFileCaptureDevice* file_capture_device_;
};

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_HELPERS_VIE_FAKE_CAMERA_H_
