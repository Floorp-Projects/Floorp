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

#ifndef nsResourceProtocolHandler_h___
#define nsResourceProtocolHandler_h___

#include "nsIProtocolHandler.h"

#define NS_RESOURCEPROTOCOLHANDLER_CID               \
{ /* 3b61f0f0-2490-11d3-9349-00104ba0fd40 */         \
    0x3b61f0f0,                                      \
    0x2490,                                          \
    0x11d3,                                          \
    {0x93, 0x49, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsResourceProtocolHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER

    // nsResourceProtocolHandler methods:
    nsResourceProtocolHandler();
    virtual ~nsResourceProtocolHandler();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();

protected:
};

#endif /* nsResourceProtocolHandler_h___ */
