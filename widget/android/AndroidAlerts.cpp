/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidAlerts.h"
#include "GeneratedJNIWrappers.h"
#include "nsAlertsUtils.h"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(AndroidAlerts, nsIAlertsService)

StaticAutoPtr<AndroidAlerts::AlertInfoMap> AndroidAlerts::sAlertInfoMap;

NS_IMETHODIMP
AndroidAlerts::ShowAlertNotification(const nsAString & aImageUrl,
                                     const nsAString & aAlertTitle,
                                     const nsAString & aAlertText,
                                     bool aAlertTextClickable,
                                     const nsAString & aAlertCookie,
                                     nsIObserver * aAlertListener,
                                     const nsAString & aAlertName,
                                     const nsAString & aBidi,
                                     const nsAString & aLang,
                                     const nsAString & aData,
                                     nsIPrincipal * aPrincipal,
                                     bool aInPrivateBrowsing)
{
    MOZ_ASSERT_UNREACHABLE("Should be implemented by nsAlertsService.");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AndroidAlerts::ShowAlert(nsIAlertNotification* aAlert,
                         nsIObserver* aAlertListener)
{
    return ShowPersistentNotification(EmptyString(), aAlert, aAlertListener);
}

NS_IMETHODIMP
AndroidAlerts::ShowPersistentNotification(const nsAString& aPersistentData,
                                          nsIAlertNotification* aAlert,
                                          nsIObserver* aAlertListener)
{
    // nsAlertsService disables our alerts backend if we ever return failure
    // here. To keep the backend enabled, we always return NS_OK even if we
    // encounter an error here.
    nsresult rv;

    nsAutoString imageUrl;
    rv = aAlert->GetImageURL(imageUrl);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsAutoString title;
    rv = aAlert->GetTitle(title);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsAutoString text;
    rv = aAlert->GetText(text);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsAutoString cookie;
    rv = aAlert->GetCookie(cookie);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsAutoString name;
    rv = aAlert->GetName(name);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsCOMPtr<nsIPrincipal> principal;
    rv = aAlert->GetPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsAutoString host;
    nsAlertsUtils::GetSourceHostPort(principal, host);

    if (aPersistentData.IsEmpty() && aAlertListener) {
        if (!sAlertInfoMap) {
            sAlertInfoMap = new AlertInfoMap();
        }
        // This will remove any observers already registered for this name.
        sAlertInfoMap->Put(name, new AlertInfo{aAlertListener, cookie});
    }

    java::GeckoAppShell::ShowNotification(
            name, cookie, title, text, host, imageUrl,
            !aPersistentData.IsEmpty() ? jni::StringParam(aPersistentData)
                                       : jni::StringParam(nullptr));
    return NS_OK;
}

NS_IMETHODIMP
AndroidAlerts::CloseAlert(const nsAString& aAlertName,
                          nsIPrincipal* aPrincipal)
{
    // We delete the entry in sAlertInfoMap later, when CloseNotification calls
    // NotifyListener.
    java::GeckoAppShell::CloseNotification(aAlertName);
    return NS_OK;
}

void
AndroidAlerts::NotifyListener(const nsAString& aName, const char* aTopic)
{
    if (!sAlertInfoMap) {
        return;
    }

    const auto pAlertInfo = sAlertInfoMap->Get(aName);
    if (!pAlertInfo) {
        return;
    }

    if (pAlertInfo->listener) {
        pAlertInfo->listener->Observe(
                nullptr, aTopic, pAlertInfo->cookie.get());
    }

    if (NS_LITERAL_CSTRING("alertfinished").Equals(aTopic)) {
        sAlertInfoMap->Remove(aName);
    }
}

} // namespace widget
} // namespace mozilla
