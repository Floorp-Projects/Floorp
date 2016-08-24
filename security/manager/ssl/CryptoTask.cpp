/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CryptoTask.h"
#include "nsNSSComponent.h"

namespace mozilla {

CryptoTask::~CryptoTask()
{
  MOZ_ASSERT(mReleasedNSSResources);

  nsNSSShutDownPreventionLock lock;
  if (!isAlreadyShutDown()) {
    shutdown(ShutdownCalledFrom::Object);
  }
}

nsresult
CryptoTask::Dispatch(const nsACString& taskThreadName)
{
  MOZ_ASSERT(taskThreadName.Length() <= 15);

  // Ensure that NSS is initialized, since presumably CalculateResult
  // will use NSS functions
  if (!EnsureNSSInitializedChromeOrContent()) {
    return NS_ERROR_FAILURE;
  }

  // Can't add 'this' as the event to run, since mThread may not be set yet
  nsresult rv = NS_NewThread(getter_AddRefs(mThread), nullptr,
                             nsIThreadManager::DEFAULT_STACK_SIZE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_SetThreadName(mThread, taskThreadName);
  // Note: event must not null out mThread!
  return mThread->Dispatch(this, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
CryptoTask::Run()
{
  if (!NS_IsMainThread()) {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      mRv = NS_ERROR_NOT_AVAILABLE;
    } else {
      mRv = CalculateResult();
    }
    NS_DispatchToMainThread(this);
  } else {
    // back on the main thread

    // call ReleaseNSSResources now, before calling CallCallback, so that
    // CryptoTasks have consistent behavior regardless of whether NSS is shut
    // down between CalculateResult being called and CallCallback being called.
    if (!mReleasedNSSResources) {
      mReleasedNSSResources = true;
      ReleaseNSSResources();
    }

    CallCallback(mRv);

    // Not all uses of CryptoTask use a transient thread
    if (mThread) {
      // Don't leak threads!
      mThread->Shutdown(); // can't Shutdown from the thread itself, darn
      // Don't null out mThread!
      // See bug 999104.  We must hold a ref to the thread across Dispatch()
      // since the internal mThread ref could be released while processing
      // the Dispatch(), and Dispatch/PutEvent itself doesn't hold a ref; it
      // assumes the caller does.
    }
  }

  return NS_OK;
}

void
CryptoTask::virtualDestroyNSSReference()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "virtualDestroyNSSReference called off the main thread");
  if (!mReleasedNSSResources) {
    mReleasedNSSResources = true;
    ReleaseNSSResources();
  }
}

} // namespace mozilla
