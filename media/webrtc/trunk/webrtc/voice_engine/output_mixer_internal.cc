/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/output_mixer_internal.h"

#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/utility/interface/audio_frame_operations.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
namespace voe {

int RemixAndResample(const AudioFrame& src_frame,
                     PushResampler* resampler,
                     AudioFrame* dst_frame) {
  const int16_t* audio_ptr = src_frame.data_;
  int audio_ptr_num_channels = src_frame.num_channels_;
  int16_t mono_audio[AudioFrame::kMaxDataSizeSamples];

  // Downmix before resampling.
  if (src_frame.num_channels_ == 2 && dst_frame->num_channels_ == 1) {
    AudioFrameOperations::StereoToMono(src_frame.data_,
                                       src_frame.samples_per_channel_,
                                       mono_audio);
    audio_ptr = mono_audio;
    audio_ptr_num_channels = 1;
  }

  if (resampler->InitializeIfNeeded(src_frame.sample_rate_hz_,
                                    dst_frame->sample_rate_hz_,
                                    audio_ptr_num_channels) == -1) {
    dst_frame->CopyFrom(src_frame);
    LOG_FERR3(LS_ERROR, InitializeIfNeeded, src_frame.sample_rate_hz_,
              dst_frame->sample_rate_hz_, audio_ptr_num_channels);
    return -1;
  }

  const int src_length = src_frame.samples_per_channel_ *
                         audio_ptr_num_channels;
  int out_length = resampler->Resample(audio_ptr, src_length, dst_frame->data_,
                                       AudioFrame::kMaxDataSizeSamples);
  if (out_length == -1) {
    dst_frame->CopyFrom(src_frame);
    LOG_FERR3(LS_ERROR, Resample, src_length, dst_frame->data_,
              AudioFrame::kMaxDataSizeSamples);
    return -1;
  }
  dst_frame->samples_per_channel_ = out_length / audio_ptr_num_channels;

  // Upmix after resampling.
  if (src_frame.num_channels_ == 1 && dst_frame->num_channels_ == 2) {
    // The audio in dst_frame really is mono at this point; MonoToStereo will
    // set this back to stereo.
    dst_frame->num_channels_ = 1;
    AudioFrameOperations::MonoToStereo(dst_frame);
  }
  return 0;
}

}  // namespace voe
}  // namespace webrtc
