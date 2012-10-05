/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_INTERNAL_H_
#define WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_INTERNAL_H_

namespace webrtc {

class AudioFrame;
class Resampler;

namespace voe {

// Upmix or downmix and resample the audio in |src_frame| to |dst_frame|.
// Expects |dst_frame| to have its |num_channels_| and |sample_rate_hz_| set to
// the desired values. Updates |samples_per_channel_| accordingly.
//
// On failure, returns -1 and copies |src_frame| to |dst_frame|.
int RemixAndResample(const AudioFrame& src_frame,
                     Resampler* resampler,
                     AudioFrame* dst_frame);

}  // namespace voe
}  // namespace webrtc

#endif  // VOICE_ENGINE_OUTPUT_MIXER_INTERNAL_H_
