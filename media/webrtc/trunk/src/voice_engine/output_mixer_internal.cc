/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "output_mixer_internal.h"

#include "audio_frame_operations.h"
#include "common_audio/resampler/include/resampler.h"
#include "module_common_types.h"
#include "trace.h"

namespace webrtc {
namespace voe {

int RemixAndResample(const AudioFrame& src_frame,
                     Resampler* resampler,
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

  const ResamplerType resampler_type = audio_ptr_num_channels == 1 ?
      kResamplerSynchronous : kResamplerSynchronousStereo;
  if (resampler->ResetIfNeeded(src_frame.sample_rate_hz_,
                               dst_frame->sample_rate_hz_,
                               resampler_type) == -1) {
    *dst_frame = src_frame;
    WEBRTC_TRACE(kTraceError, kTraceVoice, -1,
                "%s ResetIfNeeded failed", __FUNCTION__);
    return -1;
  }

  int out_length = 0;
  if (resampler->Push(audio_ptr,
                      src_frame.samples_per_channel_* audio_ptr_num_channels,
                      dst_frame->data_,
                      AudioFrame::kMaxDataSizeSamples,
                      out_length) == 0) {
    dst_frame->samples_per_channel_ = out_length / audio_ptr_num_channels;
  } else {
    *dst_frame = src_frame;
    WEBRTC_TRACE(kTraceError, kTraceVoice, -1,
                 "%s resampling failed", __FUNCTION__);
    return -1;
  }

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
