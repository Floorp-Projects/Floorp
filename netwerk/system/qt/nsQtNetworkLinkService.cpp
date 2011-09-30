/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Nokia Corporation Code.
 *
 * The Initial Developer of the Original Code is
 * Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jeremias Bosch <jeremias.bosch@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  rv = observerService->AddObserver(this, "xpcom-shutdown", PR_FALSE);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  rv = observerService->AddObserver(this, "browser-lastwindow-close-granted", PR_FALSE);
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
