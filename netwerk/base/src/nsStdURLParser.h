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

#ifndef nsStdURLParser_h__
#define nsStdURLParser_h__

#include "nsIURLParser.h"
#include "nsIURI.h"
#include "nsAgg.h"
#include "nsCRT.h"

#define NS_STANDARDURLPARSER_CID                     \
{ /* dbf72351-4fd8-46f0-9dbc-fa5ba60a30c5 */         \
    0xdbf72351,                                      \
    0x4fd8,                                          \
    0x46f0,                                          \
    {0x9d, 0xbc, 0xfa, 0x5b, 0xa6, 0x0a, 0x30, 0x5c} \
}

class nsStdURLParser : public nsIURLParser
{
public:
    NS_DECL_ISUPPORTS
    ///////////////////////////////////////////////////////////////////////////
    // nsStdURLParser methods:
    nsStdURLParser() {
        NS_INIT_REFCNT();
    }
    virtual ~nsStdURLParser();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    ///////////////////////////////////////////////////////////////////////////
    // nsIURLParser methods:
    NS_DECL_NSIURLPARSER

};

#endif // nsStdURLParser_h__
