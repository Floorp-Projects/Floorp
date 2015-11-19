/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/splitting_filter.h"

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/common_audio/channel_buffer.h"

namespace webrtc {

SplittingFilter::SplittingFilter(int channels)
    : channels_(channels),
      two_bands_states_(new TwoBandsStates[channels]),
      band1_states_(new TwoBandsStates[channels]),
      band2_states_(new TwoBandsStates[channels]) {
  for (int i = 0; i < channels; ++i) {
    analysis_resamplers_.push_back(new PushSincResampler(
        kSamplesPer48kHzChannel, kSamplesPer64kHzChannel));
    synthesis_resamplers_.push_back(new PushSincResampler(
        kSamplesPer64kHzChannel, kSamplesPer48kHzChannel));
  }
}

void SplittingFilter::Analysis(const IFChannelBuffer* data,
                               IFChannelBuffer* bands) {
  DCHECK(bands->num_bands() == 2 || bands->num_bands() == 3);
  DCHECK_EQ(channels_, data->num_channels());
  DCHECK_EQ(channels_, bands->num_channels());
  DCHECK_EQ(data->num_frames(),
            bands->num_frames_per_band() * bands->num_bands());
  if (bands->num_bands() == 2) {
    TwoBandsAnalysis(data, bands);
  } else if (bands->num_bands() == 3) {
    ThreeBandsAnalysis(data, bands);
  }
}

void SplittingFilter::Synthesis(const IFChannelBuffer* bands,
                                IFChannelBuffer* data) {
  DCHECK(bands->num_bands() == 2 || bands->num_bands() == 3);
  DCHECK_EQ(channels_, data->num_channels());
  DCHECK_EQ(channels_, bands->num_channels());
  DCHECK_EQ(data->num_frames(),
            bands->num_frames_per_band() * bands->num_bands());
  if (bands->num_bands() == 2) {
    TwoBandsSynthesis(bands, data);
  } else if (bands->num_bands() == 3) {
    ThreeBandsSynthesis(bands, data);
  }
}

void SplittingFilter::TwoBandsAnalysis(const IFChannelBuffer* data,
                                       IFChannelBuffer* bands) {
  for (int i = 0; i < channels_; ++i) {
    WebRtcSpl_AnalysisQMF(data->ibuf_const()->channels()[i],
                          data->num_frames(),
                          bands->ibuf()->channels(0)[i],
                          bands->ibuf()->channels(1)[i],
                          two_bands_states_[i].analysis_state1,
                          two_bands_states_[i].analysis_state2);
  }
}

void SplittingFilter::TwoBandsSynthesis(const IFChannelBuffer* bands,
                                        IFChannelBuffer* data) {
  for (int i = 0; i < channels_; ++i) {
    WebRtcSpl_SynthesisQMF(bands->ibuf_const()->channels(0)[i],
                           bands->ibuf_const()->channels(1)[i],
                           bands->num_frames_per_band(),
                           data->ibuf()->channels()[i],
                           two_bands_states_[i].synthesis_state1,
                           two_bands_states_[i].synthesis_state2);
  }
}

// This is a simple implementation using the existing code and will be replaced
// by a proper 3 band filter bank.
// It up-samples from 48kHz to 64kHz, splits twice into 2 bands and discards the
// uppermost band, because it is empty anyway.
void SplittingFilter::ThreeBandsAnalysis(const IFChannelBuffer* data,
                                         IFChannelBuffer* bands) {
  DCHECK_EQ(kSamplesPer48kHzChannel,
            data->num_frames());
  InitBuffers();
  for (int i = 0; i < channels_; ++i) {
    analysis_resamplers_[i]->Resample(data->ibuf_const()->channels()[i],
                                      kSamplesPer48kHzChannel,
                                      int_buffer_.get(),
                                      kSamplesPer64kHzChannel);
    WebRtcSpl_AnalysisQMF(int_buffer_.get(),
                          kSamplesPer64kHzChannel,
                          int_buffer_.get(),
                          int_buffer_.get() + kSamplesPer32kHzChannel,
                          two_bands_states_[i].analysis_state1,
                          two_bands_states_[i].analysis_state2);
    WebRtcSpl_AnalysisQMF(int_buffer_.get(),
                          kSamplesPer32kHzChannel,
                          bands->ibuf()->channels(0)[i],
                          bands->ibuf()->channels(1)[i],
                          band1_states_[i].analysis_state1,
                          band1_states_[i].analysis_state2);
    WebRtcSpl_AnalysisQMF(int_buffer_.get() + kSamplesPer32kHzChannel,
                          kSamplesPer32kHzChannel,
                          int_buffer_.get(),
                          bands->ibuf()->channels(2)[i],
                          band2_states_[i].analysis_state1,
                          band2_states_[i].analysis_state2);
  }
}

// This is a simple implementation using the existing code and will be replaced
// by a proper 3 band filter bank.
// Using an empty uppermost band, it merges the 4 bands in 2 steps and
// down-samples from 64kHz to 48kHz.
void SplittingFilter::ThreeBandsSynthesis(const IFChannelBuffer* bands,
                                          IFChannelBuffer* data) {
  DCHECK_EQ(kSamplesPer48kHzChannel,
            data->num_frames());
  InitBuffers();
  for (int i = 0; i < channels_; ++i) {
    memset(int_buffer_.get(),
           0,
           kSamplesPer64kHzChannel * sizeof(int_buffer_[0]));
    WebRtcSpl_SynthesisQMF(bands->ibuf_const()->channels(0)[i],
                           bands->ibuf_const()->channels(1)[i],
                           kSamplesPer16kHzChannel,
                           int_buffer_.get(),
                           band1_states_[i].synthesis_state1,
                           band1_states_[i].synthesis_state2);
    WebRtcSpl_SynthesisQMF(int_buffer_.get() + kSamplesPer32kHzChannel,
                           bands->ibuf_const()->channels(2)[i],
                           kSamplesPer16kHzChannel,
                           int_buffer_.get() + kSamplesPer32kHzChannel,
                           band2_states_[i].synthesis_state1,
                           band2_states_[i].synthesis_state2);
    WebRtcSpl_SynthesisQMF(int_buffer_.get(),
                           int_buffer_.get() + kSamplesPer32kHzChannel,
                           kSamplesPer32kHzChannel,
                           int_buffer_.get(),
                           two_bands_states_[i].synthesis_state1,
                           two_bands_states_[i].synthesis_state2);
    synthesis_resamplers_[i]->Resample(int_buffer_.get(),
                                       kSamplesPer64kHzChannel,
                                       data->ibuf()->channels()[i],
                                       kSamplesPer48kHzChannel);
  }
}

void SplittingFilter::InitBuffers() {
  if (!int_buffer_) {
    int_buffer_.reset(new int16_t[kSamplesPer64kHzChannel]);
  }
}

}  // namespace webrtc
