/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsAbout_h__
#define nsAbout_h__

#include "nsIAboutModule.h"

class nsAbout : public nsIAboutModule 
{
public:
	
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    nsAbout() { NS_INIT_REFCNT(); }
    virtual ~nsAbout() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};

#define NS_ABOUT_CID                    \
{ /* {1f1ce501-663a-11d3-b7a0-be426e4e69bc} */         \
0x1f1ce501, 0x663a, 0x11d3, { 0xb7, 0xa0, 0xbe, 0x42, 0x6e, 0x4e, 0x69, 0xbc } \
}

#endif // nsAboutBlank_h__
