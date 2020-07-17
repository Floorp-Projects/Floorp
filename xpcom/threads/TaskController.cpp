/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskController.h"
#include "nsIIdleRunnable.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include <algorithm>
#include <initializer_list>
#include "mozilla/AbstractEventQueue.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "nsIThreadInternal.h"
#include "nsQueryObject.h"
#include "nsThread.h"

namespace mozilla {

std::unique_ptr<TaskController> TaskController::sSingleton;
uint64_t Task::sCurrentTaskSeqNo = 0;

bool TaskManager::
    UpdateCachesForCurrentIterationAndReportPriorityModifierChanged(
        const MutexAutoLock& aProofOfLock, IterationType aIterationType) {
  mCurrentSuspended = IsSuspended(aProofOfLock);

  if (aIterationType == IterationType::EVENT_LOOP_TURN) {
    int32_t oldModifier = mCurrentPriorityModifier;
    mCurrentPriorityModifier =
        GetPriorityModifierForEventLoopTurn(aProofOfLock);

    if (mCurrentPriorityModifier != oldModifier) {
      return true;
    }
  }
  return false;
}

Task* Task::GetHighestPriorityDependency() {
  Task* currentTask = this;

  while (!currentTask->mDependencies.empty()) {
    auto iter = currentTask->mDependencies.begin();

    while (iter != currentTask->mDependencies.end()) {
      if ((*iter)->mCompleted) {
        auto oldIter = iter;
        iter++;
        // Completed tasks are removed here to prevent needlessly keeping them
        // alive or iterating over them in the future.
        currentTask->mDependencies.erase(oldIter);
        continue;
      }

      currentTask = iter->get();
      break;
    }
  }

  return currentTask == this ? nullptr : currentTask;
}

TaskController* TaskController::Get() {
  MOZ_ASSERT(sSingleton.get());
  return sSingleton.get();
}

bool TaskController::Initialize() {
  MOZ_ASSERT(!sSingleton);
  sSingleton = std::make_unique<TaskController>();
  return sSingleton->InitializeInternal();
}

bool TaskController::InitializeInternal() {
  mMTProcessingRunnable = NS_NewRunnableFunction(
      "TaskController::ExecutePendingMTTasks()",
      []() { TaskController::Get()->ProcessPendingMTTask(); });
  mMTBlockingProcessingRunnable = NS_NewRunnableFunction(
      "TaskController::ExecutePendingMTTasks()",
      []() { TaskController::Get()->ProcessPendingMTTask(true); });

  return true;
}

void TaskController::SetPerformanceCounterState(
    PerformanceCounterState* aPerformanceCounterState) {
  mPerformanceCounterState = aPerformanceCounterState;
}

/* static */
void TaskController::Shutdown() {
  if (sSingleton) {
    sSingleton->ShutdownInternal();
  }
  MOZ_ASSERT(!sSingleton);
}

void TaskController::ShutdownInternal() { sSingleton = nullptr; }

void TaskController::AddTask(already_AddRefed<Task>&& aTask) {
  MutexAutoLock lock(mGraphMutex);

  RefPtr<Task> task(aTask);

  if (TaskManager* manager = task->GetManager()) {
    if (manager->mTaskCount == 0) {
      mTaskManagers.insert(manager);
    }
    manager->DidQueueTask();

    // Set this here since if this manager's priority modifier doesn't change
    // we will not reprioritize when iterating over the queue.
    task->mPriorityModifier = manager->mCurrentPriorityModifier;
  }

#ifdef MOZ_GECKO_PROFILER
  task->mInsertionTime = TimeStamp::Now();
#endif

#ifdef DEBUG
  task->mIsInGraph = true;

  for (const RefPtr<Task>& otherTask : task->mDependencies) {
    MOZ_ASSERT(!otherTask->mTaskManager ||
               otherTask->mTaskManager == task->mTaskManager);
  }
#endif

  LogTask::LogDispatch(task);

  auto insertion = mMainThreadTasks.insert(std::move(task));
  MOZ_ASSERT(insertion.second);
  (*insertion.first)->mIterator = insertion.first;

  MaybeInterruptTask(*insertion.first);
}

void TaskController::WaitForTaskOrMessage() {
  MutexAutoLock lock(mGraphMutex);
  while (!mMayHaveMainThreadTask) {
    AUTO_PROFILER_LABEL("TaskController::WaitForTaskOrMessage", IDLE);
    mMainThreadCV.Wait();
  }
}

void TaskController::ExecuteNextTaskOnlyMainThread() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mGraphMutex);
  ExecuteNextTaskOnlyMainThreadInternal(lock);
}

void TaskController::ProcessPendingMTTask(bool aMayWait) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mGraphMutex);

  for (;;) {
    // We only ever process one event here. However we may sometimes
    // not actually process a real event because of suspended tasks.
    // This loop allows us to wait until we've processed something
    // in that scenario.

    mMTTaskRunnableProcessedTask = ExecuteNextTaskOnlyMainThreadInternal(lock);

    if (mMTTaskRunnableProcessedTask || !aMayWait) {
      break;
    }

    BackgroundHangMonitor().NotifyWait();

    {
      // ProcessNextEvent will also have attempted to wait, however we may have
      // given it a Runnable when all the tasks in our task graph were suspended
      // but we weren't able to cheaply determine that.
      AUTO_PROFILER_LABEL("TaskController::ProcessPendingMTTask", IDLE);
      mMainThreadCV.Wait();
    }

    BackgroundHangMonitor().NotifyActivity();
  }

  if (mMayHaveMainThreadTask) {
    EnsureMainThreadTasksScheduled();
  }
}

void TaskController::ReprioritizeTask(Task* aTask, uint32_t aPriority) {
  MutexAutoLock lock(mGraphMutex);
  std::set<RefPtr<Task>, Task::PriorityCompare>* queue = &mMainThreadTasks;

  MOZ_ASSERT(aTask->mIterator != queue->end());
  queue->erase(aTask->mIterator);

  aTask->mPriority = aPriority;

  auto insertion = queue->insert(aTask);
  MOZ_ASSERT(insertion.second);
  aTask->mIterator = insertion.first;

  MaybeInterruptTask(aTask);
}

// Code supporting runnable compatibility.
// Task that wraps a runnable.
class RunnableTask : public Task {
 public:
  RunnableTask(already_AddRefed<nsIRunnable>&& aRunnable, int32_t aPriority,
               bool aMainThread = true)
      : Task(aMainThread, aPriority), mRunnable(aRunnable) {}

  virtual bool Run() override {
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    MOZ_ASSERT(NS_IsMainThread());
    // If we're on the main thread, we want to record our current
    // runnable's name in a static so that BHR can record it.
    Array<char, nsThread::kRunnableNameBufSize> restoreRunnableName;
    restoreRunnableName[0] = '\0';
    auto clear = MakeScopeExit([&] {
      MOZ_ASSERT(NS_IsMainThread());
      nsThread::sMainThreadRunnableName = restoreRunnableName;
    });
    nsAutoCString name;
    nsThread::GetLabeledRunnableName(mRunnable, name,
                                     EventQueuePriority(GetPriority()));

    restoreRunnableName = nsThread::sMainThreadRunnableName;

    // Copy the name into sMainThreadRunnableName's buffer, and append a
    // terminating null.
    uint32_t length = std::min((uint32_t)nsThread::kRunnableNameBufSize - 1,
                               (uint32_t)name.Length());
    memcpy(nsThread::sMainThreadRunnableName.begin(), name.BeginReading(),
           length);
    nsThread::sMainThreadRunnableName[length] = '\0';
#endif

    mRunnable->Run();
    mRunnable = nullptr;
    return true;
  }

  void SetIdleDeadline(TimeStamp aDeadline) override {
    nsCOMPtr<nsIIdleRunnable> idleRunnable = do_QueryInterface(mRunnable);
    if (idleRunnable) {
      idleRunnable->SetDeadline(aDeadline);
    }
  }

  PerformanceCounter* GetPerformanceCounter() const override {
    return nsThread::GetPerformanceCounterBase(mRunnable);
  }

 private:
  RefPtr<nsIRunnable> mRunnable;
};

void TaskController::DispatchRunnable(already_AddRefed<nsIRunnable>&& aRunnable,
                                      uint32_t aPriority,
                                      TaskManager* aManager) {
  RefPtr<RunnableTask> task = new RunnableTask(std::move(aRunnable), aPriority);

  task->SetManager(aManager);
  TaskController::Get()->AddTask(task.forget());
}

nsIRunnable* TaskController::GetRunnableForMTTask(bool aReallyWait) {
  MutexAutoLock lock(mGraphMutex);

  while (mMainThreadTasks.empty()) {
    if (!aReallyWait) {
      return nullptr;
    }

    AUTO_PROFILER_LABEL("TaskController::GetRunnableForMTTask::Wait", IDLE);
    mMainThreadCV.Wait();
  }

  return aReallyWait ? mMTBlockingProcessingRunnable : mMTProcessingRunnable;
}

bool TaskController::HasMainThreadPendingTasks() {
  auto resetIdleState = MakeScopeExit([&idleManager = mIdleTaskManager] {
    if (idleManager) {
      idleManager->State().ClearCachedIdleDeadline();
    }
  });

  for (bool considerIdle : {false, true}) {
    if (considerIdle && !mIdleTaskManager) {
      continue;
    }

    MutexAutoLock lock(mGraphMutex);

    if (considerIdle) {
      mIdleTaskManager->State().ForgetPendingTaskGuarantee();
      // Temporarily unlock so we can peek our idle deadline.
      // XXX We could do this _before_ we take the lock if the API would let us.
      // We do want to do this before looking at mMainThreadTasks, in case
      // someone adds one while we're unlocked.
      {
        MutexAutoUnlock unlock(mGraphMutex);
        mIdleTaskManager->State().CachePeekedIdleDeadline(unlock);
      }
    }

    // Return early if there's no tasks at all.
    if (mMainThreadTasks.empty()) {
      return false;
    }

    // We can cheaply count how many tasks are suspended.
    uint64_t totalSuspended = 0;
    for (TaskManager* manager : mTaskManagers) {
      DebugOnly<bool> modifierChanged =
          manager
              ->UpdateCachesForCurrentIterationAndReportPriorityModifierChanged(
                  lock, TaskManager::IterationType::NOT_EVENT_LOOP_TURN);
      MOZ_ASSERT(!modifierChanged);

      // The idle manager should be suspended unless we're doing the idle pass.
      MOZ_ASSERT(manager != mIdleTaskManager || manager->mCurrentSuspended ||
                     considerIdle,
                 "Why are idle tasks not suspended here?");

      if (manager->mCurrentSuspended) {
        // XXX - If managers manage off-main-thread tasks this breaks! This
        // scenario is explicitly not supported.
        //
        // This is only incremented inside the lock -or- decremented on the main
        // thread so this is safe.
        totalSuspended += manager->mTaskCount;
      }
    }

    // Thi would break down if we have a non-suspended task depending on a
    // suspended task. This is why for the moment we do not allow tasks
    // to be dependent on tasks managed by another taskmanager.
    if (mMainThreadTasks.size() > totalSuspended) {
      // If mIdleTaskManager->mTaskCount is 0, we never updated the suspended
      // state of mIdleTaskManager above, hence shouldn't even check it here.
      // But in that case idle tasks are not contributing to our suspended task
      // count anyway.
      if (mIdleTaskManager && mIdleTaskManager->mTaskCount &&
          !mIdleTaskManager->mCurrentSuspended) {
        MOZ_ASSERT(considerIdle, "Why is mIdleTaskManager not suspended?");
        // Check whether the idle tasks were really needed to make our "we have
        // an unsuspended task" decision.  If they were, we need to force-enable
        // idle tasks until we run our next task.
        if (mMainThreadTasks.size() - mIdleTaskManager->mTaskCount <=
            totalSuspended) {
          mIdleTaskManager->State().EnforcePendingTaskGuarantee();
        }
      }
      return true;
    }
  }
  return false;
}

bool TaskController::ExecuteNextTaskOnlyMainThreadInternal(
    const MutexAutoLock& aProofOfLock) {
  // Block to make it easier to jump to our cleanup.
  bool taskRan = false;
  do {
    taskRan = DoExecuteNextTaskOnlyMainThreadInternal(aProofOfLock);
    if (taskRan) {
      break;
    }

    if (!mIdleTaskManager) {
      break;
    }

    if (mIdleTaskManager->mTaskCount) {
      // We have idle tasks that we may not have gotten above because
      // our idle state is not up to date.  We need to update the idle state
      // and try again.  We need to temporarily release the lock while we do
      // that.
      MutexAutoUnlock unlock(mGraphMutex);
      mIdleTaskManager->State().UpdateCachedIdleDeadline(unlock);
    } else {
      MutexAutoUnlock unlock(mGraphMutex);
      mIdleTaskManager->State().RanOutOfTasks(unlock);
    }

    // When we unlocked, someone may have queued a new task on us.  So try to
    // see whether we can run things again.
    taskRan = DoExecuteNextTaskOnlyMainThreadInternal(aProofOfLock);
  } while (false);

  if (mIdleTaskManager) {
    // The pending task guarantee is not needed anymore, since we just tried
    // running a task
    mIdleTaskManager->State().ForgetPendingTaskGuarantee();

    if (mMainThreadTasks.empty()) {
      // XXX the IdlePeriodState API demands we have a MutexAutoUnlock for it.
      // Otherwise we could perhaps just do this after we exit the locked block,
      // by pushing the lock down into this method.  Though it's not clear that
      // we could check mMainThreadTasks.size() once we unlock, and whether we
      // could maybe substitute mMayHaveMainThreadTask for that check.
      MutexAutoUnlock unlock(mGraphMutex);
      mIdleTaskManager->State().RanOutOfTasks(unlock);
    }
  }

  return taskRan;
}

bool TaskController::DoExecuteNextTaskOnlyMainThreadInternal(
    const MutexAutoLock& aProofOfLock) {
  nsCOMPtr<nsIThread> mainIThread;
  NS_GetMainThread(getter_AddRefs(mainIThread));
  nsThread* mainThread = static_cast<nsThread*>(mainIThread.get());
  mainThread->SetRunningEventDelay(TimeDuration(), TimeStamp());

  uint32_t totalSuspended = 0;
  for (TaskManager* manager : mTaskManagers) {
    bool modifierChanged =
        manager
            ->UpdateCachesForCurrentIterationAndReportPriorityModifierChanged(
                aProofOfLock, TaskManager::IterationType::EVENT_LOOP_TURN);
    if (modifierChanged) {
      ProcessUpdatedPriorityModifier(manager);
    }
    if (manager->mCurrentSuspended) {
      totalSuspended += manager->mTaskCount;
    }
  }

  MOZ_ASSERT(mMainThreadTasks.size() >= totalSuspended);

  // This would break down if we have a non-suspended task depending on a
  // suspended task. This is why for the moment we do not allow tasks
  // to be dependent on tasks managed by another taskmanager.
  if (mMainThreadTasks.size() > totalSuspended) {
    for (auto iter = mMainThreadTasks.begin(); iter != mMainThreadTasks.end();
         iter++) {
      Task* task = iter->get();

      if (task->mTaskManager && task->mTaskManager->mCurrentSuspended) {
        // Even though we may want to run some dependencies of this task, we
        // will run them at their own priority level and not the priority
        // level of their dependents.
        continue;
      }

      task = GetFinalDependency(task);

      if (!task->IsMainThreadOnly() || task->mInProgress ||
          (task->mTaskManager && task->mTaskManager->mCurrentSuspended)) {
        continue;
      }

      mCurrentTasksMT.push(task);
      mMainThreadTasks.erase(task->mIterator);
      task->mIterator = mMainThreadTasks.end();
      task->mInProgress = true;
      TaskManager* manager = task->GetManager();
      bool result = false;

      {
        MutexAutoUnlock unlock(mGraphMutex);
        if (manager) {
          manager->WillRunTask();
          if (manager != mIdleTaskManager) {
            // Notify the idle period state that we're running a non-idle task.
            // This needs to happen while our mutex is not locked!
            mIdleTaskManager->State().FlagNotIdle();
          } else {
            TimeStamp idleDeadline =
                mIdleTaskManager->State().GetCachedIdleDeadline();
            MOZ_ASSERT(
                idleDeadline,
                "How can we not have a deadline if our manager is enabled?");
            task->SetIdleDeadline(idleDeadline);
          }
        }
        if (mIdleTaskManager) {
          // We found a task to run; we can clear the idle deadline on our idle
          // task manager.  This _must_ be done before we actually run the task,
          // because running the task could reenter via spinning the event loop
          // and we want to make sure there's no cached idle deadline at that
          // point.  But we have to make sure we do it after out SetIdleDeadline
          // call above, in the case when the task is actually an idle task.
          mIdleTaskManager->State().ClearCachedIdleDeadline();
        }

        TimeStamp now = TimeStamp::Now();

#ifdef MOZ_GECKO_PROFILER
        if (task->GetPriority() < uint32_t(EventQueuePriority::InputHigh)) {
          mainThread->SetRunningEventDelay(TimeDuration(), now);
        } else {
          mainThread->SetRunningEventDelay(now - task->mInsertionTime, now);
        }
#endif

        PerformanceCounterState::Snapshot snapshot =
            mPerformanceCounterState->RunnableWillRun(
                task->GetPerformanceCounter(), now,
                manager == mIdleTaskManager);

        {
          LogTask::Run log(task);
          result = task->Run();
        }

        // Task itself should keep manager alive.
        if (manager) {
          manager->DidRunTask();
        }

        mPerformanceCounterState->RunnableDidRun(std::move(snapshot));
      }

      // Task itself should keep manager alive.
      if (manager && result && manager->mTaskCount == 0) {
        mTaskManagers.erase(manager);
      }

      task->mInProgress = false;

      if (!result) {
        // Presumably this task was interrupted, leave its dependencies
        // unresolved and reinsert into the queue.
        auto insertion =
            mMainThreadTasks.insert(std::move(mCurrentTasksMT.top()));
        MOZ_ASSERT(insertion.second);
        task->mIterator = insertion.first;
        manager->WillRunTask();
      } else {
        task->mCompleted = true;
#ifdef DEBUG
        task->mIsInGraph = false;
#endif
        // Clear dependencies to release references.
        task->mDependencies.clear();
      }

      mCurrentTasksMT.pop();
      return true;
    }
  }

  mMayHaveMainThreadTask = false;
  if (mIdleTaskManager) {
    // We did not find a task to run.  We still need to clear the cached idle
    // deadline on our idle state, because that deadline was only relevant to
    // the execution of this function.  Had we found a task, we would have
    // cleared the deadline before running that task.
    mIdleTaskManager->State().ClearCachedIdleDeadline();
  }
  return false;
}

Task* TaskController::GetFinalDependency(Task* aTask) {
  Task* nextTask;

  while ((nextTask = aTask->GetHighestPriorityDependency())) {
    aTask = nextTask;
  }

  return aTask;
}

void TaskController::MaybeInterruptTask(Task* aTask) {
  mGraphMutex.AssertCurrentThreadOwns();

  if (!aTask) {
    return;
  }

  // This optimization prevents many slow lookups in long chains of similar
  // priority.
  if (!aTask->mDependencies.empty()) {
    Task* firstDependency = aTask->mDependencies.begin()->get();
    if (aTask->GetPriority() <= firstDependency->GetPriority() &&
        !firstDependency->mCompleted &&
        aTask->IsMainThreadOnly() == firstDependency->IsMainThreadOnly()) {
      // This task has the same or a higher priority as one of its dependencies,
      // never any need to interrupt.
      return;
    }
  }

  Task* finalDependency = GetFinalDependency(aTask);

  if (finalDependency->mInProgress) {
    // No need to wake anything, we can't schedule this task right now anyway.
    return;
  }

  EnsureMainThreadTasksScheduled();

  mMayHaveMainThreadTask = true;

  if (mCurrentTasksMT.empty()) {
    return;
  }

  // We could go through the steps above here and interrupt an off main
  // thread task in case it has a lower priority.
  if (!finalDependency->IsMainThreadOnly()) {
    return;
  }

  if (mCurrentTasksMT.top()->GetPriority() < aTask->GetPriority()) {
    mCurrentTasksMT.top()->RequestInterrupt(aTask->GetPriority());
  }
}

Task* TaskController::GetHighestPriorityMTTask() {
  mGraphMutex.AssertCurrentThreadOwns();

  if (!mMainThreadTasks.empty()) {
    return mMainThreadTasks.begin()->get();
  }
  return nullptr;
}

void TaskController::EnsureMainThreadTasksScheduled() {
  if (mObserver) {
    mObserver->OnDispatchedEvent();
  }
  if (mExternalCondVar) {
    mExternalCondVar->Notify();
  }
  mMainThreadCV.Notify();
}

void TaskController::ProcessUpdatedPriorityModifier(TaskManager* aManager) {
  mGraphMutex.AssertCurrentThreadOwns();

  MOZ_ASSERT(NS_IsMainThread());

  int32_t modifier = aManager->mCurrentPriorityModifier;

  std::vector<RefPtr<Task>> storedTasks;
  // Find all relevant tasks.
  for (auto iter = mMainThreadTasks.begin(); iter != mMainThreadTasks.end();) {
    if ((*iter)->mTaskManager == aManager) {
      storedTasks.push_back(*iter);
      iter = mMainThreadTasks.erase(iter);
    } else {
      iter++;
    }
  }

  // Reinsert found tasks with their new priorities.
  for (RefPtr<Task>& ref : storedTasks) {
    // Kept alive at first by the vector and then by mMainThreadTasks.
    Task* task = ref;
    task->mPriorityModifier = modifier;
    auto insertion = mMainThreadTasks.insert(std::move(ref));
    MOZ_ASSERT(insertion.second);
    task->mIterator = insertion.first;
  }
}

}  // namespace mozilla
