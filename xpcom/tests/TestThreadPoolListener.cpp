/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIThread.h"
#include "nsIThreadPool.h"

#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "pratom.h"
#include "prinrval.h"
#include "prmon.h"
#include "prthread.h"

#include "mozilla/ReentrantMonitor.h"
using namespace mozilla;

#define NUMBER_OF_THREADS 4

// One hour... because test boxes can be slow!
#define IDLE_THREAD_TIMEOUT 3600000

static nsIThread** gCreatedThreadList = nsnull;
static nsIThread** gShutDownThreadList = nsnull;

static ReentrantMonitor* gReentrantMonitor = nsnull;

static bool gAllRunnablesPosted = false;
static bool gAllThreadsCreated = false;
static bool gAllThreadsShutDown = false;

#ifdef DEBUG
#define TEST_ASSERTION(_test, _msg) \
    NS_ASSERTION(_test, _msg);
#else
#define TEST_ASSERTION(_test, _msg) \
  PR_BEGIN_MACRO \
    if (!(_test)) { \
      NS_DebugBreak(NS_DEBUG_ABORT, _msg, #_test, __FILE__, __LINE__); \
    } \
  PR_END_MACRO
#endif

class Listener : public nsIThreadPoolListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADPOOLLISTENER
};

NS_IMPL_THREADSAFE_ISUPPORTS1(Listener, nsIThreadPoolListener)

NS_IMETHODIMP
Listener::OnThreadCreated()
{
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  TEST_ASSERTION(current, "Couldn't get current thread!");

  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

  while (!gAllRunnablesPosted) {
    mon.Wait();
  }

  for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* thread = gCreatedThreadList[i];
    TEST_ASSERTION(thread != current, "Saw the same thread twice!");

    if (!thread) {
      gCreatedThreadList[i] = current;
      if (i == (NUMBER_OF_THREADS - 1)) {
        gAllThreadsCreated = true;
        mon.NotifyAll();
      }
      return NS_OK;
    }
  }

  TEST_ASSERTION(false, "Too many threads!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Listener::OnThreadShuttingDown()
{
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  TEST_ASSERTION(current, "Couldn't get current thread!");

  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

  for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* thread = gShutDownThreadList[i];
    TEST_ASSERTION(thread != current, "Saw the same thread twice!");

    if (!thread) {
      gShutDownThreadList[i] = current;
      if (i == (NUMBER_OF_THREADS - 1)) {
        gAllThreadsShutDown = true;
        mon.NotifyAll();
      }
      return NS_OK;
    }
  }

  TEST_ASSERTION(false, "Too many threads!");
  return NS_ERROR_FAILURE;
}

class AutoCreateAndDestroyReentrantMonitor
{
public:
  AutoCreateAndDestroyReentrantMonitor(ReentrantMonitor** aReentrantMonitorPtr)
  : mReentrantMonitorPtr(aReentrantMonitorPtr) {
    *aReentrantMonitorPtr = new ReentrantMonitor("TestThreadPoolListener::AutoMon");
    TEST_ASSERTION(*aReentrantMonitorPtr, "Out of memory!");
  }

  ~AutoCreateAndDestroyReentrantMonitor() {
    if (*mReentrantMonitorPtr) {
      delete *mReentrantMonitorPtr;
      *mReentrantMonitorPtr = nsnull;
    }
  }

private:
  ReentrantMonitor** mReentrantMonitorPtr;
};

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("ThreadPoolListener");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  nsIThread* createdThreadList[NUMBER_OF_THREADS] = { nsnull };
  gCreatedThreadList = createdThreadList;

  nsIThread* shutDownThreadList[NUMBER_OF_THREADS] = { nsnull };
  gShutDownThreadList = shutDownThreadList;

  AutoCreateAndDestroyReentrantMonitor newMon(&gReentrantMonitor);
  NS_ENSURE_TRUE(gReentrantMonitor, 1);

  nsresult rv;

  nsCOMPtr<nsIThreadPool> pool =
    do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, 1);

  rv = pool->SetThreadLimit(NUMBER_OF_THREADS);
  NS_ENSURE_SUCCESS(rv, 1);

  rv = pool->SetIdleThreadLimit(NUMBER_OF_THREADS);
  NS_ENSURE_SUCCESS(rv, 1);

  rv = pool->SetIdleThreadTimeout(IDLE_THREAD_TIMEOUT);
  NS_ENSURE_SUCCESS(rv, 1);

  nsCOMPtr<nsIThreadPoolListener> listener = new Listener();
  NS_ENSURE_TRUE(listener, 1);

  rv = pool->SetListener(listener);
  NS_ENSURE_SUCCESS(rv, 1);

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
      nsCOMPtr<nsIRunnable> runnable = new nsRunnable();
      NS_ENSURE_TRUE(runnable, 1);

      rv = pool->Dispatch(runnable, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, 1);
    }

    gAllRunnablesPosted = true;
    mon.NotifyAll();
  }

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);
    while (!gAllThreadsCreated) {
      mon.Wait();
    }
  }

  rv = pool->Shutdown();
  NS_ENSURE_SUCCESS(rv, 1);

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);
    while (!gAllThreadsShutDown) {
      mon.Wait();
    }
  }

  for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* created = gCreatedThreadList[i];
    NS_ENSURE_TRUE(created, 1);

    bool match = false;
    for (PRUint32 j = 0; j < NUMBER_OF_THREADS; j++) {
      nsIThread* destroyed = gShutDownThreadList[j];
      NS_ENSURE_TRUE(destroyed, 1);

      if (destroyed == created) {
        match = true;
        break;
      }
    }

    NS_ENSURE_TRUE(match, 1);
  }

  return 0;
}
