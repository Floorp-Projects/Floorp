/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsISocketProvider.h"
#include "nsError.h"
#include "nsNSSComponent.h"
#include "nsSOCKSSocketProvider.h"
#include "nsSocketProviderService.h"
#include "nsSSLSocketProvider.h"
#include "nsTLSSocketProvider.h"
#include "nsUDPSocketProvider.h"
#include "mozilla/ClearOnShutdown.h"

mozilla::StaticRefPtr<nsSocketProviderService>
    nsSocketProviderService::gSingleton;

////////////////////////////////////////////////////////////////////////////////

already_AddRefed<nsISocketProviderService>
nsSocketProviderService::GetOrCreate() {
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
nsSocketProviderService::GetSocketProvider(const char* type,
                                           nsISocketProvider** result) {
  nsCOMPtr<nsISocketProvider> inst;
  if (!nsCRT::strcmp(type, "ssl") && XRE_IsParentProcess() &&
      EnsureNSSInitializedChromeOrContent()) {
    inst = new nsSSLSocketProvider();
  } else if (!nsCRT::strcmp(type, "starttls") && XRE_IsParentProcess() &&
             EnsureNSSInitializedChromeOrContent()) {
    inst = new nsTLSSocketProvider();
  } else if (!nsCRT::strcmp(type, "socks")) {
    inst = new nsSOCKSSocketProvider(NS_SOCKS_VERSION_5);
  } else if (!nsCRT::strcmp(type, "socks4")) {
    inst = new nsSOCKSSocketProvider(NS_SOCKS_VERSION_4);
  } else if (!nsCRT::strcmp(type, "udp")) {
    inst = new nsUDPSocketProvider();
  } else {
    return NS_ERROR_UNKNOWN_SOCKET_TYPE;
  }
  inst.forget(result);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
