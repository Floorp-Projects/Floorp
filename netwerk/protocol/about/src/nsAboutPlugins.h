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

#ifndef nsAboutPlugins_h__
#define nsAboutPlugins_h__

#include "nsIAboutModule.h"

class nsAboutPlugins : public nsIAboutModule
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    nsAboutPlugins() { NS_INIT_REFCNT(); }
    virtual ~nsAboutPlugins() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};

#define NS_ABOUT_PLUGINS_MODULE_CID                  \
{ /* 344aef06-1dd2-11b2-a070-bd6118526e42 */         \
    0x344aef06,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0xa0, 0x70, 0xbd, 0x61, 0x18, 0x52, 0x6e, 0x42} \
}

#endif // nsAboutPlugins_h__
