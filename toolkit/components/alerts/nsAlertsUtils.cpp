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
