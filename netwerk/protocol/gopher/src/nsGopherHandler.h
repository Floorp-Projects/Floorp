/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Gopher protocol code.
 *
 * The Initial Developer of the Original Code is Bradley Baetz.
 * Portions created by Bradley Baetz are Copyright (C) 2000 Bradley Baetz.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *  Bradley Baetz <bbaetz@student.usyd.edu.au>
 */

#ifndef nsGopherHandler_h___
#define nsGopherHandler_h___

#include "nsIProxiedProtocolHandler.h"
#include "nsIProtocolProxyService.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#define GOPHER_PORT 70

// {0x44588c1f-2ce8-4ad8-9b16-dfb9d9d513a7}

#define NS_GOPHERHANDLER_CID     \
{ 0x44588c1f, 0x2ce8, 0x4ad8, \
   {0x9b, 0x16, 0xdf, 0xb9, 0xd9, 0xd5, 0x13, 0xa7} }

class nsGopherHandler : public nsIProxiedProtocolHandler {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROXIEDPROTOCOLHANDLER

    // nsGopherHandler methods:
    nsGopherHandler();
    virtual ~nsGopherHandler();

    // Define a Create method to be used with a factory:
    static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID,
                            void* *aResult);
protected:
    nsCOMPtr<nsIProtocolProxyService> mProxySvc;
};

#endif /* nsGopherHandler_h___ */
