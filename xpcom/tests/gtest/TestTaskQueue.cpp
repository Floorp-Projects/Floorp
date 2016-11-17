/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "VideoUtils.h"

namespace TestTaskQueue {

using namespace mozilla;

TEST(TaskQueue, EventOrder)
{
  RefPtr<TaskQueue> tq1 =
    new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK), true);
  RefPtr<TaskQueue> tq2 =
    new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK), true);
  RefPtr<TaskQueue> tq3 =
    new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK), true);

  bool errored = false;
  int counter = 0;
  int sync = 0;
  Monitor monitor("TaskQueue::EventOrder::monitor");

  // We expect task1 happens before task3.
  for (int i = 0; i < 10000; ++i) {
    tq1->Dispatch(NS_NewRunnableFunction([&] () {
      tq2->Dispatch(NS_NewRunnableFunction([] () {     // task0
      }));
      tq3->Dispatch(NS_NewRunnableFunction([&] () {    // task1
        EXPECT_EQ(1, ++counter);
        errored = counter != 1;
        MonitorAutoLock mon(monitor);
        ++sync;
        mon.Notify();
      }));
      tq2->Dispatch(NS_NewRunnableFunction([&] () {    // task2
        tq3->Dispatch(NS_NewRunnableFunction([&] () {  // task3
          EXPECT_EQ(0, --counter);
          errored = counter != 0;
          MonitorAutoLock mon(monitor);
          ++sync;
          mon.Notify();
        }));
      }));
    }), AbstractThread::AssertDispatchSuccess, AbstractThread::TailDispatch);

    // Ensure task1 and task3 are done before next loop.
    MonitorAutoLock mon(monitor);
    while (sync != 2) {
      mon.Wait();
    }
    sync = 0;

    if (errored) {
      break;
    }
  }

  tq1->BeginShutdown();
  tq1->AwaitShutdownAndIdle();
  tq2->BeginShutdown();
  tq2->AwaitShutdownAndIdle();
  tq3->BeginShutdown();
  tq3->AwaitShutdownAndIdle();
}

} // namespace TestTaskQueue
