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
 * Contributor(s): Sammy Ford <sford@swbell.com>
 *                 Dawn Endico <endico@mozilla.org>
 *
 */

#ifndef nsAboutConfig_h__
#define nsAboutConfig_h__

#include "nsIAboutModule.h"

class nsAboutConfig : public nsIAboutModule
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    nsAboutConfig() { NS_INIT_REFCNT(); }
    virtual ~nsAboutConfig() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};

#define NS_ABOUT_CONFIG_MODULE_CID                   \
{ /* 5b9cd4b2-1dd2-11b2-85a8-f86404a6cff3 */         \
    0x5b9cd4b2,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0x85, 0xa8, 0xf8, 0x64, 0x04, 0xa6, 0xcf, 0xf3} \
}

#endif // nsAboutConfig_h__
