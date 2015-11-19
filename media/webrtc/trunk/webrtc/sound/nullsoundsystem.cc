/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/nullsoundsystem.h"

#include "webrtc/sound/sounddevicelocator.h"
#include "webrtc/sound/soundinputstreaminterface.h"
#include "webrtc/sound/soundoutputstreaminterface.h"
#include "webrtc/base/logging.h"

namespace rtc {

class Thread;

}

namespace rtc {

// Name used for the single device and the sound system itself.
static const char kNullName[] = "null";

class NullSoundDeviceLocator : public SoundDeviceLocator {
 public:
  NullSoundDeviceLocator() : SoundDeviceLocator(kNullName, kNullName) {}

  virtual SoundDeviceLocator *Copy() const {
    return new NullSoundDeviceLocator();
  }
};

class NullSoundInputStream : public SoundInputStreamInterface {
 public:
  virtual bool StartReading() {
    return true;
  }

  virtual bool StopReading() {
    return true;
  }

  virtual bool GetVolume(int *volume) {
    *volume = SoundSystemInterface::kMinVolume;
    return true;
  }

  virtual bool SetVolume(int volume) {
    return false;
  }

  virtual bool Close() {
    return true;
  }

  virtual int LatencyUsecs() {
    return 0;
  }
};

class NullSoundOutputStream : public SoundOutputStreamInterface {
 public:
  virtual bool EnableBufferMonitoring() {
    return true;
  }

  virtual bool DisableBufferMonitoring() {
    return true;
  }

  virtual bool WriteSamples(const void *sample_data,
                            size_t size) {
    LOG(LS_VERBOSE) << "Got " << size << " bytes of playback samples";
    return true;
  }

  virtual bool GetVolume(int *volume) {
    *volume = SoundSystemInterface::kMinVolume;
    return true;
  }

  virtual bool SetVolume(int volume) {
    return false;
  }

  virtual bool Close() {
    return true;
  }

  virtual int LatencyUsecs() {
    return 0;
  }
};

NullSoundSystem::~NullSoundSystem() {
}

bool NullSoundSystem::Init() {
  return true;
}

void NullSoundSystem::Terminate() {
  // Nothing to do.
}

bool NullSoundSystem::EnumeratePlaybackDevices(
      SoundSystemInterface::SoundDeviceLocatorList *devices) {
  ClearSoundDeviceLocatorList(devices);
  SoundDeviceLocator *device;
  GetDefaultPlaybackDevice(&device);
  devices->push_back(device);
  return true;
}

bool NullSoundSystem::EnumerateCaptureDevices(
      SoundSystemInterface::SoundDeviceLocatorList *devices) {
  ClearSoundDeviceLocatorList(devices);
  SoundDeviceLocator *device;
  GetDefaultCaptureDevice(&device);
  devices->push_back(device);
  return true;
}

bool NullSoundSystem::GetDefaultPlaybackDevice(
    SoundDeviceLocator **device) {
  *device = new NullSoundDeviceLocator();
  return true;
}

bool NullSoundSystem::GetDefaultCaptureDevice(
    SoundDeviceLocator **device) {
  *device = new NullSoundDeviceLocator();
  return true;
}

SoundOutputStreamInterface *NullSoundSystem::OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) {
  return new NullSoundOutputStream();
}

SoundInputStreamInterface *NullSoundSystem::OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) {
  return new NullSoundInputStream();
}

const char *NullSoundSystem::GetName() const {
  return kNullName;
}

}  // namespace rtc
