/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadSyncDispatch_h_
#define nsThreadSyncDispatch_h_

#include "nsThreadUtils.h"
#include "LeakRefPtr.h"

class nsThreadSyncDispatch : public nsRunnable
{
public:
  nsThreadSyncDispatch(nsIThread* aOrigin, already_AddRefed<nsIRunnable>&& aTask)
    : mOrigin(aOrigin)
    , mSyncTask(mozilla::Move(aTask))
    , mResult(NS_ERROR_NOT_INITIALIZED)
  {
  }

  bool IsPending()
  {
    return !!mSyncTask;
  }

  nsresult Result()
  {
    return mResult;
  }

private:
  NS_IMETHOD Run() override
  {
    if (nsCOMPtr<nsIRunnable> task = mSyncTask.take()) {
      mResult = task->Run();
      // unblock the origin thread
      mOrigin->Dispatch(this, NS_DISPATCH_NORMAL);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIThread> mOrigin;
  // The task is leaked by default when Run() is not called, because
  // otherwise we may release it in an incorrect thread.
  mozilla::LeakRefPtr<nsIRunnable> mSyncTask;
  nsresult mResult;
};

#endif // nsThreadSyncDispatch_h_
