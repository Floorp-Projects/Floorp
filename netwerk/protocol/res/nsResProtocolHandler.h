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
    NS_FORWARD_NSISUBSTITUTINGPROTOCOLHANDLER(mozilla::SubstitutingProtocolHandler::)

    nsResProtocolHandler()
      : SubstitutingProtocolHandler("resource", URI_STD | URI_IS_UI_RESOURCE | URI_IS_LOCAL_RESOURCE)
    {}

    nsresult Init();

protected:
    nsresult GetSubstitutionInternal(const nsACString& aRoot, nsIURI** aResult) override;
    virtual ~nsResProtocolHandler() {}
};

#endif /* nsResProtocolHandler_h___ */
