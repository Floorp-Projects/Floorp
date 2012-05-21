/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsQtNetworkManager.h"
#include "nsQtNetworkLinkService.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "mozilla/Services.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS2(nsQtNetworkLinkService,
                   nsINetworkLinkService,
                   nsIObserver)

nsQtNetworkLinkService::nsQtNetworkLinkService()
{
}

nsQtNetworkLinkService::~nsQtNetworkLinkService()
{
}

NS_IMETHODIMP
nsQtNetworkLinkService::GetIsLinkUp(bool* aIsUp)
{
  *aIsUp = nsQtNetworkManager::get()->isOnline();
  return NS_OK;
}

NS_IMETHODIMP
nsQtNetworkLinkService::GetLinkStatusKnown(bool* aIsKnown)
{
  *aIsKnown = nsQtNetworkManager::get()->isOnline();
  return NS_OK;
}

NS_IMETHODIMP
nsQtNetworkLinkService::GetLinkType(PRUint32 *aLinkType)
{
  NS_ENSURE_ARG_POINTER(aLinkType);

  // XXX This function has not yet been implemented for this platform
  *aLinkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  return NS_OK;
}

NS_IMETHODIMP
nsQtNetworkLinkService::Observe(nsISupports* aSubject,
                                const char* aTopic,
                                const PRUnichar* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    Shutdown();
    nsQtNetworkManager::get()->destroy();
  }

  if (!strcmp(aTopic, "browser-lastwindow-close-granted")) {
    Shutdown();
  }

  return NS_OK;
}

nsresult
nsQtNetworkLinkService::Init(void)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  nsQtNetworkManager::create();
  nsresult rv;

  rv = observerService->AddObserver(this, "xpcom-shutdown", false);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  rv = observerService->AddObserver(this, "browser-lastwindow-close-granted", false);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }


  return NS_OK;
}

nsresult
nsQtNetworkLinkService::Shutdown()
{
  nsQtNetworkManager::get()->closeSession();
  return NS_OK;
}
