/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/chrome/RegistryMessageUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/unused.h"

#include "nsResProtocolHandler.h"
#include "nsIIOService.h"
#include "nsIFile.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsURLHelper.h"
#include "nsEscape.h"

#include "mozilla/Omnijar.h"

using mozilla::dom::ContentParent;
using mozilla::LogLevel;
using mozilla::Unused;

#define kAPP           "app"
#define kGRE           "gre"

nsresult
nsResProtocolHandler::Init()
{
    nsresult rv;
    rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::APP, mAppURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::GRE, mGREURI);
    NS_ENSURE_SUCCESS(rv, rv);

    // mozilla::Omnijar::GetURIString always returns a string ending with /,
    // and we want to remove it.
    mGREURI.Truncate(mGREURI.Length() - 1);
    if (mAppURI.Length()) {
      mAppURI.Truncate(mAppURI.Length() - 1);
    } else {
      mAppURI = mGREURI;
    }

    //XXXbsmedberg Neil wants a resource://pchrome/ for the profile chrome dir...
    // but once I finish multiple chrome registration I'm not sure that it is needed

    // XXX dveditz: resource://pchrome/ defeats profile directory salting
    // if web content can load it. Tread carefully.

    return rv;
}

//----------------------------------------------------------------------------
// nsResProtocolHandler::nsISupports
//----------------------------------------------------------------------------

NS_IMPL_QUERY_INTERFACE(nsResProtocolHandler, nsIResProtocolHandler,
                        nsISubstitutingProtocolHandler, nsIProtocolHandler,
                        nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(nsResProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(nsResProtocolHandler, SubstitutingProtocolHandler)

nsresult
nsResProtocolHandler::GetSubstitutionInternal(const nsACString& root, nsIURI **result)
{
    nsAutoCString uri;

    if (!ResolveSpecialCases(root, NS_LITERAL_CSTRING("/"), uri)) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    return NS_NewURI(result, uri);
}

bool
nsResProtocolHandler::ResolveSpecialCases(const nsACString& aHost,
                                          const nsACString& aPath,
                                          nsACString& aResult)
{
    if (aHost.Equals("") || aHost.Equals(kAPP)) {
        aResult.Assign(mAppURI);
    } else if (aHost.Equals(kGRE)) {
        aResult.Assign(mGREURI);
    } else {
        return false;
    }
    aResult.Append(aPath);
    return true;
}

nsresult
nsResProtocolHandler::SetSubstitution(const nsACString& aRoot, nsIURI* aBaseURI)
{
    MOZ_ASSERT(!aRoot.Equals(""));
    MOZ_ASSERT(!aRoot.Equals(kAPP));
    MOZ_ASSERT(!aRoot.Equals(kGRE));
    return SubstitutingProtocolHandler::SetSubstitution(aRoot, aBaseURI);
}
