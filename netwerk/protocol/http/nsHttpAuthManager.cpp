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

nsHttpAuthManager::nsHttpAuthManager()
{
}

nsresult nsHttpAuthManager::Init()
{
  // get reference to the auth cache.  we assume that we will live
  // as long as gHttpHandler.  instantiate it if necessary.

  if (!gHttpHandler) {
    nsresult rv;
    nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ios->GetProtocolHandler("http", getter_AddRefs(handler));
    if (NS_FAILED(rv))
      return rv;

    // maybe someone is overriding our HTTP handler implementation?
    NS_ENSURE_TRUE(gHttpHandler, NS_ERROR_UNEXPECTED);
  }
	
  mAuthCache = gHttpHandler->AuthCache(false);
  mPrivateAuthCache = gHttpHandler->AuthCache(true);
  NS_ENSURE_TRUE(mAuthCache, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mPrivateAuthCache, NS_ERROR_FAILURE);
  return NS_OK;
}

nsHttpAuthManager::~nsHttpAuthManager()
{
}

NS_IMETHODIMP
nsHttpAuthManager::GetAuthIdentity(const nsACString & aScheme,
                                   const nsACString & aHost,
                                   int32_t aPort,
                                   const nsACString & aAuthType,
                                   const nsACString & aRealm,
                                   const nsACString & aPath,
                                   nsAString & aUserDomain,
                                   nsAString & aUserName,
                                   nsAString & aUserPassword,
                                   bool aIsPrivate,
                                   nsIPrincipal* aPrincipal)
{
  nsHttpAuthCache* auth_cache = aIsPrivate ? mPrivateAuthCache : mAuthCache;
  nsHttpAuthEntry * entry = nullptr;
  nsresult rv;

  nsAutoCString originSuffix;
  if (aPrincipal) {
    aPrincipal->OriginAttributesRef().CreateSuffix(originSuffix);
  }

  if (!aPath.IsEmpty())
    rv = auth_cache->GetAuthEntryForPath(PromiseFlatCString(aScheme).get(),
                                         PromiseFlatCString(aHost).get(),
                                         aPort,
                                         PromiseFlatCString(aPath).get(),
                                         originSuffix,
                                         &entry);
  else
    rv = auth_cache->GetAuthEntryForDomain(PromiseFlatCString(aScheme).get(),
                                           PromiseFlatCString(aHost).get(),
                                           aPort,
                                           PromiseFlatCString(aRealm).get(),
                                           originSuffix,
                                           &entry);

  if (NS_FAILED(rv))
    return rv;
  if (!entry)
    return NS_ERROR_UNEXPECTED;

  aUserDomain.Assign(entry->Domain());
  aUserName.Assign(entry->User());
  aUserPassword.Assign(entry->Pass());
  return NS_OK;
}

NS_IMETHODIMP
nsHttpAuthManager::SetAuthIdentity(const nsACString & aScheme,
                                   const nsACString & aHost,
                                   int32_t aPort,
                                   const nsACString & aAuthType,
                                   const nsACString & aRealm,
                                   const nsACString & aPath,
                                   const nsAString & aUserDomain,
                                   const nsAString & aUserName,
                                   const nsAString & aUserPassword,
                                   bool aIsPrivate,
                                   nsIPrincipal* aPrincipal)
{
  nsHttpAuthIdentity ident(PromiseFlatString(aUserDomain).get(),
                           PromiseFlatString(aUserName).get(),
                           PromiseFlatString(aUserPassword).get());

  nsAutoCString originSuffix;
  if (aPrincipal) {
    aPrincipal->OriginAttributesRef().CreateSuffix(originSuffix);
  }

  nsHttpAuthCache* auth_cache = aIsPrivate ? mPrivateAuthCache : mAuthCache;
  return auth_cache->SetAuthEntry(PromiseFlatCString(aScheme).get(),
                                  PromiseFlatCString(aHost).get(),
                                  aPort,
                                  PromiseFlatCString(aPath).get(),
                                  PromiseFlatCString(aRealm).get(),
                                  nullptr,  // credentials
                                  nullptr,  // challenge
                                  originSuffix,
                                  &ident,
                                  nullptr); // metadata
}

NS_IMETHODIMP
nsHttpAuthManager::ClearAll()
{
  nsresult rv = mAuthCache->ClearAll();
  nsresult rv2 = mPrivateAuthCache->ClearAll();
  if (NS_FAILED(rv))
    return rv;
  if (NS_FAILED(rv2))
    return rv2;
  return NS_OK;
}

} // namespace net
} // namespace mozilla
