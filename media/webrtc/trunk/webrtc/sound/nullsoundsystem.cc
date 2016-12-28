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

  SoundDeviceLocator *Copy() const override {
    return new NullSoundDeviceLocator();
  }
};

class NullSoundInputStream : public SoundInputStreamInterface {
 public:
  bool StartReading() override {
    return true;
  }

  bool StopReading() override {
    return true;
  }

  bool GetVolume(int *volume) override {
    *volume = SoundSystemInterface::kMinVolume;
    return true;
  }

  bool SetVolume(int volume) override {
    return false;
  }

  bool Close() override {
    return true;
  }

  int LatencyUsecs() override {
    return 0;
  }
};

class NullSoundOutputStream : public SoundOutputStreamInterface {
 public:
  bool EnableBufferMonitoring() override {
    return true;
  }

  bool DisableBufferMonitoring() override {
    return true;
  }

  bool WriteSamples(const void *sample_data, size_t size) override {
    LOG(LS_VERBOSE) << "Got " << size << " bytes of playback samples";
    return true;
  }

  bool GetVolume(int *volume) override {
    *volume = SoundSystemInterface::kMinVolume;
    return true;
  }

  bool SetVolume(int volume) override {
    return false;
  }

  bool Close() override {
    return true;
  }

  int LatencyUsecs() override {
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
