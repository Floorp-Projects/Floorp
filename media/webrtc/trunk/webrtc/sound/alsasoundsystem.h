/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_ALSASOUNDSYSTEM_H_
#define WEBRTC_SOUND_ALSASOUNDSYSTEM_H_

#include "webrtc/sound/alsasymboltable.h"
#include "webrtc/sound/soundsysteminterface.h"
#include "webrtc/base/constructormagic.h"

namespace rtc {

class AlsaStream;
class AlsaInputStream;
class AlsaOutputStream;

// Sound system implementation for ALSA, the predominant sound device API on
// Linux (but typically not used directly by applications anymore).
class AlsaSoundSystem : public SoundSystemInterface {
  friend class AlsaStream;
  friend class AlsaInputStream;
  friend class AlsaOutputStream;
 public:
  static SoundSystemInterface *Create() {
    return new AlsaSoundSystem();
  }

  AlsaSoundSystem();

  ~AlsaSoundSystem() override;

  bool Init() override;
  void Terminate() override;

  bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices) override;
  bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices) override;

  bool GetDefaultPlaybackDevice(SoundDeviceLocator **device) override;
  bool GetDefaultCaptureDevice(SoundDeviceLocator **device) override;

  SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) override;
  SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) override;

  const char *GetName() const override;

 private:
  bool IsInitialized() { return initialized_; }

  bool EnumerateDevices(SoundDeviceLocatorList *devices,
                        bool capture_not_playback);

  bool GetDefaultDevice(SoundDeviceLocator **device);

  static size_t FrameSize(const OpenParams &params);

  template <typename StreamInterface>
  StreamInterface *OpenDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params,
      snd_pcm_stream_t type,
      StreamInterface *(AlsaSoundSystem::*start_fn)(
          snd_pcm_t *handle,
          size_t frame_size,
          int wait_timeout_ms,
          int flags,
          int freq));

  SoundOutputStreamInterface *StartOutputStream(
      snd_pcm_t *handle,
      size_t frame_size,
      int wait_timeout_ms,
      int flags,
      int freq);

  SoundInputStreamInterface *StartInputStream(
      snd_pcm_t *handle,
      size_t frame_size,
      int wait_timeout_ms,
      int flags,
      int freq);

  const char *GetError(int err);

  bool initialized_;
  AlsaSymbolTable symbol_table_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AlsaSoundSystem);
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_ALSASOUNDSYSTEM_H_
