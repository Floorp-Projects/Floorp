/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Patrick C. Beard <beard@netscape.com>
 */

#ifndef nsThemeHandler_h___
#define nsThemeHandler_h___

#include "nsIProtocolHandler.h"

// {a5d25ff0-1dd1-11b2-8600-81d7a7540308}
#define NS_THEMEHANDLER_CID     \
{ 0xa5d25ff0, 0x1dd1, 0x11b2, \
   {0x86, 0x00, 0x81, 0xd7, 0xa7, 0x54, 0x03, 0x08} }

class nsThemeHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER

    nsThemeHandler();

    static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
};

#endif /* nsThemeHandler_h___ */
