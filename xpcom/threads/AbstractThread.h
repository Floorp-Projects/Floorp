/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AbstractThread_h_)
#  define AbstractThread_h_

#  include "mozilla/AlreadyAddRefed.h"
#  include "mozilla/ThreadLocal.h"
#  include "nscore.h"
#  include "nsISerialEventTarget.h"
#  include "nsISupports.h"

class nsIEventTarget;
class nsIRunnable;
class nsIThread;

namespace mozilla {

class TaskDispatcher;

/*
 * We often want to run tasks on a target that guarantees that events will never
 * run in parallel. There are various target types that achieve this - namely
 * nsIThread and TaskQueue. Note that nsIThreadPool (which implements
 * nsIEventTarget) does not have this property, so we do not want to use
 * nsIEventTarget for this purpose. This class encapsulates the specifics of
 * the structures we might use here and provides a consistent interface.
 *
 * At present, the supported AbstractThread implementations are TaskQueue,
 * AbstractThread::MainThread() and XPCOMThreadWrapper which can wrap any
 * nsThread.
 *
 * The primary use of XPCOMThreadWrapper is to allow any threads to provide
 * Direct Task dispatching which is similar (but not identical to) the microtask
 * semantics of JS promises. Instantiating a XPCOMThreadWrapper on the current
 * nsThread is sufficient to enable direct task dispatching.
 *
 * You shouldn't use pointers when comparing AbstractThread or nsIThread to
 * determine if you are currently on the thread, but instead use the
 * nsISerialEventTarget::IsOnCurrentThread() method.
 */
class AbstractThread : public nsISerialEventTarget {
 public:
  // Returns the AbstractThread that the caller is currently running in, or null
  // if the caller is not running in an AbstractThread.
  static AbstractThread* GetCurrent() { return sCurrentThreadTLS.get(); }

  AbstractThread(bool aSupportsTailDispatch)
      : mSupportsTailDispatch(aSupportsTailDispatch) {}

  // Returns an AbstractThread wrapper of a nsIThread.
  static already_AddRefed<AbstractThread> CreateXPCOMThreadWrapper(
      nsIThread* aThread, bool aRequireTailDispatch, bool aOnThread = false);

  NS_DECL_THREADSAFE_ISUPPORTS

  // We don't use NS_DECL_NSIEVENTTARGET so that we can remove the default
  // |flags| parameter from Dispatch. Otherwise, a single-argument Dispatch call
  // would be ambiguous.
  using nsISerialEventTarget::IsOnCurrentThread;
  NS_IMETHOD_(bool) IsOnCurrentThreadInfallible(void) override;
  NS_IMETHOD IsOnCurrentThread(bool* _retval) override;
  NS_IMETHOD Dispatch(already_AddRefed<nsIRunnable> event,
                      uint32_t flags) override;
  NS_IMETHOD DispatchFromScript(nsIRunnable* event, uint32_t flags) override;
  NS_IMETHOD DelayedDispatch(already_AddRefed<nsIRunnable> event,
                             uint32_t delay) override;

  enum DispatchReason { NormalDispatch, TailDispatch };
  virtual nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                            DispatchReason aReason = NormalDispatch) = 0;

  virtual bool IsCurrentThreadIn() const = 0;

  // Returns a TaskDispatcher that will dispatch its tasks when the currently-
  // running tasks pops off the stack.
  //
  // May only be called when running within the it is invoked up, and only on
  // threads which support it.
  virtual TaskDispatcher& TailDispatcher() = 0;

  // Returns true if we have tail tasks scheduled, or if this isn't known.
  // Returns false if we definitely don't have any tail tasks.
  virtual bool MightHaveTailTasks() { return true; }

  // Returns true if the tail dispatcher is available. In certain edge cases
  // like shutdown, it might not be.
  virtual bool IsTailDispatcherAvailable() { return true; }

  // Helper functions for methods on the tail TasklDispatcher. These check
  // HasTailTasks to avoid allocating a TailDispatcher if it isn't
  // needed.
  nsresult TailDispatchTasksFor(AbstractThread* aThread);
  bool HasTailTasksFor(AbstractThread* aThread);

  // Returns true if this supports the tail dispatcher.
  bool SupportsTailDispatch() const { return mSupportsTailDispatch; }

  // Returns true if this thread requires all dispatches originating from
  // aThread go through the tail dispatcher.
  bool RequiresTailDispatch(AbstractThread* aThread) const;
  bool RequiresTailDispatchFromCurrentThread() const;

  virtual nsIEventTarget* AsEventTarget() { MOZ_CRASH("Not an event target!"); }

  // Returns the non-DocGroup version of AbstractThread on the main thread.
  // A DocGroup-versioned one is available in
  // DispatcherTrait::AbstractThreadFor(). Note:
  // DispatcherTrait::AbstractThreadFor() SHALL be used when possible.
  static AbstractThread* MainThread();

  // Must be called exactly once during startup.
  static void InitTLS();
  static void InitMainThread();
  static void ShutdownMainThread();

  void DispatchStateChange(already_AddRefed<nsIRunnable> aRunnable);

  static void DispatchDirectTask(already_AddRefed<nsIRunnable> aRunnable);

 protected:
  virtual ~AbstractThread() = default;
  static MOZ_THREAD_LOCAL(AbstractThread*) sCurrentThreadTLS;

  // True if we want to require that every task dispatched from tasks running in
  // this queue go through our queue's tail dispatcher.
  const bool mSupportsTailDispatch;
};

}  // namespace mozilla

#endif
