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

#ifndef nsKeywordProtocolHandler_h___
#define nsKeywordProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsString.h"

#define NS_KEYWORDPROTOCOLHANDLER_CID                  \
{ /* 2E4233C0-6FB4-11d3-A180-0050041CAF44 */         \
    0x2e4233c0,                                      \
    0x6fb4,                                          \
    0x11d3,                                          \
    {0xa1, 0x80, 0x00, 0x50, 0x4, 0x1c, 0xaf, 0x44} \
}

class nsKeywordProtocolHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER

    // nsKeywordProtocolHandler methods:
    nsKeywordProtocolHandler();
    virtual ~nsKeywordProtocolHandler();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();

protected:
    nsCAutoString       mKeywordURL;
};

#endif /* nsKeywordProtocolHandler_h___ */
