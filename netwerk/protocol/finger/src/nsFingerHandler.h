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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Brian Ryner.
 * Portions created by Brian Ryner are Copyright (C) 2000 Brian Ryner.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *  Brian Ryner <bryner@uiuc.edu>
 */

// The finger protocol handler creates "finger" URIs of the form
// "finger:user@host" or "finger:host".

#ifndef nsFingerHandler_h___
#define nsFingerHandler_h___

#include "nsIProxiedProtocolHandler.h"

class nsIProxyInfo;

#define FINGER_PORT 79

// {0x76d6d5d8-1dd2-11b2-b361-850ddf15ef07}
#define NS_FINGERHANDLER_CID     \
{ 0x76d6d5d8, 0x1dd2, 0x11b2, \
   {0xb3, 0x61, 0x85, 0x0d, 0xdf, 0x15, 0xef, 0x07} }

class nsFingerHandler : public nsIProxiedProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROXIEDPROTOCOLHANDLER

    // nsFingerHandler methods:
    nsFingerHandler();
    virtual ~nsFingerHandler();

    // Define a Create method to be used with a factory:
    static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
};

#endif /* nsFingerHandler_h___ */
