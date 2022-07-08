/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_DECODE_SCHEDULER_H_
#define VIDEO_FRAME_DECODE_SCHEDULER_H_

#include <stdint.h>

#include <functional>

#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "system_wrappers/include/clock.h"
#include "video/frame_decode_timing.h"

namespace webrtc {

class FrameDecodeScheduler {
 public:
  // Invoked when a frame with `rtp_timestamp` is ready for decoding.
  using FrameReleaseCallback =
      std::function<void(uint32_t rtp_timestamp, Timestamp render_time)>;

  FrameDecodeScheduler(Clock* clock,
                       TaskQueueBase* const bookkeeping_queue,
                       FrameReleaseCallback callback);
  ~FrameDecodeScheduler();
  FrameDecodeScheduler(const FrameDecodeScheduler&) = delete;
  FrameDecodeScheduler& operator=(const FrameDecodeScheduler&) = delete;

  absl::optional<uint32_t> scheduled_rtp() const { return scheduled_rtp_; }

  void ScheduleFrame(uint32_t rtp, FrameDecodeTiming::FrameSchedule schedule);
  void CancelOutstanding();

 private:
  Clock* const clock_;
  TaskQueueBase* const bookkeeping_queue_;
  const FrameReleaseCallback callback_;

  absl::optional<uint32_t> scheduled_rtp_;
  ScopedTaskSafetyDetached task_safety_;
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_DECODE_SCHEDULER_H_
