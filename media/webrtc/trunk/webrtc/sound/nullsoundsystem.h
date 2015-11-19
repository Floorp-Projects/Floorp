/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_NULLSOUNDSYSTEM_H_
#define WEBRTC_SOUND_NULLSOUNDSYSTEM_H_

#include "webrtc/sound/soundsysteminterface.h"

namespace rtc {

class SoundDeviceLocator;
class SoundInputStreamInterface;
class SoundOutputStreamInterface;

// A simple reference sound system that drops output samples and generates
// no input samples.
class NullSoundSystem : public SoundSystemInterface {
 public:
  static SoundSystemInterface *Create() {
    return new NullSoundSystem();
  }

  virtual ~NullSoundSystem();

  virtual bool Init();
  virtual void Terminate();

  virtual bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices);
  virtual bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices);

  virtual SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);
  virtual SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);

  virtual bool GetDefaultPlaybackDevice(SoundDeviceLocator **device);
  virtual bool GetDefaultCaptureDevice(SoundDeviceLocator **device);

  virtual const char *GetName() const;
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_NULLSOUNDSYSTEM_H_
