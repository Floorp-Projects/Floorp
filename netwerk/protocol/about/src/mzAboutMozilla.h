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

#ifndef mzAboutMozilla_h__
#define mzAboutMozilla_h__

#include "nsIAboutModule.h"

class mzAboutMozilla : public nsIAboutModule
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    mzAboutMozilla() { NS_INIT_REFCNT(); }
    virtual ~mzAboutMozilla() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};

#define MZ_ABOUT_MOZILLA_MODULE_CID                    \
{ /* 15f25270-1dd2-11b2-ae92-970d00af4e34*/         \
    0x15f25270,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0xae, 0x92, 0x97, 0x0d, 0x00, 0xaf, 0x4e, 0x34} \
}

#endif // mzAboutMozilla_h__
