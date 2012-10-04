/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_STREAM_SYNCHRONIZATION_H_
#define WEBRTC_VIDEO_ENGINE_STREAM_SYNCHRONIZATION_H_

#include "typedefs.h"  // NOLINT

namespace webrtc {

struct ViESyncDelay;

class StreamSynchronization {
 public:
  struct Measurements {
    Measurements()
        : received_ntp_secs(0),
          received_ntp_frac(0),
          rtcp_arrivaltime_secs(0),
          rtcp_arrivaltime_frac(0) {}
    uint32_t received_ntp_secs;
    uint32_t received_ntp_frac;
    uint32_t rtcp_arrivaltime_secs;
    uint32_t rtcp_arrivaltime_frac;
  };

  StreamSynchronization(int audio_channel_id, int video_channel_id);
  ~StreamSynchronization();

  int ComputeDelays(const Measurements& audio,
                    int current_audio_delay_ms,
                    int* extra_audio_delay_ms,
                    const Measurements& video,
                    int* total_video_delay_target_ms);

 private:
  ViESyncDelay* channel_delay_;
  int audio_channel_id_;
  int video_channel_id_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_STREAM_SYNCHRONIZATION_H_
