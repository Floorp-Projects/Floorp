/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookiePermission.h"

#include "Cookie.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsICookie.h"
#include "nsICookieService.h"
#include "nsNetUtil.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsNetCID.h"
#include "prtime.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsContentUtils.h"

/****************************************************************
 ************************ CookiePermission **********************
 ****************************************************************/

namespace mozilla {
namespace net {

static const bool kDefaultPolicy = true;

namespace {
mozilla::StaticRefPtr<CookiePermission> gSingleton;
}

NS_IMPL_ISUPPORTS(CookiePermission, nsICookiePermission)

// static
already_AddRefed<nsICookiePermission> CookiePermission::GetOrCreate() {
  if (!gSingleton) {
    gSingleton = new CookiePermission();
    ClearOnShutdown(&gSingleton);
  }
  return do_AddRef(gSingleton);
}

bool CookiePermission::Init() {
  // Initialize nsPermissionManager and fetch relevant prefs. This is only
  // required for some methods on nsICookiePermission, so it should be done
  // lazily.

  mPermMgr = nsPermissionManager::GetInstance();
  return mPermMgr != nullptr;
}

NS_IMETHODIMP
CookiePermission::CanSetCookie(nsIURI* aURI, nsIChannel* aChannel,
                               nsICookie* aCookie, bool* aIsSession,
                               int64_t* aExpiry, bool* aResult) {
  NS_ASSERTION(aURI, "null uri");

  *aResult = kDefaultPolicy;

  // Lazily initialize ourselves
  if (!EnsureInitialized()) return NS_ERROR_UNEXPECTED;

  Cookie* cookie = static_cast<Cookie*>(aCookie);
  uint32_t perm;
  mPermMgr->LegacyTestPermissionFromURI(aURI, &cookie->OriginAttributesRef(),
                                        NS_LITERAL_CSTRING("cookie"), &perm);
  switch (perm) {
    case nsICookiePermission::ACCESS_SESSION:
      *aIsSession = true;
      [[fallthrough]];

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

}  // namespace net
}  // namespace mozilla
