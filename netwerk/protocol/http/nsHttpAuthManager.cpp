/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpHandler.h"
#include "nsHttpAuthManager.h"
#include "nsNetUtil.h"
#include "nsIPrincipal.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(nsHttpAuthManager, nsIHttpAuthManager)

nsresult nsHttpAuthManager::Init() {
  // get reference to the auth cache.  we assume that we will live
  // as long as gHttpHandler.  instantiate it if necessary.

  if (!gHttpHandler) {
    nsresult rv;
    nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ios->GetProtocolHandler("http", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    // maybe someone is overriding our HTTP handler implementation?
    NS_ENSURE_TRUE(gHttpHandler, NS_ERROR_UNEXPECTED);
  }

  mAuthCache = gHttpHandler->AuthCache(false);
  mPrivateAuthCache = gHttpHandler->AuthCache(true);
  NS_ENSURE_TRUE(mAuthCache, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mPrivateAuthCache, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpAuthManager::GetAuthIdentity(
    const nsACString& aScheme, const nsACString& aHost, int32_t aPort,
    const nsACString& aAuthType, const nsACString& aRealm,
    const nsACString& aPath, nsAString& aUserDomain, nsAString& aUserName,
    nsAString& aUserPassword, bool aIsPrivate, nsIPrincipal* aPrincipal) {
  nsHttpAuthCache* auth_cache = aIsPrivate ? mPrivateAuthCache : mAuthCache;
  nsHttpAuthEntry* entry = nullptr;
  nsresult rv;

  nsAutoCString originSuffix;
  if (aPrincipal) {
    aPrincipal->OriginAttributesRef().CreateSuffix(originSuffix);
  }

  if (!aPath.IsEmpty()) {
    rv = auth_cache->GetAuthEntryForPath(aScheme, aHost, aPort, aPath,
                                         originSuffix, &entry);
  } else {
    rv = auth_cache->GetAuthEntryForDomain(aScheme, aHost, aPort, aRealm,
                                           originSuffix, &entry);
  }

  if (NS_FAILED(rv)) return rv;
  if (!entry) return NS_ERROR_UNEXPECTED;

  aUserDomain.Assign(entry->Domain());
  aUserName.Assign(entry->User());
  aUserPassword.Assign(entry->Pass());
  return NS_OK;
}

NS_IMETHODIMP
nsHttpAuthManager::SetAuthIdentity(
    const nsACString& aScheme, const nsACString& aHost, int32_t aPort,
    const nsACString& aAuthType, const nsACString& aRealm,
    const nsACString& aPath, const nsAString& aUserDomain,
    const nsAString& aUserName, const nsAString& aUserPassword, bool aIsPrivate,
    nsIPrincipal* aPrincipal) {
  nsHttpAuthIdentity ident(aUserDomain, aUserName, aUserPassword);

  nsAutoCString originSuffix;
  if (aPrincipal) {
    aPrincipal->OriginAttributesRef().CreateSuffix(originSuffix);
  }

  nsHttpAuthCache* auth_cache = aIsPrivate ? mPrivateAuthCache : mAuthCache;
  return auth_cache->SetAuthEntry(aScheme, aHost, aPort, aPath, aRealm,
                                  ""_ns,  // credentials
                                  ""_ns,  // challenge
                                  originSuffix, &ident,
                                  nullptr);  // metadata
}

NS_IMETHODIMP
nsHttpAuthManager::ClearAll() {
  mAuthCache->ClearAll();
  mPrivateAuthCache->ClearAll();
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
