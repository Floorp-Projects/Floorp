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

#ifndef nsAuthURLParser_h__
#define nsAuthURLParser_h__

#include "nsIURLParser.h"
#include "nsIURI.h"
#include "nsAgg.h"
#include "nsCRT.h"

#define NS_AUTHORITYURLPARSER_CID                    \
{ /* 90012125-1616-4fa1-ae14-4e7fa5766eb6 */         \
    0x90012125,                                      \
    0x1616,                                          \
    0x4fa1,                                          \
    {0xae, 0x14, 0x4e, 0x7f, 0xa5, 0x76, 0x6e, 0xb6} \
}

class nsAuthURLParser : public nsIURLParser
{
public:
    NS_DECL_ISUPPORTS
    ///////////////////////////////////////////////////////////////////////////
    // nsAuthURLParser methods:
    nsAuthURLParser() {
        NS_INIT_REFCNT();
    }
    virtual ~nsAuthURLParser();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    ///////////////////////////////////////////////////////////////////////////
    // nsIURLParser methods:
    NS_DECL_NSIURLPARSER

};

#endif // nsAuthURLParser_h__
