/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIThreadManager.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Atomics.h"
#include "gtest/gtest.h"

using mozilla::Atomic;
using mozilla::Runnable;

class WaitCondition final : public nsINestedEventLoopCondition {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  WaitCondition(Atomic<uint32_t>& aCounter, uint32_t aMaxCount)
      : mCounter(aCounter), mMaxCount(aMaxCount) {}

  NS_IMETHODIMP IsDone(bool* aDone) override {
    *aDone = (mCounter == mMaxCount);
    return NS_OK;
  }

 private:
  ~WaitCondition() = default;

  Atomic<uint32_t>& mCounter;
  const uint32_t mMaxCount;
};

NS_IMPL_ISUPPORTS(WaitCondition, nsINestedEventLoopCondition)

class SpinRunnable final : public Runnable {
 public:
  explicit SpinRunnable(nsINestedEventLoopCondition* aCondition)
      : Runnable("SpinRunnable"), mCondition(aCondition), mResult(NS_OK) {}

  NS_IMETHODIMP Run() {
    nsCOMPtr<nsIThreadManager> threadMan =
        do_GetService("@mozilla.org/thread-manager;1");
    mResult = threadMan->SpinEventLoopUntil(mCondition);
    return NS_OK;
  }

  nsresult SpinLoopResult() { return mResult; }

 private:
  ~SpinRunnable() = default;

  nsCOMPtr<nsINestedEventLoopCondition> mCondition;
  Atomic<nsresult> mResult;
};

class CountRunnable final : public Runnable {
 public:
  explicit CountRunnable(Atomic<uint32_t>& aCounter)
      : Runnable("CountRunnable"), mCounter(aCounter) {}

  NS_IMETHODIMP Run() {
    mCounter++;
    return NS_OK;
  }

 private:
  Atomic<uint32_t>& mCounter;
};

TEST(ThreadManager, SpinEventLoopUntilSuccess)
{
  const uint32_t kRunnablesToDispatch = 100;
  nsresult rv;
  mozilla::Atomic<uint32_t> count(0);

  nsCOMPtr<nsINestedEventLoopCondition> condition =
      new WaitCondition(count, kRunnablesToDispatch);
  RefPtr<SpinRunnable> spinner = new SpinRunnable(condition);
  nsCOMPtr<nsIThread> thread;
  rv = NS_NewNamedThread("SpinEventLoop", getter_AddRefs(thread), spinner);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIRunnable> counter = new CountRunnable(count);
  for (uint32_t i = 0; i < kRunnablesToDispatch; ++i) {
    rv = thread->Dispatch(counter, NS_DISPATCH_NORMAL);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  rv = thread->Shutdown();
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(NS_SUCCEEDED(spinner->SpinLoopResult()));
}

class ErrorCondition final : public nsINestedEventLoopCondition {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ErrorCondition(Atomic<uint32_t>& aCounter, uint32_t aMaxCount)
      : mCounter(aCounter), mMaxCount(aMaxCount) {}

  NS_IMETHODIMP IsDone(bool* aDone) override {
    if (mCounter == mMaxCount) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    return NS_OK;
  }

 private:
  ~ErrorCondition() = default;

  Atomic<uint32_t>& mCounter;
  const uint32_t mMaxCount;
};

NS_IMPL_ISUPPORTS(ErrorCondition, nsINestedEventLoopCondition)

TEST(ThreadManager, SpinEventLoopUntilError)
{
  const uint32_t kRunnablesToDispatch = 100;
  nsresult rv;
  mozilla::Atomic<uint32_t> count(0);

  nsCOMPtr<nsINestedEventLoopCondition> condition =
      new ErrorCondition(count, kRunnablesToDispatch);
  RefPtr<SpinRunnable> spinner = new SpinRunnable(condition);
  nsCOMPtr<nsIThread> thread;
  rv = NS_NewNamedThread("SpinEventLoop", getter_AddRefs(thread), spinner);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIRunnable> counter = new CountRunnable(count);
  for (uint32_t i = 0; i < kRunnablesToDispatch; ++i) {
    rv = thread->Dispatch(counter, NS_DISPATCH_NORMAL);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  rv = thread->Shutdown();
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(NS_FAILED(spinner->SpinLoopResult()));
}
