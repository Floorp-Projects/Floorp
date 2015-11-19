/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_SOUNDSYSTEMPROXY_H_
#define WEBRTC_SOUND_SOUNDSYSTEMPROXY_H_

#include "webrtc/sound/soundsysteminterface.h"
#include "webrtc/base/basictypes.h"  // for NULL

namespace rtc {

// A SoundSystemProxy is a sound system that defers to another one.
// Init(), Terminate(), and GetName() are left as pure virtual, so a sub-class
// must define them.
class SoundSystemProxy : public SoundSystemInterface {
 public:
  SoundSystemProxy() : wrapped_(NULL) {}

  // Each of these methods simply defers to wrapped_ if non-NULL, else fails.

  virtual bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices);
  virtual bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices);

  virtual bool GetDefaultPlaybackDevice(SoundDeviceLocator **device);
  virtual bool GetDefaultCaptureDevice(SoundDeviceLocator **device);

  virtual SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);
  virtual SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);

 protected:
  SoundSystemInterface *wrapped_;
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_SOUNDSYSTEMPROXY_H_
