/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/agc/agc.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/modules/audio_processing/agc/agc_audio_proc.h"
#include "webrtc/modules/audio_processing/agc/common.h"
#include "webrtc/modules/audio_processing/agc/histogram.h"
#include "webrtc/modules/audio_processing/agc/pitch_based_vad.h"
#include "webrtc/modules/audio_processing/agc/standalone_vad.h"
#include "webrtc/modules/audio_processing/agc/utility.h"
#include "webrtc/modules/interface/module_common_types.h"

namespace webrtc {
namespace {

const int kDefaultLevelDbfs = -18;
const double kDefaultVoiceValue = 1.0;
const int kNumAnalysisFrames = 100;
const double kActivityThreshold = 0.3;

}  // namespace

Agc::Agc()
    : target_level_loudness_(Dbfs2Loudness(kDefaultLevelDbfs)),
      last_voice_probability_(kDefaultVoiceValue),
      target_level_dbfs_(kDefaultLevelDbfs),
      standalone_vad_enabled_(true),
      histogram_(Histogram::Create(kNumAnalysisFrames)),
      inactive_histogram_(Histogram::Create()),
      audio_processing_(new AgcAudioProc()),
      pitch_based_vad_(new PitchBasedVad()),
      standalone_vad_(StandaloneVad::Create()),
      // Initialize to the most common resampling situation.
      resampler_(new Resampler(32000, kSampleRateHz, 1)) {
  }

Agc::~Agc() {}

float Agc::AnalyzePreproc(const int16_t* audio, int length) {
  assert(length > 0);
  int num_clipped = 0;
  for (int i = 0; i < length; ++i) {
    if (audio[i] == 32767 || audio[i] == -32768)
      ++num_clipped;
  }
  return 1.0f * num_clipped / length;
}

int Agc::Process(const int16_t* audio, int length, int sample_rate_hz) {
  assert(length == sample_rate_hz / 100);
  if (sample_rate_hz > 32000) {
    return -1;
  }
  // Resample to the required rate.
  int16_t resampled[kLength10Ms];
  const int16_t* resampled_ptr = audio;
  if (sample_rate_hz != kSampleRateHz) {
    if (resampler_->ResetIfNeeded(sample_rate_hz, kSampleRateHz, 1) != 0) {
      return -1;
    }
    resampler_->Push(audio, length, resampled, kLength10Ms, length);
    resampled_ptr = resampled;
  }
  assert(length == kLength10Ms);

  if (standalone_vad_enabled_) {
    if (standalone_vad_->AddAudio(resampled_ptr, length) != 0)
      return -1;
  }

  AudioFeatures features;
  audio_processing_->ExtractFeatures(resampled_ptr, length, &features);
  if (features.num_frames > 0) {
    if (features.silence) {
      // The other features are invalid, so update the histogram with an
      // arbitrary low value.
      for (int n = 0; n < features.num_frames; ++n)
        histogram_->Update(features.rms[n], 0.01);
      return 0;
    }

    // Initialize to 0.5 which is a neutral value for combining probabilities,
    // in case the standalone-VAD is not enabled.
    double p_combined[] = {0.5, 0.5, 0.5, 0.5};
    static_assert(sizeof(p_combined) / sizeof(p_combined[0]) == kMaxNumFrames,
                  "combined probability incorrect size");
    if (standalone_vad_enabled_) {
      if (standalone_vad_->GetActivity(p_combined, kMaxNumFrames) < 0)
        return -1;
    }
    // If any other VAD is enabled it must be combined before calling the
    // pitch-based VAD.
    if (pitch_based_vad_->VoicingProbability(features, p_combined) < 0)
      return -1;
    for (int n = 0; n < features.num_frames; n++) {
      histogram_->Update(features.rms[n], p_combined[n]);
      last_voice_probability_ = p_combined[n];
    }
  }
  return 0;
}

bool Agc::GetRmsErrorDb(int* error) {
  if (!error) {
    assert(false);
    return false;
  }

  if (histogram_->num_updates() < kNumAnalysisFrames) {
    // We haven't yet received enough frames.
    return false;
  }

  if (histogram_->AudioContent() < kNumAnalysisFrames * kActivityThreshold) {
    // We are likely in an inactive segment.
    return false;
  }

  double loudness = Linear2Loudness(histogram_->CurrentRms());
  *error = std::floor(Loudness2Db(target_level_loudness_ - loudness) + 0.5);
  histogram_->Reset();
  return true;
}

void Agc::Reset() {
  histogram_->Reset();
}

int Agc::set_target_level_dbfs(int level) {
  // TODO(turajs): just some arbitrary sanity check. We can come up with better
  // limits. The upper limit should be chosen such that the risk of clipping is
  // low. The lower limit should not result in a too quiet signal.
  if (level >= 0 || level <= -100)
    return -1;
  target_level_dbfs_ = level;
  target_level_loudness_ = Dbfs2Loudness(level);
  return 0;
}

void Agc::EnableStandaloneVad(bool enable) {
  standalone_vad_enabled_ = enable;
}

}  // namespace webrtc
