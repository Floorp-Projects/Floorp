/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAlerts.h"

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/unused.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "nsIWindowWatcher.h"

using namespace mozilla;
using mozilla::dom::NotificationTelemetryService;

#define ALERT_CHROME_URL "chrome://global/content/alerts/alert.xul"

namespace {
StaticRefPtr<nsXULAlerts> gXULAlerts;
} // anonymous namespace

NS_IMPL_ISUPPORTS(nsXULAlertObserver, nsIObserver)

NS_IMETHODIMP
nsXULAlertObserver::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData)
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

NS_IMPL_ISUPPORTS(nsXULAlerts, nsIAlertsService, nsIAlertsDoNotDisturb)

/* static */ already_AddRefed<nsXULAlerts>
nsXULAlerts::GetInstance()
{
  if (!gXULAlerts) {
    gXULAlerts = new nsXULAlerts();
    ClearOnShutdown(&gXULAlerts);
  }
  RefPtr<nsXULAlerts> instance = gXULAlerts.get();
  return instance.forget();
}

NS_IMETHODIMP
nsXULAlerts::ShowAlertNotification(const nsAString& aImageUrl, const nsAString& aAlertTitle,
                                   const nsAString& aAlertText, bool aAlertTextClickable,
                                   const nsAString& aAlertCookie, nsIObserver* aAlertListener,
                                   const nsAString& aAlertName, const nsAString& aBidi,
                                   const nsAString& aLang, const nsAString & aData,
                                   nsIPrincipal* aPrincipal, bool aInPrivateBrowsing)
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

NS_IMETHODIMP
nsXULAlerts::ShowAlert(nsIAlertNotification* aAlert,
                       nsIObserver* aAlertListener)
{
  bool inPrivateBrowsing;
  nsresult rv = aAlert->GetInPrivateBrowsing(&inPrivateBrowsing);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString cookie;
  rv = aAlert->GetCookie(cookie);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDoNotDisturb) {
    if (!inPrivateBrowsing) {
      RefPtr<NotificationTelemetryService> telemetry =
        NotificationTelemetryService::GetInstance();
      if (telemetry) {
        // Record the number of unique senders for XUL alerts. The OS X and
        // libnotify backends will fire `alertshow` even if "do not disturb"
        // is enabled. In that case, `NotificationObserver` will record the
        // sender.
        nsCOMPtr<nsIPrincipal> principal;
        if (NS_SUCCEEDED(aAlert->GetPrincipal(getter_AddRefs(principal)))) {
          Unused << NS_WARN_IF(NS_FAILED(telemetry->RecordSender(principal)));
        }
      }
    }
    if (aAlertListener)
      aAlertListener->Observe(nullptr, "alertfinished", cookie.get());
    return NS_OK;
  }

  nsAutoString name;
  rv = aAlert->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString imageUrl;
  rv = aAlert->GetImageURL(imageUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString title;
  rv = aAlert->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString text;
  rv = aAlert->GetText(text);
  NS_ENSURE_SUCCESS(rv, rv);

  bool textClickable;
  rv = aAlert->GetTextClickable(&textClickable);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString bidi;
  rv = aAlert->GetDir(bidi);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString lang;
  rv = aAlert->GetLang(lang);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString source;
  rv = aAlert->GetSource(source);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));

  nsCOMPtr<nsISupportsArray> argsArray;
  rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
  NS_ENSURE_SUCCESS(rv, rv);

  // create scriptable versions of our strings that we can store in our nsISupportsArray....
  nsCOMPtr<nsISupportsString> scriptableImageUrl (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableImageUrl, NS_ERROR_FAILURE);

  scriptableImageUrl->SetData(imageUrl);
  rv = argsArray->AppendElement(scriptableImageUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableAlertTitle (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertTitle, NS_ERROR_FAILURE);

  scriptableAlertTitle->SetData(title);
  rv = argsArray->AppendElement(scriptableAlertTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableAlertText (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertText, NS_ERROR_FAILURE);

  scriptableAlertText->SetData(text);
  rv = argsArray->AppendElement(scriptableAlertText);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsPRBool> scriptableIsClickable (do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID));
  NS_ENSURE_TRUE(scriptableIsClickable, NS_ERROR_FAILURE);

  scriptableIsClickable->SetData(textClickable);
  rv = argsArray->AppendElement(scriptableIsClickable);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableAlertCookie (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertCookie, NS_ERROR_FAILURE);

  scriptableAlertCookie->SetData(cookie);
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

  scriptableBidi->SetData(bidi);
  rv = argsArray->AppendElement(scriptableBidi);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsString> scriptableLang (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableLang, NS_ERROR_FAILURE);

  scriptableLang->SetData(lang);
  rv = argsArray->AppendElement(scriptableLang);
  NS_ENSURE_SUCCESS(rv, rv);

  // Alerts with the same name should replace the old alert in the same position.
  // Provide the new alert window with a pointer to the replaced window so that
  // it may take the same position.
  nsCOMPtr<nsISupportsInterfacePointer> replacedWindow = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
  NS_ENSURE_TRUE(replacedWindow, NS_ERROR_FAILURE);
  nsIDOMWindow* previousAlert = mNamedWindows.GetWeak(name);
  replacedWindow->SetData(previousAlert);
  replacedWindow->SetDataIID(&NS_GET_IID(nsIDOMWindow));
  rv = argsArray->AppendElement(replacedWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add an observer (that wraps aAlertListener) to remove the window from
  // mNamedWindows when it is closed.
  nsCOMPtr<nsISupportsInterfacePointer> ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<nsXULAlertObserver> alertObserver = new nsXULAlertObserver(this, name, aAlertListener);
  nsCOMPtr<nsISupports> iSupports(do_QueryInterface(alertObserver));
  ifptr->SetData(iSupports);
  ifptr->SetDataIID(&NS_GET_IID(nsIObserver));
  rv = argsArray->AppendElement(ifptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // The source contains the host and port of the site that sent the
  // notification. It is empty for system alerts.
  nsCOMPtr<nsISupportsString> scriptableAlertSource (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertSource, NS_ERROR_FAILURE);
  scriptableAlertSource->SetData(source);
  rv = argsArray->AppendElement(scriptableAlertSource);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> newWindow;
  nsAutoCString features("chrome,dialog=yes,titlebar=no,popup=yes");
  if (inPrivateBrowsing) {
    features.AppendLiteral(",private");
  }
  rv = wwatch->OpenWindow(0, ALERT_CHROME_URL, "_blank", features.get(),
                          argsArray, getter_AddRefs(newWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  mNamedWindows.Put(name, newWindow);
  alertObserver->SetAlertWindow(newWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsXULAlerts::SetManualDoNotDisturb(bool aDoNotDisturb)
{
  mDoNotDisturb = aDoNotDisturb;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAlerts::GetManualDoNotDisturb(bool* aRetVal)
{
  *aRetVal = mDoNotDisturb;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAlerts::CloseAlert(const nsAString& aAlertName,
                        nsIPrincipal* aPrincipal)
{
  nsIDOMWindow* alert = mNamedWindows.GetWeak(aAlertName);
  nsCOMPtr<nsPIDOMWindow> domWindow = do_QueryInterface(alert);
  if (domWindow) {
    MOZ_ASSERT(domWindow->IsOuterWindow());
    domWindow->DispatchCustomEvent(NS_LITERAL_STRING("XULAlertClose"));
  }
  return NS_OK;
}

