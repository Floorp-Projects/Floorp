/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsResProtocolHandler_h___
#define nsResProtocolHandler_h___

#include "nsIResProtocolHandler.h"
#include "nsInterfaceHashtable.h"
#include "nsWeakReference.h"
#include "nsStandardURL.h"

class nsIIOService;
struct ResourceMapping;

// nsResURL : overrides nsStandardURL::GetFile to provide nsIFile resolution
class nsResURL : public nsStandardURL
{
public:
    nsResURL() : nsStandardURL(true) {}
    virtual nsStandardURL* StartClone();
    virtual nsresult EnsureFile();
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aCID);
};

class nsResProtocolHandler : public nsIResProtocolHandler, public nsSupportsWeakReference
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIRESPROTOCOLHANDLER

    nsResProtocolHandler();

    nsresult Init();

    void CollectSubstitutions(InfallibleTArray<ResourceMapping>& aResources);

private:
    virtual ~nsResProtocolHandler();

    nsresult Init(nsIFile *aOmniJar);
    nsresult AddSpecialDir(const char* aSpecialDir, const nsACString& aSubstitution);
    nsInterfaceHashtable<nsCStringHashKey,nsIURI> mSubstitutions;
    nsCOMPtr<nsIIOService> mIOService;

    friend class nsResURL;
};

#endif /* nsResProtocolHandler_h___ */
