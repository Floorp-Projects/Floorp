/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(TaskDispatcher_h_)
#define TaskDispatcher_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#include <queue>

namespace mozilla {

/*
 * A classic approach to cross-thread communication is to dispatch asynchronous
 * runnables to perform updates on other threads. This generally works well, but
 * there are sometimes reasons why we might want to delay the actual dispatch of
 * these tasks until a specified moment. At present, this is primarily useful to
 * ensure that mirrored state gets updated atomically - but there may be other
 * applications as well.
 *
 * TaskDispatcher is a general abstract class that accepts tasks and dispatches
 * them at some later point. These groups of tasks are per-target-thread, and
 * contain separate queues for several kinds of tasks (see comments  below). - "state change tasks" (which
 * run first, and are intended to be used to update the value held by mirrors),
 * and regular tasks, which are other arbitrary operations that the are gated
 * to run after all the state changes have completed.
 */
class TaskDispatcher
{
public:
  TaskDispatcher() {}
  virtual ~TaskDispatcher() {}

  // Direct tasks are run directly (rather than dispatched asynchronously) when
  // the tail dispatcher fires. A direct task may cause other tasks to be added
  // to the tail dispatcher.
  virtual void AddDirectTask(already_AddRefed<nsIRunnable> aRunnable) = 0;

  // State change tasks are dispatched asynchronously always run before regular
  // tasks. They are intended to be used to update the value held by mirrors
  // before any other dispatched tasks are run on the target thread.
  virtual void AddStateChangeTask(AbstractThread* aThread,
                                  already_AddRefed<nsIRunnable> aRunnable) = 0;

  // Regular tasks are dispatched asynchronously, and run after state change
  // tasks.
  virtual nsresult AddTask(AbstractThread* aThread,
                           already_AddRefed<nsIRunnable> aRunnable) = 0;

  virtual nsresult DispatchTasksFor(AbstractThread* aThread) = 0;
  virtual bool HasTasksFor(AbstractThread* aThread) = 0;
  virtual void DrainDirectTasks() = 0;
};

/*
 * AutoTaskDispatcher is a stack-scoped TaskDispatcher implementation that fires
 * its queued tasks when it is popped off the stack.
 */
class AutoTaskDispatcher : public TaskDispatcher
{
public:
  explicit AutoTaskDispatcher(bool aIsTailDispatcher = false)
    : mIsTailDispatcher(aIsTailDispatcher)
  {}

  ~AutoTaskDispatcher()
  {
    // Given that direct tasks may trigger other code that uses the tail
    // dispatcher, it's better to avoid processing them in the tail dispatcher's
    // destructor. So we require TailDispatchers to manually invoke
    // DrainDirectTasks before the AutoTaskDispatcher gets destroyed. In truth,
    // this is only necessary in the case where this AutoTaskDispatcher can be
    // accessed by the direct tasks it dispatches (true for TailDispatchers, but
    // potentially not true for other hypothetical AutoTaskDispatchers). Feel
    // free to loosen this restriction to apply only to mIsTailDispatcher if a
    // use-case requires it.
    MOZ_ASSERT(!HaveDirectTasks());

    for (size_t i = 0; i < mTaskGroups.Length(); ++i) {
      DispatchTaskGroup(std::move(mTaskGroups[i]));
    }
  }

  bool HaveDirectTasks() const
  {
    return mDirectTasks.isSome() && !mDirectTasks->empty();
  }

  void DrainDirectTasks() override
  {
    while (HaveDirectTasks()) {
      nsCOMPtr<nsIRunnable> r = mDirectTasks->front();
      mDirectTasks->pop();
      r->Run();
    }
  }

  void AddDirectTask(already_AddRefed<nsIRunnable> aRunnable) override
  {
    if (mDirectTasks.isNothing()) {
      mDirectTasks.emplace();
    }
    mDirectTasks->push(std::move(aRunnable));
  }

  void AddStateChangeTask(AbstractThread* aThread,
                          already_AddRefed<nsIRunnable> aRunnable) override
  {
    nsCOMPtr<nsIRunnable> r = aRunnable;
    MOZ_RELEASE_ASSERT(r);
    EnsureTaskGroup(aThread).mStateChangeTasks.AppendElement(r.forget());
  }

  nsresult AddTask(AbstractThread* aThread,
                   already_AddRefed<nsIRunnable> aRunnable) override
  {
    nsCOMPtr<nsIRunnable> r = aRunnable;
    MOZ_RELEASE_ASSERT(r);
    // To preserve the event order, we need to append a new group if the last
    // group is not targeted for |aThread|.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1318226&mark=0-3#c0
    // for the details of the issue.
    if (mTaskGroups.Length() == 0 || mTaskGroups.LastElement()->mThread != aThread) {
      mTaskGroups.AppendElement(new PerThreadTaskGroup(aThread));
    }

    PerThreadTaskGroup& group = *mTaskGroups.LastElement();
    group.mRegularTasks.AppendElement(r.forget());

    return NS_OK;
  }

  bool HasTasksFor(AbstractThread* aThread) override
  {
    return !!GetTaskGroup(aThread) ||
           (aThread == AbstractThread::GetCurrent() && HaveDirectTasks());
  }

  nsresult DispatchTasksFor(AbstractThread* aThread) override
  {
    nsresult rv = NS_OK;

    // Dispatch all groups that match |aThread|.
    for (size_t i = 0; i < mTaskGroups.Length(); ++i) {
      if (mTaskGroups[i]->mThread == aThread) {
        nsresult rv2 = DispatchTaskGroup(std::move(mTaskGroups[i]));

        if (NS_WARN_IF(NS_FAILED(rv2)) && NS_SUCCEEDED(rv)) {
          // We should try our best to call DispatchTaskGroup() as much as
          // possible and return an error if any of DispatchTaskGroup() calls
          // failed.
          rv = rv2;
        }

        mTaskGroups.RemoveElementAt(i--);
      }
    }

    return rv;
  }

private:

  struct PerThreadTaskGroup
  {
  public:
    explicit PerThreadTaskGroup(AbstractThread* aThread)
      : mThread(aThread)
    {
      MOZ_COUNT_CTOR(PerThreadTaskGroup);
    }

    ~PerThreadTaskGroup() { MOZ_COUNT_DTOR(PerThreadTaskGroup); }

    RefPtr<AbstractThread> mThread;
    nsTArray<nsCOMPtr<nsIRunnable>> mStateChangeTasks;
    nsTArray<nsCOMPtr<nsIRunnable>> mRegularTasks;
  };

  class TaskGroupRunnable : public Runnable
  {
    public:
      explicit TaskGroupRunnable(UniquePtr<PerThreadTaskGroup>&& aTasks)
        : Runnable("AutoTaskDispatcher::TaskGroupRunnable")
        , mTasks(std::move(aTasks))
      {
      }

      NS_IMETHOD Run() override
      {
        // State change tasks get run all together before any code is run, so
        // that all state changes are made in an atomic unit.
        for (size_t i = 0; i < mTasks->mStateChangeTasks.Length(); ++i) {
          mTasks->mStateChangeTasks[i]->Run();
        }

        // Once the state changes have completed, drain any direct tasks
        // generated by those state changes (i.e. watcher notification tasks).
        // This needs to be outside the loop because we don't want to run code
        // that might observe intermediate states.
        MaybeDrainDirectTasks();

        for (size_t i = 0; i < mTasks->mRegularTasks.Length(); ++i) {
          mTasks->mRegularTasks[i]->Run();

          // Scope direct tasks tightly to the task that generated them.
          MaybeDrainDirectTasks();
        }

        return NS_OK;
      }

    private:
      void MaybeDrainDirectTasks()
      {
        AbstractThread* currentThread = AbstractThread::GetCurrent();
        if (currentThread) {
          currentThread->TailDispatcher().DrainDirectTasks();
        }
      }

      UniquePtr<PerThreadTaskGroup> mTasks;
  };

  PerThreadTaskGroup& EnsureTaskGroup(AbstractThread* aThread)
  {
    PerThreadTaskGroup* existing = GetTaskGroup(aThread);
    if (existing) {
      return *existing;
    }

    mTaskGroups.AppendElement(new PerThreadTaskGroup(aThread));
    return *mTaskGroups.LastElement();
  }

  PerThreadTaskGroup* GetTaskGroup(AbstractThread* aThread)
  {
    for (size_t i = 0; i < mTaskGroups.Length(); ++i) {
      if (mTaskGroups[i]->mThread == aThread) {
        return mTaskGroups[i].get();
      }
    }

    // Not found.
    return nullptr;
  }

  nsresult DispatchTaskGroup(UniquePtr<PerThreadTaskGroup> aGroup)
  {
    RefPtr<AbstractThread> thread = aGroup->mThread;

    AbstractThread::DispatchReason reason = mIsTailDispatcher ? AbstractThread::TailDispatch
                                                              : AbstractThread::NormalDispatch;
    nsCOMPtr<nsIRunnable> r = new TaskGroupRunnable(std::move(aGroup));
    return thread->Dispatch(r.forget(), reason);
  }

  // Direct tasks. We use a Maybe<> because (a) this class is hot, (b)
  // mDirectTasks often doesn't get anything put into it, and (c) the
  // std::queue implementation in GNU libstdc++ does two largish heap
  // allocations when creating a new std::queue.
  mozilla::Maybe<std::queue<nsCOMPtr<nsIRunnable>>> mDirectTasks;

  // Task groups, organized by thread.
  nsTArray<UniquePtr<PerThreadTaskGroup>> mTaskGroups;

  // True if this TaskDispatcher represents the tail dispatcher for the thread
  // upon which it runs.
  const bool mIsTailDispatcher;
};

// Little utility class to allow declaring AutoTaskDispatcher as a default
// parameter for methods that take a TaskDispatcher&.
template<typename T>
class PassByRef
{
public:
  PassByRef() {}
  operator T&() { return mVal; }
private:
  T mVal;
};

} // namespace mozilla

#endif
