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
#include "GeckoProfiler.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/EventQueue.h"
#include "mozilla/InputTaskManager.h"
#include "mozilla/VsyncTaskManager.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "nsIThreadInternal.h"
#include "nsThread.h"
#include "prenv.h"
#include "prsystem.h"

namespace mozilla {

StaticAutoPtr<TaskController> TaskController::sSingleton;

thread_local size_t mThreadPoolIndex = -1;
std::atomic<uint64_t> Task::sCurrentTaskSeqNo = 0;

const int32_t kMinimumPoolThreadCount = 2;
const int32_t kMaximumPoolThreadCount = 8;

/* static */
int32_t TaskController::GetPoolThreadCount() {
  if (PR_GetEnv("MOZ_TASKCONTROLLER_THREADCOUNT")) {
    return strtol(PR_GetEnv("MOZ_TASKCONTROLLER_THREADCOUNT"), nullptr, 0);
  }

  int32_t numCores = std::max<int32_t>(1, PR_GetNumberOfProcessors());

  return std::clamp<int32_t>(numCores, kMinimumPoolThreadCount,
                             kMaximumPoolThreadCount);
}

#if defined(MOZ_COLLECTING_RUNNABLE_TELEMETRY)

struct TaskMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("Task");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const nsCString& aName, uint32_t aPriority) {
    aWriter.StringProperty("name", aName);
    aWriter.IntProperty("priority", aPriority);

#  define EVENT_PRIORITY(NAME, VALUE)                \
    if (aPriority == (VALUE)) {                      \
      aWriter.StringProperty("priorityName", #NAME); \
    } else
    EVENT_QUEUE_PRIORITY_LIST(EVENT_PRIORITY)
#  undef EVENT_PRIORITY
    {
      aWriter.StringProperty("priorityName", "Invalid Value");
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.SetChartLabel("{marker.data.name}");
    schema.SetTableLabel(
        "{marker.name} - {marker.data.name} - priority: "
        "{marker.data.priorityName} ({marker.data.priority})");
    schema.AddKeyLabelFormatSearchable("name", "Task Name", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormat("priorityName", "Priority Name",
                             MS::Format::String);
    schema.AddKeyLabelFormat("priority", "Priority level", MS::Format::Integer);
    return schema;
  }
};

class MOZ_RAII AutoProfileTask {
 public:
  explicit AutoProfileTask(nsACString& aName, uint64_t aPriority)
      : mName(aName), mPriority(aPriority) {
    if (profiler_is_active()) {
      mStartTime = TimeStamp::Now();
    }
  }

  ~AutoProfileTask() {
    if (!profiler_thread_is_being_profiled_for_markers()) {
      return;
    }

    AUTO_PROFILER_LABEL("AutoProfileTask", PROFILER);
    AUTO_PROFILER_STATS(AUTO_PROFILE_TASK);
    profiler_add_marker("Runnable", ::mozilla::baseprofiler::category::OTHER,
                        mStartTime.IsNull()
                            ? MarkerTiming::IntervalEnd()
                            : MarkerTiming::IntervalUntilNowFrom(mStartTime),
                        TaskMarker{}, mName, mPriority);
  }

 private:
  TimeStamp mStartTime;
  nsAutoCString mName;
  uint32_t mPriority;
};

#  define AUTO_PROFILE_FOLLOWING_TASK(task)                                  \
    nsAutoCString name;                                                      \
    (task)->GetName(name);                                                   \
    AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_NONSENSITIVE("Task", OTHER, name); \
    mozilla::AutoProfileTask PROFILER_RAII(name, (task)->GetPriority());
#else
#  define AUTO_PROFILE_FOLLOWING_TASK(task)
#endif

bool TaskManager::
    UpdateCachesForCurrentIterationAndReportPriorityModifierChanged(
        const MutexAutoLock& aProofOfLock, IterationType aIterationType) {
  mCurrentSuspended = IsSuspended(aProofOfLock);

  if (aIterationType == IterationType::EVENT_LOOP_TURN && !mCurrentSuspended) {
    int32_t oldModifier = mCurrentPriorityModifier;
    mCurrentPriorityModifier =
        GetPriorityModifierForEventLoopTurn(aProofOfLock);

    if (mCurrentPriorityModifier != oldModifier) {
      return true;
    }
  }
  return false;
}

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
class MOZ_RAII AutoSetMainThreadRunnableName {
 public:
  explicit AutoSetMainThreadRunnableName(const nsCString& aName) {
    MOZ_ASSERT(NS_IsMainThread());
    // We want to record our current runnable's name in a static so
    // that BHR can record it.
    mRestoreRunnableName = nsThread::sMainThreadRunnableName;

    // Copy the name into sMainThreadRunnableName's buffer, and append a
    // terminating null.
    uint32_t length = std::min((uint32_t)nsThread::kRunnableNameBufSize - 1,
                               (uint32_t)aName.Length());
    memcpy(nsThread::sMainThreadRunnableName.begin(), aName.BeginReading(),
           length);
    nsThread::sMainThreadRunnableName[length] = '\0';
  }

  ~AutoSetMainThreadRunnableName() {
    nsThread::sMainThreadRunnableName = mRestoreRunnableName;
  }

 private:
  Array<char, nsThread::kRunnableNameBufSize> mRestoreRunnableName;
};
#endif

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

void TaskController::Initialize() {
  MOZ_ASSERT(!sSingleton);
  sSingleton = new TaskController();
}

void ThreadFuncPoolThread(void* aIndex) {
  mThreadPoolIndex = *reinterpret_cast<int32_t*>(aIndex);
  delete reinterpret_cast<int32_t*>(aIndex);
  TaskController::Get()->RunPoolThread();
}

TaskController::TaskController()
    : mGraphMutex("TaskController::mGraphMutex"),
      mThreadPoolCV(mGraphMutex, "TaskController::mThreadPoolCV"),
      mMainThreadCV(mGraphMutex, "TaskController::mMainThreadCV"),
      mRunOutOfMTTasksCounter(0) {
  InputTaskManager::Init();
  VsyncTaskManager::Init();
  mMTProcessingRunnable = NS_NewRunnableFunction(
      "TaskController::ExecutePendingMTTasks()",
      []() { TaskController::Get()->ProcessPendingMTTask(); });
  mMTBlockingProcessingRunnable = NS_NewRunnableFunction(
      "TaskController::ExecutePendingMTTasks()",
      []() { TaskController::Get()->ProcessPendingMTTask(true); });
}

// We want our default stack size limit to be approximately 2MB, to be safe for
// JS helper tasks that can use a lot of stack, but expect most threads to use
// much less. On Linux, however, requesting a stack of 2MB or larger risks the
// kernel allocating an entire 2MB huge page for it on first access, which we do
// not want. To avoid this possibility, we subtract 2 standard VM page sizes
// from our default.
constexpr PRUint32 sBaseStackSize = 2048 * 1024 - 2 * 4096;

// TSan enforces a minimum stack size that's just slightly larger than our
// default helper stack size.  It does this to store blobs of TSan-specific data
// on each thread's stack.  Unfortunately, that means that even though we'll
// actually receive a larger stack than we requested, the effective usable space
// of that stack is significantly less than what we expect.  To offset TSan
// stealing our stack space from underneath us, double the default.
//
// Similarly, ASan requires more stack space due to red-zones.
#if defined(MOZ_TSAN) || defined(MOZ_ASAN)
constexpr PRUint32 sStackSize = 2 * sBaseStackSize;
#else
constexpr PRUint32 sStackSize = sBaseStackSize;
#endif

void TaskController::InitializeThreadPool() {
  mPoolInitializationMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mThreadPoolInitialized);
  mThreadPoolInitialized = true;

  int32_t poolSize = GetPoolThreadCount();
  for (int32_t i = 0; i < poolSize; i++) {
    int32_t* index = new int32_t(i);
    mPoolThreads.push_back(
        {PR_CreateThread(PR_USER_THREAD, ThreadFuncPoolThread, index,
                         PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                         PR_JOINABLE_THREAD, sStackSize),
         nullptr});
  }
}

/* static */
size_t TaskController::GetThreadStackSize() { return sStackSize; }

void TaskController::SetPerformanceCounterState(
    PerformanceCounterState* aPerformanceCounterState) {
  mPerformanceCounterState = aPerformanceCounterState;
}

/* static */
void TaskController::Shutdown() {
  InputTaskManager::Cleanup();
  VsyncTaskManager::Cleanup();
  if (sSingleton) {
    sSingleton->ShutdownThreadPoolInternal();
    sSingleton = nullptr;
  }
  MOZ_ASSERT(!sSingleton);
}

void TaskController::ShutdownThreadPoolInternal() {
  {
    // Prevent race condition on mShuttingDown and wait.
    MutexAutoLock lock(mGraphMutex);
    mShuttingDown = true;
    mThreadPoolCV.NotifyAll();
  }
  for (PoolThread& thread : mPoolThreads) {
    PR_JoinThread(thread.mThread);
  }
}

void TaskController::RunPoolThread() {
  IOInterposer::RegisterCurrentThread();

  // This is used to hold on to a task to make sure it is released outside the
  // lock. This is required since it's perfectly feasible for task destructors
  // to post events themselves.
  RefPtr<Task> lastTask;

  nsAutoCString threadName;
  threadName.AppendLiteral("TaskController #");
  threadName.AppendInt(static_cast<int64_t>(mThreadPoolIndex));
  AUTO_PROFILER_REGISTER_THREAD(threadName.BeginReading());

  MutexAutoLock lock(mGraphMutex);
  while (true) {
    bool ranTask = false;

    if (!mThreadableTasks.empty()) {
      for (auto iter = mThreadableTasks.begin(); iter != mThreadableTasks.end();
           ++iter) {
        // Search for the highest priority dependency of the highest priority
        // task.

        // We work with rawptrs to avoid needless refcounting. All our tasks
        // are always kept alive by the graph. If one is removed from the graph
        // it is kept alive by mPoolThreads[mThreadPoolIndex].mCurrentTask.
        Task* task = iter->get();

        MOZ_ASSERT(!task->mTaskManager);

        mPoolThreads[mThreadPoolIndex].mEffectiveTaskPriority =
            task->GetPriority();

        Task* nextTask;
        while ((nextTask = task->GetHighestPriorityDependency())) {
          task = nextTask;
        }

        if (task->GetKind() == Task::Kind::MainThreadOnly ||
            task->mInProgress) {
          continue;
        }

        mPoolThreads[mThreadPoolIndex].mCurrentTask = task;
        mThreadableTasks.erase(task->mIterator);
        task->mIterator = mThreadableTasks.end();
        task->mInProgress = true;

        if (!mThreadableTasks.empty()) {
          // Ensure at least one additional thread is woken up if there are
          // more threadable tasks to process. Notifying all threads at once
          // isn't actually better for performance since they all need the
          // GraphMutex to proceed anyway.
          mThreadPoolCV.Notify();
        }

        bool taskCompleted = false;
        {
          MutexAutoUnlock unlock(mGraphMutex);
          lastTask = nullptr;
          AUTO_PROFILE_FOLLOWING_TASK(task);
          taskCompleted = task->Run() == Task::TaskResult::Complete;
          ranTask = true;
        }

        task->mInProgress = false;

        if (!taskCompleted) {
          // Presumably this task was interrupted, leave its dependencies
          // unresolved and reinsert into the queue.
          auto insertion = mThreadableTasks.insert(
              mPoolThreads[mThreadPoolIndex].mCurrentTask);
          MOZ_ASSERT(insertion.second);
          task->mIterator = insertion.first;
        } else {
          task->mCompleted = true;
#ifdef DEBUG
          task->mIsInGraph = false;
#endif
          task->mDependencies.clear();
          // This may have unblocked a main thread task. We could do this only
          // if there was a main thread task before this one in the dependency
          // chain.
          mMayHaveMainThreadTask = true;
          // Since this could have multiple dependencies thare are restricted
          // to the main thread. Let's make sure that's awake.
          EnsureMainThreadTasksScheduled();

          MaybeInterruptTask(GetHighestPriorityMTTask());
        }

        // Store last task for release next time we release the lock or enter
        // wait state.
        lastTask = mPoolThreads[mThreadPoolIndex].mCurrentTask.forget();
        break;
      }
    }

    // Ensure the last task is released before we enter the wait state.
    if (lastTask) {
      MutexAutoUnlock unlock(mGraphMutex);
      lastTask = nullptr;

      // Run another loop iteration, while we were unlocked there was an
      // opportunity for another task to be posted or shutdown to be initiated.
      continue;
    }

    if (!ranTask) {
      if (mShuttingDown) {
        IOInterposer::UnregisterCurrentThread();
        MOZ_ASSERT(mThreadableTasks.empty());
        return;
      }

      AUTO_PROFILER_LABEL("TaskController::RunPoolThread", IDLE);
      mThreadPoolCV.Wait();
    }
  }
}

void TaskController::AddTask(already_AddRefed<Task>&& aTask) {
  RefPtr<Task> task(aTask);

  if (task->GetKind() == Task::Kind::OffMainThreadOnly) {
    MutexAutoLock lock(mPoolInitializationMutex);
    if (!mThreadPoolInitialized) {
      InitializeThreadPool();
    }
  }

  MutexAutoLock lock(mGraphMutex);

  if (TaskManager* manager = task->GetManager()) {
    if (manager->mTaskCount == 0) {
      mTaskManagers.insert(manager);
    }
    manager->DidQueueTask();

    // Set this here since if this manager's priority modifier doesn't change
    // we will not reprioritize when iterating over the queue.
    task->mPriorityModifier = manager->mCurrentPriorityModifier;
  }

  if (profiler_is_active_and_unpaused()) {
    task->mInsertionTime = TimeStamp::Now();
  }

#ifdef DEBUG
  task->mIsInGraph = true;

  for (const RefPtr<Task>& otherTask : task->mDependencies) {
    MOZ_ASSERT(!otherTask->mTaskManager ||
               otherTask->mTaskManager == task->mTaskManager);
  }
#endif

  LogTask::LogDispatch(task);

  std::pair<std::set<RefPtr<Task>, Task::PriorityCompare>::iterator, bool>
      insertion;
  switch (task->GetKind()) {
    case Task::Kind::MainThreadOnly:
      insertion = mMainThreadTasks.insert(std::move(task));
      break;
    case Task::Kind::OffMainThreadOnly:
      insertion = mThreadableTasks.insert(std::move(task));
      break;
  }
  (*insertion.first)->mIterator = insertion.first;
  MOZ_ASSERT(insertion.second);

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

#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
    // Unlock before calling into the BackgroundHangMonitor API as it uses
    // the timer API.
    {
      MutexAutoUnlock unlock(mGraphMutex);
      BackgroundHangMonitor().NotifyWait();
    }
#endif

    {
      // ProcessNextEvent will also have attempted to wait, however we may have
      // given it a Runnable when all the tasks in our task graph were suspended
      // but we weren't able to cheaply determine that.
      AUTO_PROFILER_LABEL("TaskController::ProcessPendingMTTask", IDLE);
      mMainThreadCV.Wait();
    }

#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
    {
      MutexAutoUnlock unlock(mGraphMutex);
      BackgroundHangMonitor().NotifyActivity();
    }
#endif
  }

  if (mMayHaveMainThreadTask) {
    EnsureMainThreadTasksScheduled();
  }
}

void TaskController::ReprioritizeTask(Task* aTask, uint32_t aPriority) {
  MutexAutoLock lock(mGraphMutex);
  std::set<RefPtr<Task>, Task::PriorityCompare>* queue = &mMainThreadTasks;
  if (aTask->GetKind() == Task::Kind::OffMainThreadOnly) {
    queue = &mThreadableTasks;
  }

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
               Kind aKind)
      : Task(aKind, aPriority), mRunnable(aRunnable) {}

  virtual TaskResult Run() override {
    mRunnable->Run();
    mRunnable = nullptr;
    return TaskResult::Complete;
  }

  void SetIdleDeadline(TimeStamp aDeadline) override {
    nsCOMPtr<nsIIdleRunnable> idleRunnable = do_QueryInterface(mRunnable);
    if (idleRunnable) {
      idleRunnable->SetDeadline(aDeadline);
    }
  }

  virtual bool GetName(nsACString& aName) override {
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    if (nsCOMPtr<nsINamed> named = do_QueryInterface(mRunnable)) {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(named->GetName(aName)));
    } else {
      aName.AssignLiteral("non-nsINamed runnable");
    }
    if (aName.IsEmpty()) {
      aName.AssignLiteral("anonymous runnable");
    }
    return true;
#else
    return false;
#endif
  }

 private:
  RefPtr<nsIRunnable> mRunnable;
};

void TaskController::DispatchRunnable(already_AddRefed<nsIRunnable>&& aRunnable,
                                      uint32_t aPriority,
                                      TaskManager* aManager) {
  RefPtr<RunnableTask> task = new RunnableTask(std::move(aRunnable), aPriority,
                                               Task::Kind::MainThreadOnly);

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
  MOZ_ASSERT(NS_IsMainThread());
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

    // This would break down if we have a non-suspended task depending on a
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

uint64_t TaskController::PendingMainthreadTaskCountIncludingSuspended() {
  MutexAutoLock lock(mGraphMutex);
  return mMainThreadTasks.size();
}

bool TaskController::ExecuteNextTaskOnlyMainThreadInternal(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(NS_IsMainThread());
  mGraphMutex.AssertCurrentThreadOwns();
  // Block to make it easier to jump to our cleanup.
  bool taskRan = false;
  do {
    taskRan = DoExecuteNextTaskOnlyMainThreadInternal(aProofOfLock);
    if (taskRan) {
      if (mIdleTaskManager && mIdleTaskManager->mTaskCount &&
          mIdleTaskManager->IsSuspended(aProofOfLock)) {
        uint32_t activeTasks = mMainThreadTasks.size();
        for (TaskManager* manager : mTaskManagers) {
          if (manager->IsSuspended(aProofOfLock)) {
            activeTasks -= manager->mTaskCount;
          } else {
            break;
          }
        }

        if (!activeTasks) {
          // We have only idle (and maybe other suspended) tasks left, so need
          // to update the idle state. We need to temporarily release the lock
          // while we do that.
          MutexAutoUnlock unlock(mGraphMutex);
          mIdleTaskManager->State().RequestIdleDeadlineIfNeeded(unlock);
        }
      }
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
      ++mRunOutOfMTTasksCounter;

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
  mGraphMutex.AssertCurrentThreadOwns();

  nsCOMPtr<nsIThread> mainIThread;
  NS_GetMainThread(getter_AddRefs(mainIThread));

  nsThread* mainThread = static_cast<nsThread*>(mainIThread.get());
  if (mainThread) {
    mainThread->SetRunningEventDelay(TimeDuration(), TimeStamp());
  }

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

      if (task->GetKind() == Task::Kind::OffMainThreadOnly ||
          task->mInProgress ||
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

        if (mainThread) {
          if (task->GetPriority() < uint32_t(EventQueuePriority::InputHigh) ||
              task->mInsertionTime.IsNull()) {
            mainThread->SetRunningEventDelay(TimeDuration(), now);
          } else {
            mainThread->SetRunningEventDelay(now - task->mInsertionTime, now);
          }
        }

        nsAutoCString name;
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
        task->GetName(name);
#endif

        PerformanceCounterState::Snapshot snapshot =
            mPerformanceCounterState->RunnableWillRun(
                now, manager == mIdleTaskManager);

        {
          LogTask::Run log(task);
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
          AutoSetMainThreadRunnableName nameGuard(name);
#endif
          AUTO_PROFILE_FOLLOWING_TASK(task);
          result = task->Run() == Task::TaskResult::Complete;
        }

        // Task itself should keep manager alive.
        if (manager) {
          manager->DidRunTask();
        }

        mPerformanceCounterState->RunnableDidRun(name, std::move(snapshot));
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

        if (!mThreadableTasks.empty()) {
          // We're going to wake up a single thread in our pool. This thread
          // is responsible for waking up additional threads in the situation
          // where more than one task became available.
          mThreadPoolCV.Notify();
        }
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
        aTask->GetKind() == firstDependency->GetKind()) {
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

  if (aTask->GetKind() == Task::Kind::MainThreadOnly) {
    mMayHaveMainThreadTask = true;

    EnsureMainThreadTasksScheduled();

    if (mCurrentTasksMT.empty()) {
      return;
    }

    // We could go through the steps above here and interrupt an off main
    // thread task in case it has a lower priority.
    if (finalDependency->GetKind() == Task::Kind::OffMainThreadOnly) {
      return;
    }

    if (mCurrentTasksMT.top()->GetPriority() < aTask->GetPriority()) {
      mCurrentTasksMT.top()->RequestInterrupt(aTask->GetPriority());
    }
  } else {
    Task* lowestPriorityTask = nullptr;
    for (PoolThread& thread : mPoolThreads) {
      if (!thread.mCurrentTask) {
        mThreadPoolCV.Notify();
        // There's a free thread, no need to interrupt anything.
        return;
      }

      if (!lowestPriorityTask) {
        lowestPriorityTask = thread.mCurrentTask.get();
        continue;
      }

      // This should possibly select the lowest priority task which was started
      // the latest. But for now we ignore that optimization.
      // This also doesn't guarantee a task is interruptable, so that's an
      // avenue for improvements as well.
      if (lowestPriorityTask->GetPriority() > thread.mEffectiveTaskPriority) {
        lowestPriorityTask = thread.mCurrentTask.get();
      }
    }

    if (lowestPriorityTask->GetPriority() < aTask->GetPriority()) {
      lowestPriorityTask->RequestInterrupt(aTask->GetPriority());
    }

    // We choose not to interrupt main thread tasks for tasks which may be
    // executed off the main thread.
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
