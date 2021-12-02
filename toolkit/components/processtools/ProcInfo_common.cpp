/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"

#include "mozilla/UniquePtr.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {

RefPtr<ProcInfoPromise> GetProcInfo(nsTArray<ProcInfoRequest>&& aRequests) {
  auto holder = MakeUnique<MozPromiseHolder<ProcInfoPromise>>();
  RefPtr<ProcInfoPromise> promise = holder->Ensure(__func__);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get stream transport service");
    holder->Reject(rv, __func__);
    return promise;
  }

  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__,
      [holder = std::move(holder),
       requests = std::move(aRequests)]() mutable -> void {
        holder->ResolveOrReject(GetProcInfoSync(std::move(requests)), __func__);
      });

  rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }

  return promise;
}

}  // namespace mozilla
