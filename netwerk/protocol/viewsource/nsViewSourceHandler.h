/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewSourceHandler_h___
#define nsViewSourceHandler_h___

#include "nsIProtocolHandler.h"
#include "mozilla/Attributes.h"

class nsViewSourceHandler MOZ_FINAL : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER

    nsViewSourceHandler();

    ~nsViewSourceHandler();

    // Creates a new nsViewSourceChannel to view the source of an about:srcdoc
    // URI with contents specified by srcdoc.
    nsresult NewSrcdocChannel(nsIURI* uri, const nsAString &srcdoc, 
                              nsIChannel** result);

    static nsViewSourceHandler* GetInstance();

private:
    static nsViewSourceHandler* gInstance;

};

#endif /* !defined( nsViewSourceHandler_h___ ) */
