/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_processing/agc/agc.h"

namespace webrtc {

class AudioFrame;
class DebugFile;
class GainControl;

// Callbacks that need to be injected into AgcManagerDirect to read and control
// the volume values. They have different behavior if they are called from
// AgcManager or AudioProcessing. This is done to remove the VoiceEngine
// dependency in AgcManagerDirect.
class VolumeCallbacks {
 public:
  virtual ~VolumeCallbacks() {}
  virtual void SetMicVolume(int volume) = 0;
  virtual int GetMicVolume() = 0;
};

// Direct interface to use AGC to set volume and compression values.
// AudioProcessing uses this interface directly to integrate the callback-less
// AGC. AgcManager delegates most of its calls here. See agc_manager.h for
// undocumented methods.
//
// This class is not thread-safe.
class AgcManagerDirect {
 public:
  // AgcManagerDirect will configure GainControl internally. The user is
  // responsible for processing the audio using it after the call to Process.
  AgcManagerDirect(GainControl* gctrl, VolumeCallbacks* volume_callbacks);
  // Dependency injection for testing. Don't delete |agc| as the memory is owned
  // by the manager.
  AgcManagerDirect(Agc* agc,
                   GainControl* gctrl,
                   VolumeCallbacks* volume_callbacks);
  ~AgcManagerDirect();

  int Initialize();
  void AnalyzePreProcess(int16_t* audio,
                         int num_channels,
                         int samples_per_channel);
  void Process(const int16_t* audio, int length, int sample_rate_hz);

  // Sets a new microphone level, after first checking that it hasn't been
  // updated by the user, in which case no action is taken.
  void SetLevel(int new_level);

  // Set the maximum level the AGC is allowed to apply. Also updates the
  // maximum compression gain to compensate. The level must be at least
  // |kClippedLevelMin|.
  void SetMaxLevel(int level);

  void SetCaptureMuted(bool muted);
  bool capture_muted() { return capture_muted_; }

  float voice_probability();

 private:
  int CheckVolumeAndReset();
  void UpdateGain();
  void UpdateCompressor();

  rtc::scoped_ptr<Agc> agc_;
  GainControl* gctrl_;
  VolumeCallbacks* volume_callbacks_;

  int frames_since_clipped_;
  int level_;
  int max_level_;
  int max_compression_gain_;
  int target_compression_;
  int compression_;
  float compression_accumulator_;
  bool capture_muted_;
  bool check_volume_on_next_process_;
  bool startup_;

  rtc::scoped_ptr<DebugFile> file_preproc_;
  rtc::scoped_ptr<DebugFile> file_postproc_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_
