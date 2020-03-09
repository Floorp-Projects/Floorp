/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingUtils.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "nsIChannel.h"
#include "nsIPermission.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"

#define ANTITRACKING_PERM_KEY "3rdPartyStorage"

using namespace mozilla;
using namespace mozilla::dom;

/* static */ already_AddRefed<nsPIDOMWindowOuter>
AntiTrackingUtils::GetTopWindow(nsPIDOMWindowInner* aWindow) {
  Document* document = aWindow->GetExtantDoc();
  if (!document) {
    return nullptr;
  }

  nsIChannel* channel = document->GetChannel();
  if (!channel) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin =
      aWindow->GetBrowsingContext()->Top()->GetDOMWindow();

  if (!pwin) {
    return nullptr;
  }

  return pwin.forget();
}

/* static */
already_AddRefed<nsIURI> AntiTrackingUtils::MaybeGetDocumentURIBeingLoaded(
    nsIChannel* aChannel) {
  nsCOMPtr<nsIURI> uriBeingLoaded;
  nsLoadFlags loadFlags = 0;
  nsresult rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) {
    // If the channel being loaded is a document channel, this call may be
    // coming from an OnStopRequest notification, which might mean that our
    // document may still be in the loading process, so we may need to pass in
    // the uriBeingLoaded argument explicitly.
    rv = aChannel->GetURI(getter_AddRefs(uriBeingLoaded));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  return uriBeingLoaded.forget();
}

// static
void AntiTrackingUtils::CreateStoragePermissionKey(
    const nsCString& aTrackingOrigin, nsACString& aPermissionKey) {
  MOZ_ASSERT(aPermissionKey.IsEmpty());

  static const nsLiteralCString prefix =
      NS_LITERAL_CSTRING(ANTITRACKING_PERM_KEY "^");

  aPermissionKey.SetCapacity(prefix.Length() + aTrackingOrigin.Length());
  aPermissionKey.Append(prefix);
  aPermissionKey.Append(aTrackingOrigin);
}

// static
bool AntiTrackingUtils::CreateStoragePermissionKey(nsIPrincipal* aPrincipal,
                                                   nsACString& aKey) {
  if (!aPrincipal) {
    return false;
  }

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOriginNoSuffix(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  CreateStoragePermissionKey(origin, aKey);
  return true;
}

// static
bool AntiTrackingUtils::IsStorageAccessPermission(nsIPermission* aPermission,
                                                  nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPermission);
  MOZ_ASSERT(aPrincipal);

  // The permission key may belong either to a tracking origin on the same
  // origin as the granted origin, or on another origin as the granted origin
  // (for example when a tracker in a third-party context uses window.open to
  // open another origin where that second origin would be the granted origin.)
  // But even in the second case, the type of the permission would still be
  // formed by concatenating the granted origin to the end of the type name
  // (see CreatePermissionKey).  Therefore, we pass in the same argument to
  // both tracking origin and granted origin here in order to compute the
  // shorter permission key and will then do a prefix match on the type of the
  // input permission to see if it is a storage access permission or not.
  nsAutoCString permissionKey;
  bool result = CreateStoragePermissionKey(aPrincipal, permissionKey);
  if (NS_WARN_IF(!result)) {
    return false;
  }

  nsAutoCString type;
  nsresult rv = aPermission->GetType(type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return StringBeginsWith(type, permissionKey);
}
