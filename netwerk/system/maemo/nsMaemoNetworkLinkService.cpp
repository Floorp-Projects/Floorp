/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MaemoAutodial.
 *
 * The Initial Developer of the Original Code is
 * Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jeremias Bosch <jeremias.bosch@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMaemoNetworkLinkService.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsMaemoNetworkManager.h"

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
nsMaemoNetworkLinkService::GetIsLinkUp(PRBool *aIsUp)
{
  *aIsUp = nsMaemoNetworkManager::IsConnected();
  return NS_OK;
}

NS_IMETHODIMP
nsMaemoNetworkLinkService::GetLinkStatusKnown(PRBool *aIsKnown)
{
  *aIsKnown = nsMaemoNetworkManager::GetLinkStatusKnown();
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
  nsresult rv;

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = observerService->AddObserver(this, "xpcom-shutdown", PR_FALSE);
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
