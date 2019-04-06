/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIThread.h"

#include "nsComponentManagerUtils.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "pratom.h"
#include "prinrval.h"
#include "prmon.h"
#include "prthread.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include "mozilla/ReentrantMonitor.h"

#include "gtest/gtest.h"

using namespace mozilla;

#define NUMBER_OF_THREADS 4

// One hour... because test boxes can be slow!
#define IDLE_THREAD_TIMEOUT 3600000

namespace TestThreadPoolListener {
static nsIThread** gCreatedThreadList = nullptr;
static nsIThread** gShutDownThreadList = nullptr;

static ReentrantMonitor* gReentrantMonitor = nullptr;

static bool gAllRunnablesPosted = false;
static bool gAllThreadsCreated = false;
static bool gAllThreadsShutDown = false;

class Listener final : public nsIThreadPoolListener {
  ~Listener() {}

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADPOOLLISTENER
};

NS_IMPL_ISUPPORTS(Listener, nsIThreadPoolListener)

NS_IMETHODIMP
Listener::OnThreadCreated() {
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  EXPECT_TRUE(current) << "Couldn't get current thread!";

  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

  while (!gAllRunnablesPosted) {
    mon.Wait();
  }

  for (uint32_t i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* thread = gCreatedThreadList[i];
    EXPECT_NE(thread, current) << "Saw the same thread twice!";

    if (!thread) {
      gCreatedThreadList[i] = current;
      if (i == (NUMBER_OF_THREADS - 1)) {
        gAllThreadsCreated = true;
        mon.NotifyAll();
      }
      return NS_OK;
    }
  }

  EXPECT_TRUE(false) << "Too many threads!";
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Listener::OnThreadShuttingDown() {
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  EXPECT_TRUE(current) << "Couldn't get current thread!";

  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

  for (uint32_t i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* thread = gShutDownThreadList[i];
    EXPECT_NE(thread, current) << "Saw the same thread twice!";

    if (!thread) {
      gShutDownThreadList[i] = current;
      if (i == (NUMBER_OF_THREADS - 1)) {
        gAllThreadsShutDown = true;
        mon.NotifyAll();
      }
      return NS_OK;
    }
  }

  EXPECT_TRUE(false) << "Too many threads!";
  return NS_ERROR_FAILURE;
}

class AutoCreateAndDestroyReentrantMonitor {
 public:
  explicit AutoCreateAndDestroyReentrantMonitor(
      ReentrantMonitor** aReentrantMonitorPtr)
      : mReentrantMonitorPtr(aReentrantMonitorPtr) {
    *aReentrantMonitorPtr =
        new ReentrantMonitor("TestThreadPoolListener::AutoMon");
    MOZ_RELEASE_ASSERT(*aReentrantMonitorPtr, "Out of memory!");
  }

  ~AutoCreateAndDestroyReentrantMonitor() {
    delete *mReentrantMonitorPtr;
    *mReentrantMonitorPtr = nullptr;
  }

 private:
  ReentrantMonitor** mReentrantMonitorPtr;
};

TEST(ThreadPoolListener, Test)
{
  nsIThread* createdThreadList[NUMBER_OF_THREADS] = {nullptr};
  gCreatedThreadList = createdThreadList;

  nsIThread* shutDownThreadList[NUMBER_OF_THREADS] = {nullptr};
  gShutDownThreadList = shutDownThreadList;

  AutoCreateAndDestroyReentrantMonitor newMon(&gReentrantMonitor);
  ASSERT_TRUE(gReentrantMonitor);

  nsresult rv;

  nsCOMPtr<nsIThreadPool> pool = new nsThreadPool();

  rv = pool->SetThreadLimit(NUMBER_OF_THREADS);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = pool->SetIdleThreadLimit(NUMBER_OF_THREADS);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = pool->SetIdleThreadTimeout(IDLE_THREAD_TIMEOUT);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIThreadPoolListener> listener = new Listener();
  ASSERT_TRUE(listener);

  rv = pool->SetListener(listener);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    for (uint32_t i = 0; i < NUMBER_OF_THREADS; i++) {
      nsCOMPtr<nsIRunnable> runnable = new Runnable("TestRunnable");
      ASSERT_TRUE(runnable);

      rv = pool->Dispatch(runnable, NS_DISPATCH_NORMAL);
      ASSERT_TRUE(NS_SUCCEEDED(rv));
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
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);
    while (!gAllThreadsShutDown) {
      mon.Wait();
    }
  }

  for (uint32_t i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* created = gCreatedThreadList[i];
    ASSERT_TRUE(created);

    bool match = false;
    for (uint32_t j = 0; j < NUMBER_OF_THREADS; j++) {
      nsIThread* destroyed = gShutDownThreadList[j];
      ASSERT_TRUE(destroyed);

      if (destroyed == created) {
        match = true;
        break;
      }
    }

    ASSERT_TRUE(match);
  }
}

}  // namespace TestThreadPoolListener
