/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMessageLoop.h"
#include "mozilla/WeakPtr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "nsIRunnable.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;

namespace {

/**
 * This Task runs its nsIRunnable when Run() is called, or after
 * aEnsureRunsAfterMS milliseconds have elapsed since the object was
 * constructed.
 *
 * Note that the MessageLoop owns this object and will delete it after it calls
 * Run().  Tread lightly.
 */
class MessageLoopIdleTask
  : public Task
  , public SupportsWeakPtr<MessageLoopIdleTask>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(MessageLoopIdleTask)
  MessageLoopIdleTask(nsIRunnable* aTask, uint32_t aEnsureRunsAfterMS);
  virtual void Run();

private:
  nsresult Init(uint32_t aEnsureRunsAfterMS);

  nsCOMPtr<nsIRunnable> mTask;
  nsCOMPtr<nsITimer> mTimer;

  virtual ~MessageLoopIdleTask() {}
};

/**
 * This timer callback calls MessageLoopIdleTask::Run() when its timer fires.
 * (The timer can't call back into MessageLoopIdleTask directly since that's
 * not a refcounted object; it's owned by the MessageLoop.)
 *
 * We keep a weak reference to the MessageLoopIdleTask, although a raw pointer
 * should in theory suffice: When the MessageLoopIdleTask runs (right before
 * the MessageLoop deletes it), it cancels its timer.  But the weak pointer
 * saves us from worrying about an edge case somehow messing us up here.
 */
class MessageLoopTimerCallback
  : public nsITimerCallback
{
public:
  MessageLoopTimerCallback(MessageLoopIdleTask* aTask);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

private:
  WeakPtr<MessageLoopIdleTask> mTask;

  virtual ~MessageLoopTimerCallback() {}
};

MessageLoopIdleTask::MessageLoopIdleTask(nsIRunnable* aTask,
                                         uint32_t aEnsureRunsAfterMS)
  : mTask(aTask)
{
  // Init() really shouldn't fail, but if it does, we schedule our runnable
  // immediately, because it's more important to guarantee that we run the task
  // eventually than it is to run the task when we're idle.
  nsresult rv = Init(aEnsureRunsAfterMS);
  if (NS_FAILED(rv)) {
    NS_WARNING("Running idle task early because we couldn't initialize our timer.");
    NS_DispatchToCurrentThread(mTask);

    mTask = nullptr;
    mTimer = nullptr;
  }
}

nsresult
MessageLoopIdleTask::Init(uint32_t aEnsureRunsAfterMS)
{
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (NS_WARN_IF(!mTimer)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<MessageLoopTimerCallback> callback =
    new MessageLoopTimerCallback(this);

  return mTimer->InitWithCallback(callback, aEnsureRunsAfterMS,
                                  nsITimer::TYPE_ONE_SHOT);
}

/* virtual */ void
MessageLoopIdleTask::Run()
{
  // Null out our pointers because if Run() was called by the timer, this
  // object will be kept alive by the MessageLoop until the MessageLoop calls
  // Run().

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  if (mTask) {
    mTask->Run();
    mTask = nullptr;
  }
}

MessageLoopTimerCallback::MessageLoopTimerCallback(MessageLoopIdleTask* aTask)
  : mTask(aTask->asWeakPtr())
{
}

NS_IMETHODIMP
MessageLoopTimerCallback::Notify(nsITimer* aTimer)
{
  // We don't expect to hit the case when the timer fires but mTask has been
  // deleted, because mTask should cancel the timer before the mTask is
  // deleted.  But you never know...
  NS_WARN_IF_FALSE(mTask, "This timer shouldn't have fired.");

  if (mTask) {
    mTask->Run();
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(MessageLoopTimerCallback, nsITimerCallback)

} // anonymous namespace

NS_IMPL_ISUPPORTS(nsMessageLoop, nsIMessageLoop)

NS_IMETHODIMP
nsMessageLoop::PostIdleTask(nsIRunnable* aTask, uint32_t aEnsureRunsAfterMS)
{
  // The message loop owns MessageLoopIdleTask and deletes it after calling
  // Run().  Be careful...
  MessageLoop::current()->PostIdleTask(FROM_HERE,
    new MessageLoopIdleTask(aTask, aEnsureRunsAfterMS));
  return NS_OK;
}

nsresult
nsMessageLoopConstructor(nsISupports* aOuter,
                         const nsIID& aIID,
                         void** aInstancePtr)
{
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsISupports* messageLoop = new nsMessageLoop();
  return messageLoop->QueryInterface(aIID, aInstancePtr);
}
