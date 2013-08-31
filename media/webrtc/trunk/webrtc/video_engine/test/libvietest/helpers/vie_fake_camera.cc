/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/video_engine/test/libvietest/include/vie_fake_camera.h"

#include <assert.h>

#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/test/libvietest/include/vie_file_capture_device.h"

// This callback runs the camera thread:
bool StreamVideoFileRepeatedlyIntoCaptureDevice(void* data) {
  ViEFileCaptureDevice* file_capture_device =
      reinterpret_cast<ViEFileCaptureDevice*>(data);

  // We want to interrupt the camera feeding thread every now and then in order
  // to follow the contract for the system_wrappers thread library. 1.5 seconds
  // seems about right here.
  uint64_t time_slice_ms = 1500;
  uint32_t max_fps = 30;

  file_capture_device->ReadFileFor(time_slice_ms, max_fps);

  return true;
}

ViEFakeCamera::ViEFakeCamera(webrtc::ViECapture* capture_interface)
    : capture_interface_(capture_interface),
      capture_id_(-1),
      camera_thread_(NULL),
      file_capture_device_(NULL) {
}

ViEFakeCamera::~ViEFakeCamera() {
}

bool ViEFakeCamera::StartCameraInNewThread(
    const std::string& i420_test_video_path, int width, int height) {

  assert(file_capture_device_ == NULL && camera_thread_ == NULL);

  webrtc::ViEExternalCapture* externalCapture;
  int result = capture_interface_->
      AllocateExternalCaptureDevice(capture_id_, externalCapture);
  if (result != 0) {
    return false;
  }

  file_capture_device_ = new ViEFileCaptureDevice(externalCapture);
  if (!file_capture_device_->OpenI420File(i420_test_video_path,
                                          width,
                                          height)) {
    return false;
  }

  // Set up a thread which runs the fake camera. The capturer object is
  // thread-safe.
  camera_thread_ = webrtc::ThreadWrapper::CreateThread(
      StreamVideoFileRepeatedlyIntoCaptureDevice, file_capture_device_);
  unsigned int id;
  camera_thread_->Start(id);

  return true;
}

bool ViEFakeCamera::StopCamera() {
  assert(file_capture_device_ != NULL && camera_thread_ != NULL);

  camera_thread_->Stop();
  file_capture_device_->CloseFile();

  int result = capture_interface_->ReleaseCaptureDevice(capture_id_);

  delete camera_thread_;
  delete file_capture_device_;
  camera_thread_ = NULL;
  file_capture_device_ = NULL;

  return result == 0;
}
