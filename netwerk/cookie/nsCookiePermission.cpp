/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookiePermission.h"

#include "nsICookie2.h"
#include "nsIServiceManager.h"
#include "nsICookieManager.h"
#include "nsICookieService.h"
#include "nsNetUtil.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIDOMWindow.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsILoadContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsNetCID.h"
#include "prtime.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsContentUtils.h"

/****************************************************************
 ************************ nsCookiePermission ********************
 ****************************************************************/

using namespace mozilla;

static const bool kDefaultPolicy = true;

static const nsLiteralCString kPermissionType(NS_LITERAL_CSTRING("cookie"));

namespace {
mozilla::StaticRefPtr<nsCookiePermission> gSingleton;
}

NS_IMPL_ISUPPORTS(nsCookiePermission, nsICookiePermission)

// static
already_AddRefed<nsICookiePermission> nsCookiePermission::GetOrCreate() {
  if (!gSingleton) {
    gSingleton = new nsCookiePermission();
    ClearOnShutdown(&gSingleton);
  }
  return do_AddRef(gSingleton);
}

bool nsCookiePermission::Init() {
  // Initialize nsIPermissionManager and fetch relevant prefs. This is only
  // required for some methods on nsICookiePermission, so it should be done
  // lazily.
  nsresult rv;
  mPermMgr = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return false;

  return true;
}

NS_IMETHODIMP
nsCookiePermission::SetAccess(nsIURI* aURI, nsCookieAccess aAccess) {
  // Lazily initialize ourselves
  if (!EnsureInitialized()) return NS_ERROR_UNEXPECTED;

  //
  // NOTE: nsCookieAccess values conveniently match up with
  //       the permission codes used by nsIPermissionManager.
  //       this is nice because it avoids conversion code.
  //
  return mPermMgr->Add(aURI, kPermissionType, aAccess,
                       nsIPermissionManager::EXPIRE_NEVER, 0);
}

NS_IMETHODIMP
nsCookiePermission::CanSetCookie(nsIURI* aURI, nsIChannel* aChannel,
                                 nsICookie2* aCookie, bool* aIsSession,
                                 int64_t* aExpiry, bool* aResult) {
  NS_ASSERTION(aURI, "null uri");

  *aResult = kDefaultPolicy;

  // Lazily initialize ourselves
  if (!EnsureInitialized()) return NS_ERROR_UNEXPECTED;

  uint32_t perm;
  mPermMgr->TestPermission(aURI, kPermissionType, &perm);
  switch (perm) {
    case nsICookiePermission::ACCESS_SESSION:
      *aIsSession = true;
      MOZ_FALLTHROUGH;

    case nsICookiePermission::ACCESS_ALLOW:
      *aResult = true;
      break;

    case nsICookiePermission::ACCESS_DENY:
      *aResult = false;
      break;

    default:
      // Here we can have any legacy permission value.

      // now we need to figure out what type of accept policy we're dealing with
      // if we accept cookies normally, just bail and return
      if (StaticPrefs::network_cookie_lifetimePolicy() ==
          nsICookieService::ACCEPT_NORMALLY) {
        *aResult = true;
        return NS_OK;
      }

      // declare this here since it'll be used in all of the remaining cases
      int64_t currentTime = PR_Now() / PR_USEC_PER_SEC;
      int64_t delta = *aExpiry - currentTime;

      // We are accepting the cookie, but,
      // if it's not a session cookie, we may have to limit its lifetime.
      if (!*aIsSession && delta > 0) {
        if (StaticPrefs::network_cookie_lifetimePolicy() ==
            nsICookieService::ACCEPT_SESSION) {
          // limit lifetime to session
          *aIsSession = true;
        }
      }
  }

  return NS_OK;
}
