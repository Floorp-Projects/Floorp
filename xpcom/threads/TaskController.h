/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TaskController_h
#define mozilla_TaskController_h

#include "mozilla/CondVar.h"
#include "mozilla/IdlePeriodState.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/EventQueue.h"
#include "nsISupportsImpl.h"
#include "nsIEventTarget.h"

#include <atomic>
#include <memory>
#include <vector>
#include <set>
#include <list>
#include <stack>

class nsIRunnable;
class nsIThreadObserver;

namespace mozilla {

class Task;
class TaskController;
class PerformanceCounter;
class PerformanceCounterState;

const EventQueuePriority kDefaultPriorityValue = EventQueuePriority::Normal;

// This file contains the core classes to access the Gecko scheduler. The
// scheduler forms a graph of prioritize tasks, and is responsible for ensuring
// the execution of tasks or their dependencies in order of inherited priority.
//
// The core class is the 'Task' class. The task class describes a single unit of
// work. Users scheduling work implement this class and are required to
// reimplement the 'Run' function in order to do work.
//
// The TaskManager class is reimplemented by users that require
// the ability to reprioritize or suspend tasks.
//
// The TaskController is responsible for scheduling the work itself. The AddTask
// function is used to schedule work. The ReprioritizeTask function may be used
// to change the priority of a task already in the task graph, without
// unscheduling it.

// The TaskManager is the baseclass used to atomically manage a large set of
// tasks. API users reimplementing TaskManager may reimplement a number of
// functions that they may use to indicate to the scheduler changes in the state
// for any tasks they manage. They may be used to reprioritize or suspend tasks
// under their control, and will also be notified before and after tasks under
// their control are executed. Their methods will only be called once per event
// loop turn, however they may still incur some performance overhead. In
// addition to this frequent reprioritizations may incur a significant
// performance overhead and are discouraged. A TaskManager may currently only be
// used to manage tasks that are bound to the Gecko Main Thread.
class TaskManager {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TaskManager)

  TaskManager() : mTaskCount(0) {}

  // Subclasses implementing task manager will have this function called to
  // determine whether their associated tasks are currently suspended. This
  // will only be called once per iteration of the task queue, this means that
  // suspension of tasks managed by a single TaskManager may be assumed to
  // occur atomically.
  virtual bool IsSuspended(const MutexAutoLock& aProofOfLock) { return false; }

  // Subclasses may implement this in order to supply a priority adjustment
  // to their managed tasks. This is called once per iteration of the task
  // queue, and may be assumed to occur atomically for all managed tasks.
  virtual int32_t GetPriorityModifierForEventLoopTurn(
      const MutexAutoLock& aProofOfLock) {
    return 0;
  }

  void DidQueueTask() { ++mTaskCount; }
  // This is called when a managed task is about to be executed by the
  // scheduler. Anyone reimplementing this should ensure to call the parent or
  // decrement mTaskCount.
  virtual void WillRunTask() { --mTaskCount; }
  // This is called when a managed task has finished being executed by the
  // scheduler.
  virtual void DidRunTask() {}
  uint32_t PendingTaskCount() { return mTaskCount; }

 protected:
  virtual ~TaskManager() {}

 private:
  friend class TaskController;

  enum class IterationType { NOT_EVENT_LOOP_TURN, EVENT_LOOP_TURN };
  bool UpdateCachesForCurrentIterationAndReportPriorityModifierChanged(
      const MutexAutoLock& aProofOfLock, IterationType aIterationType);

  bool mCurrentSuspended = false;
  int32_t mCurrentPriorityModifier = 0;

  std::atomic<uint32_t> mTaskCount;
};

// A Task is the the base class for any unit of work that may be scheduled.
// Subclasses may specify their priority and whether they should be bound to
// the Gecko Main thread. When not bound to the main thread tasks may be
// executed on any available thread (including the main thread), but they may
// also be executed in parallel to any other task they do not have a dependency
// relationship with. Tasks will be run in order of object creation.
class Task {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Task)

  bool IsMainThreadOnly() { return mMainThreadOnly; }

  // This returns the current task priority with its modifier applied.
  uint32_t GetPriority() { return mPriority + mPriorityModifier; }
  uint64_t GetSeqNo() { return mSeqNo; }

  // Callee needs to assume this may be called on any thread.
  // aInterruptPriority passes the priority of the higher priority task that
  // is ready to be executed. The task may safely ignore this function, or
  // interrupt any work being done. It may return 'false' from its run function
  // in order to be run automatically in the future, or true if it will
  // reschedule incomplete work manually.
  virtual void RequestInterrupt(uint32_t aInterruptPriority) {}

  // At the moment this -must- be called before the task is added to the
  // controller. Calling this after tasks have been added to the controller
  // results in undefined behavior!
  // At submission, tasks must depend only on tasks managed by the same, or
  // no idle manager.
  void AddDependency(Task* aTask) {
    MOZ_ASSERT(aTask);
    MOZ_ASSERT(!mIsInGraph);
    mDependencies.insert(aTask);
  }

  // This sets the TaskManager for the current task. Calling this after the
  // task has been added to the TaskController results in undefined behavior.
  void SetManager(TaskManager* aManager) {
    MOZ_ASSERT(mMainThreadOnly);
    MOZ_ASSERT(!mIsInGraph);
    mTaskManager = aManager;
  }
  TaskManager* GetManager() { return mTaskManager; }

  struct PriorityCompare {
    bool operator()(const RefPtr<Task>& aTaskA,
                    const RefPtr<Task>& aTaskB) const {
      uint32_t prioA = aTaskA->GetPriority();
      uint32_t prioB = aTaskB->GetPriority();
      return (prioA > prioB) ||
             (prioA == prioB && (aTaskA->GetSeqNo() < aTaskB->GetSeqNo()));
    }
  };

  // Tell the task about its idle deadline.  Will only be called for
  // tasks managed by an IdleTaskManager, right before the task runs.
  virtual void SetIdleDeadline(TimeStamp aDeadline) {}

  virtual PerformanceCounter* GetPerformanceCounter() const { return nullptr; }

  // Get a name for this task. This returns false if the task has no name.
  virtual bool GetName(nsACString& aName) { return false; }

 protected:
  Task(bool aMainThreadOnly,
       uint32_t aPriority = static_cast<uint32_t>(kDefaultPriorityValue))
      : mMainThreadOnly(aMainThreadOnly),
        mSeqNo(sCurrentTaskSeqNo++),
        mPriority(aPriority) {}

  Task(bool aMainThreadOnly,
       EventQueuePriority aPriority = kDefaultPriorityValue)
      : mMainThreadOnly(aMainThreadOnly),
        mSeqNo(sCurrentTaskSeqNo++),
        mPriority(static_cast<uint32_t>(aPriority)) {}

  virtual ~Task() {}

  friend class TaskController;

  // When this returns false, the task is considered incomplete and will be
  // rescheduled at the current 'mPriority' level.
  virtual bool Run() = 0;

 private:
  Task* GetHighestPriorityDependency();

  // Iterator pointing to this task's position in
  // mThreadableTasks/mMainThreadTasks if, and only if this task is currently
  // scheduled to be executed. This allows fast access to the task's position
  // in the set, allowing for fast removal.
  // This is safe, and remains valid unless the task is removed from the set.
  // See also iterator invalidation in:
  // https://en.cppreference.com/w/cpp/container
  //
  // Or the spec:
  // "All Associative Containers: The insert and emplace members shall not
  // affect the validity of iterators and references to the container
  // [26.2.6/9]" "All Associative Containers: The erase members shall invalidate
  // only iterators and references to the erased elements [26.2.6/9]"
  std::set<RefPtr<Task>, PriorityCompare>::iterator mIterator;
  std::set<RefPtr<Task>, PriorityCompare> mDependencies;

  RefPtr<TaskManager> mTaskManager;

  // Access to these variables is protected by the GraphMutex.
  bool mMainThreadOnly;
  bool mCompleted = false;
  bool mInProgress = false;
#ifdef DEBUG
  bool mIsInGraph = false;
#endif

  static std::atomic<uint64_t> sCurrentTaskSeqNo;
  int64_t mSeqNo;
  uint32_t mPriority;
  // Modifier currently being applied to this task by its taskmanager.
  int32_t mPriorityModifier = 0;
  // Time this task was inserted into the task graph, this is used by the
  // profiler.
  mozilla::TimeStamp mInsertionTime;
};

struct PoolThread {
  PRThread* mThread;
  RefPtr<Task> mCurrentTask;
  // This may be higher than mCurrentTask's priority due to priority
  // propagation. This is -only- valid when mCurrentTask != nullptr.
  uint32_t mEffectiveTaskPriority;
};

// A task manager implementation for priority levels that should only
// run during idle periods.
class IdleTaskManager : public TaskManager {
 public:
  explicit IdleTaskManager(already_AddRefed<nsIIdlePeriod>&& aIdlePeriod)
      : mIdlePeriodState(std::move(aIdlePeriod)) {}

  IdlePeriodState& State() { return mIdlePeriodState; }

  bool IsSuspended(const MutexAutoLock& aProofOfLock) override {
    TimeStamp idleDeadline = State().GetCachedIdleDeadline();
    return !idleDeadline;
  }

 private:
  // Tracking of our idle state of various sorts.
  IdlePeriodState mIdlePeriodState;
};

// The TaskController is the core class of the scheduler. It is used to
// schedule tasks to be executed, as well as to reprioritize tasks that have
// already been scheduled. The core functions to do this are AddTask and
// ReprioritizeTask.
class TaskController {
 public:
  TaskController()
      : mGraphMutex("TaskController::mGraphMutex"),
        mThreadPoolCV(mGraphMutex, "TaskController::mThreadPoolCV"),
        mMainThreadCV(mGraphMutex, "TaskController::mMainThreadCV") {}

  static TaskController* Get();

  static bool Initialize();

  void SetThreadObserver(nsIThreadObserver* aObserver) {
    mObserver = aObserver;
  }
  void SetConditionVariable(CondVar* aExternalCondVar) {
    mExternalCondVar = aExternalCondVar;
  }

  void SetIdleTaskManager(IdleTaskManager* aIdleTaskManager) {
    mIdleTaskManager = aIdleTaskManager;
  }
  IdleTaskManager* GetIdleTaskManager() { return mIdleTaskManager.get(); }

  // Initialization and shutdown code.
  void SetPerformanceCounterState(
      PerformanceCounterState* aPerformanceCounterState);

  static void Shutdown();

  // This adds a task to the TaskController graph.
  // This may be called on any thread.
  void AddTask(already_AddRefed<Task>&& aTask);

  // This wait function is the theoretical function you would need if our main
  // thread needs to also process OS messages or something along those lines.
  void WaitForTaskOrMessage();

  // This gets the next (highest priority) task that is only allowed to execute
  // on the main thread.
  void ExecuteNextTaskOnlyMainThread();

  // Process all pending main thread tasks.
  void ProcessPendingMTTask(bool aMayWait = false);

  // This allows reprioritization of a task already in the task graph.
  // This may be called on any thread.
  void ReprioritizeTask(Task* aTask, uint32_t aPriority);

  void DispatchRunnable(already_AddRefed<nsIRunnable>&& aRunnable,
                        uint32_t aPriority, TaskManager* aManager = nullptr);

  nsIRunnable* GetRunnableForMTTask(bool aReallyWait);

  bool HasMainThreadPendingTasks();

  // Let users know whether the last main thread task runnable did work.
  bool MTTaskRunnableProcessedTask() { return mMTTaskRunnableProcessedTask; }

  static int32_t GetPoolThreadCount();
  static size_t GetThreadStackSize();

 private:
  friend void ThreadFuncPoolThread(void* aIndex);

  bool InitializeInternal();

  void InitializeThreadPool();

  // This gets the next (highest priority) task that is only allowed to execute
  // on the main thread, if any, and executes it.
  // Returns true if it succeeded.
  bool ExecuteNextTaskOnlyMainThreadInternal(const MutexAutoLock& aProofOfLock);

  // The guts of ExecuteNextTaskOnlyMainThreadInternal, which get idle handling
  // wrapped around them.  Returns whether a task actually ran.
  bool DoExecuteNextTaskOnlyMainThreadInternal(
      const MutexAutoLock& aProofOfLock);

  Task* GetFinalDependency(Task* aTask);
  void MaybeInterruptTask(Task* aTask);
  Task* GetHighestPriorityMTTask();

  void EnsureMainThreadTasksScheduled();

  void ProcessUpdatedPriorityModifier(TaskManager* aManager);

  void ShutdownThreadPoolInternal();
  void ShutdownInternal();

  void RunPoolThread();

  static std::unique_ptr<TaskController> sSingleton;
  static StaticMutex sSingletonMutex;

  // This protects access to the task graph.
  Mutex mGraphMutex;

  // This protects thread pool initialization. We cannot do this from within
  // the GraphMutex, since thread creation on Windows can generate events on
  // the main thread that need to be handled.
  Mutex mPoolInitializationMutex =
      Mutex("TaskController::mPoolInitializationMutex");

  CondVar mThreadPoolCV;
  CondVar mMainThreadCV;

  // Variables below are protected by mGraphMutex.

  std::vector<PoolThread> mPoolThreads;
  std::stack<RefPtr<Task>> mCurrentTasksMT;

  // A list of all tasks ordered by priority.
  std::set<RefPtr<Task>, Task::PriorityCompare> mThreadableTasks;
  std::set<RefPtr<Task>, Task::PriorityCompare> mMainThreadTasks;

  // TaskManagers currently active.
  // We can use a raw pointer since tasks always hold on to their TaskManager.
  std::set<TaskManager*> mTaskManagers;

  // This ensures we keep running the main thread if we processed a task there.
  bool mMayHaveMainThreadTask = true;
  bool mShuttingDown = false;

  // This stores whether the last main thread task runnable did work.
  bool mMTTaskRunnableProcessedTask = false;

  // Whether our thread pool is initialized. We use this currently to avoid
  // starting the threads in processes where it's never used. This is protected
  // by mPoolInitializationMutex.
  bool mThreadPoolInitialized = false;

  // Whether we have scheduled a runnable on the main thread event loop.
  // This is used for nsIRunnable compatibility.
  RefPtr<nsIRunnable> mMTProcessingRunnable;
  RefPtr<nsIRunnable> mMTBlockingProcessingRunnable;

  // XXX - Thread observer to notify when a new event has been dispatched
  nsIThreadObserver* mObserver = nullptr;
  // XXX - External condvar to notify when we have received an event
  CondVar* mExternalCondVar = nullptr;
  // Idle task manager so we can properly do idle state stuff.
  RefPtr<IdleTaskManager> mIdleTaskManager;

  // Our tracking of our performance counter and long task state,
  // shared with nsThread.
  PerformanceCounterState* mPerformanceCounterState = nullptr;
};

}  // namespace mozilla

#endif  // mozilla_TaskController_h
