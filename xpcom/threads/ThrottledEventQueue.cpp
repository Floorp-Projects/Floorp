/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThrottledEventQueue.h"

#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Mutex.h"
#include "mozilla/Unused.h"
#include "nsEventQueue.h"

namespace mozilla {

using mozilla::services::GetObserverService;

namespace {

static const char kShutdownTopic[] = "xpcom-shutdown";

} // anonymous namespace

// The ThrottledEventQueue is designed with inner and outer objects:
//
//       XPCOM code    nsObserverService
//            |               |
//            |               |
//            v               |
//        +-------+           |
//        | Outer |           |
//        +-------+           |
//            |               |
//            |   +-------+   |
//            +-->| Inner |<--+
//                +-------+
//
// Client code references the outer nsIEventTarget which in turn references
// an inner object.  The inner object is also held alive by the observer
// service.
//
// If the outer object is dereferenced and destroyed, it will trigger a
// shutdown operation on the inner object.  Similarly if the observer
// service notifies that the browser is shutting down, then the inner
// object also starts shutting down.
//
// Once the queue has drained we unregister from the observer service.  If
// the outer object is already gone, then the inner object is free'd at this
// point.  If the outer object still exists then calls fall back to the
// ThrottledEventQueue's base target.  We just don't queue things
// any more.  The inner is then released once the outer object is released.
//
// Note, we must keep the inner object alive and attached to the observer
// service until the TaskQueue is fully shutdown and idle.  We must delay
// xpcom shutdown if the TaskQueue is in the middle of draining.
class ThrottledEventQueue::Inner final : public nsIObserver
{
  // The runnable which is dispatched to the underlying base target.  Since
  // we only execute one event at a time we just re-use a single instance
  // of this class while there are events left in the queue.
  class Executor final : public Runnable
  {
    RefPtr<Inner> mInner;

  public:
    explicit Executor(Inner* aInner)
      : mInner(aInner)
    { }

    NS_IMETHODIMP
    Run() override
    {
      mInner->ExecuteRunnable();
      return NS_OK;
    }

    NS_IMETHODIMP
    GetName(nsACString& aName) override
    {
      return mInner->CurrentName(aName);
    }
  };

  mutable Mutex mMutex;
  mutable CondVar mIdleCondVar;

  mozilla::CondVar mEventsAvailable;

  // any thread, protected by mutex
  nsEventQueue mEventQueue;

  // written on main thread, read on any thread
  nsCOMPtr<nsIEventTarget> mBaseTarget;

  // any thread, protected by mutex
  nsCOMPtr<nsIRunnable> mExecutor;

  // any thread, protected by mutex
  bool mShutdownStarted;

  explicit Inner(nsIEventTarget* aBaseTarget)
    : mMutex("ThrottledEventQueue")
    , mIdleCondVar(mMutex, "ThrottledEventQueue:Idle")
    , mEventsAvailable(mMutex, "[ThrottledEventQueue::Inner.mEventsAvailable]")
    , mEventQueue(mEventsAvailable, nsEventQueue::eNormalQueue)
    , mBaseTarget(aBaseTarget)
    , mShutdownStarted(false)
  {
  }

  ~Inner()
  {
    MOZ_ASSERT(!mExecutor);
    MOZ_ASSERT(mShutdownStarted);
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
      MOZ_ALWAYS_TRUE(mEventQueue.PeekEvent(getter_AddRefs(event), lock));
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
    bool shouldShutdown = false;

#ifdef DEBUG
    bool currentThread = false;
    mBaseTarget->IsOnCurrentThread(&currentThread);
    MOZ_ASSERT(currentThread);
#endif

    {
      MutexAutoLock lock(mMutex);

      // We only dispatch an executor runnable when we know there is something
      // in the queue, so this should never fail.
      MOZ_ALWAYS_TRUE(mEventQueue.GetPendingEvent(getter_AddRefs(event), lock));

      // If there are more events in the queue, then dispatch the next
      // executor.  We do this now, before running the event, because
      // the event might spin the event loop and we don't want to stall
      // the queue.
      if (mEventQueue.HasPendingEvent(lock)) {
        // Dispatch the next base target runnable to attempt to execute
        // the next throttled event.  We must do this before executing
        // the event in case the event spins the event loop.
        MOZ_ALWAYS_SUCCEEDS(
          mBaseTarget->Dispatch(mExecutor, NS_DISPATCH_NORMAL));
      }

      // Otherwise the queue is empty and we can stop dispatching the
      // executor.  We might also need to shutdown after running the
      // last event.
      else {
        shouldShutdown = mShutdownStarted;
        // Note, this breaks a ref cycle.
        mExecutor = nullptr;
        mIdleCondVar.NotifyAll();
      }
    }

    // Execute the event now that we have unlocked.
    Unused << event->Run();

    // If shutdown was started and the queue is now empty we can now
    // finalize the shutdown.  This is performed separately at the end
    // of the method in order to wait for the event to finish running.
    if (shouldShutdown) {
      MOZ_ASSERT(IsEmpty());
      NS_DispatchToMainThread(NewRunnableMethod("ThrottledEventQueue::Inner::ShutdownComplete",
                                                this, &Inner::ShutdownComplete));
    }
  }

  void
  ShutdownComplete()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsEmpty());
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    obs->RemoveObserver(this, kShutdownTopic);
  }

public:
  static already_AddRefed<Inner>
  Create(nsIEventTarget* aBaseTarget)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (ClearOnShutdown_Internal::sCurrentShutdownPhase != ShutdownPhase::NotInShutdown) {
      return nullptr;
    }

    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return nullptr;
    }

    RefPtr<Inner> ref = new Inner(aBaseTarget);

    nsresult rv = obs->AddObserver(ref, kShutdownTopic,
                                   false /* means OS will hold a strong ref */);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ref->MaybeStartShutdown();
      MOZ_ASSERT(ref->IsEmpty());
      return nullptr;
    }

    return ref.forget();
  }

  NS_IMETHOD
  Observe(nsISupports*, const char* aTopic, const char16_t*) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!strcmp(aTopic, kShutdownTopic));

    MaybeStartShutdown();

    // Once shutdown begins we set the Atomic<bool> mShutdownStarted flag.
    // This prevents any new runnables from being dispatched into the
    // TaskQueue.  Therefore this loop should be finite.
    MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() -> bool {
        return IsEmpty();
    }));

    return NS_OK;
  }

  void
  MaybeStartShutdown()
  {
    // Any thread
    MutexAutoLock lock(mMutex);

    if (mShutdownStarted) {
      return;
    }
    mShutdownStarted = true;

    // We are marked for shutdown now, but we are still processing runnables.
    // Return for now.  The shutdown will be completed once the queue is
    // drained.
    if (mExecutor) {
      return;
    }

    // The queue is empty, so we can complete immediately.
    NS_DispatchToMainThread(NewRunnableMethod("ThrottledEventQueue::Inner::ShutdownComplete",
                                              this, &Inner::ShutdownComplete));
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

    // If we are shutting down, just fall back to our base target
    // directly.
    if (mShutdownStarted) {
      return mBaseTarget->Dispatch(Move(aEvent), aFlags);
    }

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
    mEventQueue.PutEvent(Move(aEvent), lock);
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

NS_IMPL_ISUPPORTS(ThrottledEventQueue::Inner, nsIObserver);

NS_IMPL_ISUPPORTS(ThrottledEventQueue, ThrottledEventQueue, nsIEventTarget);

ThrottledEventQueue::ThrottledEventQueue(already_AddRefed<Inner> aInner)
  : mInner(aInner)
{
  MOZ_ASSERT(mInner);
}

ThrottledEventQueue::~ThrottledEventQueue()
{
  mInner->MaybeStartShutdown();
}

void
ThrottledEventQueue::MaybeStartShutdown()
{
  return mInner->MaybeStartShutdown();
}

already_AddRefed<ThrottledEventQueue>
ThrottledEventQueue::Create(nsIEventTarget* aBaseTarget)
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
  return mInner->Dispatch(Move(aEvent), aFlags);
}

NS_IMETHODIMP
ThrottledEventQueue::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                            uint32_t aFlags)
{
  return mInner->DelayedDispatch(Move(aEvent), aFlags);
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
