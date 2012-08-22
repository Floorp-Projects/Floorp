/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMaemoNetworkLinkService.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsMaemoNetworkManager.h"
#include "mozilla/Services.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS2(nsMaemoNetworkLinkService,
                   nsINetworkLinkService,
                   nsIObserver)

nsMaemoNetworkLinkService::nsMaemoNetworkLinkService()
{
}

nsMaemoNetworkLinkService::~nsMaemoNetworkLinkService()
{
}

NS_IMETHODIMP
nsMaemoNetworkLinkService::GetIsLinkUp(bool *aIsUp)
{
  *aIsUp = nsMaemoNetworkManager::IsConnected();
  return NS_OK;
}

NS_IMETHODIMP
nsMaemoNetworkLinkService::GetLinkStatusKnown(bool *aIsKnown)
{
  *aIsKnown = nsMaemoNetworkManager::GetLinkStatusKnown();
  return NS_OK;
}

NS_IMETHODIMP
nsMaemoNetworkLinkService::GetLinkType(uint32_t *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

NS_IMETHODIMP
nsMaemoNetworkLinkService::Observe(nsISupports *aSubject,
                                   const char *aTopic,
                                   const PRUnichar *aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown"))
    Shutdown();

  return NS_OK;
}

nsresult
nsMaemoNetworkLinkService::Init(void)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  nsresult rv = observerService->AddObserver(this, "xpcom-shutdown", false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsMaemoNetworkManager::Startup())
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsMaemoNetworkLinkService::Shutdown()
{
  nsMaemoNetworkManager::Shutdown();
  return NS_OK;
}
