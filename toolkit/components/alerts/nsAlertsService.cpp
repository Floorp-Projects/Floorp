/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "nsXULAppAPI.h"

#include "nsAlertsService.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#else

#include "nsXPCOM.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsPromiseFlatString.h"
#include "nsToolkitCompsCID.h"

#endif // !MOZ_WIDGET_ANDROID

using namespace mozilla;

using mozilla::dom::ContentChild;

NS_IMPL_ISUPPORTS(nsAlertsService, nsIAlertsService, nsIAlertsProgressListener)

nsAlertsService::nsAlertsService()
{
}

nsAlertsService::~nsAlertsService()
{}

bool nsAlertsService::ShouldShowAlert()
{
  bool result = true;

#ifdef XP_WIN
  HMODULE shellDLL = ::LoadLibraryW(L"shell32.dll");
  if (!shellDLL)
    return result;

  SHQueryUserNotificationStatePtr pSHQueryUserNotificationState =
    (SHQueryUserNotificationStatePtr) ::GetProcAddress(shellDLL, "SHQueryUserNotificationState");

  if (pSHQueryUserNotificationState) {
    MOZ_QUERY_USER_NOTIFICATION_STATE qstate;
    if (SUCCEEDED(pSHQueryUserNotificationState(&qstate))) {
      if (qstate != QUNS_ACCEPTS_NOTIFICATIONS) {
         result = false;
      }
    }
  }

  ::FreeLibrary(shellDLL);
#endif

  return result;
}

NS_IMETHODIMP nsAlertsService::ShowAlertNotification(const nsAString & aImageUrl, const nsAString & aAlertTitle, 
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
  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();

    if (aAlertListener)
      cpc->AddRemoteAlertObserver(PromiseFlatString(aAlertCookie), aAlertListener);

    cpc->SendShowAlertNotification(PromiseFlatString(aImageUrl),
                                   PromiseFlatString(aAlertTitle),
                                   PromiseFlatString(aAlertText),
                                   aAlertTextClickable,
                                   PromiseFlatString(aAlertCookie),
                                   PromiseFlatString(aAlertName),
                                   PromiseFlatString(aBidi),
                                   PromiseFlatString(aLang),
                                   PromiseFlatString(aData),
                                   IPC::Principal(aPrincipal),
                                   aInPrivateBrowsing);
    return NS_OK;
  }

#ifdef MOZ_WIDGET_ANDROID
  mozilla::AndroidBridge::Bridge()->ShowAlertNotification(aImageUrl, aAlertTitle, aAlertText, aAlertCookie,
                                                          aAlertListener, aAlertName);
  return NS_OK;
#else
  // Check if there is an optional service that handles system-level notifications
  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
  nsresult rv;
  if (sysAlerts) {
    rv = sysAlerts->ShowAlertNotification(aImageUrl, aAlertTitle, aAlertText, aAlertTextClickable,
                                          aAlertCookie, aAlertListener, aAlertName,
                                          aBidi, aLang, aData,
                                          IPC::Principal(aPrincipal),
                                          aInPrivateBrowsing);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
  }

  if (!ShouldShowAlert()) {
    // Do not display the alert. Instead call alertfinished and get out.
    if (aAlertListener)
      aAlertListener->Observe(nullptr, "alertfinished", PromiseFlatString(aAlertCookie).get());
    return NS_OK;
  }

  // Use XUL notifications as a fallback if above methods have failed.
  rv = mXULAlerts.ShowAlertNotification(aImageUrl, aAlertTitle, aAlertText, aAlertTextClickable,
                                        aAlertCookie, aAlertListener, aAlertName,
                                        aBidi, aLang, aPrincipal, aInPrivateBrowsing);
  return rv;
#endif // !MOZ_WIDGET_ANDROID
}

NS_IMETHODIMP nsAlertsService::CloseAlert(const nsAString& aAlertName,
                                          nsIPrincipal* aPrincipal)
{
  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendCloseAlert(nsAutoString(aAlertName), IPC::Principal(aPrincipal));
    return NS_OK;
  }

#ifdef MOZ_WIDGET_ANDROID
  widget::GeckoAppShell::CloseNotification(aAlertName);
  return NS_OK;
#else

  // Try the system notification service.
  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
  if (sysAlerts) {
    return sysAlerts->CloseAlert(aAlertName, nullptr);
  }

  return mXULAlerts.CloseAlert(aAlertName);
#endif // !MOZ_WIDGET_ANDROID
}


NS_IMETHODIMP nsAlertsService::OnProgress(const nsAString & aAlertName,
                                          int64_t aProgress,
                                          int64_t aProgressMax,
                                          const nsAString & aAlertText)
{
#ifdef MOZ_WIDGET_ANDROID
  widget::GeckoAppShell::AlertsProgressListener_OnProgress(aAlertName,
                                                           aProgress, aProgressMax,
                                                           aAlertText);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // !MOZ_WIDGET_ANDROID
}

NS_IMETHODIMP nsAlertsService::OnCancel(const nsAString & aAlertName)
{
#ifdef MOZ_WIDGET_ANDROID
  widget::GeckoAppShell::CloseNotification(aAlertName);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // !MOZ_WIDGET_ANDROID
}
