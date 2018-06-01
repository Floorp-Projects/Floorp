/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadSyncDispatch_h_
#define nsThreadSyncDispatch_h_

#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"

#include "nsThreadUtils.h"
#include "LeakRefPtr.h"

class nsThreadSyncDispatch : public mozilla::Runnable
{
public:
  nsThreadSyncDispatch(already_AddRefed<nsIEventTarget> aOrigin, already_AddRefed<nsIRunnable>&& aTask)
    : Runnable("nsThreadSyncDispatch")
    , mOrigin(aOrigin)
    , mSyncTask(std::move(aTask))
    , mIsPending(true)
  {
  }

  bool IsPending()
  {
    // This is an atomic acquire on the origin thread.
    return mIsPending;
  }

private:
  NS_IMETHOD Run() override
  {
    if (nsCOMPtr<nsIRunnable> task = mSyncTask.take()) {
      MOZ_ASSERT(!mSyncTask);

      mozilla::DebugOnly<nsresult> result = task->Run();
      MOZ_ASSERT(NS_SUCCEEDED(result),
                 "task in sync dispatch should not fail");

      // We must release the task here to ensure that when the original
      // thread is unblocked, this task has been released.
      task = nullptr;

      // This is an atomic release on the target thread.
      mIsPending = false;

      // unblock the origin thread
      mOrigin->Dispatch(this, NS_DISPATCH_NORMAL);
    }

    return NS_OK;
  }

  nsCOMPtr<nsIEventTarget> mOrigin;
  // The task is leaked by default when Run() is not called, because
  // otherwise we may release it in an incorrect thread.
  mozilla::LeakRefPtr<nsIRunnable> mSyncTask;
  mozilla::Atomic<bool, mozilla::ReleaseAcquire> mIsPending;
};

#endif // nsThreadSyncDispatch_h_
