/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// The datetime protocol handler creates "datetime" URIs of the form
// "datetime:RFC867Server".

#ifndef nsDateTimeHandler_h___
#define nsDateTimeHandler_h___

#include "nsIProxiedProtocolHandler.h"

#define DATETIME_PORT 13

// {AA27D2A0-B71B-11d3-A1A0-0050041CAF44}
#define NS_DATETIMEHANDLER_CID \
{ 0xaa27d2a0, 0xb71b, 0x11d3, { 0xa1, 0xa0, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44 } }

class nsDateTimeHandler : public nsIProxiedProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROXIEDPROTOCOLHANDLER

    // nsDateTimeHandler methods:
    nsDateTimeHandler();
    virtual ~nsDateTimeHandler();

    // Define a Create method to be used with a factory:
    static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
};

#endif /* nsDateTimeHandler_h___ */
