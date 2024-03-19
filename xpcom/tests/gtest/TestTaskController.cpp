/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <stdint.h>  // uint32_t

#include "nsString.h"                // nsACString
#include "nsThreadUtils.h"           // NS_ProcessNextEvent
#include "mozilla/Atomics.h"         // Atomic
#include "mozilla/EventQueue.h"      // EventQueuePriority
#include "mozilla/Mutex.h"           // Mutex, MutexAutoLock
#include "mozilla/RefPtr.h"          // RefPtr, do_AddRef
#include "mozilla/TaskController.h"  // TaskController, Task
#include "prthread.h"                // PR_Sleep

using namespace mozilla;

namespace TestTaskController {

class Logger {
 public:
  Logger() : mMutex("Logger") {}

  void Add(const char* aText) {
    MutexAutoLock lock(mMutex);

    mLog += aText;
  }

  const nsAutoCString& GetLog() const { return mLog; }

 private:
  nsAutoCString mLog;
  Mutex mMutex;
};

class ReschedulingTask : public Task {
  static constexpr uint32_t LoopCount = 3;

 public:
  explicit ReschedulingTask(Kind aKind, Logger* aLogger, const char* aName)
      : Task(aKind, EventQueuePriority::Normal),
        mCount(0),
        mIsDone(false),
        mLogger(aLogger),
        mName(aName) {}

  TaskResult Run() override {
    mLogger->Add(mName);

    mCount++;

    if (mCount < LoopCount) {
      return TaskResult::Incomplete;
    }

    mIsDone = true;

    return TaskResult::Complete;
  }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  bool GetName(nsACString& aName) override {
    aName.AssignLiteral("AsyncScriptCompileTask");
    return true;
  }
#endif

  bool IsDone() const { return mIsDone; }

 private:
  Atomic<uint32_t> mCount;
  Atomic<bool> mIsDone;
  Logger* mLogger;
  const char* mName;
};

using namespace mozilla;

TEST(TaskController, RescheduleOnMainThread)
{
  Logger logger;

  RefPtr<ReschedulingTask> mainThreadTask =
      new ReschedulingTask(Task::Kind::MainThreadOnly, &logger, "1");

  TaskController::Get()->AddTask(do_AddRef(mainThreadTask));

  while (NS_ProcessNextEvent(nullptr, false)) {
  }

  ASSERT_TRUE(mainThreadTask->IsDone());

  ASSERT_TRUE(logger.GetLog() == "111");
}

TEST(TaskController, RescheduleOffMainThread)
{
  Logger logger;

  RefPtr<ReschedulingTask> offThreadTask =
      new ReschedulingTask(Task::Kind::OffMainThreadOnly, &logger, "1");

  TaskController::Get()->AddTask(do_AddRef(offThreadTask));

  uint32_t count = 0;
  while (!offThreadTask->IsDone() && count < 100) {
    PR_Sleep(PR_MillisecondsToInterval(100));
    count++;
  }
  ASSERT_TRUE(offThreadTask->IsDone());

  ASSERT_TRUE(logger.GetLog() == "111");
}

TEST(TaskController, RescheduleMainAndOffMainThreads)
{
  Logger logger;

  RefPtr<ReschedulingTask> offThreadTask =
      new ReschedulingTask(Task::Kind::OffMainThreadOnly, &logger, "1");
  RefPtr<ReschedulingTask> mainThreadTask =
      new ReschedulingTask(Task::Kind::MainThreadOnly, &logger, "2");

  mainThreadTask->AddDependency(offThreadTask.get());

  TaskController::Get()->AddTask(do_AddRef(offThreadTask));
  TaskController::Get()->AddTask(do_AddRef(mainThreadTask));

  uint32_t count = 0;
  while (!offThreadTask->IsDone() && count < 100) {
    PR_Sleep(PR_MillisecondsToInterval(100));
    count++;
  }
  ASSERT_TRUE(offThreadTask->IsDone());

  // At this point, the main thread task shouldn't have run.
  ASSERT_TRUE(logger.GetLog() == "111");

  while (NS_ProcessNextEvent(nullptr, false)) {
  }

  ASSERT_TRUE(mainThreadTask->IsDone());

  ASSERT_TRUE(logger.GetLog() == "111222");
}

TEST(TaskController, RescheduleOrder)
{
  Logger logger;

  RefPtr<ReschedulingTask> mainThreadTask1 =
      new ReschedulingTask(Task::Kind::MainThreadOnly, &logger, "1");
  RefPtr<ReschedulingTask> mainThreadTask2 =
      new ReschedulingTask(Task::Kind::MainThreadOnly, &logger, "2");
  RefPtr<ReschedulingTask> mainThreadTask3 =
      new ReschedulingTask(Task::Kind::MainThreadOnly, &logger, "3");

  TaskController::Get()->AddTask(do_AddRef(mainThreadTask1));
  TaskController::Get()->AddTask(do_AddRef(mainThreadTask2));
  TaskController::Get()->AddTask(do_AddRef(mainThreadTask3));

  while (NS_ProcessNextEvent(nullptr, false)) {
  }

  ASSERT_TRUE(mainThreadTask1->IsDone());
  ASSERT_TRUE(mainThreadTask2->IsDone());
  ASSERT_TRUE(mainThreadTask3->IsDone());

  // Rescheduled tasks should be added to the beginning of the queue.
  ASSERT_TRUE(logger.GetLog() == "111222333");
}

TEST(TaskController, RescheduleOrderOffMainThread)
{
  Logger logger1;
  Logger logger2;
  Logger logger3;

  RefPtr<ReschedulingTask> offThreadTask1 =
      new ReschedulingTask(Task::Kind::OffMainThreadOnly, &logger1, "1");
  RefPtr<ReschedulingTask> offThreadTask2 =
      new ReschedulingTask(Task::Kind::OffMainThreadOnly, &logger2, "2");
  RefPtr<ReschedulingTask> offThreadTask3 =
      new ReschedulingTask(Task::Kind::OffMainThreadOnly, &logger3, "3");

  TaskController::Get()->AddTask(do_AddRef(offThreadTask1));
  TaskController::Get()->AddTask(do_AddRef(offThreadTask2));
  TaskController::Get()->AddTask(do_AddRef(offThreadTask3));

  uint32_t count = 0;
  while (!(offThreadTask1->IsDone() && offThreadTask2->IsDone() &&
           offThreadTask3->IsDone()) &&
         count < 100) {
    PR_Sleep(PR_MillisecondsToInterval(100));
    count++;
  }

  ASSERT_TRUE(offThreadTask1->IsDone());
  ASSERT_TRUE(offThreadTask2->IsDone());
  ASSERT_TRUE(offThreadTask3->IsDone());

  // Rescheduled tasks should be enqueued.
  // The order between off-thread tasks are not deterministic.
  ASSERT_TRUE(logger1.GetLog() == "111");
  ASSERT_TRUE(logger2.GetLog() == "222");
  ASSERT_TRUE(logger3.GetLog() == "333");
}

}  // namespace TestTaskController
