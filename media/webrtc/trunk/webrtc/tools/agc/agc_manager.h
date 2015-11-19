/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_AGC_AGC_MANAGER_H_
#define WEBRTC_TOOLS_AGC_AGC_MANAGER_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_processing/agc/agc_manager_direct.h"

namespace webrtc {

class Agc;
class AudioProcessing;
class CriticalSectionWrapper;
class MediaCallback;
class PreprocCallback;
class VoEExternalMedia;
class VoEVolumeControl;
class VoiceEngine;
class VolumeCallbacks;

// Handles the interaction between VoiceEngine and the internal AGC. It hooks
// into the capture stream through VoiceEngine's external media interface and
// sends the audio to the AGC for analysis. It forwards requests for a capture
// volume change from the AGC to the VoiceEngine volume interface.
class AgcManager {
 public:
  explicit AgcManager(VoiceEngine* voe);
  // Dependency injection for testing. Don't delete |agc| or |audioproc| as the
  // memory is owned by the manager. If |media| or |volume| are non-fake
  // reference counted classes, don't release them as this is handled by the
  // manager.
  AgcManager(VoEExternalMedia* media, VoEVolumeControl* volume, Agc* agc,
             AudioProcessing* audioproc);
  virtual ~AgcManager();

  // When enabled, registers external media processing callbacks with
  // VoiceEngine to hook into the capture stream. Disabling deregisters the
  // callbacks.
  virtual int Enable(bool enable);
  virtual bool enabled() const { return enabled_; }

  // Call when the capture device has changed. This will trigger a retrieval of
  // the initial capture volume on the next audio frame.
  virtual void CaptureDeviceChanged();

  // Call when the capture stream has been muted/unmuted. This causes the
  // manager to disregard all incoming audio; chances are good it's background
  // noise to which we'd like to avoid adapting.
  virtual void SetCaptureMuted(bool muted);
  virtual bool capture_muted() const { return direct_->capture_muted(); }

 protected:
  // Provide a default constructor for testing.
  AgcManager();

 private:
  int DeregisterCallbacks();
  int CheckVolumeAndReset();

  VoEExternalMedia* media_;
  rtc::scoped_ptr<VolumeCallbacks> volume_callbacks_;
  rtc::scoped_ptr<CriticalSectionWrapper> crit_;
  rtc::scoped_ptr<AudioProcessing> audioproc_;
  rtc::scoped_ptr<AgcManagerDirect> direct_;
  rtc::scoped_ptr<MediaCallback> media_callback_;
  rtc::scoped_ptr<PreprocCallback> preproc_callback_;
  bool enabled_;
  bool initialized_;
};

}  // namespace webrtc

#endif  // WEBRTC_TOOLS_AGC_AGC_MANAGER_H_
