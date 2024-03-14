/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "gtest/gtest.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/gtest/MozAssertions.h"

namespace TestAutoOwningEventTarget {

using namespace mozilla;

#ifdef MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED

namespace {

static MozExternalRefCountType GetRefCount(nsISupports* aSupports) {
  aSupports->AddRef();
  return aSupports->Release();
}

void CheckAutoOwningEventTarget(
    nsISerialEventTarget* aSerialEventTarget,
    const nsAutoOwningEventTarget& aAutoOwningEventTarget,
    const bool aIsCurrent) {
  ASSERT_TRUE(aSerialEventTarget);

  ASSERT_EQ(aAutoOwningEventTarget.IsCurrentThread(), aIsCurrent);

  {
    const auto refCountBefore = GetRefCount(aSerialEventTarget);

    {
      nsAutoOwningEventTarget copyConstructedEventTarget(
          aAutoOwningEventTarget);
      ASSERT_EQ(copyConstructedEventTarget.IsCurrentThread(), aIsCurrent);
    }

    const auto refCountAfter = GetRefCount(aSerialEventTarget);

    ASSERT_GE(refCountAfter, refCountBefore);
    ASSERT_EQ(refCountAfter - refCountBefore, 0u);
  }

  {
    const auto refCountBefore = GetRefCount(aSerialEventTarget);

    {
      nsAutoOwningEventTarget copyAssignedEventTarget;
      ASSERT_TRUE(copyAssignedEventTarget.IsCurrentThread());

      copyAssignedEventTarget = aAutoOwningEventTarget;
      ASSERT_EQ(copyAssignedEventTarget.IsCurrentThread(), aIsCurrent);
    }

    const auto refCountAfter = GetRefCount(aSerialEventTarget);

    ASSERT_GE(refCountAfter, refCountBefore);
    ASSERT_EQ(refCountAfter - refCountBefore, 0u);
  }
}

}  // namespace

TEST(TestAutoOwningEventTarget, Simple)
{
  {
    nsAutoOwningEventTarget autoOwningEventTarget;

    ASSERT_NO_FATAL_FAILURE(CheckAutoOwningEventTarget(
        GetCurrentSerialEventTarget(), autoOwningEventTarget,
        /* aIsCurrent */ true));
  }
}

TEST(TestAutoOwningEventTarget, TaskQueue)
{
  nsresult rv;
  nsCOMPtr<nsIEventTarget> threadPool =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  ASSERT_NS_SUCCEEDED(rv);

  auto taskQueue = TaskQueue::Create(threadPool.forget(), "TestTaskQueue",
                                     /* aSupportsTailDispatch */ false);

  nsAutoOwningEventTarget autoOwningEventTarget;

  // XXX This call can't be wrapped with ASSERT_NS_SUCCEEDED directly because
  // base-toolchains builds fail with: error: duplicate label
  // 'gtest_label_testnofatal_111'
  rv = SyncRunnable::DispatchToThread(
      taskQueue,
      NS_NewRunnableFunction(
          "TestRunnable", [taskQueue, &autoOwningEventTarget] {
            {
              ASSERT_NO_FATAL_FAILURE(CheckAutoOwningEventTarget(
                  taskQueue, autoOwningEventTarget, /* aIsCurrent */ false));
            }

            {
              nsAutoOwningEventTarget autoOwningEventTarget;

              ASSERT_NO_FATAL_FAILURE(CheckAutoOwningEventTarget(
                  taskQueue, autoOwningEventTarget, /* aIsCurrent */ true));
            }
          }));
  ASSERT_NS_SUCCEEDED(rv);
}

#endif

}  // namespace TestAutoOwningEventTarget
