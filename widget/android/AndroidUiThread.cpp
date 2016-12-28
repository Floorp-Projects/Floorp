/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidBridge.h"
#include "base/message_loop.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsThread.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"

using namespace mozilla;

namespace {

class AndroidUiThread;

StaticRefPtr<AndroidUiThread> sThread;
static MessageLoop* sMessageLoop;
static Monitor* sMessageLoopAccessMonitor;
static bool sInitialized = false;

/*
 * The AndroidUiThread is derived from nsThread so that nsIRunnable objects that get
 * dispatched may be intercepted. Only nsIRunnable objects that need to be synchronously
 * executed are passed into the nsThread to be queued. All other nsIRunnable object
 * are immediately dispatched to the Android UI thread via the AndroidBridge.
 * AndroidUiThread is derived from nsThread instead of being an nsIEventTarget
 * wrapper that contains an nsThread object because if nsIRunnable objects with a
 * delay were dispatch directly to an nsThread object, such as obtained from
 * nsThreadManager::GetCurrentThread(), the nsIRunnable could get stuck in the
 * nsThread nsIRunnable queue. This is due to the fact that Android controls the
 * event loop in the Android UI thread and has no knowledge of when the nsThread
 * needs to be drained.
*/

class AndroidUiThread : public nsThread
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  AndroidUiThread() : nsThread(nsThread::NOT_MAIN_THREAD, 0)
  {}

  nsresult Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags) override;
  nsresult DelayedDispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aDelayMs) override;

private:
  ~AndroidUiThread()
  {}
};

NS_IMPL_ISUPPORTS_INHERITED0(AndroidUiThread, nsThread)

NS_IMETHODIMP
AndroidUiThread::Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags)
{
  if (aFlags & NS_DISPATCH_SYNC) {
    return nsThread::Dispatch(Move(aEvent), aFlags);
  } else {
    AndroidBridge::Bridge()->PostTaskToUiThread(Move(aEvent), 0);
    return NS_OK;
  }
}

NS_IMETHODIMP
AndroidUiThread::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aDelayMs)
{
  AndroidBridge::Bridge()->PostTaskToUiThread(Move(aEvent), aDelayMs);
  return NS_OK;
}

static void
PumpEvents() {
  NS_ProcessPendingEvents(sThread.get());
}

class ThreadObserver : public nsIThreadObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADOBSERVER

  ThreadObserver()
  {}

private:
  virtual ~ThreadObserver()
  {}
};

NS_IMPL_ISUPPORTS(ThreadObserver, nsIThreadObserver)

NS_IMETHODIMP
ThreadObserver::OnDispatchedEvent(nsIThreadInternal *thread)
{
  AndroidBridge::Bridge()->PostTaskToUiThread(NS_NewRunnableFunction(&PumpEvents), 0);
  return NS_OK;
}

NS_IMETHODIMP
ThreadObserver::OnProcessNextEvent(nsIThreadInternal *thread, bool mayWait)
{
  return NS_OK;
}

NS_IMETHODIMP
ThreadObserver::AfterProcessNextEvent(nsIThreadInternal *thread, bool eventWasProcessed)
{
  return NS_OK;
}

class CreateOnUiThread : public Runnable {
public:
  CreateOnUiThread()
  {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(sMessageLoopAccessMonitor);
    MonitorAutoLock lock(*sMessageLoopAccessMonitor);

    sThread = new AndroidUiThread();
    sThread->InitCurrentThread();
    sThread->SetObserver(new ThreadObserver());
    sMessageLoop = new MessageLoop(MessageLoop::TYPE_MOZILLA_ANDROID_UI, sThread.get());
    lock.NotifyAll();
    return NS_OK;
  }
};

class DestroyOnUiThread : public Runnable {
public:
  DestroyOnUiThread() : mDestroyed(false)
  {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(sMessageLoopAccessMonitor);
    MonitorAutoLock lock(*sMessageLoopAccessMonitor);

    delete sMessageLoop;
    sMessageLoop = nullptr;
    MOZ_ASSERT(sThread);
    nsThreadManager::get().UnregisterCurrentThread(*sThread);
    sThread = nullptr;
    mDestroyed = true;
    lock.NotifyAll();
    return NS_OK;
  }

  void WaitForDestruction()
  {
    MOZ_ASSERT(sMessageLoopAccessMonitor);
    MonitorAutoLock lock(*sMessageLoopAccessMonitor);
    while (!mDestroyed) {
      lock.Wait();
    }
  }

private:
  bool mDestroyed;
};

} // namespace

namespace mozilla {

void
CreateAndroidUiThread()
{
  MOZ_ASSERT(!sInitialized);
  MOZ_ASSERT(!sThread);
  MOZ_ASSERT(!sMessageLoopAccessMonitor);
  sMessageLoopAccessMonitor = new Monitor("AndroidUiThreadMessageLoopAccessMonitor");
  RefPtr<CreateOnUiThread> runnable = new CreateOnUiThread;
  AndroidBridge::Bridge()->PostTaskToUiThread(do_AddRef(runnable), 0);
  sInitialized = true;
}

void
DestroyAndroidUiThread()
{
  MOZ_ASSERT(sInitialized);
  MOZ_ASSERT(sThread);
  // Insure the Android bridge has not already been deconstructed.
  MOZ_ASSERT(AndroidBridge::Bridge() != nullptr);
  sInitialized = false;
  RefPtr<DestroyOnUiThread> runnable = new DestroyOnUiThread;
  AndroidBridge::Bridge()->PostTaskToUiThread(do_AddRef(runnable), 0);
  runnable->WaitForDestruction();
  delete sMessageLoopAccessMonitor;
  sMessageLoopAccessMonitor = nullptr;
}

MessageLoop*
GetAndroidUiThreadMessageLoop()
{
  if (!sInitialized) {
    return nullptr;
  }

  if (!sMessageLoop) {
    MOZ_ASSERT(sMessageLoopAccessMonitor);
    MonitorAutoLock lock(*sMessageLoopAccessMonitor);
    while (!sMessageLoop) {
      lock.Wait();
    }
  }

  return sMessageLoop;
}

RefPtr<nsThread>
GetAndroidUiThread()
{
  if (!sInitialized) {
    return nullptr;
  }

  if (!sThread) {
    MOZ_ASSERT(sMessageLoopAccessMonitor);
    MonitorAutoLock lock(*sMessageLoopAccessMonitor);
    while (!sThread) {
      lock.Wait();
    }
  }

  return sThread;
}

} // namespace mozilla
