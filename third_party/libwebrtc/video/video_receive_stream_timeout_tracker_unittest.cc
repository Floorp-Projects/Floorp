/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_receive_stream_timeout_tracker.h"

#include <utility>

#include "api/task_queue/task_queue_base.h"
#include "rtc_base/task_queue.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

namespace {

constexpr auto kMaxWaitForKeyframe = TimeDelta::Millis(500);
constexpr auto kMaxWaitForFrame = TimeDelta::Millis(1500);
constexpr VideoReceiveStreamTimeoutTracker::Timeouts config = {
    kMaxWaitForKeyframe, kMaxWaitForFrame};
}  // namespace

class VideoReceiveStreamTimeoutTrackerTest : public ::testing::Test {
 public:
  VideoReceiveStreamTimeoutTrackerTest()
      : time_controller_(Timestamp::Millis(2000)),
        task_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "scheduler",
            TaskQueueFactory::Priority::NORMAL)),
        timeout_tracker_(time_controller_.GetClock(),
                         task_queue_.Get(),
                         config,
                         [this] { OnTimeout(); }) {}

 protected:
  template <class Task>
  void OnQueue(Task&& t) {
    task_queue_.PostTask(std::forward<Task>(t));
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  GlobalSimulatedTimeController time_controller_;
  rtc::TaskQueue task_queue_;
  VideoReceiveStreamTimeoutTracker timeout_tracker_;
  int timeouts_ = 0;

 private:
  void OnTimeout() { ++timeouts_; }
};

TEST_F(VideoReceiveStreamTimeoutTrackerTest, TimeoutAfterInitialPeriod) {
  OnQueue([&] { timeout_tracker_.Start(true); });
  time_controller_.AdvanceTime(kMaxWaitForKeyframe);
  EXPECT_EQ(1, timeouts_);
  OnQueue([&] { timeout_tracker_.Stop(); });
}

TEST_F(VideoReceiveStreamTimeoutTrackerTest, NoTimeoutAfterStop) {
  OnQueue([&] { timeout_tracker_.Start(true); });
  time_controller_.AdvanceTime(kMaxWaitForKeyframe / 2);
  OnQueue([&] { timeout_tracker_.Stop(); });
  time_controller_.AdvanceTime(kMaxWaitForKeyframe);
  EXPECT_EQ(0, timeouts_);
}

TEST_F(VideoReceiveStreamTimeoutTrackerTest, TimeoutForDeltaFrame) {
  OnQueue([&] { timeout_tracker_.Start(true); });
  time_controller_.AdvanceTime(TimeDelta::Millis(5));
  OnQueue([&] { timeout_tracker_.OnEncodedFrameReleased(); });
  time_controller_.AdvanceTime(kMaxWaitForFrame);
  EXPECT_EQ(1, timeouts_);

  OnQueue([&] { timeout_tracker_.Stop(); });
}

TEST_F(VideoReceiveStreamTimeoutTrackerTest, TimeoutForKeyframeWhenForced) {
  OnQueue([&] { timeout_tracker_.Start(true); });
  time_controller_.AdvanceTime(TimeDelta::Millis(5));
  OnQueue([&] { timeout_tracker_.OnEncodedFrameReleased(); });
  OnQueue([&] { timeout_tracker_.SetWaitingForKeyframe(); });
  time_controller_.AdvanceTime(kMaxWaitForKeyframe);
  EXPECT_EQ(1, timeouts_);

  OnQueue([&] { timeout_tracker_.Stop(); });
}

}  // namespace webrtc
