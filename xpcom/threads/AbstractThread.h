/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AbstractThread_h_)
#define AbstractThread_h_

#include "mozilla/RefPtr.h"
#include "mozilla/ThreadLocal.h"
#include "nscore.h"
#include "nsIRunnable.h"
#include "nsISerialEventTarget.h"
#include "nsISupportsImpl.h"
#include "nsIThread.h"

namespace mozilla {

class TaskQueue;
class TaskDispatcher;

/*
 * NOTE: PLEASE AVOID USE OF AbstractThread OUTSIDE MEDIA CODE WHEN POSSIBLE.
 * The nsISerialEventTarget interface should be preferred. AbstractThread
 * has unusual "tail dispatch" semantics that usually are not needed outside
 * of media code.
 *
 * We often want to run tasks on a target that guarantees that events will never
 * run in parallel. There are various target types that achieve this - namely
 * nsIThread and TaskQueue. Note that nsIThreadPool (which implements
 * nsIEventTarget) does not have this property, so we do not want to use
 * nsIEventTarget for this purpose. This class encapsulates the specifics of
 * the structures we might use here and provides a consistent interface.
 *
 * At present, the supported AbstractThread implementations are TaskQueue,
 * AbstractThread::MainThread() and DocGroup::AbstractThreadFor().
 * If you add support for another thread that is not the MainThread, you'll need
 * to figure out how to make it unique such that comparing AbstractThread
 * pointers is equivalent to comparing nsIThread pointers.
 */
class AbstractThread : public nsISerialEventTarget
{
public:
  // Returns the AbstractThread that the caller is currently running in, or null
  // if the caller is not running in an AbstractThread.
  static AbstractThread* GetCurrent() { return sCurrentThreadTLS.get(); }

  AbstractThread(bool aSupportsTailDispatch) : mSupportsTailDispatch(aSupportsTailDispatch) {}

  // Returns an AbstractThread wrapper of a nsIThread.
  static already_AddRefed<AbstractThread>
  CreateXPCOMThreadWrapper(nsIThread* aThread, bool aRequireTailDispatch);

  // Returns an AbstractThread wrapper of a non-nsIThread EventTarget on the main thread.
  static already_AddRefed<AbstractThread>
  CreateEventTargetWrapper(nsIEventTarget* aEventTarget, bool aRequireTailDispatch);

  NS_DECL_THREADSAFE_ISUPPORTS

  // We don't use NS_DECL_NSIEVENTTARGET so that we can remove the default
  // |flags| parameter from Dispatch. Otherwise, a single-argument Dispatch call
  // would be ambiguous.
  NS_IMETHOD_(bool) IsOnCurrentThreadInfallible(void) override;
  NS_IMETHOD IsOnCurrentThread(bool *_retval) override;
  NS_IMETHOD Dispatch(already_AddRefed<nsIRunnable> event, uint32_t flags) override;
  NS_IMETHOD DispatchFromScript(nsIRunnable *event, uint32_t flags) override;
  NS_IMETHOD DelayedDispatch(already_AddRefed<nsIRunnable> event, uint32_t delay) override;

  enum DispatchFailureHandling { AssertDispatchSuccess, DontAssertDispatchSuccess };
  enum DispatchReason { NormalDispatch, TailDispatch };
  virtual void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                        DispatchFailureHandling aHandling = AssertDispatchSuccess,
                        DispatchReason aReason = NormalDispatch) = 0;

  virtual bool IsCurrentThreadIn() = 0;

  // Returns a TaskDispatcher that will dispatch its tasks when the currently-
  // running tasks pops off the stack.
  //
  // May only be called when running within the it is invoked up, and only on
  // threads which support it.
  virtual TaskDispatcher& TailDispatcher() = 0;

  // Returns true if we have tail tasks scheduled, or if this isn't known.
  // Returns false if we definitely don't have any tail tasks.
  virtual bool MightHaveTailTasks() { return true; }

  // Helper functions for methods on the tail TasklDispatcher. These check
  // HasTailTasks to avoid allocating a TailDispatcher if it isn't
  // needed.
  void TailDispatchTasksFor(AbstractThread* aThread);
  bool HasTailTasksFor(AbstractThread* aThread);

  // Returns true if this supports the tail dispatcher.
  bool SupportsTailDispatch() const { return mSupportsTailDispatch; }

  // Returns true if this thread requires all dispatches originating from
  // aThread go through the tail dispatcher.
  bool RequiresTailDispatch(AbstractThread* aThread) const;
  bool RequiresTailDispatchFromCurrentThread() const;

  virtual TaskQueue* AsTaskQueue() { MOZ_CRASH("Not a task queue!"); }
  virtual nsIEventTarget* AsEventTarget() { MOZ_CRASH("Not an event target!"); }

  // Returns the non-DocGroup version of AbstractThread on the main thread.
  // A DocGroup-versioned one is available in DispatcherTrait::AbstractThreadFor().
  // Note: DispatcherTrait::AbstractThreadFor() SHALL be used when possible.
  static AbstractThread* MainThread();

  // Must be called exactly once during startup.
  static void InitTLS();
  static void InitMainThread();

  void DispatchStateChange(already_AddRefed<nsIRunnable> aRunnable);

  static void DispatchDirectTask(already_AddRefed<nsIRunnable> aRunnable);

  // Create a runnable that will run |aRunnable| and drain the direct tasks
  // generated by it.
  virtual already_AddRefed<nsIRunnable>
  CreateDirectTaskDrainer(already_AddRefed<nsIRunnable> aRunnable)
  {
    MOZ_CRASH("Not support!");
  }

protected:
  virtual ~AbstractThread() {}
  static MOZ_THREAD_LOCAL(AbstractThread*) sCurrentThreadTLS;

  // True if we want to require that every task dispatched from tasks running in
  // this queue go through our queue's tail dispatcher.
  const bool mSupportsTailDispatch;
};

} // namespace mozilla

#endif
