/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAlertsUtils.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

#define ALERTS_BUNDLE "chrome://alerts/locale/alert.properties"

/* static */
bool
nsAlertsUtils::IsActionablePrincipal(nsIPrincipal* aPrincipal)
{
  return aPrincipal &&
         !nsContentUtils::IsSystemOrExpandedPrincipal(aPrincipal) &&
         !aPrincipal->GetIsNullPrincipal();
}

/* static */
void
nsAlertsUtils::GetSource(nsIPrincipal* aPrincipal, nsAString& aSource)
{
  nsAutoString hostPort;
  GetSourceHostPort(aPrincipal, hostPort);
  if (hostPort.IsEmpty()) {
    return;
  }
  nsCOMPtr<nsIStringBundleService> stringService(
    mozilla::services::GetStringBundleService());
  if (!stringService) {
    return;
  }
  nsCOMPtr<nsIStringBundle> alertsBundle;
  if (NS_WARN_IF(NS_FAILED(stringService->CreateBundle(ALERTS_BUNDLE,
      getter_AddRefs(alertsBundle))))) {
    return;
  }
  const char16_t* params[1] = { hostPort.get() };
  nsXPIDLString result;
  if (NS_WARN_IF(NS_FAILED(
      alertsBundle->FormatStringFromName(MOZ_UTF16("source.label"), params, 1,
      getter_Copies(result))))) {
    return;
  }
  aSource = result;
}

/* static */
void
nsAlertsUtils::GetSourceHostPort(nsIPrincipal* aPrincipal,
                                 nsAString& aHostPort)
{
  if (!IsActionablePrincipal(aPrincipal)) {
    return;
  }
  nsCOMPtr<nsIURI> principalURI;
  if (NS_WARN_IF(NS_FAILED(
      aPrincipal->GetURI(getter_AddRefs(principalURI))))) {
    return;
  }
  if (!principalURI) {
    return;
  }
  nsAutoCString hostPort;
  if (NS_WARN_IF(NS_FAILED(principalURI->GetHostPort(hostPort)))) {
    return;
  }
  CopyUTF8toUTF16(hostPort, aHostPort);
}
