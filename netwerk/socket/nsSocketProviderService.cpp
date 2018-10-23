/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsISocketProvider.h"
#include "nsSocketProviderService.h"
#include "nsError.h"
#include "nsSSLSocketProvider.h"
#include "nsTLSSocketProvider.h"
#include "nsUDPSocketProvider.h"
#include "mozilla/ClearOnShutdown.h"

mozilla::StaticRefPtr<nsSocketProviderService> nsSocketProviderService::gSingleton;

////////////////////////////////////////////////////////////////////////////////

already_AddRefed<nsISocketProviderService>
nsSocketProviderService::GetOrCreate()
{
  RefPtr<nsSocketProviderService> inst;
  if (gSingleton) {
    inst = gSingleton;
  } else {
    inst = new nsSocketProviderService();
    gSingleton = inst;
    if (NS_IsMainThread()) {
      mozilla::ClearOnShutdown(&gSingleton);
    } else {
      NS_DispatchToMainThread(NS_NewRunnableFunction(
        "net::nsSocketProviderService::GetOrCreate",
        []() -> void { mozilla::ClearOnShutdown(&gSingleton); }));
    }
  }
  return inst.forget();
}

NS_IMPL_ISUPPORTS(nsSocketProviderService, nsISocketProviderService)

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsSocketProviderService::GetSocketProvider(const char         *type,
                                           nsISocketProvider **result)
{
  nsresult rv;
  nsAutoCString contractID(
          NS_LITERAL_CSTRING(NS_NETWORK_SOCKET_CONTRACTID_PREFIX) +
          nsDependentCString(type));

  rv = CallGetService(contractID.get(), result);
  if (NS_FAILED(rv))
      rv = NS_ERROR_UNKNOWN_SOCKET_TYPE;
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
