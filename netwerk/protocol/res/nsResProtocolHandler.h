/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsResProtocolHandler_h___
#define nsResProtocolHandler_h___

#include "SubstitutingProtocolHandler.h"

#include "nsIResProtocolHandler.h"
#include "nsInterfaceHashtable.h"
#include "nsWeakReference.h"
#include "nsStandardURL.h"

struct SubstitutionMapping;
class nsResProtocolHandler final : public nsIResProtocolHandler,
                                   public mozilla::SubstitutingProtocolHandler,
                                   public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIRESPROTOCOLHANDLER

    NS_FORWARD_NSIPROTOCOLHANDLER(mozilla::SubstitutingProtocolHandler::)

    nsResProtocolHandler()
      : SubstitutingProtocolHandler("resource", URI_STD | URI_IS_UI_RESOURCE | URI_IS_LOCAL_RESOURCE,
                                    /* aEnforceFileOrJar = */ false)
    {}

    MOZ_MUST_USE nsresult Init();

    NS_IMETHOD SetSubstitution(const nsACString& aRoot, nsIURI* aBaseURI) override;

    NS_IMETHOD GetSubstitution(const nsACString& aRoot, nsIURI** aResult) override
    {
        return mozilla::SubstitutingProtocolHandler::GetSubstitution(aRoot, aResult);
    }

    NS_IMETHOD HasSubstitution(const nsACString& aRoot, bool* aResult) override
    {
        return mozilla::SubstitutingProtocolHandler::HasSubstitution(aRoot, aResult);
    }

    NS_IMETHOD ResolveURI(nsIURI *aResURI, nsACString& aResult) override
    {
        return mozilla::SubstitutingProtocolHandler::ResolveURI(aResURI, aResult);
    }

protected:
    MOZ_MUST_USE nsresult GetSubstitutionInternal(const nsACString& aRoot, nsIURI** aResult) override;
    virtual ~nsResProtocolHandler() {}

    MOZ_MUST_USE bool ResolveSpecialCases(const nsACString& aHost,
                                          const nsACString& aPath,
                                          const nsACString& aPathname,
                                          nsACString& aResult) override;

private:
    nsCString mAppURI;
    nsCString mGREURI;
};

#endif /* nsResProtocolHandler_h___ */
