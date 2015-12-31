/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/Telemetry.h"
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

NS_IMPL_ISUPPORTS(nsAlertsService, nsIAlertsService, nsIAlertsDoNotDisturb, nsIAlertsProgressListener)

nsAlertsService::nsAlertsService() :
  mXULAlerts(nsXULAlerts::GetInstance())
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


NS_IMETHODIMP nsAlertsService::ShowAlert(nsIAlertNotification * aAlert,
                                         nsIObserver * aAlertListener)
{
  NS_ENSURE_ARG(aAlert);

  nsAutoString cookie;
  nsresult rv = aAlert->GetCookie(cookie);
  NS_ENSURE_SUCCESS(rv, rv);

  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();

    if (aAlertListener)
      cpc->AddRemoteAlertObserver(cookie, aAlertListener);

    cpc->SendShowAlert(aAlert);
    return NS_OK;
  }

#ifdef MOZ_WIDGET_ANDROID
  nsAutoString imageUrl;
  rv = aAlert->GetImageURL(imageUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString title;
  rv = aAlert->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString text;
  rv = aAlert->GetText(text);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString name;
  rv = aAlert->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal;
  rv = aAlert->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::AndroidBridge::Bridge()->ShowAlertNotification(imageUrl, title, text, cookie,
                                                          aAlertListener, name, principal);
  return NS_OK;
#else
  // Check if there is an optional service that handles system-level notifications
  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
  if (sysAlerts) {
    rv = sysAlerts->ShowAlert(aAlert, aAlertListener);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
  }

  if (!ShouldShowAlert()) {
    // Do not display the alert. Instead call alertfinished and get out.
    if (aAlertListener)
      aAlertListener->Observe(nullptr, "alertfinished", cookie.get());
    return NS_OK;
  }

  // Use XUL notifications as a fallback if above methods have failed.
  rv = mXULAlerts->ShowAlert(aAlert, aAlertListener);
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

  return mXULAlerts->CloseAlert(aAlertName, aPrincipal);
#endif // !MOZ_WIDGET_ANDROID
}


// nsIAlertsDoNotDisturb
NS_IMETHODIMP nsAlertsService::GetManualDoNotDisturb(bool* aRetVal)
{
#ifdef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  // Try the system notification service.
  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
  if (sysAlerts) {
    nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
    if (!alertsDND) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    return alertsDND->GetManualDoNotDisturb(aRetVal);
  }

  nsCOMPtr<nsIAlertsDoNotDisturb> xulDND(do_QueryInterface(mXULAlerts));
  return xulDND ? xulDND->GetManualDoNotDisturb(aRetVal) : NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP nsAlertsService::SetManualDoNotDisturb(bool aDoNotDisturb)
{
#ifdef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  // Try the system notification service.
  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
  nsresult rv;
  if (sysAlerts) {
    nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID));
    if (!alertsDND) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    rv = alertsDND->SetManualDoNotDisturb(aDoNotDisturb);
  } else {
    nsCOMPtr<nsIAlertsDoNotDisturb> xulDND(do_QueryInterface(mXULAlerts));
    if (!xulDND) {
      return NS_ERROR_FAILURE;
    }
    rv = xulDND->SetManualDoNotDisturb(aDoNotDisturb);
  }

  Telemetry::Accumulate(Telemetry::ALERTS_SERVICE_DND_ENABLED, 1);
  return rv;
#endif
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
