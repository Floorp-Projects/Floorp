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

#ifndef nsNoAuthURLParser_h__
#define nsNoAuthURLParser_h__

#include "nsIURLParser.h"
#include "nsIURI.h"
#include "nsAgg.h"
#include "nsCRT.h"

#define NS_NOAUTHORITYURLPARSER_CID                  \
{ /* 9eeb1b89-c87e-4404-9de6-dbd41aeaf3d7 */         \
    0x9eeb1b89,                                      \
    0xc87e,                                          \
    0x4404,                                          \
    {0x9d, 0xe6, 0xdb, 0xd4, 0x1a, 0xea, 0xf3, 0xd7} \
}

class nsNoAuthURLParser : public nsIURLParser
{
public:
    NS_DECL_ISUPPORTS
    ///////////////////////////////////////////////////////////////////////////
    // nsNoAuthURLParser methods:
    nsNoAuthURLParser() {
        NS_INIT_REFCNT();
    }
    virtual ~nsNoAuthURLParser();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    ///////////////////////////////////////////////////////////////////////////
    // nsIURLParser methods:
    NS_DECL_NSIURLPARSER

};

#endif // nsNoAuthURLParser_h__
