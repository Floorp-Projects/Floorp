/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProcessManager.h"

#include "nsINetworkLinkService.h"
#include "nsISupportsImpl.h"
#include "mozilla/Services.h"

namespace mozilla {

/* static */ void GeckoProcessManager::Init() {
  BaseNatives::Init();
  ConnectionManager::Init();
}

NS_IMPL_ISUPPORTS(GeckoProcessManager::ConnectionManager, nsIObserver)

NS_IMETHODIMP GeckoProcessManager::ConnectionManager::Observe(
    nsISupports* aSubject, const char* aTopic, const char16_t* aData) {
  java::GeckoProcessManager::ConnectionManager::LocalRef connMgr(mJavaConnMgr);
  if (!connMgr) {
    return NS_OK;
  }

  if (!strcmp("application-foreground", aTopic)) {
    connMgr->OnForeground();
    return NS_OK;
  }

  if (!strcmp("application-background", aTopic)) {
    connMgr->OnBackground();
    return NS_OK;
  }

  if (!strcmp(NS_NETWORK_LINK_TOPIC, aTopic)) {
    const nsDependentString state(aData);
    // state can be up, down, or unknown. For the purposes of socket process
    // prioritization, we treat unknown as being up.
    const bool isUp = !state.EqualsLiteral(NS_NETWORK_LINK_DATA_DOWN);
    connMgr->OnNetworkStateChange(isUp);
    return NS_OK;
  }

  return NS_OK;
}

/* static */ void GeckoProcessManager::ConnectionManager::AttachTo(
    java::GeckoProcessManager::ConnectionManager::Param aInstance) {
  RefPtr<ConnectionManager> native(new ConnectionManager());
  BaseNatives::AttachNative(aInstance, native);

  native->mJavaConnMgr = aInstance;

  nsCOMPtr<nsIObserverService> obsServ(services::GetObserverService());
  obsServ->AddObserver(native, "application-background", false);
  obsServ->AddObserver(native, "application-foreground", false);
}

void GeckoProcessManager::ConnectionManager::ObserveNetworkNotifications() {
  nsCOMPtr<nsIObserverService> obsServ(services::GetObserverService());
  obsServ->AddObserver(this, NS_NETWORK_LINK_TOPIC, false);

  const bool isUp = java::GeckoAppShell::IsNetworkLinkUp();

  java::GeckoProcessManager::ConnectionManager::LocalRef connMgr(mJavaConnMgr);
  if (!connMgr) {
    return;
  }

  connMgr->OnNetworkStateChange(isUp);
}

}  // namespace mozilla
