/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AbstractThread.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"  // We initialize the MozPromise logging in this file.
#include "mozilla/StaticPtr.h"
#include "mozilla/StateWatching.h"  // We initialize the StateWatching logging in this file.
#include "mozilla/TaskQueue.h"
#include "mozilla/TaskDispatcher.h"
#include "mozilla/Unused.h"

#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

LazyLogModule gMozPromiseLog("MozPromise");
LazyLogModule gStateWatchingLog("StateWatching");

StaticRefPtr<AbstractThread> sMainThread;
MOZ_THREAD_LOCAL(AbstractThread*) AbstractThread::sCurrentThreadTLS;

class EventTargetWrapper : public AbstractThread {
 public:
  explicit EventTargetWrapper(nsIEventTarget* aTarget,
                              bool aRequireTailDispatch)
      : AbstractThread(aRequireTailDispatch), mTarget(aTarget) {
    // Our current mechanism of implementing tail dispatch is appshell-specific.
    // This is because a very similar mechanism already exists on the main
    // thread, and we want to avoid making event dispatch on the main thread
    // more complicated than it already is.
    //
    // If you need to use tail dispatch on other XPCOM threads, you'll need to
    // implement an nsIThreadObserver to fire the tail dispatcher at the
    // appropriate times. You will also need to modify this assertion.
    MOZ_ASSERT_IF(aRequireTailDispatch,
                  NS_IsMainThread() && aTarget->IsOnCurrentThread());
  }

  nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                    DispatchReason aReason = NormalDispatch) override {
    AbstractThread* currentThread;
    if (aReason != TailDispatch && (currentThread = GetCurrent()) &&
        RequiresTailDispatch(currentThread)) {
      return currentThread->TailDispatcher().AddTask(this,
                                                     std::move(aRunnable));
    }

    RefPtr<nsIRunnable> runner = new Runner(this, std::move(aRunnable));
    return mTarget->Dispatch(runner.forget(), NS_DISPATCH_NORMAL);
  }

  // Prevent a GCC warning about the other overload of Dispatch being hidden.
  using AbstractThread::Dispatch;

  bool IsCurrentThreadIn() const override {
    return mTarget->IsOnCurrentThread();
  }

  void FireTailDispatcher() {
    AutoEnter context(this);

    MOZ_DIAGNOSTIC_ASSERT(mTailDispatcher.isSome());
    mTailDispatcher.ref().DrainDirectTasks();
    mTailDispatcher.reset();
  }

  TaskDispatcher& TailDispatcher() override {
    MOZ_ASSERT(IsCurrentThreadIn());
    if (!mTailDispatcher.isSome()) {
      mTailDispatcher.emplace(/* aIsTailDispatcher = */ true);

      nsCOMPtr<nsIRunnable> event =
          NewRunnableMethod("EventTargetWrapper::FireTailDispatcher", this,
                            &EventTargetWrapper::FireTailDispatcher);
      nsContentUtils::RunInStableState(event.forget());
    }

    return mTailDispatcher.ref();
  }

  bool MightHaveTailTasks() override { return mTailDispatcher.isSome(); }

  nsIEventTarget* AsEventTarget() override { return mTarget; }

 private:
  const RefPtr<nsIEventTarget> mTarget;
  Maybe<AutoTaskDispatcher> mTailDispatcher;

  class Runner : public CancelableRunnable {
   public:
    explicit Runner(EventTargetWrapper* aThread,
                    already_AddRefed<nsIRunnable> aRunnable)
        : CancelableRunnable("EventTargetWrapper::Runner"),
          mThread(aThread),
          mRunnable(aRunnable) {}

    NS_IMETHOD Run() override {
      AutoEnter taskGuard(mThread);

      MOZ_ASSERT(mThread == AbstractThread::GetCurrent());
      MOZ_ASSERT(mThread->IsCurrentThreadIn());
      return mRunnable->Run();
    }

    nsresult Cancel() override {
      // Set the TLS during Cancel() just in case it calls Run().
      AutoEnter taskGuard(mThread);

      nsresult rv = NS_OK;

      // Try to cancel the runnable if it implements the right interface.
      // Otherwise just skip the runnable.
      nsCOMPtr<nsICancelableRunnable> cr = do_QueryInterface(mRunnable);
      if (cr) {
        rv = cr->Cancel();
      }

      return rv;
    }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    NS_IMETHOD GetName(nsACString& aName) override {
      aName.AssignLiteral("AbstractThread::Runner");
      if (nsCOMPtr<nsINamed> named = do_QueryInterface(mRunnable)) {
        nsAutoCString name;
        named->GetName(name);
        if (!name.IsEmpty()) {
          aName.AppendLiteral(" for ");
          aName.Append(name);
        }
      }
      return NS_OK;
    }
#endif

   private:
    const RefPtr<EventTargetWrapper> mThread;
    const RefPtr<nsIRunnable> mRunnable;
  };
};

NS_IMPL_ISUPPORTS(AbstractThread, nsIEventTarget, nsISerialEventTarget)

NS_IMETHODIMP_(bool)
AbstractThread::IsOnCurrentThreadInfallible() { return IsCurrentThreadIn(); }

NS_IMETHODIMP
AbstractThread::IsOnCurrentThread(bool* aResult) {
  *aResult = IsCurrentThreadIn();
  return NS_OK;
}

NS_IMETHODIMP
AbstractThread::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
AbstractThread::Dispatch(already_AddRefed<nsIRunnable> aEvent,
                         uint32_t aFlags) {
  return Dispatch(std::move(aEvent), NormalDispatch);
}

NS_IMETHODIMP
AbstractThread::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                uint32_t aDelayMs) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult AbstractThread::TailDispatchTasksFor(AbstractThread* aThread) {
  if (MightHaveTailTasks()) {
    return TailDispatcher().DispatchTasksFor(aThread);
  }

  return NS_OK;
}

bool AbstractThread::HasTailTasksFor(AbstractThread* aThread) {
  if (!MightHaveTailTasks()) {
    return false;
  }
  return TailDispatcher().HasTasksFor(aThread);
}

bool AbstractThread::RequiresTailDispatch(AbstractThread* aThread) const {
  MOZ_ASSERT(aThread);
  // We require tail dispatch if both the source and destination
  // threads support it.
  return SupportsTailDispatch() && aThread->SupportsTailDispatch();
}

bool AbstractThread::RequiresTailDispatchFromCurrentThread() const {
  AbstractThread* current = GetCurrent();
  return current && RequiresTailDispatch(current);
}

AbstractThread* AbstractThread::MainThread() {
  MOZ_ASSERT(sMainThread);
  return sMainThread;
}

void AbstractThread::InitTLS() {
  if (!sCurrentThreadTLS.init()) {
    MOZ_CRASH();
  }
}

void AbstractThread::InitMainThread() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sMainThread);
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));
  MOZ_DIAGNOSTIC_ASSERT(mainThread);
  sMainThread = new EventTargetWrapper(mainThread.get(),
                                       /* aRequireTailDispatch = */ true);
  ClearOnShutdown(&sMainThread);

  if (!sCurrentThreadTLS.init()) {
    MOZ_CRASH();
  }
}

void AbstractThread::DispatchStateChange(
    already_AddRefed<nsIRunnable> aRunnable) {
  GetCurrent()->TailDispatcher().AddStateChangeTask(this, std::move(aRunnable));
}

/* static */
void AbstractThread::DispatchDirectTask(
    already_AddRefed<nsIRunnable> aRunnable) {
  GetCurrent()->TailDispatcher().AddDirectTask(std::move(aRunnable));
}

/* static */
already_AddRefed<AbstractThread> AbstractThread::CreateXPCOMThreadWrapper(
    nsIThread* aThread, bool aRequireTailDispatch) {
  RefPtr<EventTargetWrapper> wrapper =
      new EventTargetWrapper(aThread, aRequireTailDispatch);

  bool onCurrentThread = false;
  Unused << aThread->IsOnCurrentThread(&onCurrentThread);

  if (onCurrentThread) {
    sCurrentThreadTLS.set(wrapper);
    return wrapper.forget();
  }

  // Set the thread-local sCurrentThreadTLS to point to the wrapper on the
  // target thread. This ensures that sCurrentThreadTLS is as expected by
  // AbstractThread::GetCurrent() on the target thread.
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("AbstractThread::CreateXPCOMThreadWrapper",
                             [wrapper]() { sCurrentThreadTLS.set(wrapper); });
  aThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  return wrapper.forget();
}

/* static  */
already_AddRefed<AbstractThread> AbstractThread::CreateEventTargetWrapper(
    nsIEventTarget* aEventTarget, bool aRequireTailDispatch) {
  MOZ_ASSERT(aEventTarget);
  nsCOMPtr<nsIThread> thread(do_QueryInterface(aEventTarget));
  Unused << thread;  // simpler than DebugOnly<nsCOMPtr<nsIThread>>
  MOZ_ASSERT(!thread,
             "nsIThread should be wrapped by CreateXPCOMThreadWrapper!");

  RefPtr<EventTargetWrapper> wrapper =
      new EventTargetWrapper(aEventTarget, aRequireTailDispatch);

  return wrapper.forget();
}

}  // namespace mozilla
