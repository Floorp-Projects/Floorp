/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAlertsUtils.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

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
