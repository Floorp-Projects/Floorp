/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/agc/agc_manager.h"

#include <assert.h>

#include "webrtc/modules/audio_processing/agc/agc.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

namespace webrtc {

class AgcManagerVolume : public VolumeCallbacks {
 public:
  // AgcManagerVolume acquires ownership of |volume|.
  explicit AgcManagerVolume(VoEVolumeControl* volume)
      : volume_(volume) {
  }

  ~AgcManagerVolume() {
    if (volume_) {
      volume_->Release();
    }
  }

  virtual void SetMicVolume(int volume) {
    if (volume_->SetMicVolume(volume) != 0) {
      LOG_FERR1(LS_WARNING, SetMicVolume, volume);
    }
  }

  int GetMicVolume() {
    unsigned int volume = 0;
    if (volume_->GetMicVolume(volume) != 0) {
      LOG_FERR0(LS_WARNING, GetMicVolume);
      return -1;
    }
    return volume;
  }

 private:
  VoEVolumeControl* volume_;
};

class MediaCallback : public VoEMediaProcess {
 public:
  MediaCallback(AgcManagerDirect* direct, AudioProcessing* audioproc,
                CriticalSectionWrapper* crit)
      : direct_(direct),
        audioproc_(audioproc),
        crit_(crit),
        frame_() {
  }

 protected:
  virtual void Process(const int channel, const ProcessingTypes type,
                       int16_t audio[], const int samples_per_channel,
                       const int sample_rate_hz, const bool is_stereo) {
    CriticalSectionScoped cs(crit_);
    if (direct_->capture_muted()) {
      return;
    }

    // Extract the first channel.
    const int kMaxSampleRateHz = 48000;
    const int kMaxSamplesPerChannel = kMaxSampleRateHz / 100;
    assert(samples_per_channel < kMaxSamplesPerChannel &&
           sample_rate_hz < kMaxSampleRateHz);
    int16_t mono[kMaxSamplesPerChannel];
    int16_t* mono_ptr = audio;
    if (is_stereo) {
      for (int n = 0; n < samples_per_channel; n++) {
        mono[n] = audio[n * 2];
      }
      mono_ptr = mono;
    }

    direct_->Process(mono_ptr, samples_per_channel, sample_rate_hz);

    // TODO(ajm): It's unfortunate we have to memcpy to this frame here, but
    // it's needed for use with AudioProcessing.
    frame_.num_channels_ = is_stereo ? 2 : 1;
    frame_.samples_per_channel_ = samples_per_channel;
    frame_.sample_rate_hz_ = sample_rate_hz;
    const int length_samples = frame_.num_channels_ * samples_per_channel;
    memcpy(frame_.data_, audio, length_samples * sizeof(int16_t));

    // Apply compression to the audio.
    if (audioproc_->ProcessStream(&frame_) != 0) {
      LOG_FERR0(LS_ERROR, ProcessStream);
    }

    // Copy the compressed audio back to voice engine's array.
    memcpy(audio, frame_.data_, length_samples * sizeof(int16_t));
  }

 private:
  AgcManagerDirect* direct_;
  AudioProcessing* audioproc_;
  CriticalSectionWrapper* crit_;
  AudioFrame frame_;
};

class PreprocCallback : public VoEMediaProcess {
 public:
  PreprocCallback(AgcManagerDirect* direct, CriticalSectionWrapper* crit)
      : direct_(direct),
        crit_(crit) {
  }

 protected:
  virtual void Process(const int channel, const ProcessingTypes type,
                       int16_t audio[], const int samples_per_channel,
                       const int sample_rate_hz, const bool is_stereo) {
    CriticalSectionScoped cs(crit_);
    if (direct_->capture_muted()) {
      return;
    }
    direct_->AnalyzePreProcess(audio, is_stereo ? 2 : 1, samples_per_channel);
  }

 private:
  AgcManagerDirect* direct_;
  CriticalSectionWrapper* crit_;
};

AgcManager::AgcManager(VoiceEngine* voe)
    : media_(VoEExternalMedia::GetInterface(voe)),
      volume_callbacks_(new AgcManagerVolume(VoEVolumeControl::GetInterface(
          voe))),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      enabled_(false),
      initialized_(false) {
    Config config;
    config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
    audioproc_.reset(AudioProcessing::Create(config));
    direct_.reset(new AgcManagerDirect(audioproc_->gain_control(),
                                       volume_callbacks_.get()));
    media_callback_.reset(new MediaCallback(direct_.get(),
                                            audioproc_.get(),
                                            crit_.get()));
    preproc_callback_.reset(new PreprocCallback(direct_.get(), crit_.get()));
}

AgcManager::AgcManager(VoEExternalMedia* media, VoEVolumeControl* volume,
                       Agc* agc, AudioProcessing* audioproc)
    : media_(media),
      volume_callbacks_(new AgcManagerVolume(volume)),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      audioproc_(audioproc),
      direct_(new AgcManagerDirect(agc,
                                   audioproc_->gain_control(),
                                   volume_callbacks_.get())),
      media_callback_(new MediaCallback(direct_.get(),
                                        audioproc_.get(),
                                        crit_.get())),
      preproc_callback_(new PreprocCallback(direct_.get(), crit_.get())),
      enabled_(false),
      initialized_(false) {
}

AgcManager::AgcManager()
    : media_(NULL),
      enabled_(false),
      initialized_(false) {
}

AgcManager::~AgcManager() {
  if (media_) {
    if (enabled_) {
      DeregisterCallbacks();
    }
    media_->Release();
  }
}

int AgcManager::Enable(bool enable) {
  if (enable == enabled_) {
    return 0;
  }
  if (!initialized_) {
    CriticalSectionScoped cs(crit_.get());
    if (audioproc_->gain_control()->Enable(true) != 0) {
      LOG_FERR1(LS_ERROR, gain_control()->Enable, true);
      return -1;
    }
    if (direct_->Initialize() != 0) {
      assert(false);
      return -1;
    }
    initialized_ = true;
  }

  if (enable) {
    if (media_->RegisterExternalMediaProcessing(0, kRecordingAllChannelsMixed,
                                                *media_callback_) != 0) {
      LOG(LS_ERROR) << "Failed to register postproc callback";
      return -1;
    }
    if (media_->RegisterExternalMediaProcessing(0, kRecordingPreprocessing,
                                                *preproc_callback_) != 0) {
      LOG(LS_ERROR) << "Failed to register preproc callback";
      return -1;
    }
  } else {
    if (DeregisterCallbacks() != 0)
      return -1;
  }
  enabled_ = enable;
  return 0;
}

void AgcManager::CaptureDeviceChanged() {
  CriticalSectionScoped cs(crit_.get());
  direct_->Initialize();
}

void AgcManager::SetCaptureMuted(bool muted) {
  CriticalSectionScoped cs(crit_.get());
  direct_->SetCaptureMuted(muted);
}

int AgcManager::DeregisterCallbacks() {
  // DeRegister shares a lock with the Process() callback. This call will block
  // until the callback is finished and it's safe to continue teardown.
  int err = 0;
  if (media_->DeRegisterExternalMediaProcessing(0,
          kRecordingAllChannelsMixed) != 0) {
    LOG(LS_ERROR) << "Failed to deregister postproc callback";
    err = -1;
  }
  if (media_->DeRegisterExternalMediaProcessing(0,
          kRecordingPreprocessing) != 0) {
    LOG(LS_ERROR) << "Failed to deregister preproc callback";
    err = -1;
  }
  return err;
}

}  // namespace webrtc
