/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <memory>

#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "modules/utility/maybe_worker_thread.h"
#include "rtc_base/event.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"
#include "test/time_controller/real_time_controller.h"

namespace webrtc {

namespace {

constexpr char kFieldTrialString[] =
    "WebRTC-SendPacketsOnWorkerThread/Enabled/";

TEST(MaybeWorkerThreadTest, RunOrPostRunOnWorkerThreadInExperiment) {
  test::ExplicitKeyValueConfig field_trial(kFieldTrialString);
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  SequenceChecker checker;
  bool run = false;
  m.RunOrPost([&] {
    EXPECT_TRUE(checker.IsCurrent());
    run = true;
  });
  EXPECT_TRUE(run);
}

TEST(MaybeWorkerThreadTest, RunOrPostPostsOnTqPerDefault) {
  test::ExplicitKeyValueConfig field_trial("");
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  SequenceChecker checker;
  rtc::Event event;
  m.RunOrPost([&] {
    EXPECT_FALSE(checker.IsCurrent());
    event.Set();
  });
  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(10)));
}

TEST(MaybeWorkerThreadTest, RunSynchronousRunOnWorkerThreadInExperiment) {
  test::ExplicitKeyValueConfig field_trial(kFieldTrialString);
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  SequenceChecker checker;
  bool run = false;
  m.RunSynchronous([&] {
    EXPECT_TRUE(checker.IsCurrent());
    run = true;
  });
  EXPECT_TRUE(run);
}

TEST(MaybeWorkerThreadTest, RunSynchronousRunOnTqPerDefault) {
  test::ExplicitKeyValueConfig field_trial("");
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  SequenceChecker checker;
  bool run = false;
  m.RunSynchronous([&] {
    EXPECT_FALSE(checker.IsCurrent());
    run = true;
  });
  EXPECT_TRUE(run);
}

TEST(MaybeWorkerThreadTest, MaybeSafeTaskDoesNotReturnSafeTaskPerDefault) {
  // We cant really test that the return value from MaybeSafeTask is a SafeTask.
  // But we can test that the safety flag does not have more references after a
  // call.
  test::ExplicitKeyValueConfig field_trial("");
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  rtc::scoped_refptr<PendingTaskSafetyFlag> flag =
      PendingTaskSafetyFlag::Create();
  auto closure = m.MaybeSafeTask(flag, [] {});
  EXPECT_EQ(flag->Release(), rtc::RefCountReleaseStatus::kDroppedLastRef);
  flag.release();
}

TEST(MaybeWorkerThreadTest, MaybeSafeTaskDoesNotReturnSafeTaskInExperiment) {
  // We cant really test that the return value from MaybeSafeTask is a SafeTask.
  // But we can test that the safety flag does have one more references after a
  // call.
  test::ExplicitKeyValueConfig field_trial(kFieldTrialString);
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  rtc::scoped_refptr<PendingTaskSafetyFlag> flag =
      PendingTaskSafetyFlag::Create();
  auto closure = m.MaybeSafeTask(flag, [] {});
  EXPECT_EQ(flag->Release(), rtc::RefCountReleaseStatus::kOtherRefsRemained);
  flag.release();
}

TEST(MaybeWorkerThreadTest, IsCurrentBehavesCorrectPerDefault) {
  test::ExplicitKeyValueConfig field_trial("");
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  EXPECT_FALSE(m.IsCurrent());
  m.RunSynchronous([&] { EXPECT_TRUE(m.IsCurrent()); });
}

TEST(MaybeWorkerThreadTest, IsCurrentBehavesCorrectInExperiment) {
  test::ExplicitKeyValueConfig field_trial(kFieldTrialString);
  RealTimeController controller;
  MaybeWorkerThread m(field_trial, "test_tq", controller.GetTaskQueueFactory());

  EXPECT_TRUE(m.IsCurrent());
  auto tq = controller.GetTaskQueueFactory()->CreateTaskQueue(
      "tq", TaskQueueFactory::Priority::NORMAL);
  rtc::Event event;
  tq->PostTask([&] {
    EXPECT_FALSE(m.IsCurrent());
    event.Set();
  });
  ASSERT_TRUE(event.Wait(TimeDelta::Seconds(10)));
}

TEST(MaybeWorkerThreadTest, IsCurrentCanBeCalledInDestructorPerDefault) {
  test::ExplicitKeyValueConfig field_trial("");
  RealTimeController controller;
  {
    MaybeWorkerThread m(field_trial, "test_tq",
                        controller.GetTaskQueueFactory());
    m.RunOrPost([&] { EXPECT_TRUE(m.IsCurrent()); });
  }
}

TEST(MaybeWorkerThreadTest, IsCurrentCanBeCalledInDestructorInExperiment) {
  test::ExplicitKeyValueConfig field_trial(kFieldTrialString);
  RealTimeController controller;
  {
    MaybeWorkerThread m(field_trial, "test_tq",
                        controller.GetTaskQueueFactory());
    m.RunOrPost([&] { EXPECT_TRUE(m.IsCurrent()); });
  }
}

}  // namespace
}  // namespace webrtc
