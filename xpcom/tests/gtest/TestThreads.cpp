/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsXPCOM.h"
#include "mozilla/Monitor.h"
#include "gtest/gtest.h"

using namespace mozilla;

class nsRunner final : public Runnable {
  ~nsRunner() = default;

 public:
  NS_IMETHOD Run() override {
    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_GetCurrentThread(getter_AddRefs(thread));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    printf("running %d on thread %p\n", mNum, (void*)thread.get());

    // if we don't do something slow, we'll never see the other
    // worker threads run
    PR_Sleep(PR_MillisecondsToInterval(100));

    return rv;
  }

  explicit nsRunner(int num) : Runnable("nsRunner"), mNum(num) {}

 protected:
  int mNum;
};

TEST(Threads, Main)
{
  nsresult rv;

  nsCOMPtr<nsIRunnable> event = new nsRunner(0);
  EXPECT_TRUE(event);

  nsCOMPtr<nsIThread> runner;
  rv = NS_NewNamedThread("TestThreadsMain", getter_AddRefs(runner), event);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIThread> thread;
  rv = NS_GetCurrentThread(getter_AddRefs(thread));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = runner->Shutdown();  // wait for the runner to die before quitting
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  PR_Sleep(
      PR_MillisecondsToInterval(100));  // hopefully the runner will quit here
}

class nsStressRunner final : public Runnable {
 public:
  NS_IMETHOD Run() override {
    EXPECT_FALSE(mWasRun);
    mWasRun = true;
    PR_Sleep(1);
    if (!PR_AtomicDecrement(&gNum)) {
      printf("   last thread was %d\n", mNum);
    }
    return NS_OK;
  }

  explicit nsStressRunner(int num)
      : Runnable("nsStressRunner"), mNum(num), mWasRun(false) {
    PR_AtomicIncrement(&gNum);
  }

  static int32_t GetGlobalCount() { return gNum; }

 private:
  ~nsStressRunner() { EXPECT_TRUE(mWasRun); }

 protected:
  static int32_t gNum;
  int32_t mNum;
  bool mWasRun;
};

int32_t nsStressRunner::gNum = 0;

TEST(Threads, Stress)
{
#if defined(XP_WIN) && defined(MOZ_ASAN)  // Easily hits OOM
  const int loops = 250;
#else
  const int loops = 1000;
#endif

  const int threads = 50;

  for (int i = 0; i < loops; i++) {
    printf("Loop %d of %d\n", i + 1, loops);

    int k;
    nsIThread** array = new nsIThread*[threads];

    EXPECT_EQ(nsStressRunner::GetGlobalCount(), 0);

    for (k = 0; k < threads; k++) {
      nsCOMPtr<nsIThread> t;
      nsresult rv = NS_NewNamedThread("StressRunner", getter_AddRefs(t),
                                      new nsStressRunner(k));
      EXPECT_TRUE(NS_SUCCEEDED(rv));
      NS_ADDREF(array[k] = t);
    }

    for (k = threads - 1; k >= 0; k--) {
      array[k]->Shutdown();
      NS_RELEASE(array[k]);
    }
    delete[] array;
  }
}

mozilla::Monitor* gAsyncShutdownReadyMonitor;
mozilla::Monitor* gBeginAsyncShutdownMonitor;

class AsyncShutdownPreparer : public Runnable {
 public:
  NS_IMETHOD Run() override {
    EXPECT_FALSE(mWasRun);
    mWasRun = true;

    mozilla::MonitorAutoLock lock(*gAsyncShutdownReadyMonitor);
    lock.Notify();

    return NS_OK;
  }

  explicit AsyncShutdownPreparer()
      : Runnable("AsyncShutdownPreparer"), mWasRun(false) {}

 private:
  virtual ~AsyncShutdownPreparer() { EXPECT_TRUE(mWasRun); }

 protected:
  bool mWasRun;
};

class AsyncShutdownWaiter : public Runnable {
 public:
  NS_IMETHOD Run() override {
    EXPECT_FALSE(mWasRun);
    mWasRun = true;

    nsCOMPtr<nsIThread> t;
    nsresult rv;

    {
      mozilla::MonitorAutoLock lock(*gBeginAsyncShutdownMonitor);

      rv = NS_NewNamedThread("AsyncShutdownPr", getter_AddRefs(t),
                             new AsyncShutdownPreparer());
      EXPECT_TRUE(NS_SUCCEEDED(rv));

      lock.Wait();
    }

    rv = t->AsyncShutdown();
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    return NS_OK;
  }

  explicit AsyncShutdownWaiter()
      : Runnable("AsyncShutdownWaiter"), mWasRun(false) {}

 private:
  virtual ~AsyncShutdownWaiter() { EXPECT_TRUE(mWasRun); }

 protected:
  bool mWasRun;
};

class SameThreadSentinel : public Runnable {
 public:
  NS_IMETHOD Run() override {
    mozilla::MonitorAutoLock lock(*gBeginAsyncShutdownMonitor);
    lock.Notify();
    return NS_OK;
  }

  SameThreadSentinel() : Runnable("SameThreadSentinel") {}

 private:
  virtual ~SameThreadSentinel() = default;
};

TEST(Threads, AsyncShutdown)
{
  gAsyncShutdownReadyMonitor = new mozilla::Monitor("gAsyncShutdownReady");
  gBeginAsyncShutdownMonitor = new mozilla::Monitor("gBeginAsyncShutdown");

  nsCOMPtr<nsIThread> t;
  nsresult rv;

  {
    mozilla::MonitorAutoLock lock(*gAsyncShutdownReadyMonitor);

    rv = NS_NewNamedThread("AsyncShutdownWt", getter_AddRefs(t),
                           new AsyncShutdownWaiter());
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    lock.Wait();
  }

  NS_DispatchToCurrentThread(new SameThreadSentinel());
  rv = t->Shutdown();
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  delete gAsyncShutdownReadyMonitor;
  delete gBeginAsyncShutdownMonitor;
}

static void threadProc(void* arg) {
  // printf("   running thread %d\n", (int) arg);
  PR_Sleep(1);
  EXPECT_EQ(PR_JOINABLE_THREAD, PR_GetThreadState(PR_GetCurrentThread()));
}

TEST(Threads, StressNSPR)
{
#if defined(XP_WIN) && defined(MOZ_ASAN)  // Easily hits OOM
  const int loops = 250;
#else
  const int loops = 1000;
#endif

  const int threads = 50;

  for (int i = 0; i < loops; i++) {
    printf("Loop %d of %d\n", i + 1, loops);

    intptr_t k;
    PRThread** array = new PRThread*[threads];

    for (k = 0; k < threads; k++) {
      array[k] = PR_CreateThread(PR_USER_THREAD, threadProc, (void*)k,
                                 PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD, 0);
      EXPECT_TRUE(array[k]);
    }

    for (k = 0; k < threads; k++) {
      EXPECT_EQ(PR_JOINABLE_THREAD, PR_GetThreadState(array[k]));
    }

    for (k = threads - 1; k >= 0; k--) {
      PR_JoinThread(array[k]);
    }
    delete[] array;
  }
}

TEST(Threads, GetCurrentSerialEventTarget)
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv =
      NS_NewNamedThread("Testing Thread", getter_AddRefs(thread),
                        NS_NewRunnableFunction("Testing Thread::check", []() {
                          nsCOMPtr<nsISerialEventTarget> serialEventTarget =
                              GetCurrentSerialEventTarget();
                          nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();
                          EXPECT_EQ(thread.get(), serialEventTarget.get());
                        }));
  MOZ_ALWAYS_SUCCEEDS(rv);
  thread->Shutdown();
}
