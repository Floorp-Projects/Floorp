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
 * Sammy Ford
 */

#ifndef nsAboutCredits_h__
#define nsAboutCredits_h__

#include "nsIAboutModule.h"

class nsAboutCredits : public nsIAboutModule
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    nsAboutCredits() { NS_INIT_REFCNT(); }
    virtual ~nsAboutCredits() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};

#define NS_ABOUT_CREDITS_MODULE_CID                    \
{ /*  4b00d478-1dd2-11b2-9c10-ac92614ad671*/         \
    0x4b00d478,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0x9c, 0x10, 0xac, 0x92, 0x61, 0x4a, 0xd6, 0x71} \
}

#endif // nsAboutCredits_h__
