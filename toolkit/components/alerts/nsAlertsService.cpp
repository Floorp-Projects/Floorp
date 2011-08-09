/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Jens Bannmann <jens.b@web.de>
 *   Alex Pakhotin <alexp@mozilla.com>
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

#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"

#include "nsAlertsService.h"

#ifdef ANDROID
#include "AndroidBridge.h"
#else

#include "nsISupportsArray.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsIWindowWatcher.h"
#include "nsPromiseFlatString.h"
#include "nsWidgetsCID.h"
#include "nsILookAndFeel.h"
#include "nsToolkitCompsCID.h"

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

#define ALERT_CHROME_URL "chrome://global/content/alerts/alert.xul"

#endif // !ANDROID

using mozilla::dom::ContentChild;

NS_IMPL_THREADSAFE_ISUPPORTS2(nsAlertsService, nsIAlertsService, nsIAlertsProgressListener)

nsAlertsService::nsAlertsService()
{
}

nsAlertsService::~nsAlertsService()
{}

NS_IMETHODIMP nsAlertsService::ShowAlertNotification(const nsAString & aImageUrl, const nsAString & aAlertTitle, 
                                                     const nsAString & aAlertText, PRBool aAlertTextClickable,
                                                     const nsAString & aAlertCookie,
                                                     nsIObserver * aAlertListener,
                                                     const nsAString & aAlertName)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild* cpc = ContentChild::GetSingleton();

    if (aAlertListener)
      cpc->AddRemoteAlertObserver(PromiseFlatString(aAlertCookie), aAlertListener);

    cpc->SendShowAlertNotification(nsAutoString(aImageUrl),
                                   nsAutoString(aAlertTitle),
                                   nsAutoString(aAlertText),
                                   aAlertTextClickable,
                                   nsAutoString(aAlertCookie),
                                   nsAutoString(aAlertName));
    return NS_OK;
  }

#ifdef ANDROID
  mozilla::AndroidBridge::Bridge()->ShowAlertNotification(aImageUrl, aAlertTitle, aAlertText, aAlertCookie,
                                                          aAlertListener, aAlertName);
  return NS_OK;
#else
  // Check if there is an optional service that handles system-level notifications
  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
  nsresult rv;
  if (sysAlerts) {
    rv = sysAlerts->ShowAlertNotification(aImageUrl, aAlertTitle, aAlertText, aAlertTextClickable,
                                          aAlertCookie, aAlertListener, aAlertName);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsIDOMWindow> newWindow;

  nsCOMPtr<nsISupportsArray> argsArray;
  rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
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
  nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService("@mozilla.org/widget/lookandfeel;1");
  if (lookAndFeel)
  {
    PRInt32 origin;
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_AlertNotificationOrigin,
                           origin);
    scriptableOrigin->SetData(origin);
  }
  rv = argsArray->AppendElement(scriptableOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aAlertListener)
  {
    nsCOMPtr<nsISupportsInterfacePointer> ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> iSupports (do_QueryInterface(aAlertListener));
    ifptr->SetData(iSupports);
    ifptr->SetDataIID(&NS_GET_IID(nsIObserver));
    rv = argsArray->AppendElement(ifptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = wwatch->OpenWindow(0, ALERT_CHROME_URL, "_blank",
                 "chrome,dialog=yes,titlebar=no,popup=yes", argsArray,
                 getter_AddRefs(newWindow));
  return rv;
#endif // !ANDROID
}

NS_IMETHODIMP nsAlertsService::OnProgress(const nsAString & aAlertName,
                                          PRInt64 aProgress,
                                          PRInt64 aProgressMax,
                                          const nsAString & aAlertText)
{
#ifdef ANDROID
  mozilla::AndroidBridge::Bridge()->AlertsProgressListener_OnProgress(aAlertName, aProgress, aProgressMax, aAlertText);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // !ANDROID
}

NS_IMETHODIMP nsAlertsService::OnCancel(const nsAString & aAlertName)
{
#ifdef ANDROID
  mozilla::AndroidBridge::Bridge()->AlertsProgressListener_OnCancel(aAlertName);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // !ANDROID
}
