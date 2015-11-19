/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_COMMON_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_COMMON_H_

static const int kSampleRateHz = 16000;
static const int kLength10Ms = kSampleRateHz / 100;
static const int kMaxNumFrames = 4;

struct AudioFeatures {
  double log_pitch_gain[kMaxNumFrames];
  double pitch_lag_hz[kMaxNumFrames];
  double spectral_peak[kMaxNumFrames];
  double rms[kMaxNumFrames];
  int num_frames;
  bool silence;
};

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_COMMON_H_
