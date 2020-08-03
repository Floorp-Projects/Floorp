/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"
#include "VideoUtils.h"

namespace TestTaskQueue {

using namespace mozilla;

TEST(TaskQueue, EventOrder)
{
  RefPtr<TaskQueue> tq1 =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::CONTROLLER), true);
  RefPtr<TaskQueue> tq2 =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::CONTROLLER), true);
  RefPtr<TaskQueue> tq3 =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::CONTROLLER), true);

  bool errored = false;
  int counter = 0;
  int sync = 0;
  Monitor monitor("TaskQueue::EventOrder::monitor");

  // We expect task1 happens before task3.
  for (int i = 0; i < 10000; ++i) {
    Unused << tq1->Dispatch(
        NS_NewRunnableFunction(
            "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
            [&]() {
              Unused << tq2->Dispatch(NS_NewRunnableFunction(
                  "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                  []() {  // task0
                  }));
              Unused << tq3->Dispatch(NS_NewRunnableFunction(
                  "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                  [&]() {  // task1
                    EXPECT_EQ(1, ++counter);
                    errored = counter != 1;
                    MonitorAutoLock mon(monitor);
                    ++sync;
                    mon.Notify();
                  }));
              Unused << tq2->Dispatch(NS_NewRunnableFunction(
                  "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                  [&]() {  // task2
                    Unused << tq3->Dispatch(NS_NewRunnableFunction(
                        "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                        [&]() {  // task3
                          EXPECT_EQ(0, --counter);
                          errored = counter != 0;
                          MonitorAutoLock mon(monitor);
                          ++sync;
                          mon.Notify();
                        }));
                  }));
            }),
        AbstractThread::TailDispatch);

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

TEST(TaskQueue, GetCurrentSerialEventTarget)
{
  RefPtr<TaskQueue> tq1 =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::CONTROLLER), false);
  Unused << tq1->Dispatch(NS_NewRunnableFunction(
      "TestTaskQueue::TestCurrentSerialEventTarget::TestBody", [tq1]() {
        nsCOMPtr<nsISerialEventTarget> thread = GetCurrentSerialEventTarget();
        EXPECT_EQ(thread, tq1);
      }));
  tq1->BeginShutdown();
  tq1->AwaitShutdownAndIdle();
}

TEST(AbstractThread, GetCurrentSerialEventTarget)
{
  RefPtr<AbstractThread> mainThread = AbstractThread::GetCurrent();
  EXPECT_EQ(mainThread, AbstractThread::MainThread());
  Unused << mainThread->Dispatch(NS_NewRunnableFunction(
      "TestAbstractThread::TestCurrentSerialEventTarget::TestBody",
      [mainThread]() {
        nsCOMPtr<nsISerialEventTarget> thread = GetCurrentSerialEventTarget();
        EXPECT_EQ(thread, mainThread);
      }));

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

}  // namespace TestTaskQueue
