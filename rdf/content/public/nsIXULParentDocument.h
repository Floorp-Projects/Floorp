/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIXULParentDocument_h__
#define nsIXULParentDocument_h__

#include "nsString.h"

// {9878C881-D63C-11d2-BF86-00105A1B0627}
#define NS_IXULPARENTDOCUMENT_IID \
{ 0x9878c881, 0xd63c, 0x11d2, { 0xbf, 0x86, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsIContentViewerContainer;

class nsIXULParentDocument: public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IXULPARENTDOCUMENT_IID; return iid; }

    NS_IMETHOD GetContentViewerContainer(nsIContentViewerContainer** aContainer) = 0;
    NS_IMETHOD GetCommand(nsString& aCommand) = 0;
};

#endif // nsIXULParentDocument_h__
