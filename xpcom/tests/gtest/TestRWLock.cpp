/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/RWLock.h"
#include "mozilla/SyncRunnable.h"
#include "nsIThread.h"
#include "gtest/gtest.h"

using mozilla::AutoReadLock;
using mozilla::AutoTryReadLock;
using mozilla::AutoTryWriteLock;
using mozilla::AutoWriteLock;
using mozilla::RWLock;

static const size_t sNumThreads = 4;
static const size_t sOuterIterations = 100;
static const size_t sInnerIterations = 100;
static const size_t sWriteLockIteration = 10;

// Based on example code from _Programming with POSIX Threads_.  Not an actual
// test of correctness, but more of a "does this work at all" sort of test.

class RWLockRunnable : public mozilla::Runnable {
 public:
  RWLockRunnable(RWLock* aRWLock, mozilla::Atomic<size_t>* aSharedData)
      : mozilla::Runnable("RWLockRunnable"),
        mRWLock(aRWLock),
        mSharedData(aSharedData) {}

  NS_DECL_NSIRUNNABLE

 private:
  ~RWLockRunnable() = default;

  RWLock* mRWLock;
  mozilla::Atomic<size_t>* mSharedData;
};

NS_IMETHODIMP
RWLockRunnable::Run() {
  for (size_t i = 0; i < sOuterIterations; ++i) {
    if (i % sWriteLockIteration == 0) {
      mozilla::AutoWriteLock lock(*mRWLock);

      ++(*mSharedData);
    } else {
      mozilla::AutoReadLock lock(*mRWLock);

      // Loop and try to force other threads to run, but check that our
      // shared data isn't being modified by them.
      size_t initialValue = *mSharedData;
      for (size_t j = 0; j < sInnerIterations; ++j) {
        EXPECT_EQ(initialValue, *mSharedData);

        // This is a magic yield call.
        PR_Sleep(PR_INTERVAL_NO_WAIT);
      }
    }
  }

  return NS_OK;
}

TEST(RWLock, SmokeTest)
{
  nsCOMPtr<nsIThread> threads[sNumThreads];
  RWLock rwlock MOZ_UNANNOTATED("test lock");
  mozilla::Atomic<size_t> data(0);

  for (size_t i = 0; i < sNumThreads; ++i) {
    nsCOMPtr<nsIRunnable> event = new RWLockRunnable(&rwlock, &data);
    NS_NewNamedThread("RWLockTester", getter_AddRefs(threads[i]), event);
  }

  // Wait for all the threads to finish.
  for (size_t i = 0; i < sNumThreads; ++i) {
    nsresult rv = threads[i]->Shutdown();
    EXPECT_NS_SUCCEEDED(rv);
  }

  EXPECT_EQ(data, (sOuterIterations / sWriteLockIteration) * sNumThreads);
}

template <typename Function>
static std::invoke_result_t<Function> RunOnBackgroundThread(
    Function&& aFunction) {
  using Result = std::invoke_result_t<Function>;
  nsCOMPtr<nsISerialEventTarget> thread;
  MOZ_ALWAYS_SUCCEEDS(NS_CreateBackgroundTaskQueue(
      "TestRWLock Background Thread", getter_AddRefs(thread)));
  mozilla::Maybe<Result> tryResult;
  RefPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction(__func__, [&] { tryResult.emplace(aFunction()); });
  MOZ_ALWAYS_SUCCEEDS(
      mozilla::SyncRunnable::DispatchToThread(thread.get(), runnable));
  return *tryResult;
}

TEST(RWLock, AutoTryReadLock)
{
  RWLock l1 MOZ_UNANNOTATED("autotryreadlock");
  {
    AutoTryReadLock autol1(l1);

    EXPECT_TRUE(autol1);

    AutoTryReadLock autol2(l1);
    EXPECT_TRUE(autol2);

    EXPECT_TRUE(RunOnBackgroundThread([&] {
      AutoTryReadLock lock(l1);
      return !!lock;
    }));

    EXPECT_TRUE(autol1);
    EXPECT_TRUE(autol2);

    {
      RWLock l2 MOZ_UNANNOTATED("autotryreadlock2");
      AutoTryReadLock autol3(l2);

      EXPECT_TRUE(autol3);
    }

    EXPECT_TRUE(autol1);
    EXPECT_TRUE(autol2);
  }

  {
    AutoWriteLock autol4(l1);
    MOZ_ASSERT(l1.LockedForWritingByCurrentThread());

    AutoTryReadLock autol5(l1);
    EXPECT_FALSE(autol5);

    EXPECT_FALSE(RunOnBackgroundThread([&] {
      AutoTryReadLock lock(l1);
      return !!lock;
    }));
  }

  AutoTryReadLock autol6(l1);
  EXPECT_TRUE(autol6);

  EXPECT_TRUE(RunOnBackgroundThread([&] {
    AutoTryReadLock lock(l1);
    return !!lock;
  }));
}

TEST(RWLock, AutoTryWriteLock)
{
  RWLock l1 MOZ_UNANNOTATED("autotrywritelock");
  {
    AutoTryWriteLock autol1(l1);

    EXPECT_TRUE(autol1);

    AutoTryReadLock autol2(l1);
    EXPECT_FALSE(autol2);

    EXPECT_FALSE(RunOnBackgroundThread([&] {
      AutoTryWriteLock lock(l1);
      return !!lock;
    }));

    EXPECT_TRUE(autol1);
    EXPECT_FALSE(autol2);

    {
      RWLock l2 MOZ_UNANNOTATED("autotrywritelock2");
      AutoTryWriteLock autol3(l2);

      EXPECT_TRUE(autol3);
    }

    EXPECT_TRUE(autol1);
    EXPECT_FALSE(autol2);
  }

  {
    AutoReadLock autol4(l1);

    AutoTryWriteLock autol5(l1);
    EXPECT_FALSE(autol5);

    EXPECT_FALSE(RunOnBackgroundThread([&] {
      AutoTryWriteLock lock(l1);
      return !!lock;
    }));
  }

  {
    AutoWriteLock autol6(l1);
    MOZ_ASSERT(l1.LockedForWritingByCurrentThread());

    AutoTryWriteLock autol7(l1);
    EXPECT_FALSE(autol7);

    EXPECT_FALSE(RunOnBackgroundThread([&] {
      AutoTryWriteLock lock(l1);
      return !!lock;
    }));
  }

  AutoTryWriteLock autol8(l1);
  EXPECT_TRUE(autol8);
}
