/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AbstractThread.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DelayedRunnable.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"  // We initialize the MozPromise logging in this file.
#include "mozilla/StateWatching.h"  // We initialize the StateWatching logging in this file.
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskDispatcher.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsIDirectTaskDispatcher.h"
#include "nsIThreadInternal.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
namespace mozilla {

LazyLogModule gMozPromiseLog("MozPromise");
LazyLogModule gStateWatchingLog("StateWatching");

StaticRefPtr<AbstractThread> sMainThread;
MOZ_THREAD_LOCAL(AbstractThread*) AbstractThread::sCurrentThreadTLS;

class XPCOMThreadWrapper final : public AbstractThread,
                                 public nsIThreadObserver,
                                 public nsIDirectTaskDispatcher,
                                 public nsIDelayedRunnableObserver {
 public:
  XPCOMThreadWrapper(nsIThreadInternal* aThread, bool aRequireTailDispatch,
                     bool aOnThread)
      : AbstractThread(aRequireTailDispatch),
        mThread(aThread),
        mDelayedRunnableObserver(do_QueryInterface(mThread)),
        mDirectTaskDispatcher(do_QueryInterface(aThread)),
        mOnThread(aOnThread) {
    MOZ_DIAGNOSTIC_ASSERT(mThread && mDirectTaskDispatcher);
    MOZ_DIAGNOSTIC_ASSERT(!aOnThread || IsCurrentThreadIn());
    if (aOnThread) {
      MOZ_ASSERT(!sCurrentThreadTLS.get(),
                 "There can only be a single XPCOMThreadWrapper available on a "
                 "thread");
      // Set the default current thread so that GetCurrent() never returns
      // nullptr.
      sCurrentThreadTLS.set(this);
    }
  }

  NS_DECL_ISUPPORTS_INHERITED

  nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                    DispatchReason aReason = NormalDispatch) override {
    nsCOMPtr<nsIRunnable> r = aRunnable;
    AbstractThread* currentThread;
    if (aReason != TailDispatch && (currentThread = GetCurrent()) &&
        RequiresTailDispatch(currentThread) &&
        currentThread->IsTailDispatcherAvailable()) {
      return currentThread->TailDispatcher().AddTask(this, r.forget());
    }

    // At a certain point during shutdown, we stop processing events from the
    // main thread event queue (this happens long after all _other_ XPCOM
    // threads have been shut down). However, various bits of subsequent
    // teardown logic (the media shutdown blocker and the final shutdown cycle
    // collection) can trigger state watching and state mirroring notifications
    // that result in dispatch to the main thread. This causes shutdown leaks,
    // because the |Runner| wrapper below creates a guaranteed cycle
    // (Thread->EventQueue->Runnable->Thread) until the event is processed. So
    // if we put the event into a queue that will never be processed, we'll wind
    // up with a leak.
    //
    // We opt to just release the runnable in that case. Ordinarily, this
    // approach could cause problems for runnables that are only safe to be
    // released on the target thread (and not the dispatching thread). This is
    // why XPCOM thread dispatch explicitly leaks the runnable when dispatch
    // fails, rather than releasing it. But given that this condition only
    // applies very late in shutdown when only one thread remains operational,
    // that concern is unlikely to apply.
    if (gXPCOMMainThreadEventsAreDoomed) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<nsIRunnable> runner = new Runner(this, r.forget());
    return mThread->Dispatch(runner.forget(), NS_DISPATCH_NORMAL);
  }

  // Prevent a GCC warning about the other overload of Dispatch being hidden.
  using AbstractThread::Dispatch;

  bool IsCurrentThreadIn() const override {
    return mThread->IsOnCurrentThread();
  }

  TaskDispatcher& TailDispatcher() override {
    MOZ_ASSERT(IsCurrentThreadIn());
    MOZ_ASSERT(IsTailDispatcherAvailable());
    if (!mTailDispatcher.isSome()) {
      mTailDispatcher.emplace(mDirectTaskDispatcher,
                              /* aIsTailDispatcher = */ true);
      mThread->AddObserver(this);
    }

    return mTailDispatcher.ref();
  }

  bool IsTailDispatcherAvailable() override {
    // Our tail dispatching implementation relies on nsIThreadObserver
    // callbacks. If we're not doing event processing, it won't work.
    bool inEventLoop =
        static_cast<nsThread*>(mThread.get())->RecursionDepth() > 0;
    return inEventLoop;
  }

  bool MightHaveTailTasks() override { return mTailDispatcher.isSome(); }

  nsIEventTarget* AsEventTarget() override { return mThread; }

  //-----------------------------------------------------------------------------
  // nsIThreadObserver
  //-----------------------------------------------------------------------------
  NS_IMETHOD OnDispatchedEvent() override { return NS_OK; }

  NS_IMETHOD AfterProcessNextEvent(nsIThreadInternal* thread,
                                   bool eventWasProcessed) override {
    // This is the primary case.
    MaybeFireTailDispatcher();
    return NS_OK;
  }

  NS_IMETHOD OnProcessNextEvent(nsIThreadInternal* thread,
                                bool mayWait) override {
    // In general, the tail dispatcher is handled at the end of the current in
    // AfterProcessNextEvent() above. However, if start spinning a nested event
    // loop, it's generally better to fire the tail dispatcher before the first
    // nested event, rather than after it. This check handles that case.
    MaybeFireTailDispatcher();
    return NS_OK;
  }

  //-----------------------------------------------------------------------------
  // nsIDirectTaskDispatcher
  //-----------------------------------------------------------------------------
  // Forward calls to nsIDirectTaskDispatcher to the underlying nsThread object.
  // We can't use the generated NS_FORWARD_NSIDIRECTTASKDISPATCHER macro
  // as already_AddRefed type must be moved.
  NS_IMETHOD DispatchDirectTask(already_AddRefed<nsIRunnable> aEvent) override {
    return mDirectTaskDispatcher->DispatchDirectTask(std::move(aEvent));
  }
  NS_IMETHOD DrainDirectTasks() override {
    return mDirectTaskDispatcher->DrainDirectTasks();
  }
  NS_IMETHOD HaveDirectTasks(bool* aResult) override {
    return mDirectTaskDispatcher->HaveDirectTasks(aResult);
  }

  //-----------------------------------------------------------------------------
  // nsIDelayedRunnableObserver
  //-----------------------------------------------------------------------------
  void OnDelayedRunnableCreated(DelayedRunnable* aRunnable) override {
    mDelayedRunnableObserver->OnDelayedRunnableCreated(aRunnable);
  }
  void OnDelayedRunnableScheduled(DelayedRunnable* aRunnable) override {
    mDelayedRunnableObserver->OnDelayedRunnableScheduled(aRunnable);
  }
  void OnDelayedRunnableRan(DelayedRunnable* aRunnable) override {
    mDelayedRunnableObserver->OnDelayedRunnableRan(aRunnable);
  }

 private:
  const RefPtr<nsIThreadInternal> mThread;
  const nsCOMPtr<nsIDelayedRunnableObserver> mDelayedRunnableObserver;
  const nsCOMPtr<nsIDirectTaskDispatcher> mDirectTaskDispatcher;
  Maybe<AutoTaskDispatcher> mTailDispatcher;
  const bool mOnThread;

  ~XPCOMThreadWrapper() {
    if (mOnThread) {
      MOZ_DIAGNOSTIC_ASSERT(IsCurrentThreadIn(),
                            "Must be destroyed on the thread it was created");
      sCurrentThreadTLS.set(nullptr);
    }
  }

  void MaybeFireTailDispatcher() {
    if (mTailDispatcher.isSome()) {
      mTailDispatcher.ref().DrainDirectTasks();
      mThread->RemoveObserver(this);
      mTailDispatcher.reset();
    }
  }

  class Runner : public Runnable {
   public:
    explicit Runner(XPCOMThreadWrapper* aThread,
                    already_AddRefed<nsIRunnable> aRunnable)
        : Runnable("XPCOMThreadWrapper::Runner"),
          mThread(aThread),
          mRunnable(aRunnable) {}

    NS_IMETHOD Run() override {
      MOZ_ASSERT(mThread == AbstractThread::GetCurrent());
      MOZ_ASSERT(mThread->IsCurrentThreadIn());
      SerialEventTargetGuard guard(mThread);
      return mRunnable->Run();
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
    const RefPtr<XPCOMThreadWrapper> mThread;
    const RefPtr<nsIRunnable> mRunnable;
  };
};

NS_INTERFACE_MAP_BEGIN(XPCOMThreadWrapper)
  NS_INTERFACE_MAP_ENTRY(nsIThreadObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDirectTaskDispatcher)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDelayedRunnableObserver,
                                     mDelayedRunnableObserver)
NS_INTERFACE_MAP_END_INHERITING(AbstractThread)

NS_IMPL_ADDREF_INHERITED(XPCOMThreadWrapper, AbstractThread)
NS_IMPL_RELEASE_INHERITED(XPCOMThreadWrapper, AbstractThread)

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
  nsCOMPtr<nsIRunnable> event = aEvent;
  NS_ENSURE_TRUE(!!aDelayMs, NS_ERROR_UNEXPECTED);

  RefPtr<DelayedRunnable> r =
      new DelayedRunnable(do_AddRef(this), event.forget(), aDelayMs);
  nsresult rv = r->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return Dispatch(r.forget(), NS_DISPATCH_NORMAL);
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
  nsCOMPtr<nsIThreadInternal> mainThread =
      do_QueryInterface(nsThreadManager::get().GetMainThreadWeak());
  MOZ_DIAGNOSTIC_ASSERT(mainThread);

  if (!sCurrentThreadTLS.init()) {
    MOZ_CRASH();
  }
  sMainThread = new XPCOMThreadWrapper(mainThread.get(),
                                       /* aRequireTailDispatch = */ true,
                                       true /* onThread */);
}

void AbstractThread::ShutdownMainThread() {
  MOZ_ASSERT(NS_IsMainThread());
  sMainThread = nullptr;
}

void AbstractThread::DispatchStateChange(
    already_AddRefed<nsIRunnable> aRunnable) {
  AbstractThread* currentThread = GetCurrent();
  MOZ_DIAGNOSTIC_ASSERT(currentThread, "An AbstractThread must exist");
  if (currentThread->IsTailDispatcherAvailable()) {
    currentThread->TailDispatcher().AddStateChangeTask(this,
                                                       std::move(aRunnable));
  } else {
    // If the tail dispatcher isn't available, we just avoid sending state
    // updates.
    //
    // This happens, specifically (1) During async shutdown (via the media
    // shutdown blocker), and (2) During the final shutdown cycle collection.
    // Both of these trigger changes to various watched and mirrored state.
    nsCOMPtr<nsIRunnable> neverDispatched = aRunnable;
  }
}

/* static */
void AbstractThread::DispatchDirectTask(
    already_AddRefed<nsIRunnable> aRunnable) {
  AbstractThread* currentThread = GetCurrent();
  MOZ_DIAGNOSTIC_ASSERT(currentThread, "An AbstractThread must exist");
  if (currentThread->IsTailDispatcherAvailable()) {
    currentThread->TailDispatcher().AddDirectTask(std::move(aRunnable));
  } else {
    // If the tail dispatcher isn't available, we post as a regular task.
    currentThread->Dispatch(std::move(aRunnable));
  }
}

/* static */
already_AddRefed<AbstractThread> AbstractThread::CreateXPCOMThreadWrapper(
    nsIThread* aThread, bool aRequireTailDispatch, bool aOnThread) {
  nsCOMPtr<nsIThreadInternal> internalThread = do_QueryInterface(aThread);
  MOZ_ASSERT(internalThread, "Need an nsThread for AbstractThread");
  RefPtr<XPCOMThreadWrapper> wrapper =
      new XPCOMThreadWrapper(internalThread, aRequireTailDispatch, aOnThread);

  bool onCurrentThread = false;
  Unused << aThread->IsOnCurrentThread(&onCurrentThread);

  if (onCurrentThread) {
    if (!aOnThread) {
      MOZ_ASSERT(!sCurrentThreadTLS.get(),
                 "There can only be a single XPCOMThreadWrapper available on a "
                 "thread");
      sCurrentThreadTLS.set(wrapper);
    }
    return wrapper.forget();
  }

  // Set the thread-local sCurrentThreadTLS to point to the wrapper on the
  // target thread. This ensures that sCurrentThreadTLS is as expected by
  // AbstractThread::GetCurrent() on the target thread.
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "AbstractThread::CreateXPCOMThreadWrapper", [wrapper]() {
        MOZ_ASSERT(!sCurrentThreadTLS.get(),
                   "There can only be a single XPCOMThreadWrapper available on "
                   "a thread");
        sCurrentThreadTLS.set(wrapper);
      });
  aThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  return wrapper.forget();
}
}  // namespace mozilla
