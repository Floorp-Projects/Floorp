/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIThreadPool.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "mozilla/Atomics.h"
#include "gtest/gtest.h"

class Task MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit Task(int i) : mIndex(i) {}

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    printf("###(%d) running from thread: %p\n", mIndex, (void *) PR_GetCurrentThread());
    int r = (int) ((float) rand() * 200 / RAND_MAX);
    PR_Sleep(PR_MillisecondsToInterval(r));
    printf("###(%d) exiting from thread: %p\n", mIndex, (void *) PR_GetCurrentThread());
    ++sCount;
    return NS_OK;
  }

  static mozilla::Atomic<int> sCount;

private:
  ~Task() {}

  int mIndex;
};
NS_IMPL_ISUPPORTS(Task, nsIRunnable)

mozilla::Atomic<int> Task::sCount;

TEST(ThreadPool, Main)
{
  nsCOMPtr<nsIThreadPool> pool = do_CreateInstance(NS_THREADPOOL_CONTRACTID);
  EXPECT_TRUE(pool);

  for (int i = 0; i < 100; ++i) {
    nsCOMPtr<nsIRunnable> task = new Task(i);
    EXPECT_TRUE(task);

    pool->Dispatch(task, NS_DISPATCH_NORMAL);
  }

  pool->Shutdown();
  EXPECT_EQ(Task::sCount, 100);
}
