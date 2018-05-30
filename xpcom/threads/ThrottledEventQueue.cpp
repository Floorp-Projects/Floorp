/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThrottledEventQueue.h"

#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventQueue.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"

namespace mozilla {

namespace {

} // anonymous namespace

// The ThrottledEventQueue is designed with inner and outer objects:
//
//       XPCOM code     base event target
//            |               |
//            v               v
//        +-------+       +--------+
//        | Outer |   +-->|executor|
//        +-------+   |   +--------+
//            |       |       |
//            |   +-------+   |
//            +-->| Inner |<--+
//                +-------+
//
// Client code references the outer nsIEventTarget which in turn references
// an inner object, which actually holds the queue of runnables.
//
// Whenever the queue is non-empty (and not paused), it keeps an "executor"
// runnable dispatched to the base event target. Each time the executor is run,
// it draws the next event from Inner's queue and runs it. If that queue has
// more events, the executor is dispatched to the base again.
//
// The executor holds a strong reference to the Inner object. This means that if
// the outer object is dereferenced and destroyed, the Inner object will remain
// live for as long as the executor exists - that is, until the Inner's queue is
// empty.
//
// The xpcom shutdown process drains the main thread's event queue several
// times, so if a ThrottledEventQueue is being driven by the main thread, it
// should get emptied out by the time we reach the "eventq shutdown" phase.
class ThrottledEventQueue::Inner final : public nsISupports
{
  // The runnable which is dispatched to the underlying base target.  Since
  // we only execute one event at a time we just re-use a single instance
  // of this class while there are events left in the queue.
  class Executor final : public Runnable
  {
    // The Inner whose runnables we execute. mInner->mExecutor points
    // to this executor, forming a reference loop.
    RefPtr<Inner> mInner;

  public:
    explicit Executor(Inner* aInner)
      : Runnable("ThrottledEventQueue::Inner::Executor")
      , mInner(aInner)
    { }

    NS_IMETHODIMP
    Run() override
    {
      mInner->ExecuteRunnable();
      return NS_OK;
    }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    NS_IMETHODIMP
    GetName(nsACString& aName) override
    {
      return mInner->CurrentName(aName);
    }
#endif
  };

  mutable Mutex mMutex;
  mutable CondVar mIdleCondVar;

  // As-of-yet unexecuted runnables queued on this ThrottledEventQueue.
  // (Used from any thread, protected by mMutex.)
  EventQueue mEventQueue;

  // The event target we dispatch our events (actually, just our Executor) to.
  // (Written during construction on main thread; read by any thread.)
  nsCOMPtr<nsISerialEventTarget> mBaseTarget;

  // The Executor that we dispatch to mBaseTarget to draw runnables from our
  // queue. mExecutor->mInner points to this Inner, forming a reference loop.
  // (Used from any thread, protected by mMutex.)
  nsCOMPtr<nsIRunnable> mExecutor;

  explicit Inner(nsISerialEventTarget* aBaseTarget)
    : mMutex("ThrottledEventQueue")
    , mIdleCondVar(mMutex, "ThrottledEventQueue:Idle")
    , mBaseTarget(aBaseTarget)
  {
  }

  ~Inner()
  {
#ifdef DEBUG
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mExecutor);
    MOZ_ASSERT(mEventQueue.IsEmpty(lock));
#endif
  }

  nsresult
  CurrentName(nsACString& aName)
  {
    nsCOMPtr<nsIRunnable> event;

#ifdef DEBUG
    bool currentThread = false;
    mBaseTarget->IsOnCurrentThread(&currentThread);
    MOZ_ASSERT(currentThread);
#endif

    {
      MutexAutoLock lock(mMutex);

      // We only check the name of an executor runnable when we know there is something
      // in the queue, so this should never fail.
      event = mEventQueue.PeekEvent(lock);
      MOZ_ALWAYS_TRUE(event);
    }

    if (nsCOMPtr<nsINamed> named = do_QueryInterface(event)) {
      nsresult rv = named->GetName(aName);
      return rv;
    }

    aName.AssignLiteral("non-nsINamed ThrottledEventQueue runnable");
    return NS_OK;
  }

  void
  ExecuteRunnable()
  {
    // Any thread
    nsCOMPtr<nsIRunnable> event;

#ifdef DEBUG
    bool currentThread = false;
    mBaseTarget->IsOnCurrentThread(&currentThread);
    MOZ_ASSERT(currentThread);
#endif

    {
      MutexAutoLock lock(mMutex);

      // We only dispatch an executor runnable when we know there is something
      // in the queue, so this should never fail.
      event = mEventQueue.GetEvent(nullptr, lock);
      MOZ_ASSERT(event);

      // If there are more events in the queue, then dispatch the next
      // executor.  We do this now, before running the event, because
      // the event might spin the event loop and we don't want to stall
      // the queue.
      if (mEventQueue.HasReadyEvent(lock)) {
        // Dispatch the next base target runnable to attempt to execute
        // the next throttled event.  We must do this before executing
        // the event in case the event spins the event loop.
        MOZ_ALWAYS_SUCCEEDS(
          mBaseTarget->Dispatch(mExecutor, NS_DISPATCH_NORMAL));
      }

      // Otherwise the queue is empty and we can stop dispatching the
      // executor.
      else {
        // Break the Executor::mInner / Inner::mExecutor reference loop.
        mExecutor = nullptr;
        mIdleCondVar.NotifyAll();
      }
    }

    // Execute the event now that we have unlocked.
    Unused << event->Run();
  }

public:
  static already_AddRefed<Inner>
  Create(nsISerialEventTarget* aBaseTarget)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(ClearOnShutdown_Internal::sCurrentShutdownPhase == ShutdownPhase::NotInShutdown);

    RefPtr<Inner> ref = new Inner(aBaseTarget);
    return ref.forget();
  }

  bool
  IsEmpty() const
  {
    // Any thread
    return Length() == 0;
  }

  uint32_t
  Length() const
  {
    // Any thread
    MutexAutoLock lock(mMutex);
    return mEventQueue.Count(lock);
  }

  void
  AwaitIdle() const
  {
    // Any thread, except the main thread or our base target.  Blocking the
    // main thread is forbidden.  Blocking the base target is guaranteed to
    // produce a deadlock.
    MOZ_ASSERT(!NS_IsMainThread());
#ifdef DEBUG
    bool onBaseTarget = false;
    Unused << mBaseTarget->IsOnCurrentThread(&onBaseTarget);
    MOZ_ASSERT(!onBaseTarget);
#endif

    MutexAutoLock lock(mMutex);
    while (mExecutor) {
      mIdleCondVar.Wait();
    }
  }

  nsresult
  DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags)
  {
    // Any thread
    nsCOMPtr<nsIRunnable> r = aEvent;
    return Dispatch(r.forget(), aFlags);
  }

  nsresult
  Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags)
  {
    MOZ_ASSERT(aFlags == NS_DISPATCH_NORMAL ||
               aFlags == NS_DISPATCH_AT_END);

    // Any thread
    MutexAutoLock lock(mMutex);

    // We are not currently processing events, so we must start
    // operating on our base target.  This is fallible, so do
    // it first.  Our lock will prevent the executor from accessing
    // the event queue before we add the event below.
    if (!mExecutor) {
      // Note, this creates a ref cycle keeping the inner alive
      // until the queue is drained.
      mExecutor = new Executor(this);
      nsresult rv = mBaseTarget->Dispatch(mExecutor, NS_DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mExecutor = nullptr;
        return rv;
      }
    }

    // Only add the event to the underlying queue if are able to
    // dispatch to our base target.
    mEventQueue.PutEvent(std::move(aEvent), EventPriority::Normal, lock);
    return NS_OK;
  }

  nsresult
  DelayedDispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aDelay)
  {
    // The base target may implement this, but we don't.  Always fail
    // to provide consistent behavior.
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool
  IsOnCurrentThread()
  {
    return mBaseTarget->IsOnCurrentThread();
  }

  NS_DECL_THREADSAFE_ISUPPORTS
};

NS_IMPL_ISUPPORTS(ThrottledEventQueue::Inner, nsISupports);

NS_IMPL_ISUPPORTS(ThrottledEventQueue,
                  ThrottledEventQueue,
                  nsIEventTarget,
                  nsISerialEventTarget);

ThrottledEventQueue::ThrottledEventQueue(already_AddRefed<Inner> aInner)
  : mInner(aInner)
{
  MOZ_ASSERT(mInner);
}

already_AddRefed<ThrottledEventQueue>
ThrottledEventQueue::Create(nsISerialEventTarget* aBaseTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBaseTarget);

  RefPtr<Inner> inner = Inner::Create(aBaseTarget);
  if (NS_WARN_IF(!inner)) {
    return nullptr;
  }

  RefPtr<ThrottledEventQueue> ref =
    new ThrottledEventQueue(inner.forget());
  return ref.forget();
}

bool
ThrottledEventQueue::IsEmpty() const
{
  return mInner->IsEmpty();
}

uint32_t
ThrottledEventQueue::Length() const
{
  return mInner->Length();
}

void
ThrottledEventQueue::AwaitIdle() const
{
  return mInner->AwaitIdle();
}

NS_IMETHODIMP
ThrottledEventQueue::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags)
{
  return mInner->DispatchFromScript(aEvent, aFlags);
}

NS_IMETHODIMP
ThrottledEventQueue::Dispatch(already_AddRefed<nsIRunnable> aEvent,
                                     uint32_t aFlags)
{
  return mInner->Dispatch(std::move(aEvent), aFlags);
}

NS_IMETHODIMP
ThrottledEventQueue::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                            uint32_t aFlags)
{
  return mInner->DelayedDispatch(std::move(aEvent), aFlags);
}

NS_IMETHODIMP
ThrottledEventQueue::IsOnCurrentThread(bool* aResult)
{
  *aResult = mInner->IsOnCurrentThread();
  return NS_OK;
}

NS_IMETHODIMP_(bool)
ThrottledEventQueue::IsOnCurrentThreadInfallible()
{
  return mInner->IsOnCurrentThread();
}

} // namespace mozilla
