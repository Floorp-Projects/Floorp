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

#ifndef nsAboutBlank_h__
#define nsAboutBlank_h__

#include "nsIAboutModule.h"

class nsAboutBlank : public nsIAboutModule 
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD NewChannel(const char *verb,
                          nsIURI *aURI,
                          nsIEventSinkGetter *eventSinkGetter,
                          nsIChannel **result);

    nsAboutBlank() { NS_INIT_REFCNT(); }
    virtual ~nsAboutBlank() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};

#define NS_ABOUT_BLANK_MODULE_CID                    \
{ /* 3decd6c8-30ef-11d3-8cd0-0060b0fc14a3 */         \
    0x3decd6c8,                                      \
    0x30ef,                                          \
    0x11d3,                                          \
    {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#endif // nsAboutBlank_h__
