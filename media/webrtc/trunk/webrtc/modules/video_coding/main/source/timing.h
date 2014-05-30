/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TIMING_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TIMING_H_

#include "webrtc/modules/video_coding/main/source/codec_timer.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;
class VCMTimestampExtrapolator;

class VCMTiming {
 public:
  // The primary timing component should be passed
  // if this is the dual timing component.
  VCMTiming(Clock* clock,
            int32_t vcm_id = 0,
            int32_t timing_id = 0,
            VCMTiming* master_timing = NULL);
  ~VCMTiming();

  // Resets the timing to the initial state.
  void Reset();
  void ResetDecodeTime();

  // Set the amount of time needed to render an image. Defaults to 10 ms.
  void set_render_delay(uint32_t render_delay_ms);

  // Set the minimum time the video must be delayed on the receiver to
  // get the desired jitter buffer level.
  void SetJitterDelay(uint32_t required_delay_ms);

  // Set the minimum playout delay required to sync video with audio.
  void set_min_playout_delay(uint32_t min_playout_delay);

  // Increases or decreases the current delay to get closer to the target delay.
  // Calculates how long it has been since the previous call to this function,
  // and increases/decreases the delay in proportion to the time difference.
  void UpdateCurrentDelay(uint32_t frame_timestamp);

  // Increases or decreases the current delay to get closer to the target delay.
  // Given the actual decode time in ms and the render time in ms for a frame,
  // this function calculates how late the frame is and increases the delay
  // accordingly.
  void UpdateCurrentDelay(int64_t render_time_ms,
                          int64_t actual_decode_time_ms);

  // Stops the decoder timer, should be called when the decoder returns a frame
  // or when the decoded frame callback is called.
  int32_t StopDecodeTimer(uint32_t time_stamp,
                          int64_t start_time_ms,
                          int64_t now_ms);

  // Used to report that a frame is passed to decoding. Updates the timestamp
  // filter which is used to map between timestamps and receiver system time.
  void IncomingTimestamp(uint32_t time_stamp, int64_t last_packet_time_ms);
  // Returns the receiver system time when the frame with timestamp
  // frame_timestamp should be rendered, assuming that the system time currently
  // is now_ms.
  int64_t RenderTimeMs(uint32_t frame_timestamp, int64_t now_ms) const;

  // Returns the maximum time in ms that we can wait for a frame to become
  // complete before we must pass it to the decoder.
  uint32_t MaxWaitingTime(int64_t render_time_ms, int64_t now_ms) const;

  // Returns the current target delay which is required delay + decode time +
  // render delay.
  uint32_t TargetVideoDelay() const;

  // Calculates whether or not there is enough time to decode a frame given a
  // certain amount of processing time.
  bool EnoughTimeToDecode(uint32_t available_processing_time_ms) const;

  // Return current timing information.
  void GetTimings(int* decode_ms,
                  int* max_decode_ms,
                  int* current_delay_ms,
                  int* target_delay_ms,
                  int* jitter_buffer_ms,
                  int* min_playout_delay_ms,
                  int* render_delay_ms) const;

  enum { kDefaultRenderDelayMs = 10 };
  enum { kDelayMaxChangeMsPerS = 100 };

 protected:
  int32_t MaxDecodeTimeMs(FrameType frame_type = kVideoFrameDelta) const;
  int64_t RenderTimeMsInternal(uint32_t frame_timestamp, int64_t now_ms) const;
  uint32_t TargetDelayInternal() const;

 private:
  CriticalSectionWrapper* crit_sect_;
  int32_t vcm_id_;
  Clock* clock_;
  int32_t timing_id_;
  bool master_;
  VCMTimestampExtrapolator* ts_extrapolator_;
  VCMCodecTimer codec_timer_;
  uint32_t render_delay_ms_;
  uint32_t min_playout_delay_ms_;
  uint32_t jitter_delay_ms_;
  uint32_t current_delay_ms_;
  int last_decode_ms_;
  uint32_t prev_frame_timestamp_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TIMING_H_
