/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CryptoTask.h"
#include "nsNSSComponent.h"
#include "nsNetCID.h"

namespace mozilla {

nsresult CryptoTask::Dispatch() {
  // Ensure that NSS is initialized, since presumably CalculateResult
  // will use NSS functions
  if (!EnsureNSSInitializedChromeOrContent()) {
    return NS_ERROR_FAILURE;
  }

  // The stream transport service (note: not the socket transport service) can
  // be used to perform background tasks or I/O that would otherwise block the
  // main thread.
  nsCOMPtr<nsIEventTarget> target(
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID));
  if (!target) {
    return NS_ERROR_FAILURE;
  }
  return target->Dispatch(this, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
CryptoTask::Run() {
  if (!NS_IsMainThread()) {
    mRv = CalculateResult();
    NS_DispatchToMainThread(this);
  } else {
    // back on the main thread
    CallCallback(mRv);
  }
  return NS_OK;
}

}  // namespace mozilla
