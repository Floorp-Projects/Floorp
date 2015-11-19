/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/soundsystemproxy.h"

namespace rtc {

bool SoundSystemProxy::EnumeratePlaybackDevices(
    SoundDeviceLocatorList *devices) {
  return wrapped_ ? wrapped_->EnumeratePlaybackDevices(devices) : false;
}

bool SoundSystemProxy::EnumerateCaptureDevices(
    SoundDeviceLocatorList *devices) {
  return wrapped_ ? wrapped_->EnumerateCaptureDevices(devices) : false;
}

bool SoundSystemProxy::GetDefaultPlaybackDevice(
    SoundDeviceLocator **device) {
  return wrapped_ ? wrapped_->GetDefaultPlaybackDevice(device) : false;
}

bool SoundSystemProxy::GetDefaultCaptureDevice(
    SoundDeviceLocator **device) {
  return wrapped_ ? wrapped_->GetDefaultCaptureDevice(device) : false;
}

SoundOutputStreamInterface *SoundSystemProxy::OpenPlaybackDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params) {
  return wrapped_ ? wrapped_->OpenPlaybackDevice(device, params) : NULL;
}

SoundInputStreamInterface *SoundSystemProxy::OpenCaptureDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params) {
  return wrapped_ ? wrapped_->OpenCaptureDevice(device, params) : NULL;
}

}  // namespace rtc
