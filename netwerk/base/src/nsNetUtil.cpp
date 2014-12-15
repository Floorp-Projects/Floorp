/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LoadContext.h"
#include "nsNetUtil.h"
#include "nsHttp.h"

bool NS_IsReasonableHTTPHeaderValue(const nsACString& aValue)
{
  return mozilla::net::nsHttp::IsReasonableHeaderValue(aValue);
}

bool NS_IsValidHTTPToken(const nsACString& aToken)
{
  return mozilla::net::nsHttp::IsValidToken(aToken);
}

nsresult
NS_NewLoadGroup(nsILoadGroup** aResult, nsIPrincipal* aPrincipal)
{
    using mozilla::LoadContext;
    nsresult rv;

    nsCOMPtr<nsILoadGroup> group =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<LoadContext> loadContext = new LoadContext(aPrincipal);
    rv = group->SetNotificationCallbacks(loadContext);
    NS_ENSURE_SUCCESS(rv, rv);

    group.forget(aResult);
    return rv;
}

bool
NS_LoadGroupMatchesPrincipal(nsILoadGroup* aLoadGroup,
                             nsIPrincipal* aPrincipal)
{
    if (!aPrincipal) {
      return false;
    }

    // If this is a null principal then the load group doesn't really matter.
    // The principal will not be allowed to perform any actions that actually
    // use the load group.  Unconditionally treat null principals as a match.
    bool isNullPrincipal;
    nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPrincipal);
    NS_ENSURE_SUCCESS(rv, false);
    if (isNullPrincipal) {
      return true;
    }

    if (!aLoadGroup) {
        return false;
    }

    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(nullptr, aLoadGroup, NS_GET_IID(nsILoadContext),
                                  getter_AddRefs(loadContext));
    NS_ENSURE_TRUE(loadContext, false);

    // Verify load context appId and browser flag match the principal
    uint32_t contextAppId;
    bool contextInBrowserElement;
    rv = loadContext->GetAppId(&contextAppId);
    NS_ENSURE_SUCCESS(rv, false);
    rv = loadContext->GetIsInBrowserElement(&contextInBrowserElement);
    NS_ENSURE_SUCCESS(rv, false);

    uint32_t principalAppId;
    bool principalInBrowserElement;
    rv = aPrincipal->GetAppId(&principalAppId);
    NS_ENSURE_SUCCESS(rv, false);
    rv = aPrincipal->GetIsInBrowserElement(&principalInBrowserElement);
    NS_ENSURE_SUCCESS(rv, false);

    return contextAppId == principalAppId &&
           contextInBrowserElement == principalInBrowserElement;
}
