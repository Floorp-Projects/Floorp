/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIChromeRegistry_h__
#define nsIChromeRegistry_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIURL.h"

// {D8C7D8A1-E84C-11d2-BF87-00105A1B0627}
#define NS_ICHROMEREGISTRY_IID \
{ 0xd8c7d8a1, 0xe84c, 0x11d2, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }


class nsIChromeRegistry : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_ICHROMEREGISTRY_IID; return iid; }
    
    NS_IMETHOD ConvertChromeURL(nsIURL* aChromeURL) = 0;
    NS_IMETHOD InitRegistry() = 0;
};

// for component registration
// {D8C7D8A2-E84C-11d2-BF87-00105A1B0627}
#define NS_CHROMEREGISTRY_CID \
{ 0xd8c7d8a2, 0xe84c, 0x11d2, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

////////////////////////////////////////////////////////////////////////////////

extern nsresult
NS_NewChromeRegistry(nsIChromeRegistry* *aResult);

#endif // nsIChromeRegistry_h__
