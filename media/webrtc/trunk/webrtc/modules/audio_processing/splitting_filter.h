/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_SPLITTING_FILTER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_SPLITTING_FILTER_H_

#include <string.h>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class IFChannelBuffer;

enum {
  kSamplesPer8kHzChannel = 80,
  kSamplesPer16kHzChannel = 160,
  kSamplesPer32kHzChannel = 320,
  kSamplesPer48kHzChannel = 480,
  kSamplesPer64kHzChannel = 640
};

struct TwoBandsStates {
  TwoBandsStates() {
    memset(analysis_state1, 0, sizeof(analysis_state1));
    memset(analysis_state2, 0, sizeof(analysis_state2));
    memset(synthesis_state1, 0, sizeof(synthesis_state1));
    memset(synthesis_state2, 0, sizeof(synthesis_state2));
  }

  static const int kStateSize = 6;
  int analysis_state1[kStateSize];
  int analysis_state2[kStateSize];
  int synthesis_state1[kStateSize];
  int synthesis_state2[kStateSize];
};

// Splitting filter which is able to split into and merge from 2 or 3 frequency
// bands. The number of channels needs to be provided at construction time.
//
// For each block, Analysis() is called to split into bands and then Synthesis()
// to merge these bands again. The input and output signals are contained in
// IFChannelBuffers and for the different bands an array of IFChannelBuffers is
// used.
class SplittingFilter {
 public:
  SplittingFilter(int channels);

  void Analysis(const IFChannelBuffer* data, IFChannelBuffer* bands);
  void Synthesis(const IFChannelBuffer* bands, IFChannelBuffer* data);

 private:
  // These work for 640 samples or less.
  void TwoBandsAnalysis(const IFChannelBuffer* data, IFChannelBuffer* bands);
  void TwoBandsSynthesis(const IFChannelBuffer* bands, IFChannelBuffer* data);
  // These only work for 480 samples at the moment.
  void ThreeBandsAnalysis(const IFChannelBuffer* data, IFChannelBuffer* bands);
  void ThreeBandsSynthesis(const IFChannelBuffer* bands, IFChannelBuffer* data);
  void InitBuffers();

  int channels_;
  rtc::scoped_ptr<TwoBandsStates[]> two_bands_states_;
  rtc::scoped_ptr<TwoBandsStates[]> band1_states_;
  rtc::scoped_ptr<TwoBandsStates[]> band2_states_;
  ScopedVector<PushSincResampler> analysis_resamplers_;
  ScopedVector<PushSincResampler> synthesis_resamplers_;
  rtc::scoped_ptr<int16_t[]> int_buffer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_SPLITTING_FILTER_H_
