/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingAllowList.h"
#include "nsIPermissionManager.h"

namespace mozilla {

nsTArray<nsCString> BounceTrackingAllowList::GetAllowListPermissionTypes() {
  nsTArray<nsCString> types;
  types.AppendElement("trackingprotection");
  types.AppendElement("trackingprotection-pb");
  types.AppendElement("cookie");
  return types;
}

nsresult BounceTrackingAllowList::IsAllowListPermission(
    nsIPermission* aPermission, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aPermission);
  NS_ENSURE_ARG_POINTER(aResult);

  nsAutoCString type;
  nsresult rv = aPermission->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  // Assert that the permission type matches the types returned by
  // GetAllowListPermissionTypes.
  MOZ_ASSERT(type.EqualsLiteral("trackingprotection") ||
             type.EqualsLiteral("trackingprotection-pb") ||
             type.EqualsLiteral("cookie"));

  if (type.EqualsLiteral("cookie")) {
    uint32_t capability;
    rv = aPermission->GetCapability(&capability);
    NS_ENSURE_SUCCESS(rv, rv);

    // For cookie permissions only ALLOW_ACTION is considered as allow-listed.
    // We don't care about cases where cookies are blocked for a site.
    if (capability != nsIPermissionManager::ALLOW_ACTION) {
      *aResult = false;
      return NS_OK;
    }
  }

  *aResult = true;
  return NS_OK;
}

}  // namespace mozilla
