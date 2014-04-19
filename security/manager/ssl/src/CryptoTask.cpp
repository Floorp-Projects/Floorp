/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CryptoTask.h"

namespace mozilla {

CryptoTask::~CryptoTask()
{
  MOZ_ASSERT(mReleasedNSSResources);

  nsNSSShutDownPreventionLock lock;
  if (!isAlreadyShutDown()) {
    shutdown(calledFromObject);
  }
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
      mThread = nullptr;
    }
  }

  return NS_OK;
}

void
CryptoTask::virtualDestroyNSSReference()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(),
                    "virtualDestroyNSSReference called off the main thread");
  if (!mReleasedNSSResources) {
    mReleasedNSSResources = true;
    ReleaseNSSResources();
  }
}

} // namespace mozilla
