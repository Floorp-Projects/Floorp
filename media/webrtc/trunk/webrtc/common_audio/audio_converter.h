/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_AUDIO_CONVERTER_H_
#define WEBRTC_COMMON_AUDIO_AUDIO_CONVERTER_H_

// TODO(ajm): Move channel buffer to common_audio.
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/common.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"

namespace webrtc {

class PushSincResampler;

// Format conversion (remixing and resampling) for audio. Only simple remixing
// conversions are supported: downmix to mono (i.e. |dst_channels| == 1) or
// upmix from mono (i.e. |src_channels == 1|).
//
// The source and destination chunks have the same duration in time; specifying
// the number of frames is equivalent to specifying the sample rates.
class AudioConverter {
 public:
  AudioConverter(int src_channels, int src_frames,
                 int dst_channels, int dst_frames);

  void Convert(const float* const* src,
               int src_channels,
               int src_frames,
               int dst_channels,
               int dst_frames,
               float* const* dest);

 private:
  const int src_channels_;
  const int src_frames_;
  const int dst_channels_;
  const int dst_frames_;
  scoped_ptr<ChannelBuffer<float>> downmix_buffer_;
  ScopedVector<PushSincResampler> resamplers_;

  DISALLOW_COPY_AND_ASSIGN(AudioConverter);
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_AUDIO_CONVERTER_H_
