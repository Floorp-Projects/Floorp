/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIProtocolURLFactory_h___
#define nsIProtocolURLFactory_h___

#include "nsISupports.h"
#include "nscore.h"

class nsString;
class nsIURI;
class nsILoadGroup;

#define NS_IPROTOCOLURLFACTORY_IID                   \
{ /* aed57ad0-705e-11d2-8166-006008119d7a */         \
    0xaed57ad0,                                      \
    0x705e,                                          \
    0x11d2,                                          \
    {0x81, 0x66, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

/**
 * nsIProtocolURLFactory deals with protocol-specific URL parsing. It 
 * constructs a URL that implements the nsIURI interface and that gets
 * loaded by a corresponding protocol handler.
 *
 * Note that one nsIProtocolURLFactory implementation might handle the
 * URL syntax for several different protocols, e.g. http, https, file --
 * all of these protocol's URL syntax looks the same. 
 */
class nsIProtocolURLFactory : public nsISupports
{
public:

    NS_IMETHOD CreateURL(nsIURI* *aResult,
                         const nsString& aSpec,
                         const nsIURI* aContextURL = nsnull,
                         nsISupports* aContainer = nsnull,
                         nsILoadGroup* aGroup = nsnull) = 0;
};

#endif /* nsIIProtocolURLFactory_h___ */
