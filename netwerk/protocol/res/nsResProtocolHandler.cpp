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
using mozilla::unused;

#define kAPP           NS_LITERAL_CSTRING("app")
#define kGRE           NS_LITERAL_CSTRING("gre")

nsresult
nsResProtocolHandler::Init()
{
    nsresult rv;
    nsAutoCString appURI, greURI;
    rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::APP, appURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::GRE, greURI);
    NS_ENSURE_SUCCESS(rv, rv);

    //
    // make resource:/// point to the application directory or omnijar
    //
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), appURI.Length() ? appURI : greURI);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetSubstitution(EmptyCString(), uri);
    NS_ENSURE_SUCCESS(rv, rv);

    //
    // make resource://app/ point to the application directory or omnijar
    //
    rv = SetSubstitution(kAPP, uri);
    NS_ENSURE_SUCCESS(rv, rv);

    //
    // make resource://gre/ point to the GRE directory
    //
    if (appURI.Length()) { // We already have greURI in uri if appURI.Length() is 0.
        rv = NS_NewURI(getter_AddRefs(uri), greURI);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = SetSubstitution(kGRE, uri);
    NS_ENSURE_SUCCESS(rv, rv);

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
    // try invoking the directory service for "resource:root"

    nsAutoCString key;
    key.AssignLiteral("resource:");
    key.Append(root);

    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(key.get(), getter_AddRefs(file));
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;
        
    rv = IOService()->NewFileURI(file, result);
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
}
