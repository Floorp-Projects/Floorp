/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_lazyidlethread_h__
#define mozilla_lazyidlethread_h__

#ifndef MOZILLA_INTERNAL_API
#error "This header is only usable from within libxul (MOZILLA_INTERNAL_API)."
#endif

#include "nsIObserver.h"
#include "nsIThreadInternal.h"
#include "nsITimer.h"

#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

#define IDLE_THREAD_TOPIC "thread-shutting-down"

namespace mozilla {

/**
 * This class provides a basic event target that creates its thread lazily and
 * destroys its thread after a period of inactivity. It may be created on any
 * thread but it may only be used from the thread on which it is created. If it
 * is created on the main thread then it will automatically join its thread on
 * XPCOM shutdown using the Observer Service.
 */
class LazyIdleThread MOZ_FINAL : public nsIThread,
                                 public nsITimerCallback,
                                 public nsIThreadObserver,
                                 public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET
  NS_DECL_NSITHREAD
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSITHREADOBSERVER
  NS_DECL_NSIOBSERVER

  enum ShutdownMethod {
    AutomaticShutdown = 0,
    ManualShutdown
  };

  /**
   * Create a new LazyIdleThread that will destroy its thread after the given
   * number of milliseconds.
   */
  LazyIdleThread(uint32_t aIdleTimeoutMS,
                 const nsCSubstring& aName,
                 ShutdownMethod aShutdownMethod = AutomaticShutdown,
                 nsIObserver* aIdleObserver = nullptr);

  /**
   * Add an observer that will be notified when the thread is idle and about to
   * be shut down. The aSubject argument can be QueryInterface'd to an nsIThread
   * that can be used to post cleanup events. The aTopic argument will be
   * IDLE_THREAD_TOPIC, and aData will be null. The LazyIdleThread does not add
   * a reference to the observer to avoid circular references as it is assumed
   * to be the owner. It is the caller's responsibility to clear this observer
   * if the pointer becomes invalid.
   */
  void SetWeakIdleObserver(nsIObserver* aObserver);

  /**
   * Disable the idle timeout for this thread. No effect if the timeout is
   * already disabled.
   */
  void DisableIdleTimeout();

  /**
   * Enable the idle timeout. No effect if the timeout is already enabled.
   */
  void EnableIdleTimeout();

private:
  /**
   * Calls Shutdown().
   */
  ~LazyIdleThread();

  /**
   * Called just before dispatching to mThread.
   */
  void PreDispatch();

  /**
   * Makes sure a valid thread lives in mThread.
   */
  nsresult EnsureThread();

  /**
   * Called on mThread to set up the thread observer.
   */
  void InitThread();

  /**
   * Called on mThread to clean up the thread observer.
   */
  void CleanupThread();

  /**
   * Called on the main thread when mThread believes itself to be idle. Sets up
   * the idle timer.
   */
  void ScheduleTimer();

  /**
   * Called when we are shutting down mThread.
   */
  nsresult ShutdownThread();

  /**
   * Deletes this object. Used to delay calling mThread->Shutdown() during the
   * final release (during a GC, for instance).
   */
  void SelfDestruct();

  /**
   * Returns true if events should be queued rather than immediately dispatched
   * to mThread. Currently only happens when the thread is shutting down.
   */
  bool UseRunnableQueue() {
    return !!mQueuedRunnables;
  }

  /**
   * Protects data that is accessed on both threads.
   */
  mozilla::Mutex mMutex;

  /**
   * Touched on both threads but set before mThread is created. Used to direct
   * timer events to the owning thread.
   */
  nsCOMPtr<nsIThread> mOwningThread;

  /**
   * Only accessed on the owning thread. Set by EnsureThread().
   */
  nsCOMPtr<nsIThread> mThread;

  /**
   * Protected by mMutex. Created when mThread has no pending events and fired
   * at mOwningThread. Any thread that dispatches to mThread will take ownership
   * of the timer and fire a separate cancel event to the owning thread.
   */
  nsCOMPtr<nsITimer> mIdleTimer;

  /**
   * Idle observer. Called when the thread is about to be shut down. Released
   * only when Shutdown() is called.
   */
  nsIObserver* mIdleObserver;

  /**
   * Temporary storage for events that happen to be dispatched while we're in
   * the process of shutting down our real thread.
   */
  nsTArray<nsCOMPtr<nsIRunnable> >* mQueuedRunnables;

  /**
   * The number of milliseconds a thread should be idle before dying.
   */
  const uint32_t mIdleTimeoutMS;

  /**
   * The number of events that are pending on mThread. A nonzero value means
   * that the thread cannot be cleaned up.
   */
  uint32_t mPendingEventCount;

  /**
   * The number of times that mThread has dispatched an idle notification. Any
   * timer that fires while this count is nonzero can safely be ignored as
   * another timer will be on the way.
   */
  uint32_t mIdleNotificationCount;

  /**
   * Whether or not the thread should automatically shutdown. If the owner
   * specified ManualShutdown at construction time then the owner should take
   * care to call Shutdown() manually when appropriate.
   */
  ShutdownMethod mShutdownMethod;

  /**
   * Only accessed on the owning thread. Set to true when Shutdown() has been
   * called and prevents EnsureThread() from recreating mThread.
   */
  bool mShutdown;

  /**
   * Set from CleanupThread and lasting until the thread has shut down. Prevents
   * further idle notifications during the shutdown process.
   */
  bool mThreadIsShuttingDown;

  /**
   * Whether or not the idle timeout is enabled.
   */
  bool mIdleTimeoutEnabled;

  /**
   * Name of the thread, set on the actual thread after it gets created.
   */
  nsCString mName;
};

} // namespace mozilla

#endif // mozilla_lazyidlethread_h__
