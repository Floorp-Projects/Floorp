/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAlerts.h"

#include "nsAutoPtr.h"
#include "mozilla/LookAndFeel.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "nsIWindowWatcher.h"

#define ALERT_CHROME_URL "chrome://global/content/alerts/alert.xul"

NS_IMPL_ISUPPORTS1(nsXULAlertObserver, nsIObserver)

NS_IMETHODIMP
nsXULAlertObserver::Observe(nsISupports* aSubject, const char* aTopic,
                            const PRUnichar* aData)
{
  if (!strcmp("alertfinished", aTopic)) {
    nsIDOMWindow* currentAlert = mXULAlerts->mNamedWindows.GetWeak(mAlertName);
    // The window in mNamedWindows might be a replacement, thus it should only
    // be removed if it is the same window that is associated with this listener.
    if (currentAlert == mAlertWindow) {
      mXULAlerts->mNamedWindows.Remove(mAlertName);
    }
  }

  nsresult rv = NS_OK;
  if (mObserver) {
    rv = mObserver->Observe(aSubject, aTopic, aData);
  }
  return rv;
}

nsresult
nsXULAlerts::ShowAlertNotification(const nsAString& aImageUrl, const nsAString& aAlertTitle,
                                   const nsAString& aAlertText, bool aAlertTextClickable,
                                   const nsAString& aAlertCookie, nsIObserver* aAlertListener,
                                   const nsAString& aAlertName, const nsAString& aBidi,
                                   const nsAString& aLang)
{
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));

  nsCOMPtr<nsISupportsArray> argsArray;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
  NS_ENSURE_SUCCESS(rv, rv);

  // create scriptable versions of our strings that we can store in our nsISupportsArray....
  nsCOMPtr<nsISupportsString> scriptableImageUrl (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableImageUrl, NS_ERROR_FAILURE);

  scriptableImageUrl->SetData(aImageUrl);
  rv = argsArray->AppendElement(scriptableImageUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableAlertTitle (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertTitle, NS_ERROR_FAILURE);

  scriptableAlertTitle->SetData(aAlertTitle);
  rv = argsArray->AppendElement(scriptableAlertTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableAlertText (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertText, NS_ERROR_FAILURE);

  scriptableAlertText->SetData(aAlertText);
  rv = argsArray->AppendElement(scriptableAlertText);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsPRBool> scriptableIsClickable (do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID));
  NS_ENSURE_TRUE(scriptableIsClickable, NS_ERROR_FAILURE);

  scriptableIsClickable->SetData(aAlertTextClickable);
  rv = argsArray->AppendElement(scriptableIsClickable);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableAlertCookie (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertCookie, NS_ERROR_FAILURE);

  scriptableAlertCookie->SetData(aAlertCookie);
  rv = argsArray->AppendElement(scriptableAlertCookie);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsPRInt32> scriptableOrigin (do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID));
  NS_ENSURE_TRUE(scriptableOrigin, NS_ERROR_FAILURE);

  int32_t origin =
    LookAndFeel::GetInt(LookAndFeel::eIntID_AlertNotificationOrigin);
  scriptableOrigin->SetData(origin);

  rv = argsArray->AppendElement(scriptableOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableBidi (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableBidi, NS_ERROR_FAILURE);

  scriptableBidi->SetData(aBidi);
  rv = argsArray->AppendElement(scriptableBidi);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableLang (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableLang, NS_ERROR_FAILURE);

  scriptableLang->SetData(aLang);
  rv = argsArray->AppendElement(scriptableLang);
  NS_ENSURE_SUCCESS(rv, rv);

  // Alerts with the same name should replace the old alert in the same position.
  // Provide the new alert window with a pointer to the replaced window so that
  // it may take the same position.
  nsCOMPtr<nsISupportsInterfacePointer> replacedWindow = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
  NS_ENSURE_TRUE(replacedWindow, NS_ERROR_FAILURE);
  nsIDOMWindow* previousAlert = mNamedWindows.GetWeak(aAlertName);
  replacedWindow->SetData(previousAlert);
  replacedWindow->SetDataIID(&NS_GET_IID(nsIDOMWindow));
  rv = argsArray->AppendElement(replacedWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add an observer (that wraps aAlertListener) to remove the window from
  // mNamedWindows when it is closed.
  nsCOMPtr<nsISupportsInterfacePointer> ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRefPtr<nsXULAlertObserver> alertObserver = new nsXULAlertObserver(this, aAlertName, aAlertListener);
  nsCOMPtr<nsISupports> iSupports(do_QueryInterface(alertObserver));
  ifptr->SetData(iSupports);
  ifptr->SetDataIID(&NS_GET_IID(nsIObserver));
  rv = argsArray->AppendElement(ifptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = wwatch->OpenWindow(0, ALERT_CHROME_URL, "_blank",
                          "chrome,dialog=yes,titlebar=no,popup=yes", argsArray,
                          getter_AddRefs(newWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  mNamedWindows.Put(aAlertName, newWindow);
  alertObserver->SetAlertWindow(newWindow);

  return NS_OK;
}

nsresult
nsXULAlerts::CloseAlert(const nsAString& aAlertName)
{
  nsIDOMWindow* alert = mNamedWindows.GetWeak(aAlertName);
  nsCOMPtr<nsPIDOMWindow> domWindow = do_QueryInterface(alert);
  if (domWindow) {
    domWindow->DispatchCustomEvent("XULAlertClose");
  }
  return NS_OK;
}

