/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Nokia.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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
nsMaemoNetworkLinkService::GetLinkType(PRUint32 *aLinkType)
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

  nsresult rv = observerService->AddObserver(this, "xpcom-shutdown", PR_FALSE);
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
