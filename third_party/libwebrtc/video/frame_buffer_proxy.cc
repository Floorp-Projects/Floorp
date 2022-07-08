/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_buffer_proxy.h"

#include <memory>
#include <utility>

#include "modules/video_coding/frame_buffer2.h"

namespace webrtc {

class FrameBuffer2Proxy : public FrameBufferProxy {
 public:
  FrameBuffer2Proxy(Clock* clock,
                    VCMTiming* timing,
                    VCMReceiveStatisticsCallback* stats_proxy,
                    rtc::TaskQueue* decode_queue,
                    FrameSchedulingReceiver* receiver,
                    TimeDelta max_wait_for_keyframe,
                    TimeDelta max_wait_for_frame)
      : max_wait_for_keyframe_(max_wait_for_keyframe),
        max_wait_for_frame_(max_wait_for_frame),
        frame_buffer_(clock, timing, stats_proxy),
        decode_queue_(decode_queue),
        stats_proxy_(stats_proxy),
        receiver_(receiver) {
    RTC_DCHECK(decode_queue_);
    RTC_DCHECK(stats_proxy_);
    RTC_DCHECK(receiver_);
  }

  void StopOnWorker() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    decode_queue_->PostTask([this] {
      frame_buffer_.Stop();
      decode_safety_->SetNotAlive();
    });
  }

  void SetProtectionMode(VCMVideoProtection protection_mode) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    frame_buffer_.SetProtectionMode(kProtectionNackFEC);
  }

  void Clear() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    frame_buffer_.Clear();
  }

  absl::optional<int64_t> InsertFrame(
      std::unique_ptr<EncodedFrame> frame) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    int64_t last_continuous_pid = frame_buffer_.InsertFrame(std::move(frame));
    if (last_continuous_pid != -1)
      return last_continuous_pid;
    return absl::nullopt;
  }

  void UpdateRtt(int64_t max_rtt_ms) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    frame_buffer_.UpdateRtt(max_rtt_ms);
  }

  void StartNextDecode(bool keyframe_required) override {
    if (!decode_queue_->IsCurrent()) {
      decode_queue_->PostTask(ToQueuedTask(
          decode_safety_,
          [this, keyframe_required] { StartNextDecode(keyframe_required); }));
      return;
    }
    RTC_DCHECK_RUN_ON(decode_queue_);

    frame_buffer_.NextFrame(
        MaxWait(keyframe_required).ms(), keyframe_required, decode_queue_,
        /* encoded frame handler */
        [this, keyframe_required](std::unique_ptr<EncodedFrame> frame) {
          RTC_DCHECK_RUN_ON(decode_queue_);
          if (!decode_safety_->alive())
            return;
          if (frame) {
            receiver_->OnEncodedFrame(std::move(frame));
          } else {
            receiver_->OnDecodableFrameTimeout(MaxWait(keyframe_required));
          }
        });
  }

  int Size() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    return frame_buffer_.Size();
  }

 private:
  TimeDelta MaxWait(bool keyframe_required) const {
    return keyframe_required ? max_wait_for_keyframe_ : max_wait_for_frame_;
  }

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_;
  const TimeDelta max_wait_for_keyframe_;
  const TimeDelta max_wait_for_frame_;
  video_coding::FrameBuffer frame_buffer_;
  rtc::TaskQueue* const decode_queue_;
  VCMReceiveStatisticsCallback* const stats_proxy_;
  FrameSchedulingReceiver* const receiver_;
  rtc::scoped_refptr<PendingTaskSafetyFlag> decode_safety_ =
      PendingTaskSafetyFlag::CreateDetached();
};

// TODO(bugs.webrtc.org/13343): Create FrameBuffer3Proxy when complete.
std::unique_ptr<FrameBufferProxy> FrameBufferProxy::CreateFromFieldTrial(
    Clock* clock,
    TaskQueueBase* worker_queue,
    VCMTiming* timing,
    VCMReceiveStatisticsCallback* stats_proxy,
    rtc::TaskQueue* decode_queue,
    FrameSchedulingReceiver* receiver,
    TimeDelta max_wait_for_keyframe,
    TimeDelta max_wait_for_frame) {
  return std::make_unique<FrameBuffer2Proxy>(
      clock, timing, stats_proxy, decode_queue, receiver, max_wait_for_keyframe,
      max_wait_for_frame);
}

}  // namespace webrtc
