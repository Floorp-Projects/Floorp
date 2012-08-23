/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIThread.h"
#include "nsITimer.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "prinrval.h"
#include "prmon.h"
#include "mozilla/Attributes.h"

#include "mozilla/ReentrantMonitor.h"
using namespace mozilla;

typedef nsresult(*TestFuncPtr)();

class AutoTestThread
{
public:
  AutoTestThread() {
    nsCOMPtr<nsIThread> newThread;
    nsresult rv = NS_NewThread(getter_AddRefs(newThread));
    if (NS_FAILED(rv))
      return;

    newThread.swap(mThread);
  }

  ~AutoTestThread() {
    mThread->Shutdown();
  }

  operator nsIThread*() const {
    return mThread;
  }

  nsIThread* operator->() const {
    return mThread;
  }

private:
  nsCOMPtr<nsIThread> mThread;
};

class AutoCreateAndDestroyReentrantMonitor
{
public:
  AutoCreateAndDestroyReentrantMonitor() {
    mReentrantMonitor = new ReentrantMonitor("TestTimers::AutoMon");
    NS_ASSERTION(mReentrantMonitor, "Out of memory!");
  }

  ~AutoCreateAndDestroyReentrantMonitor() {
    delete mReentrantMonitor;
  }

  operator ReentrantMonitor* () {
    return mReentrantMonitor;
  }

private:
  ReentrantMonitor* mReentrantMonitor;
};

class TimerCallback MOZ_FINAL : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  TimerCallback(nsIThread** aThreadPtr, ReentrantMonitor* aReentrantMonitor)
  : mThreadPtr(aThreadPtr), mReentrantMonitor(aReentrantMonitor) { }

  NS_IMETHOD Notify(nsITimer* aTimer) {
    nsCOMPtr<nsIThread> current(do_GetCurrentThread());

    ReentrantMonitorAutoEnter mon(*mReentrantMonitor);

    NS_ASSERTION(!*mThreadPtr, "Timer called back more than once!");
    *mThreadPtr = current;

    mon.Notify();

    return NS_OK;
  }
private:
  nsIThread** mThreadPtr;
  ReentrantMonitor* mReentrantMonitor;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(TimerCallback, nsITimerCallback)

nsresult
TestTargetedTimers()
{
  AutoCreateAndDestroyReentrantMonitor newMon;
  NS_ENSURE_TRUE(newMon, NS_ERROR_OUT_OF_MEMORY);

  AutoTestThread testThread;
  NS_ENSURE_TRUE(testThread, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIEventTarget* target = static_cast<nsIEventTarget*>(testThread);

  rv = timer->SetTarget(target);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIThread* notifiedThread = nullptr;

  nsCOMPtr<nsITimerCallback> callback =
    new TimerCallback(&notifiedThread, newMon);
  NS_ENSURE_TRUE(callback, NS_ERROR_OUT_OF_MEMORY);

  rv = timer->InitWithCallback(callback, PR_MillisecondsToInterval(2000),
                               nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  ReentrantMonitorAutoEnter mon(*newMon);
  while (!notifiedThread) {
    mon.Wait();
  }
  NS_ENSURE_TRUE(notifiedThread == testThread, NS_ERROR_FAILURE);

  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("TestTimers");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  static TestFuncPtr testsToRun[] = {
    TestTargetedTimers
  };
  static uint32_t testCount = sizeof(testsToRun) / sizeof(testsToRun[0]);

  for (uint32_t i = 0; i < testCount; i++) {
    nsresult rv = testsToRun[i]();
    NS_ENSURE_SUCCESS(rv, 1);
  }

  return 0;
}
