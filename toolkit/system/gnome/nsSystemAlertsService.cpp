/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsSystemAlertsService.h"
#include "nsAlertsIconListener.h"
#include "nsAutoPtr.h"

NS_IMPL_ADDREF(nsSystemAlertsService)
NS_IMPL_RELEASE(nsSystemAlertsService)

NS_INTERFACE_MAP_BEGIN(nsSystemAlertsService)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAlertsService)
   NS_INTERFACE_MAP_ENTRY(nsIAlertsService)
NS_INTERFACE_MAP_END_THREADSAFE

nsSystemAlertsService::nsSystemAlertsService()
{
}

nsSystemAlertsService::~nsSystemAlertsService()
{}

nsresult
nsSystemAlertsService::Init()
{
  return NS_OK;
}

NS_IMETHODIMP nsSystemAlertsService::ShowAlertNotification(const nsAString & aImageUrl, const nsAString & aAlertTitle, 
                                                           const nsAString & aAlertText, bool aAlertTextClickable,
                                                           const nsAString & aAlertCookie,
                                                           nsIObserver * aAlertListener,
                                                           const nsAString & aAlertName,
                                                           const nsAString & aBidi,
                                                           const nsAString & aLang,
                                                           const nsAString & aData,
                                                           nsIPrincipal * aPrincipal,
                                                           bool aInPrivateBrowsing)
{
  nsCOMPtr<nsIAlertNotification> alert =
    do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
  NS_ENSURE_TRUE(alert, NS_ERROR_FAILURE);
  nsresult rv = alert->Init(aAlertName, aImageUrl, aAlertTitle,
                            aAlertText, aAlertTextClickable,
                            aAlertCookie, aBidi, aLang, aData,
                            aPrincipal, aInPrivateBrowsing);
  NS_ENSURE_SUCCESS(rv, rv);
  return ShowAlert(alert, aAlertListener);
}

NS_IMETHODIMP nsSystemAlertsService::ShowAlert(nsIAlertNotification* aAlert,
                                               nsIObserver* aAlertListener)
{
  NS_ENSURE_ARG(aAlert);

  RefPtr<nsAlertsIconListener> alertListener = new nsAlertsIconListener();
  if (!alertListener)
    return NS_ERROR_OUT_OF_MEMORY;

  return alertListener->InitAlertAsync(aAlert, aAlertListener);
}

NS_IMETHODIMP nsSystemAlertsService::CloseAlert(const nsAString& aAlertName,
                                                nsIPrincipal* aPrincipal)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
